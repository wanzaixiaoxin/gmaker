package cache

import (
	"context"
	"fmt"
	"reflect"
	"strconv"
	"strings"
	"time"

	"github.com/gmaker/luffa/common/go/net"
	pb "github.com/gmaker/luffa/gen/go/dbproxy"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"github.com/gmaker/luffa/common/go/rpc"
	"google.golang.org/protobuf/proto"
)

// ---------------------------------------------------------------------------
// DBProxyLoader —— 通过 DBPROXY 服务读取 MySQL 数据的 DataLoader 实现
// ---------------------------------------------------------------------------
//
// 与 SQLLoader 的区别：
//   - SQLLoader: 直接通过 *sql.DB 连接 MySQL
//   - DBProxyLoader: 通过 TCP RPC 调用 DBPROXY 服务代理访问 MySQL
//
// 两者使用相同的 struct tag 机制（db / cache:"key"），但 DB 请求走的是网络 RPC。
//
// 使用方式：
//
//	loader := cache.NewDBProxyLoader[Player](rpcClient, cache.SQLLoaderConfig{
//	    Table:     "player",
//	    KeyColumn: "uid",
//	})
//	mgr := cache.NewManager[Player](store, codec, loader, cfg)
//	player, err := mgr.Get(ctx, uint64(12345))
//
// 架构链路：
//
//	Get(key) → Redis miss → DBProxyLoader.Load(key)
//	                              ↓
//	                   构造 MySQLQueryReq{sql, args}
//	                              ↓
//	                   rpc.Client.Call(CMD_DB_INT_MYSQL_QUERY)
//	                              ↓
//	                   DBPROXY 服务 → MySQL → 返回 MySQLQueryRes
//	                              ↓
//	                   解析 MySQLRow → 反射映射到 Player 结构体

// DBProxyConfig DBProxy 请求的额外配置
type DBProxyConfig struct {
	// Timeout 单次 RPC 请求超时时间，默认 3 秒
	Timeout time.Duration

	// UID 请求携带的用户 ID（DBPROXY 用于分库分表路由，默认 0）
	UID uint64
}

// DefaultDBProxyConfig 返回默认 DBProxy 配置
func DefaultDBProxyConfig() DBProxyConfig {
	return DBProxyConfig{
		Timeout: 3 * time.Second,
		UID:     0,
	}
}

// DBProxyLoader 通过 DBPROXY 服务加载 MySQL 数据的通用加载器
type DBProxyLoader[T any] struct {
	rpcClient  *rpc.Client
	config     SQLLoaderConfig
	proxyCfg   DBProxyConfig
	columns    []string
	colToField map[string]int
	scanFunc   func(*pb.MySQLRow) (T, error)
	keyFunc    func(T) interface{}
}

// NewDBProxyLoader 创建通过 DBPROXY 服务读取数据的 Loader
//
//   - rpcClient:  rpc.Client 实例（绑定到 DBPROXY 的上游连接池）
//   - cfg:        表名、主键列等配置（与 SQLLoader 相同）
//   - proxyCfg:   DBPROXY 特定配置（超时、UID 路由等），传 nil 使用默认值
func NewDBProxyLoader[T any](
	rpcClient *rpc.Client,
	cfg SQLLoaderConfig,
	proxyCfg *DBProxyConfig,
) *DBProxyLoader[T] {
	if proxyCfg == nil {
		defaultCfg := DefaultDBProxyConfig()
		proxyCfg = &defaultCfg
	}

	l := &DBProxyLoader[T]{
		rpcClient: rpcClient,
		config:    cfg,
		proxyCfg:  *proxyCfg,
	}

	l.parseStructTags()

	if len(cfg.Columns) > 0 {
		l.columns = cfg.Columns
	}

	// 默认扫描函数：基于 db tag 反射映射
	l.scanFunc = l.reflectScan

	return l
}

// parseStructTags 从泛型类型 T 的 db / cache tag 解析字段映射
func (l *DBProxyLoader[T]) parseStructTags() {
	var zero T
	t := reflect.TypeOf(zero)
	if t.Kind() == reflect.Ptr {
		t = t.Elem()
	}

	l.colToField = make(map[string]int)
	var cols []string

	for i := 0; i < t.NumField(); i++ {
		dbTag := t.Field(i).Tag.Get("db")
		if dbTag == "" || dbTag == "-" {
			continue
		}
		l.colToField[dbTag] = i
		cols = append(cols, dbTag)

		if t.Field(i).Tag.Get("cache") == "key" {
			// 构建主键提取函数
			idx := i
			l.keyFunc = func(v T) interface{} {
				val := reflect.ValueOf(v)
				if val.Kind() == reflect.Ptr {
					val = val.Elem()
				}
				return val.Field(idx).Interface()
			}
		}
	}

	if len(l.columns) == 0 {
		l.columns = cols
	}
}

// reflectScan 将 DBPROXY 返回的 MySQLRow 反射映射到结构体 T
func (l *DBProxyLoader[T]) reflectScan(row *pb.MySQLRow) (T, error) {
	var zero T
	t := reflect.TypeOf(zero)
	if t.Kind() == reflect.Ptr {
		t = t.Elem()
	}
	v := reflect.New(t).Elem()

	// 构建 name → value 快速查找表
	colMap := make(map[string]string, len(row.Columns))
	for _, col := range row.Columns {
		colMap[col.Name] = col.Value
	}

	for i := 0; i < t.NumField(); i++ {
		dbTag := t.Field(i).Tag.Get("db")
		if dbTag == "" || dbTag == "-" {
			continue
		}
		valStr, ok := colMap[dbTag]
		if !ok {
			continue
		}
		field := v.Field(i)
		if err := setFieldFromString(field, valStr); err != nil {
			return zero, fmt.Errorf("set field %s failed: %w", dbTag, err)
		}
	}

	return v.Interface().(T), nil
}

