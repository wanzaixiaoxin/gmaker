# 限量抢购系统设计方案

> 版本：v1.0  
> 适用场景：高并发限量商品抢购（库存远小于请求量）  
> 设计原则：**用户反馈必须确定，防刷必须无感，系统必须可水平扩展**

---

## 1. 设计目标

| 目标 | 说明 |
|------|------|
| **确定性反馈** | 用户看到的任何结果（成功/失败/排队）都对应数据库的确定状态，禁止出现"系统繁忙请重试" |
| **无感防刷** | 正常用户全程无验证码、无滑块；可疑流量由后端风控拦截，验证码仅作为最后惩罚手段 |
| **不超卖** | 库存预占原子化，支付时乐观锁兜底 |
| **水平扩展** | 各层无状态，支持按需扩容 |
| **端到端延迟** | 用户从点击到看到结果 < 500ms（P99） |

---

## 2. 核心设计原则

### 2.1 层层过滤，绝不全量打到数据库

```
100万请求 → CDN/Gateway 限流挡掉 50% → 风控/Token 校验挡掉 40% → Redis 预占挡掉 9.9% → 只有约 1000 个请求写数据库
```

### 2.2 同步落骨架，异步填血肉

- **同步**：Redis 预占库存 + 写订单主表（极简字段）→ 用户立即拿到确定结果
- **异步**：订单详情、库存明细扣减、通知 → MQ 慢速消费，用户无感知

**库存原则：下单只预占，支付成功才正式扣减数据库库存。**

### 2.3 三种确定态，无中间态

| 用户看到 | 数据库状态 | 是否允许 |
|----------|-----------|---------|
| "抢购成功，订单号 xxx" | 订单主表已存在 | ✅ 必须 |
| "已售罄" / "已限购" | 库存为 0 或用户已购买 | ✅ 允许 |
| "排队中，预计 N 秒" | 排队队列中，有确定排队号 | ✅ 允许（兜底） |
| "系统繁忙，请重试" | 不确定 | ❌ 禁止 |

---

## 3. 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                         客户端层                             │
│  • 静态资源 CDN 缓存                                          │
│  • 前端防抖（点击后按钮置灰 3 秒）                              │
│  • 本地倒计时，减少无效刷新                                   │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                        接入网关层                            │
│  • WAF / DDoS 清洗                                            │
│  • 全局限流（令牌桶，保护后端总量）                             │
│  • IP / 设备指纹限流                                          │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                        抢购服务层                            │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ 风控/防刷中心 │  │ 库存预占核心 │  │ 排队/结果查询服务    │  │
│  │ • Token 校验 │  │ • Redis Lua │  │ • 队列状态轮询      │  │
│  │ • 行为评分   │  │ • 原子扣减  │  │ • 结果推送(SSE)     │  │
│  │ • 渐进验证   │  │ • 预占释放  │  │                     │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                        数据层                                │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐  │
│  │ Redis Cluster│  │ MySQL/TiDB  │  │ 消息队列(Kafka)      │  │
│  │ • 库存缓存   │  │ • 订单主表   │  │ • 订单详情填充       │  │
│  │ • 用户限购   │  │ • 支付流水   │  │ • 库存明细扣减       │  │
│  │ • 排队队列   │  │ • 风控日志   │  │ • 延迟通知           │  │
│  └─────────────┘  └─────────────┘  └─────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. 核心流程

### 4.1 下单流程（秒杀）

```
用户浏览器                    Gateway               抢购服务              Redis              MySQL
    │                            │                      │                  │                  │
    │── 1. 加载商品页 ───────────►│                      │                  │                  │
    │◄─ 2. 返回 HTML+Token+签名 ──│                      │                  │                  │
    │                            │                      │                  │                  │
    │── 3. 点击"立即抢购" ───────►│                      │                  │                  │
    │   (携带 Token+签名+行为数据) │                      │                  │                  │
    │                            │                      │                  │                  │
    │                            │── 4. 限流检查 ───────►│                  │                  │
    │                            │◄─ 5. 通过/拒绝 ──────│                  │                  │
    │                            │                      │                  │                  │
    │                            │── 6. Token+风控校验 ─►│                  │                  │
    │                            │◄─ 7. 可信/可疑/拒绝 ─│                  │                  │
    │                            │                      │                  │                  │
    │                            │── 8. Redis 预占库存 ─►│── 9. Lua 原子预占─►│                  │
    │                            │                      │                  │                  │
    │                            │◄─ 10. 预占结果 ──────│◄─ 成功/失败/限购 ─│                  │
    │                            │                      │                  │                  │
    │                            │── 11. 同步写订单 ────►│                  │── 12. 写入订单主表─►│
    │                            │                      │                  │                  │
    │                            │── 13. 发 MQ 异步任务 ─►│                  │                  │
    │                            │                      │                  │                  │
    │◄─ 14. 返回确定结果 ─────────│                      │                  │                  │
    │                            │                      │                  │                  │
    │                            │                      │                  │                  │
    │  成功：{"code":"SUCCESS","order_id":"xxx"}           │                  │                  │
    │  失败：{"code":"SOLD_OUT"} 或 {"code":"LIMIT_REACHED"}│                  │                  │
    │  排队：{"code":"QUEUING","queue_token":"xxx"}        │                  │                  │
```

