package service

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"strconv"
	"time"

	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/services/login-go/internal/dbproxy"

	bizpb "github.com/gmaker/luffa/gen/go/biz"
	dbproxypb "github.com/gmaker/luffa/gen/go/dbproxy"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

const (
	cmdMySQLQuery    = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY)
	cmdMySQLQueryRes = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY_RES)
	cmdMySQLExec     = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC)
	cmdMySQLExecRes  = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC_RES)
)

// PlayerService 玩家服务
type PlayerService struct {
	DB    dbproxy.Client
	Redis *redis.Client
	IDGen *idgen.Snowflake
}

// NewPlayerService 创建玩家服务
func NewPlayerService(db dbproxy.Client, r *redis.Client, idGen *idgen.Snowflake) *PlayerService {
	return &PlayerService{DB: db, Redis: r, IDGen: idGen}
}

// PlayerInfo 包含密码的完整玩家信息
type PlayerInfo struct {
	*bizpb.PlayerBase
	Password string
}

// QueryPlayerByAccount 根据账号查询玩家
func (s *PlayerService) QueryPlayerByAccount(ctx context.Context, account string) (*PlayerInfo, error) {
	sqlStr := "SELECT player_id, account, password, nickname, level, exp, coin, diamond, create_at, login_at FROM players WHERE account = ?"
	req := &dbproxypb.MySQLQueryReq{Uid: 1, Sql: sqlStr, Args: []string{account}}
	data, _ := proto.Marshal(req)
	resPkt, err := s.DB.Call(ctx, cmdMySQLQuery, data)
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
	return parsePlayerRow(res.Rows[0]), nil
}

// CreatePlayer 创建玩家
func (s *PlayerService) CreatePlayer(ctx context.Context, account, password string) (*bizpb.PlayerBase, error) {
	now := uint64(time.Now().Unix())
	playerIDRaw, err := s.IDGen.NextID()
	if err != nil {
		return nil, fmt.Errorf("generate player id failed: %w", err)
	}
	playerID := uint64(playerIDRaw)
	sqlStr := "INSERT INTO players (player_id, account, password, nickname, level, exp, coin, diamond, create_at, login_at) VALUES (?, ?, ?, ?, 1, 0, 0, 0, ?, ?)"
	req := &dbproxypb.MySQLExecReq{
		Uid:  playerID,
		Sql:  sqlStr,
		Args: []string{strconv.FormatUint(playerID, 10), account, HashPassword(password), account, strconv.FormatUint(now, 10), strconv.FormatUint(now, 10)},
	}
	data, _ := proto.Marshal(req)
	resPkt, err := s.DB.Call(ctx, cmdMySQLExec, data)
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

// SetToken 设置玩家 Token 到 Redis
func (s *PlayerService) SetToken(ctx context.Context, playerID uint64, token string, ttlSec uint64) error {
	if s.Redis == nil {
		return fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("token:%d", playerID)
	return s.Redis.Set(ctx, key, token, time.Duration(ttlSec)*time.Second)
}

// GetToken 从 Redis 获取玩家 Token
func (s *PlayerService) GetToken(ctx context.Context, playerID uint64) (string, error) {
	if s.Redis == nil {
		return "", fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("token:%d", playerID)
	return s.Redis.Get(ctx, key)
}

// VerifyToken 验证 Token 是否有效
func (s *PlayerService) VerifyToken(ctx context.Context, token string) (uint64, error) {
	// 简化：遍历 Redis 查找匹配的 token
	// 实际生产环境应使用反向索引 token -> player_id
	if s.Redis == nil {
		return 0, fmt.Errorf("redis not available")
	}
	// 这里简化处理：直接从 token 解析 player_id
	// token 格式：hex(sha256(player_id:nano))
	// 由于无法从哈希反推，我们在 Redis 中使用 token:{player_id} 存储
	// 验证时需要遍历所有 token key
	// 生产环境应使用 token:lookup:{token_hash} -> player_id
	return 0, fmt.Errorf("not implemented: use player_id + token comparison instead")
}

func parsePlayerRow(row *dbproxypb.MySQLRow) *PlayerInfo {
	base := parsePlayerBase(row)
	m := make(map[string]string)
	for _, col := range row.Columns {
		m[col.Name] = col.Value
	}
	return &PlayerInfo{PlayerBase: base, Password: m["password"]}
}

func parsePlayerBase(row *dbproxypb.MySQLRow) *bizpb.PlayerBase {
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
	}
}

func HashPassword(pwd string) string {
	h := sha256.Sum256([]byte(pwd))
	return hex.EncodeToString(h[:])
}

func GenerateToken(playerID uint64) string {
	h := sha256.Sum256([]byte(fmt.Sprintf("%d:%d", playerID, time.Now().UnixNano())))
	return hex.EncodeToString(h[:16])
}

// EnsurePlayerTable 确保 players 表存在
func EnsurePlayerTable(svc *PlayerService) error {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
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
	resPkt, err := svc.DB.Call(ctx, cmdMySQLExec, data)
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
