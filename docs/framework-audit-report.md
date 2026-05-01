# gmaker 框架生产级审计报告

> **审计范围**：基础设施层（`common/go/`、`common/cpp/`）+ 服务层（`services/*`）
> **审计目标**：评估框架是否具备支撑商业级、高热度 SLG 游戏服务端的生产就绪度
> **审计方法**：源码静态分析 + 边界条件推演 + 测试覆盖检查 + 与设计文档（`DESIGN.md`）对照
> **报告日期**：2026-05-01
> **结论**：基础层当前不扎实，存在大量必须修复的 P0 缺陷后方可进行上层架构抽象

---

## 1. 总体评估

| 维度 | 评分（10分制） | 核心问题 |
|------|---------------|---------|
| **功能完整性** | 6/10 | 骨架齐全，但事务、读写分离、动态扩缩容、配置热重载等缺失 |
| **健壮性** | **3/10** | 大量 goroutine 泄漏、竞态条件、panic 风险、无 graceful shutdown |
| **测试覆盖** | **2/10** | `net`/`rpc`/`discovery`/`registry`/`cache`/`redis`/`idgen`/`trace`/`errors` 等核心包 **零测试** |
| **安全** | 4/10 | 加密库实现正确，但防重放、SQL 注入、限速、Registry 无认证等缺口明显 |
| **性能** | 5/10 | 部分优化（sharded token bucket、write coalescing），但多处瓶颈和无效实现 |
| **可观测性** | 5/10 | Metrics/日志/TraceID 有基础，但 Prometheus histogram 格式无效 ❌ *[验证批注：误判，histogram 格式实际合规]*、trace 无 span |
| **生产就绪度** | **3/10** | **当前状态不能直接用于商业级生产环境** |

### 1.1 与设计文档（DESIGN.md）的对照

| DESIGN.md 承诺 | 实际状态 | 差距 |
|---------------|---------|------|
| DBProxy：Redis 代理、Pipeline 合并、热点 Key 限流、大 Key 告警 | ❌ **完全缺失** | DBProxy 仅代理 MySQL |
| DBProxy：Cache-Aside、双写缓冲、序列化优化 | ❌ **完全缺失** | 无缓存层封装 |
| Realtime：三种运行时模式（`sync_room`/`spatial_scene`/`async_combat`） | ❌ **仅一种基础 room** | 无插件架构、无动态加载 |
| Realtime：Compute Thread 与 Room 一对一绑定 | ❌ **单线程跑所有 Room** | 严重违背设计，性能瓶颈 |
| Gateway：断线重连缓冲（300 帧/10s/5s snapshot） | ❌ **完全缺失** | 无状态缓存 |
| Gateway：AOI 坐标过滤 | ❌ **完全缺失** | 仅简单 room 广播 |
| Registry：多实例负载均衡、Raft 一致性 | ❌ **完全缺失** | 单实例直连 etcd |
| LogStats：Redis Stream 缓冲、Kafka/Pulsar 平滑替换 | ❌ **完全缺失** | 纯内存，重启丢失 |

---

## 2. Top 15 关键缺陷（按严重性排序）

### 🚨 P0 — 会导致生产事故（✅ 全部已修复）

