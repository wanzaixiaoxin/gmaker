package lock

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

// RedisLock 基于 Redis 的分布式锁，使用 SET NX EX + Lua 解锁
type RedisLock struct {
	client redis.UniversalClient
	key    string
	value  string
	ttl    time.Duration
}

var unlockScript = redis.NewScript(`
	if redis.call("get", KEYS[1]) == ARGV[1] then
		return redis.call("del", KEYS[1])
	else
		return 0
	end
`)

// NewRedisLock 创建分布式锁实例
// key: 锁名称；ttl: 锁自动过期时间
func NewRedisLock(client redis.UniversalClient, key string, ttl time.Duration) *RedisLock {
	return &RedisLock{
		client: client,
		key:    key,
		ttl:    ttl,
	}
}

// TryLock 尝试获取锁，成功返回 true
func (l *RedisLock) TryLock(ctx context.Context) (bool, error) {
	value, err := randomValue()
	if err != nil {
		return false, err
	}
	l.value = value

	ok, err := l.client.SetNX(ctx, l.key, l.value, l.ttl).Result()
	if err != nil {
		return false, fmt.Errorf("redis setnx failed: %w", err)
	}
	return ok, nil
}

// Lock 阻塞获取锁，直到成功或 ctx 取消
func (l *RedisLock) Lock(ctx context.Context) error {
	for {
		ok, err := l.TryLock(ctx)
		if err != nil {
			return err
		}
		if ok {
			return nil
		}
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-time.After(50 * time.Millisecond):
		}
	}
}

// Unlock 释放锁，使用 Lua 脚本保证原子性
func (l *RedisLock) Unlock(ctx context.Context) error {
	if l.value == "" {
		return fmt.Errorf("lock not held")
	}
	res, err := unlockScript.Run(ctx, l.client, []string{l.key}, l.value).Result()
	if err != nil {
		return fmt.Errorf("redis unlock script failed: %w", err)
	}
	if res.(int64) == 0 {
		return fmt.Errorf("lock not held or expired")
	}
	l.value = ""
	return nil
}

// Extend 续期锁（看门狗模式），延长 ttl
func (l *RedisLock) Extend(ctx context.Context, ttl time.Duration) error {
	if l.value == "" {
		return fmt.Errorf("lock not held")
	}
	ok, err := redis.NewScript(`
		if redis.call("get", KEYS[1]) == ARGV[1] then
			return redis.call("pexpire", KEYS[1], ARGV[2])
		else
			return 0
		end
	`).Run(ctx, l.client, []string{l.key}, l.value, ttl.Milliseconds()).Result()
	if err != nil {
		return fmt.Errorf("redis extend script failed: %w", err)
	}
	if ok.(int64) == 0 {
		return fmt.Errorf("lock not held or expired")
	}
	return nil
}

func randomValue() (string, error) {
	b := make([]byte, 16)
	if _, err := rand.Read(b); err != nil {
		return "", err
	}
	return hex.EncodeToString(b), nil
}
