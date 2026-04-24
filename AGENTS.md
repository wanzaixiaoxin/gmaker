# gmaker — AI Agent 项目指南

> 本文档面向 AI Coding Agent，假设读者对项目一无所知。所有信息均基于当前代码库实际内容，不做推测。
> 项目主要使用中文进行注释与文档编写，本文件沿用同一语言。

---

## 1. 项目概述

`gmaker` 是一个**去中心化服务端框架骨架**，采用 **Go + C++ 双语言栈**，通过自研 TCP 二进制协议 + protobuf3 进行服务间通信。

- **Go**：负责业务服（Biz）、注册中心（Registry）、数据库代理（DBProxy）、日志统计（LogStats）等 IO/业务密集型服务。
- **C++**：负责网关（Gateway）、实时服（Realtime）等网络/计算密集型服务。
- **通信协议**：18 字节定长二进制帧头 + protobuf3 包体，大端序，Magic = `0x9D7F`。
- **服务发现**：自研 Registry（Go）作为通用服务发现层，后端支持 etcd 或 memory（兜底）。同时提供直连 etcd 的统一封装（Go + C++），业务服务可通过配置切换后端。
- **当前状态**：框架骨架已完成 6 个 Phase 的落地（详见 `IMPLEMENTATION.md`），核心链路 `Client -> Gateway(C++) -> Biz(Go) -> Registry(Go)` 已跑通。

---

## 2. 技术栈与关键配置

| 层面 | 技术/工具 | 配置文件 |
|------|-----------|----------|
| Go 版本 | 1.22+ | 根目录 `go.mod`，各服务子目录也有独立 `go.mod` |
| C++ 标准 | C++17 | `CMakeLists.txt`（CMake 3.16+） |
| 构建系统 | Makefile（Go + Proto）+ CMake（C++） | `Makefile`、`CMakeLists.txt` |
| 协议 | protobuf3 | `spec/proto/*.proto` |
| 网络库（C++） | libuv（异步 IO）+ WinSock2（阻塞模型 legacy） | `3rd/libuv/`（Git submodule） |
| 网络库（Go） | 标准库 `net` 薄封装 | `common/go/net/` |
| 配置格式 | TOML / YAML | `spec/errors.toml`、`spec/rpc_spec.yaml` |
| 容器/部署 | Docker Compose | `tools/deploy/docker-compose.yml` |
| CI/CD | GitHub Actions | `.github/workflows/ci.yml` |

### 根级依赖文件
- **`go.mod`** / **`go.sum`**：根 Go 模块，`github.com/gmaker/luffa`，依赖 `google.golang.org/grpc` 和 `protobuf`。
- **`CMakeLists.txt`**：定义 C++ 编译目标（`gateway-cpp`、`realtime-cpp`、`test-crypto`、`test-async-net`、`test-redis`、`test-net-perf-client`、`test-net-perf-server`）。
- **`Makefile`**：提供 `proto`、`build-go`、`build-cpp`、`build`、`test`、`clean` 命令。

---

## 3. 目录结构

