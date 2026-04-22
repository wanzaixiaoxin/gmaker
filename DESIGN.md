# Server Framework Design

> 去中心化、Go/C++ 双语言、Protobuf3 协议的服务端框架设计文档。
> 版本：v0.2
> 状态：草案（持续迭代中）

---

## 1. 设计目标

| 目标 | 说明 |
|------|------|
| **去中心化** | 无单点 Master，各服务节点对等，可独立水平扩展 |
| **双语言扬长避短** | Go 负责业务/IO 密集型服务，C++ 负责性能/延迟敏感型服务 |
| **高性能** | C++ 网关与战斗服零 GC 抖动；Go 业务服高并发开发效率 |
| **易扩展** | 新增服务类型只需定义 proto + 实现 Handler + 注册到 Registry |
| **协议统一** | 对外/对内全部使用 protobuf3，自有 TCP 帧格式 |

---

## 2. 整体架构

```
                         [Client]
                            │
           ┌────────────────┴────────────────┐
           ▼                                 ▼
    [HTTP Login/Auth]                [WS / TCP Gateway]
       (Go)                              (C++)
           │                                 │
           └────────────────┬────────────────┘
                            ▼
              [Service Mesh - TCP + protobuf3]
                            │
    ┌──────┬─────────┬────────┼────────┬──────┬──────┐
    ▼      ▼         ▼        ▼        ▼      ▼      ▼
  Biz  Realtime  Log/     DB        Bot    ... (扩展)
  Server Server   Stats    Proxy     Server
  (Go)  (C++)    (Go)    (Go)      (Go/
                                          C++)
```

---

## 3. 服务职责与语言选型

### 3.1 登录验签服 (Login/Auth Server)
- **对外协议**：HTTP/REST 或 HTTP/2
- **语言**：Go
- **职责**：
  - 账号注册/登录
  - Token 签发（JWT/自定义 Session）
  - 第三方 OAuth 接入预留
  - 公告、版本配置、白名单下发
- **设计要点**：完全无状态，Token 自包含签名，任何服务可本地验签

### 3.2 网关服 (Gateway Server)
- **对外协议**：WebSocket（浏览器/H5/小程序）+ TCP（原生客户端）
- **语言**：C++
- **职责**：
  - 连接管理、心跳、断线重连
  - 包解析、加密/解密、压缩/解压
  - 玩家 Session 与 Gateway 绑定
  - 按路由规则转发到后端业务服
- **设计要点**：
  - **完全无状态**，不保存业务数据
  - 玩家路由映射由 Registry + 统一路由策略决定
  - 支持 Gateway 热扩容/缩容，客户端无感重连
  - C++ 实现避免 GC 停顿，适合万级长连接高并发
  - **多场景适配**：心跳间隔、广播策略、压缩阈值、AOI 过滤规则均可通过配置热切换

### 3.3 业务服 (Biz Server)
- **语言**：Go
- **职责**：
  - 大厅逻辑：背包、任务、好友、邮件、聊天、商城、公会、社交、匹配、地图、联盟等
  - 匹配排队、房间/场景管理（非实时内）
  - 运营活动、配置驱动玩法
- **设计要点**：
  - 多 Biz 节点对等，按 UID 哈希或一致性哈希分配
  - 支持动态扩缩容，数据通过 DB Proxy 持久化
  - 业务按领域拆分为独立模块，不同项目通过配置启用不同模块子集

### 3.4 实时服 (Realtime Server)
- **语言**：C++
- **职责**：承载所有**高频、有状态、低延迟**的实时计算场景
- **设计要点**：
  - 节点间对等，有状态（房间/场景数据），其他服务通过 RoomID/SceneID 路由
  - C++ 保证计算确定性、极低延迟
  - **运行时模式切换（通过配置或插件选择实时容器类型）**：
    - **`sync_room`**：封闭同步房间，支持帧同步与状态同步（适用于竞技类、FPS、卡牌、回合制等）
    - **`spatial_scene`**：大空间场景服，支持 AOI 管理与状态同步（适用于大世界、MMORPG、大逃杀等）
    - **`async_combat`**：异步战斗计算容器，接收战斗参数后离线计算并回调 Biz（适用于策略、SLG、放置类等）
  - 统一抽象 `IRealtimeContext` 接口，不同容器类型实现不同子类，通过 `biz_mode` 配置动态加载
  - 支持回放、观战、断线重连缓存

### 3.5 日志/统计服 (Log/Stats Server)
- **语言**：Go
- **职责**：
  - 接收全服结构化埋点
  - 日志聚合、实时统计、监控告警
- **设计要点**：
  - 作为消息队列消费者，异步接收各服务推送
  - 支持多消费节点横向扩展
  - 后期可平滑替换为 Kafka/Pulsar 而不改上游接口

### 3.6 数据库代理服 (DB Proxy)
- **语言**：Go
- **职责**：
  - 统一数据库访问入口（MySQL、Redis、MongoDB 等）
  - 连接池管理、SQL 缓存、限流、分库分表路由
- **设计要点**：
  - 多 DB Proxy 节点对等
  - 服务按 Key 哈希或一致性哈希选择节点
  - **数据持久化策略适配**：
    - **大字段角色存档**：protobuf bytes 直存 MySQL Blob，减少表结构变更，适用于复杂角色数据
    - **大地图/分块数据**：Tile Map 按区域分库分表，Redis Pipeline 批量写入
    - **轻量战斗数据**：战斗回放临时写入对象存储（MinIO/OSS），DB Proxy 代理生成预签名 URL
    - **社交/邮件批量写入**：异步队列合并，减少事务开销

### 3.7 机器人服 (Bot Server)
- **语言**：Go / C++
- **职责**：
  - 压测机器人（高并发模拟客户端）
  - AI 陪玩、自动化回归测试
  - 对接 `ai/` 目录中的行为树与推理服务，执行智能对战/探索
- **设计要点**：
  - 既能走 Gateway 做端到端压测，也能直接调内部 RPC 做单元压测
  - Go 适合快速编写复杂行为树与压测脚本；C++ 适合极限性能机器人
  - 支持从 `ai/behavior-trees/` 动态加载行为树配置

### 3.8 注册中心 (Registry)
- **语言**：Go
- **职责**：
  - 服务注册、心跳保活、自动摘除
  - 服务发现：按类型查询节点列表
  - 节点变更事件推送（Watch）
- **设计要点**：
  - 本身支持多实例，底层可用 Raft 或 Etcd 保证一致性
  - 提供 Go SDK 与 C++ SDK

### 3.9 Redis 基础设施

Redis 作为全服共享的高速存储层，承担**缓存、会话、排行榜、计数器、锁、消息通道**等多重角色。

#### 部署架构
- **模式**：**Redis Cluster**（6 节点起步：3 主 3 从），天然支持分片与故障转移
- **访问入口**：由 `dbproxy-go` 统一代理，业务服务不直接连 Redis
- **备份策略**：RDB 每小时快照 + AOF 每秒刷盘，冷备定期上传对象存储

#### Key 设计规范
统一采用 `{service}:{biz_type}:{id}` 的冒号分隔格式，便于监控、扫描与权限隔离：

| 用途 | Key 示例 | 数据类型 |
|------|----------|----------|
| 玩家会话 | `session:player:{player_id}` | String (JSON) / Hash |
| 玩家基础数据缓存 | `cache:player:{player_id}` | Hash |
| 背包物品 | `inventory:player:{player_id}` | Hash / String (protobuf) |
| 好友关系 | `social:friends:{player_id}` | Set / Sorted Set |
| 邮件 | `mail:player:{player_id}` | List / Sorted Set |
| 排行榜 | `rank:{rank_type}:{season_id}` | Sorted Set |
| 全局计数器 | `counter:{counter_name}` | String |
| 分布式锁 | `lock:{resource_name}` | String (SET NX EX) |
| 全服广播队列 | `pubsub:broadcast` | List / Stream |
| 房间快照 | `room:snapshot:{room_id}` | String (protobuf) |
| 防重放/限流 | `ratelimit:{service}:{player_id}` | String |