| # | 模块 | 缺陷 | 后果 | 修复建议 |
|---|------|------|------|---------|
| 1 ✅ | `gateway-cpp` | `OnUpstreamPacket` 将**单播响应误广播给房间所有成员** ⚠️ *[验证批注：仅在玩家已加入 room 时触发，不在 room 中有 fallback 单播]* | 玩家隐私数据泄漏、状态混乱、安全灾难 | 路由前精确检查 `is_room_bcast` 标志，区分单播与广播路径 |
| 2 ✅ | `gateway-cpp` | `OnRoomBroadcast` 向**所有 Gateway 的所有连接**发送所有快照 | N² 流量放大，网络风暴，瞬间打垮带宽 | Gateway 间只转发目标 conn_id 列表，或引入 pub/sub 总线 |
| 3 ✅ | `gateway-cpp` / `realtime-cpp` | `seq_id` 被当作 `conn_id` 传给 Realtime | Realtime 无法正确路由响应，玩家永远收不到战斗/房间消息 | Gateway 转发时显式携带 `gateway_conn_id` 字段 |
| 4 ✅ | `common/go/net` | `TCPConn.readLoop()` 出错时只调 `c.raw.Close()`，**不调 `c.Close()`** | **每次断线泄漏一个 writeLoop goroutine**，长期运行后 OOM | 所有错误路径统一调用 `c.Close()`，确保 `closeCh` 关闭、`onClose` 回调触发 |
| 5 ✅ | `common/go/net` | `UpstreamPool.SendPacket()` 两次求值 `n.Client.Conn()`，中间可能被 `onClose` 置 nil | nil pointer panic，服务直接崩溃 | 缓存 `conn := n.Client.Conn()` 到局部变量后再判断和使用 |
| 6 ✅ | `common/go/discovery` | etcd `Delete` 事件中 `Node.Host`/`Node.Port` 为零值，`RemoveNode(":0")` 永远不匹配 ⚠️ *[验证批注：实际机制是 `onNodeEvent` 在 `Host==""` 时直接 return，根本不调 `RemoveNode`]* | **死节点永远留在上游连接池**，请求持续路由到故障节点 | Delete 事件携带完整节点信息，或在上游池中按 `NodeID` 反向查找删除 |
| 7 ✅ | `common/go/discovery` | `Watch` 断开后**永不重试** | 服务运行中逐渐丢失所有节点变更，新节点无法发现、死节点无法剔除 | Watch 外层包 `for { ... }` 循环，带指数退避重连 |
| 8 ✅ | `services/registry-go` | `MemoryStore` 节点**永不过期** | 内存模式下线节点一直存在，请求持续路由到死 IP | 增加后台 TTL 扫描协程，心跳超时自动剔除 |
| 9 ✅ | `services/registry-go` | 使用 `KeepAliveOnce` 而非 `KeepAlive` ⚠️ *[验证批注：问题仅在 `EtcdStore`（Registry 服务端），`EtcdImpl`（Discovery 客户端）已正确使用 `KeepAlive`]* | 网络抖动时 lease 意外过期，健康节点被误判为死亡 | 改用 `KeepAlive` 永久流，或外层包重试循环 |
| 10 ✅ | `common/go/limiter` | Token bucket `capacity` 按 64 shard 均分，`capacity < 64` 时**每 shard < 1，完全失效** | 限流形同虚设，测试用例已失败 | 全局 limit 单独处理（不经过 shard），per-key limit 确保 `perShardCap >= 1` |

### 🔴 P1 — 严重影响可靠性（✅ #11~#14 已修复，#15 为误判）

| # | 模块 | 缺陷 | 后果 | 修复建议 |
|---|------|------|------|---------|
| 11 ✅ | `common/go/cache` | Redis 故障时将所有请求打到 DB，**无降级路径** | 级联故障：Redis 挂 → DB 挂 → 全服崩溃 | 增加 `StoreError` 区分"key 不存在"与"存储不可用"，后者触发熔断或返回错误而非透传 DB |
| 12 ✅ | `common/go/replay` | 多节点无分布式保护，且 dedup key 包含 timestamp | 重放攻击可绕过（换 timestamp 保留 nonce） | 改用 Redis Set 做分布式 nonce 存储，key 仅含 nonce |
| 13 ✅ | `common/go/lock` | 同一实例多 goroutine 竞争，`l.value` 被覆盖 | A goroutine 可能解锁 B goroutine 持有的锁，导致并发安全问题 | `TryLock` 返回独立 lease 对象（含 token），不再修改 lock 实例状态 |
| 14 ✅ | `common/go/logger` | 子 logger `Stop()` 关闭共享 `stopCh`，**杀死父 logger** | 日志系统整体崩溃，所有服务节点无日志 | 子 logger 不共享 `stopCh`，或文档明确禁止子 logger 调用 `Stop()` |
| 15 ❌ | `common/go/metrics` | Histogram bucket 计数**非累积**，Prometheus 格式无效 ❌ *[验证批注：误判！源码 `if v <= b { h.counts[i]++ }` 是正确的累积计数，Prometheus 格式合规]* | 监控数据不可用，Grafana 无法正确计算分位值 | ~~输出时转换为累积计数（`le="10"` 包含所有 ≤10 的观测）~~ 无需修复 |

---

## 3. 按模块详细评估

### 3.1 网络与通信层（`common/go/net` + `rpc` + `discovery` + `registry`）

| 项目 | 评估 |
|------|------|
| **功能** | 18-byte 帧协议、AES-GCM 加密、心跳、中间件链、上游连接池 — 功能覆盖全面 |
| **健壮性** | **极差**。goroutine 泄漏、竞态条件、panic 无恢复、锁内执行阻塞 I/O |
| **测试覆盖** | **0%**。四个包合计零测试文件 |
| **生产结论** | ❌ **不能用于生产。必须先修复泄漏和竞态，补全测试。** |

#### 详细缺陷清单

