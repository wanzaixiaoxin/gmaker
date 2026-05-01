# Config Service (config-go)

集中式配置管理中心，为 gmaker 框架提供配置的版本化存储、发布/回滚、审计日志与实时推送能力。

---

## 职责

- **配置存储**：以版本化方式持久化管理所有配置文件（JSON / TOML / YAML）
- **版本管理**：每次编辑生成新版本草稿，支持一键发布与回滚
- **审计追踪**：完整记录 create / edit / publish / rollback / delete 操作日志
- **实时推送**：通过 Redis Pub/Sub 将配置变更主动推送到订阅该配置的业务服务节点
- **Web 管理**：提供 REST API 供 `web/config-admin/` 可视化后台调用

---

## 架构

```
┌──────────────────┐      HTTP REST API       ┌────────────────────┐
│  web/config-admin│  ──────────────────────▶ │   config-go        │
│  (管理后台)       │                          │   (本服务)          │
└──────────────────┘                          └──────┬─────────────┘
                                                      │
                                     ┌────────────────┼────────────────┐
                                     ▼                ▼                ▼
                                  MySQL          Redis Pub/Sub      Registry
                               (configs,     (pubsub:config:*)    (服务注册)
                                versions,
                                logs)
```

业务服务（biz-go / login-go / gateway-cpp 等）通过 **RedisWatcher** 订阅自己关注的配置频道，收到变更事件后主动调用 `/pull` 拉取新内容并热重载。

---

## 快速启动

### 前置依赖

- MySQL 5.7+（或 MariaDB）
- Redis 5.0+（单节点或 Cluster）
- Registry（用于服务注册与发现，可选）

### 初始化数据库

```sql
-- 已在项目根目录 scripts/init-db.sql 中包含以下建表语句
CREATE TABLE configs (...);
CREATE TABLE config_versions (...);
CREATE TABLE config_logs (...);
CREATE TABLE config_subscribers (...);
```

导入方式：
```bash
mysql -u root -p gmaker < ../../scripts/init-db.sql
```

### 配置文件

参考 `conf/config.json`：

```json
{
  "service": {
    "service_type": "config",
    "node_id": "config-1",
    "log_level": "info",
    "log_file": "",
    "metrics_addr": ":9087"
  },
  "network": {
    "host": "127.0.0.1",
    "port": 8087
  },
  "discovery": {
    "type": "registry",
    "addrs": ["127.0.0.1:2379"]
  },
  "mysql": {
    "dsn": "root:123456@tcp(127.0.0.1:3306)/gmaker?charset=utf8mb4",
    "max_open_conn": 10,
    "max_idle_conn": 2,
    "conn_max_lifetime_sec": 3600
  },
  "redis": {
    "addrs": ["127.0.0.1:6379"],
    "password": "",
    "pool_size": 20
  }
}
```

### 编译与运行

```bash
# 从项目根目录
cd services/config-go
go build -o ../../bin/config-go.exe .

# 运行
../../bin/config-go.exe -config ../../conf/config.json

# 或使用 CLI 覆盖 MySQL/Redis 地址
../../bin/config-go.exe -config ../../conf/config.json \
  -mysql "root:pass@tcp(192.168.1.10:3306)/gmaker?charset=utf8mb4" \
  -redis "192.168.1.10:6379"
```

### 健康检查

```bash
curl http://127.0.0.1:8087/health
# ok
```

---

## HTTP API

### 配置管理

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/api/configs?namespace=default` | 查询配置列表 |
| `POST` | `/api/configs` | 创建新配置（同时生成 v1 草稿） |
| `GET` | `/api/configs/{name}?namespace=default` | 查询配置元数据 + 当前生效内容 |
| `PUT` | `/api/configs/{name}` | 编辑配置（生成新版本草稿） |
| `DELETE` | `/api/configs/{name}?namespace=default` | 删除配置及其全部版本、日志 |
| `POST` | `/api/configs/{name}/publish` | 发布指定版本 |
| `POST` | `/api/configs/{name}/rollback` | 回滚到历史版本（基于旧内容生成新版本） |
| `GET` | `/api/configs/{name}/versions` | 查询版本历史 |
| `GET` | `/api/configs/{name}/logs?limit=100` | 查询操作审计日志 |
| `GET` | `/api/configs/{name}/pull?version_id=xxx` | 拉取配置完整内容（供业务节点调用） |
| `POST` | `/api/configs/{name}/subscribe` | 注册服务订阅关系 |

### 请求示例

**创建配置**
```bash
curl -X POST http://127.0.0.1:8087/api/configs \
  -H "Content-Type: application/json" \
  -H "X-Operator: admin" \
  -d '{
    "name": "feature_flags",
    "namespace": "default",
    "format": "json",
    "description": "功能开关",
    "content": "{\"new_ui\": true,\"pvp_match\": false}"
  }'