#### 各服务 Redis 使用约定

| 服务 | 典型 Redis 操作 | 说明 |
|------|-----------------|------|
| **Login** | `GET session:player:{id}`、`SET session:player:{id} EX 3600` | Token 关联 Session，支持多 Gateway 共享登录态 |
| **Gateway** | `EXISTS session:player:{id}` | 玩家重连时快速校验会话有效性 |
| **Biz** | `HGETALL cache:player:{id}`、`ZADD rank:daily:{season}` | 玩家数据缓存、排行榜、好友关系 |
| **Realtime** | `SET room:snapshot:{room_id} EX 300`、`GET room:snapshot:{room_id}` | 房间快照持久化（断线重连）、实时结果临时缓存 |
| **DBProxy** | 全量代理 | 连接池管理、Pipeline 合并、热点 Key 限流、大 Key 告警 |
| **LogStats** | `LPUSH queue:events`（可选） | 高并发埋点先写入 Redis Stream，再异步消费落盘 |

#### 缓存一致性策略

- **Cache-Aside（旁路缓存）**：DBProxy 的核心模式
  1. 读：先查 Redis，未命中则查 MySQL，回写 Redis
  2. 写：先写 MySQL，成功后删除/更新 Redis
- **过期时间**：玩家数据缓存 TTL 建议 `300s ~ 900s`，动态热点数据可延长至 `3600s`
- **缓存击穿防护**：对热点 Key 使用本地 `singleflight` 合并回源请求
- **缓存穿透**：对 DB 不存在的 Key 写入空值占位（TTL 60s）
- **缓存雪崩**：核心数据设置随机浮动 TTL（如 `300s + rand(60s)`）

#### DB Proxy 的 Redis 代理增强

`dbproxy-go` 不仅是透传代理，还承担以下职责：
- **连接池**：维护到 Redis Cluster 的长连接池，减少 TCP 握手开销
- **Pipeline 合并**：将多个小请求合并为 Pipeline 批量发送
- **限流与熔断**：对热点 Key 或危险命令（`KEYS`、`FLUSHALL`）进行拦截与限流
- **序列化优化**：支持将 protobuf bytes 直接写入 Redis String，减少 JSON 编解码开销
- **双写缓冲**：对同时写入 MySQL 和 Redis 的操作，提供事务化封装接口

---

## 4. 双语言通信规范

### 4.1 统一 TCP 通信协议：固定二进制头 + protobuf 包体

所有通信链路（Client ↔ Gateway、Gateway ↔ 内部服务、服务 ↔ 服务）采用同一套协议栈，由**固定长度二进制包头**与**protobuf3 序列化包体**两部分组成。

**设计原则**：
- 包头用纯二进制（避免 protobuf 自描述开销），便于 C++ 零拷贝解析
- 包体用 protobuf3（跨语言、强类型、易扩展）
- 包头长度固定 18 字节，保证快速拆包与粘包处理

#### 4.1.1 帧格式定义

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           Length                              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|             Magic             |            CmdId              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           CmdId (cont.)       |            SeqId              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           SeqId (cont.)       |             Flag              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           Flag (cont.)        |                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
|                          Payload ...                          |
+                                                               +
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Length  : 4 bytes  (Big Endian)  整包长度 = 18 + len(Payload)
Magic   : 2 bytes  (Big Endian)  固定 0x9D7F
CmdId   : 4 bytes  (Big Endian)  全局命令号
SeqId   : 4 bytes  (Big Endian)  请求序列号，Req/Res 配对；Push = 0
Flag    : 4 bytes  (Big Endian)  位标志
Payload : N bytes                protobuf3 序列化后的业务数据
```

#### 4.1.2 包头字段详解

| 字段 | 偏移 | 长度 | 说明 |
|------|------|------|------|
| `Length` | 0 | 4 bytes | 整包长度（含自身 18 字节），大端序。最大允许 `16MB`（`0x01000000`），超限直接断开连接。 |
| `Magic` | 4 | 2 bytes | 固定 `0x9D7F`。非法连接首包校验失败直接断开，用于快速过滤扫描流量。 |
| `CmdId` | 6 | 4 bytes | 全局命令号，定义域如下：<br>• `0x00000001 ~ 0x00000FFF`：系统保留（心跳、握手、错误包）<br>• `0x00001000 ~ 0x0000FFFF`：公共协议（登录、注册、Gateway 转发）<br>• `0x00010000 ~ 0x7FFFFFFF`：业务协议（Biz、Realtime 各子系统按模块分段）<br>统一维护在 `spec/cmd_ids.yaml`。 |
| `SeqId` | 10 | 4 bytes | 请求序列号。Client/服务发起请求时自增（从 1 开始），对端响应时原样返回；服务端主动 Push 填 `0`。用于 Req-Res 异步配对。 |
| `Flag` | 14 | 4 bytes | 位标志，详见下表。 |

#### 4.1.3 Flag 位定义

```c
// Flag 位掩码（32 bit，大端序存储）
#define FLAG_ENCRYPT   (1 << 0)   // Payload 已加密（AES-256-GCM）
#define FLAG_COMPRESS  (1 << 1)   // Payload 已压缩（Zstd / Snappy，默认 Zstd）
#define FLAG_BROADCAST (1 << 2)   // 广播包标志（Gateway 用于批量推送）
#define FLAG_TRACE     (1 << 3)   // 包头后紧跟 16 字节 TraceID（扩展头模式，预留）
#define FLAG_RPC_REQ   (1 << 4)   // RPC 请求包
#define FLAG_RPC_RES   (1 << 5)   // RPC 响应包
#define FLAG_RPC_FF    (1 << 6)   // Fire-Forget 单向包
#define FLAG_HEARTBEAT (1 << 7)   // 心跳包（Payload 可为空）
// bit 8 ~ 31 保留
```

#### 4.1.4 编解码流程

**发送端**：
1. 根据业务消息类型查找 `CmdId`
2. 调用 `protobuf.Marshal()` 生成 `Payload` bytes
3. 若启用加密/压缩，对 `Payload` 进行处理并设置对应 `Flag` 位
4. 填充包头：`Length = 18 + len(Payload)`，`Magic = 0x9D7F`，填入 `CmdId`、`SeqId`、`Flag`
5. `write(full_header[18] + payload)` 到 TCP 连接

**接收端**：
1. 读取 4 字节 `Length`，校验合法性（`>= 18` 且 `<= 16MB`）
2. 读取剩余 `Length - 4` 字节到缓冲区
3. 校验 `Magic == 0x9D7F`
4. 解析 `CmdId`、`SeqId`、`Flag`
5. 剩余字节即为 `Payload`，根据 `Flag` 先解密/解压
6. 按 `CmdId` 找到对应 protobuf Message 类型，执行 `protobuf.Unmarshal()`
7. 投递到业务 Handler 或 RPC 路由层

> **粘包处理**：由于 `Length` 字段在包首且固定偏移，接收端每收到数据先尝试读取 4 字节长度，若缓冲区不足则等待；长度足够后按 `Length` 切包。

#### 4.1.5 内部 RPC 与 Gateway-Client 的差异

| 场景 | 包头格式 | 差异点 |
|------|----------|--------|
| **Client ↔ Gateway** | 同上 | `Flag` 可能含 `FLAG_ENCRYPT`；`CmdId` 为业务命令号 |
| **Gateway ↔ Biz/Realtime** | 同上 | 网关转发时在 `Flag` 中追加 `FLAG_RPC_REQ`/`FLAG_RPC_RES`，`SeqId` 保持不变用于端到端追踪 |
| **Biz ↔ DBProxy** | 同上 | `Flag` 通常不含 `FLAG_ENCRYPT`（内网信任域），`CmdId` 为内部 RPC 命令号 |

### 4.2 通用 Packet Protobuf（调试与内部透传）

```protobuf
syntax = "proto3";
package common;

