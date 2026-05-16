package cache_test

import (
	"context"
	"database/sql"
	"fmt"
	"time"

	"github.com/gmaker/luffa/common/go/cache"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/common/go/rpc"
)

// ---------------------------------------------------------------------------
// 示例 1: 使用 FuncLoader —— 最简单的用法，提供加载函数即可
// ---------------------------------------------------------------------------

func ExampleFuncLoader() {
	// 1. 创建 Redis Store（实际使用时从配置初始化）
	// redisClient := redis.NewClient(redis.Config{...})
	// store := cache.NewRedisStore(redisClient)

	// 2. 配置
	cfg := cache.DefaultManagerConfig("player")
	cfg.TTL = 30 * time.Minute
	cfg.NilTTL = 60 * time.Second    // 防穿透
	cfg.JitterPct = 0.15             // 防雪崩：TTL 随机抖动 15%
	cfg.EnableBreaker = true         // 启用熔断保护

	// 3. 创建 Loader（函数式，最灵活）
	loader := cache.NewFuncLoader(func(ctx context.Context, key interface{}) (Player, error) {
		// 这里写实际的 DB 查询逻辑
		// row := db.QueryRowContext(ctx, "SELECT uid, name, level FROM player WHERE uid = ?", key)
		// ...
		return Player{UID: key.(uint64), Name: "Alice", Level: 10}, nil
	})

	// 4. 创建 Manager
	// mgr := cache.NewManager[Player](store, cache.JSONCodec[Player]{}, loader, cfg)

	// 5. 使用
	// player, err := mgr.Get(ctx, uint64(12345))

	_ = cfg
	_ = loader
	fmt.Println("FuncLoader example ready")
	// Output: FuncLoader example ready
}

// ---------------------------------------------------------------------------
// 示例 2: 使用 SQLLoader —— 基于 struct tag 自动映射，零手写 SQL
// ---------------------------------------------------------------------------

func ExampleSQLLoader() {
	// 定义业务结构体（通过 db tag 映射列名，cache:"key" 标记主键）
	type Player struct {
		UID   uint64 `db:"uid" cache:"key"`
		Name  string `db:"name"`
		Level int    `db:"level"`
		Coin  int64  `db:"coin"`
	}

	var db *sql.DB // = ... 实际的数据库连接

	// 创建 SQL 加载器（scanFunc 传 nil，自动使用 db tag 反射映射）
	loader := cache.NewSQLLoader[Player](db, cache.SQLLoaderConfig{
		Table:     "player", // 表名
		KeyColumn: "uid",    // 主键列
	}, nil)

	// loader.Load(ctx, 12345) 自动生成:
	//   SELECT uid, name, level, coin FROM player WHERE uid = ?

	_ = loader
	fmt.Println("SQLLoader example ready")
	// Output: SQLLoader example ready
}

// ---------------------------------------------------------------------------
// 示例 3: 完整的初始化流程（集成 Redis + 熔断 + 分布式锁）
// ---------------------------------------------------------------------------

func Example_fullSetup() {
	var (
		db         *sql.DB
		redisStore cache.Store // = cache.NewRedisStore(redisClient)
	)

	// ---- 1. 配置 ----
	cfg := cache.DefaultManagerConfig("player")
	cfg.TTL = 30 * time.Minute
	cfg.NilTTL = 60 * time.Second
	cfg.JitterPct = 0.15
	cfg.EnableBreaker = true
	cfg.BreakerFailureThreshold = 5
	cfg.BreakerTimeout = 30 * time.Second
	cfg.EnableStampedeLock = true     // 跨实例分布式锁防击穿
	cfg.StampedeLockTTL = 5 * time.Second
	cfg.StampedeWaitTimeout = 3 * time.Second

	// ---- 2. SQL Loader（自动映射） ----
	loader := cache.NewSQLLoader[Player](db, cache.SQLLoaderConfig{
		Table:     "player",
		KeyColumn: "uid",
	}, nil)

	// ---- 3. 创建 Manager ----
	mgr := cache.NewManager[Player](redisStore, cache.JSONCodec[Player]{}, loader, cfg)

	// 设置主键提取函数（批量加载必需）
	mgr.WithKeyFunc(func(p Player) interface{} { return p.UID })

	// 可选：注入 Redis 客户端启用分布式锁（跨实例防击穿）
	// mgr.WithLockClient(redisClient.RawClient())

	// ---- 4. 使用 ----
	ctx := context.Background()

	// 单条获取（L1 Redis → L2 DB 自动回源）
	// player, err := mgr.Get(ctx, uint64(12345))

	// 批量获取
	// players, err := mgr.GetBatch(ctx, []interface{}{uint64(1), uint64(2), uint64(3)})

	// 手动写入缓存
	// mgr.Set(ctx, uint64(12345), Player{UID: 12345, Name: "Bob", Level: 20})

	// 删除缓存（下次 Get 时自动回源）
	// mgr.Delete(ctx, uint64(12345))

	// 强制刷新（删除 + 回源）
	// mgr.Refresh(ctx, uint64(12345))

	_ = mgr
	_ = ctx
	fmt.Println("Full setup example ready")
	// Output: Full setup example ready
}

// ---------------------------------------------------------------------------
// 示例 4: 不同表结构，只需改结构体和配置（不改管理器代码）
// ---------------------------------------------------------------------------

