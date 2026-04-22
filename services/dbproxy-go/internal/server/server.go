package server

import (
	"context"
	"fmt"
	"log"
	"strconv"
	"time"

	"github.com/gmaker/luffa/common/go/net"
	pb "github.com/gmaker/luffa/gen/go/dbproxy"
	"google.golang.org/protobuf/proto"

	"github.com/gmaker/luffa/services/dbproxy-go/internal/mysql"
	"github.com/gmaker/luffa/services/dbproxy-go/internal/redis"
)

// DBProxy 内部命令号
const (
	CmdRedisGet       = uint32(0x000E0001)
	CmdRedisGetRes    = uint32(0x000E0002)
	CmdRedisSet       = uint32(0x000E0003)
	CmdRedisSetRes    = uint32(0x000E0004)
	CmdRedisDel       = uint32(0x000E0005)
	CmdRedisDelRes    = uint32(0x000E0006)
	CmdRedisPipeline  = uint32(0x000E0007)
	CmdRedisPipelineRes = uint32(0x000E0008)
	CmdMySQLQuery     = uint32(0x000E0011)
	CmdMySQLQueryRes  = uint32(0x000E0012)
	CmdMySQLExec      = uint32(0x000E0013)
	CmdMySQLExecRes   = uint32(0x000E0014)
)

type Server struct {
	addr   string
	redis  *redis.Proxy
	mysql  *mysql.Proxy
	server *net.TCPServer
}

func New(addr string, r *redis.Proxy, m *mysql.Proxy) *Server {
	return &Server{addr: addr, redis: r, mysql: m}
}

func (s *Server) Start() error {
	cfg := net.ServerConfig{
		Addr: s.addr,
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			s.handlePacket(conn, pkt)
		},
	}
	s.server = net.NewTCPServer(cfg)
	return s.server.Start()
}

func (s *Server) Stop() {
	if s.server != nil {
		s.server.Stop()
	}
}

func (s *Server) handlePacket(conn *net.TCPConn, pkt *net.Packet) {
	switch pkt.CmdID {
	case CmdRedisGet:
		s.handleRedisGet(conn, pkt)
	case CmdRedisSet:
		s.handleRedisSet(conn, pkt)
	case CmdRedisDel:
		s.handleRedisDel(conn, pkt)
	case CmdRedisPipeline:
		s.handleRedisPipeline(conn, pkt)
	case CmdMySQLQuery:
		s.handleMySQLQuery(conn, pkt)
	case CmdMySQLExec:
		s.handleMySQLExec(conn, pkt)
	default:
		s.sendError(conn, pkt.SeqID, fmt.Errorf("unknown cmd_id: %d", pkt.CmdID))
	}
}

func (s *Server) handleRedisGet(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.RedisGetReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	val, err := s.redis.Get(ctx, req.Key)
	res := &pb.RedisGetRes{Ok: err == nil, Value: val}
	s.sendProto(conn, pkt.SeqID, CmdRedisGetRes, res)
}

func (s *Server) handleRedisSet(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.RedisSetReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	var ttl time.Duration
	if req.TtlSec > 0 {
		ttl = time.Duration(req.TtlSec) * time.Second
	}
	err := s.redis.Set(ctx, req.Key, req.Value, ttl)
	res := &pb.RedisSetRes{Ok: err == nil}
	s.sendProto(conn, pkt.SeqID, CmdRedisSetRes, res)
}

func (s *Server) handleRedisDel(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.RedisDelReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	err := s.redis.Del(ctx, req.Keys...)
	res := &pb.RedisDelRes{Ok: err == nil}
	s.sendProto(conn, pkt.SeqID, CmdRedisDelRes, res)
}

func (s *Server) handleRedisPipeline(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.RedisPipelineReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()
	var cmds []redis.Cmd
	for _, c := range req.Cmds {
		cmds = append(cmds, redis.Cmd{
			Op:  c.Op,
			Key: c.Key,
			Val: c.Value,
			TTL: time.Duration(c.TtlSec) * time.Second,
		})
	}
	results, err := s.redis.PipelineExec(ctx, cmds)
	res := &pb.RedisPipelineRes{Ok: err == nil}
	for _, r := range results {
		if r == nil {
			res.Results = append(res.Results, []byte("(nil)"))
		} else {
			res.Results = append(res.Results, []byte(fmt.Sprintf("%v", r)))
		}
	}
	s.sendProto(conn, pkt.SeqID, CmdRedisPipelineRes, res)
}

func (s *Server) handleMySQLQuery(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.MySQLQueryReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	args := make([]interface{}, 0, len(req.Args))
	for _, a := range req.Args {
		args = append(args, a)
	}
	rows, err := s.mysql.QueryByUID(ctx, req.Uid, req.Sql, args...)
	res := &pb.MySQLQueryRes{Ok: err == nil}
	if err != nil {
		res.Error = err.Error()
	} else {
		defer rows.Close()
		cols, _ := rows.Columns()
		vals := make([]interface{}, len(cols))
		scan := make([]interface{}, len(cols))
		for i := range vals {
			scan[i] = &vals[i]
		}
		for rows.Next() {
			if err := rows.Scan(scan...); err != nil {
				continue
			}
			var rowCols []*pb.MySQLColumn
			for i, col := range cols {
				rowCols = append(rowCols, &pb.MySQLColumn{Name: col, Value: fmt.Sprintf("%v", vals[i])})
			}
			res.Rows = append(res.Rows, &pb.MySQLRow{Columns: rowCols})
		}
	}
	s.sendProto(conn, pkt.SeqID, CmdMySQLQueryRes, res)
}

func (s *Server) handleMySQLExec(conn *net.TCPConn, pkt *net.Packet) {
	var req pb.MySQLExecReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		s.sendError(conn, pkt.SeqID, err)
		return
	}
	ctx, cancel := context.WithTimeout(context.Background(), 3*time.Second)
	defer cancel()

	args := make([]interface{}, 0, len(req.Args))
	for _, a := range req.Args {
		// 尝试解析为数字
		if n, err := strconv.ParseInt(a, 10, 64); err == nil {
			args = append(args, n)
		} else {
			args = append(args, a)
		}
	}
	result, err := s.mysql.ExecByUID(ctx, req.Uid, req.Sql, args...)
	res := &pb.MySQLExecRes{Ok: err == nil}
	if err != nil {
		res.Error = err.Error()
	} else {
		res.LastInsertId, _ = result.LastInsertId()
		res.RowsAffected, _ = result.RowsAffected()
	}
	s.sendProto(conn, pkt.SeqID, CmdMySQLExecRes, res)
}

func (s *Server) sendProto(conn *net.TCPConn, seqID uint32, cmdID uint32, msg proto.Message) {
	data, err := proto.Marshal(msg)
	if err != nil {
		log.Printf("marshal error: %v", err)
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
	// 统一用 MySQLExecRes 作为错误承载（简化）
	res := &pb.MySQLExecRes{Ok: false, Error: err.Error()}
	s.sendProto(conn, seqID, CmdMySQLExecRes, res)
}
