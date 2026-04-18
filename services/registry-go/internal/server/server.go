package server

import (
	"context"
	"fmt"
	"log"
	"sync"

	"github.com/gmaker/game-server/common/go/net"
	pb "github.com/gmaker/game-server/gen/go/registry"
	"github.com/gmaker/game-server/services/registry-go/internal/store"
	"google.golang.org/protobuf/proto"
)

// RegistryCmdID 内部 RPC 命令号
const (
	CmdRegister  = uint32(0x000F0001)
	CmdHeartbeat = uint32(0x000F0002)
	CmdDiscover  = uint32(0x000F0003)
	CmdWatch     = uint32(0x000F0004)
	CmdNodeEvent = uint32(0x000F0005)
)

type Server struct {
	addr      string
	store     store.Store
	tcpServer *net.TCPServer

	watchers sync.Map // conn_id -> *Watcher
}

type Watcher struct {
	conn   *net.TCPConn
	svcType string
	cancel context.CancelFunc
}

func New(addr string, store store.Store) *Server {
	return &Server{
		addr:  addr,
		store: store,
	}
}

func (s *Server) Start() error {
	cfg := net.ServerConfig{
		Addr: s.addr,
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			s.handlePacket(conn, pkt)
		},
		OnClose: func(conn *net.TCPConn) {
			s.onDisconnect(conn)
		},
	}
	s.tcpServer = net.NewTCPServer(cfg)
	return s.tcpServer.Start()
}

func (s *Server) Stop() {
	if s.tcpServer != nil {
		s.tcpServer.Stop()
	}
	s.watchers.Range(func(key, value interface{}) bool {
		w := value.(*Watcher)
		if w.cancel != nil {
			w.cancel()
		}
		return true
	})
}

func (s *Server) handlePacket(conn *net.TCPConn, pkt *net.Packet) {
	switch pkt.CmdID {
	case CmdRegister:
		s.handleRegister(conn, pkt)
	case CmdHeartbeat:
		s.handleHeartbeat(conn, pkt)
	case CmdDiscover:
		s.handleDiscover(conn, pkt)
	case CmdWatch:
		s.handleWatch(conn, pkt)
	default:
		s.sendError(conn, pkt.SeqID, fmt.Errorf("unknown cmd_id: %d", pkt.CmdID))
	}
}

func (s *Server) handleRegister(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.NodeInfo
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}

	leaseID, err := s.store.Register(context.Background(), &req)
	if err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}

	// 记录 leaseID 到本地（可选：用于心跳校验）
	_ = leaseID

	res := &pb.Result{Ok: true, Code: 0, Msg: "registered"}
	s.sendProto(conn, pkt.SeqID, CmdRegister, res)
}

func (s *Server) handleHeartbeat(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.NodeId
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}

	if err := s.store.Heartbeat(context.Background(), req.NodeId); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}

	res := &pb.Result{Ok: true, Code: 0, Msg: "heartbeat ok"}
	s.sendProto(conn, pkt.SeqID, CmdHeartbeat, res)
}

func (s *Server) handleDiscover(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.ServiceType
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}

	nodes, err := s.store.Discover(context.Background(), req.ServiceType)
	if err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}

	res := &pb.NodeList{Nodes: nodes}
	s.sendProto(conn, pkt.SeqID, CmdDiscover, res)
}

func (s *Server) handleWatch(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.ServiceType
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}

	ctx, cancel := context.WithCancel(context.Background())
	w := &Watcher{
		conn:    conn,
		svcType: req.ServiceType,
		cancel:  cancel,
	}
	s.watchers.Store(conn.ID(), w)

	go func() {
		defer cancel()
		events, err := s.store.Watch(ctx, req.ServiceType)
		if err != nil {
			log.Printf("watch error: %v", err)
			return
		}
		for ev := range events {
			data, err := proto.Marshal(ev)
			if err != nil {
				continue
			}
			out := &net.Packet{
				Header: net.Header{
					Magic:  net.MagicValue,
					CmdID:  CmdNodeEvent,
					SeqID:  0,
					Flags:  0,
					Length: uint32(net.HeaderSize + len(data)),
				},
				Payload: data,
			}
			if !conn.SendPacket(out) {
				return
			}
		}
	}()
}

func (s *Server) onDisconnect(conn *net.TCPConn) {
	if v, ok := s.watchers.Load(conn.ID()); ok {
		w := v.(*Watcher)
		if w.cancel != nil {
			w.cancel()
		}
		s.watchers.Delete(conn.ID())
	}
}

func (s *Server) sendProto(conn *net.TCPConn, seqID uint32, cmdID uint32, msg proto.Message) {
	data, err := proto.Marshal(msg)
	if err != nil {
		return
	}
	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  cmdID,
			SeqID:  seqID,
			Flags:  uint32(net.FlagRPCRes),
			Length: uint32(net.HeaderSize + len(data)),
		},
		Payload: data,
	}
	conn.SendPacket(pkt)
}

func (s *Server) sendError(conn *net.TCPConn, seqID uint32, err error) {
	res := &pb.Result{Ok: false, Code: 1, Msg: err.Error()}
	s.sendProto(conn, seqID, 0, res)
}