message Packet {
    uint32 magic   = 1;  // 0x9D7F
    uint32 cmd_id  = 2;
    uint32 seq_id  = 3;
    uint32 flag    = 4;
    bytes  payload = 5;
}
```

> 正常通信时，包头采用固定二进制格式（本节 4.1），`Packet` 消息仅在以下场景使用：调试工具、日志落盘、网关内部基于消息队列的异步透传、协议分析器（如 Wireshark 插件）。

### 4.3 RPC 模式定义

| 模式 | 说明 | 典型场景 |
|------|------|----------|
| `Req-Res` | 同步请求，带超时与重试 | Biz 调用 DB Proxy 查玩家数据 |
| `Fire-Forget` | 单向发送，不等待响应 | 推送广播、日志上报 |
| `Broadcast` | 向多个节点或玩家广播 | 全服公告、房间内广播 |

### 4.4 注册中心协议

```protobuf
syntax = "proto3";
package registry;

message NodeInfo {
    string service_type = 1;  // "gateway", "biz", "realtime"...
    string node_id      = 2;  // 全局唯一实例 ID
    string host         = 3;
    uint32 port         = 4;
    map<string, string> metadata = 5;
    uint64 load_score   = 6;  // 负载分，越小越优先
    uint64 register_at  = 7;
}

message NodeList {
    repeated NodeInfo nodes = 1;
}

message NodeEvent {
    enum Type {
        JOIN  = 0;
        LEAVE = 1;
        UPDATE = 2;
    }
    Type type = 1;
    NodeInfo node = 2;
}

message Result {
    bool   ok     = 1;
    uint32 code   = 2;
    string msg    = 3;
}

message NodeId { string node_id = 1; }
message ServiceType { string service_type = 1; }

service Registry {
    rpc Register(NodeInfo) returns (Result);
    rpc Heartbeat(NodeId) returns (Result);
    rpc Discover(ServiceType) returns (NodeList);
    rpc Watch(ServiceType) returns (stream NodeEvent);
}
```

### 4.5 Go 与 C++ 公共库约定

| 功能 | Go 实现 | C++ 实现 |
|------|---------|----------|
| 网络框架 | `net` / `gnet` / `netpoll` | `libuv` / `asio` / 自研 epoll |
| protobuf 生成 | `protoc-gen-go` | `protoc-gen-cpp` |
| Registry SDK | `common/go/registry` | `common/cpp/registry` |
| 统一日志 | `zap` (JSON) | `spdlog` (JSON) |
| 配置解析 | `viper` / 自定义 | `toml++` / `yaml-cpp` |
| 线程/协程模型 | Goroutine | `std::thread` + io_context 池 |

### 4.6 网络库架构设计（C++ 与 Go）

网络库是框架的性能基石。针对**实时服（计算密集、超低延迟）**与**业务服（IO 密集、高并发）**两个截然不同的场景，C++ 与 Go 分别采用不同的线程/协程模型，但都遵循统一的协议接口与消息派发语义。

#### 4.6.1 C++ 网络库：计算与 IO 分离

C++ 网络库的核心挑战是：**实时计算的线程必须零阻塞，但战斗中不可避免地存在 Redis、DB、日志等 IO 需求**。解决方案采用 **IO 线程 + 计算线程 + 异步任务队列** 的三层架构。

```
┌─────────────────────────────────────────────────────────────┐
│                        C++ Realtime Server                   │
│                                                              │
│  ┌──────────────┐      ┌──────────────┐      ┌───────────┐  │
│  │   IO Thread  │      │   IO Thread  │      │   ...     │  │
│  │  (libuv/asio)│      │  (libuv/asio)│      │           │  │
│  │              │      │              │      │           │  │
│  │ 收包/粘包/拆包 │      │ 网络发送/定时器  │      │  Registry  │  │
│  │ 握手/心跳/加密 │      │  连接管理      │      │   Watch   │  │
│  └──────┬───────┘      └──────┬───────┘      └─────┬─────┘  │
│         │                     │                    │        │
│         └─────────────────────┼────────────────────┘        │
│                               ▼                               │
│                    ┌─────────────────────┐                   │
│                    │   Message Queue     │                   │
│                    │  (无锁环形队列 /     │                   │
│                    │   moodycamel::queue) │                  │
│                    └──────────┬──────────┘                   │
│                               │                               │
│         ┌─────────────────────┼─────────────────────┐        │
│         ▼                     ▼                     ▼        │
│  ┌──────────────┐      ┌──────────────┐      ┌───────────┐  │
│  │ Compute Thd  │      │ Compute Thd  │      │ Compute   │  │
│  │   (Room A)   │      │   (Room B)   │      │  Thd ...  │  │
│  │              │      │              │      │           │  │
│  │  帧同步推进   │      │  帧同步推进   │      │  状态同步  │  │
│  │  战斗逻辑计算 │      │  战斗逻辑计算 │      │  AOI计算  │  │
│  │  **严禁阻塞** │      │  **严禁阻塞** │      │ **严禁阻塞│  │
│  └──────┬───────┘      └──────┬───────┘      └─────┬─────┘  │
│         │                     │                    │        │
│         └─────────────────────┼────────────────────┘        │
│                               ▼                               │
│                    ┌─────────────────────┐                   │
│                    │   Async Task Queue  │                   │
│                    │  (计算线程投递的      │                   │
│                    │   IO 任务队列)        │                  │
│                    └──────────┬──────────┘                   │
│                               │                               │
│                               ▼                               │
│                    ┌─────────────────────┐                   │
│                    │   Async IO Thread   │                   │
│                    │   Pool (Redis/DB)   │                   │
│                    │  执行阻塞 IO 并回调  │                   │
│                    │  结果通过队列返给计算 │                  │
│                    └─────────────────────┘                   │
└─────────────────────────────────────────────────────────────┘
```

##### 线程模型详解

| 线程类型 | 数量 | 职责 | 阻塞要求 |
|----------|------|------|----------|
| **IO Thread** | 1 ~ N（通常等于 CPU 核数的一半） | 监听端口、epoll 事件循环、收包拆包、加密解密、握手心跳 | 不阻塞 |
| **Compute Thread** | 1 ~ M（按 Room/Scene 绑定） | 战斗/场景逻辑推进、状态同步、物理/技能计算 | **绝对禁止阻塞 IO** |
| **Async IO Thread** | 2 ~ 8 | 执行 Redis、MySQL、文件 IO、HTTP 调用等阻塞操作 | 可以阻塞 |

##### 消息流转机制

1. **收包流程**：
   - IO Thread 收到客户端/网关的 TCP 包，拆包后解析出 `CmdId` + `Payload`
   - 根据路由规则（如 `player_id % M` 或 `room_id 哈希`）将消息投递到对应 Compute Thread 的无锁队列

2. **计算线程处理**：
   - Compute Thread 每帧（如 16ms/33ms）从队列中批量取出消息
   - 执行业务逻辑（帧同步收集输入、状态同步更新实体状态）
   - **需要 IO 时**（如查询玩家装备属性、写入战斗回放），构造 `AsyncTask` 投递到 Async IO Thread 池，并注册回调（Lambda / std::function）

3. **IO 结果回调**：
   - Async IO Thread 执行完 Redis `GET` / MySQL `SELECT` 后，将结果封装为 `AsyncResult`，投递回源 Compute Thread 的队列
   - Compute Thread 在下一帧处理该回调，继续推进逻辑

##### 关键设计：Room/Scene 与计算线程绑定

- **sync_room**：一个 Room 固定绑定到一个 Compute Thread，房间内所有玩家的输入包都路由到该线程。这样房间内部无需加锁，状态更新单线程推进。
- **spatial_scene**：一个 Scene（或 AOI 区块）固定绑定到一个 Compute Thread，玩家跨场景切换时，需先将该玩家状态快照迁移到目标线程的队列，再切换绑定关系。

##### 网络库 API 设计

```cpp
class INetEngine {
public:
    // IO 线程调用：启动监听
    virtual bool Listen(const std::string& host, uint16_t port) = 0;

