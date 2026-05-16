package cache

import "time"

// ManagerConfig 二级缓存管理器配置（L1: Redis, L2: DB）
//
// 核心防护策略均通过配置开关控制，业务侧无需修改管理器代码：
//   - 防穿透：空值占位缓存（NilTTL > 0 时启用）
//   - 防雪崩：TTL 随机抖动（JitterPct > 0 时启用）
//   - 防击穿：singleflight + 可选分布式锁（EnableStampedeLock）
//   - 熔断保护：连续 DB 失败触发熔断（EnableBreaker）
type ManagerConfig struct {
	// ---- 基础配置 ----

	// Prefix Redis key 前缀，最终格式: {prefix}:{key}
	Prefix string

	// TTL 缓存基础过期时间
	TTL time.Duration

	// ---- 防穿透 ----

	// NilTTL 空值占位过期时间。数据库中不存在的 key 会在 Redis 中写入占位符，
	// 防止恶意/无效请求反复穿透到 DB。设为 0 则不缓存空值。
	NilTTL time.Duration

	// ---- 防雪崩 ----

	// JitterPct TTL 随机抖动比例 [0, 0.5)。
	// 实际 TTL = TTL * (1 + rand(0, JitterPct))，避免大量 key 同时过期。
	JitterPct float64

	// ---- 防击穿 ----

	// EnableStampedeLock 是否启用分布式锁防止热点 key 击穿。
	// 需通过 WithLockClient 注入 Redis 客户端。
	EnableStampedeLock bool

	// StampedeLockTTL 分布式锁持有超时
	StampedeLockTTL time.Duration

	// StampedeWaitTimeout 等待锁的最大时间，超时后降级直接查 DB
	StampedeWaitTimeout time.Duration

	// ---- 熔断保护 ----

	// EnableBreaker 是否启用熔断器保护数据库
	EnableBreaker bool

	// BreakerFailureThreshold 连续失败多少次触发熔断
	BreakerFailureThreshold int

	// BreakerSuccessThreshold 半开状态下连续成功多少次恢复
	BreakerSuccessThreshold int

	// BreakerTimeout 熔断持续时间，过后进入半开状态
	BreakerTimeout time.Duration
}

// DefaultManagerConfig 返回带合理默认值的配置。
//   - TTL: 30 分钟
//   - NilTTL: 60 秒（防穿透）
//   - JitterPct: 15%（防雪崩）
//   - 熔断: 5 次失败触发，30 秒恢复
func DefaultManagerConfig(prefix string) ManagerConfig {
	return ManagerConfig{
		Prefix:                  prefix,
		TTL:                     30 * time.Minute,
		NilTTL:                  60 * time.Second,
		JitterPct:               0.15,
		EnableStampedeLock:      false,
		StampedeLockTTL:         5 * time.Second,
		StampedeWaitTimeout:     3 * time.Second,
		EnableBreaker:           true,
		BreakerFailureThreshold: 5,
		BreakerSuccessThreshold: 3,
		BreakerTimeout:          30 * time.Second,
	}
}

// SQLLoaderConfig SQL 数据加载器配置
type SQLLoaderConfig struct {
	// Table 表名
	Table string

	// KeyColumn 主键列名，如 "uid"、"id"
	KeyColumn string

	// Columns 查询列（为空则从结构体的 db tag 自动解析）
	Columns []string
}
