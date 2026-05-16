package service

import (
	"encoding/binary"
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/services/match-go/internal/engine"
	"github.com/gmaker/luffa/services/match-go/internal/model"
	"github.com/gmaker/luffa/services/match-go/internal/store"
)

// ============================================================
// MatchService 匹配服务总管
// ============================================================

type MatchService struct {
	mem     *store.MemoryStore
	redis   *store.RedisStore
	pool    *engine.BucketPool
	eng     *engine.Engine
	gate    *engine.AdmissionGate
	log     *logger.Logger

	// 匹配引擎 tick 控制
	running  atomic.Bool
	stopCh   chan struct{}
	wg       sync.WaitGroup

	// 结算层回调
	realtimePool *net.UpstreamPool // Realtime 上游连接池（用于创建房间）
	notifyCh     chan *model.Match // 结算通知 channel

	// etcd 选举
	isLeader atomic.Bool
}

func NewMatchService(
	mem *store.MemoryStore,
	redis *store.RedisStore,
	log *logger.Logger,
) *MatchService {
	pool := engine.NewBucketPool()
	eng := engine.NewEngine(pool, log)
	gate := engine.NewAdmissionGate(model.MaxQueueSize)

	return &MatchService{
		mem:     mem,
		redis:   redis,
		pool:    pool,
		eng:     eng,
		gate:    gate,
		log:     log,
		stopCh:  make(chan struct{}),
		notifyCh: make(chan *model.Match, 256),
	}
}

// Log 返回日志实例（供 handler 使用）
func (s *MatchService) Log() *logger.Logger {
	return s.log
}

// SetRealtimePool 设置 Realtime 连接池
func (s *MatchService) SetRealtimePool(pool *net.UpstreamPool) {
	s.realtimePool = pool
}

// SetLeader 设置是否主节点（etcd 选举结果）
func (s *MatchService) SetLeader(isLeader bool) {
	if isLeader && !s.isLeader.Load() {
		s.isLeader.Store(true)
		s.log.Info("[MatchService] promoted to leader, starting engine")
		s.startEngine()
	} else if !isLeader && s.isLeader.Load() {
		s.isLeader.Store(false)
		s.log.Info("[MatchService] demoted from leader, stopping engine")
		s.stopEngine()
	}
}

func (s *MatchService) IsLeader() bool {
	return s.isLeader.Load()
}

// ============================================================
// 公共 API
// ============================================================

// Enqueue 入队（准入门 + 入桶）
func (s *MatchService) Enqueue(t *model.Ticket) error {
	// 准入检查
	queueLen := s.mem.CountQueuing()
	if err := s.gate.Accept(queueLen); err != nil {
		return err
	}

	// 检查是否已在队列中
	if existing, ok := s.mem.GetTicket(t.UID); ok {
		if existing.State == model.TicketQueuing || existing.State == model.TicketMatching {
			return engine.ErrAlreadyInQueue
		}
	}

	// 初始化 Ticket
	t.State = model.TicketQueuing
	t.EnqueueAt = time.Now()
	t.Priority = 0

	// 写入内存
	s.mem.PutTicket(t)

	// 入桶
	s.pool.Enqueue(t)

	s.log.Infof("[MatchService] enqueue: uid=%d mmr=%d mode=%d queue_len=%d",
		t.UID, t.MMR, t.Mode, queueLen+1)
	return nil
}

// Dequeue 取消匹配（出队）
func (s *MatchService) Dequeue(uid uint64) error {
	t, ok := s.mem.GetTicket(uid)
	if !ok {
		return engine.ErrNotInQueue
	}
	if t.State != model.TicketQueuing {
		return engine.ErrNotInQueue
	}

	// 状态转移
	if err := s.mem.UpdateTicketState(uid, model.TicketIdle); err != nil {
		return err
	}

	// 出桶
	s.pool.Dequeue(t)

	// 清理内存
	s.mem.DeleteTicket(uid)

	s.log.Infof("[MatchService] dequeue: uid=%d", uid)
	return nil
}

// GetStatus 查询匹配状态
func (s *MatchService) GetStatus(uid uint64) (state string, waitMs int32, queueLen int) {
	t, ok := s.mem.GetTicket(uid)
	if !ok {
		return "idle", 0, s.mem.CountQueuing()
	}
	return t.State.String(), t.WaitMs(), s.mem.CountQueuing()
}

// ============================================================
// 匹配引擎 tick 循环
// ============================================================

func (s *MatchService) startEngine() {
	if s.running.Load() {
		return
	}
	s.running.Store(true)

	s.wg.Add(2)
	go s.tickLoop()
	go s.settlementLoop()
}

