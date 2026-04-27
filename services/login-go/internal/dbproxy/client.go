package dbproxy

import (
	"context"

	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/rpc"
)

// Client DBProxy 客户端
type Client interface {
	Connect() error
	Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error)
	Close()
	OnPacket(pkt *net.Packet)
}

// NewClient 创建 DBProxy 客户端（地址列表模式）
func NewClient(addrs []string) Client {
	return &client{addrs: addrs}
}

// NewClientWithPool 使用外部 pool 创建 DBProxy 客户端（Registry 发现模式）
func NewClientWithPool(pool *net.UpstreamPool) Client {
	c := &client{pool: pool}
	c.rpc = rpc.NewClientWithPool(pool)
	pool.SetOnData(func(_ *net.TCPConn, pkt *net.Packet) {
		if c.rpc != nil {
			c.rpc.OnPacket(pkt)
		}
	})
	return c
}

type client struct {
	addrs []string
	pool  *net.UpstreamPool
	rpc   *rpc.Client
}

func (c *client) Connect() error {
	if c.pool != nil {
		return nil
	}
	c.pool = net.NewUpstreamPool(func(_ *net.TCPConn, pkt *net.Packet) {
		if c.rpc != nil {
			c.rpc.OnPacket(pkt)
		}
	})
	for _, addr := range c.addrs {
		c.pool.AddNode(addr)
	}
	c.pool.Start()
	c.rpc = rpc.NewClientWithPool(c.pool)
	return nil
}

func (c *client) OnPacket(pkt *net.Packet) {
	if c.rpc != nil {
		c.rpc.OnPacket(pkt)
	}
}

func (c *client) Close() {
	if c.pool != nil && len(c.addrs) > 0 {
		c.pool.Stop()
	}
}

func (c *client) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	return c.rpc.Call(ctx, cmdID, payload)
}
