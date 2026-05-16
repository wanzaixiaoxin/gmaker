package bot

import (
	"context"
	"fmt"
	"math/rand"
	"strconv"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/services/bot-go/internal/dbproxy"

	dbproxypb "github.com/gmaker/luffa/gen/go/dbproxy"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

const (
	cmdMySQLQuery = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY)
	cmdMySQLExec  = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC)
)

// Config 机器人行为配置
type Config struct {
	RoomID             uint64   `json:"room_id"`
	MessageIntervalSec int      `json:"message_interval_sec"`
	Messages           []string `json:"messages"`
	AutoStart          bool     `json:"auto_start"`
	UseWS              bool     `json:"use_ws"`

	// 批量上线参数
	BatchSize        int `json:"batch_size"`          // 每批上线的 bot 数量（默认 10）
	BatchIntervalMs  int `json:"batch_interval_ms"`   // 两批之间的间隔毫秒数（默认 500）
	ConnectTimeoutMs int `json:"connect_timeout_ms"`  // 单个 bot 连接超时毫秒数（默认 5000）
	RetryDelayMs     int `json:"retry_delay_ms"`      // 连接失败后重试间隔毫秒数（默认 3000）
	MaxRetries       int `json:"max_retries"`         // 单个 bot 最大重试次数（默认 3，0=无限）
}

// BotAccount 机器人账号记录
type BotAccount struct {
	BotID    int
	PlayerID uint64
	BotType  string
	Status   int
}

// Manager 管理多个机器人客户端
type Manager struct {
	mu        sync.RWMutex
	clients   map[int]*Client
	config    Config
	gateway   string
	masterKey string
	db        *dbproxy.Client
	idGen     *idgen.Snowflake
	log       *logger.Logger

	// 批量启动控制
	cancel context.CancelFunc // 用于停止所有 bot

	// 统计
	running   atomic.Int32
	msgCount  atomic.Uint64
	failCount atomic.Uint64
}

// NewManager 创建管理器
func NewManager(cfg Config, gatewayAddr, masterKey string, db *dbproxy.Client, idGen *idgen.Snowflake, log *logger.Logger) *Manager {
	// 填充默认值
	if cfg.BatchSize <= 0 {
		cfg.BatchSize = 10
	}
	if cfg.BatchIntervalMs <= 0 {
		cfg.BatchIntervalMs = 500
	}
	if cfg.ConnectTimeoutMs <= 0 {
		cfg.ConnectTimeoutMs = 5000
	}
	if cfg.RetryDelayMs <= 0 {
		cfg.RetryDelayMs = 3000
	}
	return &Manager{
		clients:   make(map[int]*Client),
		config:    cfg,
		gateway:   gatewayAddr,
		masterKey: masterKey,
		db:        db,
		idGen:     idGen,
		log:       log,
	}
}

// LoadBotsFromDB 从 bot_accounts 表加载启用的机器人
func (m *Manager) LoadBotsFromDB() ([]BotAccount, error) {
	if m.db == nil {
		return nil, fmt.Errorf("dbproxy not available")
	}
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	sqlStr := "SELECT bot_id, player_id, bot_type, status FROM bot_accounts WHERE status = 0"
	req := &dbproxypb.MySQLQueryReq{Uid: 1, Sql: sqlStr}
	data, _ := proto.Marshal(req)
	resPkt, err := m.db.Call(ctx, cmdMySQLQuery, data)
	if err != nil {
		return nil, fmt.Errorf("query bot_accounts failed: %w", err)
	}
	var res dbproxypb.MySQLQueryRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		return nil, err
	}
	if !res.Ok {
		return nil, fmt.Errorf("db query failed: %s", res.Error)
	}

	var bots []BotAccount
	for _, row := range res.Rows {
		m := make(map[string]string)
		for _, col := range row.Columns {
			m[col.Name] = col.Value
		}
		botID, _ := strconv.Atoi(m["bot_id"])
		playerID, _ := strconv.ParseUint(m["player_id"], 10, 64)
		status, _ := strconv.Atoi(m["status"])
		bots = append(bots, BotAccount{
			BotID:    botID,
			PlayerID: playerID,
			BotType:  m["bot_type"],
			Status:   status,
		})
	}
	return bots, nil
}

