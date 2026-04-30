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

// PlayerService 玩家业务层（只操作 player_profiles 表）
type PlayerService struct {
	DB           *dbproxy.Client
	Redis        *redis.Client
	IDGen        *idgen.Snowflake
	Log          *logger.Logger
	BotMasterKey string // 机器人内部绑定密钥，若 token 等于此值则跳过 Redis 校验
}

// NewPlayerService 创建玩家服务
func NewPlayerService(db *dbproxy.Client, r *redis.Client, idGen *idgen.Snowflake, log *logger.Logger, botMasterKey string) *PlayerService {
	return &PlayerService{DB: db, Redis: r, IDGen: idGen, Log: log, BotMasterKey: botMasterKey}
}

// QueryPlayerByID 根据 ID 查询玩家资料（自动延迟初始化）
func (s *PlayerService) QueryPlayerByID(ctx context.Context, playerID uint64) (*bizpb.PlayerBase, error) {
	sqlStr := "SELECT player_id, nickname, level, exp, coin, diamond, create_at, login_at FROM player_profiles WHERE player_id = ?"
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
		// 延迟初始化：profile 不存在时自动创建默认值
		return s.initDefaultProfile(ctx, playerID)
	}
	return parsePlayerRow(res.Rows[0]), nil
}

// initDefaultProfile 为尚未建立 profile 的玩家插入默认资料
func (s *PlayerService) initDefaultProfile(ctx context.Context, playerID uint64) (*bizpb.PlayerBase, error) {
	now := uint64(time.Now().Unix())
	sqlStr := "INSERT IGNORE INTO player_profiles (player_id, nickname, level, exp, coin, diamond, create_at, login_at) VALUES (?, ?, 1, 0, 0, 0, ?, ?)"
	req := &dbproxypb.MySQLExecReq{
		Uid:  playerID,
		Sql:  sqlStr,
		Args: []string{strconv.FormatUint(playerID, 10), fmt.Sprintf("Player%d", playerID), strconv.FormatUint(now, 10), strconv.FormatUint(now, 10)},
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
	// 无论 INSERT IGNORE 是否实际插入，都返回默认值
	return &bizpb.PlayerBase{
		PlayerId: playerID,
		Nickname: fmt.Sprintf("Player%d", playerID),
		Level:    1,
		Exp:      0,
		Coin:     0,
		Diamond:  0,
		CreateAt: now,
		LoginAt:  now,
	}, nil
}

// CreatePlayer 创建玩家资料（用于内部/NPC 创建或延迟初始化兜底）
func (s *PlayerService) CreatePlayer(ctx context.Context, playerID uint64, nickname string) (*bizpb.PlayerBase, error) {
	now := uint64(time.Now().Unix())
	if nickname == "" {
		nickname = fmt.Sprintf("Player%d", playerID)
	}
	sqlStr := "INSERT INTO player_profiles (player_id, nickname, level, exp, coin, diamond, create_at, login_at) VALUES (?, ?, 1, 0, 0, 0, ?, ?)"
	req := &dbproxypb.MySQLExecReq{
		Uid:  playerID,
		Sql:  sqlStr,
		Args: []string{strconv.FormatUint(playerID, 10), nickname, strconv.FormatUint(now, 10), strconv.FormatUint(now, 10)},
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
		Nickname: nickname,
		Level:    1,
		Exp:      0,
		Coin:     0,
		Diamond:  0,
		CreateAt: now,
		LoginAt:  now,
	}, nil
}

// UpdatePlayer 更新玩家资料
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

	sqlStr := fmt.Sprintf("UPDATE player_profiles SET %s WHERE player_id = ?", strings.Join(parts, ", "))
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

// VerifyToken 验证玩家 Token（从 Redis）
func (s *PlayerService) VerifyToken(ctx context.Context, playerID uint64, token string) (bool, error) {
	if s.Redis == nil {
		return false, fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("token:%d", playerID)
	storedToken, err := s.Redis.Get(ctx, key)
	if err != nil {
		return false, err
	}
	return storedToken == token, nil
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
		infoKey := fmt.Sprintf("chat:room:%d:info", rid)
		infoStr, err := s.Redis.Get(ctx, infoKey)
		if err != nil || infoStr == "" {
			continue
		}
		var info chatpb.ChatRoomInfo
		if proto.Unmarshal([]byte(infoStr), &info) == nil {
			rooms = append(rooms, &info)
		}
	}
	return rooms, nil
}

// EnsurePlayerProfileTable 确保 player_profiles 表存在
func EnsurePlayerProfileTable(svc *PlayerService) error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	sqlStr := `CREATE TABLE IF NOT EXISTS player_profiles (
		player_id BIGINT PRIMARY KEY,
		nickname VARCHAR(64) NOT NULL,
		level INT DEFAULT 1,
		exp BIGINT DEFAULT 0,
		coin BIGINT DEFAULT 0,
		diamond BIGINT DEFAULT 0,
		is_bot TINYINT DEFAULT 0,
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

func parsePlayerRow(row *dbproxypb.MySQLRow) *bizpb.PlayerBase {
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
