package api

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strconv"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/services/bot-go/internal/bot"
)

// Server HTTP 管理 API
type Server struct {
	Manager *bot.Manager
	Log     *logger.Logger
}

// NewServer 创建 API 服务器
func NewServer(mgr *bot.Manager, log *logger.Logger) *Server {
	return &Server{Manager: mgr, Log: log}
}

// RegisterRoutes 注册路由
func (s *Server) RegisterRoutes(mux *http.ServeMux) {
	mux.HandleFunc("/api/v1/status", s.handleStatus)
	mux.HandleFunc("/api/v1/start", s.handleStart)
	mux.HandleFunc("/api/v1/stop", s.handleStop)
	mux.HandleFunc("/api/v1/send", s.handleSend)
}

func (s *Server) writeJSON(w http.ResponseWriter, code int, v interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(v)
}

func (s *Server) handleStatus(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodGet {
		s.writeJSON(w, http.StatusMethodNotAllowed, map[string]interface{}{"code": 405, "msg": "method not allowed"})
		return
	}
	stats := s.Manager.Stats()
	stats["code"] = 0
	stats["msg"] = "ok"
	s.writeJSON(w, http.StatusOK, stats)
}

// handleStart 启动所有 bot（支持 query 参数覆盖批量配置）
//
// POST /api/v1/start?batch_size=5&batch_interval_ms=1000
func (s *Server) handleStart(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		s.writeJSON(w, http.StatusMethodNotAllowed, map[string]interface{}{"code": 405, "msg": "method not allowed"})
		return
	}

	// 从 query 参数读取可选的批量覆盖配置
	if v := r.URL.Query().Get("batch_size"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 {
			s.Manager.OverrideBatchSize(n)
		}
	}
	if v := r.URL.Query().Get("batch_interval_ms"); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 {
			s.Manager.OverrideBatchInterval(n)
		}
	}

	s.Manager.Start()

	cfg := s.Manager.GetConfig()
	s.writeJSON(w, http.StatusOK, map[string]interface{}{
		"code":              0,
		"msg":               "bots starting in batches",
		"batch_size":        cfg.BatchSize,
		"batch_interval_ms": cfg.BatchIntervalMs,
		"connect_timeout":   fmt.Sprintf("%dms", cfg.ConnectTimeoutMs),
		"retry_delay":       fmt.Sprintf("%dms", cfg.RetryDelayMs),
		"max_retries":       cfg.MaxRetries,
	})
}

func (s *Server) handleStop(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		s.writeJSON(w, http.StatusMethodNotAllowed, map[string]interface{}{"code": 405, "msg": "method not allowed"})
		return
	}
	s.Manager.Stop()
	s.writeJSON(w, http.StatusOK, map[string]interface{}{"code": 0, "msg": "bots stopped"})
}

func (s *Server) handleSend(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		s.writeJSON(w, http.StatusMethodNotAllowed, map[string]interface{}{"code": 405, "msg": "method not allowed"})
		return
	}

	content := r.URL.Query().Get("content")
	botIDStr := r.URL.Query().Get("bot_id")

	if botIDStr != "" {
		botID, err := strconv.Atoi(botIDStr)
		if err != nil {
			s.writeJSON(w, http.StatusBadRequest, map[string]interface{}{"code": 400, "msg": "invalid bot_id"})
			return
		}
		if err := s.Manager.SendRandomMessage(botID); err != nil {
			s.writeJSON(w, http.StatusOK, map[string]interface{}{"code": 500, "msg": err.Error()})
			return
		}
		s.writeJSON(w, http.StatusOK, map[string]interface{}{"code": 0, "msg": "sent", "bot_id": botID})
		return
	}

	s.Manager.SendMessageToAll(content)
	s.writeJSON(w, http.StatusOK, map[string]interface{}{"code": 0, "msg": "sent to all bots"})
}
