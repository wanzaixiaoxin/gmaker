package redis

import (
	"sync"
	"time"
)

// HotKeyLimiter 基于滑动窗口的热 Key 限流器
type HotKeyLimiter struct {
	mu      sync.RWMutex
	windows map[string]*window
	maxQPS  int
	window  time.Duration
}

type window struct {
	count int
	start time.Time
}

// NewHotKeyLimiter 创建热 Key 限流器，maxQPS 为单 Key 每秒最大访问次数
func NewHotKeyLimiter(maxQPS int) *HotKeyLimiter {
	return &HotKeyLimiter{
		windows: make(map[string]*window),
		maxQPS:  maxQPS,
		window:  time.Second,
	}
}

// Allow 检查 key 是否允许通过
func (l *HotKeyLimiter) Allow(key string) bool {
	now := time.Now()
	l.mu.Lock()
	defer l.mu.Unlock()

	w, ok := l.windows[key]
	if !ok || now.Sub(w.start) > l.window {
		l.windows[key] = &window{count: 1, start: now}
		return true
	}
	if w.count >= l.maxQPS {
		return false
	}
	w.count++
	return true
}

// CleanLoop 定期清理过期窗口，需在后台 goroutine 中运行
func (l *HotKeyLimiter) CleanLoop() {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()
	for range ticker.C {
		now := time.Now()
		l.mu.Lock()
		for k, w := range l.windows {
			if now.Sub(w.start) > l.window*2 {
				delete(l.windows, k)
			}
		}
		l.mu.Unlock()
	}
}
