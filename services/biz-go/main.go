package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"flag"
	"fmt"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/metrics"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/registry"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/common/go/rpc"
	"github.com/gmaker/luffa/common/go/trace"
	bizpb "github.com/gmaker/luffa/gen/go/biz"
	commonpb "github.com/gmaker/luffa/gen/go/common"
	dbproxypb "github.com/gmaker/luffa/gen/go/dbproxy"
	loginpb "github.com/gmaker/luffa/gen/go/login"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	pb "github.com/gmaker/luffa/gen/go/registry"
	"google.golang.org/protobuf/proto"
)

const (
	CmdLoginReq        = uint32(protocol.CmdCommon_CMD_CMN_LOGIN_REQ)
	CmdLoginRes        = uint32(protocol.CmdCommon_CMD_CMN_LOGIN_RES)
	CmdGetPlayerReq    = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_REQ)
	CmdGetPlayerRes    = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_RES)
	CmdUpdatePlayerReq = uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_REQ)
	CmdUpdatePlayerRes = uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_RES)
	CmdPing            = uint32(protocol.CmdBiz_CMD_BIZ_PING)
	CmdPong            = uint32(protocol.CmdBiz_CMD_BIZ_PONG)

	CmdMySQLQuery    = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY)
	CmdMySQLQueryRes = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY_RES)
	CmdMySQLExec     = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC)
	CmdMySQLExecRes  = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC_RES)
)