```

**发布版本**
```bash
curl -X POST http://127.0.0.1:8087/api/configs/feature_flags/publish \
  -H "Content-Type: application/json" \
  -d '{"namespace":"default","version_id":3}'
```

**回滚版本**
```bash
curl -X POST http://127.0.0.1:8087/api/configs/feature_flags/rollback \
  -H "Content-Type: application/json" \
  -d '{"namespace":"default","version_id":2}'
```

**服务注册订阅**
```bash
curl -X POST http://127.0.0.1:8087/api/configs/feature_flags/subscribe \
  -H "Content-Type: application/json" \
  -d '{"service_type":"biz","subscribe_mode":"exact"}'
```

---

## 数据库表结构

### `configs` — 配置元数据

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | BIGINT PK | 自增主键 |
| `name` | VARCHAR(128) | 配置唯一标识（如 `item_table`） |
| `namespace` | VARCHAR(64) | 命名空间，默认 `default` |
| `format` | VARCHAR(16) | `json` / `toml` / `yaml` |
| `schema_def` | JSON | JSON Schema 校验规则（可选） |
| `description` | VARCHAR(256) | 配置用途说明 |
| `current_version` | BIGINT | 当前生效版本 ID |
| `status` | TINYINT | `0=正常` `1=禁用` |
| `created_at` / `updated_at` | BIGINT | Unix 秒级时间戳 |

### `config_versions` — 版本快照

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | BIGINT PK | 自增主键 |
| `config_id` | BIGINT FK | 关联 `configs.id` |
| `version` | INT | 单调递增版本号（每个 config_id 内独立） |
| `content` | LONGTEXT | 配置内容原文 |
| `checksum` | VARCHAR(64) | SHA-256(content)，用于一致性校验 |
| `status` | TINYINT | `0=草稿` `1=已发布` `2=已回滚` `3=已废弃` |
| `published_at` | BIGINT | 发布时间 |
| `created_by` | VARCHAR(64) | 操作人 |
| `created_at` | BIGINT | 创建时间 |

### `config_logs` — 审计日志

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | BIGINT PK | 自增主键 |
| `config_id` | BIGINT | 关联配置 |
| `version_id` | BIGINT | 关联版本（如有） |
| `action` | VARCHAR(32) | `create` `edit` `publish` `rollback` `delete` |
| `operator` | VARCHAR(64) | 操作人账号 |
| `detail` | JSON | 变更摘要、diff 信息等 |
| `ip` | VARCHAR(64) | 操作来源 IP |
| `created_at` | BIGINT | 操作时间 |

### `config_subscribers` — 服务订阅关系

| 字段 | 类型 | 说明 |
|------|------|------|
| `config_name` | VARCHAR(128) | 配置名称 |
| `service_type` | VARCHAR(64) | 关注该配置的服务类型（如 `biz`、`gateway`） |
| `subscribe_mode` | VARCHAR(32) | `exact` `prefix` `glob` |

---

## 配置推送流程

```
1. 管理员在 Web 后台点击「发布」
        │
        ▼
2. config-go 更新数据库（current_version → 新版本）
        │
        ▼
3. config-go 通过 Redis 发布事件
   Channel: pubsub:config:default:feature_flags
   Payload: ConfigChangeEvent (protobuf)
        │
        ▼
4. 业务服务的 RedisWatcher 收到事件
        │
        ▼
5. 业务服务调用 /pull API 拉取完整内容
        │
        ▼
6. 校验 checksum → 覆盖写入本地文件 → 调用 Reload()
```

> **设计要点**：业务服务收到的是"变更通知"而非完整内容，主动拉取确保即使 Redis 消息丢失或消费端离线，也能通过重连后补推机制恢复。

---

## 业务服务接入指南（Go）

### 1. 配置文件增加 `config_service` 段

以 `conf/biz.json` 为例：

```json
{
  "config_service": {
    "addr": "http://127.0.0.1:8087",
    "watch_list": ["feature_flags", "rate_limits"]
  }
}
```

### 2. 启动时初始化 RedisWatcher

```go
import (
    "github.com/gmaker/luffa/common/go/config"
    configpb "github.com/gmaker/luffa/gen/go/config"
)

// 在 main() 中，Redis 连接成功后：
watcher := config.NewRedisWatcher(redisClient.RawClient(), "default")
watcher.SetLogger(log)

loader := config.NewLoader(*configFile)
_ = loader.Load()