### 4.2 异步填单流程

```
Kafka Consumer
    │
    ▼
┌─────────────────┐
│ 1. 幂等检查      │  查 Redis：token 是否已处理
│    (Token 去重)  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 2. 填充订单详情  │  商品快照、价格、图片
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 3. 扣减明细库存  │  仓库库存、SKU 库存
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 4. 发送通知      │  短信、推送、站内信
└─────────────────┘
```

### 4.3 订单超时释放流程

```
定时任务（每 30 秒）
    │
    ▼
扫描 seckill_orders 表中 status=0 且 expire_time < NOW()
    │
    ▼
┌─────────────────┐
│ 1. 关闭订单      │  UPDATE status = 2（已取消）
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 2. 释放 Redis 预占│  DECR preoccupy + SRem users
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ 3. 释放Token    │  DEL token + 允许再次抢购
└─────────────────┘
```

---

### 4.4 支付流程（与下单独立）

```
用户进入"我的订单"或订单详情页
    │
    ▼
点击"去支付"
    │
    ▼
后端：校验订单（存在、待支付、未过期）
    │
    ▼
检查是否已有"支付中"流水
    │
    ├── 有且未过期 ──► 直接返回已有流水支付参数
    │
    └── 无 ──► 创建支付流水（status=待支付）
                  │
                  ▼
            调用支付网关预下单
                  │
                  ▼
            更新流水 status = 支付中
                  │
                  ▼
            返回前端支付参数（调起微信/支付宝 SDK）
                  │
                  ▼
            用户完成支付/取消/关闭
                  │
                  ▼
            支付网关异步回调 / 定时主动查询
                  │
                  ▼
            幂等处理：
                1. 数据库乐观锁正式扣减库存
                2. 更新流水 = 成功
                3. 更新订单 = 已支付
                4. Redis 同步扣减总库存 + 释放预占
                5. 发送发货 MQ
```

---

## 5. 详细设计

### 5.1 防刷设计（无感优先）

#### 5.1.1 第一层：请求合法性校验（让脚本调不了接口）

**动态签名 Token**

页面加载时，后端生成一次性 Token 嵌入 HTML：

```html
<script>
window.__seckillConfig = {
    itemId: 10086,
    token: "tk_abc123",
    timestamp: 1745833200,
    nonce: "x7y9z2",
    sign: "HMAC-SHA256(user_id + item_id + timestamp + nonce + secret)"
};
</script>
```

抢购请求必须携带完整 Token，服务端校验：
- Token 是否有效
- 签名是否正确
- 时间戳是否在有效窗口（如 5 分钟）
- Token 是否已被使用（Redis Set 去重）

**隐藏蜜罐字段**

```html
<input type="text" name="nickname" style="position:absolute;left:-9999px" tabindex="-1" autocomplete="off">
```

- 人类用户看不到，不会填写
- 脚本按字段名自动填充 → 直接拒绝

**JS 挑战（轻量级环境校验）**

```javascript
// 页面内嵌，加载时静默执行
const challenge = hash(deviceInfo + screen.width + colorDepth + timezone + token);
// 抢购时携带 challenge，服务端校验
```

- 简单 HTTP 客户端（curl、Python requests）无法通过
- 只有真实浏览器能生成正确 challenge

#### 5.1.2 第二层：业务层无优势（让脚本发了也白发）

**预约抽签制（推荐用于超限量商品）**

```
开抢前 24~72 小时：开放预约
    │
    ▼
用户点击"预约"（仅记录意向，不扣库存）
    │
    ▼
系统综合评分：
    score = random() * 0.3          // 随机性
          + account_age * 0.2       // 账号龄
          + purchase_history * 0.2  // 购买信誉
          + device_trust * 0.2      // 设备可信度
          + real_name * 0.1         // 实名认证
    │
    ▼
开抢前 1 小时公布中签结果
    │
    ├── 未中签 → 按钮灰色"未中签"，明确结果
    └── 中签   → 开抢时直接进入支付页，无需抢购
```

- 脚本可以批量预约，但中签率由信誉分决定
- 正常老用户中签率远高于批量注册的小号

**排队制（适用于需要抢购氛围的场景）**

```
用户点击抢购
    │
    ▼
Redis 库存充足？
    │
    ├── 否 → 返回"已售罄"
    │
    └── 是 → 发放排队号，进入有序队列
              │
              ▼
        前端显示："正在排队，前面还有 42 人，预计 3 秒"
              │
              ▼
        服务端按 FIFO 顺序处理，每秒处理 N 个
              │
              ▼
        轮到该用户时：Redis 预占库存 → 写订单 → 推送结果
```

- 脚本的"毫秒级速度"优势被排队抹平
- 用户有明确进度感知，不会焦虑

#### 5.1.3 第三层：后端风控（正常用户永远无感知）

**行为评分模型**

```
输入维度：
  • 鼠标轨迹：移动速度、曲率、随机性
  • 点击模式：坐标分布、间隔时间、是否有移动过程
  • 键盘输入：按键间隔熵值
  • 设备指纹：Canvas、WebGL、字体、时区、语言
  • 网络特征：IP 归属地、延迟抖动、TCP 指纹

输出：
  score >= 80  → 直接放行
  score 50~79  → 延迟 1~2 秒处理（候补队列）
  score < 50   → 拒绝，或弹出验证码（仅脚本走到这）
```

