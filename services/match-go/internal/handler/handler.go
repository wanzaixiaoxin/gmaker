package handler

import (
	"encoding/binary"
	"fmt"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/services/match-go/internal/engine"
	"github.com/gmaker/luffa/services/match-go/internal/model"
	"github.com/gmaker/luffa/services/match-go/internal/service"

	commonpb "github.com/gmaker/luffa/gen/go/common"
	matchpb "github.com/gmaker/luffa/gen/go/match"
	"google.golang.org/protobuf/proto"
)

const (
	cmdMatchReq       uint32 = 0x00050000
	cmdMatchRes       uint32 = 0x00050001
	cmdMatchCancelReq uint32 = 0x00050002
	cmdMatchCancelRes uint32 = 0x00050003
	cmdMatchStatusReq uint32 = 0x00050004
	cmdMatchStatusRes uint32 = 0x00050005
)

// HandleMatchPacket 匹配服务包分发
// Gateway 转发时会在 payload 前附加 8 字节 conn_id，需先提取
func HandleMatchPacket(conn *net.TCPConn, pkt *net.Packet, svc *service.MatchService) {
	log := svc.Log()

	// 提取 gateway_conn_id
	var gatewayConnID uint64
	if len(pkt.Payload) >= 8 {
		gatewayConnID = binary.BigEndian.Uint64(pkt.Payload[:8])
		pkt.Payload = pkt.Payload[8:]
	}

	log.Info(fmt.Sprintf("[Flow] Gateway -> Match: cmd=0x%08X seq=%d payload=%d", pkt.CmdID, pkt.SeqID, len(pkt.Payload)))

	switch pkt.CmdID {
	case cmdMatchReq:
		handleMatchReq(conn, pkt, svc, gatewayConnID)
	case cmdMatchCancelReq:
		handleMatchCancelReq(conn, pkt, svc, gatewayConnID)
	case cmdMatchStatusReq:
		handleMatchStatusReq(conn, pkt, svc, gatewayConnID)
	default:
		log.Warnf("unknown cmd_id: 0x%08X", pkt.CmdID)
	}
}

func handleMatchReq(conn *net.TCPConn, pkt *net.Packet, svc *service.MatchService, gwConnID uint64) {
	req := &matchpb.MatchReq{}
	if err := proto.Unmarshal(pkt.Payload, req); err != nil {
		// 降级：尝试二进制手动解析
		if len(pkt.Payload) >= 20 {
			req.PlayerId = binary.BigEndian.Uint64(pkt.Payload[0:8])
			req.Mmr = int32(binary.BigEndian.Uint32(pkt.Payload[8:12]))
			req.Mode = int32(binary.BigEndian.Uint32(pkt.Payload[12:16]))
			req.TeamSize = int32(binary.BigEndian.Uint32(pkt.Payload[16:20]))
		} else {
			sendMatchRes(conn, pkt.SeqID, 1, "invalid request", nil, nil, 0, 0, gwConnID)
			return
		}
	}

	teamSize := req.GetTeamSize()
	if teamSize <= 0 {
		switch req.GetMode() {
		case 1:
			teamSize = 5
		case 2:
			teamSize = 3
		case 3:
			teamSize = 1
		default:
			teamSize = 5
		}
	}

	ticket := &model.Ticket{
		UID:      req.GetPlayerId(),
		MMR:      req.GetMmr(),
		Mode:     req.GetMode(),
		TeamSize: teamSize,
	}

	if err := svc.Enqueue(ticket, gwConnID, conn); err != nil {
		code := uint32(1)
		if me, ok := err.(*engine.MatchError); ok {
			code = uint32(me.Code)
		}
		sendMatchRes(conn, pkt.SeqID, code, err.Error(), nil, nil, 0, 0, gwConnID)
		return
	}

	// 入队成功，返回等待状态
	sendMatchRes(conn, pkt.SeqID, 0, "queuing", nil, nil, 0, 0, gwConnID)
}

