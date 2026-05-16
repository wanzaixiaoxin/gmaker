package cache

import (
	"context"
	"fmt"
	"math/rand"
	"time"

	goredis "github.com/redis/go-redis/v9"
	"golang.org/x/sync/singleflight"

	"github.com/gmaker/luffa/common/go/limiter"
)

// ---------------------------------------------------------------------------
// CacheManager —— 二级缓存管理器（L1: Redis, L2: DB）
// ---------------------------------------------------------------------------
//
// 设计原则：
//   - 泛型 + 配置驱动：通过 ManagerConfig 和 struct tag 适配任意表结构
//   - 不改代码：换一张表 = 换一个配置 + 换一个结构体
//   - 三大防护：穿透（空值占位）、雪崩（TTL 抖动）、击穿（singleflight + 分布式锁）
//   - 熔断保护：连续 DB 失败触发熔断，半开探测恢复
//
// 使用方式：
//
//	mgr := cache.NewManager[Player](store, codec, loader, config)
//	player, err := mgr.Get(ctx, 12345)
type CacheManager[T any] struct {
	store      Store
	codec      Codec[T]
	config     ManagerConfig
	loader     DataLoader[T]
	sf         singleflight.Group
	breaker    *limiter.CircuitBreaker
	lockClient goredis.UniversalClient // 分布式锁（可选，防跨实例击穿）

	// keyFunc 从实体中提取主键值（批量加载时用于构建结果 map）
	keyFunc func(T) interface{}
}

// NewManager 创建二级缓存管理器
func NewManager[T any](
	store Store,
	codec Codec[T],
	loader DataLoader[T],
	config ManagerConfig,
) *CacheManager[T] {
	m := &CacheManager[T]{
		store:  store,
		codec:  codec,
		config: config,
		loader: loader,
	}

	if config.EnableBreaker {
		m.breaker = limiter.NewCircuitBreaker(
			config.BreakerFailureThreshold,
			config.BreakerSuccessThreshold,
			2, // half-open max requests
			config.BreakerTimeout,
		)
	}

	return m
}

// WithLockClient 注入 Redis 客户端以启用分布式锁防击穿（跨实例互斥回源）
func (m *CacheManager[T]) WithLockClient(client goredis.UniversalClient) *CacheManager[T] {
	m.lockClient = client
	m.config.EnableStampedeLock = true
	return m
}

// WithKeyFunc 设置从实体提取主键的函数（批量加载必需）
func (m *CacheManager[T]) WithKeyFunc(fn func(T) interface{}) *CacheManager[T] {
	m.keyFunc = fn
	return m
}

// ---------------------------------------------------------------------------
// 公开 API
// ---------------------------------------------------------------------------

// Get 获取单条数据（L1 Redis → L2 DB 回源）
//
// 流程:
//  1. 查 Redis → 命中且非占位符 → 返回
//  2. 查 Redis → 命中空值占位符 → 返回 ErrNotFound（防穿透）
//  3. 未命中 → singleflight 合并并发请求 → 回源 DB
//  4. DB 有数据 → 写入 Redis（带抖动 TTL） → 返回
//  5. DB 无数据 → 写入空值占位符（防穿透） → 返回 ErrNotFound
func (m *CacheManager[T]) Get(ctx context.Context, key interface{}) (T, error) {
	var zero T
	ck := m.cacheKey(key)

	// ---- L1: Redis ----
	valStr, err := m.store.Get(ctx, ck)
	if err == nil {
		if IsNilPlaceholder(valStr) {
			// 空值占位符 → 防穿透
			return zero, ErrNotFound{Key: fmt.Sprintf("%v", key)}
		}
		v, decErr := m.codec.Decode(valStr)
		if decErr == nil {
			return v, nil
		}
		// 解码失败，视为 miss，回源
	}

	// ---- L1 miss → singleflight 合并 + 回源 ----
	result, err, _ := m.sf.Do(ck, func() (interface{}, error) {
		return m.loadFromDB(ctx, key, ck)
	})
	if err != nil {
		return zero, err
	}
	return result.(T), nil
}

