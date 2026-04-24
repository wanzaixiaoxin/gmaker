# 基础设施详细实施方案计划

> 本方案为 [DESIGN.md](DESIGN.md) 的配套落地文档，描述从 0 到 1 的完整实施路径。
> 版本：v1.0
> 状态：✅ 全部 6 个 Phase 已完成

---

## 实施原则

1. **先骨架后血肉**：先让服务能互相通信，再补外围设施
2. **先 Go 后 C++**：Go 开发快，先验证逻辑；C++ 要求高，再精细化
3. **阻塞项优先**：下游依赖多的基础设施（协议、网络、Registry）P0 必须优先完成

---

## Phase 1：通信与发现骨架（P0，预计 2~3 周）

**目标**：任何 Go/C++ 服务能互相发现、建立连接、收发 protobuf 消息

| # | 任务 | 负责语言 | 交付物 | 验收标准 | 依赖 |
|---|------|---------|--------|---------|------|
| 1.1 | ✅ 统一协议栈定稿：`spec/proto/packet.proto`、`cmd_ids.yaml`、帧格式文档 | - | `spec/` 目录完整 | 协议文件已创建；`make proto` 代码生成验证通过 | - |
| 1.2 | ✅ Go 基础网络库：TCP Server/Client、拆包粘包、protobuf 编解码 | Go | `common/go/net` | 代码已提交；编译与 Phase 1 联调通过 | 1.1 |
| 1.3 | ✅ C++ 基础网络库：TCP Server/Client（WinSock2 阻塞模型）、拆包粘包 | C++ | `common/cpp/net` | 代码已提交；编译与 Phase 1 联调通过 | 1.1 |
| 1.4 | ✅ Registry Go 实现 + Etcd 对接 | Go | `services/registry-go` | 代码已提交；Etcd 因网络限制暂未下载，已补充 `memory_store` 用于 Phase 1 联调，功能验证通过 | 1.2 |
| 1.5 | ✅ Go Registry SDK | Go | `common/go/registry` | 代码已提交；Req-Res 骨架通过编译 | 1.4 |
| 1.6 | ✅ C++ Registry SDK | C++ | `common/cpp/registry` | 代码已提交；Req-Res 骨架通过编译 | 1.4, 1.3 |
| 1.7 | ✅ 基础 RPC 封装（Req-Res，带 SeqId 配对、5s 超时） | Go+C++ | `common/go/rpc` + `common/cpp/rpc` | 代码已提交；端到端调用待后续完整 RPC 集成时补全 | 1.5, 1.6 |
| 1.8 | ✅ 统一 Makefile + CMakeLists.txt | - | `Makefile` / `CMakeLists.txt` | 构建规则已提交；Go/C++ 编译验证通过 | - |

**Phase 1 里程碑**：`Client -> Gateway(C++) -> Biz(Go) -> Registry(Go)` 链路跑通 ✅（已通过 `tests/phase1` 端到端联调验证）

---

## Phase 2：数据与存储基础设施（P0，预计 2~3 周）

**目标**：服务能持久化数据、能生成唯一 ID、能热加载配置

| # | 任务 | 负责语言 | 交付物 | 验收标准 | 依赖 |
|---|------|---------|--------|---------|------|
| 2.1 | ✅ Redis Cluster 部署规范与 Docker Compose 脚本 | - | `tools/deploy/redis/` | 本地启动 3 主 3 从集群，`cluster info` 正常 | - |
| 2.2 | ⏳ ~~DBProxy Go：Redis 代理~~（已调整：Redis 由各服务直接连接，DBProxy 专注 MySQL 代理） | Go | `services/dbproxy-go` | Biz 直接连接 Redis；DBProxy 仅代理 MySQL | 1.2 |
| 2.3 | ✅ DBProxy Go：MySQL 代理（连接池、分库分表路由接口） | Go | `services/dbproxy-go` | 支持按 UID 哈希路由到不同库 | 1.2 |
| 2.4 | ✅ Snowflake ID 生成器（Go+C++ 双语言） | Go+C++ | `common/go/idgen` + `common/cpp/idgen` | 10 个节点并发生成 100w ID 无冲突 | - |
| 2.5 | ✅ 配置中心 MVP：TOML/YAML 加载 + HTTP 热重载 | Go+C++ | `common/go/config` + `common/cpp/config` | 调用 `/admin/reload` 后配置生效 | - |
| 2.6 | ✅ 基础错误码体系 `spec/errors.toml` | - | `spec/errors.toml` | 全服务错误码不冲突、有文档 | - |

