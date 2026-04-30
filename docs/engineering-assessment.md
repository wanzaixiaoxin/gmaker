# gmaker 工程全面评估报告

**评估日期**: 2026-04-30
**评估范围**: 完整代码库（C++ / Go 服务、基础库、构建/部署基础设施）

---

## 总体评分：6.2 / 10

| 维度 | 评分 | 权重 |
|------|------|------|
| 基础库能力 | 6.5 | 高 |
| 服务架构设计 | 7.0 | 高 |
| 服务调度与治理 | 6.0 | 高 |
| 代码质量 | 5.5 | 高 |
| 工程质量 | 5.5 | 中 |
| 业务性能 | 6.5 | 中 |
| 可用性 | 5.0 | 高 |
| 适用性 | 7.0 | 中 |

---

## 一、基础库能力（6.5/10）

### 优势

**C++ 侧**：自研了一套基于 libuv 的异步网络框架，涵盖事件循环、TCP/WebSocket 服务器、连接池、写合并（coalescer）、零拷贝 Buffer、中间件管道。设计上有明确的接口分层（`IConnection`、`Middleware`），支持多 worker 线程模型。

**Go 侧**：提供了 TCP 帧协议（`net.Packet`）、服务发现客户端、Redis 客户端、Snowflake ID 生成、结构化日志、Prometheus 指标、配置加载、限流器（令牌桶 + 熔断器）、分布式锁、重放保护等组件，覆盖面较全。

### 关键问题

| 等级 | 问题 | 位置 | 验证 / 修复状态 |
|------|------|------|------|
| ~~严重~~ | ~~TCP Server 使用 `dup()` 传递 socket~~ **❌ 不属实** — `dup()` 创建独立文件描述符，关闭原 fd 不影响副本；Windows 侧 `WSADuplicateSocketW` 同理。代码逻辑正确，不存在"数据损坏或崩溃"风险。 | `common/cpp/net/async/tcp_server.cpp:149` | 已验证，非 Bug |
| **严重** | Redis 客户端非线程安全，无锁保护，多线程直接使用会数据竞争 | `common/cpp/redis/redis_client.hpp` | **✅ 已修复** — 添加 `std::recursive_mutex` 保护所有公共方法 |
| ~~严重~~ | ~~`WriteReq` 单连接复用~~ **❌ 不属实** — `writing_` 原子标志 + 事件循环单线程执行 + `OnWriteDone` 链式调度确保同一时刻只有一个 `uv_write` 在进行。`WriteReq` 复用是有意的性能优化，不存在竞争条件。 | `common/cpp/net/async/tcp_connection.cpp:431` | 已验证，非 Bug |
| **高** | `RegistryClient::Call()` 在事件循环线程内阻塞等待 `future`（最长 3s），会卡住整个 loop | `common/cpp/registry/client.cpp:207` | **✅ 已修复** — 检测 `IsInLoopThread()`，若在 loop 线程内则用 `PumpOnce()` 非阻塞轮询，不再阻塞 loop |
| **高** | 事件循环析构时若线程仍在运行，触发 use-after-free 或死锁 | `common/cpp/net/async/event_loop.cpp:22` | **⚠️ 已缓解** — 添加 `running` 原子标志，析构时若线程仍在跑则 `Stop()` + 最多等待 10s。**残余风险**：若 loop 线程被长耗时回调阻塞超过 10s，析构线程会在 loop 未退出时直接调用 `uv_run`，仍可能触发未定义行为 |
| **中** | `reinterpret_cast<uint64_t>(node)` 作 conn_id，重启后可能碰撞 | `common/cpp/net/async/upstream.cpp:180` | 待修复 |
| **中** | Histogram 每次 Observe 全局加锁，高并发瓶颈 | `common/cpp/metrics/metrics.hpp:45` | 待修复 |
| **中** | Logger::Fatal 调用 `std::exit(1)` 不触发栈析构 | `common/cpp/logger/logger.hpp:80` | 待修复 |
| **低** | TOML 解析只读顶层键，嵌套表被静默丢弃 | `common/cpp/config/config.cpp:117` | 待修复 |

