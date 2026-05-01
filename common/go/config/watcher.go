package config

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"sync"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	configpb "github.com/gmaker/luffa/gen/go/config"
	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

// RedisWatcher 基于 Redis Pub/Sub 的配置变更监听器
type RedisWatcher struct {
	redisClient redis.UniversalClient
	handlers    map[string]func(event *configpb.ConfigChangeEvent)
	mu          sync.RWMutex
	subscriber  *redis.PubSub
	log         *logger.Logger
	namespace   string
}

// NewRedisWatcher 创建配置监听器
//   - client: 底层 go-redis 客户端（通过 redis.Client.RawClient() 获取）
//   - namespace: 配置命名空间，默认 "default"
func NewRedisWatcher(client redis.UniversalClient, namespace string) *RedisWatcher {
	if namespace == "" {
		namespace = "default"
	}
	return &RedisWatcher{
		redisClient: client,
		handlers:    make(map[string]func(event *configpb.ConfigChangeEvent)),
		namespace:   namespace,
	}
}

// SetLogger 设置日志器
func (w *RedisWatcher) SetLogger(log *logger.Logger) {
	w.log = log
}

func (w *RedisWatcher) infof(format string, v ...interface{}) {
	if w.log != nil {
		w.log.Infof(format, v...)
	} else {
		logger.Infof(format, v...)
	}
}

func (w *RedisWatcher) warnf(format string, v ...interface{}) {
	if w.log != nil {
		w.log.Warnf(format, v...)
	} else {
		logger.Warnf(format, v...)
	}
}

func (w *RedisWatcher) errorf(format string, v ...interface{}) {
	if w.log != nil {
		w.log.Errorf(format, v...)
	} else {
		logger.Errorf(format, v...)
	}
}

// Subscribe 注册对指定配置名称的变更监听
func (w *RedisWatcher) Subscribe(configName string, handler func(event *configpb.ConfigChangeEvent)) {
	w.mu.Lock()
	defer w.mu.Unlock()
	w.handlers[configName] = handler
}

// Unsubscribe 取消对指定配置的监听
func (w *RedisWatcher) Unsubscribe(configName string) {
	w.mu.Lock()
	defer w.mu.Unlock()
	delete(w.handlers, configName)
}

// Start 启动后台监听协程，阻塞直到 ctx 取消
// 调用方应在服务初始化完成后以 goroutine 方式启动：go watcher.Start(ctx)
func (w *RedisWatcher) Start(ctx context.Context) {
	if len(w.handlers) == 0 {
		if w.log != nil {
			w.log.Warn("[ConfigWatcher] no handlers registered, skip start")
		}
		return
	}

	channels := w.buildChannels()
	if w.log != nil {
		w.log.Infof("[ConfigWatcher] subscribing channels: %v", channels)
	}

	w.subscriber = w.redisClient.Subscribe(ctx, channels...)
	defer w.subscriber.Close()

	// 消费消息
	ch := w.subscriber.Channel()
	for {
		select {
		case <-ctx.Done():
			if w.log != nil {
				w.log.Info("[ConfigWatcher] stopped")
			}
			return
		case msg, ok := <-ch:
			if !ok {
				if w.log != nil {
					w.log.Warn("[ConfigWatcher] redis channel closed, reconnecting...")
				}
				// 简单重连：重新订阅
				w.subscriber.Close()
				w.subscriber = w.redisClient.Subscribe(ctx, channels...)
				ch = w.subscriber.Channel()
				continue
			}
			w.handleMessage(msg)
		}
	}
}

// Stop 停止监听（如未通过 ctx 取消，可显式调用）
func (w *RedisWatcher) Stop() error {
	if w.subscriber != nil {
		return w.subscriber.Close()
	}
	return nil
}

func (w *RedisWatcher) buildChannels() []string {
	w.mu.RLock()
	defer w.mu.RUnlock()
	channels := make([]string, 0, len(w.handlers)+1)
	for name := range w.handlers {
		channels = append(channels, fmt.Sprintf("pubsub:config:%s:%s", w.namespace, name))
	}
	// 同时订阅全量广播频道（用于补推/强制刷新）
	channels = append(channels, fmt.Sprintf("pubsub:config:%s:all", w.namespace))
	return channels
}