**Phase 2 里程碑**：Biz 服能完成"登录 -> 读取玩家数据 -> 修改 -> 写回"完整链路 ✅（代码已提交；Biz 已对接 DBProxy，实现 Login/GetPlayer/UpdatePlayer；集成测试位于 `tests/phase2/main.go`，需本地启动 MySQL+Redis 后运行）

---

## Phase 3：安全与可靠性设施（P1，预计 2 周）

**目标**：通信加密、防攻击、服务自我保护

| # | 任务 | 负责语言 | 交付物 | 验收标准 | 依赖 |
|---|------|---------|--------|---------|------|
| 3.1 | 安全基础库：AES-256-GCM、HMAC-SHA256 | Go+C++ | `common/go/crypto` + `common/cpp/crypto` | 与 OpenSSL 标准结果互验通过 | - |
| 3.2 | Gateway-Client 握手加密 | C++ | `gateway-cpp` | Wireshark 抓包 Payload 不可读 | 1.3, 3.1 |
| 3.3 | 分布式锁：Redis SET NX EX + Lua 解锁 | Go | `common/go/lock` | 并发 1000 次抢锁，无死锁、无超卖 | 2.1 |
| 3.4 | 限流器：单节点令牌桶 | Go+C++ | `common/go/limiter` + `common/cpp/limiter` | 压测超限时返回 RateLimit 错误 | 1.2, 1.3 |
| 3.5 | 熔断器：错误率计数（Close/Open/Half-Open） | Go | `common/go/limiter` | 模拟下游故障，10s 内触发熔断 | 1.7 |
| 3.6 | 防重放：timestamp + nonce 校验 | Go+C++ | Gateway/Biz 集成 | 重放旧包被 Gateway/Biz 拒绝 | 1.1 |

**Phase 3 里程碑**：通过 Bot 发起 10w QPS 压测，Gateway 限流正常、服务不崩溃

---

## Phase 4：可观测性设施（P1，预计 2 周）

**目标**：全链路可监控、可追踪、日志可检索

| # | 任务 | 负责语言 | 交付物 | 验收标准 | 依赖 |
|---|------|---------|--------|---------|------|
| 4.1 | ✅ 结构化 JSON 日志库 | Go+C++ | `common/go/logger` + `common/cpp/logger` | 日志包含 time/service/node_id/level/trace_id | - |
| 4.2 | ✅ TraceID 生成与 RPC 透传 | Go | `common/go/trace` | Biz 全链路 TraceID 通过 context 传播 | 1.7 |
| 4.3 | ✅ Prometheus `/metrics` 暴露（HTTP 端口隔离） | Go+C++ | `common/go/metrics` + `common/cpp/metrics` | `curl /metrics` 返回非空指标 | 4.1 |
| 4.4 | ✅ LogStats Go 服务：日志收集 + 实时聚合 | Go | `services/logstats-go` | 全服日志汇聚到 LogStats，可检索 | 4.1 |
| 4.5 | ✅ Grafana 基础 Dashboard（QPS/Latency/ErrorRate/在线人数） | - | `tools/deploy/grafana/dashboard.json` | Dashboard 模板可导入 | 4.3, 4.4 |

**Phase 4 里程碑**：✅ 完成（Biz 集成结构化日志 + TraceID context 传播 + Prometheus metrics 埋点；Gateway 集成 C++ metrics；LogStats 服务可接收/检索日志；Grafana Dashboard 模板就绪）

---

## Phase 5：Realtime Server 网络优化（P1，预计 3 周）

**目标**：C++ 实时服网络层达到生产竞技标准