func (s *MatchService) stopEngine() {
	s.running.Store(false)
	close(s.stopCh)
	s.wg.Wait()
	s.stopCh = make(chan struct{})
}

func (s *MatchService) tickLoop() {
	defer s.wg.Done()
	ticker := time.NewTicker(time.Duration(model.TickIntervalMs) * time.Millisecond)
	defer ticker.Stop()

	for {
		select {
		case <-s.stopCh:
			return
		case <-ticker.C:
			if !s.isLeader.Load() {
				continue
			}
			candidates := s.eng.Tick()
			for _, c := range candidates {
				s.onMatchCandidate(c)
			}
		}
	}
}

// ============================================================
// 结算循环
// ============================================================

func (s *MatchService) settlementLoop() {
	defer s.wg.Done()
	for {
		select {
		case <-s.stopCh:
			return
		case match := <-s.notifyCh:
			s.driveForward(match)
		}
	}
}

// onMatchCandidate 处理匹配候选
func (s *MatchService) onMatchCandidate(c *engine.MatchCandidate) {
	// 生成 match_id
	matchID, err := s.redis.NextMatchID()
	if err != nil {
		s.log.Errorf("[StateMachine] generate match_id failed: %v", err)
		s.releaseCandidate(c)
		return
	}

	allUIDs := make([]uint64, 0, len(c.TeamA)+len(c.TeamB))
	for _, t := range c.TeamA {
		allUIDs = append(allUIDs, t.UID)
	}
	for _, t := range c.TeamB {
		allUIDs = append(allUIDs, t.UID)
	}

	match := &model.Match{
		ID:        matchID,
		State:     model.MatchDraft,
		Mode:      c.TeamA[0].Mode,
		TeamSize:  c.TeamA[0].TeamSize,
		TeamA:     uidsFromTickets(c.TeamA),
		TeamB:     uidsFromTickets(c.TeamB),
		CreatedAt: time.Now(),
		UpdatedAt: time.Now(),
	}

	// 内存：Ticket → MATCHING
	for _, t := range c.TeamA {
		s.mem.UpdateTicketState(t.UID, model.TicketMatching)
		s.pool.Dequeue(t)
	}
	for _, t := range c.TeamB {
		s.mem.UpdateTicketState(t.UID, model.TicketMatching)
		s.pool.Dequeue(t)
	}

	// CAS 确认（Redis 原子操作）
	ok, err := s.redis.CASConfirm(matchID, allUIDs)
	if err != nil || !ok {
		s.log.Warnf("[StateMachine] CAS confirm failed: match=%d ok=%v err=%v", matchID, ok, err)
		// 放回队列
		s.releaseCandidate(c)
		return
	}

	// 状态转移：DRAFT → CONFIRMED
	match.State = model.MatchConfirmed
	s.mem.PutMatch(match)
	s.mem.BindMatch(allUIDs, matchID)

	// 持久化到 Redis
	if err := s.redis.SaveMatchRecord(match); err != nil {
		s.log.Errorf("[StateMachine] persist match failed: %v", err)
		// 已确认，继续推进（降级运行）
	}

	// 推入结算 channel
	s.notifyCh <- match
	s.log.Infof("[StateMachine] match confirmed: id=%d teamA=%v teamB=%v",
		matchID, match.TeamA, match.TeamB)
}

