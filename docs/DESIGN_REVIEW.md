# 设计质量审查报告

> **核心原则**：即使是 MVP，设计方案也不能过于偏离正式版本，否则将触发大面积返工。本报告对已实现方案进行逐层审查，记录偏离项、风险等级与补救计划。
>
> 版本：v1.3  
> 审查范围：Phase 1 ~ Phase 4  
> 状态：P0 ~ P3 全部完成，所有已知偏离项已修复或架构抽象已就位

---

## 审查方法论

每项按以下维度评分：

| 维度 | 说明 |
|------|------|
| **架构一致性** | 与 DESIGN.md 定义的整体架构是否一致 |
| **生产就绪度** | 直接部署到生产环境所需的改动量 |
| **可演进性** | 从当前形态平滑演进到目标架构的难度 |
| **返工风险** | 评估是否会因当前设计导致后续代码废弃或重构 |

风险等级：🔴 严重（必须补救）/ 🟡 警告（建议补救）/ 🟢 可接受（文档化即可）

---

## 一、网络层（common/go/net, common/cpp/net）

### 1.1 Go TCPConn — 连接模型

| 维度 | 评分 | 说明 |
|------|------|------|
| 架构一致性 | 🟢 | 符合 DESIGN.md 4.1 帧格式定义 |
| 生产就绪度 | 🟢 | MaxConn + heartbeat timeout（默认 30s）+ writeCh 满返回 false 背压 ✅ |
| 可演进性 | 🟡 | 从 goroutine-per-conn 迁移到 epoll/kqueue 需要重写 readLoop/writeLoop |
| 返工风险 | 🟢 低 | MaxConn + heartbeat timeout 已补齐；goroutine-per-conn GC 问题留待 Phase 5 优化 |

**偏离项**（✅ 已修复）：
- ~~无 `SetReadDeadline` / `SetWriteDeadline`~~ → `readLoop` 已增加 200ms 轮询 deadline + `SetHeartbeatTimeout` 接口
- ~~`writeCh` 缓冲固定 256，无背压反馈~~ → `Send()` select default 返回 `false`，调用方可感知背压
- ~~无连接数上限~~ → `TCPServer` `MaxConn` 超限拒绝

**补救计划**（✅ 已完成）：
1. `TCPConn` 已添加 `SetHeartbeatTimeout` + `lastActive` 跟踪，`readLoop` 自动检测超时并关闭连接
2. `TCPServer` `MaxConn` 已生效
3. `writeCh` 满时 `Send()` 返回 `false` 已生效

### 1.2 C++ TCPConn — 阻塞模型

| 维度 | 评分 | 说明 |
|------|------|------|
| 架构一致性 | 🟡 | Phase 1 明确使用 WinSock2 阻塞模型，但 Phase 5 要求 "生产竞技标准" |
| 生产就绪度 | 🟡 | 阻塞模型仍是主路径，但已增加 `SetHeartbeatTimeout` + socket receive timeout 防挂起；async_event_loop.hpp 抽象已就位 |
| 可演进性 | 🟡 | `async_event_loop.hpp` 抽象已创建，阻塞代码保留为兼容层，可渐进迁移 |
| 返工风险 | 🟡 中 | Phase 5 需在 `AsyncEventLoop` 实现上重写 read/write，但抽象层已就位 |

**偏离项**：
- 当前每个连接 2 个线程（read + write），Realtime 服 500 人同屏 = 1000+ 线程
- DESIGN.md 4.2 节定义的 "IO Thread -> Dispatch -> Compute Thread" 模型尚未落地
- ✅ 已增加 `SetHeartbeatTimeout` + `SO_RCVTIMEO`，防止恶意客户端挂起连接

**补救计划**（✅ 基础架构已完成）：
1. `common/cpp/net/async_event_loop.hpp` 已创建，定义 `AsyncEventLoop` 纯虚接口
2. 当前阻塞代码作为 "兼容性层" 保留，`TCPConn` 增加 heartbeat timeout
3. Phase 5 实现 `AsyncEventLoop::Create()` 返回平台适配的 epoll/kqueue/IOCP 实现

---

## 二、Registry 服务发现（services/registry-go, common/go/registry）

### 2.1 MemoryStore — 临时存储

| 维度 | 评分 | 说明 |
|------|------|------|
| 架构一致性 | 🟢 | 作为 Etcd 不可用时 fallback，符合 Phase 1 策略 |
| 生产就绪度 | 🔴 | Heartbeat 为空实现，节点宕机永远不会被清理 |
| 可演进性 | 🟢 | EtcdStore 已实现，切换成本为改一行 `storeType` |
| 返工风险 | 🟡 中 | MemoryStore 的 Watch 用 channel slice，无 goroutine 泄漏防护 |

