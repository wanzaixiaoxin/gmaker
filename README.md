# gmaker

去中心化服务端框架骨架，采用 **Go + C++ 双语言栈**，通过自研 TCP 二进制协议 + protobuf3 进行服务间通信。

> 当前已完成 6 个 Phase 的基础设施落地，核心链路 `Client -> Gateway(C++) -> Biz(Go) -> Registry(Go)` 已跑通。

---

## 技术栈

| 层面 | 技术/工具 | 说明 |
|------|-----------|------|
| **Go** | 1.22+ | 业务服（Biz）、注册中心（Registry）、数据库代理（DBProxy）、日志统计（LogStats）等 IO/业务密集型服务 |
| **C++** | C++17 | 网关（Gateway）、实时服（Realtime）等网络/计算密集型服务 |
| **协议** | protobuf3 | 18 字节定长二进制帧头 + protobuf3 包体，大端序 |
| **网络（C++）** | libuv | 异步 IO（已统一替代阻塞模型） |
| **网络（Go）** | 标准库 `net` | 薄封装，自研拆包粘包 |
| **服务发现** | Registry (Go) + discovery 封装 | 支持 Registry 自研协议 或 直连 etcd 双方案 |
| **构建** | Makefile + CMake | Makefile 负责 Go + Proto，CMake 负责 C++ |
| **部署** | Docker Compose | 完整编排见 `tools/deploy/` |

---

## 目录结构

```
gmaker/
├── common/                 # 公共库（Go + C++ 双语言对称实现）
│   ├── go/net/             # TCP 网络框架（Server/Client、拆包粘包）
│   ├── go/discovery/       # 统一服务发现封装（Registry / etcd）
│   ├── go/registry/        # Registry SDK（由 discovery 包装）
│   ├── go/rpc/             # RPC 客户端（Req-Res、Fire-Forget、超时）
│   ├── go/crypto/          # AES-256-GCM、HMAC-SHA256
│   ├── go/limiter/         # 令牌桶限流 + 熔断器
│   ├── cpp/gs/net/         # C++ 异步网络封装（libuv）
│   ├── cpp/gs/discovery/   # C++ 统一服务发现封装（Registry / etcd）
│   ├── cpp/gs/registry/    # C++ Registry SDK（由 discovery 包装）
│   ├── cpp/gs/rpc/         # C++ RPC 封装
│   └── cpp/gs/crypto/      # C++ 加密库
├── services/               # 可独立部署的服务
│   ├── registry-go/        # 注册中心（etcd / memory 双后端）
│   ├── biz-go/             # 业务服骨架（登录、玩家数据、Ping）
│   ├── dbproxy-go/         # 数据库代理（MySQL 统一入口；Redis 由各服务直接连接）
│   ├── logstats-go/        # 日志收集与实时聚合
│   ├── chat-go/            # 聊天服（骨架）
│   ├── login-go/           # 登录验签服（占位）
│   ├── bot-go/             # 压测机器人（占位）
│   ├── gateway-cpp/        # C++ 网关（客户端接入、转发 Biz、加密握手）
│   └── realtime-cpp/       # C++ 实时服骨架（Room + ComputeThread）
├── spec/                   # 协议与规范
│   ├── proto/              # protobuf3 定义
│   ├── cmd_ids.yaml        # 全局命令号
│   ├── errors.toml         # 错误码体系
│   └── rpc_spec.yaml       # RPC 行为契约
├── gen/                    # 生成的 protobuf 代码（Go + C++）
├── tests/
│   ├── phase1/             # Phase 1 端到端联调（无需 MySQL/Redis）
│   └── phase2/             # Phase 2 数据链路测试（需 MySQL + Redis）
├── tools/
│   ├── deploy/             # Docker Compose 全服编排
│   └── testclient/         # 测试客户端
└── 3rd/                    # 第三方依赖
    ├── libuv/              # 异步 IO 库
    ├── protobuf/           # protobuf 34.1 源码与构建产物
    └── rapidjson/          # JSON 解析（header-only）
```

---

## 环境准备

已在 **Windows + MSVC 2022** 环境验证，需要：

- **Go** 1.22+
- **CMake** 3.16+
- **MSVC 2022**（C++ 服务）
- **etcd**（可选；未安装时 Registry 可用 `-store memory` 运行）
- **MySQL 8.0 + Redis 7**（仅 Phase 2 测试及完整部署需要）

> 本项目已内置 protobuf 34.1 源码与预编译产物于 `3rd/protobuf/`，无需额外安装 protoc。

---

## 编译

### Windows（推荐）

```bash
# 一键编译全部服务（Go + C++）
./build.bat
```

### Makefile（Go + Proto）

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

### 编译产物

```
bin/
  ├── registry-go.exe
  ├── dbproxy-go.exe
  ├── biz-go.exe
  ├── logstats-go.exe
  ├── gateway-cpp.exe      (从 build/Release/ 复制)
  ├── realtime-cpp.exe     (从 build/Release/ 复制)
  ├── test-crypto.exe
  └── test-async-net.exe
```

---

## 运行

### 快速启动脚本

