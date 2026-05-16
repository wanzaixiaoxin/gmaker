package store

import (
	"context"
	"encoding/json"
	"fmt"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/redis/go-redis/v9"

	"github.com/gmaker/luffa/services/match-go/internal/model"
)

const (
	keyMatchActive   = "match:active:%d"
	keyMatchSettling = "match:settling:%d"
	keyMatchCounter  = "match:id:counter"
	keyMatchHistory  = "match:history"
)

// ============================================================
// 内存存储（主存储，所有热数据）
// ============================================================

// MemoryStore 内存主存储
type MemoryStore struct {
	mu          sync.RWMutex
	tickets     map[uint64]*model.Ticket // uid → ticket
	matches     map[uint64]*model.Match  // matchID → match
	uidToMatch  map[uint64]uint64        // uid → matchID（正在参与的对局）
}

func NewMemoryStore() *MemoryStore {
	return &MemoryStore{
		tickets:    make(map[uint64]*model.Ticket),
		matches:    make(map[uint64]*model.Match),
		uidToMatch: make(map[uint64]uint64),
	}
}

// ─── Ticket 操作 ───

func (m *MemoryStore) PutTicket(t *model.Ticket) {
	m.mu.Lock()
	m.tickets[t.UID] = t
	m.mu.Unlock()
}

func (m *MemoryStore) GetTicket(uid uint64) (*model.Ticket, bool) {
	m.mu.RLock()
	t, ok := m.tickets[uid]
	m.mu.RUnlock()
	return t, ok
}

func (m *MemoryStore) DeleteTicket(uid uint64) {
	m.mu.Lock()
	delete(m.tickets, uid)
	delete(m.uidToMatch, uid)
	m.mu.Unlock()
}

func (m *MemoryStore) UpdateTicketState(uid uint64, state model.TicketState) error {
	m.mu.Lock()
	t, ok := m.tickets[uid]
	if !ok {
		m.mu.Unlock()
		return fmt.Errorf("ticket not found: uid=%d", uid)
	}
	if err := model.ValidateTransition(t.State, state); err != nil {
		m.mu.Unlock()
		return err
	}
	t.State = state
	m.mu.Unlock()
	return nil
}

// GetQueuingTickets 获取所有排队中的 Ticket（按模式分组）
func (m *MemoryStore) GetQueuingTickets(mode int32) []*model.Ticket {
	m.mu.RLock()
	var result []*model.Ticket
	for _, t := range m.tickets {
		if t.State == model.TicketQueuing && t.Mode == mode {
			result = append(result, t)
		}
	}
	m.mu.RUnlock()
	return result
}

// CountQueuing 排队人数
func (m *MemoryStore) CountQueuing() int {
	m.mu.RLock()
	count := 0
	for _, t := range m.tickets {
		if t.State == model.TicketQueuing {
			count++
		}
	}
	m.mu.RUnlock()
	return count
}

// ─── Match 操作 ───

func (m *MemoryStore) PutMatch(match *model.Match) {
	m.mu.Lock()
	m.matches[match.ID] = match
	// 建立 uid → matchID 映射
	for _, uid := range match.AllUIDs() {
		m.uidToMatch[uid] = match.ID
	}
	m.mu.Unlock()
}

func (m *MemoryStore) GetMatch(matchID uint64) (*model.Match, bool) {
	m.mu.RLock()
	match, ok := m.matches[matchID]
	m.mu.RUnlock()
	return match, ok
}

func (m *MemoryStore) UpdateMatchState(matchID uint64, state model.MatchState) error {
	m.mu.Lock()
	match, ok := m.matches[matchID]
	if !ok {
		m.mu.Unlock()
		return fmt.Errorf("match not found: id=%d", matchID)
	}
	if err := model.ValidateMatchTransition(match.State, state); err != nil {
		m.mu.Unlock()
		return err
	}
	match.State = state
	match.UpdatedAt = time.Now()
	m.mu.Unlock()
	return nil
}

func (m *MemoryStore) DeleteMatch(matchID uint64) {
	m.mu.Lock()
	delete(m.matches, matchID)
	m.mu.Unlock()
}