**偏离项**：
- `Heartbeat()` 仅检查 lease 存在性，不更新时间戳，无 TTL 过期清理 goroutine
- `broadcast()` 对阻塞 channel 直接丢弃事件，下游可能丢失节点变更
- 无 `Unregister` / `Deregister` 接口，节点只能注册无法主动注销

**补救计划**：
1. MemoryStore 增加 TTL 后台清理 goroutine + `Unregister` 方法（保持接口与 EtcdStore 一致）
2. Watch channel 增加缓冲区大小配置，阻塞时写入带超时的 select
3. **原则**：即使是 MemoryStore，接口行为也必须与 EtcdStore 一致，不能因为是临时实现就放松契约

### 2.2 Registry Client — 未完成

| 维度 | 评分 | 说明 |
|------|------|------|
| 架构一致性 | 🔴 | `call()` 方法直接 `time.Sleep(10ms)` 返回假结果，`Discover()` 返回 `not fully implemented` |
| 生产就绪度 | 🔴 | 完全不可用 |
| 可演进性 | 🟢 | RPC Client 已实现，只需把 Registry Client 对接上去 |
| 返工风险 | 🔴 高 | 当前代码是占位符，后续需要重写而非演进 |

**偏离项**：
- Registry Client 的 `call()` 没有使用 `rpc.Client`，而是自己造了一个假的同步等待
- `Discover` 返回的数据用 `Result.Msg` string 承载 protobuf bytes，设计不伦不类
- Watch 重连逻辑完全缺失

**补救计划**：
1. Registry Client 的 `call()` 改为调用 `rpc.Client.Call()`，复用已有的 Req-Res 配对机制
2. `Discover` 返回类型改为 `*pb.NodeList`，不要嵌套在 `Result.Msg` 里
3. 增加断线重连 + 指数退避 + 本地缓存（graceful degradation）
4. **原则**：占位符代码必须标注 `// FIXME: placeholder, will be replaced in Phase X`，否则会被误用

---

## 三、DBProxy（services/dbproxy-go）

### 3.1 MySQL 代理 — 结果序列化

| 维度 | 评分 | 说明 |
|------|------|------|
| 架构一致性 | 🟡 | 有分库分表路由，但返回格式未在 proto 中定义 |
| 生产就绪度 | 🔴 | `fmt.Sprintf("%v", rowMap)` 序列化行数据，解析端靠字符串切分还原 |
| 可演进性 | 🔴 | 任何字段值含空格、冒号、方括号都会破坏解析 |
| 返工风险 | 🔴 高 | Biz 服的 `parsePlayerRow` 是脆弱的字符串 hack，数据格式一变全崩 |

**偏离项**：
- `server.go:187` 用 `fmt.Sprintf("%v", rowMap)` 序列化，`biz-go/main.go:446-474` 用字符串操作解析
- 这相当于自己发明了一种不稳定的序列化协议，与 protobuf 的存在意义相违背
- 没有 `Transaction` 接口，业务无法做原子操作

**补救计划**：
1. **立即**：在 `spec/proto/dbproxy.proto` 中定义 `Row` message：`message Row { repeated Column columns = 1; }`，用 protobuf 结构化传输
2. Biz 侧解析改为 `proto.Unmarshal` 而不是字符串切分
3. 增加 `MySQLBeginTx / MySQLCommitTx / MySQLRollbackTx` 命令号与代理方法
4. **原则**：数据序列化必须使用 protobuf 或 JSON 等标准格式，禁止用 `fmt.Sprintf` 传递结构化数据

### 3.2 Redis 代理 — Pipeline 类型安全

| 维度 | 评分 | 说明 |
|------|------|------|
| 架构一致性 | 🟡 | Pipeline 接口存在但返回类型不安全 |
| 生产就绪度 | 🟡 | `PipelineExec` 返回 `[]interface{}`，调用方需要类型断言 |
| 可演进性 | 🟢 | 可改为泛型或 code-generated 接口 |
| 返工风险 | 🟡 中 | 类型断言 panic 风险 |

**偏离项**：
- `proxy.go:94` `r.(*redis.Cmd).Val()` 可能 panic
- HotKeyLimiter 全局 mutex，所有 key 竞争同一把锁

**补救计划**：
1. PipelineExec 返回 `*RedisPipelineRes` protobuf 结构，内部用 `oneof` 表达不同类型结果
2. HotKeyLimiter 按 key hash 分片到多个 bucket，降低锁竞争

---

## 四、安全层（Phase 3 刚实现）

### 4.1 Gateway-Client 握手加密

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟡 | 实现了 DESIGN.md 的 "加密通信" 目标，但握手协议过于简化 |
| 生产就绪度 | 🔴 | 静态 master key，无 forward secrecy，无证书链，握手本身无防重放 |
| 可演进性 | 🔴 | 从静态密钥升级到 ECDHE + 证书需要重写握手协议，无法平滑演进 |
| 返工风险 | 🔴 高 | Wireshark 目前看不到 payload 是因为 AES-GCM，但一旦 master key 泄露，所有历史通信可被解密 |