```
gmaker/
├── common/                 # 公共库（Go + C++ 双语言对称实现）
│   ├── go/                 # Go 公共库
│   │   ├── net/            # TCP Server/Client、拆包粘包、连接管理
│   │   ├── rpc/            # RPC 客户端（Req-Res、Fire-Forget、超时）
│   │   ├── discovery/      # 统一服务发现封装（Registry / etcd 双后端）
│   │   ├── registry/       # Registry SDK（注册/心跳/发现/监听，由 discovery 包装）
│   │   ├── logger/         # 结构化 JSON 日志（自研，非 zap）
│   │   ├── metrics/        # Prometheus 风格指标（Counter/Gauge/Histogram）
│   │   ├── config/         # 配置加载 + Etcd Watch 热重载
│   │   ├── crypto/         # AES-256-GCM、HMAC-SHA256
│   │   ├── idgen/          # Snowflake ID 生成器
│   │   ├── limiter/        # 令牌桶限流 + 熔断器
│   │   ├── lock/           # Redis 分布式锁
│   │   ├── trace/          # TraceID 生成与 Context 传播
│   │   ├── replay/         # 防重放检查
│   │   └── errors/         # 错误码封装
│   └── cpp/gs/             # C++ 公共库（命名空间 gs::*）
│       ├── net/            # TCP 网络框架（阻塞 + libuv 异步双轨）
│       ├── rpc/            # RPC 客户端
│       ├── discovery/      # 统一服务发现封装（Registry / etcd）
│       ├── registry/       # Registry SDK（由 discovery 包装）
│       ├── logger/         # JSON 日志封装
│       ├── metrics/        # Prometheus 指标
│       ├── config/         # TOML 配置加载（基于 toml11）
│       ├── crypto/         # AES/GCM、HMAC、会话密钥派生
│       ├── idgen/          # Snowflake
│       ├── limiter/        # 令牌桶
│       ├── realtime/       # 实时服基础设施（Room、ComputeThread、AOI）
│       └── replay/         # 防重放
├── services/               # 可独立部署的服务
│   ├── registry-go/        # 注册中心（支持 etcd / memory 双后端，可选部署）
│   ├── biz-go/             # 业务服骨架（登录、玩家数据、Ping）
│   ├── dbproxy-go/         # 数据库代理（MySQL 统一入口；Redis 由各服务直接连接）
│   ├── logstats-go/        # 日志收集与实时聚合
│   ├── gateway-cpp/        # C++ 网关（客户端接入、转发 Biz、加密握手）
│   └── realtime-cpp/       # C++ 实时服骨架（Room + ComputeThread）
├── spec/                   # 协议与规范定义
│   ├── proto/              # protobuf3 定义（protocol、common、biz、login、chat、dbproxy、registry、packet）
│   ├── cmd_ids.yaml        # 全局命令号定义
│   ├── errors.toml         # 全局错误码体系
│   └── rpc_spec.yaml       # RPC 行为契约（超时、重试）
├── gen/                    # 生成的 protobuf 代码
│   ├── go/                 # Go 生成代码（protoc-gen-go / protoc-gen-go-grpc）
│   └── cpp/                # C++ 生成代码（protoc-gen-cpp）
├── tests/                  # 端到端联调测试
│   ├── phase1/             # Phase 1：Client -> Gateway -> Biz -> Registry 链路
│   └── phase2/             # Phase 2：登录 -> 读玩家数据 -> 修改 -> 写回（需 MySQL+Redis）
├── tools/                  # 工具与部署脚本
│   ├── deploy/             # Docker Compose、Grafana Dashboard、Prometheus 配置
│   ├── benchmark/          # 压测工具（占位）
│   ├── gen-errors/         # 错误码生成工具（占位）
│   └── testclient/         # 测试客户端（已实现，支持 login / heartbeat / flood / chat 场景）
└── 3rd/                    # 第三方依赖
    ├── libuv/              # C++ 异步网络库
    ├── protobuf/           # protobuf 34.1 源码与构建产物
    ├── hiredis/            # Redis C 客户端
    └── rapidjson/          # JSON 解析（header-only）
```

---

## 4. 构建与测试命令

### 环境要求（已验证 Windows + MSYS2/MinGW）
- Go 1.22+
- protoc 25.1+ + `protoc-gen-go` + `protoc-gen-go-grpc`
- CMake 3.16+ + MSVC 2022（Windows）或 MinGW
- etcd（可选；未安装时 Registry 可用 `-store memory` 运行）
- MySQL 8.0 + Redis 7（仅 Phase 2 测试及完整部署需要）

### 常用命令

```bash
# 生成 protobuf 代码（Go + C++）
make proto

# Windows 下如果没有 make，可直接运行：
#   scripts\gen-proto.bat        (CMD)
#   scripts\gen-proto.ps1        (PowerShell)

# 编译全部 Go 服务（输出到 bin/）
make build-go
# 产物：bin/registry-go.exe、bin/dbproxy-go.exe、bin/biz-go.exe、bin/chat-go.exe、bin/logstats-go.exe

# 编译全部 C++ 服务（CMake Release 模式）
make build-cpp
# 产物：build/Release/*.exe，最终由 make build 自动复制到 bin/

# 一键编译全部
make build

# 运行 Go 单元测试
make test
# 仅测试 common/go/net：cd common/go/net && go test -v ./...

# 清理生成文件与编译产物
make clean
```

