# 集中式配置管理中心设计方案

> 版本：v1.0  
> 适用场景：gmaker 框架全服务的动态配置热更新与业务数值表管理  
> 核心目标：**版本可控、变更可审计、推送可感知、回滚可秒级完成**

---

## 1. 背景与问题

gmaker 框架当前采用本地 JSON 配置文件 + HTTP `/admin/reload` 热重载的 MVP 方案，在业务规模扩大后暴露出以下问题：

| 问题 | 影响 |
|------|------|
| 配置文件分散在各服务节点 | 运维需要 SSH 到每台机器修改，易遗漏、易出错 |
| 无版本管理 | 改错配置后无法快速回滚，只能依赖运维手动备份 |
| 无变更审计 | 谁改了什么、什么时候改的、改之前是什么，完全不可追溯 |
| 无主动推送 | 依赖运维手动调用 `/admin/reload`，配置生效延迟不可控 |
| 无灰度能力 | 无法先在小范围节点验证配置再全量推送 |
| `/admin/reload` 无鉴权 | 任何人可 POST 触发配置变更，存在安全漏洞 |

本方案在**不破坏现有本地配置文件兜底机制**的前提下，引入独立的 **Config Service（config-go）** 作为配置的唯一可信源，实现集中化管理、版本化存储、实时推送与完整审计。

---

## 2. 设计目标

| 目标 | 说明 |
|------|------|
| **集中管理** | 所有配置统一托管到 Config Service，Web 后台一站式编辑 |
| **版本化** | 每次保存生成版本快照，支持一键发布与回滚 |
| **审计追踪** | 完整记录操作人、操作时间、变更内容、IP 地址 |
| **实时推送** | 配置发布后秒级推送到订阅该配置的业务服务节点 |
| **与现有框架兼容** | 保留本地 JSON 文件作为启动兜底，Config Watcher 拉取新配置后覆盖写入本地并触发 Reload |
| **双语言支持** | Go 与 C++ 服务均可接入，复用现有 Redis 基础设施 |

---

## 3. 总体架构

```
                              Web 配置管理后台
                        (web/config-admin/index.html)
                              | HTTP REST API
                              v
                    +-------------------------+
                    |   Config Service        |
                    |   (config-go)           |
                    |  +-------+ +--------+   |
                    |  |Config | |Version |   |
                    |  |API    | |Manager |   |
                    |  +---+---+ +----+---+   |
                    |      |          |       |
                    |  +---+----------+---+   |
                    |  | Audit Log | Push   |   |
                    |  +-----------+Dispatcher|
                    +----+--------+---------+
                         |        |
           +-------------+        +------------+
           v                           v
        MySQL                         Redis
     (configs,                    Pub/Sub
      versions,                    pubsub:cfg:*
      logs)
                                       |
           +---------------------------+---------------------------+
           |                           |                           |
           v                           v                           v
      +---------+                +---------+                +-----------+
      | biz-go  |                |login-go |                |gateway-cpp|
      |Watcher  |                |Watcher  |                |Watcher    |
      +---------+                +---------+                +-----------+
```

**核心设计决策**：采用 **"Config Service + MySQL 持久化 + Redis Pub/Sub 推送"** 架构。

- **Config Service**：独立 HTTP 服务，负责配置的 CRUD、版本管理、审计日志、推送分发
- **MySQL**：持久化存储配置元数据、版本快照、操作日志
- **Redis Pub/Sub**：轻量级配置变更通知通道，业务服务订阅对应频道
- **业务服务 SDK**：`RedisWatcher` 监听 Redis 频道，收到事件后主动拉取配置并热重载

---

## 4. 为什么选 Redis Pub/Sub？

| 维度 | Redis Pub/Sub | Etcd Watch | Registry+RPC直连 |
|------|--------------|-----------|-----------------|
| 现有依赖 |  Redis 已全面接入 |  EtcdClient 未实现 |  需维护大量连接 |
| C++ 支持 |  hiredis 已集成 |  需新增 C++ Etcd 客户端 |  需新增 RPC 推送逻辑 |
| 语义匹配 |  "监控配置" = 频道订阅 |  Watch 前缀 |  需点对点遍历 |
| 实现成本 | 低 | 中（补齐 EtcdClient） | 中 |
| 可靠性 |  离线丢消息（接受：可补推） |  持久化 |  请求-响应确认 |