**多维度限流**

| 维度 | 阈值 | 目的 |
|------|------|------|
| 用户级 | 5 秒 1 次 | 防止单账号狂点 |
| 设备级 | 3 秒 1 次 | 防止模拟器多开 |
| IP 级 | 每秒 10 次 | 防止同 WiFi 下多人被误伤 |
| 全局限流 | 系统容量上限 | 保护后端不被打垮 |

#### 5.1.4 渐进式验证（最后防线）

```
用户请求
    │
    ▼
风控评分
    │
    ├── 高分（>=80）──────► 直接放行，无任何验证
    │
    ├── 中分（50~79）─────► 进入排队，延迟处理
    │
    └── 低分（<50）───────► 拒绝 或 弹出验证码（极罕见）
```

**原则：验证码不是默认流程，而是对可疑流量的惩罚。**

---

### 5.2 库存管理

#### 5.2.1 Redis 库存预占（Lua 原子脚本）

**核心原则：下单只预占，不改数据库库存。**

```lua
-- KEYS[1]: preoccupy:item:{item_id}      预占计数
-- KEYS[2]: preoccupy:item:{item_id}:users 已预占用户集合
-- ARGV[1]: user_id
-- ARGV[2]: token
-- ARGV[3]: item_id（用于查总库存）

-- 1. 检查用户是否已预占（限购）
local bought = redis.call('sismember', KEYS[2], ARGV[1])
if bought == 1 then
    return {-2, ""}  -- 已限购
end

-- 2. 检查可用库存（总库存 - 预占数）
local total_stock = tonumber(redis.call('get', 'stock:item:' .. ARGV[3]) or 0)
local preoccupy = tonumber(redis.call('get', KEYS[1]) or 0)
local available = total_stock - preoccupy

if total_stock == 0 then
    return {-1, ""}  -- 库存未初始化
end
if available <= 0 then
    return {0, ""}   -- 无可预占库存（已售罄）
end

-- 3. 原子预占 + 记录用户
redis.call('incr', KEYS[1])
redis.call('sadd', KEYS[2], ARGV[1])

-- 4. 生成并保存预占 Token（15 分钟有效期）
redis.call('setex', 'token:' .. ARGV[2], 900, ARGV[3])
redis.call('hset', 'token:meta:' .. ARGV[2],
    'user_id', ARGV[1],
    'item_id', ARGV[3],
    'create_time', redis.call('time')[1]
)

return {1, ARGV[2]}  -- 成功，返回 token
```

**热点预占拆分（可选，单 Key QPS > 10万时使用）**

```
库存 10000 件，预占拆分为 100 个槽：
  preoccupy:item:10086:00 ~ preoccupy:item:10086:99

预占时：slot = hash(user_id) % 100
查询总预占数：sum(所有槽)
总可用 = stock - sum(preoccupy)
```

#### 5.2.2 数据库正式扣减（支付时执行）

下单阶段**不碰数据库库存**。支付成功时，通过乐观锁正式扣减：

```sql
-- 支付回调成功后执行
UPDATE item_stock 
SET stock = stock - 1, sold = sold + 1
WHERE item_id = ? AND stock > 0;

-- 返回 RowsAffected，如果为 0 说明库存刚好耗尽，触发退款/取消
```

#### 5.2.3 预占释放（超时/取消时）

用户超时未支付或主动取消时，**只释放 Redis 预占，数据库库存从未扣减，无需回滚**：

```lua
-- 释放预占
redis.call('decr', 'preoccupy:item:' .. item_id)
redis.call('srem', 'preoccupy:item:' .. item_id .. ':users', user_id)
redis.call('del', 'token:' .. token)
redis.call('del', 'token:meta:' .. token)
```

---

### 5.3 订单创建设计

#### 5.3.1 同步写订单主表（用户确定性来源）

**订单主表（极致精简）**

```sql
CREATE TABLE `seckill_orders` (
  `order_id`      BIGINT UNSIGNED NOT NULL COMMENT '雪花ID',
  `user_id`       BIGINT UNSIGNED NOT NULL,
  `item_id`       INT UNSIGNED NOT NULL,
  `sku_id`        INT UNSIGNED NOT NULL,
  `quantity`      TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `status`        TINYINT NOT NULL DEFAULT 0 COMMENT '0:待支付 1:已支付 2:已取消 3:支付中 4:已发货',
  `token`         CHAR(32) NOT NULL,
  `expire_time`   DATETIME NOT NULL COMMENT '支付截止时间',
  `create_time`   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `update_time`   DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  
  PRIMARY KEY (`order_id`),
  UNIQUE KEY `uk_token` (`token`),
  KEY `idx_user_status` (`user_id`, `status`, `create_time`),
  KEY `idx_expire` (`status`, `expire_time`)
) ENGINE=InnoDB COMMENT='秒杀订单主表';
```

**设计要点：**
- 单行 < 100 字节，纯写入性能极高
- 雪花 ID 预分配，无数据库自增锁
- 按 `user_id` 分 128 张表，或直接使用 TiDB/OceanBase
- 只存骨架信息，详情走异步填充

**同步写代码**