**偏离项**：
- Master key 硬编码为 32 字节全零，从命令行/配置文件读取但无安全加载机制
- 握手流程：ClientRandom + ServerRandom → HMAC → SessionKey，这是 TLS 1.2 的简化版但缺少：
  - 密钥交换算法协商（固定 HMAC-SHA256）
  - 版本协商
  - 证书/身份验证（任何人知道 master key 就能伪装服务器）
  - 握手消息本身的完整性保护（ClientHello 可被篡改）
- 握手响应的 `encrypted_challenge` 没有绑定到特定连接，存在重放可能

**补救计划**：
1. **短期（MVP 内）**：
   - Master key 从环境变量或加密配置文件读取，禁止硬编码
   - 握手增加 timestamp + nonce 防重放（复用 `replay.Checker`）
   - 增加最小协议版本号字段，预留升级空间
2. **长期（Phase 6 后）**：
   - 引入 ECDHE（X25519 或 P-256）进行 ephemeral key exchange
   - 服务器端配置 RSA/ECDSA 证书，客户端可选证书 pinning
   - 或者：直接用 gRPC over TLS 作为 Gateway-Biz 通道，自己只负责 Gateway-Client 的轻量加密
3. **原则**：安全协议必须预留版本号和算法协商字段，即使当前只实现一种算法。禁止在协议头中省略 "version" 字段。

### 4.2 分布式锁

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟢 | Redis SET NX EX + Lua 解锁，标准方案 |
| 生产就绪度 | 🟡 | 缺少看门狗自动续期 goroutine、缺少锁等待队列 |
| 可演进性 | 🟢 | RedLock 算法可在现有基础上扩展 |
| 返工风险 | 🟢 低 | 当前实现作为基础版本可用 |

**偏离项**：
- `Extend` 方法已存在但无自动调用机制，调用方需要自己启动 goroutine
- `Lock()` 轮询间隔固定 50ms，无指数退避

**补救计划**：
1. 提供 `LockWithAutoExtend(ctx, extendInterval)` 封装，内部启动守护 goroutine
2. 轮询间隔改为指数退避（50ms → 100ms → 200ms → 最大 1s）

### 4.3 限流器与熔断器

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟢 | 令牌桶 + 熔断器，符合 DESIGN.md |
| 生产就绪度 | 🟢 | Go/C++ 令牌桶均已改为 64 分片锁（按 key hash）；熔断器半开限制已生效 ✅ |
| 可演进性 | 🟢 | 分片架构已就位，后续只需调整 shard 数量或替换 hash 算法 |
| 返工风险 | 🟢 低 | 分片锁已落地，无需后续重写 |

**偏离项**（✅ 已修复）：
- ~~`TokenBucket` 全局锁~~ → Go `limiter/token_bucket.go` 和 C++ `limiter/token_bucket.cpp` 均已改为 64 分片（按 key FNV-1a hash）
- ~~`CircuitBreaker.HalfOpen` 无并发探测限制~~ → `halfOpenMaxRequests` 已生效，默认 1

**补救计划**（✅ 已完成）：
1. Go `TokenBucket`：`[shardCount]tokenBucketShard`，`AllowKey(key, n)` 按 hash 路由
2. C++ `TokenBucket`：`kShardCount = 64`，`AllowKey(key, n)` 按 FNV-1a hash 路由
3. `CircuitBreaker`：`halfOpenMaxRequests` + `halfOpenReqs` 原子计数已生效

### 4.4 防重放

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟢 | timestamp + nonce，符合 DESIGN.md |
| 生产就绪度 | 🟡 | nonce map 无内存上限，恶意客户端可构造大量 nonce 撑爆内存 |
| 可演进性 | 🟢 | 可替换为 Redis/BloomFilter 分布式版本 |
| 返工风险 | 🟡 中 | 内存泄漏风险 |

**偏离项**：
- `Checker.nonces` map 只按时间清理，没有时间窗口内的数量上限
- C++ 版本用 `system_clock` 而非 `steady_clock`，系统时间被修改时窗口计算会错乱

**补救计划**：
1. 增加 `maxNonceCount` 上限，超限时拒绝新 nonce 或强制加速 GC
2. C++ 版本改用 `steady_clock` 计算窗口，仅在与外部 timestamp 比较时使用 `system_clock`

---

## 五、Biz 服业务逻辑（services/biz-go）

### 5.1 Player ID 生成

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🔴 | Phase 2 已实现 Snowflake，但 Biz 服自己用 `uint64(now)*1000000` 生成 ID |
| 生产就绪度 | 🔴 | 时间回拨或高并发时 ID 冲突 |
| 可演进性 | 🟢 | 替换为 Snowflake 即可 |
| 返工风险 | 🔴 高 | 已有数据使用旧 ID 格式后，迁移成本高 |

