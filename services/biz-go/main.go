package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/gmaker/game-server/common/go/net"
	"github.com/gmaker/game-server/common/go/registry"
	"github.com/gmaker/game-server/common/go/rpc"
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
		listenAddr   = flag.String("listen", ":8082", "Biz listen address")
		registryAddr = flag.String("registry", "127.0.0.1:2379", "Registry address")
		dbproxyAddr  = flag.String("dbproxy", "127.0.0.1:3307", "DBProxy address")
	)
	flag.Parse()

	// 注册到 Registry
	regClient := registry.NewClient(*registryAddr)
	if err := regClient.Connect(); err != nil {
		log.Fatalf("connect registry failed: %v", err)
	}
	defer regClient.Close()

	node := &pb.NodeInfo{
		ServiceType: "biz",
		NodeId:      "biz-001",
		Host:        "127.0.0.1",
		Port:        8082,
		RegisterAt:  uint64(time.Now().Unix()),
	}
	if _, err := regClient.Register(node); err != nil {
		log.Fatalf("register failed: %v", err)
	}
	log.Println("Biz registered to registry")

	// 连接 DBProxy
	dbClient := NewDBProxyClient(*dbproxyAddr)
	if err := dbClient.Connect(); err != nil {
		log.Fatalf("connect dbproxy failed: %v", err)
	}
	defer dbClient.Close()
	log.Println("Biz connected to dbproxy")

	// 确保 player 表存在
	if err := ensurePlayerTable(dbClient); err != nil {
		log.Fatalf("ensure player table failed: %v", err)
	}

	// 启动 TCP 服务
	cfg := net.ServerConfig{
		Addr: *listenAddr,
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			handleBizPacket(conn, pkt, dbClient)
		},
		OnClose: func(conn *net.TCPConn) {
			log.Printf("Biz connection closed: %s", conn.ID())
		},
	}
	srv := net.NewTCPServer(cfg)
	if err := srv.Start(); err != nil {
		log.Fatalf("start biz server failed: %v", err)
	}
	log.Printf("Biz server started on %s", *listenAddr)

	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Println("shutting down biz server...")
	srv.Stop()
}

func handleBizPacket(conn *net.TCPConn, pkt *net.Packet, db *DBProxyClient) {
	switch pkt.CmdID {
	case CmdLoginReq:
		handleLogin(conn, pkt, db)
	case CmdGetPlayerReq:
		handleGetPlayer(conn, pkt, db)
	case CmdUpdatePlayerReq:
		handleUpdatePlayer(conn, pkt, db)
	case CmdPing:
		handlePing(conn, pkt)
	default:
		log.Printf("unknown cmd_id: 0x%08X", pkt.CmdID)
	}
}

func handleLogin(conn *net.TCPConn, pkt *net.Packet, db *DBProxyClient) {
	var req loginpb.LoginReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendLoginRes(conn, pkt.SeqID, 1, "", 0, 0)
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	// 查询账号
	player, err := db.QueryPlayerByAccount(ctx, req.Account)
	if err != nil {
		// 账号不存在则创建
		player, err = db.CreatePlayer(ctx, req.Account, hashPassword(req.Password))
		if err != nil {
			log.Printf("create player failed: %v", err)
			sendLoginRes(conn, pkt.SeqID, 1, "", 0, 0)
			return
		}
	}

	// 生成 token（简化版）
	token := generateToken(player.PlayerId)
	expireAt := uint64(time.Now().Add(24 * time.Hour).Unix())

	// 缓存 token 到 Redis
	if err := db.SetToken(ctx, player.PlayerId, token, 24*3600); err != nil {
		log.Printf("set token failed: %v", err)
	}

	sendLoginRes(conn, pkt.SeqID, 0, token, player.PlayerId, expireAt)
}

func handleGetPlayer(conn *net.TCPConn, pkt *net.Packet, db *DBProxyClient) {
	var req bizpb.GetPlayerReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendGetPlayerRes(conn, pkt.SeqID, 1, nil)
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	player, err := db.QueryPlayerByID(ctx, req.PlayerId)
	if err != nil {
		sendGetPlayerRes(conn, pkt.SeqID, 1, nil)
		return
	}
	sendGetPlayerRes(conn, pkt.SeqID, 0, player)
}

