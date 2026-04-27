package handler

import (
	"context"
	"encoding/json"
	"net/http"
	"strconv"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/services/login-go/internal/service"
)

// LoginHandler HTTP 登录/注册处理器
type LoginHandler struct {
	PlayerSvc *service.PlayerService
	GatewayAddr string
	Log       *logger.Logger
}

// RegisterReq 注册请求
type RegisterReq struct {
	Account  string `json:"account"`
	Password string `json:"password"`
}

// LoginReq 登录请求
type LoginReq struct {
	Account  string `json:"account"`
	Password string `json:"password"`
}

// LoginRes 登录响应
type LoginRes struct {
	Code        int    `json:"code"`
	Msg         string `json:"msg"`
	Token       string `json:"token,omitempty"`
	PlayerID    uint64 `json:"player_id,omitempty"`
	ExpireAt    uint64 `json:"expire_at,omitempty"`
	GatewayAddr string `json:"gateway_addr,omitempty"`
}

func writeJSON(w http.ResponseWriter, status int, v interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Methods", "POST, OPTIONS")
	w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(v)
}

// HandleRegister 处理注册
func (h *LoginHandler) HandleRegister(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodOptions {
		writeJSON(w, http.StatusOK, nil)
		return
	}
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, LoginRes{Code: 405, Msg: "method not allowed"})
		return
	}

	var req RegisterReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: 400, Msg: "bad request"})
		return
	}
	if req.Account == "" || req.Password == "" {
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: 400, Msg: "account and password required"})
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	// 检查账号是否已存在
	existing, _ := h.PlayerSvc.QueryPlayerByAccount(ctx, req.Account)
	if existing != nil {
		writeJSON(w, http.StatusOK, LoginRes{Code: 1001, Msg: "account already exists"})
		return
	}

	player, err := h.PlayerSvc.CreatePlayer(ctx, req.Account, req.Password)
	if err != nil {
		h.Log.Errorf("create player failed: %v", err)
		writeJSON(w, http.StatusOK, LoginRes{Code: 500, Msg: "internal error"})
		return
	}

	writeJSON(w, http.StatusOK, LoginRes{Code: 0, Msg: "ok", PlayerID: player.PlayerId})
}

// HandleLogin 处理登录
func (h *LoginHandler) HandleLogin(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodOptions {
		writeJSON(w, http.StatusOK, nil)
		return
	}
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, LoginRes{Code: 405, Msg: "method not allowed"})
		return
	}

	var req LoginReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: 400, Msg: "bad request"})
		return
	}
	if req.Account == "" || req.Password == "" {
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: 400, Msg: "account and password required"})
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	player, err := h.PlayerSvc.QueryPlayerByAccount(ctx, req.Account)
	if err != nil {
		writeJSON(w, http.StatusOK, LoginRes{Code: 1002, Msg: "account or password incorrect"})
		return
	}

	// 验证密码
	if service.HashPassword(req.Password) != player.Password {
		writeJSON(w, http.StatusOK, LoginRes{Code: 1002, Msg: "account or password incorrect"})
		return
	}

	token := service.GenerateToken(player.PlayerId)
	expireAt := uint64(time.Now().Add(24 * time.Hour).Unix())
	if err := h.PlayerSvc.SetToken(ctx, player.PlayerId, token, 24*3600); err != nil {
		h.Log.Errorf("set token failed: %v", err)
	}

	writeJSON(w, http.StatusOK, LoginRes{
		Code:        0,
		Msg:         "ok",
		Token:       token,
		PlayerID:    player.PlayerId,
		ExpireAt:    expireAt,
		GatewayAddr: h.GatewayAddr,
	})
}

// HandleVerifyToken 验证 Token
func (h *LoginHandler) HandleVerifyToken(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodOptions {
		writeJSON(w, http.StatusOK, nil)
		return
	}
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, LoginRes{Code: 405, Msg: "method not allowed"})
		return
	}

	var body map[string]string
	if err := json.NewDecoder(r.Body).Decode(&body); err != nil {
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: 400, Msg: "bad request"})
		return
	}

	token := body["token"]
	playerIDStr := body["player_id"]
	playerID, _ := strconv.ParseUint(playerIDStr, 10, 64)

	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	storedToken, err := h.PlayerSvc.GetToken(ctx, playerID)
	if err != nil || storedToken != token {
		writeJSON(w, http.StatusOK, LoginRes{Code: 1003, Msg: "invalid token"})
		return
	}

	writeJSON(w, http.StatusOK, LoginRes{Code: 0, Msg: "ok", PlayerID: playerID})
}
