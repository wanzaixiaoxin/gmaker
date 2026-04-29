package service

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"strconv"
	"time"

	"github.com/gmaker/luffa/common/go/cache"
	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/logger"
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
	DB           dbproxy.Client
	Redis        *redis.Client
	IDGen        *idgen.Snowflake
	AccountCache *cache.Cache[*PlayerInfo] // account -> PlayerInfo
	Log          *logger.Logger
}

// NewPlayerService 创建玩家服务
func NewPlayerService(db dbproxy.Client, r *redis.Client, idGen *idgen.Snowflake, accountCache *cache.Cache[*PlayerInfo], log *logger.Logger) *PlayerService {
	if log == nil {
		log = logger.NewWithService("login", "unknown")
	}
	return &PlayerService{DB: db, Redis: r, IDGen: idGen, AccountCache: accountCache, Log: log}
}

// PlayerInfo 包含密码的完整玩家信息
type PlayerInfo struct {
	*bizpb.PlayerBase
	Password string
}

// QueryPlayerByAccount 根据账号查询玩家（带缓存）
func (s *PlayerService) QueryPlayerByAccount(ctx context.Context, account string) (*PlayerInfo, error) {
	if s.AccountCache != nil {
		info, err := s.AccountCache.GetOrLoad(ctx, account, func(ctx context.Context, key string) (*PlayerInfo, error) {
			s.Log.Infow("cache miss, loading from DB", map[string]interface{}{"account": key})
			return s.queryPlayerFromDB(ctx, key)
		})
		if err == nil {
			s.Log.Infow("cache hit", map[string]interface{}{"account": account, "player_id": info.PlayerId})
		} else {
			s.Log.Infow("cache miss after load", map[string]interface{}{"account": account, "error": err.Error()})
		}
		return info, err
	}
	s.Log.Infow("cache disabled, query DB directly", map[string]interface{}{"account": account})
	return s.queryPlayerFromDB(ctx, account)
}

func (s *PlayerService) queryPlayerFromDB(ctx context.Context, account string) (*PlayerInfo, error) {
	start := time.Now()
	sqlStr := "SELECT player_id, account, password, nickname, level, exp, coin, diamond, create_at, login_at FROM players WHERE account = ?"
	req := &dbproxypb.MySQLQueryReq{Uid: 1, Sql: sqlStr, Args: []string{account}}
	data, _ := proto.Marshal(req)
	resPkt, err := s.DB.Call(ctx, cmdMySQLQuery, data)
	if err != nil {
		s.Log.Errorw("query player from DB failed", map[string]interface{}{"account": account, "error": err.Error(), "duration_ms": time.Since(start).Milliseconds()})
		return nil, err
	}
	var res dbproxypb.MySQLQueryRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		s.Log.Errorw("unmarshal query result failed", map[string]interface{}{"account": account, "error": err.Error()})
		return nil, err
	}
	if !res.Ok || len(res.Rows) == 0 {
		s.Log.Infow("player not found in DB", map[string]interface{}{"account": account, "duration_ms": time.Since(start).Milliseconds()})
		return nil, fmt.Errorf("player not found")
	}
	s.Log.Infow("query player from DB success", map[string]interface{}{"account": account, "player_id": res.Rows[0].Columns[0].Value, "duration_ms": time.Since(start).Milliseconds()})
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
	base := &bizpb.PlayerBase{
		PlayerId: playerID,
		Nickname: account,
		Level:    1,
		Exp:      0,
		Coin:     0,
		Diamond:  0,
		CreateAt: now,
		LoginAt:  now,
	}
	// 写库成功后清除该账号的缓存（如果之前有旧缓存或空值占位）
	if s.AccountCache != nil {
		_ = s.AccountCache.Delete(ctx, account)
	}
	return base, nil
}

// SetToken 设置玩家 Token 到 Redis
func (s *PlayerService) SetToken(ctx context.Context, playerID uint64, token string, ttlSec uint64) error {
	if s.Redis == nil {
		s.Log.Warn("redis not available, skip set token")
		return fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("token:%d", playerID)
	err := s.Redis.Set(ctx, key, token, time.Duration(ttlSec)*time.Second)
	if err != nil {
		s.Log.Errorw("set token failed", map[string]interface{}{"player_id": playerID, "error": err.Error()})
	} else {
		s.Log.Infow("set token success", map[string]interface{}{"player_id": playerID, "ttl_sec": ttlSec})
	}
	return err
}

// GetToken 从 Redis 获取玩家 Token
func (s *PlayerService) GetToken(ctx context.Context, playerID uint64) (string, error) {
	if s.Redis == nil {
		s.Log.Warn("redis not available, skip get token")
		return "", fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("token:%d", playerID)
	val, err := s.Redis.Get(ctx, key)
	if err != nil {
		s.Log.Infow("get token miss", map[string]interface{}{"player_id": playerID, "error": err.Error()})
		return "", err
	}
	s.Log.Infow("get token hit", map[string]interface{}{"player_id": playerID})
	return val, nil
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
