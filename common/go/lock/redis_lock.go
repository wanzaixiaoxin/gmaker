package lock

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

type RedisLock struct {
	client redis.UniversalClient
	key    string
	ttl    time.Duration
}

var unlockScript = redis.NewScript(`
	if redis.call("get", KEYS[1]) == ARGV[1] then
		return redis.call("del", KEYS[1])
	else
		return 0
	end
`)

func NewRedisLock(client redis.UniversalClient, key string, ttl time.Duration) *RedisLock {
	return &RedisLock{
		client: client,
		key:    key,
		ttl:    ttl,
	}
}

type Lease struct {
	lock  *RedisLock
	value string
}

func (l *Lease) Unlock(ctx context.Context) error {
	if l.value == "" {
		return fmt.Errorf("lock not held")
	}
	res, err := unlockScript.Run(ctx, l.lock.client, []string{l.lock.key}, l.value).Result()
	if err != nil {
		return fmt.Errorf("redis unlock script failed: %w", err)
	}
	if res.(int64) == 0 {
		return fmt.Errorf("lock not held or expired")
	}
	l.value = ""
	return nil
}

func (l *Lease) Extend(ctx context.Context, ttl time.Duration) error {
	if l.value == "" {
		return fmt.Errorf("lock not held")
	}
	if ttl <= 0 {
		return fmt.Errorf("invalid ttl: must be positive")
	}
	ok, err := redis.NewScript(`
		if redis.call("get", KEYS[1]) == ARGV[1] then
			return redis.call("pexpire", KEYS[1], ARGV[2])
		else
			return 0
		end
	`).Run(ctx, l.lock.client, []string{l.lock.key}, l.value, ttl.Milliseconds()).Result()
	if err != nil {
		return fmt.Errorf("redis extend script failed: %w", err)
	}
	if ok.(int64) == 0 {
		return fmt.Errorf("lock not held or expired")
	}
	return nil
}

func (l *RedisLock) TryLock(ctx context.Context) (*Lease, error) {
	value, err := randomValue()
	if err != nil {
		return nil, err
	}

	ok, err := l.client.SetNX(ctx, l.key, value, l.ttl).Result()
	if err != nil {
		return nil, fmt.Errorf("redis setnx failed: %w", err)
	}
	if !ok {
		return nil, nil
	}
	return &Lease{lock: l, value: value}, nil
}

func (l *RedisLock) Lock(ctx context.Context) (*Lease, error) {
	for {
		lease, err := l.TryLock(ctx)
		if err != nil {
			return nil, err
		}
		if lease != nil {
			return lease, nil
		}
		select {
		case <-ctx.Done():
			return nil, ctx.Err()
		case <-time.After(50 * time.Millisecond):
		}
	}
}

func randomValue() (string, error) {
	b := make([]byte, 16)
	if _, err := rand.Read(b); err != nil {
		return "", err
	}
	return hex.EncodeToString(b), nil
}
