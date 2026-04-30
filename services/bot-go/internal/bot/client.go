package bot

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/gen/go/chat"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

const (
	cmdPlayerBind        = uint32(protocol.CmdGatewayInternal_CMD_GW_PLAYER_BIND)
	cmdChatJoinRoomReq   = uint32(protocol.CmdChat_CMD_CHAT_JOIN_ROOM_REQ)
	cmdChatJoinRoomRes   = uint32(protocol.CmdChat_CMD_CHAT_JOIN_ROOM_RES)
	cmdChatSendMsgReq    = uint32(protocol.CmdChat_CMD_CHAT_SEND_MSG_REQ)
	cmdChatSendMsgRes    = uint32(protocol.CmdChat_CMD_CHAT_SEND_MSG_RES)
	cmdChatMsgNotify     = uint32(protocol.CmdChat_CMD_CHAT_MSG_NOTIFY)
	cmdSysHandshake      = uint32(protocol.CmdSystem_CMD_SYS_HANDSHAKE)
)

// Client 表示一个机器人客户端
type Client struct {
	ID       int
	PlayerID uint64
	Nickname string
	Token    string // 使用 bot_master_key

	gatewayAddr string
	useWS       bool
	tcpClient   *net.TCPClient
	wsClient    *WSClient

	mu      sync.Mutex
	seqID   uint32
	pending map[uint32]chan *net.Packet
	running bool

	Log *logger.Logger
}

// NewClient 创建机器人客户端
func NewClient(id int, playerID uint64, nickname, token, gatewayAddr string, useWS bool, log *logger.Logger) *Client {
	return &Client{
		ID:          id,
		PlayerID:    playerID,
		Nickname:    nickname,
		Token:       token,
		gatewayAddr: gatewayAddr,
		useWS:       useWS,
		pending:     make(map[uint32]chan *net.Packet),
		Log:         log,
	}
}

// Connect 连接到 Gateway（支持 TCP 和 WebSocket）
func (c *Client) Connect() error {
	if c.useWS {
		c.wsClient = NewWSClient(c.gatewayAddr,
			func(_ *WSConn, pkt *net.Packet) { c.onPacket(pkt) },
			func(_ *WSConn) { c.Log.Infof("Bot-%d disconnected", c.ID) },
		)
		// Web 客户端总是握手（即使 master_key 为空也使用全 0 数组）
		c.wsClient.SetMasterKey(make([]byte, 32))
		if err := c.wsClient.Connect(); err != nil {
			return fmt.Errorf("bot-%d ws connect failed: %w", c.ID, err)
		}
		c.Log.Infof("Bot-%d websocket connected to %s", c.ID, c.gatewayAddr)
		return nil
	}
	c.tcpClient = net.NewTCPClient(c.gatewayAddr,
		func(_ *net.TCPConn, pkt *net.Packet) { c.onPacket(pkt) },
		func(_ *net.TCPConn) { c.Log.Infof("Bot-%d disconnected", c.ID) },
	)
	if err := c.tcpClient.Connect(); err != nil {
		return fmt.Errorf("bot-%d tcp connect failed: %w", c.ID, err)
	}
	c.Log.Infof("Bot-%d tcp connected to %s", c.ID, c.gatewayAddr)
	return nil
}

// Close 断开连接
func (c *Client) Close() {
	if c.wsClient != nil {
		c.wsClient.Close()
		c.wsClient = nil
	}
	if c.tcpClient != nil {
		c.tcpClient.Close()
		c.tcpClient = nil
	}
}

// IsConnected 检查连接状态
func (c *Client) IsConnected() bool {
	if c.useWS && c.wsClient != nil {
		return c.wsClient.Conn() != nil
	}
	return c.tcpClient != nil && c.tcpClient.Conn() != nil
}

// onPacket 分发收到的包到等待的 Call
func (c *Client) onPacket(pkt *net.Packet) {
	if pkt.SeqID == 0 {
		return
	}
	c.mu.Lock()
	ch, ok := c.pending[pkt.SeqID]
	c.mu.Unlock()
	if ok {
		select {
		case ch <- pkt:
		default:
		}
	}
}

