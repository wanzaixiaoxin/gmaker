package cache

import (
	"context"
	"time"

	"github.com/gmaker/luffa/common/go/redis"
)

// RedisStore 基于 common/go/redis.Client 的 Store 实现
type RedisStore struct {
	client *redis.Client
}

// NewRedisStore 创建 Redis Store
func NewRedisStore(client *redis.Client) Store {
	return &RedisStore{client: client}
}

func (s *RedisStore) Get(ctx context.Context, key string) (string, error) {
	return s.client.Get(ctx, key)
}

func (s *RedisStore) Set(ctx context.Context, key string, value string, ttl time.Duration) error {
	return s.client.Set(ctx, key, value, ttl)
}

func (s *RedisStore) Del(ctx context.Context, keys ...string) error {
	return s.client.Del(ctx, keys...)
}
