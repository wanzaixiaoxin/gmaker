package service

import (
	"context"
	"fmt"
	"strconv"
	"strings"
	"time"

	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/services/biz-go/internal/dbproxy"

	bizpb "github.com/gmaker/luffa/gen/go/biz"
	chatpb "github.com/gmaker/luffa/gen/go/chat"
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

// PlayerService 玩家业务层（与 DBProxy 解耦）
type PlayerService struct {
	DB    *dbproxy.Client
	Redis *redis.Client
	IDGen *idgen.Snowflake
	Log   *logger.Logger
}

// NewPlayerService 创建玩家服务
func NewPlayerService(db *dbproxy.Client, r *redis.Client, idGen *idgen.Snowflake, log *logger.Logger) *PlayerService {
	return &PlayerService{DB: db, Redis: r, IDGen: idGen, Log: log}
}

// QueryPlayerByAccount 根据账号查询玩家
func (s *PlayerService) QueryPlayerByAccount(ctx context.Context, account string) (*bizpb.PlayerBase, error) {
	sqlStr := "SELECT player_id, nickname, level, exp, coin, diamond, create_at, login_at FROM players WHERE account = ?"
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
	return parsePlayerRow(res.Rows[0])
}

// QueryPlayerByID 根据 ID 查询玩家
func (s *PlayerService) QueryPlayerByID(ctx context.Context, playerID uint64) (*bizpb.PlayerBase, error) {
	sqlStr := "SELECT player_id, nickname, level, exp, coin, diamond, create_at, login_at FROM players WHERE player_id = ?"
	req := &dbproxypb.MySQLQueryReq{Uid: playerID, Sql: sqlStr, Args: []string{strconv.FormatUint(playerID, 10)}}
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
	return parsePlayerRow(res.Rows[0])
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
		Args: []string{strconv.FormatUint(playerID, 10), account, password, account, strconv.FormatUint(now, 10), strconv.FormatUint(now, 10)},
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

// UpdatePlayer 更新玩家数据
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
	resPkt, err := s.DB.Call(ctx, cmdMySQLExec, data)
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

// SetToken 设置玩家 Token 到 Redis
func (s *PlayerService) SetToken(ctx context.Context, playerID uint64, token string, ttlSec uint64) error {
	if s.Redis == nil {
		return fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("token:%d", playerID)
	return s.Redis.Set(ctx, key, token, time.Duration(ttlSec)*time.Second)
}

// GetPlayerRooms 获取玩家加入的聊天室列表（从 Redis）
func (s *PlayerService) GetPlayerRooms(ctx context.Context, playerID uint64) ([]*chatpb.ChatRoomInfo, error) {
	if s.Redis == nil {
		return nil, fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("player:%d:rooms", playerID)
	roomIDs, err := s.Redis.RawClient().SMembers(ctx, key).Result()
	if err != nil {
		return nil, err
	}
	var rooms []*chatpb.ChatRoomInfo
	for _, ridStr := range roomIDs {
		rid, _ := strconv.ParseUint(ridStr, 10, 64)
		if rid == 0 {
			continue
		}
		// 查询房间基本信息（从 Redis room info）
		infoKey := fmt.Sprintf("chat:room:%d:info", rid)
		infoStr, err := s.Redis.Get(ctx, infoKey)
		if err != nil || infoStr == "" {
			// Redis 中没有，跳过（或可从 MySQL 查，简化处理）
			continue
		}
		var info chatpb.ChatRoomInfo
		if proto.Unmarshal([]byte(infoStr), &info) == nil {
			rooms = append(rooms, &info)
		}
	}
	return rooms, nil
}

// EnsurePlayerTable 确保 players 表存在
func EnsurePlayerTable(svc *PlayerService) error {
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