```go
func CreateOrderSync(userID, itemID, skuID int64, token string) (*Order, error) {
    orderID := snowflake.Generate()
    expireTime := time.Now().Add(15 * time.Minute)
    
    _, err := db.Exec(`
        INSERT INTO seckill_orders 
        (order_id, user_id, item_id, sku_id, token, status, expire_time) 
        VALUES (?, ?, ?, ?, ?, 0, ?)
    `, orderID, userID, itemID, skuID, token, expireTime)
    
    if err != nil {
        return nil, err
    }
    
    return &Order{
        OrderID: orderID,
        Token: token,
        ExpireTime: expireTime,
    }, nil
}
```

#### 5.3.2 异步填充订单详情

```go
func ProcessOrderDetail(msg OrderMessage) {
    // 1. 幂等检查
    if redis.Get("order:processed:" + msg.Token) != "" {
        return
    }
    
    // 2. 查商品快照
    snapshot := itemService.GetSnapshot(msg.ItemID)
    
    // 3. 填充详情
    db.Exec(`
        INSERT INTO order_detail 
        (order_id, item_name, item_pic, price, total_amount) 
        VALUES (?, ?, ?, ?, ?)
    `, msg.OrderID, snapshot.Name, snapshot.Pic, snapshot.Price, snapshot.Price * msg.Quantity)
    
    // 4. 扣减明细库存
    result, _ := db.Exec(`
        UPDATE warehouse_stock 
        SET stock = stock - ? 
        WHERE sku_id = ? AND stock >= ?
    `, msg.Quantity, msg.SkuID, msg.Quantity)
    
    if result.RowsAffected == 0 {
        // 库存不足，记录异常，人工介入或自动退款
        log.Error("warehouse stock insufficient", msg)
    }
    
    // 5. 标记已处理
    redis.Set("order:processed:"+msg.Token, "1", 24*time.Hour)
}
```

---

### 5.4 用户体验设计

#### 5.4.1 状态机与前端反馈

```
用户点击"立即抢购"
    │
    ▼
┌──────────────────────────────────────────┐
│ 前端立即置灰按钮，显示"处理中..."         │
│ 同时启动 5 秒超时计时器                   │
└──────────────────────────────────────────┘
    │
    ├── 200ms 内收到响应 ──► 按响应码展示结果
    │
    └── 超过 5 秒 ──► 自动查询结果接口，最多查 3 次
                      仍无结果 → 显示"网络异常，请查看我的订单"
```

#### 5.4.2 响应码与前端行为

| 响应码 | 用户看到 | 前端行为 | 数据库状态 |
|--------|----------|----------|-----------|
| `SUCCESS` | "抢购成功！订单号 xxx，15分钟内支付" | 跳转支付页或弹窗 | 订单已存在 |
| `SOLD_OUT` | "已售罄，试试其他商品吧" | 按钮变灰"已售罄" | 库存为 0 |
| `LIMIT_REACHED` | "每人限购 1 件，您已参与过" | 按钮变灰 | 用户已购买 |
| `QUEUING` | "正在排队，前面还有 N 人" | 显示进度条，自动轮询 | 排队队列中 |
| `RISK_REJECT` | "活动太火爆了，再试一次" | 按钮恢复可点击 | 无订单 |

**禁止出现的响应：**
- ❌ "系统繁忙，请稍后重试"
- ❌ "处理中，请等待"
- ❌ 空白页或转圈超过 3 秒无反馈

#### 5.4.3 排队状态轮询

```go
func QueryQueueStatus(w http.ResponseWriter, r *http.Request) {
    queueToken := r.URL.Query().Get("qtk")
    
    // 1. 查是否已有结果
    result := redis.Get("queue:result:" + queueToken)
    if result != "" {
        // 已处理完成，返回最终结果
        orderID := result // 如果 result 是 order_id
        if result == "SOLD_OUT" {
            returnJSON(w, Response{Code: "SOLD_OUT"})
        } else {
            returnJSON(w, Response{Code: "SUCCESS", OrderID: result})
        }
        return
    }
    
    // 2. 查排队位置
    rank := redis.ZRank("queue:item:"+itemID, queueToken)
    if rank < 0 {
        returnJSON(w, Response{Code: "SOLD_OUT"}) // 已被踢出队列（库存耗尽）
        return
    }
    
    // 3. 预估时间（假设每秒处理 500 人）
    estimated := rank / 500
    returnJSON(w, Response{
        Code: "QUEUING",
        Position: rank,
        EstimatedSeconds: estimated,
    })
}
```

---

### 5.5 订单状态机

订单与支付状态分离，避免互相覆盖：

```
                    用户点击支付
                           │
                           ▼
┌──────────┐      ┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│  已取消   │◄─────│   待支付      │─────►│   支付中      │─────►│   已支付      │
│ (status=2)│      │  (status=0)  │      │  (status=3)  │      │  (status=1)  │
└──────────┘      └──────────────┘      └──────┬───────┘      └──────┬───────┘
      ▲                                        │                     │
      │                                        │                     ▼
      │                                        │              ┌──────────────┐
      │                                        │              │   已发货      │
      │                                        │              │  (status=4)  │
      │                                        │              └──────────────┘
      │                                        │
      │         支付超时 / 用户取消             │         支付回调失败
      └────────────────────────────────────────┘◄─────────────────────────────┘

状态流转规则：
  • 待支付 → 支付中：用户发起支付，创建支付流水
  • 支付中 → 已支付：支付网关回调成功
  • 支付中 → 待支付：回调失败/超时，用户可重新发起
  • 待支付 → 已取消：订单超时 15 分钟未支付
  • 已支付 → 已发货：MQ 异步消费发货
```