func (w *RedisWatcher) handleMessage(msg *redis.Message) {
	var event configpb.ConfigChangeEvent
	if err := proto.Unmarshal([]byte(msg.Payload), &event); err != nil {
		w.errorf("[ConfigWatcher] unmarshal event failed: %v", err)
		return
	}

	w.mu.RLock()
	handler, ok := w.handlers[event.ConfigName]
	w.mu.RUnlock()

	if !ok {
		// 可能是 all 频道广播，忽略未注册的配置
		return
	}

	w.infof("[ConfigWatcher] received change event: config=%s version=%d action=%s",
		event.ConfigName, event.VersionNo, event.Action)

	// 在独立 goroutine 中执行 handler，避免阻塞 Redis 消费
	go handler(&event)
}

// ==================== 拉取与重载工具 ====================

// PullAndReload 从 Config Service 拉取配置内容，校验 checksum，写入本地文件后触发重载
//   - serviceAddr: Config Service HTTP 地址，如 "http://127.0.0.1:8087"
//   - configName: 配置名称
//   - event: 变更事件（含 version_id 和 checksum）
//   - loader: 本地配置加载器（用于 Reload）
//   - filePath: 本地配置文件路径（拉取后覆盖写入）
func PullAndReload(serviceAddr, configName string, event *configpb.ConfigChangeEvent, loader *LocalLoader, filePath string) error {
	// 1. 拉取完整配置内容
	url := fmt.Sprintf("%s/api/configs/%s/pull?namespace=%s&version_id=%d",
		serviceAddr, configName, event.Namespace, event.VersionId)
	resp, err := http.Get(url)
	if err != nil {
		return fmt.Errorf("pull config failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("pull config returned status %d", resp.StatusCode)
	}

	var result struct {
		OK     bool `json:"ok"`
		Code   int  `json:"code"`
		Msg    string `json:"msg"`
		Data   struct {
			Content   string `json:"content"`
			Checksum  string `json:"checksum"`
			VersionID int64  `json:"version_id"`
			Version   int32  `json:"version"`
		} `json:"data"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return fmt.Errorf("decode pull response failed: %w", err)
	}
	if !result.OK {
		return fmt.Errorf("pull config error: %s", result.Msg)
	}

	// 2. 校验 checksum
	if result.Data.Checksum != event.Checksum {
		return fmt.Errorf("checksum mismatch: expected %s, got %s", event.Checksum, result.Data.Checksum)
	}

	// 3. 写入本地文件（原子覆盖）
	if filePath != "" {
		tmpPath := filePath + ".tmp"
		if err := os.WriteFile(tmpPath, []byte(result.Data.Content), 0644); err != nil {
			return fmt.Errorf("write temp config file failed: %w", err)
		}
		if err := os.Rename(tmpPath, filePath); err != nil {
			return fmt.Errorf("rename config file failed: %w", err)
		}
		logger.Infof("[ConfigWatcher] config file updated: %s", filePath)
	}

	// 4. 触发重载
	if loader != nil {
		// 直接更新内存数据并触发回调
		if err := loader.Reload(); err != nil {
			return fmt.Errorf("reload config failed: %w", err)
		}
		logger.Infof("[ConfigWatcher] config reloaded: %s v%d", configName, result.Data.Version)
	}

	return nil
}

// PullAndReloadAsync PullAndReload 的异步包装，带超时和错误日志
func PullAndReloadAsync(serviceAddr, configName string, event *configpb.ConfigChangeEvent, loader *LocalLoader, filePath string, timeout time.Duration) {
	go func() {
		ctx, cancel := context.WithTimeout(context.Background(), timeout)
		defer cancel()
		done := make(chan error, 1)
		go func() {
			done <- PullAndReload(serviceAddr, configName, event, loader, filePath)
		}()
		select {
		case err := <-done:
			if err != nil {
				logger.Errorf("[ConfigWatcher] async reload failed: %v", err)
			}
		case <-ctx.Done():
			logger.Errorf("[ConfigWatcher] async reload timeout: %s", configName)
		}
	}()
}