func main() {
	var (
		listenAddr    = flag.String("listen", ":8082", "Biz listen address")
		registryAddrs = flag.String("registry", "127.0.0.1:2379", "Registry addresses, comma separated")
		dbproxyAddrs  = flag.String("dbproxy", "127.0.0.1:3307", "DBProxy addresses, comma separated")
		redisAddrs    = flag.String("redis", "127.0.0.1:6379", "Redis addresses, comma separated")
		redisPass     = flag.String("redis-pass", "", "Redis password")
		nodeID        = flag.Int64("node-id", 1, "Snowflake node ID")
		metricsAddr   = flag.String("metrics", ":9082", "Metrics HTTP address")
		logFile       = flag.String("log-file", "", "Log file path (stdout if empty)")
		logLevel      = flag.String("log-level", "info", "Log level: debug | info | warn | error | fatal")
	)
	flag.Parse()

	// 初始化结构化日志
	log := logger.InitServiceLogger("biz", fmt.Sprintf("biz-%d", *nodeID), *logLevel, *logFile)
	logger.SetDefault(log)

	// 启动 Prometheus metrics HTTP 服务
	metrics.ServeDefaultHTTP(*metricsAddr)
	reqCounter := metrics.DefaultCounter("biz_requests_total")
	reqLatency := metrics.DefaultHistogram("biz_request_duration_ms", []int64{1, 5, 10, 25, 50, 100, 250, 500, 1000})
	connGauge := metrics.DefaultGauge("biz_connections")

	// 初始化 Snowflake ID 生成器
	idGen, err := idgen.NewSnowflake(*nodeID)
	if err != nil {
		log.Fatalf("init snowflake failed: %v", err)
	}

	// 注册到 Registry（多节点）
	regClient := registry.NewClient(strings.Split(*registryAddrs, ","))
	if err := regClient.Connect(); err != nil {
		log.Fatalf("connect registry failed: %v", err)
	}
	defer regClient.Close()

	node := &pb.NodeInfo{
		ServiceType: "biz",
		NodeId:      fmt.Sprintf("biz-%d", *nodeID),
		Host:        "127.0.0.1",
		Port:        parsePort(*listenAddr),
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if _, err := regClient.RegisterWithRetry(node, 5); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Info("Biz registered to registry")

	// 初始化 Redis 客户端（直接连接，不经过 DBProxy）
	var redisClient *redis.Client
	if *redisAddrs != "" {
		redisClient = redis.NewClient(redis.Config{
			Addrs:    strings.Split(*redisAddrs, ","),
			Password: *redisPass,
			PoolSize: 20,
		})
		if err := redisClient.Ping(context.Background()); err != nil {
			log.Warnf("connect redis failed: %v, running without redis", err)
			redisClient.Close()
			redisClient = nil
		} else {
			log.Info("Biz connected to redis")
		}
	}

	// 通过 Registry 发现 DBProxy
	var playerSvc *PlayerService // 在闭包之前声明
	dbproxyOnData := func(_ *net.TCPConn, pkt *net.Packet) {
		// DBProxy 响应由 rpc.Client 处理，此回调在 NewDBProxyClient 中通过 pool 绑定
	}
	upstreamMgr := registry.NewUpstreamManager(regClient)
	upstreamMgr.AddInterest("dbproxy", dbproxyOnData)
	if err := upstreamMgr.Start(); err != nil {
		log.Warnf("subscribe dbproxy upstream failed: %v", err)
	}

	dbproxyPool := upstreamMgr.GetPool("dbproxy")
	if dbproxyPool == nil || dbproxyPool.TotalCount() == 0 {
		log.Warnf("no dbproxy found in registry, using fallback addrs: %s", *dbproxyAddrs)
		dbproxyPool = net.NewUpstreamPool(dbproxyOnData)
		for _, addr := range strings.Split(*dbproxyAddrs, ",") {
			dbproxyPool.AddNode(addr)
		}
		dbproxyPool.Start()
	} else {
		log.Infof("Biz discovered dbproxy from registry, nodes=%d", dbproxyPool.TotalCount())
	}

	dbClient := NewDBProxyClient(dbproxyPool)
	if err := dbClient.Connect(); err != nil {
		log.Warnf("connect dbproxy failed: %v, running without dbproxy", err)
	} else {
		log.Info("Biz connected to dbproxy")
		// 业务服务层（与 DBProxy 客户端解耦）
		playerSvc = NewPlayerService(dbClient, redisClient, idGen, log)
		// 确保 player 表存在
		if err := ensurePlayerTable(playerSvc); err != nil {
			log.Warnf("ensure player table failed: %v, player service disabled", err)
		}
	}

	// 启动 TCP 服务
	cfg := net.ServerConfig{
		Addr: *listenAddr,
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			start := time.Now()
			connGauge.Inc()
			reqCounter.Inc()
			log.Info(fmt.Sprintf("[Flow] OnData: cmd=0x%08X seq=%d", pkt.CmdID, pkt.SeqID))
			if playerSvc == nil {
				// DBProxy 不可用时返回服务不可用
				sendProto(conn, pkt.SeqID, pkt.CmdID+1, &commonpb.Result{Ok: false, Code: 503, Msg: "service unavailable"})
			} else {
				handleBizPacket(conn, pkt, playerSvc)
			}
			reqLatency.Observe(float64(time.Since(start).Milliseconds()))
			connGauge.Dec()
		},
		OnClose: func(conn *net.TCPConn) {
			log.Infof("Biz connection closed: %s", conn.ID())
		},
	}
	srv := net.NewTCPServer(cfg)
	if err := srv.Start(); err != nil {
		log.Fatalf("start biz server failed: %v", err)
	}
	log.Infof("Biz server started on %s", *listenAddr)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutting down biz server...")
	srv.Stop()
}

func handleBizPacket(conn *net.TCPConn, pkt *net.Packet, svc *PlayerService) {
	ctx, traceID := trace.Ensure(context.Background())
	_ = ctx
	log := svc.log.WithTrace(traceID)
	svc.log.Info(fmt.Sprintf("[Flow] Gateway -> Biz: cmd=0x%08X seq=%d flags=%d payload=%d", pkt.CmdID, pkt.SeqID, pkt.Flags, len(pkt.Payload)))
	switch pkt.CmdID {
	case CmdLoginReq:
		log.Infof("[%s] login req", traceID)
		handleLogin(conn, pkt, svc, traceID)
	case CmdGetPlayerReq:
		log.Infof("[%s] get_player req", traceID)
		handleGetPlayer(conn, pkt, svc, traceID)
	case CmdUpdatePlayerReq:
		log.Infof("[%s] update_player req", traceID)
		handleUpdatePlayer(conn, pkt, svc, traceID)
	case CmdPing:
		handlePing(conn, pkt, traceID)
	default:
		log.Warnf("unknown cmd_id: 0x%08X", pkt.CmdID)
	}
}