**状态隔离原则：**
- `status=0(待支付)`：可发起支付，可超时取消
- `status=3(支付中)`：不可重复发起支付，不可取消（需等支付结果）
- `status=1(已支付)`：不可再次支付，进入发货流程

---

### 5.6 支付设计（独立流程）

#### 5.6.1 支付核心原则

| 原则 | 说明 |
|------|------|
| **幂等发起** | 同一订单同一时刻只能有一笔"支付中"流水，防止用户重复点击 |
| **幂等回调** | 支付网关可能重复回调，按 `third_party_id` 去重 |
| **主动补偿** | 回调可能丢失，定时任务主动查询支付网关补状态 |
| **防重复支付** | 已支付订单拒绝任何新支付请求 |

#### 5.6.2 支付流水表

```sql
CREATE TABLE `payment_flow` (
  `flow_id`        BIGINT UNSIGNED NOT NULL COMMENT '雪花ID',
  `order_id`       BIGINT UNSIGNED NOT NULL,
  `user_id`        BIGINT UNSIGNED NOT NULL,
  `channel`        TINYINT NOT NULL COMMENT '1:微信 2:支付宝 3:银行卡',
  `amount`         INT UNSIGNED NOT NULL COMMENT '支付金额（分）',
  `status`         TINYINT NOT NULL DEFAULT 0 COMMENT '0:待支付 1:支付中 2:成功 3:失败 4:关闭',
  `third_party_id` VARCHAR(64) COMMENT '第三方支付流水号',
  `pay_params`     TEXT COMMENT '调起支付的参数（JSON）',
  `create_time`    DATETIME DEFAULT CURRENT_TIMESTAMP,
  `expire_time`    DATETIME NOT NULL COMMENT '流水有效期',
  `pay_time`       DATETIME COMMENT '实际支付时间',
  `notify_time`    DATETIME COMMENT '收到回调时间',
  `notify_raw`     TEXT COMMENT '回调原始报文',
  
  PRIMARY KEY (`flow_id`),
  UNIQUE KEY `uk_order_channel` (`order_id`, `channel`),
  KEY `idx_third_party` (`third_party_id`),
  KEY `idx_status_expire` (`status`, `expire_time`)
) ENGINE=InnoDB COMMENT='支付流水表';
```

#### 5.6.3 支付发起（幂等）

```go
func CreatePayment(userID, orderID int64, channel int) (*PaymentFlow, error) {
    // 1. 校验订单
    order := GetOrder(orderID)
    if order.Status != ORDER_STATUS_PENDING {
        return nil, errors.New("订单状态不允许支付")
    }
    if order.ExpireTime.Before(time.Now()) {
        return nil, errors.New("订单已过期")
    }
    
    // 2. 检查是否已有支付中流水（幂等：防止重复发起）
    var existing PaymentFlow
    err := db.QueryRow(`
        SELECT flow_id, status, expire_time, pay_params 
        FROM payment_flow 
        WHERE order_id = ? AND channel = ?
    `, orderID, channel).Scan(&existing.FlowID, &existing.Status, &existing.ExpireTime, &existing.PayParams)
    
    if err == nil {
        // 已存在该渠道的流水
        if existing.Status == PAY_STATUS_SUCCESS {
            return nil, errors.New("订单已支付，无需重复支付")
        }
        if existing.Status == PAY_STATUS_PROCESSING && existing.ExpireTime.After(time.Now()) {
            // 支付中且未过期，直接返回已有参数让用户继续
            return &existing, nil
        }
        // 其他状态（失败/关闭/过期）：继续创建新流水
    }
    
    // 3. 创建新流水
    flow := &PaymentFlow{
        FlowID:     snowflake.Generate(),
        OrderID:    orderID,
        UserID:     userID,
        Channel:    channel,
        Amount:     order.TotalAmount,
        Status:     PAY_STATUS_PENDING,
        ExpireTime: time.Now().Add(30 * time.Minute), // 流水有效期 30 分钟
    }
    
    // 4. 调用支付网关预下单
    prePayResult, err := paymentGateway.PreCreate(channel, orderID, flow.Amount)
    if err != nil {
        return nil, err
    }
    
    flow.ThirdPartyID = prePayResult.TradeNo
    flow.PayParams = prePayResult.PayParams
    flow.Status = PAY_STATUS_PROCESSING
    
    // 5. 更新订单为"支付中"（乐观锁）
    result, _ := db.Exec(`
        UPDATE seckill_orders 
        SET status = 3 
        WHERE order_id = ? AND status = 0
    `, orderID)
    
    if result.RowsAffected == 0 {
        return nil, errors.New("订单状态已变更，请刷新页面")
    }
    
    // 6. 保存流水
    db.Exec(`
        INSERT INTO payment_flow 
        (flow_id, order_id, user_id, channel, amount, status, third_party_id, pay_params, expire_time)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON DUPLICATE KEY UPDATE
            flow_id = VALUES(flow_id),
            status = VALUES(status),
            third_party_id = VALUES(third_party_id),
            pay_params = VALUES(pay_params),
            expire_time = VALUES(expire_time)
    `, flow.FlowID, flow.OrderID, flow.UserID, flow.Channel, flow.Amount,
       flow.Status, flow.ThirdPartyID, flow.PayParams, flow.ExpireTime)
    
    return flow, nil
}
```