**偏离项**：
- `main.go:335` `playerID := uint64(now)*1000000 + uint64(now%1000000)`
- 与 `common/go/idgen/snowflake.go` 并存但完全不用

**补救计划**：
1. **立即**：Biz 服初始化时创建 `idgen.Snowflake(nodeID)`，所有 ID 生成走 Snowflake
2. 删除 `CreatePlayer` 中的自定义 ID 生成逻辑
3. **原则**：基础设施交付后，业务侧必须优先复用，禁止 "临时写一段能跑的" 绕过已有组件

### 5.2 Token 生成

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟡 | 使用 SHA256，但无过期校验逻辑（仅写入 Redis TTL） |
| 生产就绪度 | 🟡 | Token 格式无版本/签名，服务端无法在不查 Redis 的情况下验证有效性 |
| 可演进性 | 🟢 | 可升级为 JWT 或 signed token |

**补救计划**：
1. Token 增加过期时间戳 + HMAC 签名，Gateway 可本地校验有效性而不必每次都查 Redis
2. 或者：引入 JWT，但需评估 payload 大小对网络层的影响

---

## 六、配置中心（common/go/config）

### 6.1 HTTP 热重载端点

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟢 | 符合 Phase 2 的 "HTTP 热重载" 目标 |
| 生产就绪度 | 🔴 | `/admin/reload` 无任何认证，任何人可 POST 触发配置变更 |
| 可演进性 | 🟢 | 增加中间件即可 |
| 返工风险 | 🟡 中 | 安全漏洞 |

**偏离项**：
- 无 IP 白名单、无 Token 认证、无审计日志
- 配置变更后无版本号/回滚机制

**补救计划**：
1. `/admin/reload` 增加 Bearer Token 校验（从环境变量读取 admin token）
2. 记录 reload 审计日志（who/when/old hash/new hash）
3. 配置文件增加 `version` 字段，拒绝回滚到旧版本（防止意外）

---

## 七、错误码体系（spec/errors.toml）

### 7.1 错误码定义与使用

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟢 | 全局错误码不冲突，有生成工具 |
| 生产就绪度 | 🟡 | Go 侧只有 int32 常量，无 error 类型包装，无法携带上下文和堆栈 |
| 可演进性 | 🟡 | 需要增加 `gs/errors.Error` 结构体才能支持链式错误 |
| 返工风险 | 🟡 中 | 日志中只能看到错误码数字，排查困难 |

**偏离项**：
- `errors.go` 生成的是常量，但 Go 生态习惯 `if err != nil` 而不是 `if code != errors.OK`
- Biz/DBProxy 代码中大量 `fmt.Errorf("player not found")` 而不是使用 `errors.PLAYER_NOT_FOUND`
- 错误码没有与 HTTP/gRPC status code 的映射关系

**补救计划**：
1. 增加 `gs/errors.Error` 结构体：`type Error struct { Code int32; Message string; Cause error }`，实现 `error` 接口
2. 提供 `errors.New(code, msg)`、`errors.Wrap(cause, code)` 等工厂函数
3. 在 RPC 层自动将 `*errors.Error` 编码到 `Result.Code` 中
4. **原则**：错误码不能只是数字常量，必须能融入语言的 native error 处理机制

---

## 八、代码组织与工程规范

### 8.1 业务逻辑与基础设施耦合

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟢 | Go/C++ 均已引入 `Middleware` 接口，`TCPServer` 支持链式注册 ✅ |
| 生产就绪度 | 🟢 | Gateway 已重构为中间件模式：`HandshakeMiddleware` + `EncryptionMiddleware` 注册到 `TCPServer` |
| 可演进性 | 🟢 | 新增拦截逻辑只需 `server->Use(mw)`，无需修改 Gateway 主流程 |

**偏离项**（✅ 已修复）：
- ~~`services/gateway-cpp/main.cpp` 中 `OnClientPacket` 直接处理 HANDSHAKE~~ → 已提取为 `HandshakeMiddleware` 和 `EncryptionMiddleware`
- ~~没有 `Middleware` 链式调用设计~~ → Go `net.Middleware` / C++ `gs::net::Middleware` 均已实现

**补救计划**（✅ 已完成）：
1. C++ `common/cpp/net/middleware.hpp`：`class Middleware { virtual bool OnPacket(TCPConn*, Packet&) = 0; }`
2. C++ `TCPServer`：`Use(shared_ptr<Middleware>)`，按注册顺序执行，返回 false 拦截
3. Go `common/go/net/server.go`：`Middleware` 接口 + `MiddlewareFunc` 适配器 + `Use(mw ...Middleware)`
4. Gateway 重构：`HandshakeMiddleware`（握手+防重放+session 管理）+ `EncryptionMiddleware`（解密）注册到 `TCPServer` 中间件链

