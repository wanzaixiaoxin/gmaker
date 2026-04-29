package cache

import (
	"context"
	"fmt"
	"time"

	"golang.org/x/sync/singleflight"
)

// Cache 泛型旁路缓存实现
type Cache[T any] struct {
	store   Store
	codec   Codec[T]
	sf      *singleflight.Group
	ttl     time.Duration
	nilTTL  time.Duration // 空值占位 TTL，默认 60s
	prefix  string        // key 前缀，避免不同业务冲突
}

// Option 缓存配置选项
type Option[T any] func(*Cache[T])

// WithNilTTL 设置空值占位 TTL
func WithNilTTL[T any](d time.Duration) Option[T] {
	return func(c *Cache[T]) {
		c.nilTTL = d
	}
}

// WithPrefix 设置 key 前缀
func WithPrefix[T any](prefix string) Option[T] {
	return func(c *Cache[T]) {
		c.prefix = prefix
	}
}

// NewCache 创建缓存实例
func NewCache[T any](store Store, codec Codec[T], ttl time.Duration, opts ...Option[T]) *Cache[T] {
	c := &Cache[T]{
		store:  store,
		codec:  codec,
		sf:     &singleflight.Group{},
		ttl:    ttl,
		nilTTL: 60 * time.Second,
	}
	for _, opt := range opts {
		opt(c)
	}
	return c
}

func (c *Cache[T]) key(k string) string {
	if c.prefix != "" {
		return c.prefix + ":" + k
	}
	return k
}

// Get 直接从缓存读取（不触发回源）
func (c *Cache[T]) Get(ctx context.Context, key string) (T, error) {
	var zero T
	valStr, err := c.store.Get(ctx, c.key(key))
	if err != nil {
		return zero, ErrNotFound{Key: key}
	}
	if IsNilPlaceholder(valStr) {
		return zero, ErrNotFound{Key: key}
	}
	v, err := c.codec.Decode(valStr)
	if err != nil {
		return zero, fmt.Errorf("cache decode failed: %w", err)
	}
	return v, nil
}

// Set 写入缓存
func (c *Cache[T]) Set(ctx context.Context, key string, value T, ttl time.Duration) error {
	valStr, err := c.codec.Encode(value)
	if err != nil {
		return fmt.Errorf("cache encode failed: %w", err)
	}
	if ttl <= 0 {
		ttl = c.ttl
	}
	return c.store.Set(ctx, c.key(key), valStr, ttl)
}

// Delete 删除缓存
func (c *Cache[T]) Delete(ctx context.Context, keys ...string) error {
	for i, k := range keys {
		keys[i] = c.key(k)
	}
	return c.store.Del(ctx, keys...)
}

// GetOrLoad 旁路缓存读取：先读缓存，未命中则通过 singleflight 合并回源
// loader 为回源函数，从 DB 或其他持久层加载数据
func (c *Cache[T]) GetOrLoad(ctx context.Context, key string, loader func(ctx context.Context, key string) (T, error)) (T, error) {
	var zero T

	// 1. 先读缓存
	v, err := c.Get(ctx, key)
	if err == nil {
		return v, nil
	}
	if _, ok := err.(ErrNotFound); !ok {
		return zero, err // 解码错误等，直接返回
	}

	// 2. 缓存未命中，使用 singleflight 合并并发回源
	valStr, err, _ := c.sf.Do(c.key(key), func() (interface{}, error) {
		loaded, loadErr := loader(ctx, key)
		if loadErr != nil {
			// 回源失败，写入空值占位防止穿透
			_ = c.store.Set(ctx, c.key(key), nilPlaceholder, c.nilTTL)
			return nil, loadErr
		}
		// 编码后写入缓存
		enc, encErr := c.codec.Encode(loaded)
		if encErr != nil {
			return nil, encErr
		}
		_ = c.store.Set(ctx, c.key(key), enc, c.ttl)
		return loaded, nil
	})
	if err != nil {
		return zero, err
	}
	return valStr.(T), nil
}

// Refresh 强制刷新缓存：先 loader 加载，再写缓存
func (c *Cache[T]) Refresh(ctx context.Context, key string, loader func(ctx context.Context, key string) (T, error)) (T, error) {
	var zero T
	loaded, err := loader(ctx, key)
	if err != nil {
		return zero, err
	}
	if err := c.Set(ctx, key, loaded, c.ttl); err != nil {
		return zero, err
	}
	return loaded, nil
}