func handleMatchCancelReq(conn *net.TCPConn, pkt *net.Packet, svc *service.MatchService, gwConnID uint64) {
	req := &matchpb.MatchCancelReq{}
	if err := proto.Unmarshal(pkt.Payload, req); err != nil {
		// 降级：二进制解析
		if len(pkt.Payload) >= 8 {
			req.PlayerId = binary.BigEndian.Uint64(pkt.Payload[0:8])
		} else {
			sendMatchCancelRes(conn, pkt.SeqID, 1, "invalid request", gwConnID)
			return
		}
	}

	if err := svc.Dequeue(req.GetPlayerId()); err != nil {
		code := uint32(1)
		if me, ok := err.(*engine.MatchError); ok {
			code = uint32(me.Code)
		}
		sendMatchCancelRes(conn, pkt.SeqID, code, err.Error(), gwConnID)
		return
	}

	sendMatchCancelRes(conn, pkt.SeqID, 0, "ok", gwConnID)
}

func handleMatchStatusReq(conn *net.TCPConn, pkt *net.Packet, svc *service.MatchService, gwConnID uint64) {
	req := &matchpb.MatchStatusReq{}
	if err := proto.Unmarshal(pkt.Payload, req); err != nil {
		// 降级：二进制解析
		if len(pkt.Payload) >= 8 {
			req.PlayerId = binary.BigEndian.Uint64(pkt.Payload[0:8])
		} else {
			sendMatchStatusRes(conn, pkt.SeqID, 1, "invalid request", "idle", 0, 0, gwConnID)
			return
		}
	}

	state, waitMs, queueLen := svc.GetStatus(req.GetPlayerId())
	sendMatchStatusRes(conn, pkt.SeqID, 0, "ok", state, waitMs, queueLen, gwConnID)
}

// ============================================================
// 响应发送
// ============================================================

func sendMatchRes(conn *net.TCPConn, seqID uint32, code uint32, msg string, teamA, teamB []uint64, matchID, roomID uint64, gwConnID uint64) {
	res := &matchpb.MatchRes{
		Result:  &commonpb.Result{Ok: code == 0, Code: code, Msg: msg},
		MatchId: matchID,
		RoomId:  roomID,
		TeamA:   teamA,
		TeamB:   teamB,
	}
	SendProto(conn, seqID, cmdMatchRes, res, gwConnID)
}

func sendMatchCancelRes(conn *net.TCPConn, seqID uint32, code uint32, msg string, gwConnID uint64) {
	res := &matchpb.MatchCancelRes{
		Result: &commonpb.Result{Ok: code == 0, Code: code, Msg: msg},
	}
	SendProto(conn, seqID, cmdMatchCancelRes, res, gwConnID)
}

func sendMatchStatusRes(conn *net.TCPConn, seqID uint32, code uint32, msg string, state string, waitMs int32, queueLen int, gwConnID uint64) {
	res := &matchpb.MatchStatusRes{
		Result:   &commonpb.Result{Ok: code == 0, Code: code, Msg: msg},
		State:    state,
		WaitMs:   waitMs,
		QueueLen: int32(queueLen),
	}
	SendProto(conn, seqID, cmdMatchStatusRes, res, gwConnID)
}

// SendProto 发送 protobuf 响应给 Gateway
// payload 前附加 8 字节 conn_id（大端序）用于 Gateway 路由回客户端
func SendProto(conn *net.TCPConn, seqID uint32, cmdID uint32, msg proto.Message, gatewayConnID uint64) {
	data, err := proto.Marshal(msg)
	if err != nil {
		logger.Errorf("marshal error: %v", err)
		return
	}
	payload := make([]byte, 8+len(data))
	binary.BigEndian.PutUint64(payload, gatewayConnID)
	copy(payload[8:], data)
	pkt := &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  cmdID,
			SeqID:  seqID,
			Flags:  uint32(net.FlagRPCRes),
			Length: uint32(net.HeaderSize + len(payload)),
		},
		Payload: payload,
	}
	conn.SendPacket(pkt)
}