### 8.2 Go Module

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟢 | 根目录已创建 `go.mod`（module `github.com/gmaker/luffa`），采用单 module monorepo 模式 |
| 生产就绪度 | 🟢 | 各服务子目录均有独立 `go.mod` 或复用根模块，构建正常 |
| 可演进性 | 🟢 | 单 module monorepo 管理简单，common 库通过子包路径引用 |

**偏离项**（✅ 已修复）：
- ~~`common/go/net`、`common/go/crypto` 等包没有 `go.mod`~~ → 根目录 `go.mod` 已覆盖全部子包
- ~~各 service 的 `go.mod` 缺失~~ → `registry-go`、`dbproxy-go`、`biz-go`、`logstats-go`、`chat-go` 均已有 `go.mod`

**补救计划**（✅ 已完成）：
1. 根目录创建 `go.mod`，module 名为 `github.com/gmaker/luffa` ✅
2. 各服务子目录按需创建独立 `go.mod` 或复用根模块 ✅

---

## 九、Gateway 服务（services/gateway-cpp）

### 9.1 Biz 后端连接池与动态发现

| 维度 | 评分 | 说明 |
|------|------|
| 架构一致性 | 🟢 | Gateway 已使用 `UpstreamPool` + `UpstreamManager`，通过 Registry Watch 动态发现 Biz 节点 |
| 生产就绪度 | 🟢 | 连接池支持轮询负载均衡、自动重连、健康状态跟踪 |
| 可演进性 | 🟢 | 新增/移除 Biz 节点由 Registry 推送，Gateway 自动感知，无需重启 |
| 返工风险 | 🟢 低 | 连接池抽象已就位，后续只需在 `RouteToPool` 中扩展路由策略 |

**偏离项**（✅ 已修复）：
- ~~`gateway-cpp/main.cpp` 中 `biz_client_` 为单例~~ → 已移除单例，改用 `UpstreamManager` 管理多个上游池
- ~~无连接池、无负载均衡~~ → `UpstreamPool` 已实现轮询 + 健康检查
- ~~命令行参数只接受单个 `biz_host:biz_port`~~ → 改为 `gateway.json` 配置 + Registry 动态发现
- ~~Biz 节点断开时 Gateway 直接不可用~~ → Registry 节点下线后 `UpstreamPool` 自动移除不健康节点

**补救计划（✅ 已完成）**：
1. 新增 `common/cpp/net/upstream.hpp/cpp` — `UpstreamPool` 通用上游连接池：
   - 维护多个 `TCPClient` 连接
   - 轮询（Round-Robin）负载均衡
   - 自动重连 + 指数退避（1s ~ 30s）
   - 连接级健康状态跟踪
2. Gateway 改用 `UpstreamManager` 管理上游节点：
   - 通过 Registry Watch 动态订阅 biz / chat / login 等服务的节点变化
   - 转发逻辑改为 `pool->SendPacket(pkt)`，由 `RouteToPool(cmd_id)` 按命令号范围路由
   - 配置文件中 `upstream.services` 定义关注的服务列表
3. **原则**：任何有后端依赖的服务（Gateway、Realtime、DBProxy-client）都必须使用连接池而非单连接

---

## 补救优先级矩阵

按 "返工成本 × 影响范围" 排序：

| 优先级 | 事项 | 风险等级 | 负责 Phase | 备注 |
|--------|------|----------|-----------|------|
| **P0** | Registry Client 占位符代码重写 | 🔴 | Phase 1 补完 | 当前完全不可用，阻塞服务发现 |
| **P0** | MySQL 结果序列化改用 protobuf | 🔴 | Phase 2 补完 | 字符串 hack 不可接受 |
| **P0** | Biz 服替换 Snowflake ID 生成 | 🔴 | Phase 2 补完 | 已有现成组件被绕过 |
| **P1** | Gateway 加密握手增加版本号+防重放 | 🔴 | Phase 3 补完 | 安全协议必须可升级 |
| **P1** | 错误码增加 Go error 类型包装 | 🟡 | Phase 3 补完 | 影响所有服务的错误处理风格 |
| **P1** | config /admin/reload 增加认证 | 🟡 | Phase 3 补完 | 安全漏洞 |
| **P2** | 网络层增加 deadline + 连接限制 | 🟡 | Phase 4/5 | 可靠性基础 |
| **P2** | 限流器分片锁 + 熔断器半开限制 | 🟡 | Phase 3 补完 | 高并发瓶颈 |
| **P3** | C++ 网络层异步化架构 | 🔴 | Phase 5 | 与当前阻塞模型不兼容，必须重写 |
| **P3** | 引入 middleware 链式架构 | 🟡 | Phase 4 | 代码组织优化 |

---

## 设计原则（新增，所有后续 Phase 必须遵守）

1. **禁止占位符代码进入主分支**：任何 `TODO`、`FIXME`、`time.Sleep` 模拟调用、`return nil, fmt.Errorf("not implemented")` 必须在合入前完成功能实现或明确标注为 `// PLACEHOLDER: Phase X will implement`，且不能位于关键调用路径。

