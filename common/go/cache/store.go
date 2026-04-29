package cache

import (
	"context"
	"time"
)

// Store 底层存储抽象（Redis、本地内存等均可实现）
type Store interface {
	Get(ctx context.Context, key string) (string, error)
	Set(ctx context.Context, key string, value string, ttl time.Duration) error
	Del(ctx context.Context, keys ...string) error
}
