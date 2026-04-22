package redis

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

// Client Redis 客户端封装，支持单节点和集群模式，可选热 Key 限流保护
type Client struct {
	client  redis.UniversalClient
	limiter *HotKeyLimiter
}

// NewClient 创建 Redis 客户端
//   - 单节点：cfg.Addrs 填一个地址
//   - 集群：cfg.Addrs 填多个地址
//   - 热 Key 限流：可通过 WithHotKeyLimiter 选项启用
func NewClient(cfg Config, opts ...Option) *Client {
	var client redis.UniversalClient
	if len(cfg.Addrs) == 1 {
		client = redis.NewClient(&redis.Options{
			Addr:     cfg.Addrs[0],
			Password: cfg.Password,
			PoolSize: cfg.PoolSize,
		})
	} else {
		client = redis.NewClusterClient(&redis.ClusterOptions{
			Addrs:    cfg.Addrs,
			Password: cfg.Password,
			PoolSize: cfg.PoolSize,
		})
	}
	c := &Client{client: client}
	for _, opt := range opts {
		opt(c)
	}
	return c
}

// Option Client 配置选项
type Option func(*Client)

// WithHotKeyLimiter 启用热 Key 限流保护，maxQPS 为单 Key 每秒最大访问次数
func WithHotKeyLimiter(maxQPS int) Option {
	return func(c *Client) {
		c.limiter = NewHotKeyLimiter(maxQPS)
		go c.limiter.CleanLoop()
	}
}

// Close 关闭连接池
func (c *Client) Close() error {
	return c.client.Close()
}

// Ping 检测连接可用性
func (c *Client) Ping(ctx context.Context) error {
	return c.client.Ping(ctx).Err()
}

// Get 读取 Key，若启用了热 Key 限流且被限流则返回 error
func (c *Client) Get(ctx context.Context, key string) (string, error) {
	if c.limiter != nil && !c.limiter.Allow(key) {
		return "", fmt.Errorf("hot key rate limited: %s", key)
	}
	return c.client.Get(ctx, key).Result()
}

// Set 写入 Key，ttl 为 0 表示不设置过期时间
func (c *Client) Set(ctx context.Context, key string, value string, ttl time.Duration) error {
	return c.client.Set(ctx, key, value, ttl).Err()
}

// Del 删除 Key
func (c *Client) Del(ctx context.Context, keys ...string) error {
	return c.client.Del(ctx, keys...).Err()
}

// Pipeline 返回原生 Pipeline 对象，适合批量操作
func (c *Client) Pipeline(ctx context.Context) redis.Pipeliner {
	return c.client.Pipeline()
}

// RawClient 返回底层 go-redis 客户端，用于高级场景
func (c *Client) RawClient() redis.UniversalClient {
	return c.client
}