### 改进建议
1. ~~用 `uv_tcp_init_ex` 替代 `dup()`~~ **无需修改** — 经验证 `dup()` 方案在 POSIX 和 Windows 上均正确工作
2. **✅ 已完成**：Redis 客户端已加 `std::recursive_mutex`，所有公共方法受锁保护
3. **✅ 已完成**：`RegistryClient::Call()` 检测 `IsInLoopThread()` → 用 `PumpOnce()` 非阻塞轮询；非 loop 线程仍用原有 `future::wait_for`
4. **⚠️ 已缓解**：`AsyncEventLoop` 析构检查 `running` 标志，线程仍在跑时先 `Stop()` 并等待退出（最多 10s）。**建议**：改为析构前强制 `join` loop 线程，彻底消除超时 fallback 导致的并发 `uv_run` 风险

---

## 二、服务架构设计（7.0/10）

### 优势

- **分层清晰**：接入层（Gateway）-> 业务层（Biz/Chat/Login）-> 数据层（DBProxy）-> 存储层（MySQL/Redis），职责边界明确
- **双协议接入**：Gateway 同时支持 TCP 二进制协议和 WebSocket，兼容原生客户端和 Web 端
- **统一协议**：所有服务使用相同的 18-byte 包头 + Protobuf payload，命令号按区间划分（`0x0001xxxx` Biz、`0x0003xxxx` Chat 等）
- **安全设计**：逐连接 AES-256-GCM 加密、HMAC-SHA256 密钥派生、nonce 重放保护（5 分钟窗口）
- **服务发现**：自研 Registry 服务 + etcd 后端可选，支持动态上下线和订阅推送

### 关键问题

| 问题 | 说明 |
|------|------|
| Gateway <-> Biz 的 `conn_id` 前缀约定 | Gateway 在 payload 前 prepend 8-byte `conn_id`，Biz 负责剥离。这个约定没有版本标识，协议升级困难 |
| Login 是 HTTP，其余是 TCP | 登录走 HTTP REST，游戏走 TCP Protobuf，两套协议增加了客户端复杂度 |
| PlayerBind 走 Biz 验证 token | Gateway 收到 PlayerBind 后转发给 Biz，Biz 再查 Redis/DBProxy 验 token，链路长、延迟高 |
| Room 广播仅限单 Gateway | `realtime-cpp` 的广播只发给当前 Gateway 连接，多 Gateway 部署时无法跨节点广播 |
| `bot_master_key` 后门 | `biz-go/handler.go:150` 存在硬编码的 bot 免校验逻辑，是安全隐患 |

### 改进建议
1. 将 token 验证下沉到 Gateway 层（JWT 或网关本地缓存），减少一次 Biz 往返
2. Room 广播通过 Redis Pub/Sub 或 Registry 的 Subscribe 机制实现跨 Gateway
3. 移除 `bot_master_key` 后门，改用独立的内部管理接口
4. 考虑 Login 也统一为 TCP 协议，或 Gateway 提供 HTTP 代理层

---

## 三、服务调度与治理（6.0/10）

### 优势

- **动态发现**：Registry 支持注册、心跳续租、服务发现、Watch 订阅，能感知节点变更
- **连接池**：Gateway 的 `UpstreamManager` 自动维护到后端服务的连接池，支持增量更新
- **负载均衡**：UpstreamPool 内部轮询选择健康节点
- **健康检查**：TCP 连接级健康检查（连接成功即标记 healthy）

### 关键问题

| 问题 | 说明 |
|------|------|
| 无熔断机制 | C++ 侧 `AsyncUpstreamPool` 没有熔断，节点故障时仍会持续轮询到故障节点 |
| 无自适应负载均衡 | 纯轮询，不考虑节点负载、延迟、权重 |
| 无限流/反压 | Gateway 没有连接级或 IP 级限流，dbproxy-go 的 worker pool 满时直接丢包 |
| 启动时序脆弱 | Gateway 在 Biz 之前启动时，Discover 为空，依赖 Watch 增量推送；`AddNode` 不立即连接就是一个例证 |
| 无优雅关闭 | `run.py` 用 `taskkill /F` 强制结束，无 drain 逻辑 |
| Registry 单点 | memory store 模式下 Registry 是单点，故障后所有服务发现失效 |