// GetBatch 批量获取数据。需要通过 WithKeyFunc 设置主键提取函数。
//
// 流程:
//  1. 批量查 Redis（支持 BatchStore 时使用 MGet，否则逐条 Get）
//  2. 收集 miss 的 key，批量回源 DB
//  3. 将 DB 结果写入 Redis
func (m *CacheManager[T]) GetBatch(ctx context.Context, keys []interface{}) (map[interface{}]T, error) {
	result := make(map[interface{}]T, len(keys))
	if len(keys) == 0 {
		return result, nil
	}

	cacheKeys := make([]string, len(keys))
	for i, k := range keys {
		cacheKeys[i] = m.cacheKey(k)
	}

	// ---- L1: 批量查 Redis ----
	var hitIndices []int // 记录命中但需要回源的索引
	var missedKeys []interface{}
	var missedIndices []int

	if bs, ok := m.store.(BatchStore); ok {
		vals, err := bs.MGet(ctx, cacheKeys...)
		if err != nil {
			// MGet 失败，全部视为 miss
			missedKeys = keys
			for i := range keys {
				missedIndices = append(missedIndices, i)
			}
		} else {
			for i, valStr := range vals {
				if valStr == "" {
					missedKeys = append(missedKeys, keys[i])
					missedIndices = append(missedIndices, i)
					continue
				}
				if IsNilPlaceholder(valStr) {
					continue // 空值占位，跳过
				}
				v, decErr := m.codec.Decode(valStr)
				if decErr != nil {
					hitIndices = append(hitIndices, i)
					continue
				}
				result[keys[i]] = v
			}
		}
	} else {
		// fallback: 逐条查询
		for i, k := range keys {
			valStr, err := m.store.Get(ctx, cacheKeys[i])
			if err != nil || valStr == "" {
				missedKeys = append(missedKeys, k)
				missedIndices = append(missedIndices, i)
				continue
			}
			if IsNilPlaceholder(valStr) {
				continue
			}
			v, decErr := m.codec.Decode(valStr)
			if decErr != nil {
				hitIndices = append(hitIndices, i)
				continue
			}
			result[k] = v
		}
	}

	// ---- L2: 批量回源 DB ----
	if len(missedKeys) > 0 {
		loaded, err := m.loader.LoadBatch(ctx, missedKeys)
		if err != nil {
			return result, fmt.Errorf("batch load from db failed: %w", err)
		}

		// 构建已加载 key 集合
		loadedSet := make(map[interface{}]bool)
		pairs := make(map[string]string)

		for _, v := range loaded {
			var k interface{}
			if m.keyFunc != nil {
				k = m.keyFunc(v)
			} else {
				// fallback: 用 missedKeys 顺序匹配（不可靠，建议设置 KeyFunc）
				continue
			}
			loadedSet[k] = true
			result[k] = v

			encoded, encErr := m.codec.Encode(v)
			if encErr == nil {
				pairs[m.cacheKey(k)] = encoded
			}
		}

		// 批量写入 Redis（带抖动 TTL）
		if len(pairs) > 0 {
			ttl := m.jitterTTL()
			if bs, ok := m.store.(BatchStore); ok {
				_ = bs.MSet(ctx, pairs, ttl)
			} else {
				for k, v := range pairs {
					_ = m.store.Set(ctx, k, v, ttl)
				}
			}
		}

		// 未加载到的 key → 空值占位（防穿透）
		if m.config.NilTTL > 0 {
			for _, k := range missedKeys {
				if !loadedSet[k] {
					_ = m.store.Set(ctx, m.cacheKey(k), nilPlaceholder, m.config.NilTTL)
				}
			}
		}
	}

	// 解码失败的 key 重新回源
	for _, idx := range hitIndices {
		v, err := m.Get(ctx, keys[idx])
		if err == nil {
			result[keys[idx]] = v
		}
	}

	return result, nil
}

// Set 手动写入缓存
func (m *CacheManager[T]) Set(ctx context.Context, key interface{}, value T) error {
	ck := m.cacheKey(key)
	return m.encodeAndSet(ctx, ck, value)
}

// Delete 删除缓存（使缓存失效，下次 Get 时回源）
func (m *CacheManager[T]) Delete(ctx context.Context, keys ...interface{}) error {
	cacheKeys := make([]string, len(keys))
	for i, k := range keys {
		cacheKeys[i] = m.cacheKey(k)
	}
	return m.store.Del(ctx, cacheKeys...)
}

// Refresh 强制刷新：删除缓存后立即回源加载
func (m *CacheManager[T]) Refresh(ctx context.Context, key interface{}) (T, error) {
	ck := m.cacheKey(key)
	_ = m.store.Del(ctx, ck)
	return m.Get(ctx, key)
}