func (m *MemoryStore) GetMatchByUID(uid uint64) (*model.Match, bool) {
	m.mu.RLock()
	matchID, ok := m.uidToMatch[uid]
	if !ok {
		m.mu.RUnlock()
		return nil, false
	}
	match, ok := m.matches[matchID]
	m.mu.RUnlock()
	return match, ok
}

// ─── 批量操作 ───

// BindMatch 绑定 Ticket 到 Match
func (m *MemoryStore) BindMatch(uids []uint64, matchID uint64) {
	m.mu.Lock()
	for _, uid := range uids {
		if t, ok := m.tickets[uid]; ok {
			t.MatchID = matchID
		}
		m.uidToMatch[uid] = matchID
	}
	m.mu.Unlock()
}

// ReleaseTickets 释放 Ticket 回 Queuing 状态
func (m *MemoryStore) ReleaseTickets(uids []uint64) {
	m.mu.Lock()
	for _, uid := range uids {
		if t, ok := m.tickets[uid]; ok {
			t.State = model.TicketQueuing
			t.MatchID = 0
			t.Priority++ // 优先级提高
		}
		delete(m.uidToMatch, uid)
	}
	m.mu.Unlock()
}

// RemoveTickets 移除 Ticket
func (m *MemoryStore) RemoveTickets(uids []uint64) {
	m.mu.Lock()
	for _, uid := range uids {
		delete(m.tickets, uid)
		delete(m.uidToMatch, uid)
	}
	m.mu.Unlock()
}

// ============================================================
// Redis 检查点存储（关键状态持久化）
// ============================================================

// RedisStore Redis 检查点存储
type RedisStore struct {
	rdb redis.UniversalClient
}

func NewRedisStore(rdb redis.UniversalClient) *RedisStore {
	return &RedisStore{rdb: rdb}
}

// SaveMatchRecord 持久化对局记录（关键状态转移时调用）
func (r *RedisStore) SaveMatchRecord(m *model.Match) error {
	if r.rdb == nil {
		return nil // Redis 不可用时降级运行
	}
	ctx := context.Background()
	key := fmt.Sprintf(keyMatchActive, m.ID)

	fields := map[string]interface{}{
		"state":      m.State.String(),
		"team_a":     uidsToString(m.TeamA),
		"team_b":     uidsToString(m.TeamB),
		"room_id":    m.RoomID,
		"retries":    m.Retries,
		"mode":       m.Mode,
		"team_size":  m.TeamSize,
		"created_at": m.CreatedAt.Unix(),
		"updated_at": time.Now().Unix(),
	}

	pipe := r.rdb.Pipeline()
	pipe.HSet(ctx, key, fields)
	pipe.Expire(ctx, key, time.Duration(model.RedisKeyTTL)*time.Second)
	_, err := pipe.Exec(ctx)
	return err
}

// DeleteMatchRecord 删除对局记录（完成后清理）
func (r *RedisStore) DeleteMatchRecord(matchID uint64) error {
	if r.rdb == nil {
		return nil
	}
	return r.rdb.Del(context.Background(), fmt.Sprintf(keyMatchActive, matchID)).Err()
}

// LoadActiveMatches 加载所有未完成的对局（崩溃恢复）
func (r *RedisStore) LoadActiveMatches() ([]*model.Match, error) {
	if r.rdb == nil {
		return nil, nil
	}
	ctx := context.Background()
	var records []*model.Match
	var cursor uint64
	for {
		keys, next, err := r.rdb.Scan(ctx, cursor, "match:active:*", 100).Result()
		if err != nil {
			return nil, err
		}
		for _, key := range keys {
			data, err := r.rdb.HGetAll(ctx, key).Result()
			if err != nil {
				continue
			}
			m := parseMatchRecord(data)
			if m != nil {
				records = append(records, m)
			}
		}
		cursor = next
		if cursor == 0 {
			break
		}
	}
	return records, nil
}

// SaveSettlingRef 保存 Ticket 结算引用
func (r *RedisStore) SaveSettlingRef(uid uint64, matchID uint64) error {
	if r.rdb == nil {
		return nil
	}
	return r.rdb.SetEx(context.Background(),
		fmt.Sprintf(keyMatchSettling, uid),
		matchID,
		time.Duration(model.RedisKeyTTL)*time.Second,
	).Err()
}

