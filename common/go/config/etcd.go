package config

import (
	"context"
	"encoding/json"
	"log"
	"sync"
	"time"
)

// EtcdClient 抽象 etcd 客户端接口（解耦具体实现）
type EtcdClient interface {
	Get(ctx context.Context, key string) (string, error)
	Watch(ctx context.Context, key string, onChange func(string))
	Close() error
}

// EtcdLoader 基于 Etcd 的配置加载器
type EtcdLoader struct {
	client   EtcdClient
	key      string
	data     map[string]interface{}
	mu       sync.RWMutex
	onReload func()
	stopCh   chan struct{}
}

// NewEtcdLoader 创建 Etcd 配置加载器
func NewEtcdLoader(client EtcdClient, key string) *EtcdLoader {
	return &EtcdLoader{
		client: client,
		key:    key,
		data:   make(map[string]interface{}),
		stopCh: make(chan struct{}),
	}
}

// SetOnReload 设置重载回调
func (l *EtcdLoader) SetOnReload(fn func()) {
	l.onReload = fn
}

// Load 从 Etcd 加载配置
func (l *EtcdLoader) Load() error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	val, err := l.client.Get(ctx, l.key)
	if err != nil {
		return err
	}
	return l.parse(val)
}

// Watch 启动后台 Watch，配置变更自动重载
func (l *EtcdLoader) Watch(ctx context.Context) {
	l.client.Watch(ctx, l.key, func(newVal string) {
		if err := l.parse(newVal); err != nil {
			log.Printf("[config] etcd watch parse failed: %v", err)
			return
		}
		log.Printf("[config] etcd watch reload triggered")
		if l.onReload != nil {
			l.onReload()
		}
	})
}

// Stop 停止 Watch
func (l *EtcdLoader) Stop() {
	select {
	case <-l.stopCh:
		// 已关闭，避免重复 close 导致 panic
	default:
		close(l.stopCh)
	}
	l.client.Close()
}

// GetString 读取字符串配置
func (l *EtcdLoader) GetString(key string) string {
	l.mu.RLock()
	defer l.mu.RUnlock()
	if v, ok := l.data[key]; ok {
		if s, ok := v.(string); ok {
			return s
		}
	}
	return ""
}

// GetInt 读取整型配置
func (l *EtcdLoader) GetInt(key string) int {
	l.mu.RLock()
	defer l.mu.RUnlock()
	if v, ok := l.data[key]; ok {
		switch n := v.(type) {
		case int:
			return n
		case int64:
			return int(n)
		case float64:
			return int(n)
		}
	}
	return 0
}

// GetBool 读取布尔配置
func (l *EtcdLoader) GetBool(key string) bool {
	l.mu.RLock()
	defer l.mu.RUnlock()
	if v, ok := l.data[key]; ok {
		if b, ok := v.(bool); ok {
			return b
		}
	}
	return false
}

func (l *EtcdLoader) parse(val string) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	// 尝试解析为 JSON map；失败时保留原始字符串
	var m map[string]interface{}
	if err := json.Unmarshal([]byte(val), &m); err == nil {
		l.data = m
		return nil
	}
	l.data = map[string]interface{}{"raw": val}
	return nil
}

// ==================== GrayRelease 灰度发布 ====================

// GrayRule 灰度规则
type GrayRule struct {
	Region   string            `json:"region"`   // 匹配 region
	NodeID   string            `json:"node_id"`  // 匹配 node_id
	Tags     map[string]string `json:"tags"`     // 匹配标签
	Percent  int               `json:"percent"`  // 百分比灰度 (0-100)
}

// Match 判断当前节点是否匹配灰度规则
func (r *GrayRule) Match(region, nodeID string, tags map[string]string) bool {
	if r.Region != "" && r.Region != region {
		return false
	}
	if r.NodeID != "" && r.NodeID != nodeID {
		return false
	}
	for k, v := range r.Tags {
		if tags[k] != v {
			return false
		}
	}
	// 百分比灰度：使用 node_id hash 取模
	if r.Percent > 0 && r.Percent < 100 {
		hash := 0
		for _, c := range nodeID {
			hash = (hash*31 + int(c)) & 0x7FFFFFFF
		}
		if hash%100 >= r.Percent {
			return false
		}
	}
	return true
}

// GrayRelease 灰度发布管理器
type GrayRelease struct {
	mu     sync.RWMutex
	rules  []GrayRule
	active map[string]bool // node_id -> 是否启用新配置
}

// NewGrayRelease 创建灰度发布管理器
func NewGrayRelease() *GrayRelease {
	return &GrayRelease{active: make(map[string]bool)}
}

// UpdateRules 更新灰度规则
func (g *GrayRelease) UpdateRules(rules []GrayRule) {
	g.mu.Lock()
	defer g.mu.Unlock()
	g.rules = rules
}

// ShouldActivate 判断节点是否应该启用新配置
func (g *GrayRelease) ShouldActivate(region, nodeID string, tags map[string]string) bool {
	g.mu.RLock()
	defer g.mu.RUnlock()
	for _, rule := range g.rules {
		if rule.Match(region, nodeID, tags) {
			return true
		}
	}
	return false
}

// EtcdLoaderWithGray 带灰度发布的 Etcd 配置加载器
type EtcdLoaderWithGray struct {
	*EtcdLoader
	gray   *GrayRelease
	region string
	nodeID string
	tags   map[string]string
}

// NewEtcdLoaderWithGray 创建带灰度的配置加载器
func NewEtcdLoaderWithGray(client EtcdClient, key, region, nodeID string, tags map[string]string) *EtcdLoaderWithGray {
	return &EtcdLoaderWithGray{
		EtcdLoader: NewEtcdLoader(client, key),
		gray:       NewGrayRelease(),
		region:     region,
		nodeID:     nodeID,
		tags:       tags,
	}
}

// WatchGray 监听灰度规则变更
func (l *EtcdLoaderWithGray) WatchGray(ctx context.Context, grayKey string) {
	l.client.Watch(ctx, grayKey, func(newVal string) {
		log.Printf("[config] gray rules updated: %s", newVal)
		// TODO: 解析 JSON 为 GrayRule 列表
		// l.gray.UpdateRules(rules)
		// 如果当前节点被灰度命中，触发重载
		if l.gray.ShouldActivate(l.region, l.nodeID, l.tags) {
			if err := l.Load(); err == nil && l.onReload != nil {
				l.onReload()
			}
		}
	})
}
