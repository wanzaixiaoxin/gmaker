# gmaker

去中心化游戏服务器框架骨架，采用 Go + C++ 双语言栈，通过 TCP + protobuf3 进行服务间通信。

## 技术栈

- **Go**：业务服（Biz）、注册中心（Registry）、日志统计（LogStats）等 IO/业务密集型服务
- **C++**：网关（Gateway）、实时服（Realtime）等网络/计算密集型服务
- **协议**：18 字节定长二进制帧头 + protobuf3 包体
- **服务发现**：Registry (Go) + Etcd（Phase 1 提供 memory 模式兜底）
- **构建**：Makefile（Go）+ CMake（C++）

## 目录结构

```
common/
  go/net/        Go 网络库（TCP 连接/拆包/服务端/客户端）
  go/registry/   Go Registry SDK
  go/rpc/        Go RPC 封装（Req-Res 配对、超时）
  cpp/gs/net/    C++ 网络库（WinSock2 阻塞模型）
  cpp/gs/registry/  C++ Registry SDK
  cpp/gs/rpc/    C++ RPC 封装
services/
  registry-go/   注册中心（支持 etcd / memory 双后端）
  biz-go/        业务服骨架
  gateway-cpp/   网关服（客户端接入 + 转发 Biz）
  realtime-cpp/  实时服骨架
spec/
  proto/         protobuf3 协议定义
  cmd_ids.yaml   全局命令号
  errors.toml    错误码体系
  rpc_spec.yaml  RPC 行为契约
tests/phase1/    Phase 1 端到端联调测试
```

## 环境准备

当前已在 Windows + MSYS2/MinGW 环境验证，需要：

- Go 1.22+
- protoc 25.1+ + Go 插件（`protoc-gen-go`、`protoc-gen-go-grpc`）
- CMake 3.16+ + MSVC 2022
- etcd（可选；未安装时 Registry 可用 `-store memory` 运行）

> 若本地未安装上述工具，可下载并加入 `PATH` 后使用。

## 编译

```bash
# 生成 protobuf 代码（Go + C++）
make proto

# 编译全部 Go 服务
make build-go

# 编译全部 C++ 服务
make build-cpp

# 一键编译全部
make build
```

编译产物：
- `bin/registry-go.exe`
- `bin/biz-go.exe`
- `build/Release/gateway-cpp.exe`
- `build/Release/realtime-cpp.exe`

## 运行

### 手动启动（Phase 1 链路）

```bash
# 1. 启动 Registry（内存模式，无需 etcd）
./bin/registry-go.exe -listen 127.0.0.1:2379 -store memory

# 2. 启动 Biz
./bin/biz-go.exe -listen 127.0.0.1:8082 -registry 127.0.0.1:2379

# 3. 启动 Gateway（参数：gateway_port biz_host biz_port）
./build/Release/gateway-cpp.exe 8081 127.0.0.1 8082
```

### 一键联调测试

```bash
go run tests/phase1/main.go
```

测试会自动拉起 Registry、Biz、Gateway，并模拟 Client 通过 Gateway 向 Biz 发送 Ping 包，验证 `Client -> Gateway(C++) -> Biz(Go) -> Registry(Go)` 整条链路。
