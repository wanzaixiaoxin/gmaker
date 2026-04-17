package trace

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"fmt"
	"sync/atomic"
	"time"
)

// Key context key 类型，避免冲突
type Key struct{}

var traceKey Key

// Generate 生成新的 trace_id（16 字节随机 hex = 32 字符）
func Generate() string {
	b := make([]byte, 16)
	if _, err := rand.Read(b); err != nil {
		// 降级：timestamp + 计数器
		return fallbackID()
	}
	return hex.EncodeToString(b)
}

var fallbackCounter atomic.Uint64

func fallbackID() string {
	return fmt.Sprintf("%016x%08x", time.Now().UnixNano(), fallbackCounter.Add(1))
}

// WithContext 将 trace_id 注入 context
func WithContext(ctx context.Context, traceID string) context.Context {
	return context.WithValue(ctx, traceKey, traceID)
}

// FromContext 从 context 提取 trace_id
func FromContext(ctx context.Context) string {
	if v := ctx.Value(traceKey); v != nil {
		if s, ok := v.(string); ok {
			return s
		}
	}
	return ""
}

// Ensure 确保 context 中一定有 trace_id，没有则生成
func Ensure(ctx context.Context) (context.Context, string) {
	if id := FromContext(ctx); id != "" {
		return ctx, id
	}
	id := Generate()
	return WithContext(ctx, id), id
}
