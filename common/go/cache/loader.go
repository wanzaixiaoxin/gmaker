package cache

import (
	"context"
	"database/sql"
	"fmt"
	"reflect"
	"strings"
)

// ---------------------------------------------------------------------------
// DataLoader —— 二级缓存的数据源接口（L2 层）
// ---------------------------------------------------------------------------

// DataLoader 数据加载器接口，由业务侧实现或使用内置的 FuncLoader / SQLLoader
type DataLoader[T any] interface {
	// Load 从数据库加载单条数据，key 通常为主键值
	Load(ctx context.Context, key interface{}) (T, error)

	// LoadBatch 从数据库批量加载数据，返回的切片长度可能小于 keys 长度（部分 key 无数据）
	LoadBatch(ctx context.Context, keys []interface{}) ([]T, error)
}

// ---------------------------------------------------------------------------
// FuncLoader —— 函数式适配器
// ---------------------------------------------------------------------------

// LoadFunc 单条加载函数签名
type LoadFunc[T any] func(ctx context.Context, key interface{}) (T, error)

// BatchLoadFunc 批量加载函数签名
type BatchLoadFunc[T any] func(ctx context.Context, keys []interface{}) ([]T, error)

// FuncLoader 基于 Go 函数的 DataLoader 适配器
type FuncLoader[T any] struct {
	singleFn LoadFunc[T]
	batchFn  BatchLoadFunc[T]
}

// NewFuncLoader 创建仅支持单条加载的 FuncLoader（批量加载自动退化为逐条调用）
func NewFuncLoader[T any](fn LoadFunc[T]) *FuncLoader[T] {
	return &FuncLoader[T]{singleFn: fn}
}

// NewFuncLoaderWithBatch 创建同时支持单条和批量加载的 FuncLoader
func NewFuncLoaderWithBatch[T any](single LoadFunc[T], batch BatchLoadFunc[T]) *FuncLoader[T] {
	return &FuncLoader[T]{singleFn: single, batchFn: batch}
}

func (l *FuncLoader[T]) Load(ctx context.Context, key interface{}) (T, error) {
	return l.singleFn(ctx, key)
}

func (l *FuncLoader[T]) LoadBatch(ctx context.Context, keys []interface{}) ([]T, error) {
	if l.batchFn != nil {
		return l.batchFn(ctx, keys)
	}
	// fallback：逐条加载
	var result []T
	for _, k := range keys {
		v, err := l.singleFn(ctx, k)
		if err != nil {
			continue
		}
		result = append(result, v)
	}
	return result, nil
}

// ---------------------------------------------------------------------------
// SQLLoader —— 基于 struct tag 自动映射的通用 SQL 加载器
// ---------------------------------------------------------------------------
//
// 通过 db tag 标注列名，cache:"key" 标注主键字段，实现零代码的 SQL 自动生成：
//
//	type Player struct {
//	    UID   uint64 `db:"uid" cache:"key"`   // 主键
//	    Name  string `db:"name"`
//	    Level int    `db:"level"`
//	    Coin  int64  `db:"coin"`
//	}
//
//	loader := cache.NewSQLLoader[Player](db, cache.SQLLoaderConfig{
//	    Table:     "player",
//	    KeyColumn: "uid",
//	}, nil)  // scanFunc 传 nil，自动使用 db tag 反射映射
//
//	// 自动生成: SELECT uid, name, level, coin FROM player WHERE uid = ?
//	player, err := loader.Load(ctx, 12345)

// SQLLoader 基于 *sql.DB 的通用数据加载器
type SQLLoader[T any] struct {
	db           *sql.DB
	config       SQLLoaderConfig
	columns      []string
	colToField   map[string]int
	keyFieldIdx  int
	scanFunc     func(*sql.Rows) (T, error)
	keyExtractor func(T) interface{}
}

// NewSQLLoader 创建 SQL 加载器。
//   - db:      *sql.DB 实例
//   - cfg:     表名、主键列等配置
//   - scanFunc: 自定义行扫描函数。传 nil 则自动使用 db tag 反射映射。
func NewSQLLoader[T any](db *sql.DB, cfg SQLLoaderConfig, scanFunc func(*sql.Rows) (T, error)) *SQLLoader[T] {
	l := &SQLLoader[T]{
		db:     db,
		config: cfg,
	}
	l.parseStructTags()

	// 用户显式指定列时优先使用
	if len(cfg.Columns) > 0 {
		l.columns = cfg.Columns
	}

	// 扫描函数
	if scanFunc != nil {
		l.scanFunc = scanFunc
	} else {
		l.scanFunc = l.reflectScan
	}

	return l
}

