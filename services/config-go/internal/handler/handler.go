package handler

import (
	"context"
	"encoding/json"
	"fmt"
	"net/http"
	"strconv"
	"strings"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/services/config-go/internal/store"

	configpb "github.com/gmaker/luffa/gen/go/config"
	"google.golang.org/protobuf/proto"
)

// ConfigHandler HTTP API 处理器
type ConfigHandler struct {
	store       *store.ConfigStore
	redisClient *redis.Client
	log         *logger.Logger
}

// NewConfigHandler 创建处理器
func NewConfigHandler(store *store.ConfigStore, redisClient *redis.Client, log *logger.Logger) *ConfigHandler {
	return &ConfigHandler{store: store, redisClient: redisClient, log: log}
}

// writeJSON 通用 JSON 响应
type response struct {
	OK    bool        `json:"ok"`
	Code  int         `json:"code"`
	Msg   string      `json:"msg,omitempty"`
	Data  interface{} `json:"data,omitempty"`
}

func writeJSON(w http.ResponseWriter, code int, ok bool, msg string, data interface{}) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(code)
	json.NewEncoder(w).Encode(response{OK: ok, Code: code, Msg: msg, Data: data})
}

func writeErr(w http.ResponseWriter, code int, msg string) {
	writeJSON(w, code, false, msg, nil)
}

func writeOK(w http.ResponseWriter, data interface{}) {
	writeJSON(w, http.StatusOK, true, "", data)
}

// getOperator 从请求头获取操作人（简化版，后续可接入登录 Session）
func getOperator(r *http.Request) string {
	op := r.Header.Get("X-Operator")
	if op == "" {
		op = "admin"
	}
	return op
}

// ==================== 路由分发 ====================

// HandleConfigs /api/configs  支持 GET（列表）和 POST（创建）
func (h *ConfigHandler) HandleConfigs(w http.ResponseWriter, r *http.Request) {
	switch r.Method {
	case http.MethodGet:
		h.listConfigs(w, r)
	case http.MethodPost:
		h.createConfig(w, r)
	default:
		writeErr(w, http.StatusMethodNotAllowed, "method not allowed")
	}
}

// HandleConfigPath /api/configs/{name} 及 /api/configs/{name}/action 的统一入口
func (h *ConfigHandler) HandleConfigPath(w http.ResponseWriter, r *http.Request) {
	name, action := h.extractNameAndAction(r)
	if name == "" {
		writeErr(w, http.StatusBadRequest, "config name is required")
		return
	}

	switch action {
	case "":
		switch r.Method {
		case http.MethodGet:
			h.getConfig(w, r, name)
		case http.MethodPut:
			h.updateConfig(w, r, name)
		case http.MethodDelete:
			h.deleteConfig(w, r, name)
		default:
			writeErr(w, http.StatusMethodNotAllowed, "method not allowed")
		}
	case "publish":
		if r.Method != http.MethodPost {
			writeErr(w, http.StatusMethodNotAllowed, "method not allowed")
			return
		}
		h.publishConfig(w, r, name)
	case "rollback":
		if r.Method != http.MethodPost {
			writeErr(w, http.StatusMethodNotAllowed, "method not allowed")
			return
		}
		h.rollbackConfig(w, r, name)
	case "versions":
		if r.Method != http.MethodGet {
			writeErr(w, http.StatusMethodNotAllowed, "method not allowed")
			return
		}
		h.listVersions(w, r, name)
	case "logs":
		if r.Method != http.MethodGet {
			writeErr(w, http.StatusMethodNotAllowed, "method not allowed")
			return
		}
		h.listLogs(w, r, name)
	case "pull":
		if r.Method != http.MethodGet {
			writeErr(w, http.StatusMethodNotAllowed, "method not allowed")
			return
		}
		h.pullConfig(w, r, name)
	case "subscribe":
		if r.Method != http.MethodPost {
			writeErr(w, http.StatusMethodNotAllowed, "method not allowed")
			return
		}
		h.subscribeConfig(w, r, name)
	default:
		writeErr(w, http.StatusNotFound, "unknown action: "+action)
	}
}

// extractNameAndAction 从 URL 路径提取配置名和动作
// 例如 /api/configs/item_table/publish -> name=item_table, action=publish
func (h *ConfigHandler) extractNameAndAction(r *http.Request) (string, string) {
	path := strings.TrimPrefix(r.URL.Path, "/api/configs/")
	parts := strings.SplitN(path, "/", 2)
	name := parts[0]
	action := ""
	if len(parts) > 1 {
		action = parts[1]
	}
	return name, action
}