    // 计算线程调用：发送消息到指定连接（IO 线程异步执行）
    virtual void Send(uint64_t conn_id, const bytes& payload) = 0;

    // 计算线程调用：投递异步 IO 任务
    virtual void PostAsync(std::function<void()> task,
                           std::function<void()> callback) = 0;

    // 计算线程调用：广播到多个连接
    virtual void Broadcast(const std::vector<uint64_t>& conn_ids,
                           const bytes& payload) = 0;

    // 注册消息处理器（运行在 Compute Thread）
    virtual void RegisterHandler(uint32_t cmd_id,
                                 std::function<void(uint64_t, const bytes&)> handler) = 0;
};
```

#### 4.6.2 Go 网络库：协程池 + 多 IO 协作

Go 业务服的核心挑战是：**单个玩家请求可能涉及 Redis、MySQL、Registry、下游 RPC 等多个 IO 协作**，如果全部顺序调用会导致延迟叠加。Go 利用原生 goroutine 的轻量并发特性，采用 **goroutine-per-connection + 协程池 + context 超时控制** 的模型。

##### 线程/协程模型

```
[Client] ──TCP/WS──▶ [Gateway(C++)] ──RPC──▶ [Biz Server(Go)]
                                                  │
                    ┌─────────────────────────────┼─────────────────────────────┐
                    ▼                             ▼                             ▼
              ┌───────────┐               ┌───────────┐                 ┌───────────┐
              │ Goroutine │               │ Goroutine │                 │ Goroutine │
              │  Player A │               │  Player B │                 │  Player C │
              │  Request  │               │  Request  │                 │  Request  │
              └─────┬─────┘               └─────┬─────┘                 └─────┬─────┘
                    │                             │                             │
                    ▼                             ▼                             ▼
              ┌───────────┐               ┌───────────┐                 ┌───────────┐
              │  Handler  │               │  Handler  │                 │  Handler  │
              │  (业务入口) │               │  (业务入口) │                 │  (业务入口) │
              └─────┬─────┘               └─────┬─────┘                 └─────┬─────┘
                    │                             │                             │
                    ▼                             ▼                             ▼
              [并发 IO 协作示例]

              ┌─────────┐ ┌─────────┐       ┌─────────┐ ┌─────────┐
              │ Redis   │ │ DBProxy │       │ Redis   │ │ DBProxy │
              │  GET    │ │  RPC    │       │  GET    │ │  RPC    │
              │ (并发)  │ │ (并发)  │       │ (顺序)  │ │ (并发)  │
              └────┬────┘ └────┬────┘       └────┬────┘ └────┬────┘
                   │           │                 │           │
                   └─────┬─────┘                 └─────┬─────┘
                         ▼                             ▼
                   [聚合结果]                      [聚合结果]
```

##### 单请求多 IO 协作的两种模式

**模式 A：并行聚合（Fan-Out / Fan-In）**

当一次业务请求需要同时查询多个独立数据源时，使用 `sync.WaitGroup` 或 `errgroup` 并发调用：

```go
func GetPlayerFullData(ctx context.Context, playerID uint64) (*PlayerData, error) {
    var wg sync.WaitGroup
    var base *PlayerBase
    var bag *PlayerBag
    var err1, err2 error

    wg.Add(2)
    go func() {
        defer wg.Done()
        base, err1 = redisClient.HGetAll(ctx, fmt.Sprintf("cache:player:%d", playerID))
    }()
    go func() {
        defer wg.Done()
        bag, err2 = dbproxyClient.QueryBag(ctx, playerID)
    }()
    wg.Wait()

    if err1 != nil || err2 != nil {
        return nil, fmt.Errorf("query failed: %v / %v", err1, err2)
    }
    return &PlayerData{Base: base, Bag: bag}, nil
}
```

**模式 B：管道顺序（Pipeline）**

当 IO 之间存在依赖关系时（如先查 Redis 配置表，再决定查哪个 MySQL 分表），顺序执行，但通过 `context.WithTimeout` 严格控制总超时：

```go
func HandleBuyItem(ctx context.Context, req *BuyItemReq) (*BuyItemRes, error) {
    ctx, cancel := context.WithTimeout(ctx, 500*time.Millisecond)
    defer cancel()

    // 1. 查配置
    cfg, err := redisClient.GetItemConfig(ctx, req.ItemID)
    if err != nil { return nil, err }

    // 2. 查背包
    bag, err := dbproxyClient.QueryBag(ctx, req.PlayerID)
    if err != nil { return nil, err }

    // 3. 扣减 + 写入（双写 MySQL + Redis，由 DBProxy 保证）
    res, err := dbproxyClient.BuyItem(ctx, req.PlayerID, req.ItemID, cfg.Price)
    return res, err
}
```

##### 协程池（Goroutine Pool）

虽然 goroutine 创建成本低，但无限制创建会导致：
- 内存爆炸（每个 goroutine 初始 2KB 栈，百万级就是 2GB）
- 调度抖动
- DB/Redis 连接数被打爆

因此 Biz 服对**高并发入口**和**下游 IO 调用**引入协程池：

| 池类型 | 用途 | 推荐实现 |
|--------|------|----------|
| **连接池** | Redis / MySQL / RPC 连接复用 | `go-redis` 自带连接池；`sql.DB` 自带连接池 |
| **任务池** | 限制同时处理中的业务请求数 | `ants` / `tunny` / 自研 channel 池 |
| **批量写入池** | 合并多个小写入为批量操作 | 自研 `BatchWriter`，定时或定量 flush |

##### 网络库封装约定

Go 侧的 `common/go/net` 对标准库 `net` 做一层薄封装：

```go
// 统一的 TCP Server 封装
type TCPServer struct {
    Addr         string
    Handler      func(conn net.Conn)
    IdleTimeout  time.Duration
    MaxConn      int
}

