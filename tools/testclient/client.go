package main

import (
	"context"
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/gen/go/biz"
	"github.com/gmaker/luffa/gen/go/chat"
	"github.com/gmaker/luffa/gen/go/login"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

// CmdIDs
const (
	CmdLoginReq        = uint32(protocol.CmdCommon_CMD_CMN_LOGIN_REQ)
	CmdLoginRes        = uint32(protocol.CmdCommon_CMD_CMN_LOGIN_RES)
	CmdGetPlayerReq    = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_REQ)
	CmdGetPlayerRes    = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_RES)
	CmdUpdatePlayerReq = uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_REQ)
	CmdUpdatePlayerRes = uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_RES)
	CmdPing            = uint32(protocol.CmdBiz_CMD_BIZ_PING)
	CmdPong            = uint32(protocol.CmdBiz_CMD_BIZ_PONG)

	CmdChatCreateRoomReq = uint32(protocol.CmdChat_CMD_CHAT_CREATE_ROOM_REQ)
	CmdChatCreateRoomRes = uint32(protocol.CmdChat_CMD_CHAT_CREATE_ROOM_RES)
	CmdChatJoinRoomReq   = uint32(protocol.CmdChat_CMD_CHAT_JOIN_ROOM_REQ)
	CmdChatJoinRoomRes   = uint32(protocol.CmdChat_CMD_CHAT_JOIN_ROOM_RES)
	CmdChatLeaveRoomReq  = uint32(protocol.CmdChat_CMD_CHAT_LEAVE_ROOM_REQ)
	CmdChatLeaveRoomRes  = uint32(protocol.CmdChat_CMD_CHAT_LEAVE_ROOM_RES)
	CmdChatSendMsgReq    = uint32(protocol.CmdChat_CMD_CHAT_SEND_MSG_REQ)
	CmdChatSendMsgRes    = uint32(protocol.CmdChat_CMD_CHAT_SEND_MSG_RES)
	CmdChatMsgNotify     = uint32(protocol.CmdChat_CMD_CHAT_MSG_NOTIFY)
	CmdChatGetHistoryReq = uint32(protocol.CmdChat_CMD_CHAT_GET_HISTORY_REQ)
	CmdChatGetHistoryRes = uint32(protocol.CmdChat_CMD_CHAT_GET_HISTORY_RES)
	CmdChatCloseRoomReq  = uint32(protocol.CmdChat_CMD_CHAT_CLOSE_ROOM_REQ)
	CmdChatCloseRoomRes  = uint32(protocol.CmdChat_CMD_CHAT_CLOSE_ROOM_RES)
)

// Bot 表示一个模拟客户端
type Bot struct {
	id        int
	addr      string
	masterKey []byte
	useWS     bool

	// 传输层（两者互斥，由 useWS 决定）
	tcpClient *net.TCPClient
	wsClient  *WSClient

	mu      sync.Mutex
	seqID   uint32
	pending map[uint32]chan *net.Packet

	token    string
	playerID uint64

	// 统计
	success  atomic.Uint64
	fail     atomic.Uint64
	inflight atomic.Int32
}

// NewBot 创建一个新的测试机器人
func NewBot(id int, addr string, masterKey []byte, useWS bool) *Bot {
	return &Bot{
		id:        id,
		addr:      addr,
		masterKey: masterKey,
		useWS:     useWS,
		pending:   make(map[uint32]chan *net.Packet),
	}
}

// Connect 连接到 Gateway 并完成握手
func (b *Bot) Connect() error {
	if b.useWS {
		b.wsClient = NewWSClient(b.addr,
			func(_ *WSConn, pkt *net.Packet) { b.onPacket(pkt) },
			func(_ *WSConn) {},
		)
		if len(b.masterKey) > 0 {
			b.wsClient.SetMasterKey(b.masterKey)
		}
		return b.wsClient.Connect()
	}
	b.tcpClient = net.NewTCPClient(b.addr,
		func(_ *net.TCPConn, pkt *net.Packet) { b.onPacket(pkt) },
		func(_ *net.TCPConn) {},
	)
	if len(b.masterKey) > 0 {
		b.tcpClient.SetMasterKey(b.masterKey)
	}
	return b.tcpClient.Connect()
}