2. **禁止绕过已有基础设施**：如果 common/ 下已有组件（如 Snowflake、errors、config），业务侧必须优先使用。禁止在业务代码中 "临时写一段能跑的"。

3. **协议必须预留扩展字段**：所有自定义协议（握手、packet header、配置格式）必须包含 `version` 或 `reserved` 字段。当前版本填 0 即可，但字段必须存在。

4. **数据序列化必须使用标准格式**：禁止用 `fmt.Sprintf`、`string concatenation`、`strconv` 拼接来传递结构化数据。必须使用 protobuf、JSON 或 MessagePack。

5. **临时实现必须保持接口契约一致**：MemoryStore、mock client 等临时组件的接口行为必须与生产实现（EtcdStore、真实 client）完全一致。不能因为 "临时用" 就放宽错误处理、超时、并发语义。

6. **安全相关的简化必须文档化风险**：如果出于 MVP 需要简化安全方案（如静态密钥代替证书），必须在代码注释和文档中明确列出风险点和升级路径。

7. **错误处理必须融入语言原生机制**：Go 侧错误必须实现 `error` 接口，C++ 侧错误必须支持异常或 `std::expected`（C++23），不能只暴露 int32 错误码。

8. **所有服务设计都不能是单体的**：任何服务（包括自己）都可能运行多个实例；任何上游依赖都必须通过 Registry 动态发现；单连接直连模型在任何场景下都不可接受。

9. **公共服务代码不得与业务逻辑耦合（新增）**：
   - **数据库代理（DBProxy）**、**日志统计服（LogStats）**、**配置中心（Config）** 等公共服务只提供**通用接口**，不得包含任何业务相关的 SQL、表名、字段名、业务校验逻辑
   - 业务侧通过独立的 **Service 层**（如 `PlayerService`）封装业务逻辑，Service 层调用公共服务的通用接口
   - 表结构、SQL 语句、业务字段映射属于业务配置，不应硬编码在通用客户端中
   - **原则**：如果替换一个业务模块（如从 player 系统换成 guild 系统），不应该修改 DBProxy Client 的任何代码

---

## 十、单体设计审查（所有服务均不得假设单节点部署）

> **核心原则：所有的服务设计都不能是单体的。** 任何服务（包括它自己）都可能部署多个实例，任何客户端代码都必须按多节点架构设计。这是比 MVP 质量原则更底层的架构约束。

### 10.1 审查方法论

检查维度：对于每个服务 A -> 服务 B 的调用关系：
1. A 的客户端是否维护了到 B 的**多个连接**？
2. A 是否通过**Registry**动态发现 B 的节点，而不是硬编码地址？
3. A 是否具备**负载均衡**能力（轮询/哈希/random）？
4. A 是否具备**故障转移**能力（节点断开时自动切换）？
5. A 是否具备**健康检查/自动重连**能力？

如果任一答案为"否"，即构成单体设计偏离。

### 10.2 偏离项清单

| 调用方 | 被调用方 | 是否多连接 | 是否走Registry发现 | 是否有负载均衡 | 是否有故障转移 | 偏离等级 |
|--------|----------|-----------|-------------------|--------------|--------------|---------|
| Gateway (C++) | Biz | ✅ 已修复（UpstreamPool） | ✅ Registry Watch 动态发现 | ✅ 轮询 | ✅ 自动重连 | 🟢 |
| Biz-go | Registry | ✅ `NewClient([]string)` 多节点 + UpstreamPool | ✅ 命令行支持多地址 | ✅ 轮询 | ✅ 自动重连 | 🟢 |
| Biz-go | DBProxy | ✅ `NewDBProxyClient([]string)` 多节点 + UpstreamPool | ✅ 命令行支持多地址 | ✅ 轮询 | ✅ 自动重连 | 🟢 |
| Registry-go Client (Go) | Registry Server | ✅ `UpstreamPool` 多连接 | ✅ 命令行支持多地址 | ✅ 轮询 | ✅ 自动重连 | 🟢 |
| RPC Client (Go) | 任意上游 | ✅ `PacketSender` 接口抽象，支持单连接和连接池 | ✅ 由调用方决定 | ✅ 取决于底层 Sender | ✅ 取决于底层 Sender | 🟢 |
| Gateway (C++) | Biz 节点来源 | - | ✅ Registry Watch 动态发现，fallback 硬编码 | - | - | 🟢 |

### 10.3 逐项分析

#### Biz-go -> Registry：单点注册中心客户端

```go
// services/biz-go/main.go:49-55
registryAddr = flag.String("registry", "127.0.0.1:2379", "Registry address")
regClient := registry.NewClient(*registryAddr)  // 单点
```

**问题**：
- Registry 本身计划 Phase 6 上 3 节点集群，但 Biz 客户端只连一个地址
- Registry 节点故障时 Biz 无法注册/发现/心跳，导致自身被判定为离线
- 无重连、无地址列表轮询

