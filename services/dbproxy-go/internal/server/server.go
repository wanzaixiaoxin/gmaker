package server

import (
	"context"
	"fmt"
	"log"
	"strconv"
	"time"

	"github.com/gmaker/luffa/common/go/net"
	pb "github.com/gmaker/luffa/gen/go/dbproxy"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"

	"github.com/gmaker/luffa/services/dbproxy-go/internal/mysql"
)

// task 表示一个待处理的 RPC 请求
type task struct {
	conn *net.TCPConn
	pkt  *net.Packet
}

// DBProxy 内部命令号
const (
	CmdMySQLQuery    = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY)
	CmdMySQLQueryRes = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY_RES)
	CmdMySQLExec     = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC)
	CmdMySQLExecRes  = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC_RES)
)

type Server struct {
	addr     string
	mysql    *mysql.Proxy
	server   *net.TCPServer
	workerCh chan task
	workers  int
}

// New 创建 DBProxy 服务器，默认 worker 数为 64
func New(addr string, m *mysql.Proxy) *Server {
	return &Server{addr: addr, mysql: m, workers: 64}
}

// SetWorkers 设置处理 worker 数量（必须在 Start 前调用）
func (s *Server) SetWorkers(n int) {
	if n > 0 {
		s.workers = n
	}
}

func (s *Server) Start() error {
	s.workerCh = make(chan task, s.workers*4)
	for i := 0; i < s.workers; i++ {
		go s.workerLoop()
	}
	cfg := net.ServerConfig{
		Addr: s.addr,
		OnData: func(conn *net.TCPConn, pkt *net.Packet) {
			select {
			case s.workerCh <- task{conn: conn, pkt: pkt}:
			default:
				log.Printf("[DBProxy] worker queue full, drop packet seq=%d", pkt.SeqID)
				s.sendError(conn, pkt.SeqID, fmt.Errorf("server busy"))
			}
		},
	}
	s.server = net.NewTCPServer(cfg)
	return s.server.Start()
}

func (s *Server) workerLoop() {
	for t := range s.workerCh {
		s.handlePacket(t.conn, t.pkt)
	}
}

func (s *Server) Stop() {
	if s.server != nil {
		s.server.Stop()
	}
}

func (s *Server) handlePacket(conn *net.TCPConn, pkt *net.Packet) {
	switch pkt.CmdID {
	case CmdMySQLQuery:
		s.handleMySQLQuery(conn, pkt)
	case CmdMySQLExec:
		s.handleMySQLExec(conn, pkt)
	default:
		s.sendError(conn, pkt.SeqID, fmt.Errorf("unknown cmd_id: %d", pkt.CmdID))
	}
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
				var valStr string
				switch v := vals[i].(type) {
				case []byte:
					valStr = string(v)
				case string:
					valStr = v
				case int64:
					valStr = strconv.FormatInt(v, 10)
				case float64:
					valStr = strconv.FormatFloat(v, 'f', -1, 64)
				case bool:
					valStr = strconv.FormatBool(v)
				case time.Time:
					valStr = v.Format(time.RFC3339)
				case nil:
					valStr = ""
				default:
					valStr = fmt.Sprintf("%v", v)
				}
				rowCols = append(rowCols, &pb.MySQLColumn{Name: col, Value: valStr})
			}
			res.Rows = append(res.Rows, &pb.MySQLRow{Columns: rowCols})
		}
	}
	s.sendProto(conn, pkt.SeqID, CmdMySQLQueryRes, res)
}

func (s *Server) handleMySQLExec(conn *net.TCPConn, pkt *net.Packet) {
	log.Printf("[DBProxy] received MySQLExec cmd=0x%08X seq=%d payload=%d", pkt.CmdID, pkt.SeqID, len(pkt.Payload))
	var req pb.MySQLExecReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		log.Printf("[DBProxy] unmarshal error: %v", err)
		s.sendError(conn, pkt.SeqID, err)
		return
	}
	log.Printf("[DBProxy] executing SQL: %s", req.Sql)
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
		log.Printf("[DBProxy] exec error: %v", err)
		res.Error = err.Error()
	} else {
		res.LastInsertId, _ = result.LastInsertId()
		res.RowsAffected, _ = result.RowsAffected()
		log.Printf("[DBProxy] exec ok, rowsAffected=%d", res.RowsAffected)
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
