package limiter

import (
	"sync"
	"time"
)

// TokenBucket 单节点令牌桶限流器
type TokenBucket struct {
	mu         sync.Mutex
	capacity   float64
	tokens     float64
	fillRate   float64 // tokens per second
	lastUpdate time.Time
}

// NewTokenBucket 创建令牌桶
// capacity: 桶容量（突发上限）；fillRate: 每秒填充令牌数
func NewTokenBucket(capacity int, fillRate int) *TokenBucket {
	return &TokenBucket{
		capacity:   float64(capacity),
		tokens:     float64(capacity),
		fillRate:   float64(fillRate),
		lastUpdate: time.Now(),
	}
}

// Allow 请求 n 个令牌，成功返回 true
func (tb *TokenBucket) Allow(n int) bool {
	tb.mu.Lock()
	defer tb.mu.Unlock()

	tb.refill()
	if tb.tokens >= float64(n) {
		tb.tokens -= float64(n)
		return true
	}
	return false
}

// Allow1 请求 1 个令牌
func (tb *TokenBucket) Allow1() bool {
	return tb.Allow(1)
}

func (tb *TokenBucket) refill() {
	now := time.Now()
	elapsed := now.Sub(tb.lastUpdate).Seconds()
	tb.tokens += elapsed * tb.fillRate
	if tb.tokens > tb.capacity {
		tb.tokens = tb.capacity
	}
	tb.lastUpdate = now
}