func handleUpdatePlayer(conn *net.TCPConn, pkt *net.Packet, db *DBProxyClient) {
	var req bizpb.UpdatePlayerReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendUpdatePlayerRes(conn, pkt.SeqID, 1)
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	if err := db.UpdatePlayer(ctx, req.PlayerId, req.Nickname, req.Coin, req.Diamond); err != nil {
		log.Printf("update player failed: %v", err)
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
		log.Printf("marshal error: %v", err)
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

// ==================== DBProxy 客户端封装 ====================

type DBProxyClient struct {
	addr   string
	tcp    *net.TCPClient
	rpc    *rpc.Client
}

func NewDBProxyClient(addr string) *DBProxyClient {
	return &DBProxyClient{addr: addr}
}

func (c *DBProxyClient) Connect() error {
	c.tcp = net.NewTCPClient(c.addr,
		func(conn *net.TCPConn, pkt *net.Packet) {
			if c.rpc != nil {
				c.rpc.OnPacket(pkt)
			}
		},
		func(conn *net.TCPConn) {
			log.Println("DBProxy connection closed")
		})
	if err := c.tcp.Connect(); err != nil {
		return err
	}
	c.rpc = rpc.NewClient(c.tcp.Conn())
	return nil
}

func (c *DBProxyClient) Close() {
	if c.tcp != nil {
		c.tcp.Close()
	}
}

func (c *DBProxyClient) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	return c.rpc.Call(ctx, cmdID, payload)
}

func (c *DBProxyClient) QueryPlayerByAccount(ctx context.Context, account string) (*bizpb.PlayerBase, error) {
	sqlStr := "SELECT player_id, nickname, level, exp, coin, diamond, create_at, login_at FROM players WHERE account = ?"
	req := &dbproxypb.MySQLQueryReq{Uid: 1, Sql: sqlStr, Args: []string{account}}
	data, _ := proto.Marshal(req)
	resPkt, err := c.Call(ctx, CmdMySQLQuery, data)
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
	return parsePlayerRow(string(res.Rows[0]))
}

func (c *DBProxyClient) QueryPlayerByID(ctx context.Context, playerID uint64) (*bizpb.PlayerBase, error) {
	sqlStr := "SELECT player_id, nickname, level, exp, coin, diamond, create_at, login_at FROM players WHERE player_id = ?"
	req := &dbproxypb.MySQLQueryReq{Uid: playerID, Sql: sqlStr, Args: []string{strconv.FormatUint(playerID, 10)}}
	data, _ := proto.Marshal(req)
	resPkt, err := c.Call(ctx, CmdMySQLQuery, data)
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
	return parsePlayerRow(string(res.Rows[0]))
}

func (c *DBProxyClient) CreatePlayer(ctx context.Context, account, password string) (*bizpb.PlayerBase, error) {
	now := uint64(time.Now().Unix())
	playerID := uint64(now)*1000000 + uint64(now%1000000)
	sqlStr := "INSERT INTO players (player_id, account, password, nickname, level, exp, coin, diamond, create_at, login_at) VALUES (?, ?, ?, ?, 1, 0, 0, 0, ?, ?)"
	req := &dbproxypb.MySQLExecReq{
		Uid:  playerID,
		Sql:  sqlStr,
		Args: []string{strconv.FormatUint(playerID, 10), account, password, account, strconv.FormatUint(now, 10), strconv.FormatUint(now, 10)},
	}
	data, _ := proto.Marshal(req)
	resPkt, err := c.Call(ctx, CmdMySQLExec, data)
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

func (c *DBProxyClient) UpdatePlayer(ctx context.Context, playerID uint64, nickname string, coin uint64, diamond uint64) error {
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
	resPkt, err := c.Call(ctx, CmdMySQLExec, data)
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

func (c *DBProxyClient) SetToken(ctx context.Context, playerID uint64, token string, ttlSec uint64) error {
	key := fmt.Sprintf("token:%d", playerID)
	req := &dbproxypb.RedisSetReq{Key: key, Value: token, TtlSec: ttlSec}
	data, _ := proto.Marshal(req)
	resPkt, err := c.Call(ctx, CmdRedisSet, data)
	if err != nil {
		return err
	}
	var res dbproxypb.RedisSetRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		return err
	}
	return nil
}

func ensurePlayerTable(db *DBProxyClient) error {
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
	resPkt, err := db.Call(ctx, CmdMySQLExec, data)
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

func parsePlayerRow(row string) (*bizpb.PlayerBase, error) {
	// DBProxy 返回的是 map[string]string 的 fmt.Sprintf 结果
	// 格式类似: map[coin:0 create_at:123 diamond:0 exp:0 level:1 login_at:123 nickname:test player_id:123]
	m := make(map[string]string)
	row = strings.TrimPrefix(row, "map[")
	row = strings.TrimSuffix(row, "]")
	for _, pair := range strings.Fields(row) {
		kv := strings.SplitN(pair, ":", 2)
		if len(kv) == 2 {
			m[kv[0]] = kv[1]
		}
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
