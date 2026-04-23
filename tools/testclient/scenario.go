package main

import (
	"fmt"
	"math/rand"
	"sync"
	"time"
)

// Scenario 定义一个测试场景
type Scenario interface {
	Name() string
	// Run 执行场景，ctx 用于控制超时/取消，stats 用于记录指标
	Run(bot *Bot, stats *GlobalStats, stopCh <-chan struct{}) error
}

// LoginScenario 登录->获取玩家->更新->登出 流程
type LoginScenario struct{}

func (s *LoginScenario) Name() string { return "login" }

func (s *LoginScenario) Run(bot *Bot, stats *GlobalStats, stopCh <-chan struct{}) error {
	account := fmt.Sprintf("bot_%d_%d", bot.id, time.Now().UnixNano())
	password := "test1234"

	// 登录
	start := time.Now()
	err := bot.Login(account, password)
	stats.Record(err == nil, time.Since(start))
	if err != nil {
		return fmt.Errorf("bot %d login failed: %w", bot.id, err)
	}

	// 获取玩家
	start = time.Now()
	player, err := bot.GetPlayer()
	stats.Record(err == nil, time.Since(start))
	if err != nil {
		return fmt.Errorf("bot %d get player failed: %w", bot.id, err)
	}

	// 更新玩家
	start = time.Now()
	err = bot.UpdatePlayer(
		fmt.Sprintf("bot_%d", bot.id),
		player.Coin+1,
		player.Diamond+1,
	)
	stats.Record(err == nil, time.Since(start))
	if err != nil {
		return fmt.Errorf("bot %d update player failed: %w", bot.id, err)
	}

	// 再次获取验证
	start = time.Now()
	_, err = bot.GetPlayer()
	stats.Record(err == nil, time.Since(start))
	if err != nil {
		return fmt.Errorf("bot %d verify player failed: %w", bot.id, err)
	}

	return nil
}

// HeartbeatScenario 登录后持续发送 Ping
type HeartbeatScenario struct {
	Interval time.Duration
}

func (s *HeartbeatScenario) Name() string { return "heartbeat" }

func (s *HeartbeatScenario) Run(bot *Bot, stats *GlobalStats, stopCh <-chan struct{}) error {
	account := fmt.Sprintf("bot_%d_%d", bot.id, time.Now().UnixNano())
	password := "test1234"

	// 登录（失败时不退出，继续发送心跳）
	start := time.Now()
	if err := bot.Login(account, password); err != nil {
		stats.Record(false, time.Since(start))
		fmt.Printf("Bot %d login failed (continuing with heartbeat): %v\n", bot.id, err)
	} else {
		stats.Record(true, time.Since(start))
	}

	ticker := time.NewTicker(s.Interval)
	defer ticker.Stop()

	for {
		select {
		case <-stopCh:
			return nil
		case <-ticker.C:
			start = time.Now()
			_, err := bot.Ping()
			stats.Record(err == nil, time.Since(start))
		}
	}
}

// FloodScenario 全速压测场景：不断随机发送 GetPlayer/UpdatePlayer
type FloodScenario struct {
	Rate int // 每秒请求数，0=不限速
}

func (s *FloodScenario) Name() string { return "flood" }

func (s *FloodScenario) Run(bot *Bot, stats *GlobalStats, stopCh <-chan struct{}) error {
	account := fmt.Sprintf("bot_%d_%d", bot.id, time.Now().UnixNano())
	password := "test1234"

	// 登录
	start := time.Now()
	if err := bot.Login(account, password); err != nil {
		stats.Record(false, time.Since(start))
		return fmt.Errorf("bot %d login failed: %w", bot.id, err)
	}
	stats.Record(true, time.Since(start))

	var ticker *time.Ticker
	if s.Rate > 0 {
		ticker = time.NewTicker(time.Second / time.Duration(s.Rate))
		defer ticker.Stop()
	}

	for {
		// 限速等待
		if ticker != nil {
			select {
			case <-stopCh:
				return nil
			case <-ticker.C:
			}
		} else {
			select {
			case <-stopCh:
				return nil
			default:
			}
		}

		start = time.Now()
		var err error
		if rand.Intn(2) == 0 {
			_, err = bot.GetPlayer()
		} else {
			err = bot.UpdatePlayer(fmt.Sprintf("bot_%d", bot.id), 0, 0)
		}
		stats.Record(err == nil, time.Since(start))
	}
}

// ChatScenario 聊天室测试场景
type ChatScenario struct {
	roomID     uint64
	roomIDMu   sync.RWMutex
	roomIDOnce sync.Once
}

func (s *ChatScenario) Name() string { return "chat" }

func (s *ChatScenario) setRoomID(id uint64) {
	s.roomIDMu.Lock()
	s.roomID = id
	s.roomIDMu.Unlock()
}

func (s *ChatScenario) getRoomID() uint64 {
	s.roomIDMu.RLock()
	defer s.roomIDMu.RUnlock()
	return s.roomID
}

func (s *ChatScenario) Run(bot *Bot, stats *GlobalStats, stopCh <-chan struct{}) error {
	// 创建房间（只有 bot 0 创建）
	if bot.id == 0 {
		start := time.Now()
		room, err := bot.CreateRoom(fmt.Sprintf("test_room_%d", bot.id), uint64(bot.id))
		stats.Record(err == nil, time.Since(start))
		if err != nil {
			return fmt.Errorf("bot %d create room failed: %w", bot.id, err)
		}
		s.setRoomID(room.RoomId)
		fmt.Printf("[Chat] Bot %d created room %d\n", bot.id, room.RoomId)
	} else {
		// 其他 bot 等待房间创建
		for i := 0; i < 10; i++ {
			if s.getRoomID() != 0 {
				break
			}
			select {
			case <-stopCh:
				return nil
			case <-time.After(100 * time.Millisecond):
			}
		}
	}

	roomID := s.getRoomID()
	if roomID == 0 {
		return fmt.Errorf("bot %d: room not created", bot.id)
	}

	// 加入房间
	start := time.Now()
	joinRes, err := bot.JoinRoom(roomID, uint64(bot.id))
	stats.Record(err == nil, time.Since(start))
	if err != nil {
		return fmt.Errorf("bot %d join room failed: %w", bot.id, err)
	}
	fmt.Printf("[Chat] Bot %d joined room %d, got %d recent msgs\n", bot.id, roomID, len(joinRes.RecentMsgs))

	// 发送消息
	for i := 0; i < 3; i++ {
		select {
		case <-stopCh:
			return nil
		default:
		}
		start = time.Now()
		_, err = bot.SendMsg(roomID, uint64(bot.id), fmt.Sprintf("hello from bot %d msg %d", bot.id, i))
		stats.Record(err == nil, time.Since(start))
		if err != nil {
			fmt.Printf("[Chat] Bot %d send msg failed: %v\n", bot.id, err)
		}
		select {
		case <-stopCh:
			return nil
		case <-time.After(200 * time.Millisecond):
		}
	}

	// bot 0 关闭房间
	if bot.id == 0 {
		select {
		case <-stopCh:
			return nil
		case <-time.After(1 * time.Second):
		}
		start = time.Now()
		err = bot.CloseRoom(roomID, uint64(bot.id))
		stats.Record(err == nil, time.Since(start))
		if err != nil {
			fmt.Printf("[Chat] Bot %d close room failed: %v\n", bot.id, err)
		} else {
			fmt.Printf("[Chat] Bot %d closed room %d\n", bot.id, roomID)
		}
	}

	// 等待关闭信号
	<-stopCh
	return nil
}
