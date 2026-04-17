package redis

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
	"dbproxy-go/internal/limiter"
)

// Proxy Redis 代理层
type Proxy struct {
	client  redis.UniversalClient
	limiter *limiter.HotKeyLimiter
}

// Config Redis 配置
type Config struct {
	Addrs    []string
	Password string
	PoolSize int
}

func NewProxy(cfg Config) *Proxy {
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
	return &Proxy{
		client:  client,
		limiter: limiter.NewHotKeyLimiter(1000),
	}
}

func (p *Proxy) Start() {
	go p.limiter.CleanLoop()
}

func (p *Proxy) Close() error {
	return p.client.Close()
}

func (p *Proxy) Ping(ctx context.Context) error {
	return p.client.Ping(ctx).Err()
}

// Get 带热 Key 限流保护
func (p *Proxy) Get(ctx context.Context, key string) (string, error) {
	if !p.limiter.Allow(key) {
		return "", fmt.Errorf("hot key rate limited: %s", key)
	}
	return p.client.Get(ctx, key).Result()
}

func (p *Proxy) Set(ctx context.Context, key string, value string, ttl time.Duration) error {
	return p.client.Set(ctx, key, value, ttl).Err()
}

func (p *Proxy) Del(ctx context.Context, keys ...string) error {
	return p.client.Del(ctx, keys...).Err()
}

// PipelineExec 执行 Pipeline
func (p *Proxy) PipelineExec(ctx context.Context, cmds []Cmd) ([]interface{}, error) {
	pipe := p.client.Pipeline()
	for _, c := range cmds {
		switch c.Op {
		case "GET":
			pipe.Get(ctx, c.Key)
		case "SET":
			pipe.Set(ctx, c.Key, c.Val, c.TTL)
		case "DEL":
			pipe.Del(ctx, c.Key)
		}
	}
	res, err := pipe.Exec(ctx)
	if err != nil && err != redis.Nil {
		return nil, err
	}
	out := make([]interface{}, 0, len(res))
	for _, r := range res {
		out = append(out, r.(*redis.Cmd).Val())
	}
	return out, nil
}

// Cmd Pipeline 命令描述
type Cmd struct {
	Op  string
	Key string
	Val string
	TTL time.Duration
}
