package model

import (
	"fmt"
	"time"
)

// ============================================================
// Ticket 状态机（单个玩家的匹配生命周期）
// ============================================================

type TicketState int32

const (
	TicketIdle     TicketState = iota // 空闲
	TicketQueuing                     // 排队中
	TicketMatching                    // 候选匹配
	TicketSettling                    // 结算中（已确认进对局）
	TicketRoomed                      // 已进入战斗房间
)

func (s TicketState) String() string {
	switch s {
	case TicketIdle:
		return "idle"
	case TicketQueuing:
		return "queuing"
	case TicketMatching:
		return "matching"
	case TicketSettling:
		return "settling"
	case TicketRoomed:
		return "roomed"
	default:
		return "unknown"
	}
}

// ticketTransitions 合法状态转移表
var ticketTransitions = map[TicketState][]TicketState{
	TicketIdle:     {TicketQueuing},
	TicketQueuing:  {TicketMatching, TicketIdle},    // 匹配或取消/超时
	TicketMatching: {TicketSettling, TicketQueuing}, // 确认或回队列
	TicketSettling: {TicketRoomed, TicketQueuing},   // 进房或回队列
	TicketRoomed:   {TicketIdle},                    // 战斗结束
}

func (s TicketState) CanTransitionTo(target TicketState) bool {
	for _, t := range ticketTransitions[s] {
		if t == target {
			return true
		}
	}
	return false
}

// ============================================================
// Match 状态机（一场对局的生命周期）
// ============================================================

type MatchState int32

const (
	MatchDraft      MatchState = iota // 候选（引擎算出，尚未确认）
	MatchConfirmed                    // 已确认（CAS通过）
	MatchRoomPrep                     // 房间创建中
	MatchRetry                        // 重试创建房间
	MatchNotifying                    // 通知玩家进房
	MatchComplete                     // 完成
	MatchAborted                      // 流产（CAS失败）
	MatchRollback                     // 回滚（房间创建失败）
	MatchPartial                      // 部分完成（部分玩家超时）
)

func (s MatchState) String() string {
	switch s {
	case MatchDraft:
		return "draft"
	case MatchConfirmed:
		return "confirmed"
	case MatchRoomPrep:
		return "room_prep"
	case MatchRetry:
		return "retry"
	case MatchNotifying:
		return "notifying"
	case MatchComplete:
		return "complete"
	case MatchAborted:
		return "aborted"
	case MatchRollback:
		return "rollback"
	case MatchPartial:
		return "partial"
	default:
		return "unknown"
	}
}

// IsTerminal 是否终态
func (s MatchState) IsTerminal() bool {
	return s == MatchComplete || s == MatchAborted || s == MatchRollback || s == MatchPartial
}

// matchTransitions 合法状态转移表
var matchTransitions = map[MatchState][]MatchState{
	MatchDraft:     {MatchConfirmed, MatchAborted},
	MatchConfirmed: {MatchRoomPrep},
	MatchRoomPrep:  {MatchNotifying, MatchRetry},
	MatchRetry:     {MatchRoomPrep, MatchRollback},
	MatchNotifying: {MatchComplete, MatchPartial},
	MatchRollback:  {},
	MatchAborted:   {},
	MatchComplete:  {},
	MatchPartial:   {},
}

func (s MatchState) CanTransitionTo(target MatchState) bool {
	for _, t := range matchTransitions[s] {
		if t == target {
			return true
		}
	}
	return false
}

// ============================================================
// 数据结构
// ============================================================

// Ticket 玩家匹配票据
type Ticket struct {
	UID       uint64      `json:"uid"`
	MMR       int32       `json:"mmr"`
	Mode      int32       `json:"mode"`
	TeamSize  int32       `json:"team_size"`
	State     TicketState `json:"state"`
	MatchID   uint64      `json:"match_id,omitempty"` // 当前参与的对局ID
	EnqueueAt time.Time   `json:"enqueue_at"`         // 入队时间
	Priority  int32       `json:"priority"`           // 优先级（等待越久越高）
}

// WaitDuration 等待时长
func (t *Ticket) WaitDuration() time.Duration {
	if t.EnqueueAt.IsZero() {
		return 0
	}
	return time.Since(t.EnqueueAt)
}

// WaitMs 等待毫秒数
func (t *Ticket) WaitMs() int32 {
	return int32(t.WaitDuration().Milliseconds())
}

// IsReadyForMatch 是否已过蓄水期，可以参与匹配
// 蓄水期内不参与匹配，等待更多合适的人进入队列
func (t *Ticket) IsReadyForMatch() bool {
	return t.WaitMs() >= AccumulateWindowMs
}