// Start 启动所有从 DB 加载的机器人（分批错峰）
func (m *Manager) Start() {
	// 如果已有任务在运行，先停止
	m.Stop()

	ctx, cancel := context.WithCancel(context.Background())
	m.cancel = cancel

	bots, err := m.LoadBotsFromDB()
	if err != nil {
		m.log.Errorf("LoadBotsFromDB failed: %v, fallback to config generation", err)
		// fallback：用 Snowflake 生成指定数量的 bot
		if m.idGen != nil {
			var fallback []BotAccount
			for i := 0; i < 10; i++ {
				fallback = append(fallback, BotAccount{BotID: i, PlayerID: 0, BotType: "chatbot", Status: 0})
			}
			bots = fallback
		}
	}

	m.log.Infof("BotManager loaded %d bots, starting in batches (batch_size=%d, interval=%dms)",
		len(bots), m.config.BatchSize, m.config.BatchIntervalMs)

	go m.startBatches(ctx, bots)
}

// startBatches 分批错峰启动 bot
// 每批 BatchSize 个 bot 同时连接，两批之间等待 BatchIntervalMs
func (m *Manager) startBatches(ctx context.Context, bots []BotAccount) {
	total := len(bots)
	batchSize := m.config.BatchSize
	batchInterval := time.Duration(m.config.BatchIntervalMs) * time.Millisecond

	for i := 0; i < total; i += batchSize {
		// 检查是否被取消
		select {
		case <-ctx.Done():
			m.log.Infof("BotManager startBatches cancelled")
			return
		default:
		}

		// 计算本批范围
		end := i + batchSize
		if end > total {
			end = total
		}
		batch := bots[i:end]
		batchNum := (i / batchSize) + 1
		totalBatches := (total + batchSize - 1) / batchSize

		m.log.Infof("[Batch %d/%d] starting %d bots...", batchNum, totalBatches, len(batch))

		// 本批内每个 bot 错开一小段时间启动（每个 bot 之间间隔 50ms）
		for j, b := range batch {
			select {
			case <-ctx.Done():
				return
			default:
			}
			go m.runBotWithID(b.BotID, b.PlayerID)
			// 批内错峰：每个 bot 间隔 50ms，避免同一瞬间全部并发连接
			if j < len(batch)-1 {
				time.Sleep(50 * time.Millisecond)
			}
		}

		// 不是最后一批，等待间隔后再启动下一批
		if end < total {
			m.log.Infof("[Batch %d/%d] waiting %dms before next batch...", batchNum, totalBatches, m.config.BatchIntervalMs)
			select {
			case <-ctx.Done():
				return
			case <-time.After(batchInterval):
			}
		}
	}

	m.log.Infof("BotManager all %d bots launched", total)
}

// Stop 停止所有机器人
func (m *Manager) Stop() {
	// 取消批量启动任务
	if m.cancel != nil {
		m.cancel()
		m.cancel = nil
	}

	m.log.Info("BotManager stopping all bots...")
	m.mu.RLock()
	list := make([]*Client, 0, len(m.clients))
	for _, c := range m.clients {
		list = append(list, c)
	}
	m.mu.RUnlock()

	for _, c := range list {
		c.Close()
	}
	m.running.Store(0)
}

// SendRandomMessage 让指定 bot 发送一条随机消息
func (m *Manager) SendRandomMessage(botID int) error {
	m.mu.RLock()
	c, ok := m.clients[botID]
	m.mu.RUnlock()
	if !ok || !c.IsConnected() {
		return fmt.Errorf("bot-%d not found or not connected", botID)
	}
	content := m.randomMessage()
	if err := c.SendMsg(m.config.RoomID, content); err != nil {
		m.failCount.Add(1)
		return err
	}
	m.msgCount.Add(1)
	return nil
}

// SendMessageToAll 让所有在线 bot 各发送一条消息
func (m *Manager) SendMessageToAll(content string) {
	m.mu.RLock()
	list := make([]*Client, 0, len(m.clients))
	for _, c := range m.clients {
		list = append(list, c)
	}
	m.mu.RUnlock()

	for _, c := range list {
		if !c.IsConnected() {
			continue
		}
		msg := content
		if msg == "" {
			msg = m.randomMessage()
		}
		if err := c.SendMsg(m.config.RoomID, msg); err != nil {
			m.failCount.Add(1)
			m.log.Warnf("Bot-%d send msg failed: %v", c.ID, err)
		} else {
			m.msgCount.Add(1)
		}
	}
}