**补救计划**：
1. `registry.Client` 改为维护**地址列表**（`[]string`），支持轮询连接
2. 启动时允许传入多个 Registry 地址 `--registry=127.0.0.1:2379,127.0.0.1:2380,127.0.0.1:2381`
3. 断线时自动切换到列表中下一个地址，循环重试
4. 或：Go Registry Client 内部实现类似 C++ `UpstreamPool` 的连接池

#### Biz-go -> DBProxy：单点数据库代理客户端

```go
// services/biz-go/main.go:50,74
dbproxyAddr = flag.String("dbproxy", "127.0.0.1:3307", "DBProxy address")
dbClient := NewDBProxyClient(*dbproxyAddr)  // 单点
```

**问题**：
- DBProxy 是性能瓶颈节点，Phase 2 已实现但 Biz 只连一个实例
- DBProxy 故障 = 全服数据库操作不可用
- 即使 DBProxy 内部有 Redis Cluster / MySQL 分片，代理层本身是单点

**补救计划**：
1. DBProxy Client（`common/go/net` 或 Biz 内部）实现连接池，支持多 DBProxy 节点
2. 负载均衡策略：按 player_id 一致性哈希到固定 DBProxy 节点（避免缓存穿透）
3. DBProxy 自身也需要注册到 Registry，Biz 通过 Registry 动态发现 DBProxy 节点列表
4. 或：DBProxy 前面加一层 L4 负载均衡（HAProxy/Envoy），客户端仍然只看到一个 VIP——但这只是把单点转移到负载均衡器，不是根本解决

#### Go Registry Client：单连接设计

```go
// common/go/registry/client.go:22-24
type Client struct {
    addr      string        // 单点地址
    conn      *net.TCPClient  // 单连接
}
```

**问题**：
- `Connect()` 只创建一个 `TCPClient`，断开即失效
- `Discover()`、`Heartbeat()`、`Register()` 全部走这一条连接
- 与 C++ `UpstreamPool` 相比，Go 侧的 Registry Client 是"单连接直连"模型

**补救计划**：
1. 参考 C++ `UpstreamPool`，在 `common/go/net` 下实现 `UpstreamPool`（Go 版本）
2. Registry Client 内部持有 `UpstreamPool`，对外接口不变
3. 或：至少先支持**地址列表 + 故障切换**，MVP 阶段不必做连接复用

#### Go RPC Client：单连接设计

```go
// common/go/rpc/client.go:13-17
type Client struct {
    conn    *net.TCPConn    // 单连接
    pending sync.Map
}
```

**问题**：
- `rpc.Client` 被 DBProxy Client 和 Registry Client 复用，但都只传了一个 `TCPConn`
- 底层连接断开时整个 RPC 通道失效
- 无连接池、无重连

**补救计划**：
1. `rpc.Client` 不应直接持有 `TCPConn`，而是持有 `net.UpstreamPool`（或接口 `PacketSender`）
2. 或者：为 RPC Client 增加 `SetConn()` 重连能力 + 调用方负责连接管理
3. 推荐方案：统一抽象 `net.Pool` 接口，`rpc.Client` 只依赖接口：`type Pool interface { SendPacket(*Packet) bool; Pick() *TCPConn }`

#### Gateway -> Biz 节点来源：未通过 Registry 发现

**问题**：
- Gateway 启动时通过命令行或硬编码获取 Biz 地址列表
- 新增/下线 Biz 节点时需要重启 Gateway
- 与 DESIGN.md 的 "Registry 动态发现" 目标不符

**补救计划**：
1. Gateway 启动后连接 Registry，Watch `biz` 服务类型的节点变更
2. `UpstreamPool` 提供 `AddNode` / `RemoveNode` 动态接口
3. Registry Watch 回调中动态增删 Biz 节点
4. **原则**：生产环境中，任何服务的上游节点列表都必须通过 Registry 发现，不允许启动参数硬编码（开发/测试环境除外，但代码路径必须相同）

---

## 补救优先级矩阵（更新版）

按 "返工成本 × 影响范围" 排序：