// 统一的 RPC Client 封装（内部服务间调用）
type RPCClient struct {
    Endpoints   []string          // 从 Registry 发现
    Timeout     time.Duration
    RetryPolicy RetryPolicy
}
```

- 对外（Gateway→Biz）使用**长连接 + 连接池**，避免频繁 TCP 握手
- 每个连接对应一个读 goroutine 和一个写 goroutine（双 goroutine 模型），通过 channel 收发消息
- 支持按 `player_id` 做一致性哈希路由到下游 Biz 节点

#### 4.6.3 C++ 与 Go 网络库的交互约定

| 维度 | C++ 网络库 | Go 网络库 |
|------|-----------|-----------|
| **核心抽象** | `INetEngine`：IO / Compute / Async IO 三层 | `TCPServer` / `RPCClient`：goroutine + context |
| **并发单位** | 线程（IO Thread / Compute Thread） | goroutine（轻量协程） |
| **消息派发** | 无锁队列绑定到 Compute Thread | channel 或函数回调 |
| **跨线程 IO** | `PostAsync(task, callback)` | goroutine + `sync.WaitGroup` / `errgroup` |
| **阻塞容忍** | Compute Thread 严禁阻塞；Async IO Thread 可以阻塞 | goroutine 可以阻塞，但需控制超时与并发数 |
| **典型场景** | 帧同步房间、AOI 场景计算 | 大厅业务、多数据源聚合、批量写入 |

#### 4.6.4 网关（Gateway-C++）的特殊网络需求

Gateway 作为 C++ 实现的万级长连接接入层，其网络模型与实时服略有不同：

- **IO 线程**：专职处理 epoll 事件、TLS/加密、拆包粘包、心跳检测
- **路由线程池**：负责将客户端包按 `CmdId` 路由到内部服务（Biz / Realtime），或反向将服务端响应路由回客户端
- **广播优化**：对 `sync_room` 的全广播，使用**写聚合（Write Coalescing）**——同一帧内多个响应包合并为一个 TCP 帧发送，减少系统调用次数
- **AOI 过滤**：对 `spatial_scene`，Gateway 本地维护玩家 `conn_id` 与坐标映射，只将 AOI 广播推送到视野范围内的连接

### 4.7 公共基础设施

除 Redis、Etcd 外，框架还需要以下公共基础设施支撑全服运行：

#### 4.7.1 全局唯一 ID 生成器
- **用途**：玩家 ID、订单号、日志 TraceID、房间号
- **方案**：内嵌 **Snowflake**（41 位时间戳 + 10 位机器码 + 12 位序列号）
- **部署**：每个服务节点启动时从 Registry 或配置中心获取唯一 `worker_id`
- **双语言**：Go 用 `bwmarrin/snowflake` 或自研；C++ 自研实现（无外部依赖）

#### 4.7.2 分布式锁
- **底层**：基于 Redis `SET key value NX EX` + Lua 解锁脚本
- **封装**：公共库提供 `Lock(key, ttl)` / `Unlock(key)` 接口，支持自动续租看门狗
- **使用场景**：全局排行榜结算、限量道具发放、跨服匹配撮合

#### 4.7.3 限流与熔断
- **限流**：
  - 单节点级别：令牌桶 / 漏桶（公共库内实现）
  - 全局级别：Redis 滑动窗口计数（如 `ratelimit:{api}:{player_id}`）
- **熔断**：基于错误率的简易熔断器（Close → Open → Half-Open），保护 DBProxy / 下游服务
- **封装**：Go 用 `golang.org/x/time/rate` + 自研熔断器；C++ 自研

#### 4.7.4 监控与指标（Metrics）
- **方案**：**Prometheus Pull + Grafana 展示**
- **指标类型**：
  - Counter：请求总量、错误总量、在线人数
  - Gauge：当前连接数、内存占用、CPU 负载
  - Histogram：RPC 延迟分布、Gateway 包处理耗时
  - Summary：P99 延迟
- **暴露方式**：每个服务内置 HTTP `/metrics` 端口（与业务端口隔离），Prometheus 定时抓取
- **双语言**：Go 用 `prometheus/client_golang`；C++ 用 `prometheus-cpp`

#### 4.7.5 链路追踪（Tracing）
- **方案**：自研轻量级 Trace，或兼容 **OpenTelemetry**
- **Trace 传播**：
  - 请求发起时生成 `trace_id`（16 字节十六进制）
  - 通过 RPC 的 `metadata` 或自定义包头字段透传到下游
  - 各服务记录 `span`（服务名、操作名、起止时间、状态码）
- **存储**：Trace 数据异步写入 `logstats-go`，再转存到 ClickHouse / Jaeger（后期扩展）
- **日志关联**：所有结构化日志强制携带 `trace_id` 字段，便于错误排查

#### 4.7.6 配置中心（轻量版）
- **MVP 阶段**：本地配置文件（TOML/YAML）+ 文件热加载
- **生产阶段**：配置文件统一托管到 **Etcd** 或 **Git + 文件分发**，各服务 `Watch` 配置变更并热重载
- **关键配置项**：
  - 服务监听地址与线程数
  - Redis / MySQL / Etcd 连接地址
  - 限流阈值、日志级别、功能开关
  - 业务数值表（后期可迁移到独立配置服）

#### 4.7.7 业务时间与时钟
- **需求**：支持暂停、加速、回拨检测（用于测试、回放、-debug 模式）
- **封装**：公共库提供 `GameTime.Now()`，默认代理系统时间，但可被配置覆盖
- **定时器**：
  - Go：`time.Timer` / `time.Ticker`
  - C++：基于 `asio::steady_timer` 或最小堆定时器

#### 4.7.8 安全基础库
- **对称加密**：AES-256-GCM（网关与客户端通信加密）
- **非对称加密**：RSA-2048（登录时密钥交换）
- **签名验证**：HMAC-SHA256（防篡改）
- **哈希**：Argon2id（密码存储）、MD5/SHA1（校验和，不推荐用于安全）
- **防重放**：请求包内带 `timestamp + nonce`，服务端 Redis 缓存 `nonce` 窗口（如 60s）

#### 4.7.9 配置版本管理与推送

为支持**不停服热更新**、**灰度发布**与**问题快速回滚**，配置系统必须具备版本管理、差异推送与全服广播能力。

##### 配置分层模型

| 层级 | 用途 | 示例 | 更新频率 |
|------|------|------|----------|
| **L0 系统配置** | 服务启动参数、网络地址、线程数 | `listen_addr`、`etcd_endpoints` | 极低，需重启生效 |
| **L1 动态配置** | 运行时可热重载的参数 | `log_level`、`rate_limit_qps`、`feature_flags` | 低，分钟级 |
| **L2 业务数值表** | 道具、关卡、掉落、技能数值 | `item_table`、`level_exp` | 中，运营活动时更新 |
| **L3 紧急补丁** | 热修复开关、黑名单、公告 | `ban_list`、`emergency_notice` | 高，秒级 |

##### 版本管理规范

- **版本号格式**：`{major}.{minor}.{patch}-{timestamp}`，如 `v1.3.0-20260417120000`
- **文件命名**：`{config_name}_{version}.toml` / `.yaml` / `.json`
- **存储位置**：
  - MVP 阶段：`configs/` 目录按版本子目录存放
  - 生产阶段：Etcd Key 前缀 `/configs/{service_type}/{config_name}/{version}`
- **兼容性要求**：
  - L2/L3 配置必须保证**向后兼容**（只增字段、不改字段语义、不删已用字段）
  - 破坏性变更必须走 `major` 版本升级，并配合代码发布

##### 配置推送流程

**MVP 阶段（文件系统 + 信号/HTTP 触发）**：
1. 运维/脚本将新配置写入 `configs/active/{service_type}/`
2. 调用目标服务的 HTTP `POST /admin/reload` 接口，携带配置名称与版本号
3. 服务加载新配置，校验 schema 通过后热重载；失败则保持旧配置并告警

**生产阶段（Etcd + Watch 主动推送）**：
1. 配置管理平台将新配置写入 Etcd：`/configs/{svc}/{name}/{version}`
2. 各服务的 `config` 公共库通过 Etcd `Watch` 监听前缀变化
3. 收到变更事件后，拉取新配置内容，本地校验 schema 与版本连续性
4. 校验通过则原子替换内存配置对象；校验失败则记录错误日志并忽略
5. 关键配置（如 L3 紧急补丁）通过 `logstats-go` 广播变更事件，便于审计追踪

##### 全服配置一致性校验

- **配置摘要（Digest）**：每个服务在加载配置后计算 SHA-256 摘要，周期性上报到 `logstats-go`
- **一致性仪表盘**：通过 Digest 比对，快速发现哪台机器配置版本不一致
- **强制同步**：对配置漂移的节点，可通过管理后台下发 `FORCE_RELOAD` 指令

##### 灰度与回滚

- **灰度**：配置项支持 `metadata.scope = "global" | "region:cn" | "node:biz-01"`，Registry 按标签过滤目标节点后推送
- **回滚**：将 Etcd 中的活跃版本指针指回上一个版本，所有 Watch 节点自动重新加载旧配置

### 4.8 公共库模块详细划分

`common/` 目录下的公共库按功能域拆分，Go 与 C++ 保持模块名一致：

```
common/
├── go/pkg/
│   ├── net/          # TCP Server/Client、WS 适配、连接管理、拆包粘包
│   ├── rpc/          # RPC 引擎：Req-Res、Fire-Forget、Broadcast、超时重试
│   ├── registry/     # Registry SDK：Register/Heartbeat/Discover/Watch
│   ├── pb/           # 公共 protobuf 生成的 go 代码
│   ├── logger/       # 基于 zap 的 JSON 结构化日志，统一字段规范
│   ├── config/       # 配置加载与热重载
│   ├── idgen/        # Snowflake ID 生成器
│   ├── lock/         # Redis 分布式锁封装
│   ├── limiter/      # 令牌桶限流 + 熔断器
│   ├── metrics/      # Prometheus 指标封装
│   ├── trace/        # TraceID 生成与传播
│   ├── crypto/       # AES/GCM、RSA、HMAC、Hash 封装
│   ├── timer/        # 业务时间、定时器工具
│   └── errors/       # 全局错误码定义与错误包装
│
└── cpp/gs/
    ├── net/          # 网络框架（libuv/asio + 拆包粘包）
    ├── rpc/          # RPC 引擎
    ├── registry/     # Registry SDK
    ├── pb/           # 公共 protobuf 生成的 cpp 代码
    ├── logger/       # 基于 spdlog 的 JSON 日志
    ├── config/       # 配置加载
    ├── idgen/        # Snowflake
    ├── lock/         # Redis 分布式锁（Redis++ 或 hiredis 封装）
    ├── limiter/      # 限流与熔断
    ├── metrics/      # prometheus-cpp 封装
    ├── trace/        # Trace 传播
    ├── crypto/       # OpenSSL 封装
    ├── timer/        # 定时器与业务时间
    └── errors/       # 错误码与异常处理封装