func handleLogin(conn *net.TCPConn, pkt *net.Packet, svc *PlayerService, traceID string) {
	var req loginpb.LoginReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendLoginRes(conn, pkt.SeqID, 1, "", 0, 0)
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	// 查询账号
	player, err := svc.QueryPlayerByAccount(ctx, req.Account)
	if err != nil {
		// 账号不存在则创建
		player, err = svc.CreatePlayer(ctx, req.Account, hashPassword(req.Password))
		if err != nil {
			svc.log.WithTrace(traceID).Errorf("create player failed: %v", err)
			sendLoginRes(conn, pkt.SeqID, 1, "", 0, 0)
			return
		}
	}

	// 生成 token（简化版）
	token := generateToken(player.PlayerId)
	expireAt := uint64(time.Now().Add(24 * time.Hour).Unix())

	// 缓存 token 到 Redis
	if err := svc.SetToken(ctx, player.PlayerId, token, 24*3600); err != nil {
		svc.log.WithTrace(traceID).Errorf("set token failed: %v", err)
	}

	sendLoginRes(conn, pkt.SeqID, 0, token, player.PlayerId, expireAt)
}

func handleGetPlayer(conn *net.TCPConn, pkt *net.Packet, svc *PlayerService, traceID string) {
	var req bizpb.GetPlayerReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendGetPlayerRes(conn, pkt.SeqID, 1, nil)
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	player, err := svc.QueryPlayerByID(ctx, req.PlayerId)
	if err != nil {
		sendGetPlayerRes(conn, pkt.SeqID, 1, nil)
		return
	}
	sendGetPlayerRes(conn, pkt.SeqID, 0, player)
}

func handleUpdatePlayer(conn *net.TCPConn, pkt *net.Packet, svc *PlayerService, traceID string) {
	var req bizpb.UpdatePlayerReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendUpdatePlayerRes(conn, pkt.SeqID, 1)
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	if err := svc.UpdatePlayer(ctx, req.PlayerId, req.Nickname, req.Coin, req.Diamond); err != nil {
		svc.log.WithTrace(traceID).Errorf("update player failed: %v", err)
		sendUpdatePlayerRes(conn, pkt.SeqID, 1)
		return
	}
	sendUpdatePlayerRes(conn, pkt.SeqID, 0)
}

func handlePing(conn *net.TCPConn, pkt *net.Packet, traceID string) {
	var req bizpb.Ping
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		logger.Info(fmt.Sprintf("[Flow] Biz -> Gateway: ping parse error: %v", err))
		return
	}
	res := &bizpb.Pong{ClientTime: req.ClientTime, ServerTime: uint64(time.Now().UnixMilli())}
	logger.Info(fmt.Sprintf("[Flow] Biz -> Gateway: cmd=0x%08X seq=%d (pong)", CmdPong, pkt.SeqID))
	sendProto(conn, pkt.SeqID, CmdPong, res)
}

func sendLoginRes(conn *net.TCPConn, seqID uint32, code uint32, token string, playerID uint64, expireAt uint64) {
	res := &loginpb.LoginRes{
		Result:   &commonpb.Result{Ok: code == 0, Code: code},
		Token:    token,
		PlayerId: playerID,
		ExpireAt: expireAt,
	}
	sendProto(conn, seqID, CmdLoginRes, res)
}

func sendGetPlayerRes(conn *net.TCPConn, seqID uint32, code uint32, player *bizpb.PlayerBase) {
	res := &bizpb.GetPlayerRes{
		Result: &commonpb.Result{Ok: code == 0, Code: code},
		Player: player,
	}
	sendProto(conn, seqID, CmdGetPlayerRes, res)
}

func sendUpdatePlayerRes(conn *net.TCPConn, seqID uint32, code uint32) {
	res := &bizpb.UpdatePlayerRes{
		Result: &commonpb.Result{Ok: code == 0, Code: code},
	}
	sendProto(conn, seqID, CmdUpdatePlayerRes, res)
}

func sendProto(conn *net.TCPConn, seqID uint32, cmdID uint32, msg proto.Message) {
	data, err := proto.Marshal(msg)
	if err != nil {
		logger.Errorf("marshal error: %v", err)
		return
	}
	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  cmdID,
			SeqID:  seqID,
			Flags:  uint32(net.FlagRPCRes),
			Length: uint32(net.HeaderSize + len(data)),
		},
		Payload: data,
	}
	conn.SendPacket(pkt)
}

func hashPassword(pwd string) string {
	h := sha256.Sum256([]byte(pwd))
	return hex.EncodeToString(h[:])
}