**结论**：Redis Pub/Sub 与项目现有基础设施最匹配，实现成本最低，且天然契合"服务器监控（订阅）对应配置"的语义。对于离线期间丢失的消息，业务服务启动时会主动拉取当前生效版本作为兜底。

---

## 5. 数据模型

### 5.1 ER 关系

```
+-----------+        +---------------+        +-----------+
|  configs  | 1 ---> N | config_versions| 1 ---> N | config_logs|
+-----------+        +---------------+        +-----------+
       |
       | 1 ---> N
       v
+---------------------+
| config_subscribers  |
+---------------------+
```

### 5.2 表结构

#### configs — 配置元数据

```sql
CREATE TABLE configs (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    name            VARCHAR(128) NOT NULL COMMENT '配置唯一标识',
    namespace       VARCHAR(64)  NOT NULL DEFAULT 'default',
    format          VARCHAR(16)  NOT NULL DEFAULT 'json',
    schema_def      JSON         NULL COMMENT 'JSON Schema 校验规则',
    description     VARCHAR(256) NULL,
    current_version BIGINT       NOT NULL DEFAULT 0 COMMENT '当前生效版本ID',
    status          TINYINT      NOT NULL DEFAULT 0 COMMENT '0=正常,1=禁用',
    created_at      BIGINT       NOT NULL DEFAULT 0,
    updated_at      BIGINT       NOT NULL DEFAULT 0,
    UNIQUE KEY uk_name_ns (name, namespace)
);
```

#### config_versions — 版本快照

每次"保存草稿"或"发布"都会生成一条版本记录，内容原文 + SHA-256 校验和确保完整性。

```sql
CREATE TABLE config_versions (
    id           BIGINT AUTO_INCREMENT PRIMARY KEY,
    config_id    BIGINT       NOT NULL COMMENT '关联 configs.id',
    version      INT          NOT NULL DEFAULT 1 COMMENT '单调递增版本号',
    content      LONGTEXT     NOT NULL COMMENT '配置内容原文',
    checksum     VARCHAR(64)  NOT NULL COMMENT 'SHA-256(content)',
    status       TINYINT      NOT NULL DEFAULT 0 COMMENT '0=草稿,1=已发布,2=已回滚,3=已废弃',
    published_at BIGINT       NULL,
    created_by   VARCHAR(64)  NOT NULL,
    created_at   BIGINT       NOT NULL DEFAULT 0,
    UNIQUE KEY uk_config_ver (config_id, version)
);
```

#### config_logs — 审计日志

```sql
CREATE TABLE config_logs (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    config_id   BIGINT       NOT NULL,
    version_id  BIGINT       NULL,
    action      VARCHAR(32)  NOT NULL COMMENT 'create|edit|publish|rollback|delete',
    operator    VARCHAR(64)  NOT NULL,
    detail      JSON         NULL COMMENT '变更摘要、diff等',
    ip          VARCHAR(64)  NULL,
    created_at  BIGINT       NOT NULL DEFAULT 0
);
```

#### config_subscribers — 服务订阅关系

记录哪些服务类型关注哪些配置，用于推送时过滤目标（未来扩展）。

```sql
CREATE TABLE config_subscribers (
    id             BIGINT AUTO_INCREMENT PRIMARY KEY,
    config_name    VARCHAR(128) NOT NULL,
    service_type   VARCHAR(64)  NOT NULL,
    subscribe_mode VARCHAR(32)  NOT NULL DEFAULT 'exact',
    created_at     BIGINT       NOT NULL DEFAULT 0,
    UNIQUE KEY uk_sub (config_name, service_type)
);
```

---

## 6. 协议设计

### 6.1 Protobuf 消息

新增 `spec/proto/config.proto`：

```protobuf
syntax = "proto3";
package config;

// 配置变更推送事件（Redis Pub/Sub 频道 payload）
message ConfigChangeEvent {
    string config_name = 1;
    string namespace   = 2;
    int64  version_id  = 3;
    int32  version_no  = 4;
    string checksum    = 5;
    string action      = 6;     // publish | rollback
    int64  timestamp   = 7;
}

// 配置内容拉取响应
message ConfigPullRes {
    bool   ok         = 1;
    string content    = 2;
    string checksum   = 3;
    int64  version_id = 4;
    int32  version_no = 5;
}
```

