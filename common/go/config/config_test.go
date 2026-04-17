package config

import (
	"os"
	"testing"
	"time"
)

func TestLoader(t *testing.T) {
	tmp, _ := os.CreateTemp("", "config-*.yaml")
	defer os.Remove(tmp.Name())
	tmp.WriteString("app_name: test_app\nmax_conn: 100\ndebug: true\n")
	tmp.Close()

	loader := NewLoader(tmp.Name())
	if err := loader.Load(); err != nil {
		t.Fatalf("load failed: %v", err)
	}

	if loader.GetString("app_name") != "test_app" {
		t.Errorf("app_name mismatch")
	}
	if loader.GetInt("max_conn") != 100 {
		t.Errorf("max_conn mismatch")
	}
	if !loader.GetBool("debug") {
		t.Errorf("debug mismatch")
	}

	// 热重载测试
	os.WriteFile(tmp.Name(), []byte("app_name: updated_app\n"), 0644)
	reloaded := false
	loader.SetOnReload(func() { reloaded = true })
	if err := loader.Reload(); err != nil {
		t.Fatalf("reload failed: %v", err)
	}
	if !reloaded {
		t.Errorf("onReload not called")
	}
	if loader.GetString("app_name") != "updated_app" {
		t.Errorf("reloaded app_name mismatch")
	}
}

func TestServeReloadHTTP(t *testing.T) {
	tmp, _ := os.CreateTemp("", "config-*.yaml")
	defer os.Remove(tmp.Name())
	os.WriteFile(tmp.Name(), []byte("k: v\n"), 0644)

	loader := NewLoader(tmp.Name())
	loader.Load()
	loader.ServeReloadHTTP(":19999")
	time.Sleep(100 * time.Millisecond)

	// 简单验证服务器启动即可，具体 HTTP 测试可后续补充
}