// parseStructTags 从泛型类型 T 的 db / cache tag 解析字段映射
func (l *SQLLoader[T]) parseStructTags() {
	var zero T
	t := reflect.TypeOf(zero)
	if t.Kind() == reflect.Ptr {
		t = t.Elem()
	}

	l.colToField = make(map[string]int)
	l.keyFieldIdx = -1
	var cols []string

	for i := 0; i < t.NumField(); i++ {
		dbTag := t.Field(i).Tag.Get("db")
		if dbTag == "" || dbTag == "-" {
			continue
		}
		l.colToField[dbTag] = i
		cols = append(cols, dbTag)

		if t.Field(i).Tag.Get("cache") == "key" {
			l.keyFieldIdx = i
		}
	}

	if len(l.columns) == 0 {
		l.columns = cols
	}

	// 构建主键提取函数
	if l.keyFieldIdx >= 0 {
		idx := l.keyFieldIdx
		l.keyExtractor = func(v T) interface{} {
			val := reflect.ValueOf(v)
			if val.Kind() == reflect.Ptr {
				val = val.Elem()
			}
			return val.Field(idx).Interface()
		}
	}
}

// reflectScan 基于 db tag 的反射行扫描
func (l *SQLLoader[T]) reflectScan(rows *sql.Rows) (T, error) {
	var zero T
	cols, err := rows.Columns()
	if err != nil {
		return zero, err
	}

	t := reflect.TypeOf(zero)
	if t.Kind() == reflect.Ptr {
		t = t.Elem()
	}
	v := reflect.New(t).Elem()

	scanVals := make([]interface{}, len(cols))
	for i, col := range cols {
		if fi, ok := l.colToField[col]; ok {
			scanVals[i] = v.Field(fi).Addr().Interface()
		} else {
			scanVals[i] = new(interface{})
		}
	}
	if err := rows.Scan(scanVals...); err != nil {
		return zero, err
	}
	return v.Interface().(T), nil
}

func (l *SQLLoader[T]) selectColumns() string {
	if len(l.columns) > 0 {
		return strings.Join(l.columns, ", ")
	}
	return "*"
}

// Load 从数据库加载单条记录
func (l *SQLLoader[T]) Load(ctx context.Context, key interface{}) (T, error) {
	var zero T
	query := fmt.Sprintf("SELECT %s FROM %s WHERE %s = ?",
		l.selectColumns(), l.config.Table, l.config.KeyColumn)

	rows, err := l.db.QueryContext(ctx, query, key)
	if err != nil {
		return zero, fmt.Errorf("sql query failed: %w", err)
	}
	defer rows.Close()

	if !rows.Next() {
		return zero, ErrNotFound{Key: fmt.Sprintf("%v", key)}
	}

	v, err := l.scanFunc(rows)
	if err != nil {
		return zero, fmt.Errorf("sql scan failed: %w", err)
	}
	return v, nil
}

// LoadBatch 从数据库批量加载记录（WHERE key IN (...)）
func (l *SQLLoader[T]) LoadBatch(ctx context.Context, keys []interface{}) ([]T, error) {
	if len(keys) == 0 {
		return nil, nil
	}

	placeholders := make([]string, len(keys))
	args := make([]interface{}, len(keys))
	for i, k := range keys {
		placeholders[i] = "?"
		args[i] = k
	}

	query := fmt.Sprintf("SELECT %s FROM %s WHERE %s IN (%s)",
		l.selectColumns(), l.config.Table, l.config.KeyColumn,
		strings.Join(placeholders, ","))

	rows, err := l.db.QueryContext(ctx, query, args...)
	if err != nil {
		return nil, fmt.Errorf("sql batch query failed: %w", err)
	}
	defer rows.Close()

	var result []T
	for rows.Next() {
		v, err := l.scanFunc(rows)
		if err != nil {
			continue
		}
		result = append(result, v)
	}
	return result, nil
}

// KeyExtractor 返回从 T 提取主键值的函数（基于 cache:"key" tag 自动构建）
func (l *SQLLoader[T]) KeyExtractor() func(T) interface{} {
	return l.keyExtractor
}
