package service

import (
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/hex"
	"fmt"
	"strconv"
	"time"
	"unicode"

	"github.com/gmaker/luffa/common/go/cache"
	"github.com/gmaker/luffa/common/go/crypto"
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

// AccountService 账号认证服务（只操作 accounts 表）
type AccountService struct {
	DB           dbproxy.Client
	Redis        *redis.Client
	IDGen        *idgen.Snowflake
	AccountCache *cache.Cache[*AccountInfo] // account -> AccountInfo（不含业务字段）
	Log          *logger.Logger
}

// NewAccountService 创建账号服务
func NewAccountService(db dbproxy.Client, r *redis.Client, idGen *idgen.Snowflake, accountCache *cache.Cache[*AccountInfo], log *logger.Logger) *AccountService {
	if log == nil {
		log = logger.NewWithService("login", "unknown")
	}
	return &AccountService{DB: db, Redis: r, IDGen: idGen, AccountCache: accountCache, Log: log}
}

// AccountInfo 账号认证信息（仅含认证相关字段）
type AccountInfo struct {
	PlayerId uint64
	Account  string
	Password string
	CreateAt uint64
}

// ToPlayerBase 转为 PlayerBase（注册/登录回包需要最小化玩家信息）
func (a *AccountInfo) ToPlayerBase() *bizpb.PlayerBase {
	return &bizpb.PlayerBase{
		PlayerId: a.PlayerId,
		CreateAt: a.CreateAt,
	}
}

// QueryAccountByAccount 根据账号名查询账号信息（带缓存）
func (s *AccountService) QueryAccountByAccount(ctx context.Context, account string) (*AccountInfo, error) {
	if s.AccountCache != nil {
		info, err := s.AccountCache.GetOrLoad(ctx, account, func(ctx context.Context, key string) (*AccountInfo, error) {
			s.Log.Infow("cache miss, loading from DB", map[string]interface{}{"account": key})
			return s.queryAccountFromDB(ctx, key)
		})
		if err == nil {
			s.Log.Infow("cache hit", map[string]interface{}{"account": account, "player_id": info.PlayerId})
		} else {
			s.Log.Infow("cache miss after load", map[string]interface{}{"account": account, "error": err.Error()})
		}
		return info, err
	}
	s.Log.Infow("cache disabled, query DB directly", map[string]interface{}{"account": account})
	return s.queryAccountFromDB(ctx, account)
}

func (s *AccountService) queryAccountFromDB(ctx context.Context, account string) (*AccountInfo, error) {
	start := time.Now()
	sqlStr := "SELECT player_id, account, password, create_at FROM accounts WHERE account = ?"
	req := &dbproxypb.MySQLQueryReq{Uid: 1, Sql: sqlStr, Args: []string{account}}
	data, _ := proto.Marshal(req)
	resPkt, err := s.DB.Call(ctx, cmdMySQLQuery, data)
	if err != nil {
		s.Log.Errorw("query account from DB failed", map[string]interface{}{"account": account, "error": err.Error(), "duration_ms": time.Since(start).Milliseconds()})
		return nil, err
	}
	var res dbproxypb.MySQLQueryRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		s.Log.Errorw("unmarshal query result failed", map[string]interface{}{"account": account, "error": err.Error()})
		return nil, err
	}
	if !res.Ok || len(res.Rows) == 0 {
		s.Log.Infow("account not found in DB", map[string]interface{}{"account": account, "duration_ms": time.Since(start).Milliseconds()})
		return nil, fmt.Errorf("account not found")
	}
	s.Log.Infow("query account from DB success", map[string]interface{}{"account": account, "player_id": res.Rows[0].Columns[0].Value, "duration_ms": time.Since(start).Milliseconds()})
	return parseAccountRow(res.Rows[0]), nil
}

// CreateAccount 创建账号（仅写入 accounts 表）
func (s *AccountService) CreateAccount(ctx context.Context, account, password string) (*bizpb.PlayerBase, error) {
	now := uint64(time.Now().Unix())
	playerIDRaw, err := s.IDGen.NextID()
	if err != nil {
		return nil, fmt.Errorf("generate player id failed: %w", err)
	}
	playerID := uint64(playerIDRaw)
	sqlStr := "INSERT INTO accounts (player_id, account, password, status, create_at) VALUES (?, ?, ?, 0, ?)"
	req := &dbproxypb.MySQLExecReq{
		Uid:  playerID,
		Sql:  sqlStr,
		Args: []string{strconv.FormatUint(playerID, 10), account, HashPassword(password), strconv.FormatUint(now, 10)},
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
		CreateAt: now,
	}
	// 写库成功后清除该账号的缓存（如果之前有旧缓存或空值占位）
	if s.AccountCache != nil {
		_ = s.AccountCache.Delete(ctx, account)
	}
	return base, nil
}

