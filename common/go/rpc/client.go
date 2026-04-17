package rpc

import (
	"context"
	"fmt"
	"sync"
	"time"

	"github.com/gmaker/game-server/common/go/net"
)

// Client RPC 客户端（基于已有的 TCPConn）
type Client struct {
	conn    *net.TCPConn
	pending sync.Map // seq_id -> chan *net.Packet
	seqMu   sync.Mutex
	seqID   uint32
}

// NewClient 创建 RPC 客户端
func NewClient(conn *net.TCPConn) *Client {
	c := &Client{conn: conn}
	// 接管 conn 的数据回调，用于分发响应
	// 注意：这里假设外部不再直接设置 OnData
	return c
}

// SetConn 设置底层连接（通常在重连后调用）
func (c *Client) SetConn(conn *net.TCPConn) {
	c.conn = conn
}

// OnPacket 处理收到的数据包，根据 SeqID 分发给等待的 Call
func (c *Client) OnPacket(pkt *net.Packet) {
	if pkt.SeqID == 0 {
		return // Push 包，不处理
	}
	if ch, ok := c.pending.Load(pkt.SeqID); ok {
		select {
		case ch.(chan *net.Packet) <- pkt:
		default:
		}
		c.pending.Delete(pkt.SeqID)
	}
}

// Call 同步 RPC 调用
func (c *Client) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	if c.conn == nil {
		return nil, fmt.Errorf("no connection")
	}

	seq := c.nextSeqID()
	ch := make(chan *net.Packet, 1)
	c.pending.Store(seq, ch)
	defer c.pending.Delete(seq)

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

	if !c.conn.SendPacket(pkt) {
		return nil, fmt.Errorf("send failed")
	}

	select {
	case res := <-ch:
		return res, nil
	case <-ctx.Done():
		return nil, ctx.Err()
	}
}

// CallWithTimeout 带默认超时的 Call
func (c *Client) CallWithTimeout(cmdID uint32, payload []byte, timeout time.Duration) (*net.Packet, error) {
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	return c.Call(ctx, cmdID, payload)
}

// FireForget 单向发送，不等待响应
func (c *Client) FireForget(cmdID uint32, payload []byte) error {
	if c.conn == nil {
		return fmt.Errorf("no connection")
	}
	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  cmdID,
			SeqID:  0,
			Flags:  uint32(net.FlagRPCFF),
			Length: uint32(net.HeaderSize + len(payload)),
		},
		Payload: payload,
	}
	if !c.conn.SendPacket(pkt) {
		return fmt.Errorf("send failed")
	}
	return nil
}

func (c *Client) nextSeqID() uint32 {
	c.seqMu.Lock()
	defer c.seqMu.Unlock()
	c.seqID++
	return c.seqID
}
