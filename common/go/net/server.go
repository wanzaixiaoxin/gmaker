package net

import (
	"net"
	"sync"
)

// ServerConfig TCP 服务器配置
type ServerConfig struct {
	Addr        string
	MaxConn     int
	OnConnect   func(*TCPConn)
	OnData      func(*TCPConn, *Packet)
	OnClose     func(*TCPConn)
}

// TCPServer TCP 服务器
type TCPServer struct {
	config   ServerConfig
	listener net.Listener
	conns    sync.Map // uint64 -> *TCPConn
	closed   int32
	wg       sync.WaitGroup
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
	onClose := func(c *TCPConn) {
		s.conns.Delete(c.ID())
		if s.config.OnClose != nil {
			s.config.OnClose(c)
		}
	}

	conn := NewTCPConn(raw, onData, onClose)
	s.conns.Store(conn.ID(), conn)

	if s.config.OnConnect != nil {
		s.config.OnConnect(conn)
	}
}