// DeleteSettlingRef 删除 Ticket 结算引用
func (r *RedisStore) DeleteSettlingRef(uid uint64) error {
	if r.rdb == nil {
		return nil
	}
	return r.rdb.Del(context.Background(), fmt.Sprintf(keyMatchSettling, uid)).Err()
}

// CASConfirm Lua 原子 CAS 确认：检查所有 Ticket 是否空闲
// KEYS[1..N] = match:settling:uid1, match:settling:uid2, ...
// ARGV[1]    = match_id
// 返回 1=成功 0=有人已被占用
var casConfirmScript = redis.NewScript(`
local all_free = true
for i = 1, #KEYS do
    local existing = redis.call('GET', KEYS[i])
    if existing then
        all_free = false
        break
    end
end
if not all_free then
    return 0
end
for i = 1, #KEYS do
    redis.call('SETEX', KEYS[i], 60, ARGV[1])
end
return 1
`)

// CASConfirm 原子确认对局
func (r *RedisStore) CASConfirm(matchID uint64, allUIDs []uint64) (bool, error) {
	if r.rdb == nil {
		return true, nil // Redis 不可用时降级：直接通过
	}
	keys := make([]string, len(allUIDs))
	for i, uid := range allUIDs {
		keys[i] = fmt.Sprintf(keyMatchSettling, uid)
	}
	result, err := casConfirmScript.Run(context.Background(), r.rdb, keys, matchID).Int()
	if err != nil {
		return false, err
	}
	return result == 1, nil
}

// NextMatchID 自增 match_id
func (r *RedisStore) NextMatchID() (uint64, error) {
	if r.rdb == nil {
		return uint64(time.Now().UnixNano()), nil // 降级：用时间戳
	}
	return r.rdb.Incr(context.Background(), keyMatchCounter).Uint64()
}

// ArchiveMatch 归档匹配记录到 Redis List
func (r *RedisStore) ArchiveMatch(m *model.Match) error {
	if r.rdb == nil {
		return nil
	}
	data, _ := json.Marshal(m)
	ctx := context.Background()
	pipe := r.rdb.Pipeline()
	pipe.LPush(ctx, keyMatchHistory, data)
	pipe.LTrim(ctx, keyMatchHistory, 0, 9999) // 保留最近 10000 条
	_, err := pipe.Exec(ctx)
	return err
}

// ============================================================
// 辅助函数
// ============================================================

func uidsToString(uids []uint64) string {
	parts := make([]string, len(uids))
	for i, uid := range uids {
		parts[i] = strconv.FormatUint(uid, 10)
	}
	return strings.Join(parts, ",")
}

func stringToUIDs(s string) []uint64 {
	if s == "" {
		return nil
	}
	parts := strings.Split(s, ",")
	uids := make([]uint64, 0, len(parts))
	for _, p := range parts {
		if uid, err := strconv.ParseUint(p, 10, 64); err == nil {
			uids = append(uids, uid)
		}
	}
	return uids
}

func parseMatchRecord(data map[string]string) *model.Match {
	m := &model.Match{
		TeamA:    stringToUIDs(data["team_a"]),
		TeamB:    stringToUIDs(data["team_b"]),
		Mode:     parseInt32(data["mode"]),
		TeamSize: parseInt32(data["team_size"]),
	}
	if id, err := strconv.ParseUint(data["id"], 10, 64); err == nil {
		m.ID = id
	}
	if roomID, err := strconv.ParseUint(data["room_id"], 10, 64); err == nil {
		m.RoomID = roomID
	}
	m.Retries = int(parseInt32(data["retries"]))
	m.State = parseMatchState(data["state"])
	if ts, err := strconv.ParseInt(data["created_at"], 10, 64); err == nil {
		m.CreatedAt = time.Unix(ts, 0)
	}
	return m
}

func parseInt32(s string) int32 {
	v, _ := strconv.ParseInt(s, 10, 32)
	return int32(v)
}

func parseMatchState(s string) model.MatchState {
	switch s {
	case "confirmed":
		return model.MatchConfirmed
	case "room_prep":
		return model.MatchRoomPrep
	case "retry":
		return model.MatchRetry
	case "notifying":
		return model.MatchNotifying
	case "complete":
		return model.MatchComplete
	case "aborted":
		return model.MatchAborted
	case "rollback":
		return model.MatchRollback
	case "partial":
		return model.MatchPartial
	default:
		return model.MatchDraft
	}
}