// SetToken 设置玩家 Token 到 Redis
func (s *AccountService) SetToken(ctx context.Context, playerID uint64, token string, ttlSec uint64) error {
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
func (s *AccountService) GetToken(ctx context.Context, playerID uint64) (string, error) {
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

// VerifyToken 验证 Token 是否有效（反向查找，当前未实现完整反向索引）
func (s *AccountService) VerifyToken(ctx context.Context, token string) (uint64, error) {
	if s.Redis == nil {
		return 0, fmt.Errorf("redis not available")
	}
	// 生产环境应使用 token:lookup:{token_hash} -> player_id
	return 0, fmt.Errorf("not implemented: use player_id + token comparison instead")
}

func parseAccountRow(row *dbproxypb.MySQLRow) *AccountInfo {
	m := make(map[string]string)
	for _, col := range row.Columns {
		m[col.Name] = col.Value
	}
	pid, _ := strconv.ParseUint(m["player_id"], 10, 64)
	cat, _ := strconv.ParseUint(m["create_at"], 10, 64)
	return &AccountInfo{
		PlayerId: pid,
		Account:  m["account"],
		Password: m["password"],
		CreateAt: cat,
	}
}

// HashPassword 使用 Argon2id 哈希密码
func HashPassword(pwd string) string {
	hash, err := crypto.HashPasswordArgon2id(pwd)
	if err != nil {
		h := sha256.Sum256([]byte(pwd))
		return hex.EncodeToString(h[:])
	}
	return hash
}

// VerifyPassword 验证密码，支持 Argon2id 和旧 SHA256 兼容
func VerifyPassword(pwd, storedHash string) (ok bool, needUpgrade bool) {
	if crypto.IsArgon2idHash(storedHash) {
		ok, _ = crypto.VerifyPasswordArgon2id(pwd, storedHash)
		return ok, false
	}
	// 兼容旧 SHA256（无盐）
	h := sha256.Sum256([]byte(pwd))
	if hex.EncodeToString(h[:]) == storedHash {
		return true, true
	}
	return false, false
}

// GenerateToken 生成密码学安全的随机 token
func GenerateToken() string {
	b := make([]byte, 32)
	rand.Read(b)
	return hex.EncodeToString(b)
}

// GenerateRefreshToken 生成 Refresh Token
func GenerateRefreshToken() string {
	return GenerateToken()
}

// UpdatePassword 更新账号密码
func (s *AccountService) UpdatePassword(ctx context.Context, playerID uint64, newPassword string) error {
	newHash := HashPassword(newPassword)
	sqlStr := "UPDATE accounts SET password = ? WHERE player_id = ?"
	req := &dbproxypb.MySQLExecReq{
		Uid:  playerID,
		Sql:  sqlStr,
		Args: []string{newHash, strconv.FormatUint(playerID, 10)},
	}
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
		return fmt.Errorf("update password failed: %s", res.Error)
	}
	return nil
}

// SetRefreshToken 设置 Refresh Token
func (s *AccountService) SetRefreshToken(ctx context.Context, playerID uint64, token string, ttlSec uint64) error {
	if s.Redis == nil {
		return fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("refresh:%d", playerID)
	return s.Redis.Set(ctx, key, token, time.Duration(ttlSec)*time.Second)
}

// GetRefreshToken 获取 Refresh Token
func (s *AccountService) GetRefreshToken(ctx context.Context, playerID uint64) (string, error) {
	if s.Redis == nil {
		return "", fmt.Errorf("redis not available")
	}
	key := fmt.Sprintf("refresh:%d", playerID)
	return s.Redis.Get(ctx, key)
}

// ValidateInput 校验账号密码输入合法性
func ValidateInput(account, password string) error {
	if len(account) < 3 || len(account) > 32 {
		return fmt.Errorf("account length must be 3-32")
	}
	if len(password) < 6 || len(password) > 64 {
		return fmt.Errorf("password length must be 6-64")
	}
	for _, r := range account {
		if !unicode.IsLetter(r) && !unicode.IsDigit(r) && r != '_' {
			return fmt.Errorf("account can only contain letters, digits and underscore")
		}
	}
	return nil
}

// EnsureAccountTable 确保 accounts 表存在
func EnsureAccountTable(svc *AccountService) error {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	sqlStr := `CREATE TABLE IF NOT EXISTS accounts (
		player_id BIGINT PRIMARY KEY,
		account VARCHAR(64) UNIQUE NOT NULL,
		password VARCHAR(128) NOT NULL,
		status TINYINT DEFAULT 0,
		create_at BIGINT DEFAULT 0
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
