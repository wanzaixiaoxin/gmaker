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

	"github.com/gmaker/game-server/common/go/idgen"
	"github.com/gmaker/game-server/common/go/logger"
	"github.com/gmaker/game-server/common/go/metrics"
	"github.com/gmaker/game-server/common/go/net"
	"github.com/gmaker/game-server/common/go/registry"
	"github.com/gmaker/game-server/common/go/rpc"
	"github.com/gmaker/game-server/common/go/trace"
	bizpb "github.com/gmaker/game-server/gen/go/biz"
	commonpb "github.com/gmaker/game-server/gen/go/common"
	dbproxypb "github.com/gmaker/game-server/gen/go/dbproxy"
	loginpb "github.com/gmaker/game-server/gen/go/login"
	pb "github.com/gmaker/game-server/gen/go/registry"
	"google.golang.org/protobuf/proto"
)

const (
	CmdLoginReq        = uint32(0x00001000)
	CmdLoginRes        = uint32(0x00001001)
	CmdGetPlayerReq    = uint32(0x00010000)
	CmdGetPlayerRes    = uint32(0x00010001)
	CmdUpdatePlayerReq = uint32(0x00010006)
	CmdUpdatePlayerRes = uint32(0x00010007)
	CmdPing            = uint32(0x00010004)
	CmdPong            = uint32(0x00010005)

	CmdRedisSet   = uint32(0x000E0003)
	CmdRedisSetRes = uint32(0x000E0004)
	CmdMySQLQuery = uint32(0x000E0011)
	CmdMySQLQueryRes = uint32(0x000E0012)
	CmdMySQLExec  = uint32(0x000E0013)
	CmdMySQLExecRes = uint32(0x000E0014)
)

func main() {
	var (
		listenAddr    = flag.String("listen", ":8082", "Biz listen address")
		registryAddrs = flag.String("registry", "127.0.0.1:2379", "Registry addresses, comma separated")
		dbproxyAddrs  = flag.String("dbproxy", "127.0.0.1:3307", "DBProxy addresses, comma separated")
		nodeID        = flag.Int64("node-id", 1, "Snowflake node ID")
		metricsAddr   = flag.String("metrics", ":9082", "Metrics HTTP address")
		logFile       = flag.String("log-file", "", "Log file path (stdout if empty)")
		logLevel      = flag.String("log-level", "info", "Log level: debug | info | warn | error | fatal")
	)
	flag.Parse()

	// 初始化结构化日志
	log := logger.NewWithService("biz", fmt.Sprintf("biz-%d", *nodeID))
	log.SetLevel(*logLevel)
	if *logFile != "" {
		f, err := os.OpenFile(*logFile, os.O_CREATE|os.O_WRONLY|os.O_APPEND, 0644)
		if err == nil {
			log = logger.New(logger.Config{Level: *logLevel, Service: "biz", NodeID: fmt.Sprintf("biz-%d", *nodeID), Output: f})
		}
	}
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
	if _, err := regClient.Register(node); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Info("Biz registered to registry")

	// 连接 DBProxy（多节点）
	dbClient := NewDBProxyClient(strings.Split(*dbproxyAddrs, ","))
	if err := dbClient.Connect(); err != nil {
		log.Fatalf("connect dbproxy failed: %v", err)
	}
	defer dbClient.Close()
	log.Info("Biz connected to dbproxy")

	// 业务服务层（与 DBProxy 客户端解耦）
	playerSvc := NewPlayerService(dbClient, idGen, log)

	// 确保 player 表存在
	if err := ensurePlayerTable(playerSvc); err != nil {
		log.Fatalf("ensure player table failed: %v", err)
	}

	// 启动 TCP 服务
	cfg := net.ServerConfig{
		Addr: *listenAddr,
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			start := time.Now()
			connGauge.Inc()
			reqCounter.Inc()
			handleBizPacket(conn, pkt, playerSvc)
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
		handlePing(conn, pkt)
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

func handlePing(conn *net.TCPConn, pkt *net.Packet) {
	var req bizpb.Ping
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		return
	}
	res := &bizpb.Pong{ClientTime: req.ClientTime, ServerTime: uint64(time.Now().UnixMilli())}
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
	addrs []string
	pool  *net.UpstreamPool
	rpc   *rpc.Client
}

func NewDBProxyClient(addrs []string) *DBProxyClient {
	return &DBProxyClient{addrs: addrs}
}

func (c *DBProxyClient) Connect() error {
	c.pool = net.NewUpstreamPool(func(_ *net.TCPConn, pkt *net.Packet) {
		if c.rpc != nil {
			c.rpc.OnPacket(pkt)
		}
	})
	for _, addr := range c.addrs {
		c.pool.AddNode(addr)
	}
	c.pool.Start()
	c.rpc = rpc.NewClientWithPool(c.pool)
	return nil
}

func (c *DBProxyClient) Close() {
	if c.pool != nil {
		c.pool.Stop()
	}
}

// Call 通用 RPC 调用，业务层通过此接口访问 DBProxy
func (c *DBProxyClient) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	return c.rpc.Call(ctx, cmdID, payload)
}

// ==================== PlayerService 业务层（与 DBProxy 解耦） ====================

type PlayerService struct {
	db    *DBProxyClient
	idGen *idgen.Snowflake
	log   *logger.Logger
}

func NewPlayerService(db *DBProxyClient, idGen *idgen.Snowflake, log *logger.Logger) *PlayerService {
	return &PlayerService{db: db, idGen: idGen, log: log}
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
	key := fmt.Sprintf("token:%d", playerID)
	req := &dbproxypb.RedisSetReq{Key: key, Value: token, TtlSec: ttlSec}
	data, _ := proto.Marshal(req)
	resPkt, err := s.db.Call(ctx, CmdRedisSet, data)
	if err != nil {
		return err
	}
	var res dbproxypb.RedisSetRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		return err
	}
	return nil
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