#### 5.6.4 支付回调处理（幂等 + 防并发）

```go
func HandlePaymentCallback(notify PaymentNotify) error {
    // 1. 签名校验（防伪造）
    if !verifySign(notify) {
        return errors.New("签名验证失败")
    }
    
    // 2. 查流水（按第三方流水号幂等）
    var flow PaymentFlow
    err := db.QueryRow(`
        SELECT flow_id, order_id, status, amount 
        FROM payment_flow 
        WHERE third_party_id = ?
    `, notify.TradeNo).Scan(&flow.FlowID, &flow.OrderID, &flow.Status, &flow.Amount)
    
    if err != nil {
        return errors.New("流水不存在")
    }
    
    // 3. 金额校验
    if flow.Amount != notify.Amount {
        log.Error("支付金额不一致", "expect", flow.Amount, "actual", notify.Amount)
        return errors.New("金额不一致")
    }
    
    // 4. 幂等：已处理过直接返回成功
    if flow.Status == PAY_STATUS_SUCCESS {
        return nil // 已支付，直接返回 SUCCESS 给支付网关
    }
    
    // 5. 数据库事务：扣库存 + 更新流水 + 更新订单（原子性）
    tx, _ := db.Begin()
    defer tx.Rollback()
    
    // 5.1 正式扣减库存（乐观锁兜底）
    stockResult, _ := tx.Exec(`
        UPDATE item_stock 
        SET stock = stock - 1, sold = sold + 1
        WHERE item_id = ? AND stock > 0
    `, order.ItemID)
    
    if stockResult.RowsAffected == 0 {
        tx.Rollback()
        // 库存刚好耗尽，关闭订单并退款
        CloseOrderAndRefund(flow.OrderID, flow.FlowID)
        return errors.New("库存不足，已自动退款")
    }
    
    // 5.2 更新流水
    _, err = tx.Exec(`
        UPDATE payment_flow 
        SET status = 2, pay_time = ?, notify_time = ?, notify_raw = ?
        WHERE flow_id = ? AND status != 2
    `, notify.PayTime, time.Now(), notify.RawBody, flow.FlowID)
    
    if err != nil {
        tx.Rollback()
        return err
    }
    
    // 5.3 更新订单（乐观锁：必须是支付中或待支付）
    result, _ := tx.Exec(`
        UPDATE seckill_orders 
        SET status = 1, pay_time = ?
        WHERE order_id = ? AND status IN (0, 3)
    `, notify.PayTime, flow.OrderID)
    
    if result.RowsAffected == 0 {
        tx.Rollback()
        return errors.New("订单状态已变更")
    }
    
    tx.Commit()
    
    // 6. 同步 Redis：总库存减1，预占数减1
    redis.Decr("stock:item:" + order.ItemID)
    redis.Decr("preoccupy:item:" + order.ItemID)
    
    // 7. 异步发送发货 MQ
    mq.Publish("order_paid", OrderPaidMessage{
        OrderID: flow.OrderID,
        PayTime: notify.PayTime,
    })
    
    return nil
}
```

#### 5.6.5 主动查询补偿（防回调丢失）

```go
func PaymentQueryCompensate() {
    for {
        // 查询"支付中"超过 3 分钟未收到回调的流水
        rows, _ := db.Query(`
            SELECT flow_id, order_id, third_party_id, channel
            FROM payment_flow
            WHERE status = 1 AND create_time < NOW() - INTERVAL 3 MINUTE
            LIMIT 100
        `)
        
        for rows.Next() {
            var flow PaymentFlow
            rows.Scan(&flow.FlowID, &flow.OrderID, &flow.ThirdPartyID, &flow.Channel)
            
            // 主动查询支付网关
            result := paymentGateway.Query(flow.Channel, flow.ThirdPartyID)
            
            if result.Status == "SUCCESS" {
                // 回调丢失，补偿处理
                HandlePaymentCallback(PaymentNotify{
                    TradeNo:  flow.ThirdPartyID,
                    Amount:   flow.Amount,
                    PayTime:  result.PayTime,
                    RawBody:  result.Raw,
                })
            } else if result.Status == "CLOSED" || flow.ExpireTime.Before(time.Now()) {
                // 支付已关闭或流水过期，回滚订单状态
                db.Exec("UPDATE payment_flow SET status = 4 WHERE flow_id = ?", flow.FlowID)
                db.Exec("UPDATE seckill_orders SET status = 0 WHERE order_id = ? AND status = 3", flow.OrderID)
            }
        }
        
        time.Sleep(60 * time.Second)
    }
}
```

#### 5.6.6 订单超时释放定时任务

