package config

import (
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"sync"

	"gopkg.in/yaml.v3"
)

// Loader 配置加载器，支持 YAML 文件与 HTTP 热重载
type Loader struct {
	mu       sync.RWMutex
	path     string
	data     map[string]interface{}
	onReload func()
}

// NewLoader 创建配置加载器
func NewLoader(path string) *Loader {
	return &Loader{path: path, data: make(map[string]interface{})}
}

// SetOnReload 设置重载回调
func (l *Loader) SetOnReload(fn func()) {
	l.onReload = fn
}

// Load 从文件加载配置
func (l *Loader) Load() error {
	absPath, err := filepath.Abs(l.path)
	if err != nil {
		return err
	}
	f, err := os.Open(absPath)
	if err != nil {
		return err
	}
	defer f.Close()

	bytes, err := io.ReadAll(f)
	if err != nil {
		return err
	}

	l.mu.Lock()
	defer l.mu.Unlock()
	if err := yaml.Unmarshal(bytes, &l.data); err != nil {
		return err
	}
	log.Printf("[config] loaded from %s", absPath)
	return nil
}

// Reload 热重载配置
func (l *Loader) Reload() error {
	if err := l.Load(); err != nil {
		return err
	}
	if l.onReload != nil {
		l.onReload()
	}
	return nil
}

// GetString 读取字符串配置
func (l *Loader) GetString(key string) string {
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
func (l *Loader) GetInt(key string) int {
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
func (l *Loader) GetBool(key string) bool {
	l.mu.RLock()
	defer l.mu.RUnlock()
	if v, ok := l.data[key]; ok {
		if b, ok := v.(bool); ok {
			return b
		}
	}
	return false
}

// ServeReloadHTTP 启动 HTTP /admin/reload 服务（端口隔离）
func (l *Loader) ServeReloadHTTP(addr string) {
	mux := http.NewServeMux()
	mux.HandleFunc("/admin/reload", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "method not allowed", http.StatusMethodNotAllowed)
			return
		}
		if err := l.Reload(); err != nil {
			http.Error(w, err.Error(), http.StatusInternalServerError)
			return
		}
		w.Write([]byte("reload ok"))
	})
	go func() {
		log.Printf("[config] reload HTTP server started on %s", addr)
		if err := http.ListenAndServe(addr, mux); err != nil {
			log.Printf("[config] reload HTTP server error: %v", err)
		}
	}()
}