**`common/go/net`：**
- `TCPConn.readLoop()` 心跳超时、DecodeHeader 错误、解密失败等路径均泄漏 writeLoop goroutine
- `TCPClient.doHandshake()` 无锁修改 `c.conn.onData` 和 `c.conn.sessionKey`，与 readLoop 数据竞态
- `UpstreamPool.Start()` / `tryConnectLocked()` 持有 `p.mu` 期间执行 5 秒阻塞 `net.DialTimeout`，阻塞所有并发调用
- `TCPServer` 的 `MaxConn` 检查遍历整个 `sync.Map`，连接风暴时 O(n) 开销
- `writeLoop` 无 `SetWriteDeadline`，对端接收缓冲满时可能永久阻塞
- `Broadcast()` 对单个连接写满时静默丢包，无日志无告警
- 无 `SetNoDelay`、`SetKeepAlive` TCP 调优

**`common/go/rpc`：**
- `seqID` 为 `atomic.Uint32`，回绕后可能将超时响应错发给新调用者
- `OnPacket` 无 panic recovery，业务 panic 直接崩溃进程
- 连接断开后无快速失败机制，`Call()` 阻塞到 context 超时

**`common/go/discovery`：**
- `UpstreamManager.Watch` 失败即退出，永不重启
- `EtcdImpl.Watch` 中 callback panic 导致 `wg.Done()` 永不执行，`Watch` 死锁
- `RegistryImpl.Register` 重复调用会启动多个心跳 goroutine
- `RegistryImpl.Deregister` 重复调用 `close(r.heartbeatStop)` 会 panic
- 无重连 jitter，Registry 重启时可能引发惊群

**`common/go/registry`：**
- `Client.Close()` 关闭 pending channel 导致 in-flight `call()` 收到 nil packet，随后解引用 panic
- `call()` 硬编码 5 秒超时，不可配置
- `Subscribe` 覆盖全局 `onEvent`，多订阅者冲突

---

### 3.2 安全与可靠性（`crypto` + `limiter` + `lock` + `replay`）

| 项目 | 评估 |
|------|------|
| **crypto** | AES-GCM / HMAC / Argon2id 实现正确，但 **Argon2id 参数未编码进 hash 字符串**。未来升级参数（如增大 memory）后，所有已有密码无法验证。`RandomBytes` 无负长度保护。 |
| **limiter** | 熔断器状态机正确，但 **token bucket 容量按 shard 均分导致失效**（`capacity < 64` 时完全不可用）。⚠️ *[验证批注：报告称"熔断器 `Allow()` 在 CAS 失败时错误返回 `false`"，实际 CAS 失败是合理的退避策略，不是 bug]* `halfOpenReqs` 在状态转换时泄漏。 |
| **lock** | Lua 脚本正确（SET NX EX + verify-and-delete），但 **struct 级竞态**（多 goroutine 共用同一 `RedisLock` 实例竞争 `l.value`）。无 Redlock/多主安全。`Extend` 接受 `ttl <= 0` 会调用 `PEXPIRE 0` **意外删除 key**。 |
| **replay** | **仅内存保护**，多节点部署时可被绕过。dedup key 为 `timestamp + ":" + nonce`，攻击者可用相同 nonce 换 timestamp 通过。O(n) GC 每次 Check 遍历全 map。无界内存增长（攻击者填充唯一 nonce 可 OOM）。 |
| **测试** | crypto 50%（Argon2id 零测试），limiter 83.3%（但 token bucket 测试失败），lock/replay 各一个基础测试 |
| **生产结论** | ⚠️ 加密可用，但限流/锁/防重放都有明显漏洞，**不能直接用于金融/安全敏感操作** |

---

### 3.3 可观测性（`logger` + `metrics` + `trace` + `logstats-go`）

| 项目 | 评估 |
|------|------|
| **logger** | 功能齐全（5 级、JSON、异步、文件+stdout），但 `l.level` 读写竞态（`log()` 无锁读，`SetLevel()` 加锁写）。子 logger `Stop()` 关闭共享 channel 杀死父级。无背压策略（channel 满时同步写入导致日志乱序）。`Fatal` 不保证 flush 完成。`Flush()` 是 `sleep(50ms)` 假实现。文件句柄永不关闭。 |
| **metrics** | Counter/Gauge/Histogram/Registry 有基础实现，但 **不支持 label**（多维指标不可用）。`Histogram.Observe` 将 `float64` 截断为 `int64` ⚠️ *[验证批注：截断确实存在，但仅影响 sum 亚毫秒精度，严重性被夸大]*。bucket 计数非累积，**Prometheus  exposition 格式无效** ❌ *[验证批注：误判！`if v <= b { counts[i]++ }` 已是正确的累积计数]*。Gauge/Counter 仅支持 `int64` 不支持浮点。无 metric 注销/过期。 |
| **trace** | 仅生成 32 字符 hex trace ID 字符串，**无 span ID、无采样决策、无标准传播格式**（W3C/OpenTelemetry）。C++ 侧完全缺失 trace 能力。 |
| **logstats-go** | **纯内存存储**，重启即丢失。缓冲区满时丢弃前 `maxSize/2` 条（非环形缓冲，灾难性数据丢失）。`QueryByTrace` 返回内部可变 slice（并发修改风险）。双次 JSON unmarshal。无 TCP 读超时。无认证。 |
| **测试** | logger/metrics/logstats 均零测试 |
| **生产结论** | ❌ metrics 不可用，logger 有 race，trace 不完整，logstats 不能持久化 |

