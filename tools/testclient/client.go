package main

import (
	"context"
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gmaker/game-server/common/go/net"
	"github.com/gmaker/game-server/gen/go/biz"
	"github.com/gmaker/game-server/gen/go/login"
	"google.golang.org/protobuf/proto"
)

// CmdIDs
const (
	CmdLoginReq        uint32 = 0x00001000
	CmdLoginRes        uint32 = 0x00001001
	CmdGetPlayerReq    uint32 = 0x00010000
	CmdGetPlayerRes    uint32 = 0x00010001
	CmdUpdatePlayerReq uint32 = 0x00010006
	CmdUpdatePlayerRes uint32 = 0x00010007
	CmdPing            uint32 = 0x00010004
	CmdPong            uint32 = 0x00010005
)

// Bot 表示一个模拟客户端
type Bot struct {
	id        int
	addr      string
	masterKey []byte
	client    *net.TCPClient

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
func NewBot(id int, addr string, masterKey []byte) *Bot {
	return &Bot{
		id:        id,
		addr:      addr,
		masterKey: masterKey,
		pending:   make(map[uint32]chan *net.Packet),
	}
}

// Connect 连接到 Gateway 并完成握手
func (b *Bot) Connect() error {
	b.client = net.NewTCPClient(b.addr,
		func(_ *net.TCPConn, pkt *net.Packet) { b.onPacket(pkt) },
		func(_ *net.TCPConn) {},
	)
	if len(b.masterKey) > 0 {
		b.client.SetMasterKey(b.masterKey)
	}
	return b.client.Connect()
}

// Close 断开连接
func (b *Bot) Close() {
	if b.client != nil {
		b.client.Close()
	}
}

// IsConnected 检查是否已连接
func (b *Bot) IsConnected() bool {
	return b.client != nil && b.client.Conn() != nil
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

	if !b.client.Conn().SendPacket(pkt) {
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
		return fmt.Errorf("login failed: code=%d msg=%s", res.Result.Code, res.Result.Message)
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