// driveForward 从当前状态推进到终态
func (s *MatchService) driveForward(m *model.Match) {
	for !m.State.IsTerminal() {
		switch m.State {
		case model.MatchConfirmed:
			// → 创建房间
			s.mem.UpdateMatchState(m.ID, model.MatchRoomPrep)
			m.State = model.MatchRoomPrep
			s.redis.SaveMatchRecord(m)

			roomID, err := s.createRoom(m)
			if err != nil {
				s.log.Errorf("[Settlement] create room failed: %v", err)
				s.mem.UpdateMatchState(m.ID, model.MatchRetry)
				m.State = model.MatchRetry
				m.Retries++
				s.redis.SaveMatchRecord(m)
				continue
			}
			m.RoomID = roomID
			s.mem.UpdateMatchState(m.ID, model.MatchNotifying)
			m.State = model.MatchNotifying
			s.redis.SaveMatchRecord(m)

		case model.MatchRetry:
			if m.Retries >= model.MaxRetries {
				s.log.Warnf("[Settlement] max retries reached, rollback: match=%d", m.ID)
				s.mem.UpdateMatchState(m.ID, model.MatchRollback)
				m.State = model.MatchRollback
				s.redis.SaveMatchRecord(m)
				s.mem.ReleaseTickets(m.AllUIDs())
				// 重新入桶
				for _, uid := range m.AllUIDs() {
					if t, ok := s.mem.GetTicket(uid); ok {
						s.pool.Enqueue(t)
					}
				}
				s.redis.DeleteMatchRecord(m.ID)
				return
			}
			s.mem.UpdateMatchState(m.ID, model.MatchRoomPrep)
			m.State = model.MatchRoomPrep
			s.redis.SaveMatchRecord(m)

		case model.MatchRoomPrep:
			roomID, err := s.createRoom(m)
			if err != nil {
				s.log.Errorf("[Settlement] create room failed (retry %d): %v", m.Retries, err)
				s.mem.UpdateMatchState(m.ID, model.MatchRetry)
				m.State = model.MatchRetry
				m.Retries++
				s.redis.SaveMatchRecord(m)
				continue
			}
			m.RoomID = roomID
			s.mem.UpdateMatchState(m.ID, model.MatchNotifying)
			m.State = model.MatchNotifying
			s.redis.SaveMatchRecord(m)

		case model.MatchNotifying:
			// 异步归档
			go s.redis.ArchiveMatch(m)
			// 标记完成
			s.mem.UpdateMatchState(m.ID, model.MatchComplete)
			m.State = model.MatchComplete
			s.redis.DeleteMatchRecord(m.ID)
			// 更新 Ticket 状态
			for _, uid := range m.AllUIDs() {
				s.mem.UpdateTicketState(uid, model.TicketSettling)
			}
			s.log.Infof("[Settlement] match complete: id=%d room=%d", m.ID, m.RoomID)
		}
	}
}

// createRoom 请求 Realtime 创建房间
func (s *MatchService) createRoom(m *model.Match) (uint64, error) {
	if s.realtimePool == nil {
		// Realtime 不可用时用时间戳生成假 roomID（降级）
		return uint64(time.Now().UnixNano()), nil
	}

	// 发送 RoomEnterReq 到 Realtime（room_id=0 表示动态创建）
	// 协议格式：8字节 gateway_conn_id + protobuf payload
	// 这里简化处理，直接生成 roomID
	// TODO: 实现完整的 Realtime RPC 通信
	return uint64(m.ID), nil
}

// releaseCandidate 释放候选回队列
func (s *MatchService) releaseCandidate(c *engine.MatchCandidate) {
	for _, t := range c.TeamA {
		s.mem.UpdateTicketState(t.UID, model.TicketQueuing)
		t.State = model.TicketQueuing
		t.Priority++
		s.pool.Enqueue(t)
	}
	for _, t := range c.TeamB {
		s.mem.UpdateTicketState(t.UID, model.TicketQueuing)
		t.State = model.TicketQueuing
		t.Priority++
		s.pool.Enqueue(t)
	}
}

// ============================================================
// 崩溃恢复
// ============================================================

// Recover 从 Redis 恢复未完成的对局
func (s *MatchService) Recover() error {
	matches, err := s.redis.LoadActiveMatches()
	if err != nil {
		return fmt.Errorf("load active matches: %w", err)
	}
	if len(matches) == 0 {
		s.log.Info("[Recovery] no active matches to recover")
		return nil
	}

	s.log.Infof("[Recovery] found %d active matches to recover", len(matches))

	for _, m := range matches {
		age := time.Since(m.CreatedAt)
		if age > time.Duration(model.RedisKeyTTL)*time.Second {
			// 过老的记录，直接丢弃
			s.log.Warnf("[Recovery] discarding stale match: id=%d age=%v", m.ID, age)
			s.redis.DeleteMatchRecord(m.ID)
			continue
		}

		// 重建内存状态
		s.mem.PutMatch(m)

		// 推入结算 channel 继续推进
		s.notifyCh <- m
		s.log.Infof("[Recovery] recovering match: id=%d state=%s", m.ID, m.State)
	}

	return nil
}

// ============================================================
// 发送匹配结果给客户端（通过 Gateway 回包）
// ============================================================

// BuildMatchResponse 构建匹配成功的 payload（给 handler 用）
func (s *MatchService) BuildMatchResponse(match *model.Match) ([]byte, error) {
	// payload 格式：8字节 gateway_conn_id + protobuf
	// gateway_conn_id 由 handler 层填充
	_ = match
	return nil, nil
}

// ============================================================
// 辅助函数
// ============================================================

func uidsFromTickets(tickets []*model.Ticket) []uint64 {
	ids := make([]uint64, len(tickets))
	for i, t := range tickets {
		ids[i] = t.UID
	}
	return ids
}

// unused import guard
var _ = binary.BigEndian.Uint64