```go
func ReleaseExpiredOrders() {
    for {
        // 查询已过期且未支付的订单
        rows, _ := db.Query(`
            SELECT order_id, user_id, item_id, token 
            FROM seckill_orders 
            WHERE status = 0 AND expire_time < NOW() 
            LIMIT 100
        `)
        
        for rows.Next() {
            var o Order
            rows.Scan(&o.OrderID, &o.UserID, &o.ItemID, &o.Token)
            
            // 1. 关闭订单
            db.Exec("UPDATE seckill_orders SET status = 2 WHERE order_id = ? AND status = 0", o.OrderID)
            
            // 2. 关闭关联的待支付流水（如果有）
            db.Exec(`
                UPDATE payment_flow SET status = 4 
                WHERE order_id = ? AND status IN (0, 1)
            `, o.OrderID)
            
            // 3. 释放 Redis 预占（数据库库存从未扣减，无需回滚）
            luaReleasePreoccupy(o.ItemID, o.UserID, o.Token)
            
            log.Info("order expired, preoccupy released", "order", o.OrderID)
        }
        
        time.Sleep(30 * time.Second)
    }
}
```

---

## 6. 数据模型

### 6.1 Redis Key 设计

| Key | 类型 | 说明 | TTL |
|-----|------|------|-----|
| `stock:item:{item_id}` | String | 总库存（支付成功时减1） | 活动结束 + 1小时 |
| `preoccupy:item:{item_id}` | String | 当前预占数量（下单+1，超时/支付-1） | 活动结束 + 1小时 |
| `preoccupy:item:{item_id}:users` | Set | 已预占用户ID（去重/限购） | 活动结束 + 1小时 |
| `token:{token}` | String | Token 有效性标记 | 15 分钟 |
| `token:meta:{token}` | Hash | Token 元数据（user_id, item_id, time） | 15 分钟 |
| `token:used:{token}` | String | Token 是否已消费 | 24 小时 |
| `queue:item:{item_id}` | SortedSet | 排队队列（score=时间戳, member=queue_token） | 10 分钟 |
| `queue:result:{qtoken}` | String | 排队结果（order_id 或 SOLD_OUT） | 10 分钟 |
| `seckill:token:seq` | String | Token 序号生成器 | 永久 |

### 6.2 MySQL 表结构

**seckill_orders（订单主表）**

见 5.3.1，此处省略。

**order_detail（订单详情表）**

```sql
CREATE TABLE `order_detail` (
  `order_id`       BIGINT UNSIGNED NOT NULL PRIMARY KEY,
  `item_name`      VARCHAR(255) NOT NULL,
  `item_pic`       VARCHAR(500),
  `price`          INT UNSIGNED NOT NULL COMMENT '单价（分）',
  `total_amount`   INT UNSIGNED NOT NULL COMMENT '总价（分）',
  `receiver_name`  VARCHAR(50),
  `receiver_phone` VARCHAR(20),
  `receiver_addr`  VARCHAR(500),
  `create_time`    DATETIME DEFAULT CURRENT_TIMESTAMP,
  
  KEY `idx_order` (`order_id`)
) ENGINE=InnoDB;
```

**payment_flow（支付流水表）**

```sql
CREATE TABLE `payment_flow` (
  `flow_id`        BIGINT UNSIGNED NOT NULL COMMENT '雪花ID',
  `order_id`       BIGINT UNSIGNED NOT NULL,
  `user_id`        BIGINT UNSIGNED NOT NULL,
  `channel`        TINYINT NOT NULL COMMENT '1:微信 2:支付宝 3:银行卡',
  `amount`         INT UNSIGNED NOT NULL COMMENT '支付金额（分）',
  `status`         TINYINT NOT NULL DEFAULT 0 COMMENT '0:待支付 1:支付中 2:成功 3:失败 4:关闭',
  `third_party_id` VARCHAR(64) COMMENT '第三方支付流水号',
  `pay_params`     TEXT COMMENT '调起支付的参数（JSON）',
  `create_time`    DATETIME DEFAULT CURRENT_TIMESTAMP,
  `expire_time`    DATETIME NOT NULL COMMENT '流水有效期',
  `pay_time`       DATETIME COMMENT '实际支付时间',
  `notify_time`    DATETIME COMMENT '收到回调时间',
  `notify_raw`     TEXT COMMENT '回调原始报文',
  
  PRIMARY KEY (`flow_id`),
  UNIQUE KEY `uk_order_channel` (`order_id`, `channel`),
  KEY `idx_third_party` (`third_party_id`),
  KEY `idx_status_expire` (`status`, `expire_time`)
) ENGINE=InnoDB COMMENT='支付流水表';
```

**item_stock（库存表）**

```sql
CREATE TABLE `item_stock` (
  `item_id`  INT UNSIGNED NOT NULL PRIMARY KEY,
  `sku_id`   INT UNSIGNED NOT NULL,
  `stock`    INT UNSIGNED NOT NULL DEFAULT 0,
  `version`  INT UNSIGNED NOT NULL DEFAULT 0 COMMENT '乐观锁版本号',
  `update_time` DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) ENGINE=InnoDB;
```

---

## 7. 性能与扩展性

### 7.1 性能指标（设计目标）

| 指标 | 目标值 | 说明 |
|------|--------|------|
| 库存扣减 QPS | 10万+/秒 | Redis Cluster 单分片可达 8~10万 |
| 订单写入 QPS | 5万+/秒 | 分库分片后线性扩展 |
| 端到端延迟 | P99 < 500ms | 同步路径只走 Redis + 极简 SQL |
| 系统总容量 | 100万并发 | Gateway 无状态水平扩展 |

