package handler

import (
	"context"
	"encoding/binary"
	"fmt"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/trace"
	"github.com/gmaker/luffa/services/biz-go/internal/service"

	bizpb "github.com/gmaker/luffa/gen/go/biz"
	chatpb "github.com/gmaker/luffa/gen/go/chat"
	commonpb "github.com/gmaker/luffa/gen/go/common"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

const (
	cmdGetPlayerReq       = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_REQ)
	cmdGetPlayerRes       = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_RES)
	cmdUpdatePlayerReq    = uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_REQ)
	cmdUpdatePlayerRes    = uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_RES)
	cmdGetPlayerRoomsReq  = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_ROOMS_REQ)
	cmdGetPlayerRoomsRes  = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_ROOMS_RES)
	cmdPing               = uint32(protocol.CmdBiz_CMD_BIZ_PING)
	cmdPong               = uint32(protocol.CmdBiz_CMD_BIZ_PONG)
	cmdPlayerBind         = uint32(protocol.CmdGatewayInternal_CMD_GW_PLAYER_BIND)
)

// HandleBizPacket 业务包分发
// Gateway 转发时会在 payload 前附加 8 字节 conn_id，Biz 需要先提取
func HandleBizPacket(conn *net.TCPConn, pkt *net.Packet, svc *service.PlayerService) {
	ctx, traceID := trace.Ensure(context.Background())
	_ = ctx
	log := svc.Log.WithTrace(traceID)
	svc.Log.Info(fmt.Sprintf("[Flow] Gateway -> Biz: cmd=0x%08X seq=%d flags=%d payload=%d", pkt.CmdID, pkt.SeqID, pkt.Flags, len(pkt.Payload)))

	// Gateway 在 payload 前附加了 conn_id（8 字节 BE），跳过
	var gatewayConnID uint64
	if len(pkt.Payload) >= 8 {
		gatewayConnID = binary.BigEndian.Uint64(pkt.Payload[:8])
		pkt.Payload = pkt.Payload[8:]
	}

	switch pkt.CmdID {
	case cmdGetPlayerReq:
		log.Infof("[%s] get_player req", traceID)
		handleGetPlayer(conn, pkt, svc, traceID, gatewayConnID)
	case cmdUpdatePlayerReq:
		log.Infof("[%s] update_player req", traceID)
		handleUpdatePlayer(conn, pkt, svc, traceID, gatewayConnID)
	case cmdGetPlayerRoomsReq:
		log.Infof("[%s] get_player_rooms req", traceID)
		handleGetPlayerRooms(conn, pkt, svc, traceID, gatewayConnID)
	case cmdPing:
		handlePing(conn, pkt, traceID, gatewayConnID)
	case cmdPlayerBind:
		log.Infof("[%s] player_bind req", traceID)
		handlePlayerBind(conn, pkt, svc, traceID, gatewayConnID)
	default:
		log.Warnf("unknown cmd_id: 0x%08X", pkt.CmdID)
	}
}

func handleGetPlayer(conn *net.TCPConn, pkt *net.Packet, svc *service.PlayerService, traceID string, gatewayConnID uint64) {
	var req bizpb.GetPlayerReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendGetPlayerRes(conn, pkt.SeqID, 1, nil, gatewayConnID)
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	player, err := svc.QueryPlayerByID(ctx, req.PlayerId)
	if err != nil {
		sendGetPlayerRes(conn, pkt.SeqID, 1, nil, gatewayConnID)
		return
	}
	sendGetPlayerRes(conn, pkt.SeqID, 0, player, gatewayConnID)
}

func handleUpdatePlayer(conn *net.TCPConn, pkt *net.Packet, svc *service.PlayerService, traceID string, gatewayConnID uint64) {
	var req bizpb.UpdatePlayerReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendUpdatePlayerRes(conn, pkt.SeqID, 1, gatewayConnID)
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	if err := svc.UpdatePlayer(ctx, req.PlayerId, req.Nickname, req.Coin, req.Diamond); err != nil {
		svc.Log.WithTrace(traceID).Errorf("update player failed: %v", err)
		sendUpdatePlayerRes(conn, pkt.SeqID, 1, gatewayConnID)
		return
	}
	sendUpdatePlayerRes(conn, pkt.SeqID, 0, gatewayConnID)
}

func handleGetPlayerRooms(conn *net.TCPConn, pkt *net.Packet, svc *service.PlayerService, traceID string, gatewayConnID uint64) {
	var req bizpb.GetPlayerRoomsReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendGetPlayerRoomsRes(conn, pkt.SeqID, 1, nil, gatewayConnID)
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	rooms, err := svc.GetPlayerRooms(ctx, req.PlayerId)
	if err != nil {
		svc.Log.WithTrace(traceID).Errorf("get player rooms failed: %v", err)
		sendGetPlayerRoomsRes(conn, pkt.SeqID, 1, nil, gatewayConnID)
		return
	}
	sendGetPlayerRoomsRes(conn, pkt.SeqID, 0, rooms, gatewayConnID)
}

