# Spec 规范目录说明

本目录存放框架所有语言无关的规范与协议定义，是 Go/C++ 双语言通信的唯一事实来源。

## 文件清单

| 文件 | 说明 |
|------|------|
| `proto/packet.proto` | 通用 Packet 消息定义（调试/日志/透传场景） |
| `proto/common.proto` | 跨服务通用结构：Result、PageReq、PageRes、Empty |
| `proto/registry.proto` | 注册中心协议：NodeInfo、NodeEvent、Registry 服务接口 |
| `proto/login.proto` | 登录/注册协议：LoginReq/Res、RegisterReq/Res、VerifyToken |
| `proto/biz.proto` | 业务服基础协议：PlayerBase、GetPlayer、GetBag、Ping/Pong |
| `cmd_ids.yaml` | 全局命令号分配表，所有服务共用 |
| `errors.toml` | 全局错误码定义表，按模块分段 |
| `rpc_spec.yaml` | RPC 行为契约：超时、重试、广播策略 |

## 修改规范

1. **任何人修改 proto 前必须在 README 中登记变更原因与影响范围**
2. **命令号（CmdId）一旦分配不可复用**，废弃命令号标记为 `DEPRECATED` 但保留编号
3. **protobuf 字段编号不可复用、不可修改语义**，只增不改不删
4. **错误码按模块分段**，新增模块需先在 `errors.toml` 中申请空段

## 代码生成

```bash
# 从仓库根目录执行
make proto
```

生成结果位于：
- `gen/go/` — Go 代码
- `gen/cpp/` — C++ 代码