### C++ 测试二进制文件
CMake 会额外生成多个测试可执行文件：
- `build/Release/test-crypto.exe`：AES-GCM / HMAC 自测
- `build/Release/test-async-net.exe`：libuv 异步网络层自测
- `build/Release/test-redis.exe`：Redis 连接测试
- `build/Release/test-net-perf-client.exe` / `test-net-perf-server.exe`：网络性能压测

### 一键联调测试

```bash
# Phase 1（无需 MySQL/Redis，仅需编译产物）
go run tests/phase1/main.go

# Phase 2（需要本地 MySQL + Redis 运行，并准备对应 DSN）
go run tests/phase2/main.go
```

---

## 5. 代码组织与模块约定

### 5.1 命令号与协议规范

所有命令号统一定义在 `spec/cmd_ids.yaml`，范围划分：
- `0x00000001 ~ 0x00000FFF`：系统保留（心跳、握手、错误包）
- `0x00001000 ~ 0x0000FFFF`：公共协议（登录、注册、Gateway 转发）
- `0x00010000 ~ 0x7FFFFFFF`：业务协议（Biz、Realtime、DBProxy 内部按模块分段）

**修改协议时必须先改 `spec/cmd_ids.yaml`，再同步到各语言常量定义。**

### 5.2 Go 服务内部结构

Go 服务遵循以下目录惯例：
```
services/{name}-go/
├── main.go                 # 入口：flag 解析、组件初始化、信号处理
├── internal/
│   ├── handler/            # 请求处理与业务路由
│   ├── service/            # 业务领域服务（如 player/、auth/）
│   └── dbproxy/            # 下游 DBProxy 客户端封装
└── go.mod                  # 服务级 Go 模块（部分服务有，部分复用根模块）
```

> 注：`biz-go` 是典型范例，其 `internal/` 下分为 `handler/`（路由分发）、`service/`（玩家业务）、`dbproxy/`（下游代理客户端），无 `server/` 子包。

### 5.3 C++ 服务内部结构

C++ 服务以 `main.cpp` 为主入口，配合公共库实现。当前 `gateway-cpp` 已包含完整的 `Gateway` 类、中间件链（`HandshakeMiddleware` / `EncryptionMiddleware`）、上游连接池管理、Room 成员管理、信号处理等，不再是简单的"骨架阶段"单文件模式。公共库头文件统一放在 `common/cpp/gs/{module}/`。

### 5.4 公共库双语言对称约定

| 功能 | Go 包路径 | C++ 路径/命名空间 |
|------|-----------|-------------------|
| 网络 | `common/go/net` | `common/cpp/gs/net` |
| RPC | `common/go/rpc` | `common/cpp/gs/rpc` |
| 服务发现 | `common/go/discovery` | `common/cpp/gs/discovery` |
| Registry SDK | `common/go/registry` | `common/cpp/gs/registry` |
| 日志 | `common/go/logger` | `common/cpp/gs/logger` |
| 指标 | `common/go/metrics` | `common/cpp/gs/metrics` |
| 配置 | `common/go/config` | `common/cpp/gs/config` |
| 加密 | `common/go/crypto` | `common/cpp/gs/crypto` |
| ID 生成 | `common/go/idgen` | `common/cpp/gs/idgen` |
| 限流 | `common/go/limiter` | `common/cpp/gs/limiter` |
| Redis | `common/go/redis` | `common/cpp/gs/redis` |

**原则**：同一功能在两种语言中的模块名、接口语义、常量值应尽量保持一致，方便跨语言维护。

---

## 6. 测试策略

### 6.1 单元测试
- **Go**：位于 `common/go/{package}/*_test.go`，使用标准库 `testing`。运行方式：`go test ./common/go/...`
- **C++**：当前以可执行文件形式存在（`test-crypto`、`test-async-net`），未引入 GTest/GoogleBenchmark（预留接口）。