### 6.2 Redis 频道命名

- `pubsub:config:{namespace}:{config_name}` — 精确配置变更频道
- `pubsub:config:{namespace}:all` — 全量广播（用于强制刷新）

### 6.3 HTTP API

| 方法 | 路径 | 说明 |
|------|------|------|
| `GET` | `/api/configs?namespace=` | 配置列表 |
| `POST` | `/api/configs` | 创建配置（同时生成 v1 草稿） |
| `GET` | `/api/configs/{name}` | 查询配置元数据 + 当前生效内容 |
| `PUT` | `/api/configs/{name}` | 编辑配置（生成新版本草稿） |
| `DELETE` | `/api/configs/{name}` | 删除配置及其全部版本 |
| `POST` | `/api/configs/{name}/publish` | 发布指定版本 |
| `POST` | `/api/configs/{name}/rollback` | 回滚到历史版本 |
| `GET` | `/api/configs/{name}/versions` | 版本历史 |
| `GET` | `/api/configs/{name}/logs` | 审计日志 |
| `GET` | `/api/configs/{name}/pull` | 服务节点主动拉取 |
| `POST` | `/api/configs/{name}/subscribe` | 注册订阅关系 |

---

## 7. 核心流程

### 7.1 创建到发布到推送到重载（正向流程）

```
[管理员] 在 Web 后台编辑配置内容
    |
    v
[Web] POST /api/configs/item_table
    body: { name, namespace, format, content }
    |
    v
[Config Service]
  1. 插入 configs 表（元数据）
  2. 插入 config_versions 表（v1 草稿）
  3. 记录 config_logs（action=create）
    |
    v
[管理员] 点击「发布」
    |
    v
[Web] POST /api/configs/item_table/publish
    body: { namespace, version_id }
    |
    v
[Config Service]
  1. 校验版本存在且为草稿
  2. 更新 configs.current_version = v1.id
  3. 更新 config_versions.status = 1（已发布）
  4. 记录 config_logs（action=publish）
  5. 组装 ConfigChangeEvent
  6. Redis.Publish("pubsub:config:default:item_table", event)
    |
    v
[业务服务 RedisWatcher]
  收到 ConfigChangeEvent { version_id=1, checksum=abc123 }
    |
    v
[业务服务]
  1. HTTP GET /pull?version_id=1
  2. 校验返回内容的 SHA-256 == abc123
  3. 原子覆盖写入本地 conf/biz.json
  4. 调用 loader.Reload()
  5. 配置生效
```

### 7.2 回滚流程

```
[管理员] 在 Web 后台查看版本历史
    |
    v
[管理员] 选择 v2 点击「回滚」
    |
    v
[Web] POST /api/configs/item_table/rollback
    body: { namespace, version_id: 2 }
    |
    v
[Config Service]
  1. 查询 v2 的内容和 checksum
  2. 将 v2 标记为 status=2（已回滚）
  3. 新建 v3（内容 = v2 的内容，status=1 已发布）
  4. 更新 configs.current_version = v3.id
  5. 记录 config_logs（action=rollback, detail={from:2, to:3}）
  6. Redis.Publish("pubsub:config:default:item_table", event)
    |
    v
[业务服务]
  拉取 v3 -> 校验 -> 写入本地 -> Reload
```

**回滚的设计要点**：回滚不是把 current_version 指针指回旧版本，而是**基于旧版本内容创建一个新版本**。这样做的好处：
- 版本号始终单调递增，不会出现"回退"导致的心理困惑
- 审计日志完整保留"回滚了哪个版本->生成了哪个新版本"
- 未来如果回滚错了，还可以"回滚回滚"

### 7.3 服务启动时的配置同步

业务服务启动时：
1. 先读取本地 JSON 配置文件（启动兜底）
2. 初始化 `RedisWatcher`，订阅自己关注的配置频道
3. 可选：启动后立即向 Config Service `/pull` 拉取最新版本，覆盖本地文件
4. 进入正常运行，等待 Redis 推送事件

---

## 8. SDK 设计

### 8.1 Go SDK（common/go/config/watcher.go）

