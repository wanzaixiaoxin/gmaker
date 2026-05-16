package cache

import (
	"context"
	"fmt"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// ---------------------------------------------------------------------------
// 测试用结构体
// ---------------------------------------------------------------------------

type Player struct {
	UID   uint64 `db:"uid" cache:"key"`
	Name  string `db:"name"`
	Level int    `db:"level"`
	Coin  int64  `db:"coin"`
}

// ---------------------------------------------------------------------------
// Mock Store（同时实现 Store 和 BatchStore）
// ---------------------------------------------------------------------------

type mockStore struct {
	mu   sync.RWMutex
	data map[string]storeItem
}

type storeItem struct {
	value   string
	expireAt time.Time
}

func newMockStore() *mockStore {
	return &mockStore{data: make(map[string]storeItem)}
}

func (s *mockStore) Get(_ context.Context, key string) (string, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	item, ok := s.data[key]
	if !ok || (!item.expireAt.IsZero() && time.Now().After(item.expireAt)) {
		return "", fmt.Errorf("redis: nil")
	}
	return item.value, nil
}

func (s *mockStore) Set(_ context.Context, key string, value string, ttl time.Duration) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	item := storeItem{value: value}
	if ttl > 0 {
		item.expireAt = time.Now().Add(ttl)
	}
	s.data[key] = item
	return nil
}

func (s *mockStore) Del(_ context.Context, keys ...string) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	for _, k := range keys {
		delete(s.data, k)
	}
	return nil
}

func (s *mockStore) MGet(_ context.Context, keys ...string) ([]string, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	result := make([]string, len(keys))
	for i, k := range keys {
		item, ok := s.data[k]
		if !ok || (!item.expireAt.IsZero() && time.Now().After(item.expireAt)) {
			result[i] = ""
		} else {
			result[i] = item.value
		}
	}
	return result, nil
}

func (s *mockStore) MSet(_ context.Context, pairs map[string]string, ttl time.Duration) error {
	s.mu.Lock()
	defer s.mu.Unlock()
	for k, v := range pairs {
		item := storeItem{value: v}
		if ttl > 0 {
			item.expireAt = time.Now().Add(ttl)
		}
		s.data[k] = item
	}
	return nil
}

// ---------------------------------------------------------------------------
// Mock Loader
// ---------------------------------------------------------------------------

type mockLoader struct {
	mu        sync.RWMutex
	data      map[interface{}]Player
	loadCount atomic.Int32
}

func newMockLoader() *mockLoader {
	return &mockLoader{data: make(map[interface{}]Player)}
}

func (l *mockLoader) Add(p Player) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.data[p.UID] = p
}

func (l *mockLoader) Load(_ context.Context, key interface{}) (Player, error) {
	l.loadCount.Add(1)
	l.mu.RLock()
	defer l.mu.RUnlock()
	p, ok := l.data[key]
	if !ok {
		return Player{}, ErrNotFound{Key: fmt.Sprintf("%v", key)}
	}
	return p, nil
}

func (l *mockLoader) LoadBatch(_ context.Context, keys []interface{}) ([]Player, error) {
	l.loadCount.Add(1)
	l.mu.RLock()
	defer l.mu.RUnlock()
	var result []Player
	for _, k := range keys {
		if p, ok := l.data[k]; ok {
			result = append(result, p)
		}
	}
	return result, nil
}

// ---------------------------------------------------------------------------
// 辅助函数
// ---------------------------------------------------------------------------

func newTestManager(loader *mockLoader) *CacheManager[Player] {
	store := newMockStore()
	codec := JSONCodec[Player]{}
	cfg := ManagerConfig{
		Prefix:                  "player",
		TTL:                     30 * time.Minute,
		NilTTL:                  5 * time.Second,
		JitterPct:               0.1,
		EnableBreaker:           false,
		BreakerFailureThreshold: 5,
		BreakerSuccessThreshold: 3,
		BreakerTimeout:          10 * time.Second,
	}
	mgr := NewManager[Player](store, codec, loader, cfg)
	mgr.WithKeyFunc(func(p Player) interface{} { return p.UID })
	return mgr
}

