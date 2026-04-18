package main

import (
	"fmt"
	"math"
	"sort"
	"sync"
	"sync/atomic"
	"time"
)

const latencyWindowSize = 10000

// BotStats 单个 Bot 的统计
type BotStats struct {
	Success atomic.Uint64
	Fail    atomic.Uint64
}

// GlobalStats 全局聚合统计
type GlobalStats struct {
	mu sync.RWMutex

	TotalReq    atomic.Uint64
	SuccessReq  atomic.Uint64
	FailReq     atomic.Uint64
	ActiveConns atomic.Int32

	// 延迟环形缓冲区
	latencies []time.Duration
	latIdx    int
	latCount  int
}

// NewGlobalStats 创建全局统计
func NewGlobalStats() *GlobalStats {
	return &GlobalStats{
		latencies: make([]time.Duration, latencyWindowSize),
	}
}

// Record 记录一次请求结果
func (s *GlobalStats) Record(success bool, latency time.Duration) {
	s.TotalReq.Add(1)
	if success {
		s.SuccessReq.Add(1)
	} else {
		s.FailReq.Add(1)
	}

	s.mu.Lock()
	s.latencies[s.latIdx] = latency
	s.latIdx = (s.latIdx + 1) % latencyWindowSize
	if s.latCount < latencyWindowSize {
		s.latCount++
	}
	s.mu.Unlock()
}

// Snapshot 获取当前统计快照
func (s *GlobalStats) Snapshot() StatsSnapshot {
	s.mu.RLock()
	defer s.mu.RUnlock()

	var total, success, fail uint64
	total = s.TotalReq.Load()
	success = s.SuccessReq.Load()
	fail = s.FailReq.Load()

	var p50, p99, avg time.Duration
	if s.latCount > 0 {
		// 拷贝延迟数据并排序
		samples := make([]time.Duration, s.latCount)
		start := 0
		if s.latCount == latencyWindowSize {
			start = s.latIdx
		}
		for i := 0; i < s.latCount; i++ {
			samples[i] = s.latencies[(start+i)%latencyWindowSize]
		}
		sort.Slice(samples, func(i, j int) bool {
			return samples[i] < samples[j]
		})

		p50 = samples[len(samples)*50/100]
		p99 = samples[len(samples)*99/100]

		var sum time.Duration
		for _, v := range samples {
			sum += v
		}
		avg = sum / time.Duration(len(samples))
	}

	return StatsSnapshot{
		Total:       total,
		Success:     success,
		Fail:        fail,
		ActiveConns: int32(s.ActiveConns.Load()),
		P50:         p50,
		P99:         p99,
		Avg:         avg,
	}
}

// StatsSnapshot 统计快照
type StatsSnapshot struct {
	Total       uint64        `json:"total"`
	Success     uint64        `json:"success"`
	Fail        uint64        `json:"fail"`
	ActiveConns int32         `json:"active_conns"`
	P50         time.Duration `json:"p50_ms"`
	P99         time.Duration `json:"p99_ms"`
	Avg         time.Duration `json:"avg_ms"`
}

// SuccessRate 成功率
func (s StatsSnapshot) SuccessRate() float64 {
	if s.Total == 0 {
		return 0
	}
	return float64(s.Success) / float64(s.Total) * 100
}

// ReqPerSec 每秒请求数（基于给定时间窗口）
func (s StatsSnapshot) ReqPerSec(window time.Duration) float64 {
	sec := window.Seconds()
	if sec <= 0 {
		return 0
	}
	return float64(s.Total) / sec
}

// PrintLine 打印单行文本统计
func (s StatsSnapshot) PrintLine() string {
	return fmt.Sprintf("conns=%d  total=%d  succ=%d  fail=%d  rate=%.1f%%  qps=%.1f  p50=%.1fms  p99=%.1fms  avg=%.1fms",
		s.ActiveConns, s.Total, s.Success, s.Fail,
		s.SuccessRate(),
		math.Round(s.ReqPerSec(time.Second)*100)/100,
		float64(s.P50.Microseconds())/1000.0,
		float64(s.P99.Microseconds())/1000.0,
		float64(s.Avg.Microseconds())/1000.0,
	)
}