项目提供 `scripts/` 目录下的快捷脚本，方便开发调试：

```bash
# 启动 Phase 1 最小链路（Registry + Biz + Gateway）
scripts\start-minimal.bat

# 启动 Phase 2 完整链路（Registry + DBProxy + Biz + Gateway）
scripts\start-full.bat

# 停止所有服务
scripts\stop-all.bat

# 单独启动某个服务
scripts\start-registry.bat
scripts\start-biz.bat
scripts\start-dbproxy.bat
scripts\start-gateway.bat
scripts\start-logstats.bat

# 一键编译并运行测试
scripts\run-phase1.bat
scripts\run-phase2.bat
```

PowerShell 用户也可以使用：

```powershell
# 启动指定服务组合（TODO：待实现 PowerShell 版本）
# .\scripts\start-services.ps1 minimal

# 停止指定或全部服务（TODO：待实现 PowerShell 版本）
# .\scripts\stop-services.ps1 all
```

### 手动启动（Phase 1，无需 MySQL/Redis）

```bash
# 1. 启动 Registry（内存模式）
./bin/registry-go.exe -listen 127.0.0.1:2379 -store memory

# 2. 启动 Biz
./bin/biz-go.exe -config conf/biz.json

# 3. 启动 Gateway（需要 conf/gateway.json 配置文件）
./bin/gateway-cpp.exe --config conf/gateway.json
```

### 开发规范：重大改动后强制验证

任何涉及协议、公共库、Gateway/Biz 交互的改动，提交前必须完成：

1. **全量编译通过**：`make build`（或 Windows `build.bat`）
2. **Phase 1 端到端通过**：`go run tests/phase1/main.go`
3. **手动一键验证**：`scripts\start-minimal.bat`（Windows）

> 本项目为 Go + C++ 双语言栈， handshake 帧格式、conn_id 前缀、FlagEncrypt 标志等均为跨语言隐式契约，单侧单元测试无法覆盖，历史多次回归因此产生。

### 一键联调测试

```bash
# Phase 1（验证 Client -> Gateway -> Biz -> Registry 链路）
go run tests/phase1/main.go

# Phase 2（验证登录 -> 读玩家数据 -> 修改 -> 写回，需本地 MySQL + Redis）
go run tests/phase2/main.go
```

### Docker Compose 全服部署

```bash
cd tools/deploy
docker compose up -d
```

完整编排包含：etcd、mysql、redis、registry-go、dbproxy-go、biz-go×2、gateway-cpp、realtime-cpp、logstats-go、prometheus、grafana。

---

## 服务发现配置（Registry / etcd 双方案）

本项目支持两种服务发现后端，业务服务通过统一的 `discovery` 接口调用，底层差异完全封装。

### Go 服务

通过配置文件切换（`conf/biz.json` / `conf/dbproxy.json`）：

```json
{
  "discovery": {
    "type": "registry",
    "addrs": ["127.0.0.1:2379"]
  }
}
```

```bash
./bin/biz-go.exe -config conf/biz.json
```

### C++ 服务

通过 `conf/gateway.json` / `conf/realtime.json` 中的 `discovery` 字段配置：

```json
{
  "discovery": {
    "type": "registry",
    "addrs": ["127.0.0.1:2379"]
  }
}
```

> **注意**：C++ 侧的 etcd 直连依赖 `etcd-cpp-apiv3`，当前在 Windows 上默认关闭（`ENABLE_ETCD_DISCOVERY=OFF`）。Linux/Docker 环境可通过 `-DENABLE_ETCD_DISCOVERY=ON` 启用。

---

## 项目状态

| Phase | 目标 | 状态 |
|-------|------|------|
| Phase 1 | 通信与发现骨架 | ✅ 完成 |
| Phase 2 | 数据与存储基础设施 | ✅ 完成 |
| Phase 3 | 安全与可靠性设施 | 🚧 进行中 |
| Phase 4 | 可观测性设施 | ✅ 已完成子项 |
| Phase 5 | 实时服基础设施 | 🚧 骨架阶段 |
| Phase 6 | 部署与 CI/CD | ✅ 已完成子项 |

详见 [IMPLEMENTATION.md](IMPLEMENTATION.md) 了解各 Phase 详细验收标准。

---

## 关键端口

| 服务 | 业务端口 | Metrics 端口 |
|------|----------|--------------|
| registry-go | 2379 | - |
| dbproxy-go | 3307 | - |
| biz-go | 8082 | 9082 |
| gateway-cpp | 8081 | 9081 |
| realtime-cpp | 8084 | 9090 |
| logstats-go | 8085 | 8086 |
| prometheus | - | 9091 |
| grafana | - | 3000 |

---

## 相关文档

- [DESIGN.md](DESIGN.md) — 完整架构设计文档
- [IMPLEMENTATION.md](IMPLEMENTATION.md) — 落地实施计划与验收标准
- [DESIGN_REVIEW.md](DESIGN_REVIEW.md) — 设计评审记录
- [AGENTS.md](AGENTS.md) — 面向 AI Coding Agent 的项目指南

---

## License

MIT
