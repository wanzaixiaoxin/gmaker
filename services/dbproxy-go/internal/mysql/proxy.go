package mysql

import (
	"context"
	"database/sql"
	"fmt"
	"strings"
	"time"

	_ "github.com/go-sql-driver/mysql"
)

// Proxy MySQL 代理层（连接池 + 分库分表路由接口）
type Proxy struct {
	shards []*sql.DB
}

// Config 单分片配置
type Config struct {
	DSN             string
	MaxOpenConn     int
	MaxIdleConn     int
	ConnMaxLifetime time.Duration // 0 表示不限制
}

// NewProxy 创建 MySQL 代理
func NewProxy(cfgs []Config) (*Proxy, error) {
	var shards []*sql.DB
	for _, cfg := range cfgs {
		db, err := sql.Open("mysql", cfg.DSN)
		if err != nil {
			return nil, err
		}
		if cfg.MaxOpenConn > 0 {
			db.SetMaxOpenConns(cfg.MaxOpenConn)
		} else {
			db.SetMaxOpenConns(20) // 默认兜底
		}
		if cfg.MaxIdleConn > 0 {
			db.SetMaxIdleConns(cfg.MaxIdleConn)
		} else {
			db.SetMaxIdleConns(5)
		}
		if cfg.ConnMaxLifetime > 0 {
			db.SetConnMaxLifetime(cfg.ConnMaxLifetime)
		}
		// Warmup: ensure first physical connection is established
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
		if err := db.PingContext(ctx); err != nil {
			cancel()
			return nil, fmt.Errorf("mysql ping failed: %w", err)
		}
		cancel()
		shards = append(shards, db)
	}
	return &Proxy{shards: shards}, nil
}

func (p *Proxy) Close() error {
	for _, db := range p.shards {
		db.Close()
	}
	return nil
}

func (p *Proxy) Ping(ctx context.Context) error {
	for _, db := range p.shards {
		if err := db.PingContext(ctx); err != nil {
			return err
		}
	}
	return nil
}

// RouteKey 按 key 路由到目标分片
func (p *Proxy) RouteKey(key string) int {
	if len(p.shards) == 0 {
		return -1
	}
	h := hashCode(key)
	if h < 0 {
		h = -h
	}
	return h % len(p.shards)
}

// RouteUID 按 UID 路由到目标分片
func (p *Proxy) RouteUID(uid uint64) int {
	if len(p.shards) == 0 {
		return -1
	}
	return int(uid % uint64(len(p.shards)))
}

// QueryRowByUID 按 UID 路由执行单行查询
func (p *Proxy) QueryRowByUID(ctx context.Context, uid uint64, sqlStr string, args ...interface{}) *sql.Row {
	idx := p.RouteUID(uid)
	if idx < 0 {
		return nil
	}
	return p.shards[idx].QueryRowContext(ctx, sqlStr, args...)
}

// QueryByUID 按 UID 路由执行查询
func (p *Proxy) QueryByUID(ctx context.Context, uid uint64, sqlStr string, args ...interface{}) (*sql.Rows, error) {
	idx := p.RouteUID(uid)
	if idx < 0 {
		return nil, fmt.Errorf("no shard available")
	}
	return p.shards[idx].QueryContext(ctx, sqlStr, args...)
}

// ExecByUID 按 UID 路由执行写操作
func (p *Proxy) ExecByUID(ctx context.Context, uid uint64, sqlStr string, args ...interface{}) (sql.Result, error) {
	idx := p.RouteUID(uid)
	if idx < 0 {
		return nil, fmt.Errorf("no shard available")
	}
	return p.shards[idx].ExecContext(ctx, sqlStr, args...)
}

// ExecAll 对所有分片执行同一语句（如建表）
func (p *Proxy) ExecAll(ctx context.Context, sqlStr string, args ...interface{}) error {
	for i, db := range p.shards {
		if _, err := db.ExecContext(ctx, sqlStr, args...); err != nil {
			return fmt.Errorf("shard %d exec failed: %w", i, err)
		}
	}
	return nil
}

// BuildShardTable 生成分表名，如 player_01
func BuildShardTable(base string, uid uint64, tableCount int) string {
	if tableCount <= 1 {
		return base
	}
	suffix := uid % uint64(tableCount)
	return fmt.Sprintf("%s_%02d", base, suffix)
}

// hashCode 简单字符串哈希（兼容 Java hashCode 逻辑）
func hashCode(s string) int {
	h := 0
	for _, c := range s {
		h = 31*h + int(c)
	}
	return h
}

// EscapeLike 转义 LIKE 模糊查询特殊字符
func EscapeLike(s string) string {
	s = strings.ReplaceAll(s, "\\", "\\\\")
	s = strings.ReplaceAll(s, "%", "\\%")
	s = strings.ReplaceAll(s, "_", "\\_")
	return s
}