### 改进建议
1. Gateway 增加令牌桶限流（C++ 侧 `common/cpp/limiter/token_bucket` 已有实现，但未接入 Gateway）
2. UpstreamPool 增加错误计数熔断，连续失败 N 次后摘除节点
3. Registry 至少部署 3 节点 Raft 集群，或使用 etcd 作为生产后端
4. 服务启动增加 readiness probe，避免上游过早接收流量
5. graceful shutdown：停止 accept 新连接 -> 等待现有请求完成 -> 再退出

---

## 四、代码质量（5.5/10）

### 优势

- **结构清晰**：目录按语言/功能分层，头文件与实现分离
- **日志规范**：C++ 和 Go 都使用结构化 JSON 日志，带 service/node_id/trace_id 字段
- **Protobuf 统一**：协议定义集中在 `spec/proto/`，命令号区间划分合理
- **Trace 传播**：Go 服务支持 trace context 透传

### 关键问题

**C++ 侧：**

| 问题 | 位置 |
|------|------|
| `reinterpret_cast` 处理 float（严格别名违规，UB） | `realtime-cpp/main.cpp:145` |
| 手动编码 protobuf wire format（极易出错） | `gateway-cpp/main.cpp:353` |
| WebSocket/TCP 双路径类型转换可疑 | `gateway-cpp/main.cpp:873` |
| 信号处理在 Windows 为空操作，Unix 创建 detached 线程 | `gateway-cpp/main.cpp:1236` |
| 大量 `if (logger_)` 判空，建议改用 Null Object 模式 | 多处 |

**Go 侧：**

| 问题 | 位置 |
|------|------|
| 大量错误被 `_ =` 静默忽略 | `login-go/handler.go:102,167,259` |
| 魔法字符串比较（`err.Error() == "redis not available"`） | `biz-go/handler.go:150` |
| `strconv.ParseUint` 错误忽略，player_id=0 继续执行 | `login-go/handler.go:211` |
| `json.Unmarshal` 同一对象调两次 | `logstats-go/main.go:45` |
| 无 payload 长度校验就 slice | `biz-go/handler.go:42` |
| 字符串匹配判断错误类型，易碎 | `dbproxy-go/internal/server/server.go:207` |

### 改进建议
1. 引入 `golangci-lint` + `errcheck`，禁止 `_ =` 忽略错误
2. C++ 侧引入 `clang-tidy`（`cppcoreguidelines-*`, `bugprone-*`, `performance-*`）
3. 统一错误码体系，Go 侧使用 `errors.Is` / 自定义错误类型，C++ 侧使用枚举错误码
4. 所有网络切片操作前加长度校验

---

## 五、工程质量（5.5/10）

### 优势

- **构建系统**：CMake 3.16+，C++17，依赖有本地/FetchContent 多级回退；Go 用标准 modules
- **Docker**：6 个服务均有多阶段 Dockerfile，镜像体积控制合理
- **CI/CD**：GitHub Actions 多平台（Ubuntu + Windows），含构建、单元测试、E2E、Docker 构建
- **部署工具**：自研 `deploy-gen` 生成器，支持多实例、跨平台启动脚本

### 关键问题

| 问题 | 说明 |
|------|------|
| 测试覆盖率约 18% | 生产代码 ~11K 行，测试 ~2K 行；C++ 服务零单元测试 |
| C++ CI 只跑 `test-crypto` | `test-async-net`、`test-redis` 未在 CI 中执行 |
| 无 Go race detector | CI 未加 `-race` |
| 无静态分析 | 无 `clang-tidy`、`cppcheck`、`golangci-lint` |
| 无安全扫描 | 无 Trivy、Snyk、依赖漏洞检测 |
| 无代码覆盖率报告 | 无 codecov/coveralls 集成 |
| E2E 测试薄弱 | `tests/phase1`、`tests/phase2` 只有基本连通性验证 |
| Docker Compose 无健康检查 | 无 `healthcheck`，无重启策略，无资源限制 |
| 无 K8s/云原生支持 | 部署脚本只支持裸机/Docker Compose |

