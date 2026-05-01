package store

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"fmt"
	"time"

	_ "github.com/go-sql-driver/mysql"
)

// DBConfig MySQL 连接配置
type DBConfig struct {
	DSN             string
	MaxOpenConn     int
	MaxIdleConn     int
	ConnMaxLifetime time.Duration
}

// NewDB 创建 MySQL 连接
func NewDB(cfg DBConfig) (*sql.DB, error) {
	db, err := sql.Open("mysql", cfg.DSN)
	if err != nil {
		return nil, err
	}
	if cfg.MaxOpenConn > 0 {
		db.SetMaxOpenConns(cfg.MaxOpenConn)
	} else {
		db.SetMaxOpenConns(20)
	}
	if cfg.MaxIdleConn > 0 {
		db.SetMaxIdleConns(cfg.MaxIdleConn)
	} else {
		db.SetMaxIdleConns(5)
	}
	if cfg.ConnMaxLifetime > 0 {
		db.SetConnMaxLifetime(cfg.ConnMaxLifetime)
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	if err := db.PingContext(ctx); err != nil {
		return nil, fmt.Errorf("mysql ping failed: %w", err)
	}
	return db, nil
}

// ConfigStore 配置存储层
type ConfigStore struct {
	db *sql.DB
}

// NewConfigStore 创建配置存储实例
func NewConfigStore(db *sql.DB) *ConfigStore {
	return &ConfigStore{db: db}
}

// Close 关闭数据库连接
func (s *ConfigStore) Close() error {
	return s.db.Close()
}

// ==================== Config 元数据操作 ====================

// Config 配置元数据记录
type Config struct {
	ID             int64  `json:"id"`
	Name           string `json:"name"`
	Namespace      string `json:"namespace"`
	Format         string `json:"format"`
	SchemaDef      string `json:"schema_def,omitempty"`
	Description    string `json:"description,omitempty"`
	CurrentVersion int64  `json:"current_version"`
	Status         int32  `json:"status"`
	CreatedAt      int64  `json:"created_at"`
	UpdatedAt      int64  `json:"updated_at"`
}

// CreateConfig 创建新配置元数据
func (s *ConfigStore) CreateConfig(ctx context.Context, cfg *Config) error {
	now := time.Now().Unix()
	result, err := s.db.ExecContext(ctx,
		"INSERT INTO configs (name, namespace, format, schema_def, description, current_version, status, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
		cfg.Name, cfg.Namespace, cfg.Format, cfg.SchemaDef, cfg.Description, 0, cfg.Status, now, now)
	if err != nil {
		return err
	}
	cfg.ID, _ = result.LastInsertId()
	cfg.CreatedAt = now
	cfg.UpdatedAt = now
	return nil
}

// GetConfig 根据 name + namespace 查询配置元数据
func (s *ConfigStore) GetConfig(ctx context.Context, name, namespace string) (*Config, error) {
	row := s.db.QueryRowContext(ctx,
		"SELECT id, name, namespace, format, schema_def, description, current_version, status, created_at, updated_at FROM configs WHERE name = ? AND namespace = ?",
		name, namespace)
	var cfg Config
	var schemaDef sql.NullString
	var desc sql.NullString
	err := row.Scan(&cfg.ID, &cfg.Name, &cfg.Namespace, &cfg.Format, &schemaDef, &desc, &cfg.CurrentVersion, &cfg.Status, &cfg.CreatedAt, &cfg.UpdatedAt)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	cfg.SchemaDef = schemaDef.String
	cfg.Description = desc.String
	return &cfg, nil
}

// ListConfigs 查询配置列表
func (s *ConfigStore) ListConfigs(ctx context.Context, namespace string, status *int32) ([]*Config, error) {
	query := "SELECT id, name, namespace, format, schema_def, description, current_version, status, created_at, updated_at FROM configs WHERE 1=1"
	var args []interface{}
	if namespace != "" {
		query += " AND namespace = ?"
		args = append(args, namespace)
	}
	if status != nil {
		query += " AND status = ?"
		args = append(args, *status)
	}
	query += " ORDER BY updated_at DESC"

	rows, err := s.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var list []*Config
	for rows.Next() {
		var cfg Config
		var schemaDef, desc sql.NullString
		if err := rows.Scan(&cfg.ID, &cfg.Name, &cfg.Namespace, &cfg.Format, &schemaDef, &desc, &cfg.CurrentVersion, &cfg.Status, &cfg.CreatedAt, &cfg.UpdatedAt); err != nil {
			return nil, err
		}
		cfg.SchemaDef = schemaDef.String
		cfg.Description = desc.String
		list = append(list, &cfg)
	}
	return list, rows.Err()
}

// UpdateConfigMeta 更新配置元数据（不含 current_version）
func (s *ConfigStore) UpdateConfigMeta(ctx context.Context, cfg *Config) error {
	now := time.Now().Unix()
	_, err := s.db.ExecContext(ctx,
		"UPDATE configs SET format = ?, schema_def = ?, description = ?, status = ?, updated_at = ? WHERE id = ?",
		cfg.Format, cfg.SchemaDef, cfg.Description, cfg.Status, now, cfg.ID)
	return err
}

// UpdateConfigCurrentVersion 更新当前生效版本
func (s *ConfigStore) UpdateConfigCurrentVersion(ctx context.Context, configID, versionID int64) error {
	now := time.Now().Unix()
	_, err := s.db.ExecContext(ctx,
		"UPDATE configs SET current_version = ?, updated_at = ? WHERE id = ?",
		versionID, now, configID)
	return err
}

// DeleteConfig 删除配置及其版本、日志
func (s *ConfigStore) DeleteConfig(ctx context.Context, configID int64) error {
	tx, err := s.db.BeginTx(ctx, nil)
	if err != nil {
		return err
	}
	defer tx.Rollback()
	if _, err := tx.ExecContext(ctx, "DELETE FROM config_logs WHERE config_id = ?", configID); err != nil {
		return err
	}
	if _, err := tx.ExecContext(ctx, "DELETE FROM config_versions WHERE config_id = ?", configID); err != nil {
		return err
	}
	if _, err := tx.ExecContext(ctx, "DELETE FROM configs WHERE id = ?", configID); err != nil {
		return err
	}
	return tx.Commit()
}

// ==================== ConfigVersion 版本操作 ====================

// ConfigVersion 配置版本记录
type ConfigVersion struct {
	ID          int64  `json:"id"`
	ConfigID    int64  `json:"config_id"`
	Version     int32  `json:"version"`
	Content     string `json:"content"`
	Checksum    string `json:"checksum"`
	Status      int32  `json:"status"` // 0=草稿, 1=已发布, 2=已回滚, 3=已废弃
	PublishedAt int64  `json:"published_at,omitempty"`
	CreatedBy   string `json:"created_by"`
	CreatedAt   int64  `json:"created_at"`
}

// CreateVersion 创建新版本（草稿状态）
func (s *ConfigStore) CreateVersion(ctx context.Context, v *ConfigVersion) error {
	now := time.Now().Unix()
	v.Checksum = calcChecksum(v.Content)
	result, err := s.db.ExecContext(ctx,
		"INSERT INTO config_versions (config_id, version, content, checksum, status, published_at, created_by, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
		v.ConfigID, v.Version, v.Content, v.Checksum, v.Status, nil, v.CreatedBy, now)
	if err != nil {
		return err
	}
	v.ID, _ = result.LastInsertId()
	v.CreatedAt = now
	return nil
}

// GetNextVersionNo 获取下一个版本号
func (s *ConfigStore) GetNextVersionNo(ctx context.Context, configID int64) (int32, error) {
	var maxVer sql.NullInt32
	err := s.db.QueryRowContext(ctx,
		"SELECT MAX(version) FROM config_versions WHERE config_id = ?", configID).Scan(&maxVer)
	if err != nil {
		return 1, err
	}
	if maxVer.Valid {
		return maxVer.Int32 + 1, nil
	}
	return 1, nil
}

// GetVersion 根据 ID 查询版本
func (s *ConfigStore) GetVersion(ctx context.Context, versionID int64) (*ConfigVersion, error) {
	row := s.db.QueryRowContext(ctx,
		"SELECT id, config_id, version, content, checksum, status, published_at, created_by, created_at FROM config_versions WHERE id = ?",
		versionID)
	var v ConfigVersion
	var pubAt sql.NullInt64
	err := row.Scan(&v.ID, &v.ConfigID, &v.Version, &v.Content, &v.Checksum, &v.Status, &pubAt, &v.CreatedBy, &v.CreatedAt)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	if pubAt.Valid {
		v.PublishedAt = pubAt.Int64
	}
	return &v, nil
}

// GetVersionByConfigVer 根据 config_id + version_no 查询
func (s *ConfigStore) GetVersionByConfigVer(ctx context.Context, configID int64, versionNo int32) (*ConfigVersion, error) {
	row := s.db.QueryRowContext(ctx,
		"SELECT id, config_id, version, content, checksum, status, published_at, created_by, created_at FROM config_versions WHERE config_id = ? AND version = ?",
		configID, versionNo)
	var v ConfigVersion
	var pubAt sql.NullInt64
	err := row.Scan(&v.ID, &v.ConfigID, &v.Version, &v.Content, &v.Checksum, &v.Status, &pubAt, &v.CreatedBy, &v.CreatedAt)
	if err == sql.ErrNoRows {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	if pubAt.Valid {
		v.PublishedAt = pubAt.Int64
	}
	return &v, nil
}

// ListVersions 查询某个配置的全部版本
func (s *ConfigStore) ListVersions(ctx context.Context, configID int64) ([]*ConfigVersion, error) {
	rows, err := s.db.QueryContext(ctx,
		"SELECT id, config_id, version, content, checksum, status, published_at, created_by, created_at FROM config_versions WHERE config_id = ? ORDER BY version DESC",
		configID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var list []*ConfigVersion
	for rows.Next() {
		var v ConfigVersion
		var pubAt sql.NullInt64
		if err := rows.Scan(&v.ID, &v.ConfigID, &v.Version, &v.Content, &v.Checksum, &v.Status, &pubAt, &v.CreatedBy, &v.CreatedAt); err != nil {
			return nil, err
		}
		if pubAt.Valid {
			v.PublishedAt = pubAt.Int64
		}
		list = append(list, &v)
	}
	return list, rows.Err()
}

// PublishVersion 将版本标记为已发布
func (s *ConfigStore) PublishVersion(ctx context.Context, versionID int64) error {
	now := time.Now().Unix()
	_, err := s.db.ExecContext(ctx,
		"UPDATE config_versions SET status = 1, published_at = ? WHERE id = ?",
		now, versionID)
	return err
}

// RollbackVersion 将版本标记为已回滚
func (s *ConfigStore) RollbackVersion(ctx context.Context, versionID int64) error {
	_, err := s.db.ExecContext(ctx,
		"UPDATE config_versions SET status = 2 WHERE id = ?",
		versionID)
	return err
}

// ==================== ConfigLog 审计日志操作 ====================

// ConfigLog 审计日志记录
type ConfigLog struct {
	ID         int64  `json:"id"`
	ConfigID   int64  `json:"config_id"`
	VersionID  int64  `json:"version_id,omitempty"`
	Action     string `json:"action"`
	Operator   string `json:"operator"`
	Detail     string `json:"detail,omitempty"`
	IP         string `json:"ip,omitempty"`
	CreatedAt  int64  `json:"created_at"`
}

// AddLog 添加审计日志
func (s *ConfigStore) AddLog(ctx context.Context, log *ConfigLog) error {
	now := time.Now().Unix()
	result, err := s.db.ExecContext(ctx,
		"INSERT INTO config_logs (config_id, version_id, action, operator, detail, ip, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)",
		log.ConfigID, log.VersionID, log.Action, log.Operator, log.Detail, log.IP, now)
	if err != nil {
		return err
	}
	log.ID, _ = result.LastInsertId()
	log.CreatedAt = now
	return nil
}

// ListLogs 查询审计日志
func (s *ConfigStore) ListLogs(ctx context.Context, configID int64, limit int) ([]*ConfigLog, error) {
	if limit <= 0 || limit > 1000 {
		limit = 100
	}
	rows, err := s.db.QueryContext(ctx,
		"SELECT id, config_id, version_id, action, operator, detail, ip, created_at FROM config_logs WHERE config_id = ? ORDER BY created_at DESC LIMIT ?",
		configID, limit)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var list []*ConfigLog
	for rows.Next() {
		var l ConfigLog
		var verID sql.NullInt64
		var detail, ip sql.NullString
		if err := rows.Scan(&l.ID, &l.ConfigID, &verID, &l.Action, &l.Operator, &detail, &ip, &l.CreatedAt); err != nil {
			return nil, err
		}
		if verID.Valid {
			l.VersionID = verID.Int64
		}
		l.Detail = detail.String
		l.IP = ip.String
		list = append(list, &l)
	}
	return list, rows.Err()
}

// ==================== ConfigSubscriber 订阅关系操作 ====================

// ConfigSubscriber 配置订阅记录
type ConfigSubscriber struct {
	ID            int64  `json:"id"`
	ConfigName    string `json:"config_name"`
	ServiceType   string `json:"service_type"`
	SubscribeMode string `json:"subscribe_mode"`
	CreatedAt     int64  `json:"created_at"`
}

// UpsertSubscriber 注册或更新订阅关系
func (s *ConfigStore) UpsertSubscriber(ctx context.Context, sub *ConfigSubscriber) error {
	now := time.Now().Unix()
	_, err := s.db.ExecContext(ctx,
		"INSERT INTO config_subscribers (config_name, service_type, subscribe_mode, created_at) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE subscribe_mode = ?",
		sub.ConfigName, sub.ServiceType, sub.SubscribeMode, now, sub.SubscribeMode)
	return err
}

// ListSubscribers 查询某配置的全部订阅者
func (s *ConfigStore) ListSubscribers(ctx context.Context, configName string) ([]*ConfigSubscriber, error) {
	rows, err := s.db.QueryContext(ctx,
		"SELECT id, config_name, service_type, subscribe_mode, created_at FROM config_subscribers WHERE config_name = ?",
		configName)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var list []*ConfigSubscriber
	for rows.Next() {
		var sub ConfigSubscriber
		if err := rows.Scan(&sub.ID, &sub.ConfigName, &sub.ServiceType, &sub.SubscribeMode, &sub.CreatedAt); err != nil {
			return nil, err
		}
		list = append(list, &sub)
	}
	return list, rows.Err()
}

// ==================== 工具函数 ====================

func calcChecksum(content string) string {
	h := sha256.Sum256([]byte(content))
	return hex.EncodeToString(h[:])
}