// Close 断开连接
func (b *Bot) Close() {
	if b.wsClient != nil {
		b.wsClient.Close()
		b.wsClient = nil
	}
	if b.tcpClient != nil {
		b.tcpClient.Close()
		b.tcpClient = nil
	}
}

// IsConnected 检查是否已连接
func (b *Bot) IsConnected() bool {
	if b.wsClient != nil {
		return b.wsClient.Conn() != nil
	}
	return b.tcpClient != nil && b.tcpClient.Conn() != nil
}

// sendPacket 根据当前传输层发送 Packet
func (b *Bot) sendPacket(pkt *net.Packet) bool {
	if b.wsClient != nil {
		return b.wsClient.Conn().SendPacket(pkt)
	}
	if b.tcpClient != nil {
		return b.tcpClient.Conn().SendPacket(pkt)
	}
	return false
}

// onPacket 分发收到的响应到等待的 Call
func (b *Bot) onPacket(pkt *net.Packet) {
	if pkt.SeqID == 0 {
		return
	}
	b.mu.Lock()
	ch, ok := b.pending[pkt.SeqID]
	b.mu.Unlock()
	if ok {
		select {
		case ch <- pkt:
		default:
		}
	}
}

// Call 同步发送请求并等待响应
func (b *Bot) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	if !b.IsConnected() {
		return nil, fmt.Errorf("not connected")
	}

	b.mu.Lock()
	b.seqID++
	seq := b.seqID
	ch := make(chan *net.Packet, 1)
	b.pending[seq] = ch
	b.mu.Unlock()

	b.inflight.Add(1)
	defer b.inflight.Add(-1)

	defer func() {
		b.mu.Lock()
		delete(b.pending, seq)
		b.mu.Unlock()
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

	if !b.sendPacket(pkt) {
		return nil, fmt.Errorf("send failed")
	}

	select {
	case res := <-ch:
		return res, nil
	case <-ctx.Done():
		return nil, ctx.Err()
	}
}

// CallWithTimeout Call 的快捷方式，带默认超时
func (b *Bot) CallWithTimeout(cmdID uint32, payload []byte, timeout time.Duration) (*net.Packet, error) {
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	return b.Call(ctx, cmdID, payload)
}

// Login 执行登录流程
func (b *Bot) Login(account, password string) error {
	req := &login.LoginReq{
		Account:  account,
		Password: password,
		Platform: "testclient",
		Version:  "1.0.0",
	}
	data, err := proto.Marshal(req)
	if err != nil {
		return fmt.Errorf("marshal login req: %w", err)
	}

	resPkt, err := b.CallWithTimeout(CmdLoginReq, data, 5*time.Second)
	if err != nil {
		return fmt.Errorf("login call: %w", err)
	}

	res := &login.LoginRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return fmt.Errorf("unmarshal login res: %w", err)
	}
	if res.Result != nil && res.Result.Code != 0 {
		return fmt.Errorf("login failed: code=%d msg=%s", res.Result.Code, res.Result.Msg)
	}

	b.token = res.Token
	b.playerID = res.PlayerId
	return nil
}

// GetPlayer 获取玩家数据
func (b *Bot) GetPlayer() (*biz.PlayerBase, error) {
	req := &biz.GetPlayerReq{PlayerId: b.playerID}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}

	resPkt, err := b.CallWithTimeout(CmdGetPlayerReq, data, 5*time.Second)
	if err != nil {
		return nil, err
	}

	res := &biz.GetPlayerRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return nil, err
	}
	if res.Result != nil && res.Result.Code != 0 {
		return nil, fmt.Errorf("get player failed: code=%d", res.Result.Code)
	}
	return res.Player, nil
}

// UpdatePlayer 更新玩家数据
func (b *Bot) UpdatePlayer(nickname string, coin, diamond uint64) error {
	req := &biz.UpdatePlayerReq{
		PlayerId: b.playerID,
		Nickname: nickname,
		Coin:     coin,
		Diamond:  diamond,
	}
	data, err := proto.Marshal(req)
	if err != nil {
		return err
	}

	resPkt, err := b.CallWithTimeout(CmdUpdatePlayerReq, data, 5*time.Second)
	if err != nil {
		return err
	}

	res := &biz.UpdatePlayerRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return err
	}
	if res.Result != nil && res.Result.Code != 0 {
		return fmt.Errorf("update player failed: code=%d", res.Result.Code)
	}
	return nil
}

