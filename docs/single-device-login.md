# 单设备登录设计方案（顶号机制）

## 背景

玩家通过 HTTP Login 服务获取 `token` 和 `player_id` 后，通过 TCP/WebSocket 连接到 Gateway。为了避免同一玩家同时在多个客户端保持连接（例如账号被盗、多端登录），需要在 Gateway 层实现**单设备登录**（Single-Device Login）机制：当新连接绑定同一 `player_id` 时，Gateway 主动踢掉旧连接。

## 架构原则

1. **Gateway 维护连接绑定** — Gateway 是连接层，最适合做 `player_id → conn_id` 的映射和踢人操作
2. **Biz 验证身份** — Token 验证仍由 Biz（业务层）完成，Gateway 不直接访问 Redis（避免在 libuv 事件循环中引入阻塞 I/O）
3. **两步绑定** — Gateway 先转发 BIND 请求给 Biz 验证，收到 Biz 确认成功响应后再执行绑定和顶号，防止伪造 player_id 导致合法玩家被误踢

## 数据流

```
┌─────────┐     ┌──────────┐     ┌─────────┐     ┌───────┐
│ Client  │────▶│ Gateway  │────▶│   Biz   │────▶│ Redis │
│(新连接)  │     │ (C++)    │     │  (Go)   │     │       │
└─────────┘     └──────────┘     └─────────┘     └───────┘
                      │
                      ▼
               ┌─────────────┐
               │ pending_binds│  conn_id → player_id (等待 Biz 确认)
               │ player_conn  │  player_id → conn_id (已绑定)
               │ conn_player  │  conn_id → player_id (已绑定)
               └─────────────┘
```

### 绑定流程

```
1. 客户端 Handshake 完成后，发送 CMD_GW_PLAYER_BIND
   Payload: PlayerBindReq { player_id, token }

2. Gateway 收到 BIND
   - 解析 player_id
   - 记录 pending_binds_[conn_id] = player_id
   - 将请求转发给 Biz（payload 前附加 8 字节 conn_id）

3. Biz 收到 BIND
   - 从 Redis 验证 token:{player_id} 是否匹配
   - 返回 PlayerBindRes { code, msg }

4. Gateway 收到 BIND 响应（OnUpstreamPacket 拦截 CMD_GW_PLAYER_BIND）
   - code == 0（成功）:
     * 从 pending_binds_ 取出 player_id
     * 调用 BindPlayer(player_id, conn_id)
     * BindPlayer: 如果 player_id 已有旧 conn_id，先 KickConnection(旧连接)
   - code != 0（失败）:
     * 从 pending_binds_ 移除 conn_id，不执行绑定

5. Gateway 将响应转发给客户端（去掉 conn_id 前缀）
```

### 踢人流程

```
BindPlayer 发现 player_id 已有旧连接:
  1. 构造 PlayerKickNotify { reason = "account login elsewhere" }
  2. 使用 SendToClientDirect 发送 CMD_GW_PLAYER_KICK 给旧 conn_id
     - 自动加密（复用该连接的 session key）
     - 通过 coalescer 异步发送
  3. 关闭旧连接（conn->Close()）
  4. OnClientClose / OnWebSocketClose 触发 CleanupPlayerState
     - 清理 player_conn_ / conn_player_ / pending_binds_
```

## 协议变更

### spec/proto/protocol.proto

```protobuf
enum CmdGatewayInternal {
    CMD_GW_PLAYER_BIND = 0x00001020;  // 玩家绑定请求
    CMD_GW_PLAYER_KICK = 0x00001021;  // 玩家被踢通知
}

message PlayerBindReq {
    uint64 player_id = 1;
    string token     = 2;
}

message PlayerBindRes {
    uint32 code = 1;  // 0 = 成功
    string msg  = 2;
}

message PlayerKickNotify {
    string reason = 1;
}
```

### spec/cmd_ids.yaml

```yaml
gateway_internal:
  GW_PLAYER_BIND: 0x00001020
  GW_PLAYER_KICK: 0x00001021
```