---

### 3.4 数据与缓存（`dbproxy-go` + `redis` + `cache` + `idgen`）

| 项目 | 评估 |
|------|------|
| **dbproxy-go** | MySQL 连接池 + UID/Key 哈希分片路由有基础实现，但 **无事务、无读写分离、无 SQL 注入防护、无 graceful shutdown**。`QueryRowByUID` 在无 shard 时返回 `nil` 而非 error（调用方 panic 风险）。类型转换用 `fmt.Sprintf("%v", v)`，对 `sql.NullString`/`DECIMAL` 等会损坏数据。硬编码 3 秒超时。无 prepared statement 缓存。 |
| **redis** (`common/go/redis`) | 只是 go-redis 的薄封装（单节点/Cluster 自动切换），**无 Sentinel 支持、无重试退避、无熔断**。`WithHotKeyLimiter` 每次调用泄漏一个 `CleanLoop` goroutine。hotkey 限流器是**固定窗口**（非声称的滑动窗口），窗口边界可 2x burst。 |
| **cache** (`common/go/cache`) | Cache-aside + singleflight 架构正确，但 **singleflight 是进程级**，多实例时 Redis 过期仍会引发 DB stampede。Redis 故障（超时/断连）被当作 `ErrNotFound`，**所有请求透传 DB**，级联崩溃。`Delete` 函数副作用修改 caller 的输入 slice。`GetOrLoad` 将临时 DB 错误缓存为 nil-placeholder（60 秒内真实数据不可见）。无本地 L1 缓存。 |
| **idgen** | Snowflake 布局正确（41+10+12），但 **时钟回拨立即返回 fatal error**（无等待阈值或优雅恢复）。序列溢出时 tight spin-loop（CPU 燃烧）。`nodeID` 需手动传入，K8s 环境无外部协调时易碰撞。无批量预分配。 |
| **测试** | 四个模块均零测试 |
| **生产结论** | ⚠️ 骨架可用，但生产缺口大。cache 的级联故障风险最高，idgen 的时钟回拨可能中断服务 |

---

### 3.5 服务层（Gateway / Registry / Realtime / DBProxy / LogStats）

| 服务 | 关键问题 | 与设计文档差距 |
------|---------|--------------|
| **gateway-cpp** | 单播变广播 bug、广播放大 bug、无客户端心跳超时 ⚠️ *[验证批注：有 `HeartbeatLoop` 但仅清理 pending_bind 超时，不做客户端活跃度检测]* 、无 graceful shutdown、无速率限制、`FLAG_COMPRESS` 定义但未实现、C++ signal handler Windows 分支为空 | 断线重连缓冲、AOI 过滤、KCP/QUIC 预留、TLS 均未实现 |
| **registry-go** | MemoryStore 节点永不过期、KeepAliveOnce 错误、无认证、无 HA/集群、Watcher context 生命周期 bug、Lease 泄漏 | 多 Registry 负载均衡、Raft、SDK 本地缓存回退均未实现 |
| **realtime-cpp** | **单 ComputeThread 处理所有 Room**（DESIGN.md 要求 per-room）、`conn_id` 使用 `seq_id`、原始指针悬空风险、`msg_queue_` 无界、无动态 Room 管理（仅 5 个硬编码）、无重连缓冲、无帧时间保证、无输入校验 | `IRealtimeContext` 插件架构、`async_combat`、Replay、Spectator 均未实现 |
| **dbproxy-go** | 无事务、无 graceful shutdown、无 SQL 注入防护、无健康检查、无 metrics | Redis 代理、Pipeline 合并、热点 Key 限流、大 Key 告警、Cache-Aside 均未实现 |
| **logstats-go** | 内存数据重启丢失、缓冲区满丢一半、返回内部可变 slice、无认证、无 TCP 读超时、无持久化后端 | Redis Stream 缓冲、Kafka/Pulsar 替换、ClickHouse/Jaeger 存储均未实现 |

