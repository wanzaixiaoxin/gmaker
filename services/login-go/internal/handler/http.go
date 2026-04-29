package handler

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/gmaker/luffa/common/go/errors"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/services/login-go/internal/service"
)

// LoginHandler HTTP 登录/注册处理器
type LoginHandler struct {
	PlayerSvc    *service.PlayerService
	GatewayAddr  string
	GatewayAddrs []string
	Log          *logger.Logger
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
	Code         int      `json:"code"`
	Msg          string   `json:"msg"`
	Token        string   `json:"token,omitempty"`
	RefreshToken string   `json:"refresh_token,omitempty"`
	PlayerID     uint64   `json:"player_id,omitempty"`
	ExpireAt     uint64   `json:"expire_at,omitempty"`
	GatewayAddr  string   `json:"gateway_addr,omitempty"`
	GatewayAddrs []string `json:"gateway_addrs,omitempty"`
}

// RefreshReq 刷新 Token 请求
type RefreshReq struct {
	PlayerID     uint64 `json:"player_id"`
	RefreshToken string `json:"refresh_token"`
}

// RefreshRes 刷新 Token 响应
type RefreshRes struct {
	Code         int    `json:"code"`
	Msg          string `json:"msg"`
	Token        string `json:"token,omitempty"`
	RefreshToken string `json:"refresh_token,omitempty"`
	ExpireAt     uint64 `json:"expire_at,omitempty"`
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
		h.Log.Warnw("register decode failed", map[string]interface{}{"client_ip": r.RemoteAddr, "error": err.Error()})
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: int(errors.INVALID_PARAM), Msg: "bad request"})
		return
	}
	if err := service.ValidateInput(req.Account, req.Password); err != nil {
		h.Log.Warnw("register param invalid", map[string]interface{}{"client_ip": r.RemoteAddr, "account": req.Account, "error": err.Error()})
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: int(errors.INVALID_PARAM), Msg: err.Error()})
		return
	}

	start := time.Now()
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	// 检查账号是否已存在
	h.Log.Infow("register request", map[string]interface{}{"account": req.Account, "client_ip": r.RemoteAddr})

	existing, _ := h.PlayerSvc.QueryPlayerByAccount(ctx, req.Account)
	if existing != nil {
		h.Log.Infow("register account exists", map[string]interface{}{"account": req.Account, "duration_ms": time.Since(start).Milliseconds()})
		writeJSON(w, http.StatusOK, LoginRes{Code: int(errors.ACCOUNT_EXISTS), Msg: "account already exists"})
		return
	}

	player, err := h.PlayerSvc.CreatePlayer(ctx, req.Account, req.Password)
	if err != nil {
		h.Log.Errorw("create player failed", map[string]interface{}{"account": req.Account, "error": err.Error()})
		writeJSON(w, http.StatusOK, LoginRes{Code: int(errors.INTERNAL_ERROR), Msg: "internal error"})
		return
	}

	h.Log.Infow("register success", map[string]interface{}{"account": req.Account, "player_id": player.PlayerId, "duration_ms": time.Since(start).Milliseconds()})
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
		h.Log.Warnw("login decode failed", map[string]interface{}{"client_ip": r.RemoteAddr, "error": err.Error()})
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: int(errors.INVALID_PARAM), Msg: "bad request"})
		return
	}
	if req.Account == "" || req.Password == "" {
		h.Log.Warnw("login param invalid", map[string]interface{}{"client_ip": r.RemoteAddr})
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: 400, Msg: "account and password required"})
		return
	}

	start := time.Now()
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	h.Log.Infow("login request", map[string]interface{}{"account": req.Account, "client_ip": r.RemoteAddr})

	player, err := h.PlayerSvc.QueryPlayerByAccount(ctx, req.Account)
	if err != nil {
		h.Log.Infow("login player not found", map[string]interface{}{"account": req.Account, "duration_ms": time.Since(start).Milliseconds()})
		writeJSON(w, http.StatusOK, LoginRes{Code: int(errors.AUTH_FAILED), Msg: "account or password incorrect"})
		return
	}

	// 验证密码（支持 Argon2id + 旧 SHA256 自动升级）
	ok, needUpgrade := service.VerifyPassword(req.Password, player.Password)
	if !ok {
		h.Log.Infow("login password wrong", map[string]interface{}{"account": req.Account, "player_id": player.PlayerId, "duration_ms": time.Since(start).Milliseconds()})
		writeJSON(w, http.StatusOK, LoginRes{Code: int(errors.AUTH_FAILED), Msg: "account or password incorrect"})
		return
	}
	// 如果是旧 SHA256 密码，自动升级为 Argon2id
	if needUpgrade {
		h.Log.Infow("auto upgrading password hash", map[string]interface{}{"account": req.Account, "player_id": player.PlayerId})
		_ = h.PlayerSvc.UpdatePassword(ctx, player.PlayerId, req.Password)
	}

	accessToken := service.GenerateToken()
	refreshToken := service.GenerateRefreshToken()
	expireAt := uint64(time.Now().Add(15 * time.Minute).Unix())
	if err := h.PlayerSvc.SetToken(ctx, player.PlayerId, accessToken, 15*60); err != nil {
		h.Log.Errorw("set access token failed", map[string]interface{}{"player_id": player.PlayerId, "error": err.Error()})
	}
	if err := h.PlayerSvc.SetRefreshToken(ctx, player.PlayerId, refreshToken, 7*24*3600); err != nil {
		h.Log.Errorw("set refresh token failed", map[string]interface{}{"player_id": player.PlayerId, "error": err.Error()})
	}

	h.Log.Infow("login success", map[string]interface{}{"account": req.Account, "player_id": player.PlayerId, "duration_ms": time.Since(start).Milliseconds()})
	writeJSON(w, http.StatusOK, LoginRes{
		Code:         int(errors.OK),
		Msg:          "ok",
		Token:        accessToken,
		RefreshToken: refreshToken,
		PlayerID:     player.PlayerId,
		ExpireAt:     expireAt,
		GatewayAddr:  h.GatewayAddr,
		GatewayAddrs: h.GatewayAddrs,
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
		writeJSON(w, http.StatusBadRequest, LoginRes{Code: int(errors.INVALID_PARAM), Msg: "bad request"})
		return
	}

	token := body["token"]
	playerIDStr := body["player_id"]
	playerID, _ := strconv.ParseUint(playerIDStr, 10, 64)

	start := time.Now()
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	storedToken, err := h.PlayerSvc.GetToken(ctx, playerID)
	if err != nil || storedToken != token {
		h.Log.Infow("verify token failed", map[string]interface{}{"player_id": playerID, "error": fmt.Sprintf("%v", err), "duration_ms": time.Since(start).Milliseconds()})
		writeJSON(w, http.StatusOK, LoginRes{Code: int(errors.TOKEN_INVALID), Msg: "invalid token"})
		return
	}

	h.Log.Infow("verify token success", map[string]interface{}{"player_id": playerID, "duration_ms": time.Since(start).Milliseconds()})
	writeJSON(w, http.StatusOK, LoginRes{Code: int(errors.OK), Msg: "ok", PlayerID: playerID})
}

