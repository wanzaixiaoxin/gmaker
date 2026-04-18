package main

import (
	"fmt"
	"math/rand"
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

	// 登录
	start := time.Now()
	if err := bot.Login(account, password); err != nil {
		stats.Record(false, time.Since(start))
		return fmt.Errorf("bot %d login failed: %w", bot.id, err)
	}
	stats.Record(true, time.Since(start))

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