---

## 4. 修复路线图建议

### 4.1 原则

> **在往上搭建"框架抽象层"（App 容器、Module 注册、Domain 接口、DSL）之前，必须先完成 P0 缺陷修复。上层抽象建立在漏水的地基上，返工成本会指数级增长。**

### 4.2 三阶段修复计划

| 阶段 | 周期 | 目标 | 具体任务 |
|------|------|------|---------|
| **P0 紧急修复** | **1~2 周** | 消除会导致生产事故的 bug | 修复 goroutine 泄漏、竞态条件、Gateway 广播 bug、Registry lease、token bucket 容量、logger race ~~、metrics 格式~~ ❌ *[验证批注：metrics 格式问题为误判，不需修复]* |
| **P0 补测试** | **1 周** | 核心包达到基本测试覆盖 | `net`/`rpc`/`discovery`/`registry` 写单元测试；所有已有测试必须通过（当前 token bucket 测试失败） |
| **P1 健壮性** | **1~2 周** | graceful shutdown、降级、限流 | 全服务支持 graceful shutdown（drain → close → wait）；cache 增加 Redis 故障降级路径；replay 改为 Redis 分布式；lock 改为 lease 模式 |
| **P1 安全加固** | **1 周** | 防攻击、防注入、防重放 | Registry 增加 Token 认证；DBProxy SQL 参数化/白名单；Gateway 增加 per-IP 速率限制；Redis Sentinel 支持 |

### 4.3 修复完成后的状态预期

P0 + P1 全部完成后，框架可达到：

| 维度 | 当前评分 | 目标评分 |
|------|---------|---------|
| 健壮性 | 3/10 | **7/10**（泄漏修复、panic recovery、graceful shutdown） |
| 测试覆盖 | 2/10 | **6/10**（核心路径有测试，关键 bug 有回归用例） |
| 安全 | 4/10 | **7/10**（注入防护、限速、认证、分布式防重放） |
| 生产就绪度 | 3/10 | **7/10（可用于内测/小服）** |

此时再开始"框架抽象层"（`framework/go/app.go`、Module 注册、Domain 接口、配置驱动 DSL），才是合理的时机。

---

## 5. 修复任务详细分解

### 5.1 `common/go/net` — 网络层修复

| # | 任务 | 工作量 | 验证方式 |
|---|------|--------|---------|
| 1 | `TCPConn.readLoop()` 所有错误路径调用 `c.Close()` | 小 ✅ 已修复 | 单元测试：模拟心跳超时，断言 `onClose` 被调用、writeLoop 退出 |
| 2 | `UpstreamPool.SendPacket()` 缓存 `conn` 局部变量 | 小 ✅ 已修复 | 单元测试：并发 Send + 模拟节点断开 |
| 3 | 所有 readLoop/writeLoop/acceptLoop/OnPacket 加 `defer recover()` | 中 | 注入 panic，断言进程不崩溃、连接被清理 |
| 4 | `UpstreamPool` 锁内阻塞 I/O 改为非阻塞或分段锁 | 中 | 压测：1000 并发 Send，latency P99 < 10ms |
| 5 | `MaxConn` 改为 `atomic.Int32` | 小 | 基准测试：连接风暴场景 CPU 占用 |
| 6 | `writeLoop` 增加 `SetWriteDeadline` | 小 | 模拟慢客户端，断言 goroutine 不泄漏 |

### 5.2 `common/go/discovery` + `registry` — 服务发现修复

| # | 任务 | 工作量 | 验证方式 |
|---|------|--------|---------|
| 7 | `Watch` 外层包 `for { ... }` 循环 + 指数退避重试 | 中 ✅ 已修复 | 模拟 etcd 重启，断言 Watch 恢复、事件不丢失 |
| 8 | etcd Delete 事件按 `NodeID` 反向查找删除 | 小 ✅ 已修复 | 单元测试：注册 → 删除 → 断言池中无该节点 |
| 9 | `RegistryImpl.Deregister` 加 `sync.Once` 防重复 close | 小 | 单元测试：两次 Deregister 不 panic |
| 10 | `registry.Client.Close()` 用 context cancel 替代 close channel | 中 | 单元测试：in-flight call 收到 context cancel 而非 nil panic |

### 5.3 `services/gateway-cpp` + `realtime-cpp` — 关键 bug 修复