```go
// 创建 Watcher
watcher := config.NewRedisWatcher(redisClient.RawClient(), "default")
watcher.SetLogger(log)

// 声明关注的配置
watcher.Subscribe("feature_flags", func(ev *configpb.ConfigChangeEvent) {
    if err := config.PullAndReload(
        "http://127.0.0.1:8087",  // Config Service 地址
        ev.ConfigName,
        ev,
        loader,                   // 本地配置加载器
        "conf/biz.json",          // 本地文件路径（会被覆盖）
    ); err != nil {
        log.Errorf("reload failed: %v", err)
    }
})

// 启动后台监听
go watcher.Start(ctx)
```

**关键设计**：
- `PullAndReload` 是同步阻塞的（含 HTTP 拉取 + 文件写入 + Reload），建议在 handler 中异步执行
- 文件写入采用"写临时文件 + rename"的原子操作，避免服务读取到半成品配置
- 如果拉取或校验失败，本地配置保持原状，不会触发 Reload

### 8.2 C++ SDK（已实现）

**头文件**：`common/cpp/config/config_watcher.hpp`

```cpp
#include "config/config_watcher.hpp"

// 创建 Watcher
auto watcher = std::make_unique<gs::config::RedisWatcher>(redisContext, "default");
watcher->SetConfigServiceAddr("http://127.0.0.1:8087");
watcher->SetLocalLoader(loader, "conf/gateway.json");
watcher->SetNodeInfo("cn", "gateway-1", {{"env", "prod"}});

// 订阅配置变更
watcher->Subscribe("gateway_policy", [](const gs::config::ConfigChangeEvent& ev) {
    // 自定义处理，或留空由 SetLocalLoader 自动触发 PullAndReload
});

// 启动后台监听线程
watcher->Start();
```

**实现要点**：
- 基于 hiredis 的阻塞订阅模式，独立后台线程监听 Redis 消息
- 使用 WinHTTP 客户端（Windows）调用 `/pull` API 拉取配置内容
- 文件写入采用".tmp + rename"原子操作
- 自动触发 `Loader::Reload()`
- 预留 `ShouldAcceptGray` 灰度匹配接口

---

## 9. Web 配置管理后台

位于 `web/config-admin/`，纯原生 HTML/JS 实现，零外部框架依赖。

### 9.1 页面结构

- **配置列表页**：表格展示全部配置，显示当前版本、状态、最近修改时间，支持搜索
- **配置编辑页**：
  - 左侧：配置内容编辑器（原生 textarea，预留 Monaco/CodeMirror 升级接口）
  - 右侧：版本历史列表，点击切换内容、发布草稿、回滚已发布版本、**对比当前版本差异**
  - 底部：保存草稿 / 直接发布
- **操作日志页**：全量审计记录，支持按配置名/操作人过滤

### 9.3 Diff 对比功能

在版本历史列表中点击 **"对比当前"**，弹出 diff 弹窗：
- 绿色行：新增内容
- 红色行：删除内容
- 左侧显示行号，支持行级差异定位

Diff 算法为原生 JS 实现的贪心行级 diff，在后续 8 行范围内查找最佳匹配。

### 9.2 打开方式

1. VS Code Live Server 右键打开
2. 部署到任意静态文件服务器（Nginx / Caddy / Gateway 静态资源目录）
3. 直接 `file://` 打开（API 已默认开启 CORS）

---

## 10. 安全设计

### 10.1 HTTP API 鉴权

当前阶段：通过请求头 `X-Operator` 传递操作人身份，后续可接入 Login 服务的 Token 验证。

```
X-Operator: admin
```

### 10.2 /admin/reload 鉴权

`common/go/config/loader.go` 的 `ServeReloadHTTP` 已支持 Bearer Token 鉴权：

```go
loader := config.NewLoaderWithAuth("conf/biz.json", os.Getenv("ADMIN_TOKEN"))
loader.ServeReloadHTTP(":9090")
```

调用方式：
```bash
curl -X POST http://127.0.0.1:9090/admin/reload \
  -H "Authorization: Bearer your-secret-token"
```

### 10.3 Checksum 校验

