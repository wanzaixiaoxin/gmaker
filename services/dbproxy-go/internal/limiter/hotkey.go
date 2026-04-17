package limiter

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
	count  int
	start  time.Time
}

func NewHotKeyLimiter(maxQPS int) *HotKeyLimiter {
	return &HotKeyLimiter{
		windows: make(map[string]*window),
		maxQPS:  maxQPS,
		window:  time.Second,
	}
}

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
