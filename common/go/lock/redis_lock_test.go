package lock

import (
	"context"
	"testing"
	"time"

	"github.com/redis/go-redis/v9"
)

// TestRedisLock 需要本地 Redis，未启动时跳过
func TestRedisLock(t *testing.T) {
	client := redis.NewClient(&redis.Options{Addr: "127.0.0.1:6379"})
	if err := client.Ping(context.Background()).Err(); err != nil {
		t.Skip("redis not available:", err)
	}

	lock := NewRedisLock(client, "test:lock:1", 5*time.Second)
	ctx := context.Background()

	// 获取锁
	ok, err := lock.TryLock(ctx)
	if err != nil {
		t.Fatalf("try lock failed: %v", err)
	}
	if !ok {
		t.Fatal("expected lock to be acquired")
	}

	// 再次获取应失败
	ok2, _ := lock.TryLock(ctx)
	if ok2 {
		t.Fatal("expected lock to be already held")
	}

	// 释放锁
	if err := lock.Unlock(ctx); err != nil {
		t.Fatalf("unlock failed: %v", err)
	}

	// 释放后应可再次获取
	ok3, _ := lock.TryLock(ctx)
	if !ok3 {
		t.Fatal("expected lock to be re-acquired")
	}
	lock.Unlock(ctx)
}