// ==================== 具体业务逻辑 ====================

func (h *ConfigHandler) listConfigs(w http.ResponseWriter, r *http.Request) {
	namespace := r.URL.Query().Get("namespace")
	if namespace == "" {
		namespace = "default"
	}
	var status *int32
	if s := r.URL.Query().Get("status"); s != "" {
		if v, err := strconv.Atoi(s); err == nil {
			sv := int32(v)
			status = &sv
		}
	}
	configs, err := h.store.ListConfigs(r.Context(), namespace, status)
	if err != nil {
		h.log.Errorf("list configs failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeOK(w, configs)
}

func (h *ConfigHandler) createConfig(w http.ResponseWriter, r *http.Request) {
	var req struct {
		Name        string `json:"name"`
		Namespace   string `json:"namespace"`
		Format      string `json:"format"`
		SchemaDef   string `json:"schema_def"`
		Description string `json:"description"`
		Content     string `json:"content"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, "invalid json")
		return
	}
	if req.Name == "" || req.Content == "" {
		writeErr(w, http.StatusBadRequest, "name and content are required")
		return
	}
	if req.Namespace == "" {
		req.Namespace = "default"
	}
	if req.Format == "" {
		req.Format = "json"
	}

	ctx := r.Context()
	cfg := &store.Config{
		Name:        req.Name,
		Namespace:   req.Namespace,
		Format:      req.Format,
		SchemaDef:   req.SchemaDef,
		Description: req.Description,
		Status:      0,
	}
	if err := h.store.CreateConfig(ctx, cfg); err != nil {
		h.log.Errorf("create config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}

	// 同时创建第一个版本（草稿）
	verNo, err := h.store.GetNextVersionNo(ctx, cfg.ID)
	if err != nil {
		h.log.Errorf("get next version failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	ver := &store.ConfigVersion{
		ConfigID:  cfg.ID,
		Version:   verNo,
		Content:   req.Content,
		Status:    0, // 草稿
		CreatedBy: getOperator(r),
	}
	if err := h.store.CreateVersion(ctx, ver); err != nil {
		h.log.Errorf("create version failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}

	// 记录日志
	_ = h.store.AddLog(ctx, &store.ConfigLog{
		ConfigID: cfg.ID,
		VersionID: ver.ID,
		Action:   "create",
		Operator: getOperator(r),
		Detail:   fmt.Sprintf(`{"name":"%s","namespace":"%s"}`, req.Name, req.Namespace),
		IP:       r.RemoteAddr,
	})

	writeOK(w, map[string]interface{}{"config_id": cfg.ID, "version_id": ver.ID, "version": ver.Version})
}

func (h *ConfigHandler) getConfig(w http.ResponseWriter, r *http.Request, name string) {
	namespace := r.URL.Query().Get("namespace")
	if namespace == "" {
		namespace = "default"
	}
	ctx := r.Context()
	cfg, err := h.store.GetConfig(ctx, name, namespace)
	if err != nil {
		h.log.Errorf("get config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if cfg == nil {
		writeErr(w, http.StatusNotFound, "config not found")
		return
	}

	// 同时返回当前生效版本的内容
	var content string
	if cfg.CurrentVersion > 0 {
		ver, err := h.store.GetVersion(ctx, cfg.CurrentVersion)
		if err != nil {
			h.log.Warnf("get current version failed: %v", err)
		} else if ver != nil {
			content = ver.Content
		}
	}

	writeOK(w, map[string]interface{}{
		"config":  cfg,
		"content": content,
	})
}

func (h *ConfigHandler) updateConfig(w http.ResponseWriter, r *http.Request, name string) {
	var req struct {
		Namespace   string `json:"namespace"`
		Format      string `json:"format"`
		SchemaDef   string `json:"schema_def"`
		Description string `json:"description"`
		Content     string `json:"content"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, "invalid json")
		return
	}
	if req.Namespace == "" {
		req.Namespace = "default"
	}

	ctx := r.Context()
	cfg, err := h.store.GetConfig(ctx, name, req.Namespace)
	if err != nil {
		h.log.Errorf("get config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if cfg == nil {
		writeErr(w, http.StatusNotFound, "config not found")
		return
	}

	// 更新元数据
	if req.Format != "" {
		cfg.Format = req.Format
	}
	cfg.SchemaDef = req.SchemaDef
	cfg.Description = req.Description
	if err := h.store.UpdateConfigMeta(ctx, cfg); err != nil {
		h.log.Errorf("update config meta failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}

	// 如果有新内容，创建新版本（草稿）
	var ver *store.ConfigVersion
	if req.Content != "" {
		verNo, err := h.store.GetNextVersionNo(ctx, cfg.ID)
		if err != nil {
			h.log.Errorf("get next version failed: %v", err)
			writeErr(w, http.StatusInternalServerError, err.Error())
			return
		}
		ver = &store.ConfigVersion{
			ConfigID:  cfg.ID,
			Version:   verNo,
			Content:   req.Content,
			Status:    0,
			CreatedBy: getOperator(r),
		}
		if err := h.store.CreateVersion(ctx, ver); err != nil {
			h.log.Errorf("create version failed: %v", err)
			writeErr(w, http.StatusInternalServerError, err.Error())
			return
		}
	}

	_ = h.store.AddLog(ctx, &store.ConfigLog{
		ConfigID: cfg.ID,
		VersionID: func() int64 { if ver != nil { return ver.ID } ; return 0 }(),
		Action:   "edit",
		Operator: getOperator(r),
		Detail:   fmt.Sprintf(`{"format":"%s"}`, cfg.Format),
		IP:       r.RemoteAddr,
	})

	writeOK(w, map[string]interface{}{"config_id": cfg.ID, "version_id": func() int64 { if ver != nil { return ver.ID } ; return 0 }()})
}

func (h *ConfigHandler) deleteConfig(w http.ResponseWriter, r *http.Request, name string) {
	namespace := r.URL.Query().Get("namespace")
	if namespace == "" {
		namespace = "default"
	}
	ctx := r.Context()
	cfg, err := h.store.GetConfig(ctx, name, namespace)
	if err != nil {
		h.log.Errorf("get config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if cfg == nil {
		writeErr(w, http.StatusNotFound, "config not found")
		return
	}
	if err := h.store.DeleteConfig(ctx, cfg.ID); err != nil {
		h.log.Errorf("delete config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	_ = h.store.AddLog(ctx, &store.ConfigLog{
		ConfigID: cfg.ID,
		Action:   "delete",
		Operator: getOperator(r),
		IP:       r.RemoteAddr,
	})
	writeOK(w, nil)
}

func (h *ConfigHandler) publishConfig(w http.ResponseWriter, r *http.Request, name string) {
	var req struct {
		Namespace string `json:"namespace"`
		VersionID int64  `json:"version_id"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, "invalid json")
		return
	}
	if req.Namespace == "" {
		req.Namespace = "default"
	}
	if req.VersionID == 0 {
		writeErr(w, http.StatusBadRequest, "version_id is required")
		return
	}

	ctx := r.Context()
	cfg, err := h.store.GetConfig(ctx, name, req.Namespace)
	if err != nil {
		h.log.Errorf("get config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if cfg == nil {
		writeErr(w, http.StatusNotFound, "config not found")
		return
	}

	ver, err := h.store.GetVersion(ctx, req.VersionID)
	if err != nil {
		h.log.Errorf("get version failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if ver == nil || ver.ConfigID != cfg.ID {
		writeErr(w, http.StatusBadRequest, "version not found or not belong to this config")
		return
	}

	// 标记版本为已发布
	if err := h.store.PublishVersion(ctx, ver.ID); err != nil {
		h.log.Errorf("publish version failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	// 更新当前生效版本
	if err := h.store.UpdateConfigCurrentVersion(ctx, cfg.ID, ver.ID); err != nil {
		h.log.Errorf("update current version failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}

	// 记录日志
	_ = h.store.AddLog(ctx, &store.ConfigLog{
		ConfigID:  cfg.ID,
		VersionID: ver.ID,
		Action:    "publish",
		Operator:  getOperator(r),
		Detail:    fmt.Sprintf(`{"version":%d,"checksum":"%s"}`, ver.Version, ver.Checksum),
		IP:        r.RemoteAddr,
	})

	// 推送 Redis 事件
	h.pushEvent(ctx, cfg, ver, "publish")

	writeOK(w, map[string]interface{}{"version_id": ver.ID, "version": ver.Version})
}

func (h *ConfigHandler) rollbackConfig(w http.ResponseWriter, r *http.Request, name string) {
	var req struct {
		Namespace string `json:"namespace"`
		VersionID int64  `json:"version_id"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, "invalid json")
		return
	}
	if req.Namespace == "" {
		req.Namespace = "default"
	}
	if req.VersionID == 0 {
		writeErr(w, http.StatusBadRequest, "version_id is required")
		return
	}

	ctx := r.Context()
	cfg, err := h.store.GetConfig(ctx, name, req.Namespace)
	if err != nil {
		h.log.Errorf("get config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if cfg == nil {
		writeErr(w, http.StatusNotFound, "config not found")
		return
	}

	// 获取要回滚到的历史版本
	targetVer, err := h.store.GetVersion(ctx, req.VersionID)
	if err != nil {
		h.log.Errorf("get version failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if targetVer == nil || targetVer.ConfigID != cfg.ID {
		writeErr(w, http.StatusBadRequest, "version not found or not belong to this config")
		return
	}

	// 将目标版本标记为已回滚（历史状态）
	_ = h.store.RollbackVersion(ctx, targetVer.ID)

	// 创建新版本（复用旧内容，标记为已发布）
	verNo, err := h.store.GetNextVersionNo(ctx, cfg.ID)
	if err != nil {
		h.log.Errorf("get next version failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	newVer := &store.ConfigVersion{
		ConfigID:  cfg.ID,
		Version:   verNo,
		Content:   targetVer.Content,
		Status:    1, // 直接发布
		CreatedBy: getOperator(r),
	}
	if err := h.store.CreateVersion(ctx, newVer); err != nil {
		h.log.Errorf("create rollback version failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	_ = h.store.PublishVersion(ctx, newVer.ID)
	_ = h.store.UpdateConfigCurrentVersion(ctx, cfg.ID, newVer.ID)

	_ = h.store.AddLog(ctx, &store.ConfigLog{
		ConfigID:  cfg.ID,
		VersionID: newVer.ID,
		Action:    "rollback",
		Operator:  getOperator(r),
		Detail:    fmt.Sprintf(`{"from_version":%d,"to_version":%d}`, targetVer.Version, newVer.Version),
		IP:        r.RemoteAddr,
	})

	h.pushEvent(ctx, cfg, newVer, "rollback")

	writeOK(w, map[string]interface{}{"version_id": newVer.ID, "version": newVer.Version})
}

func (h *ConfigHandler) listVersions(w http.ResponseWriter, r *http.Request, name string) {
	namespace := r.URL.Query().Get("namespace")
	if namespace == "" {
		namespace = "default"
	}
	ctx := r.Context()
	cfg, err := h.store.GetConfig(ctx, name, namespace)
	if err != nil {
		h.log.Errorf("get config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if cfg == nil {
		writeErr(w, http.StatusNotFound, "config not found")
		return
	}
	versions, err := h.store.ListVersions(ctx, cfg.ID)
	if err != nil {
		h.log.Errorf("list versions failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeOK(w, versions)
}

func (h *ConfigHandler) listLogs(w http.ResponseWriter, r *http.Request, name string) {
	namespace := r.URL.Query().Get("namespace")
	if namespace == "" {
		namespace = "default"
	}
	limit := 100
	if l := r.URL.Query().Get("limit"); l != "" {
		if v, err := strconv.Atoi(l); err == nil && v > 0 {
			limit = v
		}
	}
	ctx := r.Context()
	cfg, err := h.store.GetConfig(ctx, name, namespace)
	if err != nil {
		h.log.Errorf("get config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if cfg == nil {
		writeErr(w, http.StatusNotFound, "config not found")
		return
	}
	logs, err := h.store.ListLogs(ctx, cfg.ID, limit)
	if err != nil {
		h.log.Errorf("list logs failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeOK(w, logs)
}

func (h *ConfigHandler) pullConfig(w http.ResponseWriter, r *http.Request, name string) {
	namespace := r.URL.Query().Get("namespace")
	if namespace == "" {
		namespace = "default"
	}
	versionIDStr := r.URL.Query().Get("version_id")
	ctx := r.Context()

	// 优先从 Redis 读取配置内容
	var redisData map[string]interface{}
	if h.redisClient != nil {
		var redisKey string
		if versionIDStr != "" {
			redisKey = fmt.Sprintf("config:version:%s:%s:%s", namespace, name, versionIDStr)
		} else {
			redisKey = fmt.Sprintf("config:current:%s:%s", namespace, name)
		}
		val, err := h.redisClient.Get(ctx, redisKey)
		if err == nil && val != "" {
			if err := json.Unmarshal([]byte(val), &redisData); err == nil {
				h.log.Infof("pull config from redis: %s", redisKey)
				writeOK(w, redisData)
				return
			}
		}
	}

	// Redis miss 或不可用，回源 MySQL
	cfg, err := h.store.GetConfig(ctx, name, namespace)
	if err != nil {
		h.log.Errorf("get config failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if cfg == nil {
		writeErr(w, http.StatusNotFound, "config not found")
		return
	}

	var ver *store.ConfigVersion
	if versionIDStr != "" {
		vid, err := strconv.ParseInt(versionIDStr, 10, 64)
		if err != nil {
			writeErr(w, http.StatusBadRequest, "invalid version_id")
			return
		}
		ver, err = h.store.GetVersion(ctx, vid)
	} else {
		ver, err = h.store.GetVersion(ctx, cfg.CurrentVersion)
	}
	if err != nil {
		h.log.Errorf("get version failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	if ver == nil {
		writeErr(w, http.StatusNotFound, "version not found")
		return
	}

	writeOK(w, map[string]interface{}{
		"content":    ver.Content,
		"checksum":   ver.Checksum,
		"version_id": ver.ID,
		"version":    ver.Version,
	})
}

func (h *ConfigHandler) subscribeConfig(w http.ResponseWriter, r *http.Request, name string) {
	var req struct {
		ServiceType   string `json:"service_type"`
		SubscribeMode string `json:"subscribe_mode"`
	}
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, "invalid json")
		return
	}
	if req.ServiceType == "" {
		writeErr(w, http.StatusBadRequest, "service_type is required")
		return
	}
	if req.SubscribeMode == "" {
		req.SubscribeMode = "exact"
	}
	ctx := r.Context()
	if err := h.store.UpsertSubscriber(ctx, &store.ConfigSubscriber{
		ConfigName:    name,
		ServiceType:   req.ServiceType,
		SubscribeMode: req.SubscribeMode,
	}); err != nil {
		h.log.Errorf("upsert subscriber failed: %v", err)
		writeErr(w, http.StatusInternalServerError, err.Error())
		return
	}
	writeOK(w, nil)
}

// ==================== 推送分发 ====================

func (h *ConfigHandler) pushEvent(ctx context.Context, cfg *store.Config, ver *store.ConfigVersion, action string) {
	if h.redisClient == nil {
		h.log.Warnf("redis not available, skip push event for %s", cfg.Name)
		return
	}

	// 1. 将配置内容持久化到 Redis（供 /pull API 快速读取）
	configData, _ := json.Marshal(map[string]interface{}{
		"content":    ver.Content,
		"checksum":   ver.Checksum,
		"version_id": ver.ID,
		"version":    ver.Version,
	})
	currentKey := fmt.Sprintf("config:current:%s:%s", cfg.Namespace, cfg.Name)
	versionKey := fmt.Sprintf("config:version:%s:%s:%d", cfg.Namespace, cfg.Name, ver.ID)
	if err := h.redisClient.Set(ctx, currentKey, string(configData), 0); err != nil {
		h.log.Errorf("save config to redis failed: %v", err)
	}
	if err := h.redisClient.Set(ctx, versionKey, string(configData), 0); err != nil {
		h.log.Errorf("save config version to redis failed: %v", err)
	}

	// 2. 发布变更事件到 Pub/Sub
	event := &configpb.ConfigChangeEvent{
		ConfigName: cfg.Name,
		Namespace:  cfg.Namespace,
		VersionId:  ver.ID,
		VersionNo:  ver.Version,
		Checksum:   ver.Checksum,
		Action:     action,
		Timestamp:  time.Now().UnixMilli(),
	}
	data, err := proto.Marshal(event)
	if err != nil {
		h.log.Errorf("marshal config change event failed: %v", err)
		return
	}
	channel := fmt.Sprintf("pubsub:config:%s:%s", cfg.Namespace, cfg.Name)
	if err := h.redisClient.RawClient().Publish(ctx, channel, data).Err(); err != nil {
		h.log.Errorf("publish config change event failed: %v", err)
		return
	}
	h.log.Infof("pushed config change event to %s, version=%d action=%s", channel, ver.Version, action)
}
