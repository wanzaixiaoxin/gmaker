package config

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"sync"
)

// LocalLoader 本地文件配置加载器
type LocalLoader struct {
	path     string
	data     map[string]interface{}
	mu       sync.RWMutex
	onReload func()
}

// NewLoader 创建本地文件配置加载器（兼容 config_test.go）
func NewLoader(path string) *LocalLoader {
	return &LocalLoader{
		path: path,
		data: make(map[string]interface{}),
	}
}

// Load 从文件加载配置
func (l *LocalLoader) Load() error {
	data, err := os.ReadFile(l.path)
	if err != nil {
		return fmt.Errorf("read config file: %w", err)
	}
	return l.parse(data)
}

// Reload 重新加载配置并触发回调
func (l *LocalLoader) Reload() error {
	if err := l.Load(); err != nil {
		return err
	}
	if l.onReload != nil {
		l.onReload()
	}
	return nil
}

func (l *LocalLoader) parse(data []byte) error {
	l.mu.Lock()
	defer l.mu.Unlock()
	var m map[string]interface{}
	if err := json.Unmarshal(data, &m); err != nil {
		return fmt.Errorf("parse config: %w", err)
	}
	l.data = m
	return nil
}

// GetString 读取字符串配置
func (l *LocalLoader) GetString(key string) string {
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
func (l *LocalLoader) GetInt(key string) int {
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
func (l *LocalLoader) GetBool(key string) bool {
	l.mu.RLock()
	defer l.mu.RUnlock()
	if v, ok := l.data[key]; ok {
		if b, ok := v.(bool); ok {
			return b
		}
	}
	return false
}

// SetOnReload 设置重载回调
func (l *LocalLoader) SetOnReload(fn func()) {
	l.onReload = fn
}

// ServeReloadHTTP 启动 HTTP 热重载端点
func (l *LocalLoader) ServeReloadHTTP(addr string) {
	http.HandleFunc("/admin/reload", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			w.WriteHeader(http.StatusMethodNotAllowed)
			return
		}
		if err := l.Reload(); err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		w.WriteHeader(http.StatusOK)
		w.Write([]byte("reloaded"))
	})
	go http.ListenAndServe(addr, nil)
}