// Stats 返回运行统计
func (m *Manager) Stats() map[string]interface{} {
	m.mu.RLock()
	total := len(m.clients)
	m.mu.RUnlock()
	return map[string]interface{}{
		"total_bots":           total,
		"connected":            m.running.Load(),
		"msg_sent":             m.msgCount.Load(),
		"msg_failed":           m.failCount.Load(),
		"room_id":              m.config.RoomID,
		"message_interval_sec": m.config.MessageIntervalSec,
		"batch_size":           m.config.BatchSize,
		"batch_interval_ms":    m.config.BatchIntervalMs,
	}
}

// GetConfig 返回当前配置（只读副本）
func (m *Manager) GetConfig() Config {
	return m.config
}

// OverrideBatchSize 运行时覆盖每批启动数量
func (m *Manager) OverrideBatchSize(n int) {
	m.config.BatchSize = n
}

// OverrideBatchInterval 运行时覆盖批间间隔（毫秒）
func (m *Manager) OverrideBatchInterval(ms int) {
	m.config.BatchIntervalMs = ms
}

// runBotWithID 单个 bot 的生命周期 goroutine
// 带重试限制：连接失败最多重试 MaxRetries 次，成功后断线重连不受限制
func (m *Manager) runBotWithID(id int, playerID uint64) {
	if playerID == 0 && m.idGen != nil {
		rawID, err := m.idGen.NextID()
		if err != nil {
			m.log.Errorf("Bot-%d generate snowflake id failed: %v", id, err)
			return
		}
		playerID = uint64(rawID)
	}
	nickname := fmt.Sprintf("Bot%d", id)
	client := NewClient(id, playerID, nickname, m.masterKey, m.gateway, m.config.UseWS, m.log)

	m.mu.Lock()
	m.clients[id] = client
	m.mu.Unlock()

	retryDelay := time.Duration(m.config.RetryDelayMs) * time.Millisecond
	if retryDelay <= 0 {
		retryDelay = 3 * time.Second
	}
	maxRetries := m.config.MaxRetries // 0 = 无限重试
	consecutiveFails := 0

	for {
		if err := m.connectAndBind(client); err != nil {
			consecutiveFails++
			if maxRetries > 0 && consecutiveFails > maxRetries {
				m.log.Errorf("Bot-%d exceeded max retries (%d), giving up", id, maxRetries)
				return
			}
			m.log.Warnf("Bot-%d setup failed (attempt %d): %v, retry in %dms...", id, consecutiveFails, err, m.config.RetryDelayMs)
			time.Sleep(retryDelay)
			continue
		}

		// 连接成功，重置失败计数
		consecutiveFails = 0
		m.running.Add(1)
		m.log.Infof("Bot-%d (player_id=%d) is now active in room %d", id, playerID, m.config.RoomID)

		m.messageLoop(client)

		m.running.Add(-1)
		client.Close()
		m.log.Warnf("Bot-%d disconnected, reconnect in %dms...", id, m.config.RetryDelayMs)
		time.Sleep(retryDelay)
	}
}

func (m *Manager) connectAndBind(c *Client) error {
	if err := c.Connect(); err != nil {
		return err
	}
	if err := c.PlayerBind(); err != nil {
		c.Close()
		return err
	}
	if err := c.JoinRoom(m.config.RoomID); err != nil {
		c.Close()
		return err
	}
	return nil
}

func (m *Manager) messageLoop(c *Client) {
	interval := time.Duration(m.config.MessageIntervalSec) * time.Second
	if interval <= 0 {
		interval = 5 * time.Second
	}

	// 首次发送前随机延迟 0~interval，避免所有 bot 同时开口
	if interval > 0 {
		jitter := time.Duration(rand.Int63n(int64(interval)))
		time.Sleep(jitter)
	}

	for {
		if !c.IsConnected() {
			return
		}

		content := m.randomMessage()
		if err := c.SendMsg(m.config.RoomID, content); err != nil {
			m.failCount.Add(1)
			m.log.Warnf("Bot-%d send msg failed: %v", c.ID, err)
			return
		}
		m.msgCount.Add(1)

		// 每次发送后随机睡眠：基础间隔的 50%~150%，让节奏更自然
		minSleep := interval / 2
		maxSleep := interval * 3 / 2
		sleepTime := minSleep + time.Duration(rand.Int63n(int64(maxSleep-minSleep)))
		time.Sleep(sleepTime)
	}
}

func (m *Manager) randomMessage() string {
	if len(m.config.Messages) == 0 {
		return "hello"
	}
	return m.config.Messages[rand.Intn(len(m.config.Messages))]
}