## 代码变更

### Gateway (C++) — services/gateway-cpp/main.cpp

新增数据结构:
```cpp
std::mutex player_mtx_;
std::unordered_map<uint64_t, uint64_t> player_conn_;   // player_id → conn_id
std::unordered_map<uint64_t, uint64_t> conn_player_;   // conn_id → player_id
std::unordered_map<uint64_t, uint64_t> pending_binds_; // conn_id → player_id
```

新增方法:
- `HandlePlayerBind()` — 解析 BIND 请求，记录 pending，转发给 Biz
- `BindPlayer()` — 执行绑定，发现旧连接时调用 KickConnection
- `KickConnection()` — 发送踢人通知并关闭连接
- `CleanupPlayerState()` — 断线时清理所有 player 相关状态
- `SendToClientDirect()` — 直接给指定 conn_id 发送加密包

修改点:
- `OnClientPacket()` — 拦截 `CMD_GW_PLAYER_BIND`
- `OnUpstreamPacket()` — 拦截 `CMD_GW_PLAYER_BIND` 响应，确认后执行绑定
- `OnClientClose()` / `OnWebSocketClose()` — 调用 `CleanupPlayerState()`

### Biz (Go) — services/biz-go/internal/handler/handler.go

新增 handler:
- `handlePlayerBind()` — 解析 `PlayerBindReq`，调用 `svc.VerifyToken()` 验证 token
- `sendPlayerBindRes()` — 构造 `PlayerBindRes` 响应

新增 service 方法 — services/biz-go/internal/service/player.go:
- `VerifyToken()` — 从 Redis `token:{player_id}` 读取并比对 token

## 客户端接入指南

### 连接后流程

```
1. TCP/WS 连接 Gateway
2. 完成 Handshake（加密通道建立）
3. 发送 CMD_GW_PLAYER_BIND
   protobuf: PlayerBindReq {
       player_id: <从 /login 获取的 player_id>
       token:     <从 /login 获取的 token>
   }
4. 等待 PlayerBindRes 响应
   - code == 0: 绑定成功，可以发送业务请求
   - code != 0: 绑定失败（token 无效），应断开连接并重新登录
5. 正常进行游戏业务（GetPlayer、UpdatePlayer 等）
```

### 被踢处理

客户端需要监听 `CMD_GW_PLAYER_KICK`:
```
收到 CMD_GW_PLAYER_KICK:
   protobuf: PlayerKickNotify { reason: "account login elsewhere" }
   → 显示提示："您的账号已在其他地方登录"
   → 断开连接
   → 返回登录界面
```

## 安全考量

1. **Token 验证在 Biz 层完成** — Gateway 不直接验证 token，避免在 C++ 中引入 Redis 同步阻塞调用
2. **两步绑定防止误踢** — Gateway 在 Biz 确认 token 有效后才执行绑定，防止攻击者伪造 player_id 顶掉合法玩家
3. **Handshake 前置** — 只有完成加密握手的连接才能发送 BIND，增加了伪造成本
4. **断线自动清理** — 任何连接断开都会自动清理 player_conn_ / conn_player_，避免脏数据

## 已知限制

1. **C++ 编译环境** — 当前 Windows Debug 构建存在 `libprotobuf.lib` Debug/Release 不匹配问题（`_ITERATOR_DEBUG_LEVEL` 0 vs 2）。Gateway C++ 代码已更新，但需切换到 Release 模式或重新编译 protobuf Debug 库后才能构建。
2. **多 Gateway 节点** — 当前方案仅在单个 Gateway 进程内有效。如果部署多个 Gateway 节点，同一 player_id 可能同时连接到不同 Gateway 节点。后续可通过 Redis 全局在线状态 + Gateway 间通知来解决。
3. **pending_bind 超时** — 当前未实现 pending_bind 的超时清理。如果 Biz 不响应，pending_bind 会一直保留。后续可添加定时器清理超时的 pending 记录。
