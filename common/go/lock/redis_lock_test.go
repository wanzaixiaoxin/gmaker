package lock

import (
	"context"
	"testing"
	"time"

	"github.com/redis/go-redis/v9"
)

func TestRedisLock(t *testing.T) {
	client := redis.NewClient(&redis.Options{Addr: "127.0.0.1:6379"})
	if err := client.Ping(context.Background()).Err(); err != nil {
		t.Skip("redis not available:", err)
	}

	lock := NewRedisLock(client, "test:lock:1", 5*time.Second)
	ctx := context.Background()

	lease, err := lock.TryLock(ctx)
	if err != nil {
		t.Fatalf("try lock failed: %v", err)
	}
	if lease == nil {
		t.Fatal("expected lock to be acquired")
	}

	lease2, _ := lock.TryLock(ctx)
	if lease2 != nil {
		t.Fatal("expected lock to be already held")
	}

	if err := lease.Unlock(ctx); err != nil {
		t.Fatalf("unlock failed: %v", err)
	}

	lease3, _ := lock.TryLock(ctx)
	if lease3 == nil {
		t.Fatal("expected lock to be re-acquired")
	}
	lease3.Unlock(ctx)
}

func TestRedisLockConcurrent(t *testing.T) {
	client := redis.NewClient(&redis.Options{Addr: "127.0.0.1:6379"})
	if err := client.Ping(context.Background()).Err(); err != nil {
		t.Skip("redis not available:", err)
	}

	lock := NewRedisLock(client, "test:lock:concurrent", 5*time.Second)
	ctx := context.Background()

	lease1, _ := lock.TryLock(ctx)
	if lease1 == nil {
		t.Fatal("first TryLock should succeed")
	}

	lease2, _ := lock.TryLock(ctx)
	if lease2 != nil {
		lease2.Unlock(ctx)
		t.Fatal("second TryLock on same instance should fail")
	}

	lease1.Unlock(ctx)
}