// HandleRefreshToken 刷新 Access Token
func (h *LoginHandler) HandleRefreshToken(w http.ResponseWriter, r *http.Request) {
	if r.Method == http.MethodOptions {
		writeJSON(w, http.StatusOK, nil)
		return
	}
	if r.Method != http.MethodPost {
		writeJSON(w, http.StatusMethodNotAllowed, RefreshRes{Code: 405, Msg: "method not allowed"})
		return
	}

	var req RefreshReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeJSON(w, http.StatusBadRequest, RefreshRes{Code: int(errors.INVALID_PARAM), Msg: "bad request"})
		return
	}

	start := time.Now()
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	storedToken, err := h.PlayerSvc.GetRefreshToken(ctx, req.PlayerID)
	if err != nil || storedToken != req.RefreshToken {
		h.Log.Infow("refresh token failed", map[string]interface{}{"player_id": req.PlayerID, "error": fmt.Sprintf("%v", err), "duration_ms": time.Since(start).Milliseconds()})
		writeJSON(w, http.StatusOK, RefreshRes{Code: int(errors.TOKEN_INVALID), Msg: "invalid refresh token"})
		return
	}

	newAccessToken := service.GenerateToken()
	newRefreshToken := service.GenerateRefreshToken()
	expireAt := uint64(time.Now().Add(15 * time.Minute).Unix())
	_ = h.PlayerSvc.SetToken(ctx, req.PlayerID, newAccessToken, 15*60)
	_ = h.PlayerSvc.SetRefreshToken(ctx, req.PlayerID, newRefreshToken, 7*24*3600)

	h.Log.Infow("refresh token success", map[string]interface{}{"player_id": req.PlayerID, "duration_ms": time.Since(start).Milliseconds()})
	writeJSON(w, http.StatusOK, RefreshRes{
		Code:         int(errors.OK),
		Msg:          "ok",
		Token:        newAccessToken,
		RefreshToken: newRefreshToken,
		ExpireAt:     expireAt,
	})
}