func handlePing(conn *net.TCPConn, pkt *net.Packet, traceID string, gatewayConnID uint64) {
	var req bizpb.Ping
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		logger.Info(fmt.Sprintf("[Flow] Biz -> Gateway: ping parse error: %v", err))
		return
	}
	res := &bizpb.Pong{ClientTime: req.ClientTime, ServerTime: uint64(time.Now().UnixMilli())}
	logger.Info(fmt.Sprintf("[Flow] Biz -> Gateway: cmd=0x%08X seq=%d (pong)", cmdPong, pkt.SeqID))
	SendProto(conn, pkt.SeqID, cmdPong, res, gatewayConnID)
}

func handlePlayerBind(conn *net.TCPConn, pkt *net.Packet, svc *service.PlayerService, traceID string, gatewayConnID uint64) {
	var req protocol.PlayerBindReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		svc.Log.WithTrace(traceID).Warnf("player_bind parse error: %v", err)
		sendPlayerBindRes(conn, pkt.SeqID, 1, "invalid request", gatewayConnID)
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	// 验证 Token（查 Redis），若匹配 bot_master_key 则跳过校验直接放行
	if svc.BotMasterKey != "" && req.GetToken() == svc.BotMasterKey {
		svc.Log.WithTrace(traceID).Infof("player_bind bot_master_key matched: player_id=%d", req.GetPlayerId())
	} else {
		ok, err := svc.VerifyToken(ctx, req.GetPlayerId(), req.GetToken())
		if err != nil {
			svc.Log.WithTrace(traceID).Warnf("player_bind token verify error: player_id=%d, err=%v", req.GetPlayerId(), err)
			// Redis 不可用返回明确错误，避免客户端误以为是 token 错误
			if err.Error() == "redis not available" {
				sendPlayerBindRes(conn, pkt.SeqID, 2, "redis not available", gatewayConnID)
			} else {
				sendPlayerBindRes(conn, pkt.SeqID, 1, "invalid token", gatewayConnID)
			}
			return
		}
		if !ok {
			svc.Log.WithTrace(traceID).Warnf("player_bind token mismatch: player_id=%d", req.GetPlayerId())
			sendPlayerBindRes(conn, pkt.SeqID, 1, "invalid token", gatewayConnID)
			return
		}
	}

	svc.Log.WithTrace(traceID).Infof("player_bind success: player_id=%d", req.GetPlayerId())
	sendPlayerBindRes(conn, pkt.SeqID, 0, "ok", gatewayConnID)
}

func sendPlayerBindRes(conn *net.TCPConn, seqID uint32, code uint32, msg string, gatewayConnID uint64) {
	res := &protocol.PlayerBindRes{
		Code: code,
		Msg:  msg,
	}
	SendProto(conn, seqID, cmdPlayerBind, res, gatewayConnID)
}

func sendGetPlayerRes(conn *net.TCPConn, seqID uint32, code uint32, player *bizpb.PlayerBase, gatewayConnID uint64) {
	res := &bizpb.GetPlayerRes{
		Result: &commonpb.Result{Ok: code == 0, Code: code},
		Player: player,
	}
	SendProto(conn, seqID, cmdGetPlayerRes, res, gatewayConnID)
}

func sendUpdatePlayerRes(conn *net.TCPConn, seqID uint32, code uint32, gatewayConnID uint64) {
	res := &bizpb.UpdatePlayerRes{
		Result: &commonpb.Result{Ok: code == 0, Code: code},
	}
	SendProto(conn, seqID, cmdUpdatePlayerRes, res, gatewayConnID)
}

func sendGetPlayerRoomsRes(conn *net.TCPConn, seqID uint32, code uint32, rooms []*chatpb.ChatRoomInfo, gatewayConnID uint64) {
	res := &bizpb.GetPlayerRoomsRes{
		Result: &commonpb.Result{Ok: code == 0, Code: code},
		Rooms:  rooms,
	}
	SendProto(conn, seqID, cmdGetPlayerRoomsRes, res, gatewayConnID)
}

// SendProto 发送 protobuf 响应给 Gateway
// Gateway 要求 payload 前附加 8 字节 conn_id（大端序）用于路由回客户端
func SendProto(conn *net.TCPConn, seqID uint32, cmdID uint32, msg proto.Message, gatewayConnID uint64) {
	data, err := proto.Marshal(msg)
	if err != nil {
		logger.Errorf("marshal error: %v", err)
		return
	}
	// 在 payload 前附加 conn_id（Gateway 路由需要）
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