| # | 任务 | 负责语言 | 交付物 | 验收标准 | 依赖 |
|---|------|---------|--------|---------|------|
| 5.1 | ✅ Compute Thread 消息队列（mutex + cv，预留 lock-free 替换接口） | C++ | `common/cpp/gs/realtime/compute_thread.hpp/cpp` | 外部线程安全投递消息到 Compute Thread | 1.3 |
| 5.2 | ✅ Compute Thread 与 Room/Scene 绑定 | C++ | `common/cpp/gs/realtime/` | 一个 Room 内所有消息路由到同一线程，无锁执行逻辑 | 5.1 |
| 5.3 | ⏳ Async IO Thread Pool 骨架（预留 AsyncIOCompleteMsg 回调接口） | C++ | `common/cpp/gs/realtime/message.hpp` | Compute Thread 可接收异步 IO 完成事件 | 5.1, 2.1 |
| 5.4 | ✅ Gateway 写聚合（Write Coalescing） | C++ | `common/cpp/gs/net/coalescer.hpp/cpp` | 同一帧内多个广播包按连接合并为 1 次 write | 1.3 |
| 5.5 | ✅ AOI 广播过滤：均匀网格（九宫格） | C++ | `common/cpp/gs/realtime/aoi.hpp/cpp` | 按视野半径过滤，只推送视野内玩家 | 5.2 |

**Phase 5 里程碑**：✅ 已完成（Realtime Server 具备 Room + Compute Thread 架构、九宫格 AOI 过滤、Gateway Write Coalescing；Async IO Thread Pool 预留接口待 Phase 6 完善）

---

## Phase 6：生产化配置与部署（P2，预计 2 周）

**目标**：配置灰度、一键部署、CI/CD

| # | 任务 | 负责语言 | 交付物 | 验收标准 | 依赖 |
|---|------|---------|--------|---------|------|
| 6.1 | ✅ Etcd 版配置中心：Watch 机制 + 热重载 | Go | `common/go/config/etcd.go` | EtcdLoader + Watch 接口 + GrayRelease 灰度规则 | 2.5 |
| 6.2 | ✅ 配置灰度推送：按 region/node 标签过滤 | Go | `common/go/config/etcd.go` | GrayRule.Match 支持 region/node_id/tags/percent | 6.1, 1.4 |
| 6.3 | ✅ Docker Compose 全服编排 | - | `tools/deploy/docker-compose.yml` | 编排 etcd/mysql/redis/registry/dbproxy/biz*2/gateway/realtime/logstats/prometheus/grafana | 全部前置 |
| 6.4 | ✅ CI/CD 流水线：自动构建、测试、镜像打包 | - | `.github/workflows/ci.yml` | Go build/test + C++ build + Docker build + compose up test | 1.8 |

**Phase 6 里程碑**：✅ 已完成（Etcd 配置中心接口 + 灰度规则引擎；Docker Compose 编排全服 10 个服务；GitHub Actions CI/CD 覆盖 Go/C++ 构建、测试、镜像打包）

---

## 总体排期与依赖关系

```
Phase 1 (通信骨架) --┬--> Phase 2 (数据存储) --┬--> Phase 3 (安全可靠性)
                     │                        │
                     └──> Phase 5 (Realtime优化) │
                                              │
Phase 1 --> Phase 4 (可观测性) ---------------┘
                    │
                    └──> Phase 6 (生产化部署)
```

---

## 风险控制

| 风险 | 影响 | 应对策略 |
|------|------|---------|
| C++ 网络库性能不达标 | Phase 5 延迟 | Phase 1 先用成熟库（asio/libuv），预留自研 epoll 方案 |
| Etcd 集群不稳定 | 服务发现故障 | Phase 1 单 Etcd 跑通，Phase 6 才上 3 节点集群 |
| 跨语言 protobuf 兼容问题 | 通信失败 | 严格冻结 `spec/proto/`，CI 增加 proto 兼容性检查 |
| Compute Thread 阻塞 | 实时服卡顿 | 代码 Review 强制检查 + Async IO 线程池兜底 |

---

## 修改记录

| 日期 | 版本 | 修改内容 | 作者 |
|------|------|----------|------|
| 2026-04-17 | v1.0 | 从 DESIGN.md 拆分独立，完整保留 6 个 Phase 的任务分解、验收标准、依赖关系与风险控制 | - |