// sendPacket 发送包
func (c *Client) sendPacket(pkt *net.Packet) bool {
	if c.useWS && c.wsClient != nil && c.wsClient.Conn() != nil {
		return c.wsClient.Conn().SendPacket(pkt)
	}
	if c.tcpClient != nil && c.tcpClient.Conn() != nil {
		return c.tcpClient.Conn().SendPacket(pkt)
	}
	return false
}

// Call 同步 RPC 调用
func (c *Client) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	if !c.IsConnected() {
		return nil, fmt.Errorf("bot-%d not connected", c.ID)
	}

	c.mu.Lock()
	c.seqID++
	seq := c.seqID
	ch := make(chan *net.Packet, 1)
	c.pending[seq] = ch
	c.mu.Unlock()

	defer func() {
		c.mu.Lock()
		delete(c.pending, seq)
		c.mu.Unlock()
	}()

	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  cmdID,
			SeqID:  seq,
			Flags:  uint32(net.FlagRPCReq),
			Length: uint32(net.HeaderSize + len(payload)),
		},
		Payload: payload,
	}

	if !c.sendPacket(pkt) {
		return nil, fmt.Errorf("bot-%d send failed", c.ID)
	}

	select {
	case res := <-ch:
		return res, nil
	case <-ctx.Done():
		return nil, ctx.Err()
	}
}

// CallWithTimeout 带超时的同步调用
func (c *Client) CallWithTimeout(cmdID uint32, payload []byte, timeout time.Duration) (*net.Packet, error) {
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	return c.Call(ctx, cmdID, payload)
}

// PlayerBind 发送玩家绑定请求（使用 bot_master_key 作为 token）
func (c *Client) PlayerBind() error {
	req := &protocol.PlayerBindReq{
		PlayerId: c.PlayerID,
		Token:    c.Token,
	}
	data, err := proto.Marshal(req)
	if err != nil {
		return err
	}

	resPkt, err := c.CallWithTimeout(cmdPlayerBind, data, 5*time.Second)
	if err != nil {
		return fmt.Errorf("player_bind call failed: %w", err)
	}

	res := &protocol.PlayerBindRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return fmt.Errorf("unmarshal player_bind res: %w", err)
	}
	if res.Code != 0 {
		return fmt.Errorf("player_bind rejected: code=%d msg=%s", res.Code, res.Msg)
	}

	c.Log.Infof("Bot-%d player_bind success: player_id=%d", c.ID, c.PlayerID)
	return nil
}

// JoinRoom 加入聊天室
func (c *Client) JoinRoom(roomID uint64) error {
	req := &chat.ChatJoinRoomReq{RoomId: roomID, PlayerId: c.PlayerID}
	data, err := proto.Marshal(req)
	if err != nil {
		return err
	}

	resPkt, err := c.CallWithTimeout(cmdChatJoinRoomReq, data, 5*time.Second)
	if err != nil {
		return fmt.Errorf("join_room call failed: %w", err)
	}

	res := &chat.ChatJoinRoomRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return fmt.Errorf("unmarshal join_room res: %w", err)
	}
	if res.Result != nil && !res.Result.Ok {
		return fmt.Errorf("join_room rejected: code=%d msg=%s", res.Result.Code, res.Result.Msg)
	}

	c.Log.Infof("Bot-%d joined room %d", c.ID, roomID)
	return nil
}

// SendMsg 发送聊天消息
func (c *Client) SendMsg(roomID uint64, content string) error {
	req := &chat.ChatSendMsgReq{
		RoomId:     roomID,
		SenderId:   c.PlayerID,
		Content:    content,
		SenderName: c.Nickname,
	}
	data, err := proto.Marshal(req)
	if err != nil {
		return err
	}

	resPkt, err := c.CallWithTimeout(cmdChatSendMsgReq, data, 5*time.Second)
	if err != nil {
		return fmt.Errorf("send_msg call failed: %w", err)
	}

	res := &chat.ChatSendMsgRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return fmt.Errorf("unmarshal send_msg res: %w", err)
	}
	if res.Result != nil && !res.Result.Ok {
		return fmt.Errorf("send_msg rejected: code=%d msg=%s", res.Result.Code, res.Result.Msg)
	}

	return nil
}