| # | 任务 | 工作量 | 验证方式 |
|---|------|--------|---------|
| 11 | 修复 `OnUpstreamPacket` 单播/广播路由逻辑 | 中 ✅ 已修复 | 集成测试：单玩家请求，断言仅该玩家收到响应 |
| 12 | 修复 `OnRoomBroadcast` N² 放大问题 | 中 ✅ 已修复 | 集成测试：2 Gateway × 50 玩家，断言总转发包数 = 50 |
| 13 | Gateway 转发时显式携带 `gateway_conn_id` | 小 ✅ 已修复 | 集成测试：Realtime 响应能正确路由回源客户端 |
| 14 | Realtime 改为 per-room goroutine / per-room ComputeThread | 大 | 压测：100 rooms × 20 players，断言延迟 P99 < 50ms |

### 5.4 `services/registry-go` — 注册中心修复

| # | 任务 | 工作量 | 验证方式 |
|---|------|--------|---------|
| 15 | MemoryStore 增加后台 TTL 扫描协程 | 中 ✅ 已修复 | 单元测试：注册 → 停止心跳 → 断言 30s 后被剔除 |
| 16 | 改用 `KeepAlive` 永久流替代 `KeepAliveOnce` | 中 ✅ 已修复 | 模拟网络抖动，断言 lease 不意外过期 |
| 17 | 增加简单 Token 认证（注册/心跳需带 secret） | 小 | 单元测试：无 Token 注册被拒绝 |

### 5.5 安全与限流修复

| # | 任务 | 工作量 | 验证方式 |
|---|------|--------|---------|
| 18 | Token bucket 全局 limit 不经过 shard，per-key limit 确保 `perShardCap >= 1` | 小 ✅ 已修复 | 单元测试：`capacity=10` 时仍能限流 |
| 19 | `cache` 区分 "key 不存在" 与 "store 不可用"，后者返回错误 | 中 ✅ 已修复 | 单元测试：Redis 断开时 `GetOrLoad` 返回错误而非透传 DB |
| 20 | `replay` 改用 Redis Set + 过期时间做分布式 nonce 存储 | 中 ✅ 部分修复（dedup key 已修正） | 集成测试：多实例部署，重放同一 nonce 被所有实例拒绝 |
| 21 | `lock` 改为返回独立 lease 对象，不再修改 lock 实例 | 中 ✅ 已修复 | 单元测试：同一 lock 实例并发 TryLock，token 不冲突 |
| 22 | `logger` `level` 改为 `atomic.Int32` | 小 | `go test -race` 通过 |
| 23 | `metrics` histogram 输出改为累积计数 ❌ *[验证批注：误判，无需执行]* | ~~小~~ **无需修复** | ~~用 Prometheus parser 验证输出格式合规~~ 源码已正确实现累积计数 |

---

## 6. 附录：审计方法说明

### 6.1 审计范围

- **Go 公共库**：`common/go/{net,rpc,discovery,registry,logger,metrics,config,crypto,idgen,limiter,lock,trace,replay,errors,redis,cache}`
- **C++ 公共库**：`common/cpp/gs/{net,rpc,discovery,registry,logger,metrics,config,crypto,idgen,limiter,replay,redis,realtime}`
- **服务层**：`services/{dbproxy-go,gateway-cpp,registry-go,realtime-cpp,logstats-go}`

### 6.2 审计维度

| 维度 | 检查项 |
|------|--------|
| 功能完整性 | 是否实现了 DESIGN.md 承诺的全部功能 |
| 边界条件 | 空值、零值、越界、溢出、竞态、死锁 |
| 资源管理 | goroutine/线程泄漏、内存泄漏、句柄泄漏、连接泄漏 |
| 错误处理 | panic recovery、错误传播、降级策略、快速失败 |
| 性能 | 锁竞争、O(n) 算法、阻塞 I/O、GC 压力、CPU 燃烧 |
| 安全 | 注入、重放、竞态、越权、信息泄漏 |
| 测试覆盖 | 单元测试、并发测试、边界测试、失败路径测试 |
| 可观测性 | 日志、metrics、trace、health check |

### 6.3 关键发现统计

| 类别 | 数量 |
|------|------|
| 🚨 P0 关键缺陷（会导致生产事故） | 10 |
| 🔴 P1 严重缺陷（严重影响可靠性） | 5 |
| ⚠️ 设计缺陷或性能隐患 | 25+ |
| 零测试的核心包 | 11 个 |
| 与设计文档承诺不符的模块 | 8 个 |

---

> **下一步行动建议**：按上述 P0 → P0 测试 → P1 健壮性 → P1 安全 四阶段顺序执行修复。每阶段完成后进行全量编译 + Phase 1/2 端到端联调验证。全部完成后，框架才具备支撑上层架构抽象的条件。

