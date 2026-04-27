package dbproxy

import (
	"context"
	"fmt"

	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/rpc"
)

// Client DBProxy 通用客户端（零业务耦合）
type Client struct {
	pool *net.UpstreamPool
	rpc  *rpc.Client
}

// NewClient 创建 DBProxy 客户端
func NewClient(pool *net.UpstreamPool) *Client {
	c := &Client{pool: pool}
	c.rpc = rpc.NewClientWithPool(pool)
	pool.SetOnData(func(_ *net.TCPConn, pkt *net.Packet) {
		if c.rpc != nil {
			c.rpc.OnPacket(pkt)
		}
	})
	return c
}

// Connect 连接 DBProxy（pool 由外部管理，此处仅校验）
func (c *Client) Connect() error {
	if c.pool == nil {
		return fmt.Errorf("dbproxy pool is nil")
	}
	return nil
}

// Close 关闭客户端（pool 由外部管理，此处不停止）
func (c *Client) Close() {}

// Call 通用 RPC 调用，业务层通过此接口访问 DBProxy
func (c *Client) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	return c.rpc.Call(ctx, cmdID, payload)
}

// OnPacket 处理 DBProxy 回包
func (c *Client) OnPacket(pkt *net.Packet) {
	if c.rpc != nil {
		c.rpc.OnPacket(pkt)
	}
}
