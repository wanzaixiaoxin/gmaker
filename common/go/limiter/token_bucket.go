package limiter

import (
	"sync"
	"time"
)

// shardCount 分片数量，降低全局锁竞争
const shardCount = 64

// TokenBucket 分片令牌桶限流器，按 key hash 分片降低锁竞争
type TokenBucket struct {
	shards [shardCount]*tokenBucketShard
}

type tokenBucketShard struct {
	mu         sync.Mutex
	capacity   float64
	tokens     float64
	fillRate   float64 // tokens per second
	lastUpdate time.Time
}

// NewTokenBucket 创建令牌桶
// capacity: 桶容量（突发上限）；fillRate: 每秒填充令牌数
func NewTokenBucket(capacity int, fillRate int) *TokenBucket {
	tb := &TokenBucket{}
	perShardCap := float64(capacity) / shardCount
	perShardRate := float64(fillRate) / shardCount
	for i := 0; i < shardCount; i++ {
		tb.shards[i] = &tokenBucketShard{
			capacity:   perShardCap,
			tokens:     perShardCap,
			fillRate:   perShardRate,
			lastUpdate: time.Now(),
		}
	}
	return tb
}

func (tb *TokenBucket) shardIndex(key string) int {
	h := 0
	for i := 0; i < len(key); i++ {
		h = 31*h + int(key[i])
	}
	return (h & 0x7FFFFFFF) % shardCount
}

// AllowKey 按 key 分片请求 n 个令牌，成功返回 true
func (tb *TokenBucket) AllowKey(key string, n int) bool {
	s := tb.shards[tb.shardIndex(key)]
	s.mu.Lock()
	defer s.mu.Unlock()

	s.refill()
	if s.tokens >= float64(n) {
		s.tokens -= float64(n)
		return true
	}
	return false
}

// Allow1Key 按 key 分片请求 1 个令牌
func (tb *TokenBucket) Allow1Key(key string) bool {
	return tb.AllowKey(key, 1)
}

// Allow 请求 n 个令牌（全局，向后兼容）
func (tb *TokenBucket) Allow(n int) bool {
	return tb.AllowKey("__global__", n)
}

// Allow1 请求 1 个令牌（全局，向后兼容）
func (tb *TokenBucket) Allow1() bool {
	return tb.AllowKey("__global__", 1)
}

func (s *tokenBucketShard) refill() {
	now := time.Now()
	elapsed := now.Sub(s.lastUpdate).Seconds()
	s.tokens += elapsed * s.fillRate
	if s.tokens > s.capacity {
		s.tokens = s.capacity
	}
	s.lastUpdate = now
}