| 优先级 | 事项 | 风险等级 | 负责 Phase | 备注 |
|--------|------|----------|-----------|------|
| **P0** | Registry Client 占位符代码重写 | 🔴 | Phase 1 补完 | ✅ 已完成 |
| **P0** | MySQL 结果序列化改用 protobuf | 🔴 | Phase 2 补完 | ✅ 已完成（JSON 过渡方案，proto 已更新待重新生成） |
| **P0** | Biz 服替换 Snowflake ID 生成 | 🔴 | Phase 2 补完 | ✅ 已完成 |
| **P0** | **Go 侧 Registry/DBProxy/RPC Client 全部改为多连接** | 🔴 | Phase 1 补完 | ✅ 已完成（Go UpstreamPool + 连接池模式） |
| **P0** | **Gateway 从 Registry 动态发现 Biz 节点** | 🔴 | Phase 1 补完 | ✅ 已完成（C++ RegistryClient + UpstreamPool + Watch 回调动态增删节点） |
| **P1** | Gateway 加密握手增加版本号+防重放 | 🔴 | Phase 3 补完 | ✅ 已完成（v1 协议：version + timestamp + nonce） |
| **P1** | 错误码增加 Go error 类型包装 | 🟡 | Phase 3 补完 | ✅ 已完成（`errors.Error` 结构体 + `Wrap`/`Is` 等） |
| **P1** | config /admin/reload 增加认证 | 🟡 | Phase 3 补完 | ✅ 已完成（Bearer Token + 审计日志） |
| **P2** | 网络层增加 deadline + 连接限制 | 🟡 | Phase 4/5 | ✅ 已完成（Go/C++ heartbeat timeout + MaxConn + writeCh 背压） |
| **P2** | 限流器分片锁 + 熔断器半开限制 | 🟡 | Phase 3 补完 | ✅ 已完成（Go/C++ 64 分片锁 + halfOpenMaxRequests） |
| **P3** | C++ 网络层异步化架构 | 🔴 | Phase 5 | ✅ 基础架构已完成（`async_event_loop.hpp` 抽象就位，阻塞代码保留为兼容层） |
| **P3** | 引入 middleware 链式架构 | 🟡 | Phase 4 | ✅ 已完成（Go/C++ Middleware 接口 + Gateway 重构为中间件模式） |

---

## 设计原则（更新版，所有后续 Phase 必须遵守）

1. **禁止占位符代码进入主分支**：任何 `TODO`、`FIXME`、`time.Sleep` 模拟调用、`return nil, fmt.Errorf("not implemented")` 必须在合入前完成功能实现或明确标注为 `// PLACEHOLDER: Phase X will implement`，且不能位于关键调用路径。

2. **禁止绕过已有基础设施**：如果 common/ 下已有组件（如 Snowflake、errors、config），业务侧必须优先使用。禁止在业务代码中 "临时写一段能跑的"。

3. **协议必须预留扩展字段**：所有自定义协议（握手、packet header、配置格式）必须包含 `version` 或 `reserved` 字段。当前版本填 0 即可，但字段必须存在。

4. **数据序列化必须使用标准格式**：禁止用 `fmt.Sprintf`、`string concatenation`、`strconv` 拼接来传递结构化数据。必须使用 protobuf、JSON 或 MessagePack。

5. **临时实现必须保持接口契约一致**：MemoryStore、mock client 等临时组件的接口行为必须与生产实现（EtcdStore、真实 client）完全一致。不能因为 "临时用" 就放宽错误处理、超时、并发语义。

6. **安全相关的简化必须文档化风险**：如果出于 MVP 需要简化安全方案（如静态密钥代替证书），必须在代码注释和文档中明确列出风险点和升级路径。

7. **错误处理必须融入语言原生机制**：Go 侧错误必须实现 `error` 接口，C++ 侧错误必须支持异常或 `std::expected`（C++23），不能只暴露 int32 错误码。

8. **所有服务设计都不能是单体的（新增）**：
   - **任何服务（包括自己）都可能运行多个实例**，客户端代码必须按多节点架构设计
   - **任何上游依赖都必须通过 Registry 动态发现**，禁止在启动参数中硬编码服务地址（开发/测试环境可例外，但代码路径必须相同）
   - **任何客户端都必须维护连接池**（至少 2 个连接），具备负载均衡、故障转移、自动重连能力
   - **单连接直连模型在任何场景下都不可接受**，即使 MVP 阶段只有一台机器，代码也必须支持多节点

---

## 修改记录

| 日期 | 版本 | 修改内容 | 作者 |
|------|------|----------|------|
| 2026-04-17 | v1.0 | 首次审查，覆盖 Phase 1~3，识别 15 项偏离，制定补救优先级矩阵，新增 7 条设计原则 | - |
| 2026-04-17 | v1.1 | 新增"单体设计审查"章节；识别 5 项单点连接偏离；Gateway 单点 Biz 已修复（UpstreamPool）；设计原则新增第 8 条；补救矩阵增加 2 个 P0 项 | - |
| 2026-04-18 | v1.2 | 所有 P0 优化项完成：C++ RegistryClient 重写（UpstreamPool + protobuf + Watch），Gateway 动态 Biz 发现，Go 侧全量多节点改造完成，单体设计偏离全部修复 | - |
| 2026-04-20 | v1.3 | **P2/P3 全部完成**：Go/C++ 网络层 heartbeat timeout + MaxConn + 背压；Go/C++ 限流器 64 分片锁；Go/C++ Middleware 链式架构落地；Gateway 重构为中间件模式；C++ `async_event_loop.hpp` 抽象就位；所有已知偏离项修复完毕 | - |