配置内容在存储和传输过程中使用 SHA-256 校验和：
- 版本创建时计算 `SHA-256(content)` 存入 `config_versions.checksum`
- 推送事件携带 checksum
- 业务服务拉取后重新计算并比对，不一致则拒绝重载

### 10.4 原子文件写入

`PullAndReload` 写入本地文件时采用先写 `.tmp` 再 `rename` 的方式，确保服务不会读取到半写入的配置文件。

---

## 11. 持久化存储分层（MySQL + Redis）

配置内容采用**双写**策略：

| 存储层 | 职责 | Key/Table |
|--------|------|-----------|
| **MySQL** | 配置元数据、完整版本历史、审计日志（关系型查询友好） | `configs`、`config_versions`、`config_logs` |
| **Redis** | 当前生效版本缓存、指定版本缓存、Pub/Sub 推送通道 | `config:current:{ns}:{name}`、`config:version:{ns}:{name}:{ver_id}`、`pubsub:config:{ns}:{name}` |

**设计要点**：
- 发布/回滚时，Config Service **先写 MySQL 事务，再写 Redis 缓存**，最后推送 Pub/Sub 事件
- `/pull` API **优先读 Redis**，命中直接返回（延迟 < 1ms）；Redis miss 时回源 MySQL
- Redis 零 TTL 长期保留，MySQL 是最终可信源，Redis 作为高性能读缓存

---

## 12. 灰度发布设计

### 12.1 灰度规则

业务服务在启动时通过 `SetNodeInfo(region, node_id, tags)` 注册自身身份：

```go
watcher.SetNodeInfo("cn", "biz-01", map[string]string{"env": "prod", "zone": "a"})
```

Config Service 发布时支持以下灰度参数：

| 参数 | 类型 | 说明 |
|------|------|------|
| `gray_region` | string | 目标 region，如 `"cn"`，空表示不限 |
| `gray_node_id` | string | 目标 node_id，如 `"biz-01"`，空表示不限 |
| `gray_percent` | int32 | 百分比灰度 0-100，0 表示全量 |
| `gray_tags` | map<string,string> | 标签匹配，如 `{"env": "prod"}` |

### 12.2 灰度匹配算法

```
if gray_region 不为空且 != 本节点 region → 拒绝
if gray_node_id 不为空且 != 本节点 node_id → 拒绝
if gray_tags 存在不匹配的标签 → 拒绝
if gray_percent > 0 且 gray_percent < 100:
    hash = node_id 字符串 hash 取模
    if hash % 100 >= gray_percent → 拒绝
→ 接受
```

百分比灰度使用 `node_id` 哈希取模，确保**同一节点始终得到一致结果**（不会出现同一节点一会儿收到一会儿收不到）。

### 12.3 发布示例

```bash
# 仅推送给 region=cn、env=prod 的 20% 节点
curl -X POST http://127.0.0.1:8087/api/configs/feature_flags/publish \
  -H "Content-Type: application/json" \
  -d '{
    "namespace": "default",
    "version_id": 3,
    "gray_region": "cn",
    "gray_percent": 20,
    "gray_tags": {"env": "prod"}
  }'
```

---

## 13. JSON Schema 校验

### 13.1 后端校验

`services/config-go/internal/handler/schema.go` 实现轻量级 JSON Schema 校验器：

- `type`：string / number / integer / boolean / array / object
- `required`：必填字段数组
- `properties`：对象字段递归校验
- `enum`：枚举值限制
- `minimum` / `maximum`：数值范围
- `minLength` / `maxLength`：字符串长度

在**创建配置**和**编辑配置**时自动触发，校验失败返回 `400 Bad Request`。

### 13.2 前端校验

Web 管理后台在保存前执行 `JSON.parse(content)` 格式检查，格式错误立即拦截并提示具体错误位置。

### 13.3 配置示例

```json
{
  "name": "feature_flags",
  "format": "json",
  "schema_def": "{\"type\":\"object\",\"required\":[\"new_ui\"],\"properties\":{\"new_ui\":{\"type\":\"boolean\"},\"rate_limit\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":10000}}}"
}
```

---

## 14. 与现有框架的兼容策略