### 改进建议
1. 强制要求服务级单元测试（覆盖核心 handler 逻辑）
2. CI 增加 `test-async-net`、`test-redis`、`go test -race`
3. 引入 `golangci-lint` + `clang-tidy` + `cppcheck`
4. 增加集成测试：模拟完整登录->绑定->发消息链路
5. 添加 `healthcheck` 到 Docker Compose，使用 `deploy.replicas` 替代硬编码多实例

---

## 六、业务性能（6.5/10）

### 优势

- **异步 I/O**：libuv 多 worker 模型，能支撑较高并发连接
- **写合并**：Gateway 的 `AsyncWriteCoalescer` 批量 flush，减少 syscall
- **零拷贝广播**：`Buffer` 用 `shared_ptr` 实现引用计数，Room 广播时共享同一 payload
- **Snowflake ID**：分布式 ID 生成，性能高、无单点
- **Redis Pipeline**：Go 侧批量操作减少 RTT
- **连接池复用**：dbproxy-go 的 MySQL 连接池有 max_open/max_idle 配置

### 关键问题

| 问题 | 影响 |
|------|------|
| ~~`dup()` socket 共享~~ **❌ 不属实** — `dup()`/`WSADuplicateSocketW` 正确创建了独立的 fd/socket handle，不存在 fd 复用/损坏风险，但多 worker 模型下每次 accept 都有一次系统调用开销 |
| Histogram 全局互斥锁 | 高频指标采集成为瓶颈 |
| dbproxy-go worker 池固定 64 | 无动态扩缩容，突发流量下队列满直接丢请求 |
| realtime-cpp 广播 O(n x m) | 对每个目标 conn 单独发一次 Gateway，无批量优化 |
| logstats-go 全量遍历统计 | `Stats` API O(n)，大内存时响应慢 |
| Redis 客户端阻塞同步 | 所有 Redis 操作同步阻塞 goroutine，未使用连接池 pipeline |

### 改进建议
1. ~~用 io_uring 替代 libuv 的 `dup()` 方案~~ **无需修改** — `dup()` 方案正确，若追求极致性能可考虑 `SO_REUSEPORT` + 多进程监听替代多 worker 模型
2. Metrics Histogram 改用原子计数器数组 + 定期合并
3. dbproxy-go 用有界队列 + 背压，而非直接丢包
4. realtime-cpp 批量序列化 Room Snapshot，一次发送
5. Go 侧 Redis 改用 `go-redis` 或异步 pipeline

---

## 七、可用性（5.0/10）

### 优势

- **多实例部署**：支持 Registry、DBProxy、Biz、Chat、Login、Gateway 多节点
- **服务发现自动重连**：节点掉线后 Registry 推送 Leave，Gateway 自动移除
- **Redis/MySQL 故障降级**：Login/Biz/Chat 在 Redis/DBProxy 不可用时能继续运行（部分功能降级）

### 关键问题

| 问题 | 影响 |
|------|------|
| Registry memory store 单点 | Registry 故障 = 全系统服务发现失效 |
| 无熔断 | 下游 Biz 全挂时，Gateway 请求仍持续转发，拖死自身 |
| 无优雅关闭 | 发版时强制 kill，正在处理的 PlayerBind 直接丢失 |
| 无连接数限制 | Gateway 未限制 `max_connections`，恶意连接可耗尽 fd |
| 超时硬编码 | 3s/5s 超时散落各处，无法根据负载调整 |
| 回退配置静态 | Login 的 fallback Gateway 地址硬编码 8083，与实际部署脱节 |
| 日志无轮转 | 长期运行后日志文件无限增长 |
| 无监控告警 | Prometheus + Grafana 有面板，但无告警规则 |

### 改进建议
1. Registry 生产环境必须 3 节点集群或使用 etcd
2. Gateway 增加连接数限制、IP 限速、熔断器
3. 所有超时改为可配置，支持自适应
4. 实现 graceful shutdown（SIGTERM 处理）
5. 日志增加按大小/时间轮转
6. Prometheus 增加告警规则（服务存活、错误率、延迟 P99）

---

## 八、适用性（7.0/10）

### 适用场景

