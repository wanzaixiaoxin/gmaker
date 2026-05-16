package engine

import (
	"sort"
	"sync"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/services/match-go/internal/model"
)

// ============================================================
// MMR 分桶（减少锁竞争）
// ============================================================

// Bucket 单个 MMR 桶
type Bucket struct {
	mu      sync.RWMutex
	tickets []*model.Ticket // 按 MMR 排序
}

func newBucket() *Bucket {
	return &Bucket{
		tickets: make([]*model.Ticket, 0),
	}
}

func (b *Bucket) Push(t *model.Ticket) {
	b.mu.Lock()
	// 按优先级+MMR插入排序
	b.tickets = append(b.tickets, t)
	sort.Slice(b.tickets, func(i, j int) bool {
		if b.tickets[i].Priority != b.tickets[j].Priority {
			return b.tickets[i].Priority > b.tickets[j].Priority // 优先级高的在前
		}
		return b.tickets[i].MMR < b.tickets[j].MMR
	})
	b.mu.Unlock()
}

func (b *Bucket) Remove(uid uint64) {
	b.mu.Lock()
	for i, t := range b.tickets {
		if t.UID == uid {
			b.tickets = append(b.tickets[:i], b.tickets[i+1:]...)
			break
		}
	}
	b.mu.Unlock()
}

func (b *Bucket) Snapshot() []*model.Ticket {
	b.mu.RLock()
	snap := make([]*model.Ticket, len(b.tickets))
	copy(snap, b.tickets)
	b.mu.RUnlock()
	return snap
}

func (b *Bucket) Len() int {
	b.mu.RLock()
	n := len(b.tickets)
	b.mu.RUnlock()
	return n
}

// ============================================================
// MMR Bucket Pool
// ============================================================

// BucketPool MMR 桶池，按模式和桶索引组织
type BucketPool struct {
	mu      sync.RWMutex
	buckets map[int32]map[int32]*Bucket // mode → bucketIndex → Bucket
}

func NewBucketPool() *BucketPool {
	return &BucketPool{
		buckets: make(map[int32]map[int32]*Bucket),
	}
}

func (p *BucketPool) getOrCreateBucket(mode, bucketIdx int32) *Bucket {
	p.mu.Lock()
	modeBuckets, ok := p.buckets[mode]
	if !ok {
		modeBuckets = make(map[int32]*Bucket)
		p.buckets[mode] = modeBuckets
	}
	b, ok := modeBuckets[bucketIdx]
	if !ok {
		b = newBucket()
		modeBuckets[bucketIdx] = b
	}
	p.mu.Unlock()
	return b
}

// Enqueue 将 Ticket 入桶
func (p *BucketPool) Enqueue(t *model.Ticket) {
	bucketIdx := t.BucketIndex()
	b := p.getOrCreateBucket(t.Mode, bucketIdx)
	b.Push(t)
}

// Dequeue 将 Ticket 从桶中移除
func (p *BucketPool) Dequeue(t *model.Ticket) {
	bucketIdx := t.BucketIndex()
	p.mu.RLock()
	modeBuckets, ok := p.buckets[t.Mode]
	p.mu.RUnlock()
	if !ok {
		return
	}
	modeBuckets[bucketIdx].Remove(t.UID)
}

// QueryRange 范围查询（搜索半径内的所有 Ticket）
func (p *BucketPool) QueryRange(mode int32, centerMMR int32, radius int32) []*model.Ticket {
	minBucket := (centerMMR - radius) / model.BucketMMRRange
	maxBucket := (centerMMR + radius) / model.BucketMMRRange

	// 边界保护
	if minBucket < 0 {
		minBucket = 0
	}

	var result []*model.Ticket

	p.mu.RLock()
	modeBuckets, ok := p.buckets[mode]
	if !ok {
		p.mu.RUnlock()
		return result
	}

	// 收集涉及桶的快照
	var snapshots [][]*model.Ticket
	for bi := minBucket; bi <= maxBucket; bi++ {
		if b, ok := modeBuckets[bi]; ok {
			snapshots = append(snapshots, b.Snapshot())
		}
	}
	p.mu.RUnlock()

	// 合并并过滤
	for _, snap := range snapshots {
		for _, t := range snap {
			if t.State != model.TicketQueuing {
				continue
			}
			// MMR 差值在半径内
			diff := t.MMR - centerMMR
			if diff < 0 {
				diff = -diff
			}
			if diff <= radius {
				result = append(result, t)
			}
		}
	}

	return result
}

// TotalQueuing 排队总人数
func (p *BucketPool) TotalQueuing(mode int32) int {
	p.mu.RLock()
	modeBuckets, ok := p.buckets[mode]
	p.mu.RUnlock()
	if !ok {
		return 0
	}
	total := 0
	for _, b := range modeBuckets {
		total += b.Len()
	}
	return total
}

