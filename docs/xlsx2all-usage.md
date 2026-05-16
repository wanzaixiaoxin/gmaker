# xlsx2all — Excel 表定义驱动代码生成工具

## 概述

`xlsx2all` 是一个代码生成工具，核心思路：**一张 Excel 表 = 一张数据库表**。

维护 Excel 即可自动生成：
- **SQL DDL** — 建表语句（`CREATE TABLE`）
- **Protobuf .proto** — 消息定义
- **Go 结构体** — 通过 protoc 编译生成

## 目录结构

```
gmaker/
├── tables/                    # 输入：Excel 表定义（每张表一个 .xlsx）
│   ├── accounts.xlsx
│   ├── player_profiles.xlsx
│   └── ...
├── sql/                       # 输出：生成的 SQL 文件
│   ├── all.sql                # 合并的完整 SQL
│   ├── accounts.sql           # 单表 SQL
│   └── ...
├── spec/proto/                # 输出：生成的 Proto 文件
│   ├── accounts.proto
│   └── ...
├── gen/go/                    # 输出：protoc 编译的 Go 代码
│   ├── accounts/
│   │   └── accounts.pb.go
│   └── ...
├── tools/xlsx2all/            # 工具源码
│   ├── main.go
│   └── go.mod
└── scripts/
    └── gen-tables.bat         # 一键构建脚本
```

## Excel 格式规范

### 文件命名

文件名即表名，例如 `player_profiles.xlsx` → 表名 `player_profiles`。

### Sheet 结构

第一行为**表头**，后续行为字段定义：

| A | B | C | D | E | F | G | H | I | J |
|---|---|---|---|---|---|---|---|---|---|
| **字段名** | **数据类型** | **长度** | **无符号** | **默认值** | **非空** | **主键** | **自增** | **Proto类型** | **注释** |
| player_id | bigint | 20 | Y | | Y | Y | | uint64 | 玩家ID |
| nickname | varchar | 64 | | | Y | | | string | 昵称 |
| level | int | | | 1 | | | | int32 | 等级 |

### 列说明

| 列 | 必填 | 说明 | 示例 |
|---|---|---|---|
| 字段名 | ✅ | 数据库列名 | `player_id`, `nickname` |
| 数据类型 | ✅ | MySQL 数据类型 | `bigint`, `varchar`, `int`, `tinyint`, `json` |
| 长度 | ❌ | 类型长度/精度 | `20`, `64`, `128`（留空则不指定） |
| 无符号 | ❌ | 是否 UNSIGNED | `Y` / 留空 |
| 默认值 | ❌ | 列默认值 | `0`, `1`, `default`, 留空 |
| 非空 | ❌ | 是否 NOT NULL | `Y` / 留空 |
| 主键 | ❌ | 是否主键 | `Y` / 留空 |
| 自增 | ❌ | 是否 AUTO_INCREMENT | `Y` / 留空 |
| Proto类型 | ❌ | Protobuf 字段类型，留空自动推导 | `uint64`, `string`, `int32` |
| 注释 | ❌ | 列注释 | `玩家ID`, `0=正常 1=禁用` |

### 元数据行

在表头行之前，可以添加 `#` 开头的元数据行：

| 元数据 | 格式 | 默认值 |
|---|---|---|
| 表注释 | `#表注释: 玩家基础信息表` | 无 |
| 引擎 | `#引擎: InnoDB` | `InnoDB` |
| 字符集 | `#字符集: utf8mb4` | `utf8mb4` |

### MySQL → Proto 类型自动映射

| MySQL 类型 | Protobuf 类型 |
|---|---|
| `bool`, `boolean` | `bool` |
| `tinyint` | `int32` (unsigned: `uint32`) |
| `smallint`, `mediumint` | `int32` (unsigned: `uint32`) |
| `int`, `integer` | `int32` (unsigned: `uint32`) |
| `bigint` | `int64` (unsigned: `uint64`) |
| `float` | `float` |
| `double` | `double` |
| `decimal` | `string`（避免精度丢失） |
| `varchar`, `char`, `text` 系列 | `string` |
| `blob`, `binary` 系列 | `bytes` |
| `date`, `datetime`, `timestamp` | `int64`（unix 时间戳） |
| `json`, `enum`, `set` | `string` |

## 使用方式

### 一键完整流程

```bash
scripts\gen-tables.bat
```

执行三步操作：
1. 构建 `xlsx2all` 工具
2. 读取 `tables/*.xlsx` → 生成 SQL + Proto
3. 编译 Proto → Go 代码

### 创建新表模板

```bash
scripts\gen-tables.bat --init my_new_table
```

在 `tables/` 目录下创建 `my_new_table.xlsx`，包含示例行。

### 创建示例 Excel

```bash
scripts\gen-tables.bat --demo
```

为项目已有的 5 张表创建 Excel 定义文件：`accounts`, `player_profiles`, `chat_rooms`, `bot_accounts`, `configs`。

### 只生成 SQL + Proto，跳过 protoc

```bash
scripts\gen-tables.bat --skip-proto
```

### 直接使用工具