func generateToken(playerID uint64) string {
	h := sha256.Sum256([]byte(fmt.Sprintf("%d:%d", playerID, time.Now().UnixNano())))
	return hex.EncodeToString(h[:16])
}

// ==================== DBProxy 通用客户端（零业务耦合） ====================

type DBProxyClient struct {
	pool *net.UpstreamPool
	rpc  *rpc.Client
}

func NewDBProxyClient(pool *net.UpstreamPool) *DBProxyClient {
	c := &DBProxyClient{pool: pool}
	c.rpc = rpc.NewClientWithPool(pool)
	return c
}

func (c *DBProxyClient) Connect() error {
	if c.pool == nil {
		return fmt.Errorf("dbproxy pool is nil")
	}
	// rpc 已在构造时创建，pool 由外部管理
	return nil
}

func (c *DBProxyClient) Close() {
	// pool 由外部 UpstreamManager 管理，此处不停止
}

// Call 通用 RPC 调用，业务层通过此接口访问 DBProxy
func (c *DBProxyClient) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	return c.rpc.Call(ctx, cmdID, payload)
}

// ==================== PlayerService 业务层（与 DBProxy 解耦） ====================

type PlayerService struct {
	db    *DBProxyClient
	redis *redis.Client
	idGen *idgen.Snowflake
	log   *logger.Logger
}

func NewPlayerService(db *DBProxyClient, r *redis.Client, idGen *idgen.Snowflake, log *logger.Logger) *PlayerService {
	return &PlayerService{db: db, redis: r, idGen: idGen, log: log}
}

func (s *PlayerService) QueryPlayerByAccount(ctx context.Context, account string) (*bizpb.PlayerBase, error) {
	sqlStr := "SELECT player_id, nickname, level, exp, coin, diamond, create_at, login_at FROM players WHERE account = ?"
	req := &dbproxypb.MySQLQueryReq{Uid: 1, Sql: sqlStr, Args: []string{account}}
	data, _ := proto.Marshal(req)
	resPkt, err := s.db.Call(ctx, CmdMySQLQuery, data)
	if err != nil {
		return nil, err
	}
	var res dbproxypb.MySQLQueryRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		return nil, err
	}
	if !res.Ok || len(res.Rows) == 0 {
		return nil, fmt.Errorf("player not found")
	}
	return parsePlayerRow(res.Rows[0])
}

func (s *PlayerService) QueryPlayerByID(ctx context.Context, playerID uint64) (*bizpb.PlayerBase, error) {
	sqlStr := "SELECT player_id, nickname, level, exp, coin, diamond, create_at, login_at FROM players WHERE player_id = ?"
	req := &dbproxypb.MySQLQueryReq{Uid: playerID, Sql: sqlStr, Args: []string{strconv.FormatUint(playerID, 10)}}
	data, _ := proto.Marshal(req)
	resPkt, err := s.db.Call(ctx, CmdMySQLQuery, data)
	if err != nil {
		return nil, err
	}
	var res dbproxypb.MySQLQueryRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		return nil, err
	}
	if !res.Ok || len(res.Rows) == 0 {
		return nil, fmt.Errorf("player not found")
	}
	return parsePlayerRow(res.Rows[0])
}

func (s *PlayerService) CreatePlayer(ctx context.Context, account, password string) (*bizpb.PlayerBase, error) {
	now := uint64(time.Now().Unix())
	playerIDRaw, err := s.idGen.NextID()
	if err != nil {
		return nil, fmt.Errorf("generate player id failed: %w", err)
	}
	playerID := uint64(playerIDRaw)
	sqlStr := "INSERT INTO players (player_id, account, password, nickname, level, exp, coin, diamond, create_at, login_at) VALUES (?, ?, ?, ?, 1, 0, 0, 0, ?, ?)"
	req := &dbproxypb.MySQLExecReq{
		Uid:  playerID,
		Sql:  sqlStr,
		Args: []string{strconv.FormatUint(playerID, 10), account, password, account, strconv.FormatUint(now, 10), strconv.FormatUint(now, 10)},
	}
	data, _ := proto.Marshal(req)
	resPkt, err := s.db.Call(ctx, CmdMySQLExec, data)
	if err != nil {
		return nil, err
	}
	var res dbproxypb.MySQLExecRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		return nil, err
	}
	if !res.Ok {
		return nil, fmt.Errorf("exec failed: %s", res.Error)
	}
	return &bizpb.PlayerBase{
		PlayerId: playerID,
		Nickname: account,
		Level:    1,
		Exp:      0,
		Coin:     0,
		Diamond:  0,
		CreateAt: now,
		LoginAt:  now,
	}, nil
}