---

## 7. 审计验证批注（源码交叉验证结果）

> 以下批注基于对源码的逐行交叉验证，标记报告中描述有偏差或不准确的部分。
> 验证日期：2026-05-02
> 验证结论：报告整体准确度约 **85-88%**，15 条缺陷中 10 条完全准确，4 条部分准确但结论正确，**1 条为误判**。

### 7.1 🚨 P0 缺陷验证结果

#### ⚠️ P0 #1 — `OnUpstreamPacket` 单播变广播 — **结论正确，但触发条件描述不完整**

**报告描述**：`OnUpstreamPacket` 将单播响应误广播给房间所有成员。

**源码实际情况**（`services/gateway-cpp/main.cpp` `OnUpstreamPacket` 方法）：
- 非广播路径中，代码查找 `conn_id` 所在的 room，然后将响应发送给该 room 的 **所有成员**，而非仅发送给 `conn_id` 对应的客户端。
- **但存在 fallback**：当 `targets` 为空时（即玩家不在任何 room 中），代码会直接将响应发送给 `conn_id` 对应的客户端，此时行为正确。
- **结论**：只有在玩家已加入 room 的场景下才会触发误广播。不在任何 room 的玩家不受影响。报告应补充此触发条件。

#### ✅ P0 #2 — `OnRoomBroadcast` N² 放大 — **完全准确**

`services/realtime-cpp/main.cpp` 的 `OnRoomBroadcast` 方法中，外层遍历 `target_conns`，内层遍历所有 Gateway 连接 `conns_`，形成 M×N 的流量放大。

#### ✅ P0 #3 — `seq_id` 被当作 `conn_id` — **完全准确**

`services/realtime-cpp/main.cpp` 中明确写了 `msg->conn_id = pkt.header.seq_id;` 并注释 `// 复用 seq_id 作为 conn_id（简化）`。

#### ✅ P0 #4 — `readLoop` 出错只调 `raw.Close()` — **完全准确**

`common/go/net/conn.go` 的 `readLoop()` 中所有错误路径（心跳超时、DecodeHeader 错误、payload 读取错误、解密失败）均只调用 `c.raw.Close()` 而非 `c.Close()`，导致 `closeCh` 不关闭、`writeLoop` goroutine 泄漏、`onClose` 回调不触发。

#### ✅ P0 #5 — `UpstreamPool.SendPacket` 两次求值 — **完全准确**

`common/go/net/upstream.go` 的 `SendPacket()` 中 `n.Client.Conn()` 被调用两次，`onClose` 回调会在两次调用之间将 `c.conn` 置为 nil，可能导致 nil pointer panic。

#### ⚠️ P0 #6 — etcd Delete 事件零值 — **结论正确，机制描述有偏差**

**报告描述**：etcd `Delete` 事件中 `Node.Host`/`Node.Port` 为零值，`RemoveNode(":0")` 永远不匹配。

**源码实际情况**（`common/go/discovery/etcd_impl.go`）：
- Delete 事件确实不解析 Host/Port，只从 key 中提取 ServiceType 和 NodeID。
- 但 `common/go/discovery/upstream_manager.go` 的 `onNodeEvent` 方法在 `ev.Node.Host == ""` 时直接 `return`，**根本不会调用 `RemoveNode`**。
- **结论**：实际后果是 Leave 事件被静默忽略（死节点无法移除），结论正确，但机制不是 `RemoveNode(":0")` 不匹配，而是更早阶段就被过滤了。

#### ✅ P0 #7 — `Watch` 永不重试 — **完全准确**

`common/go/discovery/upstream_manager.go` 中 Watch 失败后只打日志就退出，无重试循环。

#### ✅ P0 #8 — MemoryStore 节点永不过期 — **完全准确**

`services/registry-go/internal/store/memory_store.go` 的 `Heartbeat` 方法仅检查节点是否存在，不更新最后活跃时间，也无后台 TTL 扫描。

#### ⚠️ P0 #9 — `KeepAliveOnce` vs `KeepAlive` — **准确但需区分模块**

**报告描述**：使用 `KeepAliveOnce` 而非 `KeepAlive`。

**源码实际情况**：
- `services/registry-go/internal/store/etcd_store.go`（Registry 服务端）确实使用 `KeepAliveOnce`，**有问题**。
- `common/go/discovery/etcd_impl.go`（Discovery 客户端）使用的是 `KeepAlive`（永久流），**没有问题**。
- 报告将两者混为一谈，应明确指出问题仅在 `EtcdStore` 中。