```bash
# 处理所有表
go run ./tools/xlsx2all --dir tables --sql-out sql --proto-out spec/proto

# 处理单张表
go run ./tools/xlsx2all --dir tables --table player_profiles

# 指定 Go module 和数据库名
go run ./tools/xlsx2all --dir tables --module github.com/gmaker/luffa --db gmaker
```

### 命令行参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| `--dir` | `tables` | Excel 输入目录 |
| `--sql-out` | `sql` | SQL 输出目录 |
| `--proto-out` | `spec/proto` | Proto 输出目录 |
| `--table` | (全部) | 只处理指定表 |
| `--module` | `github.com/gmaker/luffa` | Go module 路径 |
| `--db` | `gmaker` | 数据库名（用于合并 SQL） |
| `--init <name>` | — | 创建模板 Excel |
| `--demo` | — | 创建示例 Excel |

## 生成示例

以 `player_profiles.xlsx` 为例：

### 输入 Excel

| 字段名 | 数据类型 | 长度 | 无符号 | 默认值 | 非空 | 主键 | 自增 | Proto类型 | 注释 |
|---|---|---|---|---|---|---|---|---|---|
| player_id | bigint | 20 | Y | | Y | Y | | uint64 | 玩家ID |
| nickname | varchar | 64 | | | Y | | | string | 昵称 |
| level | int | | | 1 | | | | int32 | 等级 |
| exp | bigint | | | 0 | | | | int64 | 经验值 |
| coin | bigint | | | 0 | | | | int64 | 金币 |

### 生成 SQL (`sql/player_profiles.sql`)

```sql
-- player_profiles (用户业务资料表)
CREATE TABLE IF NOT EXISTS player_profiles (
    player_id bigint(20) UNSIGNED NOT NULL COMMENT '玩家ID',
    nickname varchar(64) NOT NULL COMMENT '昵称',
    level int DEFAULT 1 COMMENT '等级',
    exp bigint DEFAULT 0 COMMENT '经验值',
    coin bigint DEFAULT 0 COMMENT '金币',
    PRIMARY KEY (player_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='用户业务资料表';
```

### 生成 Proto (`spec/proto/player_profiles.proto`)

```protobuf
syntax = "proto3";

package player_profiles;
option go_package = "github.com/gmaker/luffa/gen/go/player_profiles";

// player_profiles — 用户业务资料表
message PlayerProfiles {
    uint64     player_id = 1; // 玩家ID
    string     nickname  = 2; // 昵称
    int32      level     = 3; // 等级
    int64      exp       = 4; // 经验值
    int64      coin      = 5; // 金币
}
```

### 生成 Go (`gen/go/player_profiles/player_profiles.pb.go`)

```go
type PlayerProfiles struct {
    PlayerId uint64 `protobuf:"varint,1,opt,name=player_id,json=playerId,proto3" json:"player_id,omitempty"` // 玩家ID
    Nickname string `protobuf:"bytes,2,opt,name=nickname,proto3" json:"nickname,omitempty"`                  // 昵称
    Level    int32  `protobuf:"varint,3,opt,name=level,proto3" json:"level,omitempty"`                       // 等级
    Exp      int64  `protobuf:"varint,4,opt,name=exp,proto3" json:"exp,omitempty"`                           // 经验值
    Coin     int64  `protobuf:"varint,5,opt,name=coin,proto3" json:"coin,omitempty"`                         // 金币
}
```

## 工作流程

```
┌──────────────────────────────────────────────────────────┐
│  tables/*.xlsx                                           │
│  ┌──────────┐ ┌──────────────────┐ ┌───────────────┐    │
│  │accounts  │ │player_profiles   │ │chat_rooms     │    │
│  │  .xlsx   │ │    .xlsx         │ │  .xlsx        │    │
│  └────┬─────┘ └───────┬──────────┘ └──────┬────────┘    │
│       │               │                    │              │
└───────┼───────────────┼────────────────────┼──────────────┘
        │               │                    │
        ▼               ▼                    ▼
   ┌─────────────────────────────────────────────────┐
   │            xlsx2all 工具                         │
   │  解析 Excel → 生成 SQL + Proto                   │
   └──────┬──────────────────────┬───────────────────┘
          │                      │
          ▼                      ▼
   ┌─────────────┐      ┌──────────────────┐
   │ sql/*.sql   │      │ spec/proto/*.proto│
   │ 建表语句     │      │ 消息定义          │
   └─────────────┘      └───────┬──────────┘
                                 │
                                 ▼
                        ┌──────────────────┐
                        │    protoc 编译    │
                        └───────┬──────────┘
                                │
                                ▼
                        ┌──────────────────┐
                        │ gen/go/**/*.pb.go │
                        │ Go 结构体代码      │
                        └──────────────────┘
```

## 注意事项

1. **Proto 类型可手动指定**：如果自动推导的类型不符合需求，在 `Proto类型` 列手动填写
2. **合并 SQL**：工具会额外生成 `sql/all.sql`，包含所有表的 `CREATE DATABASE` + `CREATE TABLE`
3. **文件覆盖**：每次运行会覆盖之前生成的文件，请勿手动修改生成的文件
4. **新增表**：只需在 `tables/` 放入新的 `.xlsx` 文件，重新运行即可
5. **修改表结构**：直接修改对应 Excel，重新运行即可更新 SQL 和 Proto
