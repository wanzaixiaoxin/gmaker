package replay

import (
	"testing"
	"time"
)

func TestReplayChecker(t *testing.T) {
	checker := NewChecker(5 * time.Second)

	ts := time.Now()
	if err := checker.Check(ts, "nonce1"); err != nil {
		t.Fatalf("expected first check to pass: %v", err)
	}

	// 相同 nonce 应该被拒绝
	if err := checker.Check(ts, "nonce1"); err == nil {
		t.Fatal("expected duplicate nonce to be rejected")
	}

	// 不同 nonce 应该通过
	if err := checker.Check(ts, "nonce2"); err != nil {
		t.Fatalf("expected different nonce to pass: %v", err)
	}

	// 未来时间超过 30 秒应该被拒绝
	if err := checker.Check(ts.Add(31*time.Second), "nonce3"); err == nil {
		t.Fatal("expected future timestamp to be rejected")
	}
}