// Ping 发送心跳
func (b *Bot) Ping() (*biz.Pong, error) {
	req := &biz.Ping{ClientTime: uint64(time.Now().UnixMilli())}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}

	resPkt, err := b.CallWithTimeout(CmdPing, data, 5*time.Second)
	if err != nil {
		return nil, err
	}

	res := &biz.Pong{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return nil, err
	}
	return res, nil
}

// ==================== 聊天室方法 ====================

func (b *Bot) CreateRoom(name string, creatorID uint64) (*chat.ChatRoomInfo, error) {
	req := &chat.ChatCreateRoomReq{Name: name, CreatorId: creatorID}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}
	resPkt, err := b.CallWithTimeout(CmdChatCreateRoomReq, data, 5*time.Second)
	if err != nil {
		return nil, err
	}
	res := &chat.ChatCreateRoomRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return nil, err
	}
	if res.Result != nil && res.Result.Code != 0 {
		return nil, fmt.Errorf("create room failed: code=%d", res.Result.Code)
	}
	return res.Room, nil
}

func (b *Bot) JoinRoom(roomID, playerID uint64) (*chat.ChatJoinRoomRes, error) {
	req := &chat.ChatJoinRoomReq{RoomId: roomID, PlayerId: playerID}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}
	resPkt, err := b.CallWithTimeout(CmdChatJoinRoomReq, data, 5*time.Second)
	if err != nil {
		return nil, err
	}
	res := &chat.ChatJoinRoomRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return nil, err
	}
	if res.Result != nil && res.Result.Code != 0 {
		return nil, fmt.Errorf("join room failed: code=%d msg=%s", res.Result.Code, res.Result.Msg)
	}
	return res, nil
}

func (b *Bot) LeaveRoom(roomID, playerID uint64) error {
	req := &chat.ChatLeaveRoomReq{RoomId: roomID, PlayerId: playerID}
	data, err := proto.Marshal(req)
	if err != nil {
		return err
	}
	resPkt, err := b.CallWithTimeout(CmdChatLeaveRoomReq, data, 5*time.Second)
	if err != nil {
		return err
	}
	res := &chat.ChatLeaveRoomRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return err
	}
	if res.Result != nil && res.Result.Code != 0 {
		return fmt.Errorf("leave room failed: code=%d", res.Result.Code)
	}
	return nil
}

func (b *Bot) SendMsg(roomID, senderID uint64, content string) (*chat.ChatMessage, error) {
	req := &chat.ChatSendMsgReq{RoomId: roomID, SenderId: senderID, Content: content}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}
	resPkt, err := b.CallWithTimeout(CmdChatSendMsgReq, data, 5*time.Second)
	if err != nil {
		return nil, err
	}
	res := &chat.ChatSendMsgRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return nil, err
	}
	if res.Result != nil && res.Result.Code != 0 {
		return nil, fmt.Errorf("send msg failed: code=%d", res.Result.Code)
	}
	return res.Msg, nil
}

func (b *Bot) GetHistory(roomID uint64, limit uint32) ([]*chat.ChatMessage, error) {
	req := &chat.ChatGetHistoryReq{RoomId: roomID, Limit: limit}
	data, err := proto.Marshal(req)
	if err != nil {
		return nil, err
	}
	resPkt, err := b.CallWithTimeout(CmdChatGetHistoryReq, data, 5*time.Second)
	if err != nil {
		return nil, err
	}
	res := &chat.ChatGetHistoryRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return nil, err
	}
	if res.Result != nil && res.Result.Code != 0 {
		return nil, fmt.Errorf("get history failed: code=%d", res.Result.Code)
	}
	return res.Msgs, nil
}

func (b *Bot) CloseRoom(roomID, operatorID uint64) error {
	req := &chat.ChatCloseRoomReq{RoomId: roomID, OperatorId: operatorID}
	data, err := proto.Marshal(req)
	if err != nil {
		return err
	}
	resPkt, err := b.CallWithTimeout(CmdChatCloseRoomReq, data, 5*time.Second)
	if err != nil {
		return err
	}
	res := &chat.ChatCloseRoomRes{}
	if err := proto.Unmarshal(resPkt.Payload, res); err != nil {
		return err
	}
	if res.Result != nil && res.Result.Code != 0 {
		return fmt.Errorf("close room failed: code=%d", res.Result.Code)
	}
	return nil
}