// ============================================================
// 5 秒渐进匹配引擎
// ============================================================

// MatchCandidate 匹配候选结果
type MatchCandidate struct {
	TeamA []*model.Ticket
	TeamB []*model.Ticket
}

// Engine 匹配引擎
type Engine struct {
	pool   *BucketPool
	log    *logger.Logger
	tick   int // 当前 tick 计数（0~24 循环）
}

func NewEngine(pool *BucketPool, log *logger.Logger) *Engine {
	return &Engine{
		pool: pool,
		log:  log,
	}
}

// Tick 单次匹配 tick（每 200ms 调用一次）
func (e *Engine) Tick() []*MatchCandidate {
	e.tick++
	if e.tick >= model.TicksPerWindow {
		e.tick = 0
	}

	// 按模式匹配（目前支持 mode=1 5v5）
	var results []*MatchCandidate
	for _, mode := range []int32{1, 2, 3} {
		candidates := e.matchMode(mode)
		results = append(results, candidates...)
	}
	return results
}

// matchMode 单个模式的匹配（三阶段蓄水策略）
//
// 策略：
//   阶段 A（立即匹配）：队列人数 >= totalNeeded → 立即匹配，选择最优 MMR 组合
//   阶段 B（蓄水等待）：队列人数 < totalNeeded → 等待 AccumulateWindowMs 蓄水
//   阶段 C（放宽条件）：蓄水超时后人数够了 → 扩大 MMR 搜索半径，降低匹配精度门槛
func (e *Engine) matchMode(mode int32) []*MatchCandidate {
	teamSize := e.getTeamSize(mode)
	totalNeeded := int(teamSize * 2)

	// ---- 全量快照 ----
	allTickets := e.pool.QueryRange(mode, 0, 99999)
	queueLen := len(allTickets)

	if queueLen == 0 {
		return nil
	}

	// 计算最长等待时间（判断是否在蓄水期）
	maxWait := int32(0)
	for _, t := range allTickets {
		if w := t.WaitMs(); w > maxWait {
			maxWait = w
		}
	}

	// ---- 阶段 A：人数已够，立即匹配 ----
	if queueLen >= totalNeeded {
		return e.doMatch(allTickets, mode, teamSize, totalNeeded, queueLen, false)
	}

	// ---- 阶段 B：人数不够，检查蓄水窗口 ----
	if maxWait < model.AccumulateWindowMs {
		e.log.Debugf("[Engine] mode=%d accumulating: %d/%d players, oldest=%dms/%dms",
			mode, queueLen, totalNeeded, maxWait, model.AccumulateWindowMs)
		return nil // 蓄水期未到，继续等待
	}

	// ---- 阶段 C：蓄水超时，人数仍不够 ----
	// 此时无法匹配（人数确实不足），记录日志
	e.log.Debugf("[Engine] mode=%d accumulation expired but %d/%d players, keep waiting",
		mode, queueLen, totalNeeded)
	return nil
}

// doMatch 从候选池中执行匹配，relaxed=true 时放宽 MMR 条件
func (e *Engine) doMatch(allTickets []*model.Ticket, mode int32, teamSize int32, totalNeeded int, queueLen int, relaxed bool) []*MatchCandidate {
	// 按优先级排序（等待越久优先级越高）
	sort.Slice(allTickets, func(i, j int) bool {
		return allTickets[i].Priority > allTickets[j].Priority
	})

	var results []*MatchCandidate
	used := make(map[uint64]bool)

	for _, anchor := range allTickets {
		if used[anchor.UID] {
			continue
		}

		// 计算动态搜索半径
		radius := model.SearchRadius(queueLen, anchor.WaitMs())

		// 放宽模式：扩大搜索半径 1.5 倍（降低 MMR 精度门槛）
		if relaxed {
			radius = radius * 3 / 2
			if radius > model.MaxSearchRadius {
				radius = model.MaxSearchRadius
			}
		}

		// 搜索范围内的候选
		nearby := e.pool.QueryRange(mode, anchor.MMR, radius)

		// 过滤已使用 + 不是自己
		var available []*model.Ticket
		for _, t := range nearby {
			if !used[t.UID] && t.UID != anchor.UID {
				available = append(available, t)
			}
		}

		if len(available)+1 < totalNeeded {
			continue
		}

		// 按 MMR 质量排序候选（选最接近 anchor MMR 的）
		sort.Slice(available, func(i, j int) bool {
			return anchor.MatchQuality(available[i]) < anchor.MatchQuality(available[j])
		})

		// 取 top-N 组队
		picked := make([]*model.Ticket, 0, totalNeeded)
		picked = append(picked, anchor)
		for _, t := range available {
			if len(picked) >= totalNeeded {
				break
			}
			picked = append(picked, t)
		}

		if len(picked) < totalNeeded {
			continue
		}

		// 蛇形分队（MMR 排序后交替分配，保证两队实力均衡）
		sort.Slice(picked, func(i, j int) bool {
			return picked[i].MMR > picked[j].MMR
		})

		teamA := make([]*model.Ticket, 0, teamSize)
		teamB := make([]*model.Ticket, 0, teamSize)
		for i, t := range picked {
			if i%2 == 0 {
				teamA = append(teamA, t)
			} else {
				teamB = append(teamB, t)
			}
		}

		// 计算本局匹配质量
		quality := e.evaluateMatchQuality(teamA, teamB)

		// 标记已使用
		for _, t := range picked {
			used[t.UID] = true
		}

		results = append(results, &MatchCandidate{
			TeamA: teamA,
			TeamB: teamB,
		})

		e.log.Infof("[Engine] match found: mode=%d teamA=%v teamB=%v radius=%d quality=%.1f relaxed=%v",
			mode, uids(teamA), uids(teamB), radius, quality, relaxed)
	}

	return results
}

