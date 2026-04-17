package limiter

import (
	"testing"
	"time"
)

func TestTokenBucket(t *testing.T) {
	tb := NewTokenBucket(10, 10)

	// 初始应该能一次性通过 10 个
	for i := 0; i < 10; i++ {
		if !tb.Allow1() {
			t.Fatalf("expected allow at index %d", i)
		}
	}
	// 第 11 个应该拒绝
	if tb.Allow1() {
		t.Fatal("expected reject after capacity exhausted")
	}

	// 等待 1 秒，应该恢复 10 个令牌
	time.Sleep(1100 * time.Millisecond)
	for i := 0; i < 10; i++ {
		if !tb.Allow1() {
			t.Fatalf("expected allow after refill at index %d", i)
		}
	}
}

func TestTokenBucketBurst(t *testing.T) {
	tb := NewTokenBucket(5, 100)
	// 突发 5 个
	for i := 0; i < 5; i++ {
		if !tb.Allow1() {
			t.Fatalf("expected burst allow at %d", i)
		}
	}
	if tb.Allow1() {
		t.Fatal("expected reject after burst")
	}
}
