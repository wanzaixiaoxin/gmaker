package net

import (
	"net"
	"time"
)

// TCPClient TCP 客户端
type TCPClient struct {
	addr    string
	conn    *TCPConn
	onData  func(*TCPConn, *Packet)
	onClose func(*TCPConn)
}

// NewTCPClient 创建客户端
func NewTCPClient(addr string, onData func(*TCPConn, *Packet), onClose func(*TCPConn)) *TCPClient {
	return &TCPClient{
		addr:    addr,
		onData:  onData,
		onClose: onClose,
	}
}

// Connect 建立连接
func (c *TCPClient) Connect() error {
	raw, err := net.DialTimeout("tcp", c.addr, 5*time.Second)
	if err != nil {
		return err
	}
	onClose := func(conn *TCPConn) {
		c.conn = nil
		if c.onClose != nil {
			c.onClose(conn)
		}
	}
	c.conn = NewTCPConn(raw, c.onData, onClose)
	return nil
}

// Conn 返回当前连接
func (c *TCPClient) Conn() *TCPConn {
	return c.conn
}

// Close 关闭连接
func (c *TCPClient) Close() {
	if c.conn != nil {
		c.conn.Close()
		c.conn = nil
	}
}
