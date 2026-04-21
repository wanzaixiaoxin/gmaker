package net

import (
	"net"
	"sync"
	"time"
)

// Middleware 中间件接口，返回 false 表示拦截该包
type Middleware interface {
	OnPacket(conn *TCPConn, pkt *Packet) bool
}

// MiddlewareFunc 函数适配器，实现 Middleware 接口
type MiddlewareFunc func(conn *TCPConn, pkt *Packet) bool

func (f MiddlewareFunc) OnPacket(conn *TCPConn, pkt *Packet) bool {
	return f(conn, pkt)
}

// ServerConfig TCP 服务器配置
type ServerConfig struct {
	Addr              string
	MaxConn           int
	HeartbeatTimeout  time.Duration // 默认 30s，0 表示不检测
	OnConnect         func(*TCPConn)
	OnData            func(*TCPConn, *Packet)
	OnClose           func(*TCPConn)
}

// TCPServer TCP 服务器
type TCPServer struct {
	config      ServerConfig
	listener    net.Listener
	conns       sync.Map // uint64 -> *TCPConn
	closed      int32
	wg          sync.WaitGroup
	middlewares []Middleware
}

// NewTCPServer 创建服务器
func NewTCPServer(cfg ServerConfig) *TCPServer {
	return &TCPServer{config: cfg}
}

// Start 开始监听
func (s *TCPServer) Start() error {
	ln, err := net.Listen("tcp", s.config.Addr)
	if err != nil {
		return err
	}
	s.listener = ln
	go s.acceptLoop()
	return nil
}

// Stop 停止服务器
func (s *TCPServer) Stop() {
	if s.listener != nil {
		s.listener.Close()
	}
	s.conns.Range(func(key, value interface{}) bool {
		value.(*TCPConn).Close()
		return true
	})
	s.wg.Wait()
}

// Use 注册中间件
func (s *TCPServer) Use(mw ...Middleware) {
	s.middlewares = append(s.middlewares, mw...)
}

// GetConn 按 ID 获取连接
func (s *TCPServer) GetConn(id uint64) (*TCPConn, bool) {
	v, ok := s.conns.Load(id)
	if !ok {
		return nil, false
	}
	return v.(*TCPConn), true
}

// Broadcast 广播到所有连接
func (s *TCPServer) Broadcast(p *Packet) {
	data := p.Encode()
	s.conns.Range(func(key, value interface{}) bool {
		value.(*TCPConn).Send(data)
		return true
	})
}

func (s *TCPServer) acceptLoop() {
	for {
		conn, err := s.listener.Accept()
		if err != nil {
			return
		}
		// 连接数上限检查
		if s.config.MaxConn > 0 {
			count := 0
			s.conns.Range(func(_, _ interface{}) bool {
				count++
				return true
			})
			if count >= s.config.MaxConn {
				conn.Close()
				continue
			}
		}
		s.wg.Add(1)
		go s.handleConn(conn)
	}
}

func (s *TCPServer) handleConn(raw net.Conn) {
	defer s.wg.Done()

	onData := s.config.OnData
	if len(s.middlewares) > 0 {
		onData = func(c *TCPConn, p *Packet) {
			for _, mw := range s.middlewares {
				if !mw.OnPacket(c, p) {
					return
				}
			}
			if s.config.OnData != nil {
				s.config.OnData(c, p)
			}
		}
	}

	onClose := func(c *TCPConn) {
		s.conns.Delete(c.ID())
		if s.config.OnClose != nil {
			s.config.OnClose(c)
		}
	}

	conn := NewTCPConn(raw, onData, onClose)
	s.conns.Store(conn.ID(), conn)

	if s.config.HeartbeatTimeout > 0 {
		conn.SetHeartbeatTimeout(s.config.HeartbeatTimeout)
	}

	if s.config.OnConnect != nil {
		s.config.OnConnect(conn)
	}
}