// evaluateMatchQuality 评估一局匹配的质量分数（0~100，越高越好）
// 综合考虑：队内 MMR 方差 + 两队平均 MMR 差距
func (e *Engine) evaluateMatchQuality(teamA, teamB []*model.Ticket) float64 {
	avgA := avgMMR(teamA)
	avgB := avgMMR(teamB)

	// 两队平均 MMR 差距
	teamDiff := avgA - avgB
	if teamDiff < 0 {
		teamDiff = -teamDiff
	}

	// 队内 MMR 标准差
	stdA := stdMMR(teamA, avgA)
	stdB := stdMMR(teamB, avgB)

	// 质量评分：差距越小、方差越小 → 分数越高
	// 满分 100 = teamDiff=0 + std=0
	quality := 100.0 - float64(teamDiff)*0.3 - (stdA+stdB)*0.2
	if quality < 0 {
		quality = 0
	}
	return quality
}

// avgMMR 计算一组 Ticket 的平均 MMR
func avgMMR(tickets []*model.Ticket) float64 {
	if len(tickets) == 0 {
		return 0
	}
	var sum int32
	for _, t := range tickets {
		sum += t.MMR
	}
	return float64(sum) / float64(len(tickets))
}

// stdMMR 计算一组 Ticket 的 MMR 标准差
func stdMMR(tickets []*model.Ticket, avg float64) float64 {
	if len(tickets) <= 1 {
		return 0
	}
	var sum float64
	for _, t := range tickets {
		d := float64(t.MMR) - avg
		sum += d * d
	}
	variance := sum / float64(len(tickets))
	return sqrt(variance)
}

// sqrt 简易平方根（牛顿迭代）
func sqrt(x float64) float64 {
	if x <= 0 {
		return 0
	}
	z := x
	for i := 0; i < 10; i++ {
		z = (z + x/z) / 2
	}
	return z
}

func (e *Engine) getTeamSize(mode int32) int32 {
	switch mode {
	case 1:
		return 5 // 5v5
	case 2:
		return 3 // 3v3
	case 3:
		return 1 // 1v1
	default:
		return model.DefaultTeamSize
	}
}

// ============================================================
// Admission Gate（准入门）
// ============================================================

type AdmissionGate struct {
	maxQueueSize int
}

func NewAdmissionGate(maxQueueSize int) *AdmissionGate {
	return &AdmissionGate{maxQueueSize: maxQueueSize}
}

// Accept 检查是否允许入队
func (g *AdmissionGate) Accept(currentQueueLen int) error {
	if currentQueueLen >= g.maxQueueSize {
		return ErrQueueFull
	}
	return nil
}

// ============================================================
// 错误定义
// ============================================================

var (
	ErrQueueFull      = &MatchError{Code: 1001, Msg: "匹配队列已满，请稍后再试"}
	ErrAlreadyInQueue = &MatchError{Code: 1002, Msg: "已在匹配队列中"}
	ErrNotInQueue     = &MatchError{Code: 1003, Msg: "不在匹配队列中"}
)

type MatchError struct {
	Code int32
	Msg  string
}

func (e *MatchError) Error() string {
	return e.Msg
}

// ============================================================
// 辅助函数
// ============================================================

func uids(tickets []*model.Ticket) []uint64 {
	ids := make([]uint64, len(tickets))
	for i, t := range tickets {
		ids[i] = t.UID
	}
	return ids
}
