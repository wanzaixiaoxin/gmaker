package replay

import (
	"fmt"
	"sync"
	"time"
)

// Checker timestamp + nonce 防重放检查器
type Checker struct {
	mu       sync.RWMutex
	window   time.Duration
	nonces   map[string]time.Time // nonce -> 首次收到时间
	minTs    time.Time            // 窗口下界，小于此时间的包直接拒绝
}

// NewChecker 创建防重放检查器
// window: 时间窗口，同一 nonce 在此窗口内不可重复
func NewChecker(window time.Duration) *Checker {
	return &Checker{
		window: window,
		nonces: make(map[string]time.Time),
		minTs:  time.Now().Add(-window),
	}
}

// Check 检查 (timestamp, nonce) 是否合法
// timestamp 使用 time.Time，nonce 为唯一标识字符串
func (c *Checker) Check(ts time.Time, nonce string) error {
	now := time.Now()
	if ts.After(now.Add(30 * time.Second)) {
		return fmt.Errorf("timestamp too far in the future")
	}
	if ts.Before(c.minTs) {
		return fmt.Errorf("timestamp outside replay window")
	}

	key := ts.UTC().Format(time.RFC3339Nano) + ":" + nonce

	c.mu.Lock()
	defer c.mu.Unlock()

	if _, exists := c.nonces[key]; exists {
		return fmt.Errorf("duplicate nonce detected")
	}
	c.nonces[key] = now
	c.gcLocked(now)
	return nil
}

// Clean 主动清理过期的 nonce 记录
func (c *Checker) Clean() {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.gcLocked(time.Now())
}

func (c *Checker) gcLocked(now time.Time) {
	cutoff := now.Add(-c.window)
	if cutoff.Before(c.minTs) {
		return
	}
	c.minTs = cutoff
	for k, t := range c.nonces {
		if t.Before(cutoff) {
			delete(c.nonces, k)
		}
	}
}
