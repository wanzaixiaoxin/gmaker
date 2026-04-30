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

	// 统计
	running   atomic.Int32
	msgCount  atomic.Uint64
	failCount atomic.Uint64
}

// NewManager 创建管理器
func NewManager(cfg Config, gatewayAddr, masterKey string, db *dbproxy.Client, idGen *idgen.Snowflake, log *logger.Logger) *Manager {
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

// Start 启动所有从 DB 加载的机器人
func (m *Manager) Start() {
	bots, err := m.LoadBotsFromDB()
	if err != nil {
		m.log.Errorf("LoadBotsFromDB failed: %v, fallback to config generation", err)
		// fallback：用 Snowflake 生成指定数量的 bot
		if m.idGen != nil {
			for i := 0; i < 10; i++ {
				go m.runBotWithID(i, 0)
			}
		}
		return
	}
	m.log.Infof("BotManager loaded %d bots from DB", len(bots))
	for _, b := range bots {
		go m.runBotWithID(b.BotID, b.PlayerID)
	}
}

// Stop 停止所有机器人
func (m *Manager) Stop() {
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
	}
}

// runBotWithID 单个 bot 的生命周期 goroutine
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

	for {
		if err := m.connectAndBind(client); err != nil {
			m.log.Warnf("Bot-%d setup failed: %v, retry in 5s...", id, err)
			time.Sleep(5 * time.Second)
			continue
		}

		m.running.Add(1)
		m.log.Infof("Bot-%d (player_id=%d) is now active in room %d", id, playerID, m.config.RoomID)

		m.messageLoop(client)

		m.running.Add(-1)
		client.Close()
		m.log.Warnf("Bot-%d disconnected, reconnect in 3s...", id)
		time.Sleep(3 * time.Second)
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
	ticker := time.NewTicker(time.Duration(m.config.MessageIntervalSec) * time.Second)
	defer ticker.Stop()

	for {
		if !c.IsConnected() {
			return
		}
		select {
		case <-ticker.C:
			content := m.randomMessage()
			if err := c.SendMsg(m.config.RoomID, content); err != nil {
				m.failCount.Add(1)
				m.log.Warnf("Bot-%d send msg failed: %v", c.ID, err)
				return
			}
			m.msgCount.Add(1)
		}
	}
}

func (m *Manager) randomMessage() string {
	if len(m.config.Messages) == 0 {
		return "hello"
	}
	return m.config.Messages[rand.Intn(len(m.config.Messages))]
}