for _, name := range cfg.ConfigService.WatchList {
    watcher.Subscribe(name, func(ev *configpb.ConfigChangeEvent) {
        if err := config.PullAndReload(cfg.ConfigService.Addr, ev.ConfigName, ev, loader, *configFile); err != nil {
            log.Errorf("config reload failed: %v", err)
        }
    })
}
go watcher.Start(context.Background())
```

### 3. 使用带鉴权的 Reload 端点（可选）

```go
loader := config.NewLoaderWithAuth(*configFile, os.Getenv("ADMIN_TOKEN"))
loader.ServeReloadHTTP(":9090")
```

外部调用时需携带 Token：
```bash
curl -X POST http://127.0.0.1:9090/admin/reload \
  -H "Authorization: Bearer your-secret-token"
```

---

## 业务服务接入指南（C++）

> C++ SDK 待实现，规划如下：

1. `common/cpp/config/config_watcher.hpp` — 基于 hiredis 的 Pub/Sub 订阅封装
2. 后台线程阻塞监听 Redis 消息
3. 收到 `ConfigChangeEvent` 后，通过 `PostAsync` 投递到主线程
4. 使用 curl / libuv HTTP 客户端调用 `/pull` API 拉取内容
5. 写入本地文件并触发 `Loader::Reload()`

Gateway / Realtime 启动时示例：

```cpp
auto watcher = std::make_unique<config::RedisWatcher>(redisContext, "default");
watcher->Subscribe("gateway_policy", [](const config::ConfigChangeEvent& ev) {
    config::PullAndReload("http://127.0.0.1:8087", ev, loader, configPath);
});
watcher->Start();
```

---

## Web 管理后台

位于 `web/config-admin/`，纯原生 HTML/JS 实现。

### 打开方式

1. **VS Code Live Server**：右键 `web/config-admin/index.html` → "Open with Live Server"
2. **直接打开**：浏览器访问 `file:///.../web/config-admin/index.html`（API 需允许跨域，已默认开启 CORS）
3. **内嵌到现有网关**：将 `web/config-admin/` 部署到任意静态文件服务器

### 功能概览

- **配置列表**：搜索、新建、编辑、删除
- **配置编辑**：语法高亮编辑器（原生 textarea，可扩展为 Monaco/CodeMirror）、保存草稿、直接发布
- **版本历史**：右侧展示全部版本，点击切换内容、一键发布草稿、一键回滚已发布版本
- **操作日志**：全量审计记录，支持按操作人/配置名过滤

---

## 目录结构

```
services/config-go/
├── main.go                      # 服务入口：HTTP 服务 + MySQL/Redis/Registry 初始化
├── internal/
│   ├── handler/
│   │   └── handler.go           # HTTP API 路由与业务逻辑
│   └── store/
│       └── store.go             # MySQL 数据访问层（Config / Version / Log / Subscriber）
└── README.md                    # 本文档
```

---

## 与其他服务的关系

| 交互方 | 协议 | 用途 |
|--------|------|------|
| **MySQL** | `database/sql` + `go-sql-driver/mysql` | 配置元数据、版本、审计日志持久化 |
| **Redis** | `go-redis/v9` | Pub/Sub 配置变更事件推送 |
| **Registry** | TCP + protobuf3（通过 `common/go/discovery`） | 服务注册与发现 |
| **业务服务** | HTTP REST（业务服务作为客户端） | 配置拉取 `/pull`、订阅注册 `/subscribe` |
| **Web 后台** | HTTP REST + CORS | 可视化配置管理 |

---

## 设计决策

### 为什么用 Redis Pub/Sub 而不是 Etcd Watch？

- 项目已全面接入 Redis（Go/C++ 均有客户端），无需新增依赖
- `etcd.go` 骨架中 `EtcdClient` 尚未实现，补齐成本更高
- Redis 频道天然契合"服务器订阅（监控）对应配置"的语义
- 发布/订阅轻量解耦，config-go 无需维护到每个业务节点的长连接

### 为什么推送事件后还要业务服务主动拉取？

- **可靠性**：Redis Pub/Sub 不保证消费者离线时消息不丢失，主动拉取是兜底
- **校验**：拉取后比对 checksum，确保配置内容在传输过程中未被篡改
- **解耦**：事件消息体很小（仅版本号 + checksum），不携带完整配置内容

---

##  Roadmap

- [x] 配置 CRUD + 版本管理 + 发布/回滚
- [x] 审计日志
- [x] Redis Pub/Sub 推送
- [x] Go 侧 RedisWatcher SDK
- [x] Web 管理后台（基础版本）
- [x] `/admin/reload` Bearer Token 鉴权
- [ ] C++ 侧 RedisWatcher SDK
- [ ] Web 端版本内容 diff 对比
- [ ] JSON Schema 校验（前端 + 后端）
- [ ] 灰度发布（按 region / node_id / percent 推送）