### 6.2 端到端集成测试
- **`tests/phase1/main.go`**：验证 `Client -> Gateway(C++) -> Biz(Go) -> Registry(Go)` 整条链路（Registry 模式）。自动拉起 Registry（memory 模式）、Biz、Gateway，模拟客户端发送 Ping 包。
- **`tests/phase2/main.go`**：验证完整数据链路（登录 -> 读玩家数据 -> 修改 -> 写回）。需要本地 MySQL（默认 `root:123456@tcp(127.0.0.1:3306)/gmaker`）和 Redis（默认 `127.0.0.1:6379`）。

### 6.3 重大改动后的强制验证标准

任何涉及以下范围的 PR 或本地提交，在合并前**必须**完成以下三步验证：

1. **全量编译通过**
   ```bash
   make build        # 或 Windows: build.bat
   ```
   - Go：全部服务产物输出到 `bin/*.exe`
   - C++：`build/Release/gateway-cpp.exe`、`realtime-cpp.exe`、`test-crypto.exe`、`test-async-net.exe`

2. **Phase 1 端到端联调通过**
   ```bash
   # 方式 A：程序化测试（推荐 CI 使用）
   go run tests/phase1/main.go

   # 方式 B：手动一键启动（本地开发使用）
   scripts\start-minimal.bat
   ```
   验收标准：
   - Registry、Biz、Gateway 正常启动且无 ERROR 级日志
   - Client 成功完成 Handshake（`Handshake completed`）
   - Client 成功发送 Ping 并收到 Pong（`cmd=65541`）
   - `start-minimal` 场景下 TestClient heartbeat 至少成功 1 次

3. **清理验证**
   运行 `scripts\stop-all.bat`（或手动 `taskkill`）确保无残留进程，避免端口占用导致下次测试失败。

> **为什么强制？** 本项目为 Go + C++ 双语言栈，且存在协议层（握手帧格式、conn_id 前缀、FlagEncrypt 标志）的跨语言隐式契约。单一语言的单元测试无法覆盖这些边界，历史多次回归均因只测了单侧导致。

### 6.4 CI/CD 流水线
`.github/workflows/ci.yml` 包含 5 个 Job：
1. `go-build`（ubuntu-latest）：编译全部 Go 服务 + 运行 `go test ./common/go/...`
2. `cpp-build`（windows-latest）：CMake 配置并编译 C++ 服务 + 运行 `test-crypto.exe`
3. `proto-check`（ubuntu-latest）：验证 protoc 版本，检查生成代码是否最新（TODO 中）
4. `e2e-phase1`（windows-latest）：编译全部产物后运行 `go run tests/phase1/main.go`，验证最小链路
5. `docker-build`（ubuntu-latest）：依赖前序 Job，构建 Docker 镜像并 `docker compose up` 冒烟测试

---

## 7. 开发风格指南

### 7.1 语言与注释
- **所有注释、文档、日志输出使用中文**。代码中的变量/函数名使用英文。
- 公共库接口必须带中文注释说明用途与约束。

### 7.2 Go 代码风格
- 使用标准 `gofmt` 格式化。
- 错误处理显式返回，不使用 panic 传递业务错误。
- `context.Context` 用于超时控制和 TraceID 传播。
- 包级常量命名：命令号使用 `CmdXxx` 前缀，Flag 使用 `FlagXxx`。

### 7.3 C++ 代码风格
- 命名空间：`gs::{module}`，如 `gs::net`、`gs::crypto`。
- 文件命名：小写 + 下划线，如 `tcp_server.cpp`、`aes_gcm.hpp`。
- 结构体/类：PascalCase，函数：PascalCase（项目当前风格，非 Google Style 的 snake_case）。
- 大端序读写工具函数统一放在 `gs::net` 中（`WriteU32BE`、`ReadU32BE` 等）。