| 现有机制 | 处理方式 |
|---------|---------|
| `conf/*.json` 本地文件 | 保留作为启动兜底。Config Watcher 拉取新配置后**覆盖写入**本地文件，然后触发 `Reload()` |
| `POST /admin/reload` | 保留并增加 Bearer Token 鉴权。Config Watcher 的 `Reload()` 仍复用此逻辑 |
| `common/go/config/etcd.go` | 保留不做删除。未来如需迁移到 Etcd，可让 `EtcdLoader` 与 `RedisWatcher` 共存，启动参数选择 |
| 无配置文件的服务（registry-go、logstats-go、realtime-cpp） | 在 Config Center 中注册订阅，逐步将硬编码参数迁移为可配置项 |

---

## 15. 配置分层模型

参考 DESIGN.md 的 L0~L3 分层，本方案优先支持 L1/L2：

| 层级 | 用途 | 示例 | 更新频率 | 本方案支持度 |
|------|------|------|----------|-------------|
| **L0 系统配置** | 服务启动参数、网络地址 | `listen_addr`、`etcd_endpoints` | 极低，需重启 |  不建议热更新（重启生效） |
| **L1 动态配置** | 运行时可热重载的参数 | `log_level`、`rate_limit_qps`、`feature_flags` | 低，分钟级 |  完全支持 |
| **L2 业务数值表** | 道具、关卡、掉落、技能数值 | `item_table`、`level_exp` | 中，运营活动时 |  完全支持 |
| **L3 紧急补丁** | 热修复开关、黑名单、公告 | `ban_list`、`emergency_notice` | 高，秒级 |  完全支持 |

---

## 16. 演进路线

| 阶段 | 目标 | 状态 |
|------|------|------|
| **Phase 1** | Config Service 骨架（HTTP API + MySQL + Redis 推送） |  已完成 |
| **Phase 2** | Go SDK（RedisWatcher + PullAndReload）+ biz-go 接入示例 |  已完成 |
| **Phase 3** | Web 管理后台（配置列表、编辑器、版本历史、日志） |  已完成 |
| **Phase 4** | `/admin/reload` Bearer Token 鉴权 + CORS |  已完成 |
| **Phase 5** | C++ SDK（hiredis Pub/Sub + HTTP 拉取） |  已完成 |
| **Phase 6** | Web 端版本内容 diff 对比 |  已完成 |
| **Phase 7** | JSON Schema 校验（前端 + 后端双重校验） |  已完成 |
| **Phase 8** | 灰度发布（按 region / node_id / percent 推送） |  已完成 |
| **Phase 9** | 配置内容加密存储（敏感配置如密钥） |  远期规划 |

---

## 17. 相关文档与代码

| 资源 | 路径 |
|------|------|
| 服务入口 | `services/config-go/main.go` |
| HTTP Handler | `services/config-go/internal/handler/handler.go` |
| Schema 校验 | `services/config-go/internal/handler/schema.go` |
| 数据存储层 | `services/config-go/internal/store/store.go` |
| Go SDK | `common/go/config/watcher.go` |
| C++ SDK | `common/cpp/config/config_watcher.hpp` / `.cpp` |
| C++ HTTP 客户端 | `common/cpp/config/http_client.hpp` / `.cpp` |
| 本地加载器（含鉴权） | `common/go/config/loader.go` |
| Protobuf 定义 | `spec/proto/config.proto` |
| Web 管理后台 | `web/config-admin/` |
| 服务 README | `services/config-go/README.md` |
| 数据库脚本 | `scripts/init-db.sql` |
| 服务配置文件 | `conf/config.json` |

---

## 18. 附录：术语表

| 术语 | 说明 |
|------|------|
| **Config Service** | 配置中心服务（`services/config-go`），配置的唯一可信源 |
| **RedisWatcher** | 业务服务中的配置变更监听器，订阅 Redis Pub/Sub 频道 |
| **PullAndReload** | 收到变更事件后，主动拉取配置内容 -> 校验 -> 写本地文件 -> 触发重载的完整流程 |
| **草稿 (Draft)** | 已保存但尚未发布的版本，业务服务不会收到推送 |
| **发布 (Publish)** | 将某个草稿版本标记为当前生效版本，触发 Redis 推送 |
| **回滚 (Rollback)** | 基于某个历史版本的内容创建一个新版本并发布，用于快速恢复 |
| **Checksum** | SHA-256 摘要，用于校验配置内容在传输和存储过程中未被篡改 |
