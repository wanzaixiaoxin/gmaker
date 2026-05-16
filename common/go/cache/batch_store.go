package cache

import (
	"context"
	"time"

	goredis "github.com/redis/go-redis/v9"

	"github.com/gmaker/luffa/common/go/redis"
)

// BatchStore 批量存储扩展接口（在 Store 基础上增加 MGet/MSet）
type BatchStore interface {
	Store
	MGet(ctx context.Context, keys ...string) ([]string, error)
	MSet(ctx context.Context, pairs map[string]string, ttl time.Duration) error
}

// RedisStoreWithBatch 扩展 RedisStore，支持批量读写
var _ BatchStore = (*RedisStoreWithBatch)(nil)

// RedisStoreWithBatch 支持批量操作的 Redis Store
type RedisStoreWithBatch struct {
	*RedisStore
	raw goredis.UniversalClient
}

// NewRedisStoreWithBatch 创建支持批量操作的 Redis Store
func NewRedisStoreWithBatch(client *redis.Client) *RedisStoreWithBatch {
	return &RedisStoreWithBatch{
		RedisStore: NewRedisStore(client).(*RedisStore),
		raw:        client.RawClient(),
	}
}

// MGet 批量读取，返回值与 keys 一一对应，不存在的 key 对应空字符串
func (s *RedisStoreWithBatch) MGet(ctx context.Context, keys ...string) ([]string, error) {
	if len(keys) == 0 {
		return nil, nil
	}
	vals, err := s.raw.MGet(ctx, keys...).Result()
	if err != nil {
		return nil, err
	}
	result := make([]string, len(vals))
	for i, v := range vals {
		if v == nil {
			result[i] = ""
			continue
		}
		result[i], _ = v.(string)
	}
	return result, nil
}

// MSet 批量写入，ttl > 0 时使用 Pipeline 逐条设置过期时间
func (s *RedisStoreWithBatch) MSet(ctx context.Context, pairs map[string]string, ttl time.Duration) error {
	if len(pairs) == 0 {
		return nil
	}
	if ttl <= 0 {
		// 无 TTL：使用原生 MSET（单条命令，高效）
		flat := make([]interface{}, 0, len(pairs)*2)
		for k, v := range pairs {
			flat = append(flat, k, v)
		}
		return s.raw.MSet(ctx, flat...).Err()
	}

	// 有 TTL：使用 Pipeline 逐条 SET（MSET 不支持 per-key TTL）
	pipe := s.raw.Pipeline()
	for k, v := range pairs {
		pipe.Set(ctx, k, v, ttl)
	}
	_, err := pipe.Exec(ctx)
	return err
}