| 场景 | 适配度 | 说明 |
|------|--------|------|
| 中小规模游戏服务器（<1万在线） | 4/5 | 架构清晰，功能完整，部署简单 |
| 快速原型/MVP 开发 | 5/5 | 代码量适中，自研组件可控，易二次开发 |
| 实时竞技/帧同步游戏 | 3/5 | realtime-cpp 有 ComputeThread 和 AOI，但广播性能不足 |
| 大型 MMO（>10万在线） | 2/5 | 多 worker `dup()` 有 syscall 开销、单点 Registry、无水平扩展的 Room 广播是大瓶颈 |
| 云原生/K8s 部署 | 2/5 | 无 K8s manifest，无服务网格，无 Sidecar |
| 金融级高可用 | 2/5 | 无事务、无强一致、无多活 |

### 核心优势

1. **自研可控**：不依赖 heavy framework（如 UE Dedicated Server、Agones），代码量适中，团队能完全掌握
2. **跨平台**：Windows/Linux 均支持，C++17 + Go 的组合兼顾性能和开发效率
3. **Web 友好**：内置 WebSocket 支持，浏览器客户端可直接接入
4. **部署灵活**：支持裸机、Docker、二进制直接部署

### 核心短板

1. **未经过大规模验证**：Registry 单点、Redis 客户端历史非线程安全、Room 广播无法跨 Gateway 等问题在生产高并发下会暴露
2. **缺少云原生能力**：无 K8s、无服务网格、无自动扩缩容
3. **运维工具薄弱**：无动态配置热更新、无流量灰度、无 A/B Test 支持

---

## 总结与优先行动项

### 立即可做（1-2 周）

1. ~~修复 `dup()` socket 传递~~ **经代码审查确认无需修复** — `dup()` 方案在 POSIX/Windows 上均正确
2. **✅ 已完成**：Redis 客户端加 `std::recursive_mutex` 保护（[redis_client.hpp](file:///d:/Documents/learn/opensource/gmaker/common/cpp/redis/redis_client.hpp) / [redis_client.cpp](file:///d:/Documents/learn/opensource/gmaker/common/cpp/redis/redis_client.cpp)）
3. ~~修复 `WriteReq` 复用竞争~~ **经代码审查确认无需修复** — 原子标志 + 事件循环串行调度确保安全
4. **统一错误处理** — Go 侧禁用 `_ =` 忽略错误，C++ 侧统一错误码
5. **增加 payload 长度校验** — 所有 slice 前检查长度

**已完成的修复（本批）：**

6. **✅ RegistryClient::Call() 事件循环阻塞** — [client.cpp](file:///d:/Documents/learn/opensource/gmaker/common/cpp/registry/client.cpp) 检测 `IsInLoopThread()`，loop 线程内改用 `PumpOnce()` 非阻塞轮询驱动响应到达
7. **⚠️ AsyncEventLoop 析构安全性（已缓解）** — [event_loop.cpp](file:///d:/Documents/learn/opensource/gmaker/common/cpp/net/async/event_loop.cpp) 添加 `running` 原子标志，析构先 `Stop()` 再等待线程退出（最多 10s）。**残余风险**：loop 线程阻塞超时时，析构线程与 loop 线程并发调用 `uv_run` 属未定义行为；建议改为析构前强制 `join`

### 短期（1-2 月）

6. Registry 集群化或切 etcd 生产后端
7. Gateway 增加限流、熔断、连接数限制
8. 实现 graceful shutdown
9. CI 增加 race detector、static analysis、覆盖率报告
10. 补充服务级单元测试和集成测试

### 中期（3-6 月）

11. Room 广播跨 Gateway（Redis Pub/Sub 或 gossip）
12. 引入 K8s Helm Chart / Operator
13. 动态配置热更新（SIGHUP / watch）
14. 性能基准测试和优化（Histogram 原子化、dbproxy 背压）

---

**结论**：这份工程在当前阶段属于**"功能完整、设计合理，但距离生产级高可用还有一段路"**的水平。作为学习项目或中小规模游戏的起步框架，它是合格的；若要支撑百万级并发或 99.99% SLA，需要对上述 Critical/High 问题逐一攻坚。
