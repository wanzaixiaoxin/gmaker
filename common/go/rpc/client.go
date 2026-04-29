package rpc

import (
	"context"
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	"github.com/gmaker/luffa/common/go/net"
)

// callChanPool 复用 RPC 等待通道，减少高并发下的 GC 压力
var callChanPool = sync.Pool{
	New: func() interface{} {
		return make(chan *net.Packet, 1)
	},
}

func acquireCallChan() chan *net.Packet {
	return callChanPool.Get().(chan *net.Packet)
}

func releaseCallChan(ch chan *net.Packet) {
	select {
	case <-ch: // drain orphaned packet if any
	default:
	}
	callChanPool.Put(ch)
}

// Client RPC 客户端，支持单连接和连接池两种模式
type Client struct {
	sender  net.PacketSender
	pending sync.Map // seq_id -> chan *net.Packet
	seqID   atomic.Uint32
}

// NewClient 创建单连接 RPC 客户端（向后兼容）
func NewClient(conn *net.TCPConn) *Client {
	return &Client{sender: conn}
}

// NewClientWithPool 创建基于连接池的 RPC 客户端
func NewClientWithPool(pool net.PacketSender) *Client {
	return &Client{sender: pool}
}

// OnPacket 处理收到的数据包，根据 SeqID 分发给等待的 Call
func (c *Client) OnPacket(pkt *net.Packet) {
	if pkt.SeqID == 0 {
		return // Push 包，不处理
	}
	if ch, ok := c.pending.Load(pkt.SeqID); ok {
		select {
		case ch.(chan *net.Packet) <- pkt:
			c.pending.Delete(pkt.SeqID)
		default:
			// 通道满时不删除 entry，让 Call() 的 defer 或超时逻辑清理，
			// 避免响应被丢弃后 Call() 永远阻塞到 context 超时
		}
	}
}

// Call 同步 RPC 调用
func (c *Client) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	if c.sender == nil {
		return nil, fmt.Errorf("no sender configured")
	}

	seq := c.nextSeqID()
	ch := acquireCallChan()
	c.pending.Store(seq, ch)
	defer func() {
		c.pending.Delete(seq)
		releaseCallChan(ch)
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

	if !c.sender.SendPacket(pkt) {
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
	if c.sender == nil {
		return fmt.Errorf("no sender configured")
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
	if !c.sender.SendPacket(pkt) {
		return fmt.Errorf("send failed")
	}
	return nil
}

func (c *Client) nextSeqID() uint32 {
	return c.seqID.Add(1)
}