// MatchQuality 与另一个 Ticket 的匹配质量（MMR 差值越小越好）
func (t *Ticket) MatchQuality(other *Ticket) int32 {
	diff := t.MMR - other.MMR
	if diff < 0 {
		diff = -diff
	}
	return diff
}

// Key 桶排序键（用于 MMR 分桶）
func (t *Ticket) BucketIndex() int32 {
	// 每 300 MMR 一个桶
	return t.MMR / 300
}

// Match 一场候选对局
type Match struct {
	ID        uint64     `json:"id"`
	State     MatchState `json:"state"`
	Mode      int32      `json:"mode"`
	TeamSize  int32      `json:"team_size"`
	TeamA     []uint64   `json:"team_a"`
	TeamB     []uint64   `json:"team_b"`
	RoomID    uint64     `json:"room_id,omitempty"`
	Retries   int        `json:"retries"`
	CreatedAt time.Time  `json:"created_at"`
	UpdatedAt time.Time  `json:"updated_at"`
}

// TotalPlayers 总人数
func (m *Match) TotalPlayers() int {
	return len(m.TeamA) + len(m.TeamB)
}

// AllUIDs 所有玩家UID
func (m *Match) AllUIDs() []uint64 {
	all := make([]uint64, 0, m.TotalPlayers())
	all = append(all, m.TeamA...)
	all = append(all, m.TeamB...)
	return all
}

// AvgMMR 平均MMR
func (m *Match) AvgMMR(tickets map[uint64]*Ticket) float64 {
	var sum int32
	count := 0
	for _, uid := range m.AllUIDs() {
		if t, ok := tickets[uid]; ok {
			sum += t.MMR
			count++
		}
	}
	if count == 0 {
		return 0
	}
	return float64(sum) / float64(count)
}

// ============================================================
// 匹配引擎参数
// ============================================================

const (
	BucketMMRRange   = 300  // 每桶MMR范围
	MaxRetries       = 3    // 房间创建最大重试次数
	DefaultTeamSize  = 5    // 默认每队人数
	TickIntervalMs   = 200  // 匹配引擎 tick 间隔（毫秒）
	MatchWindowSec   = 5    // 匹配窗口（秒）
	TicksPerWindow   = MatchWindowSec * 1000 / TickIntervalMs // 25次

	// 搜索半径参数
	BaseSearchRadius     = 20   // 基础搜索半径（MMR）
	MaxSearchRadius      = 300  // 最大搜索半径（不超过一个桶）
	PopRadiusFactor      = 10   // 队列人数因子（radius += queueLen / factor）
	WaitRadiusFactor     = 50   // 等待时间因子（radius += waitMs / factor）

	// 蓄水等待窗口（三阶段策略）
	// 阶段 A：人数够 → 立即匹配
	// 阶段 B：人数不够 → 等待 AccumulateWindowMs 蓄水
	// 阶段 C：蓄水超时后人数够了 → 放宽 MMR 条件匹配
	AccumulateWindowMs = 2000 // 蓄水等待窗口：人数不足时至少等 2 秒再尝试放宽匹配

	// 准入门参数
	MaxQueueSize         = 50000  // 最大队列容量
	AdmissionRateLimit   = 10000  // 每秒最大入队数

	// Redis 持久化
	RedisKeyTTL          = 60     // 秒
)

// SearchRadius 计算动态搜索半径
func SearchRadius(queueLen int, waitMs int32) int32 {
	radius := int32(BaseSearchRadius)

	// 队列越多人越少 → 搜索越精准（半径不变或缩小）
	// 队列越少人越多 → 搜索越宽松（半径增大）
	if queueLen < 100 {
		radius += int32(queueLen) / int32(PopRadiusFactor)
	} else {
		radius += int32(queueLen) / int32(PopRadiusFactor)
	}

	// 等待越久 → 搜索越宽松
	waitBonus := waitMs / int32(WaitRadiusFactor)
	radius += waitBonus

	// 限制最大范围
	if radius > int32(MaxSearchRadius) {
		radius = int32(MaxSearchRadius)
	}
	return radius
}

// ValidateTransition 校验并返回错误
func ValidateTransition(current, target TicketState) error {
	if !current.CanTransitionTo(target) {
		return fmt.Errorf("invalid ticket transition: %s → %s", current, target)
	}
	return nil
}

func ValidateMatchTransition(current, target MatchState) error {
	if !current.CanTransitionTo(target) {
		return fmt.Errorf("invalid match transition: %s → %s", current, target)
	}
	return nil
}