// ---------------------------------------------------------------------------
// 测试用例
// ---------------------------------------------------------------------------

// TestManager_Get_HitCache 测试缓存命中
func TestManager_Get_HitCache(t *testing.T) {
	loader := newMockLoader()
	loader.Add(Player{UID: 1, Name: "Alice", Level: 10, Coin: 1000})
	mgr := newTestManager(loader)

	// 第一次：缓存 miss，从 loader 加载
	p, err := mgr.Get(context.Background(), uint64(1))
	if err != nil {
		t.Fatalf("Get failed: %v", err)
	}
	if p.Name != "Alice" {
		t.Fatalf("expected Alice, got %s", p.Name)
	}
	firstCount := loader.loadCount.Load()

	// 第二次：应命中缓存，不再调用 loader
	p, err = mgr.Get(context.Background(), uint64(1))
	if err != nil {
		t.Fatalf("Get failed: %v", err)
	}
	if loader.loadCount.Load() != firstCount {
		t.Fatalf("expected no additional loader call, got %d", loader.loadCount.Load())
	}
	_ = p
}

// TestManager_Get_PenetrationProtection 测试防穿透（空值占位符）
func TestManager_Get_PenetrationProtection(t *testing.T) {
	loader := newMockLoader() // 空 loader，所有 key 都不存在
	mgr := newTestManager(loader)

	// 第一次查询不存在的 key
	_, err := mgr.Get(context.Background(), uint64(999))
	if _, ok := err.(ErrNotFound); !ok {
		t.Fatalf("expected ErrNotFound, got %v", err)
	}
	firstCount := loader.loadCount.Load()
	if firstCount != 1 {
		t.Fatalf("expected 1 loader call, got %d", firstCount)
	}

	// 第二次查询同一个 key → 应命中空值占位符，不再调用 loader
	_, err = mgr.Get(context.Background(), uint64(999))
	if _, ok := err.(ErrNotFound); !ok {
		t.Fatalf("expected ErrNotFound, got %v", err)
	}
	if loader.loadCount.Load() != 1 {
		t.Fatalf("空值占位符未生效，loader 被调用 %d 次", loader.loadCount.Load())
	}
}

// TestManager_Get_ConcurrentStampede 测试防击穿（singleflight 合并并发请求）
func TestManager_Get_ConcurrentStampede(t *testing.T) {
	loader := newMockLoader()
	loader.Add(Player{UID: 1, Name: "Alice", Level: 10, Coin: 1000})
	mgr := newTestManager(loader)

	// 100 个并发请求同一个 key，loader 应只被调用 1 次（singleflight）
	var wg sync.WaitGroup
	var successCount atomic.Int32
	var errorCount atomic.Int32

	for i := 0; i < 100; i++ {
		wg.Add(1)
		go func() {
			defer wg.Done()
			p, err := mgr.Get(context.Background(), uint64(1))
			if err != nil {
				errorCount.Add(1)
				return
			}
			if p.Name != "Alice" {
				errorCount.Add(1)
				return
			}
			successCount.Add(1)
		}()
	}
	wg.Wait()

	if successCount.Load() != 100 {
		t.Fatalf("expected 100 successes, got %d (errors: %d)", successCount.Load(), errorCount.Load())
	}
	if loader.loadCount.Load() > 3 {
		t.Fatalf("singleflight 未生效，loader 被调用 %d 次", loader.loadCount.Load())
	}
}

