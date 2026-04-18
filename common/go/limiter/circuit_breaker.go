package limiter

import (
	"sync"
	"sync/atomic"
	"time"
)

// State 熔断器状态
type State int32

const (
	StateClosed    State = iota // 正常关闭，请求通过
	StateOpen                   // 熔断打开，拒绝请求
	StateHalfOpen               // 半开，允许探测请求
)

// CircuitBreaker 错误率计数熔断器
type CircuitBreaker struct {
	mu sync.RWMutex

	failureThreshold    int           // 触发熔断的连续错误次数
	successThreshold    int           // 半开状态下恢复所需的连续成功次数
	halfOpenMaxRequests int           // 半开状态下允许的最大并发探测数
	timeout             time.Duration // 熔断持续时间

	state        atomic.Int32
	failures     atomic.Int32
	successes    atomic.Int32
	halfOpenReqs atomic.Int32 // 半开状态下当前在执行的请求数
	lastFailure  time.Time
}

// NewCircuitBreaker 创建熔断器
func NewCircuitBreaker(failureThreshold, successThreshold, halfOpenMaxRequests int, timeout time.Duration) *CircuitBreaker {
	if halfOpenMaxRequests <= 0 {
		halfOpenMaxRequests = 1
	}
	cb := &CircuitBreaker{
		failureThreshold:    failureThreshold,
		successThreshold:    successThreshold,
		halfOpenMaxRequests: halfOpenMaxRequests,
		timeout:             timeout,
	}
	cb.state.Store(int32(StateClosed))
	return cb
}

// State 返回当前状态
func (cb *CircuitBreaker) CurrentState() State {
	return State(cb.state.Load())
}

// Allow 判断是否允许请求通过
func (cb *CircuitBreaker) Allow() bool {
	st := cb.CurrentState()
	if st == StateClosed {
		return true
	}
	if st == StateOpen {
		cb.mu.RLock()
		canRetry := time.Since(cb.lastFailure) > cb.timeout
		cb.mu.RUnlock()
		if canRetry {
			if cb.state.CompareAndSwap(int32(StateOpen), int32(StateHalfOpen)) {
				cb.failures.Store(0)
				cb.successes.Store(0)
				return true
			}
		}
		return false
	}
	// HalfOpen: 限制并发探测数
	if int(cb.halfOpenReqs.Add(1)) > cb.halfOpenMaxRequests {
		cb.halfOpenReqs.Add(-1)
		return false
	}
	return true
}

// RecordSuccess 记录成功
func (cb *CircuitBreaker) RecordSuccess() {
	st := cb.CurrentState()
	if st == StateHalfOpen {
		cb.halfOpenReqs.Add(-1)
		if cb.successes.Add(1) >= int32(cb.successThreshold) {
			cb.state.Store(int32(StateClosed))
			cb.failures.Store(0)
			cb.successes.Store(0)
		}
	} else if st == StateClosed {
		cb.failures.Store(0)
	}
}

// RecordFailure 记录失败
func (cb *CircuitBreaker) RecordFailure() {
	st := cb.CurrentState()
	if st == StateHalfOpen {
		cb.halfOpenReqs.Add(-1)
		cb.transitionToOpen()
		return
	}
	if st == StateClosed {
		if cb.failures.Add(1) >= int32(cb.failureThreshold) {
			cb.transitionToOpen()
		}
	}
}

func (cb *CircuitBreaker) transitionToOpen() {
	cb.mu.Lock()
	cb.lastFailure = time.Now()
	cb.mu.Unlock()
	cb.state.Store(int32(StateOpen))
	cb.successes.Store(0)
}
