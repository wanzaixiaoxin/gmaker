package logger

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"sync"
	"time"
)

// Level 日志级别
type Level int

const (
	DebugLevel Level = iota
	InfoLevel
	WarnLevel
	ErrorLevel
	FatalLevel
)

func (l Level) String() string {
	switch l {
	case DebugLevel:
		return "DEBUG"
	case InfoLevel:
		return "INFO"
	case WarnLevel:
		return "WARN"
	case ErrorLevel:
		return "ERROR"
	case FatalLevel:
		return "FATAL"
	default:
		return "UNKNOWN"
	}
}

// Logger 结构化 JSON 日志器
type Logger struct {
	mu       sync.RWMutex
	out      io.Writer
	level    Level
	service  string
	nodeID   string
	fields   map[string]interface{}
	writeCh  chan []byte
	stopCh   chan struct{}
	wg       sync.WaitGroup
}

// Config 日志配置
type Config struct {
	Level   string
	Service string
	NodeID  string
	Output  io.Writer
}

// New 创建 Logger
func New(cfg Config) *Logger {
	if cfg.Output == nil {
		cfg.Output = os.Stdout
	}
	lvl := parseLevel(cfg.Level)
	l := &Logger{
		out:     cfg.Output,
		level:   lvl,
		service: cfg.Service,
		nodeID:  cfg.NodeID,
		fields:  make(map[string]interface{}),
		writeCh: make(chan []byte, 256),
		stopCh:  make(chan struct{}),
	}
	l.wg.Add(1)
	go l.writeLoop()
	return l
}

// NewWithService 快捷创建，只指定服务名
func NewWithService(service, nodeID string) *Logger {
	return New(Config{Level: "info", Service: service, NodeID: nodeID})
}