// TestManager_GetBatch 测试批量获取
func TestManager_GetBatch(t *testing.T) {
	loader := newMockLoader()
	loader.Add(Player{UID: 1, Name: "Alice", Level: 10, Coin: 1000})
	loader.Add(Player{UID: 2, Name: "Bob", Level: 20, Coin: 2000})
	loader.Add(Player{UID: 3, Name: "Charlie", Level: 30, Coin: 3000})
	mgr := newTestManager(loader)

	keys := []interface{}{uint64(1), uint64(2), uint64(3), uint64(999)} // 999 不存在
	result, err := mgr.GetBatch(context.Background(), keys)
	if err != nil {
		t.Fatalf("GetBatch failed: %v", err)
	}

	if len(result) != 3 {
		t.Fatalf("expected 3 results, got %d", len(result))
	}
	if result[uint64(1)].Name != "Alice" {
		t.Fatalf("expected Alice, got %v", result[uint64(1)])
	}
	if result[uint64(2)].Name != "Bob" {
		t.Fatalf("expected Bob, got %v", result[uint64(2)])
	}
	if result[uint64(3)].Name != "Charlie" {
		t.Fatalf("expected Charlie, got %v", result[uint64(3)])
	}
	if _, ok := result[uint64(999)]; ok {
		t.Fatal("key 999 should not exist in result")
	}
}

// TestManager_Delete 测试缓存删除
func TestManager_Delete(t *testing.T) {
	loader := newMockLoader()
	loader.Add(Player{UID: 1, Name: "Alice", Level: 10, Coin: 1000})
	mgr := newTestManager(loader)

	// 加载到缓存
	_, _ = mgr.Get(context.Background(), uint64(1))
	firstCount := loader.loadCount.Load()

	// 删除缓存
	_ = mgr.Delete(context.Background(), uint64(1))

	// 再次获取 → 缓存 miss → 重新加载
	_, err := mgr.Get(context.Background(), uint64(1))
	if err != nil {
		t.Fatalf("Get after delete failed: %v", err)
	}
	if loader.loadCount.Load() <= firstCount {
		t.Fatal("expected loader to be called again after delete")
	}
}

// TestManager_Refresh 测试强制刷新
func TestManager_Refresh(t *testing.T) {
	loader := newMockLoader()
	loader.Add(Player{UID: 1, Name: "Alice", Level: 10, Coin: 1000})
	mgr := newTestManager(loader)

	// 加载到缓存
	p, _ := mgr.Get(context.Background(), uint64(1))
	if p.Level != 10 {
		t.Fatalf("expected level 10, got %d", p.Level)
	}

	// 更新 loader 数据
	loader.Add(Player{UID: 1, Name: "Alice", Level: 20, Coin: 2000})

	// 刷新
	p, err := mgr.Refresh(context.Background(), uint64(1))
	if err != nil {
		t.Fatalf("Refresh failed: %v", err)
	}
	if p.Level != 20 {
		t.Fatalf("expected level 20 after refresh, got %d", p.Level)
	}
}

// TestManager_JitterTTL 测试 TTL 抖动（防雪崩）
func TestManager_JitterTTL(t *testing.T) {
	loader := newMockLoader()
	mgr := newTestManager(loader)
	mgr.config.TTL = 100 * time.Second
	mgr.config.JitterPct = 0.2

	ttls := make(map[time.Duration]bool)
	for i := 0; i < 100; i++ {
		ttl := mgr.jitterTTL()
		// TTL 应在 [100s, 120s) 范围内
		if ttl < 100*time.Second || ttl >= 120*time.Second {
			t.Fatalf("jitter TTL out of range: %v", ttl)
		}
		ttls[ttl] = true
	}

	// 应该有多种不同的 TTL 值（抖动生效）
	if len(ttls) < 50 {
		t.Fatalf("jitter not producing enough variety: %d unique TTLs", len(ttls))
	}
}

// TestManager_SetAndGet 测试手动 Set + Get
func TestManager_SetAndGet(t *testing.T) {
	loader := newMockLoader()
	mgr := newTestManager(loader)

	p := Player{UID: 42, Name: "Manual", Level: 99, Coin: 99999}
	err := mgr.Set(context.Background(), uint64(42), p)
	if err != nil {
		t.Fatalf("Set failed: %v", err)
	}

	got, err := mgr.Get(context.Background(), uint64(42))
	if err != nil {
		t.Fatalf("Get failed: %v", err)
	}
	if got.Name != "Manual" {
		t.Fatalf("expected Manual, got %s", got.Name)
	}
	// Set 不应触发 loader
	if loader.loadCount.Load() != 0 {
		t.Fatal("Set should not trigger loader")
	}
}