// setFieldFromString 将字符串值设置到 reflect.Value（DBPROXY 返回的值都是 string 类型）
func setFieldFromString(field reflect.Value, val string) error {
	if !field.CanSet() {
		return nil
	}

	// 处理指针类型字段
	if field.Kind() == reflect.Ptr {
		if val == "" {
			return nil // nil pointer
		}
		elem := reflect.New(field.Type().Elem())
		if err := setFieldFromString(elem.Elem(), val); err != nil {
			return err
		}
		field.Set(elem)
		return nil
	}

	switch field.Kind() {
	case reflect.String:
		field.SetString(val)
	case reflect.Int, reflect.Int8, reflect.Int16, reflect.Int32, reflect.Int64:
		n, err := strconv.ParseInt(val, 10, 64)
		if err != nil {
			return err
		}
		field.SetInt(n)
	case reflect.Uint, reflect.Uint8, reflect.Uint16, reflect.Uint32, reflect.Uint64:
		n, err := strconv.ParseUint(val, 10, 64)
		if err != nil {
			return err
		}
		field.SetUint(n)
	case reflect.Float32, reflect.Float64:
		n, err := strconv.ParseFloat(val, 64)
		if err != nil {
			return err
		}
		field.SetFloat(n)
	case reflect.Bool:
		b, err := strconv.ParseBool(val)
		if err != nil {
			return err
		}
		field.SetBool(b)
	default:
		field.SetString(val) // fallback
	}
	return nil
}

func (l *DBProxyLoader[T]) selectColumns() string {
	if len(l.columns) > 0 {
		return strings.Join(l.columns, ", ")
	}
	return "*"
}

// Load 通过 DBPROXY 从数据库加载单条记录
func (l *DBProxyLoader[T]) Load(ctx context.Context, key interface{}) (T, error) {
	var zero T
	query := fmt.Sprintf("SELECT %s FROM %s WHERE %s = ?",
		l.selectColumns(), l.config.Table, l.config.KeyColumn)

	req := &pb.MySQLQueryReq{
		Uid:  l.proxyCfg.UID,
		Sql:  query,
		Args: []string{fmt.Sprintf("%v", key)},
	}

	res, err := l.doQuery(ctx, req)
	if err != nil {
		return zero, err
	}

	if len(res.Rows) == 0 {
		return zero, ErrNotFound{Key: fmt.Sprintf("%v", key)}
	}

	return l.scanFunc(res.Rows[0])
}

// LoadBatch 通过 DBPROXY 批量加载记录（WHERE key IN (...)）
func (l *DBProxyLoader[T]) LoadBatch(ctx context.Context, keys []interface{}) ([]T, error) {
	if len(keys) == 0 {
		return nil, nil
	}

	placeholders := make([]string, len(keys))
	args := make([]string, len(keys))
	for i, k := range keys {
		placeholders[i] = "?"
		args[i] = fmt.Sprintf("%v", k)
	}

	query := fmt.Sprintf("SELECT %s FROM %s WHERE %s IN (%s)",
		l.selectColumns(), l.config.Table, l.config.KeyColumn,
		strings.Join(placeholders, ","))

	req := &pb.MySQLQueryReq{
		Uid:  l.proxyCfg.UID,
		Sql:  query,
		Args: args,
	}

	res, err := l.doQuery(ctx, req)
	if err != nil {
		return nil, err
	}

	var result []T
	for _, row := range res.Rows {
		v, err := l.scanFunc(row)
		if err != nil {
			continue
		}
		result = append(result, v)
	}
	return result, nil
}

// doQuery 执行一次 RPC 调用
func (l *DBProxyLoader[T]) doQuery(ctx context.Context, req *pb.MySQLQueryReq) (*pb.MySQLQueryRes, error) {
	payload, err := proto.Marshal(req)
	if err != nil {
		return nil, fmt.Errorf("marshal query req failed: %w", err)
	}

	// 设置超时
	if l.proxyCfg.Timeout > 0 {
		var cancel context.CancelFunc
		ctx, cancel = context.WithTimeout(ctx, l.proxyCfg.Timeout)
		defer cancel()
	}

	pkt, err := l.rpcClient.Call(ctx, uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY), payload)
	if err != nil {
		return nil, fmt.Errorf("dbproxy rpc call failed: %w", err)
	}

	var res pb.MySQLQueryRes
	if err := proto.Unmarshal(pkt.Payload, &res); err != nil {
		return nil, fmt.Errorf("unmarshal query res failed: %w", err)
	}

	if !res.Ok {
		return nil, fmt.Errorf("dbproxy query error: %s", res.Error)
	}

	return &res, nil
}

// KeyExtractor 返回从 T 提取主键值的函数（基于 cache:"key" tag 自动构建）
func (l *DBProxyLoader[T]) KeyExtractor() func(T) interface{} {
	return l.keyFunc
}

// ---------------------------------------------------------------------------
// 辅助函数：创建已绑定 DBPROXY 的 RPC 客户端
// ---------------------------------------------------------------------------

// NewDBProxyRPCClient 创建一个连接到 DBPROXY 服务的 RPC 客户端
//
// 使用方式：
//
//	pool := net.NewUpstreamPool(onData)
//	pool.AddNode("127.0.0.1:9003")
//	rpcClient := cache.NewDBProxyRPCClient(pool)
//	pool.Start()
func NewDBProxyRPCClient(pool *net.UpstreamPool) *rpc.Client {
	rpcClient := rpc.NewClientWithPool(pool)
	pool.SetOnData(func(conn *net.TCPConn, pkt *net.Packet) {
		rpcClient.OnPacket(pkt)
	})
	return rpcClient
}