func Example_differentEntity() {
	// 商品表 —— 不同的结构体，不同的配置
	type Item struct {
		ItemID    uint64 `db:"item_id" cache:"key"`
		Name      string `db:"name"`
		Price     int64  `db:"price"`
		Stock     int    `db:"stock"`
		UpdatedAt int64  `db:"updated_at"`
	}

	var (
		db         *sql.DB
		redisStore cache.Store
	)

	itemLoader := cache.NewSQLLoader[Item](db, cache.SQLLoaderConfig{
		Table:     "item",
		KeyColumn: "item_id",
	}, nil)

	itemCfg := cache.DefaultManagerConfig("item")
	itemCfg.TTL = 10 * time.Minute // 商品缓存更短

	itemMgr := cache.NewManager[Item](redisStore, cache.JSONCodec[Item]{}, itemLoader, itemCfg)
	itemMgr.WithKeyFunc(func(i Item) interface{} { return i.ItemID })

	// 订单表 —— 又一个完全不同的结构体
	type Order struct {
		OrderNo   string `db:"order_no" cache:"key"`
		UserID    uint64 `db:"user_id"`
		Amount    int64  `db:"amount"`
		Status    int    `db:"status"`
		CreatedAt int64  `db:"created_at"`
	}

	orderLoader := cache.NewSQLLoader[Order](db, cache.SQLLoaderConfig{
		Table:     "orders",
		KeyColumn: "order_no",
	}, nil)

	orderCfg := cache.DefaultManagerConfig("order")
	orderCfg.TTL = 5 * time.Minute

	orderMgr := cache.NewManager[Order](redisStore, cache.JSONCodec[Order]{}, orderLoader, orderCfg)
	orderMgr.WithKeyFunc(func(o Order) interface{} { return o.OrderNo })

	// 使用方式完全一致：
	// item, _ := itemMgr.Get(ctx, uint64(1001))
	// order, _ := orderMgr.Get(ctx, "ORD-20260516-001")

	_ = itemMgr
	_ = orderMgr
	fmt.Println("Different entity example ready")
	// Output: Different entity example ready
}

// ---------------------------------------------------------------------------
// 示例 5: 使用 DBProxyLoader —— 通过 DBPROXY 服务读取 MySQL
// ---------------------------------------------------------------------------

func ExampleDBProxyLoader() {
	// ---- 1. 创建到 DBPROXY 的 RPC 连接池 ----
	pool := net.NewUpstreamPool(nil)
	pool.AddNode("127.0.0.1:9003") // DBPROXY 服务地址

	// 创建 RPC 客户端并绑定连接池
	rpcClient := cache.NewDBProxyRPCClient(pool)
	pool.Start()
	defer pool.Stop()

	// ---- 2. 创建 DBProxyLoader（使用与 SQLLoader 相同的 struct tag） ----
	loader := cache.NewDBProxyLoader[Player](rpcClient, cache.SQLLoaderConfig{
		Table:     "player",
		KeyColumn: "uid",
	}, nil) // proxyCfg 传 nil 使用默认配置

	// ---- 3. 创建缓存管理器 ----
	// redisClient := redis.NewClient(redis.Config{...})
	// store := cache.NewRedisStore(redisClient)

	var redisStore cache.Store // = cache.NewRedisStore(redisClient)
	cfg := cache.DefaultManagerConfig("player")
	cfg.TTL = 30 * time.Minute
	cfg.NilTTL = 60 * time.Second
	cfg.JitterPct = 0.15
	cfg.EnableBreaker = true

	mgr := cache.NewManager[Player](redisStore, cache.JSONCodec[Player]{}, loader, cfg)
	mgr.WithKeyFunc(loader.KeyExtractor())

	// ---- 4. 使用（与 SQLLoader 完全一致） ----
	ctx := context.Background()

	// 单条获取：Redis miss → DBProxyLoader.Load() → RPC 到 DBPROXY → MySQL
	// player, err := mgr.Get(ctx, uint64(12345))

	// 批量获取：Redis miss → DBProxyLoader.LoadBatch() → RPC 到 DBPROXY → MySQL WHERE IN
	// players, err := mgr.GetBatch(ctx, []interface{}{uint64(1), uint64(2), uint64(3)})

	_ = mgr
	_ = ctx
	fmt.Println("DBProxyLoader example ready")
	// Output: DBProxyLoader example ready
}

// ---------------------------------------------------------------------------
// 示例 6: DBProxyLoader 带分库分表路由（UID 路由）
// ---------------------------------------------------------------------------

func ExampleDBProxyLoader_withSharding() {
	pool := net.NewUpstreamPool(nil)
	pool.AddNode("127.0.0.1:9003")
	rpcClient := cache.NewDBProxyRPCClient(pool)
	pool.Start()
	defer pool.Stop()

	// DBPROXY 会根据 UID 路由到不同的分库分表
	loader := cache.NewDBProxyLoader[Player](rpcClient, cache.SQLLoaderConfig{
		Table:     "player",
		KeyColumn: "uid",
	}, &cache.DBProxyConfig{
		Timeout: 5 * time.Second,
		UID:     12345, // 指定路由 UID
	})

	_ = loader
	fmt.Println("DBProxyLoader with sharding example ready")
	// Output: DBProxyLoader with sharding example ready
}

// ---------------------------------------------------------------------------
// 辅助类型（避免依赖外部包）
// ---------------------------------------------------------------------------

type Player struct {
	UID   uint64 `db:"uid" cache:"key"`
	Name  string `db:"name"`
	Level int    `db:"level"`
	Coin  int64  `db:"coin"`
}

// 确保 redis.Config 等类型在示例中可用
var _ redis.Config
var _ cache.Store
var _ *sql.DB
var _ *rpc.Client
var _ *net.UpstreamPool