```

### 4.9 开发工具链

| 工具 | 用途 | 推荐方案 |
|------|------|----------|
| **Proto 生成** | 统一生成 Go/C++ 代码 | `Makefile` 调用 `protoc` + `protoc-gen-go` |
| **代码格式化** | 保持代码风格一致 | Go: `gofmt` / `golangci-lint`；C++: `clang-format` |
| **单元测试** | 公共库与服务单测 | Go: `go test`；C++: `GoogleTest` |
| **Benchmark** | 性能基线测试 | Go: `go test -bench`；C++: `GoogleBenchmark` |
| **Mock 工具** | 接口隔离测试 | Go: `gomock` / `mockery`；C++: `gmock` |
| **CI/CD** | 自动构建、测试、镜像打包 | GitHub Actions / GitLab CI |
| **包管理** | 依赖拉取与版本锁定 | Go: `go mod`；C++: `vcpkg` 或 `Conan` |

---

## 5. 服务发现与路由

### 5.1 方案选型：自研 Registry（Go）+ Etcd

为兼顾**快速落地**、**C++ 生态兼容**与**高可用**，服务发现采用以下两层架构：

```
[Go/C++ Services] ──TCP+protobuf3──▶ [Registry: Go] ──gRPC──▶ [Etcd Cluster]
```

- **Registry**：无状态 TCP 服务，仅对框架内部暴露。负责协议转换、Lease 续租代理、Watch 事件推送。
- **Etcd**：3/5 节点集群，作为高一致性的元数据存储。利用 Etcd `Lease` 机制做自动超时摘除，`Watch` 机制做实时节点变更通知。

**选型原因**：
1. **C++ 侧零额外依赖**：C++ 服务无需引入 Etcd gRPC 客户端，复用框架统一的 TCP + protobuf3 连接 Registry。
2. **Lease 天然匹配心跳**：Etcd 的 TTL 续租比自研扫描定时器更可靠，Registry 宕机重启后数据不会丢失。
3. **演进平滑**：早期可用单 Registry + 单 Etcd 跑通 MVP；生产期 Registry 多实例负载均衡，Etcd 3 节点集群即可满足多数业务服规模。

### 5.2 Etcd 存储与 Lease 设计

**Key 前缀约定**：
```
/registry/nodes/{service_type}/{node_id}
# 示例：
/registry/nodes/biz/biz-01
/registry/nodes/gateway/gw-01
/registry/nodes/realtime/realtime-03
```

**Lease 参数**：
- `TTL = 15s`（服务注册时由 Registry 统一创建 Lease）
- `Heartbeat 间隔 = 5s`（各服务通过框架 SDK 自动发送）
- Registry 收到 `Heartbeat` 后，向 Etcd 发起 `KeepAlive`，无需业务感知 LeaseID。

**数据格式**：Key 对应的 Value 为 `NodeInfo` 的 protobuf 序列化 bytes，便于 Etcd 直接透读，也便于后续工具通过 `etcdctl` 做诊断。

### 5.3 注册与心跳流程

1. **Register**
   - 服务启动 → 生成全局唯一 `node_id`（建议 `{hostname}-{pid}-{timestamp}`）→ 监听端口
   - 调用 `Registry.Register(NodeInfo)`
   - Registry 校验后，向 Etcd 创建 `Lease(TTL=15s)`，并 `Put` 节点信息
   - 返回 `Result{ok=true}` 给服务

2. **Heartbeat**
   - 服务启动后台协程/线程，每 5s 发送 `Heartbeat(NodeId)`
   - Registry 收到后调用 Etcd `KeepAlive` 续租
   - 若服务崩溃未发送心跳，Etcd 在 15s 后自动删除 Key，并触发所有 Watch 客户端的 `LEAVE` 事件

3. **Discover & Watch**
   - 消费方启动时先调用 `Discover(ServiceType)`，获取当前全量节点列表，建立本地路由缓存
   - 随后建立长连接调用 `Watch(ServiceType)`，Registry 将该请求转译为 Etcd `Watch(prefix)`
   - Etcd 发生任何节点增删改时，Registry 实时将事件转为 `NodeEvent` 推送给消费方
   - SDK 收到 `NodeEvent` 后增量更新本地缓存，无需再次全量 Discover

### 5.4 路由策略

- **无状态服务**（Login、Gateway、Biz、Log、DBProxy）：轮询 / 最小连接数 / 负载分优先
- **有状态服务**（Realtime）：按 RoomID 一致性哈希路由到固定 Realtime 节点
- **广播**：遍历目标类型的全部存活节点，逐个发送

### 5.5 故障转移

- **节点级故障**：调用某节点失败时，本地临时标记为“不可用”，下次路由跳过。同时异步重试探测，若恢复则重新启用。
- **Registry 故障**：Registry 本身无状态，可部署多实例。若某 Registry 实例断开，SDK 自动重连到另一实例，并重新执行 `Discover + Watch`。
- **极端兜底**：若 Registry 集群全部不可用，本地缓存继续生效（数据可能旧但不会中断），同时 SDK 定时发起重连。

---

## 6. 扩展性设计

| 扩展点 | 设计 |
|--------|------|
| **新增服务类型** | 定义 proto → 实现 Handler → 注册到 Registry → 自动加入服务网格（Go 或 C++ 任选） |
| **新增网关协议** | 增加 KCP/QUIC 监听，底层只改网络接入层，转发逻辑复用 |
| **新增实时模式** | Realtime Server（C++）内按业务类型实现 Room/Scene 插件，动态加载（SO/DLL） |
| **分区分服/跨服** | Registry 增加 `region` `zone` 标签，路由层按标签过滤 |
| **热更新** | Go 服可用 plugin 机制；C++ 服可用动态库加载；Gateway/Realtime 推荐滚动更新 |

### 6.1 多业务类型适配设计

框架从底层设计上需同时支撑 **高频竞技**、**策略大地图**、**大世界社交** 以及后续的卡牌、回合制、FPS 等场景。核心思路是：**协议统一、服务复用、实时层插件化、数据层按需扩展**。

#### 6.1.1 三种典型业务场景的架构映射

| 维度 | MOBA | SLG | MMORPG |
|------|------|-----|--------|
| **核心体验** | 小房间、高频操作、低延迟 | 大地图、异步交互、重策略 | 大世界、强社交、状态同步 |
| **Realtime Server 形态** | `Sync Room`（帧同步/状态同步） | `Async Combat` / `World Tile Server` | `Scene Server`（AOI + 状态同步） |
| **单场景人数** | 5v5 / 10v10 | 全服同图，逻辑分片 | 单场景 50~500 人 |
| **广播特征** | 房间内全广播 | 联盟/个人邮件推送为主 | AOI 九宫格/十字链表广播 |
| **数据热点** | 战斗回放、匹配队列 | 地图 Tile 数据、联盟信息 | 玩家存档、背包、公会数据 |
| **数据库侧重** | Redis 缓存为主，MySQL 轻量存档 | 大地图分库分表、时间序列数据 | 大字段 Blob 存档、复杂事务 |

#### 6.1.2 Realtime Server 插件化抽象

C++ 实时服内部定义统一接口，不同实时容器实现对应插件：

```cpp
// common/cpp/gs/realtime/ 接口定义
class IRealtimeContext {
public:
    virtual void OnPlayerJoin(uint64_t player_id) = 0;
    virtual void OnPlayerLeave(uint64_t player_id) = 0;
    virtual void OnClientInput(uint64_t player_id, const bytes& payload) = 0;
    virtual void OnTick(uint32_t delta_ms) = 0;
    virtual void Broadcast(const bytes& payload, const Filter& filter) = 0;
};