// InitServiceLogger 根据命令行参数初始化服务日志
// 当 logFile 非空时，日志同时输出到控制台和文件（tee 模式）
func InitServiceLogger(service, nodeID, logLevel, logFile string) *Logger {
	var output io.Writer = os.Stdout
	if logFile != "" {
		f, err := os.OpenFile(logFile, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
		if err == nil {
			output = io.MultiWriter(os.Stdout, f)
		}
	}
	return New(Config{Level: logLevel, Service: service, NodeID: nodeID, Output: output})
}

// SetLevel 动态调整日志级别
func (l *Logger) SetLevel(level string) {
	l.mu.Lock()
	defer l.mu.Unlock()
	l.level = parseLevel(level)
}

// With 返回一个带有固定字段的子 Logger（线程安全，复制字段）
func (l *Logger) With(key string, value interface{}) *Logger {
	l.mu.RLock()
	defer l.mu.RUnlock()
	child := &Logger{
		out:     l.out,
		level:   l.level,
		service: l.service,
		nodeID:  l.nodeID,
		fields:  make(map[string]interface{}, len(l.fields)+1),
		writeCh: l.writeCh,
		stopCh:  l.stopCh,
	}
	for k, v := range l.fields {
		child.fields[k] = v
	}
	child.fields[key] = value
	return child
}

// WithTrace 返回绑定 trace_id 的子 Logger
func (l *Logger) WithTrace(traceID string) *Logger {
	return l.With("trace_id", traceID)
}

func (l *Logger) writeLoop() {
	defer l.wg.Done()
	for {
		select {
		case data := <-l.writeCh:
			l.out.Write(data)
			l.out.Write([]byte{'\n'})
		case <-l.stopCh:
			// 排空 channel
			for {
				select {
				case data := <-l.writeCh:
					l.out.Write(data)
					l.out.Write([]byte{'\n'})
				default:
					return
				}
			}
		}
	}
}

func (l *Logger) log(level Level, msg string, extra map[string]interface{}) {
	if level < l.level {
		return
	}
	l.mu.RLock()
	record := map[string]interface{}{
		"time":    time.Now().UTC().Format(time.RFC3339Nano),
		"level":   level.String(),
		"service": l.service,
		"node_id": l.nodeID,
		"msg":     msg,
	}
	for k, v := range l.fields {
		record[k] = v
	}
	for k, v := range extra {
		record[k] = v
	}
	l.mu.RUnlock()

	data, err := json.Marshal(record)
	if err != nil {
		log.Printf("[logger] marshal error: %v", err)
		return
	}
	select {
	case l.writeCh <- data:
	default:
		// channel 满，同步写入避免丢日志（仅用于极端情况）
		l.out.Write(data)
		l.out.Write([]byte{'\n'})
	}
}

// Stop 停止日志后台 goroutine 并刷新剩余日志
func (l *Logger) Stop() {
	close(l.stopCh)
	l.wg.Wait()
}

// Flush 等待日志 channel 排空（简单休眠方式）
func (l *Logger) Flush() {
	time.Sleep(50 * time.Millisecond)
}

func (l *Logger) Debug(msg string)                 { l.log(DebugLevel, msg, nil) }
func (l *Logger) Info(msg string)                  { l.log(InfoLevel, msg, nil) }
func (l *Logger) Warn(msg string)                  { l.log(WarnLevel, msg, nil) }
func (l *Logger) Error(msg string)                 { l.log(ErrorLevel, msg, nil) }
func (l *Logger) Fatal(msg string)                 { l.log(FatalLevel, msg, nil); l.Stop(); os.Exit(1) }
func (l *Logger) Debugf(format string, v ...interface{}) { l.Debug(fmt.Sprintf(format, v...)) }
func (l *Logger) Infof(format string, v ...interface{})  { l.Info(fmt.Sprintf(format, v...)) }
func (l *Logger) Warnf(format string, v ...interface{})  { l.Warn(fmt.Sprintf(format, v...)) }
func (l *Logger) Errorf(format string, v ...interface{}) { l.Error(fmt.Sprintf(format, v...)) }
func (l *Logger) Fatalf(format string, v ...interface{}) { l.Fatal(fmt.Sprintf(format, v...)) }

// Infow / Debugw / Warnw / Errorw 支持结构化字段的日志方法
func (l *Logger) Infow(msg string, extra map[string]interface{})  { l.log(InfoLevel, msg, extra) }
func (l *Logger) Debugw(msg string, extra map[string]interface{}) { l.log(DebugLevel, msg, extra) }
func (l *Logger) Warnw(msg string, extra map[string]interface{})  { l.log(WarnLevel, msg, extra) }
func (l *Logger) Errorw(msg string, extra map[string]interface{}) { l.log(ErrorLevel, msg, extra) }

func parseLevel(s string) Level {
	switch s {
	case "debug":
		return DebugLevel
	case "info":
		return InfoLevel
	case "warn", "warning":
		return WarnLevel
	case "error":
		return ErrorLevel
	case "fatal":
		return FatalLevel
	default:
		return InfoLevel
	}
}

// 全局默认 Logger（包级函数，兼容旧代码）
var defaultLogger = New(Config{Level: "info", Service: "unknown", Output: os.Stdout})

func SetDefault(l *Logger) { defaultLogger = l }
func Debug(msg string)     { defaultLogger.Debug(msg) }
func Info(msg string)      { defaultLogger.Info(msg) }
func Warn(msg string)      { defaultLogger.Warn(msg) }
func Error(msg string)     { defaultLogger.Error(msg) }
func Fatal(msg string)     { defaultLogger.Fatal(msg) }
func Debugf(format string, v ...interface{}) { defaultLogger.Debugf(format, v...) }
func Infof(format string, v ...interface{})  { defaultLogger.Infof(format, v...) }
func Warnf(format string, v ...interface{})  { defaultLogger.Warnf(format, v...) }
func Errorf(format string, v ...interface{}) { defaultLogger.Errorf(format, v...) }
func Fatalf(format string, v ...interface{}) { defaultLogger.Fatalf(format, v...) }