### 7.2 扩展策略

**Gateway 层**
- 无状态，支持 Nginx / K8s 水平扩容
- 长连接场景用一致性哈希保持会话

**Redis 层**
- Cluster 模式，16~64 分片
- 热点 Key 拆分（库存分槽）

**MySQL 层**
- 订单表按 `user_id` 分 128 张表，或直接使用 TiDB
- 读写分离：订单查询走从库

**MQ 层**
- Kafka Partition 按 `user_id` 分区，保证单用户消息顺序
- 消费者按需扩容

---

## 8. 降级与容灾

### 8.1 分级降级策略

| 故障场景 | 降级动作 | 用户体验 |
|----------|----------|----------|
| Redis 节点故障 | 自动切主从，降级为数据库限流模式 | 延迟增加，但不丢请求 |
| Redis 集群全挂 | 关闭抢购入口，返回"活动暂未开始" | 明确告知，不丢单 |
| MQ 堆积严重 | 降低异步消费并发，延长填单时间 | 订单详情加载稍慢 |
| MySQL 主库压力大 | 暂停非核心查询，只保留下单写 | 部分页面功能受限 |
| 被攻击（CC/DDoS） | WAF 自动清洗，IP 黑名单 | 正常用户无感知 |

### 8.2 补偿机制

**订单补偿扫描（兜底）**

```go
func OrderCompensateWorker() {
    for {
        // 扫描最近 5 分钟内 Redis 已预占但数据库无订单的记录
        tokens := redis.Scan("token:meta:*")
        
        for _, token := range tokens {
            meta := redis.HGetAll("token:meta:" + token)
            orderID := meta["order_id"] // 如果已补单会写回
            
            if orderID == "" {
                // Redis 有 token，但数据库没订单 → 异常
                log.Error("order missing, compensate", "token", token)
                
                // 策略 A：补单
                recreateOrderFromToken(token, meta)
                
                // 策略 B：释放预占（如果补单也失败）
                // releasePreoccupy(meta["item_id"], meta["user_id"], token)
            }
        }
        
        time.Sleep(60 * time.Second)
    }
}
```

---

## 9. 部署架构

```
                    ┌─────────────┐
                    │   CDN/WAF   │
                    │  静态 + 清洗 │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ┌─────────┐  ┌─────────┐  ┌─────────┐
        │Gateway-1│  │Gateway-2│  │Gateway-N│  （K8s HPA 自动扩缩）
        └────┬────┘  └────┬────┘  └────┬────┘
             └────────────┼────────────┘
                          ▼
                    ┌─────────────┐
                    │  风控中心    │
                    │  Token 服务  │
                    └──────┬──────┘
                           │
              ┌────────────┼────────────┐
              ▼            ▼            ▼
        ┌─────────┐  ┌─────────┐  ┌─────────┐
        │Seckill-1│  │Seckill-2│  │Seckill-N│  （无状态服务）
        └────┬────┘  └────┬────┘  └────┬────┘
             └────────────┼────────────┘
                          ▼
              ┌───────────────────────┐
              │     Redis Cluster     │
              │   (16-64 分片主从)    │
              └───────────┬───────────┘
                          │
              ┌───────────┴───────────┐
              ▼                       ▼
        ┌─────────────┐        ┌─────────────┐
        │ Kafka/Rocket│        │ MySQL/TiDB  │
        │     MQ      │        │  订单+库存   │
        └─────────────┘        └─────────────┘
```

---

## 10. 关键检查清单（上线前核对）

- [ ] Redis 预占 Lua 脚本经过压测，单分片 QPS 达标
- [ ] 订单主表按 `user_id` 分片或使用分布式数据库
- [ ] 所有返回给用户的状态都是数据库已提交状态
- [ ] 风控评分模型经过灰度验证，误杀率 < 0.1%
- [ ] 验证码仅在 score < 50 时弹出，正常用户不会遇到
- [ ] 支付超时释放定时任务已部署，预占释放逻辑已验证
- [ ] 订单补偿扫描任务已部署，异常告警已配置
- [ ] 全链路监控（Prometheus + Grafana）已接入
- [ ] 降级开关（Redis 挂、MQ 挂、DB 挂）已配置并演练

---

## 11. 总结

| 传统做法 | 本方案 | 收益 |
|----------|--------|------|
| 验证码/滑块防刷 | 后端风控 + 渐进验证 | 用户体验大幅提升 |
| 异步创建订单，用户等待结果 | 同步写订单主表 + 异步填详情 | 确定性反馈，零焦虑 |
| 全量请求打到数据库 | 层层过滤，仅千分之一写库 | 系统容量提升百倍 |
| 系统繁忙请重试 | 只有三种确定态 | 用户信任度提升 |
| 先到先得拼手速 | 预约抽签/排队制 | 公平性提升，脚本失效 |
| 下单扣库存，超时回滚 | 下单预占，支付成功才扣库存 | 避免僵尸订单占库存 |

> **核心公式：好的秒杀系统 = 后端扛得住 + 前端不卡人 + 结果不忽悠。**