// 实时容器插件目录
// services/realtime-cpp/plugins/
//   ├── sync_room/      # 封闭同步房间（帧同步/状态同步）
//   ├── spatial_scene/  # 大空间场景服（AOI + 状态同步）
//   └── async_combat/   # 异步战斗计算容器
```

- **启动配置**：`plugin = "sync_room"`，服务启动时加载对应动态库
- **房间/场景分配**：Registry 的 `metadata` 中标记 `biz_mode = "sync_room"`，Gateway/Biz 按此标签路由

#### 6.1.3 Gateway 的策略适配

Gateway（C++）作为连接层，需根据项目类型调整连接策略：

| 策略项 | 高频竞技（sync_room） | 策略/放置（async_combat） | 大世界（spatial_scene） |
|--------|----------------------|--------------------------|------------------------|
| **心跳间隔** | 1~3s（极敏感） | 15~30s（低活跃） | 5~10s（常规） |
| **包压缩阈值** | 64B（高频小包） | 1KB（低频大包） | 256B |
| **广播模式** | RoomID 全广播 | 单点/Alliance 推送 | AOI 过滤广播 |
| **连接加密** | 必选（竞技公平） | 可选 | 必选（保护隐私） |
| **断线重连缓冲** | 最近 300 帧/10s | 最近 10 条事件 | 最近 5s 状态快照 |

Gateway 通过读取 `biz_mode` 配置动态选择上述参数，所有策略封装为 `IGatewayPolicy` 接口。

#### 6.1.4 Biz Server 的模块化加载

Go 业务服按项目需求启用不同模块子集，避免无关代码加载：

```go
// biz-go 的模块注册表
type Module interface {
    Name() string
    Init() error
    Handle(cmdId uint32, ctx *Context) error
}

// 示例：不同项目启用不同模块组合
var ProjectModules = map[string][]Module{
    "project_a": {MatchModule, RoomModule, ReplayModule, RankModule},
    "project_b": {AllianceModule, MapModule, ResourceModule, MailModule},
    "project_c": {GuildModule, TradeModule, DungeonModule, SocialModule},
}
```

#### 6.1.5 数据库与缓存的差异化策略

DB Proxy 内部按 `storage_mode` 选择不同的持久化策略：

- **大字段角色存档模式**：
  - 玩家数据采用 `protobuf bytes -> MySQL Blob` 大字段直存，减少表结构变更
  - 背包、邮件等复杂关系型数据使用 JSON 字段或辅助表
  - Redis 缓存玩家全量数据，离线时异步落库

- **大地图/分块数据模式**：
  - 地图 Tile 数据按 `x,y` 坐标分片，使用 Redis Hash + MySQL 分表双写
  - 联盟、战役记录使用宽表 + 时间序列数据库（如 ClickHouse，后期扩展）
  - 支持批量 Pipeline 写入，减少 IO 次数

- **轻量数据+回放模式**：
  - 玩家基础数据轻量，MySQL 表结构简单
  - 战斗回放临时存储于对象存储（MinIO/OSS），DB Proxy 代理生成预签名 URL
  - 排行榜、匹配分使用 Redis Sorted Set

#### 6.1.6 消息广播的差异化实现

框架内部提供三种广播原语，Biz/Realtime 服务按需调用：

```protobuf
service BroadcastRouter {
    // 1. 全服广播
    rpc BroadcastAll(BroadcastMsg) returns (Result);

    // 2. 房间/场景内广播（sync_room、回合制等）
    rpc BroadcastRoom(BroadcastMsg) returns (Result);

    // 3. AOI 范围广播（spatial_scene）
    rpc BroadcastAOI(AOIMsg) returns (Result);
}
```

- **BroadcastRoom**：Gateway 维护 `player_id -> room_id` 映射，按 RoomID 过滤推送
- **BroadcastAOI**：Gateway 或 Scene Server 维护玩家坐标四叉树/九宫格，只推送给视野内玩家
- **联盟/范围广播**：使用 `BroadcastAll` 的变体（按标签过滤），通过 Biz 服逻辑层完成

#### 6.1.7 从 MVP 到多项目运营的演进路线

| 阶段 | 目标场景 | 验证重点 |
|------|----------|----------|
| **Phase 1** | 通用大厅 + 极简实时房间 | 框架跨语言通信、服务发现、配置推送 |
| **Phase 2a** | sync_room（帧同步/状态同步） | 低延迟同步房间、回放、匹配 |
| **Phase 2b** | spatial_scene（单场景状态同步） | AOI 广播、大字段存档、场景无缝切换 |
| **Phase 2c** | async_combat（异步战斗） | 大地图分片、批量写入、异步计算回调 |
| **Phase 3** | 混合运营（多项目同框架） | 同一 Registry 下同时运行 sync_room + spatial_scene + async_combat 节点 |

---

## 7. 工程目录结构

```
luffa/
├── DESIGN.md                  # 本设计文档
├── spec/                      # 规范与协议定义
│   ├── proto/                 # 所有 protobuf 文件
│   ├── cmd_ids.yaml           # 全局命令号表
│   ├── rpc_spec.yaml          # RPC 行为契约
│   ├── errors.toml            # 全局错误码
│   └── README.md              # 规范修改说明
├── common/                    # 双语言公共库
│   ├── go/    pkg/            # Go 公共模块
│   │   ├── net/               # 网络封装
│   │   ├── rpc/               # RPC 引擎
│   │   ├── registry/          # 注册中心 SDK
│   │   └── logger/            # 结构化日志
│   └── cpp/   gs/             # C++ 公共库
│       ├── net/               # TCP/WS 网络框架
│       ├── rpc/               # RPC 引擎
│       ├── registry/          # 注册中心 SDK
│       └── logger/            # 结构化日志
├── services/                  # 各服务实现
│   ├── gateway-cpp/           # C++ 网关
│   ├── login-go/              # Go 登录服
│   ├── biz-go/                # Go 业务服
│   ├── realtime-cpp/          # C++ 实时服（同步房间/空间场景/异步战斗）
│   │   ├── plugins/           # 实时容器插件目录
│   │   │   ├── sync_room/     # 封闭同步房间
│   │   │   ├── spatial_scene/ # 大空间场景服
│   │   │   └── async_combat/  # 异步战斗计算容器
│   │   └── main.cpp           # 插件加载器入口
│   ├── dbproxy-go/            # Go DB 代理
│   ├── logstats-go/           # Go 日志统计
│   ├── bot-go/                # Go 机器人（默认）
│   ├── bot-cpp/               # C++ 极限性能机器人（可选）
│   └── registry-go/           # Go 注册中心
├── ai/                        # AI 编程辅助相关（预留目录）
│   ├── prompts/               # 常用 AI Prompt 模板（代码审查、重构、单测生成）
│   ├── context/               # 项目上下文说明（供 AI 读取的精简架构文档）
│   ├── rules/                 # AI 代码生成规则（命名规范、禁止项、偏好风格）
│   └── README.md              # 本目录使用说明
├── tools/                     # 构建与辅助工具
│   ├── proto-gen/             # 双语言代码生成脚本
│   ├── deploy/                # Docker / k8s / Helm
│   └── benchmark/             # 压测工具
├── Makefile                   # 统一构建入口
└── CMakeLists.txt             # C++ 顶层编译入口
```

---

## 8. 开发阶段与里程碑

### Phase 1：基础骨架（MVP）
目标：跑通最小 Go/C++ 跨语言链路
- [ ] `spec/proto/` 定义：Packet、Registry、Login、Biz 基础消息
- [ ] `registry-go`：注册中心实现
- [ ] `gateway-cpp`：最小 TCP/WS 网关（解析帧 + 转发）
- [ ] `biz-go`：最小业务服（响应登录后请求）
- [ ] `login-go`：HTTP 登录接口，签发 Token
- [ ] 统一 `Makefile`：双语言 proto 生成与构建

验证链路：
```
Client --TCP/WS--> Gateway(C++) --RPC--> Biz(Go)
                        │
                        └─ Registry(Go) 服务发现