// ---------------------------------------------------------------------------
// 内部方法
// ---------------------------------------------------------------------------

// cacheKey 生成带前缀的 Redis key
func (m *CacheManager[T]) cacheKey(key interface{}) string {
	return m.config.Prefix + ":" + fmt.Sprintf("%v", key)
}

// jitterTTL 在基础 TTL 上添加随机抖动，防止大量 key 同时过期（防雪崩）
func (m *CacheManager[T]) jitterTTL() time.Duration {
	base := m.config.TTL
	if m.config.JitterPct <= 0 {
		return base
	}
	jitter := time.Duration(float64(base) * m.config.JitterPct * rand.Float64())
	return base + jitter
}

// encodeAndSet 编码并写入 Redis（带抖动 TTL）
func (m *CacheManager[T]) encodeAndSet(ctx context.Context, ck string, value T) error {
	encoded, err := m.codec.Encode(value)
	if err != nil {
		return fmt.Errorf("codec encode failed: %w", err)
	}
	return m.store.Set(ctx, ck, encoded, m.jitterTTL())
}

// loadFromDB 从数据库加载数据并写入缓存（singleflight 合并后的执行体）
func (m *CacheManager[T]) loadFromDB(ctx context.Context, key interface{}, ck string) (T, error) {
	// 分布式锁防击穿
	if m.config.EnableStampedeLock && m.lockClient != nil {
		return m.loadWithLock(ctx, key, ck)
	}
	return m.doLoad(ctx, key, ck)
}

// doLoad 实际的 DB 加载 + 缓存写入逻辑
func (m *CacheManager[T]) doLoad(ctx context.Context, key interface{}, ck string) (T, error) {
	var zero T

	// 熔断检查
	if m.breaker != nil && !m.breaker.Allow() {
		return zero, fmt.Errorf("cache manager breaker open for key: %v", key)
	}

	// 从 DB 加载
	v, err := m.loader.Load(ctx, key)
	if err != nil {
		if _, ok := err.(ErrNotFound); ok {
			// 数据不存在 → 写入空值占位符（防穿透）
			if m.config.NilTTL > 0 {
				_ = m.store.Set(ctx, ck, nilPlaceholder, m.config.NilTTL)
			}
			if m.breaker != nil {
				m.breaker.RecordSuccess() // 数据不存在不算 DB 故障
			}
			return zero, err
		}
		// DB 出错 → 记录熔断
		if m.breaker != nil {
			m.breaker.RecordFailure()
		}
		return zero, err
	}

	// 写入 Redis（带抖动 TTL，防雪崩）
	if encErr := m.encodeAndSet(ctx, ck, v); encErr == nil {
		if m.breaker != nil {
			m.breaker.RecordSuccess()
		}
	}

	return v, nil
}

// loadWithLock 分布式锁保护下的回源加载（防跨实例击穿）
func (m *CacheManager[T]) loadWithLock(ctx context.Context, key interface{}, ck string) (T, error) {
	var zero T
	lockKey := ck + ":lock"
	lockVal := fmt.Sprintf("%d", time.Now().UnixNano())

	// 尝试获取锁（SetNX）
	acquired, err := m.lockClient.SetNX(ctx, lockKey, lockVal, m.config.StampedeLockTTL).Result()
	if err != nil {
		// Redis 故障 → 降级直接加载
		return m.doLoad(ctx, key, ck)
	}

	if acquired {
		// 获取锁成功 → 回源加载
		defer m.lockClient.Del(ctx, lockKey)
		return m.doLoad(ctx, key, ck)
	}

	// 获取锁失败 → 其他实例正在加载，轮询等待缓存就绪
	waitCtx, cancel := context.WithTimeout(ctx, m.config.StampedeWaitTimeout)
	defer cancel()

	ticker := time.NewTicker(50 * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-waitCtx.Done():
			// 等待超时 → 降级直接查 DB
			return m.doLoad(ctx, key, ck)
		case <-ticker.C:
			valStr, err := m.store.Get(waitCtx, ck)
			if err != nil {
				continue
			}
			if IsNilPlaceholder(valStr) {
				return zero, ErrNotFound{Key: fmt.Sprintf("%v", key)}
			}
			v, decErr := m.codec.Decode(valStr)
			if decErr == nil {
				return v, nil
			}
		}
	}
}