func (s *PlayerService) UpdatePlayer(ctx context.Context, playerID uint64, nickname string, coin uint64, diamond uint64) error {
	parts := []string{}
	args := []string{}
	if nickname != "" {
		parts = append(parts, "nickname = ?")
		args = append(args, nickname)
	}
	parts = append(parts, "coin = ?")
	args = append(args, strconv.FormatUint(coin, 10))
	parts = append(parts, "diamond = ?")
	args = append(args, strconv.FormatUint(diamond, 10))
	parts = append(parts, "login_at = ?")
	args = append(args, strconv.FormatUint(uint64(time.Now().Unix()), 10))

	sqlStr := fmt.Sprintf("UPDATE players SET %s WHERE player_id = ?", strings.Join(parts, ", "))
	args = append(args, strconv.FormatUint(playerID, 10))

	req := &dbproxypb.MySQLExecReq{Uid: playerID, Sql: sqlStr, Args: args}
	data, _ := proto.Marshal(req)
	resPkt, err := s.db.Call(ctx, CmdMySQLExec, data)
	if err != nil {
		return err
	}
	var res dbproxypb.MySQLExecRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		return err
	}
	if !res.Ok {
		return fmt.Errorf("exec failed: %s", res.Error)
	}
	return nil
}

func (s *PlayerService) SetToken(ctx context.Context, playerID uint64, token string, ttlSec uint64) error {
	if s.redis == nil {
		return fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("token:%d", playerID)
	return s.redis.Set(ctx, key, token, time.Duration(ttlSec)*time.Second)
}

func ensurePlayerTable(svc *PlayerService) error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	sqlStr := `CREATE TABLE IF NOT EXISTS players (
		player_id BIGINT PRIMARY KEY,
		account VARCHAR(64) UNIQUE NOT NULL,
		password VARCHAR(128) NOT NULL,
		nickname VARCHAR(64) NOT NULL,
		level INT DEFAULT 1,
		exp BIGINT DEFAULT 0,
		coin BIGINT DEFAULT 0,
		diamond BIGINT DEFAULT 0,
		create_at BIGINT DEFAULT 0,
		login_at BIGINT DEFAULT 0
	)`
	req := &dbproxypb.MySQLExecReq{Uid: 1, Sql: sqlStr}
	data, _ := proto.Marshal(req)
	resPkt, err := svc.db.Call(ctx, CmdMySQLExec, data)
	if err != nil {
		return err
	}
	var res dbproxypb.MySQLExecRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		return err
	}
	if !res.Ok {
		return fmt.Errorf("create table failed: %s", res.Error)
	}
	return nil
}

func parsePort(addr string) uint32 {
	parts := strings.Split(addr, ":")
	if len(parts) >= 2 {
		if p, err := strconv.ParseUint(parts[len(parts)-1], 10, 32); err == nil {
			return uint32(p)
		}
	}
	return 0
}

func parsePlayerRow(row *dbproxypb.MySQLRow) (*bizpb.PlayerBase, error) {
	m := make(map[string]string)
	for _, col := range row.Columns {
		m[col.Name] = col.Value
	}
	pid, _ := strconv.ParseUint(m["player_id"], 10, 64)
	lvl, _ := strconv.ParseUint(m["level"], 10, 64)
	exp, _ := strconv.ParseUint(m["exp"], 10, 64)
	coin, _ := strconv.ParseUint(m["coin"], 10, 64)
	dia, _ := strconv.ParseUint(m["diamond"], 10, 64)
	cat, _ := strconv.ParseUint(m["create_at"], 10, 64)
	lat, _ := strconv.ParseUint(m["login_at"], 10, 64)
	return &bizpb.PlayerBase{
		PlayerId: pid,
		Nickname: m["nickname"],
		Level:    uint32(lvl),
		Exp:      exp,
		Coin:     coin,
		Diamond:  dia,
		CreateAt: cat,
		LoginAt:  lat,
	}, nil
}