```

### Phase 2：核心玩法
- [ ] `realtime-cpp`：实时服骨架 + 首个插件（如同步房间 `sync_room`）
- [ ] `dbproxy-go`：数据库代理（Redis + MySQL 连接池）
- [ ] `biz-go`：完整大厅逻辑（按项目需求加载模块）
- [ ] Gateway 与 Realtime 的路由打通

### Phase 2b/2c（可选分支）
- [ ] `realtime-cpp` 新增 `spatial_scene` 插件：AOI 广播 + 场景切换
- [ ] `realtime-cpp` 新增 `async_combat` 插件：异步战斗计算 + 回调 Biz

### Phase 3：运营与工具
- [ ] `logstats-go`：日志收集与实时统计
- [ ] `bot-go` / `bot-cpp`：压测机器人与自动化测试骨架
- [ ] 监控告警、链路追踪（Trace）

### Phase 4：生产化
- [ ] Docker / Kubernetes 部署
- [ ] 多区服、灰度发布
- [ ] 性能调优与回放/场景快照系统

---

## 9. 基础设施详细实施方案计划

详细实施路径（分 6 个 Phase 的任务分解、验收标准、依赖关系与风险控制）已独立为配套文档：

**→ [IMPLEMENTATION.md](IMPLEMENTATION.md)**

---

## 10. 待补充项

> 以下内容后续根据实际落地情况补充完善。

- [ ] 安全方案：加密算法选择、防重放攻击、Token 刷新机制
- [ ] 详细的心跳与断线重连策略（网关层 + 客户端 SDK）
- [ ] 战斗同步方案选型：帧同步 vs 状态同步 的详细对比与落地
- [ ] 数据持久化策略：缓存一致性、玩家数据冷热分离、回档方案
- [ ] 多区服/跨服架构：全球同服 vs 分区分服的注册中心标签设计
- [ ] 限流、熔断、降级策略的具体参数与实现
- [ ] CI/CD 流程与版本兼容性管理（protobuf 向前/向后兼容规范）
- [ ] C++ 侧编译依赖管理：vcpkg / conan / xmake 选型

---

## 11. 修改记录

| 日期 | 版本 | 修改内容 | 作者 |
|------|------|----------|------|
| 2026-04-17 | v0.1 | 初始架构设计：跨语言选型、协议规范、服务职责、目录结构 | - |
| 2026-04-17 | v0.2 | 语言范围收窄为 Go 与 C++，调整各服务语言选型、目录结构、MVP 验证链路 | - |
| 2026-04-17 | v0.3 | 确定服务发现方案为 Registry(Go) + Etcd，补充 Etcd 存储设计、Lease 机制、故障转移策略，新增 `spec/proto/registry.proto` | - |
| 2026-04-17 | v0.4 | 新增 Redis 基础设施设计：部署架构、Key 规范、各服务使用约定、缓存一致性策略、DB Proxy 的 Redis 代理增强 | - |
| 2026-04-17 | v0.5 | 新增公共基础设施与公共库设计：ID 生成、分布式锁、限流熔断、监控追踪、配置中心、安全库、common/ 目录模块划分、开发工具链 | - |
| 2026-04-17 | v0.6 | 重构 4.1 通信协议为固定二进制头 + protobuf 包体，详细定义帧格式、Flag 位掩码、编解码流程；新增 4.7.9 配置版本管理与推送（分层模型、Etcd Watch、灰度回滚） | - |
| 2026-04-17 | v0.7 | 框架扩展为支持多种实时场景：Realtime Server 插件化抽象、Gateway 策略适配、Biz 模块化加载、DBProxy 差异化持久化策略、AOI 广播设计、新增 6.1 多项目适配设计 | - |
| 2026-04-17 | v0.8 | 将具体游戏类型命名修正为通用能力命名：`moba_room`→`sync_room`、`mmorpg_scene`→`spatial_scene`、`slg_async_battle`→`async_combat`；DBProxy/Biz/Gateway 描述去游戏化 | - |
| 2026-04-17 | v0.9 | 新增 4.6 网络库架构设计：C++ 侧 IO 线程 + Compute 线程 + Async IO 线程三层模型、无锁队列消息流转、Room/Scene 绑定策略；Go 侧 goroutine 池 + 多 IO 协作（并行聚合 / 管道顺序）模型；同步修复文档中残留的 `Battle`→`Realtime`、`battle:room`→`room:snapshot` 命名 | - |
| 2026-04-17 | v1.0 | 新增 IMPLEMENTATION.md：分 6 个 Phase 定义了从通信骨架到生产化部署的落地路径、任务分解表、验收标准、依赖关系图与风险控制；DESIGN.md 第 9 章改为引用链接；工程目录结构已同步落地到磁盘 | - |
