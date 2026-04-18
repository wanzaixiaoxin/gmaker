package limiter

import (
	"testing"
	"time"
)

func TestCircuitBreakerStateTransition(t *testing.T) {
	cb := NewCircuitBreaker(3, 2, 1, 100*time.Millisecond)

	if cb.CurrentState() != StateClosed {
		t.Fatalf("expected initial state Closed, got %v", cb.CurrentState())
	}

	// 3 次失败触发熔断
	for i := 0; i < 3; i++ {
		if !cb.Allow() {
			t.Fatalf("expected allow at index %d", i)
		}
		cb.RecordFailure()
	}
	if cb.CurrentState() != StateOpen {
		t.Fatalf("expected Open after failures, got %v", cb.CurrentState())
	}
	if cb.Allow() {
		t.Fatal("expected reject when Open")
	}

	// 等待超时，进入 HalfOpen
	time.Sleep(150 * time.Millisecond)
	if !cb.Allow() {
		t.Fatal("expected allow when HalfOpen")
	}
	if cb.CurrentState() != StateHalfOpen {
		t.Fatalf("expected HalfOpen, got %v", cb.CurrentState())
	}

	// 2 次成功恢复 Closed
	cb.RecordSuccess()
	cb.RecordSuccess()
	if cb.CurrentState() != StateClosed {
		t.Fatalf("expected Closed after successes, got %v", cb.CurrentState())
	}
}

func TestCircuitBreakerHalfOpenFail(t *testing.T) {
	cb := NewCircuitBreaker(2, 2, 1, 100*time.Millisecond)
	cb.RecordFailure()
	cb.RecordFailure()
	if cb.CurrentState() != StateOpen {
		t.Fatal("expected Open")
	}

	time.Sleep(150 * time.Millisecond)
	cb.Allow()
	if cb.CurrentState() != StateHalfOpen {
		t.Fatal("expected HalfOpen")
	}
	cb.RecordFailure()
	if cb.CurrentState() != StateOpen {
		t.Fatal("expected back to Open after HalfOpen failure")
	}
}