#### ✅ P0 #10 — Token bucket shard 均分失效 — **完全准确**

`common/go/limiter/token_bucket.go` 中 `perShardCap = float64(capacity) / 64`，当 `capacity=10` 时 perShard 容量仅 0.156，不足 1 个令牌，限流完全失效。测试 `TestTokenBucket` 用 `capacity=10` 确实会失败。

---

### 7.2 🔴 P1 缺陷验证结果

#### ✅ P1 #11 — Cache Redis 故障透传 DB — **准确**

`common/go/cache/cache.go` 的 `Get()` 方法将所有 store 错误（包括 Redis 连接失败）统一包装为 `ErrNotFound`，`GetOrLoad` 随后触发回源，导致 Redis 不可用时所有请求透传到 DB。

#### ✅ P1 #12 — Replay dedup key 含 timestamp — **完全准确**

`common/go/replay/replay.go` 中 `key := ts.UTC().Format(time.RFC3339Nano) + ":" + nonce`，相同 nonce 搭配不同 timestamp 可绕过。

#### ✅ P1 #13 — Lock struct 竞态 — **完全准确**

`common/go/lock/redis_lock.go` 的 `TryLock` 中 `l.value = value` 直接修改 struct 字段，多 goroutine 共用实例会互相覆盖。

#### ✅ P1 #14 — 子 logger 共享 stopCh — **完全准确**

`common/go/logger/logger.go` 的 `With()` 方法中 `stopCh: l.stopCh` 共享了 channel。

#### ❌ P1 #15 — Histogram bucket 非累积 — **报告误判**

**报告描述**：Histogram bucket 计数非累积，Prometheus 格式无效。

**源码实际情况**（`common/go/metrics/metrics.go`）：
```go
// Observe 方法：
for i, b := range h.buckets {
    if v <= b {
        h.counts[i]++
    }
}
```
这段代码对 **所有** 满足 `v <= b` 的 bucket 都执行 `++`，这正是 **累积计数** 的正确实现。例如 buckets=[1,5,10]，观测值 3 会使 buckets[1]（le=5）和 buckets[2]（le=10）各加 1，而 buckets[0]（le=1）不加。

输出部分直接输出 `counts[i]`，由于已经是累积值，Prometheus exposition 格式是合规的。

**结论**：此缺陷 **不存在**。代码实现的是正确的累积计数，Prometheus 格式有效。对应的修复任务 #23（"histogram 输出改为累积计数"）也不需要执行。

---

### 7.3 模块详细评估中的偏差

#### ⚠️ 3.3 可观测性 — metrics 描述有两处偏差

1. **"bucket 计数非累积，Prometheus exposition 格式无效"** — ❌ 误判，如上 P1 #15 分析。
2. **"`Histogram.Observe` 将 `float64` 截断为 `int64`"** — ⚠️ 部分准确。`Observe(ms float64)` 内部确实用 `v := int64(ms)` 截断，这对 sum 精度有影响（丢失亚毫秒精度），但对 bucket 分配影响极小（bucket 边界都是整数毫秒）。精度损失是真实问题，但严重性被夸大了。

#### ⚠️ 3.2 安全与可靠性 — limiter 熔断器描述偏差

**报告描述**：熔断器 `Allow()` 在 CAS 失败时错误返回 `false`。

**源码实际情况**（`common/go/limiter/circuit_breaker.go`）：
- Open → HalfOpen 转换使用 `cb.state.CompareAndSwap(int32(StateOpen), int32(StateHalfOpen))`。
- CAS 失败意味着另一个 goroutine 已经完成了转换，此时返回 `false` 让请求重试是合理的退避策略，并非"错误"。
- **结论**：这是一个保守但合理的设计选择，不是 bug。

#### ⚠️ 3.5 服务层 — gateway-cpp "无客户端心跳超时" 需补充

报告说 gateway-cpp 无客户端心跳超时。实际 `Gateway` 类有 `HeartbeatLoop` 线程，但它只清理 pending_bind 超时记录，不检测客户端连接活跃度。Gateway 确实依赖底层 TCP keepalive 或 WebSocket ping/pong，而非应用层心跳超时。结论基本正确，但表述可更精确。

---

### 7.4 总体评估中的偏差

#### ⚠️ 1. 总体评估表 — "Prometheus histogram 格式无效"

此判断基于 P1 #15 的误判，应修正为"Prometheus histogram 基本可用，但 sum 精度因 int64 截断有亚毫秒损失"。

#### ⚠️ 4.2 修复计划 — 任务 #23 不需要执行

"metrics histogram 输出改为累积计数"任务基于误判，无需执行。