### 7.4 协议修改流程
1. 修改 `spec/proto/*.proto`
2. 修改 `spec/cmd_ids.yaml`（如有新增命令号）
3. 运行 `make proto` 重新生成 `gen/go/` 和 `gen/cpp/`
4. 同步更新 Go/C++ 源码中的常量定义
5. **严禁直接修改 `gen/` 目录下的生成代码**

---

## 8. 安全注意事项

### 8.1 通信安全
- Gateway 与客户端之间支持 **AES-256-GCM** 加密，通过握手流程交换会话密钥（`HandshakeMiddleware`）。
- 握手包带 **timestamp + nonce**，Gateway 使用 `ReplayChecker`（300 秒窗口）防重放攻击。
- 内部服务间通信当前**不加密**（内网信任域），但通过 `FlagRPCReq`/`FlagRPCRes` 标记 RPC 类型。

### 8.2 数据安全
- 密码存储使用 SHA256（当前骨架简化实现），生产环境应迁移到 Argon2id（已在 `DESIGN.md` 中规划）。
- 各服务直接连接 Redis（不再经过 DBProxy 代理）。Redis 热点 Key 限流和危险命令拦截由各服务自行实现（当前预留接口）。

### 8.3 服务安全
- Gateway 支持令牌桶限流（`common/cpp/gs/limiter/token_bucket.cpp`）。
- Biz 服集成熔断器（`common/go/limiter/circuit_breaker.go`），基于错误率计数（Close/Open/Half-Open）。

---

## 9. 部署与运行

### 9.1 手动启动（Phase 1 最小链路）

```bash
# 1. 启动 Registry（内存模式，无需 etcd）
./bin/registry-go.exe -listen 127.0.0.1:2379 -store memory

# 2. 启动 Biz
./bin/biz-go.exe -config biz.json

# 3. 启动 Gateway（需要 gateway.json 配置文件在工作目录）
./bin/gateway-cpp.exe --config gateway.json
```

### 9.2 Docker Compose 全服编排

```bash
cd tools/deploy
docker compose up -d
```

编排包含：etcd、mysql、redis、registry-go、dbproxy-go、biz-go×2、gateway-cpp、realtime-cpp、logstats-go、prometheus、grafana。

### 9.3 关键端口说明

| 服务 | 业务端口 | Metrics 端口 | 说明 |
|------|----------|--------------|------|
| registry-go | 2379 | - | 服务发现 TCP 端口 |
| dbproxy-go | 3307 | - | DB 代理 TCP 端口 |
| biz-go | 8082 | 9082 | 业务服 + Prometheus `/metrics` |
| gateway-cpp | 8081 | 9081 | 网关 + Prometheus `/metrics` |
| realtime-cpp | 8084 | 9090 | 实时服 + Prometheus `/metrics` |
| logstats-go | 8085 | 8086 | 日志接收 + HTTP 查询 |
| prometheus | - | 9091 | 指标采集 |
| grafana | - | 3000 | 监控面板 |

---

## 10. 扩展与新增服务

若需新增一种服务类型（如 `match-go` 或 `chat-cpp`）：

1. **协议层**：在 `spec/proto/` 添加 `.proto` 文件；在 `spec/cmd_ids.yaml` 分配命令号段。
2. **公共库层**：若需新基础设施，在 `common/go/` 和 `common/cpp/gs/` 对称添加模块。
3. **服务层**：在 `services/` 新建目录，实现 `main.go` / `main.cpp`，遵循现有服务的初始化模式（flag 解析 → 组件初始化 → Registry 注册 → TCP 服务启动 → 信号监听）。
4. **部署层**：在 `tools/deploy/docker-compose.yml` 添加服务定义；在 `.github/workflows/ci.yml` 添加构建步骤。

---

## 11. 参考文档

- `README.md`：快速开始、编译运行命令
- `DESIGN.md`：完整架构设计文档（服务职责、网络模型、Redis 规范、配置中心、安全基础库等）
- `IMPLEMENTATION.md`：6 个 Phase 的落地实施计划与验收标准
- `DESIGN_REVIEW.md`：设计评审记录

---

> 最后更新：基于当前代码库实际内容生成。若项目结构发生变更，请同步更新本文件。
