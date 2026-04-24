package handler

import (
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"fmt"
	"time"

	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/trace"
	"github.com/gmaker/luffa/services/biz-go/internal/service"

	bizpb "github.com/gmaker/luffa/gen/go/biz"
	commonpb "github.com/gmaker/luffa/gen/go/common"
	loginpb "github.com/gmaker/luffa/gen/go/login"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

const (
	cmdLoginReq        = uint32(protocol.CmdCommon_CMD_CMN_LOGIN_REQ)
	cmdLoginRes        = uint32(protocol.CmdCommon_CMD_CMN_LOGIN_RES)
	cmdGetPlayerReq    = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_REQ)
	cmdGetPlayerRes    = uint32(protocol.CmdBiz_CMD_BIZ_GET_PLAYER_RES)
	cmdUpdatePlayerReq = uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_REQ)
	cmdUpdatePlayerRes = uint32(protocol.CmdBiz_CMD_BIZ_UPDATE_PLAYER_RES)
	cmdPing            = uint32(protocol.CmdBiz_CMD_BIZ_PING)
	cmdPong            = uint32(protocol.CmdBiz_CMD_BIZ_PONG)
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
	case cmdLoginReq:
		log.Infof("[%s] login req", traceID)
		handleLogin(conn, pkt, svc, traceID, gatewayConnID)
	case cmdGetPlayerReq:
		log.Infof("[%s] get_player req", traceID)
		handleGetPlayer(conn, pkt, svc, traceID, gatewayConnID)
	case cmdUpdatePlayerReq:
		log.Infof("[%s] update_player req", traceID)
		handleUpdatePlayer(conn, pkt, svc, traceID, gatewayConnID)
	case cmdPing:
		handlePing(conn, pkt, traceID, gatewayConnID)
	default:
		log.Warnf("unknown cmd_id: 0x%08X", pkt.CmdID)
	}
}

func handleLogin(conn *net.TCPConn, pkt *net.Packet, svc *service.PlayerService, traceID string, gatewayConnID uint64) {
	var req loginpb.LoginReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendLoginRes(conn, pkt.SeqID, 1, "", 0, 0, gatewayConnID)
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	player, err := svc.QueryPlayerByAccount(ctx, req.Account)
	if err != nil {
		player, err = svc.CreatePlayer(ctx, req.Account, hashPassword(req.Password))
		if err != nil {
			svc.Log.WithTrace(traceID).Errorf("create player failed: %v", err)
			sendLoginRes(conn, pkt.SeqID, 1, "", 0, 0, gatewayConnID)
			return
		}
	}

	token := generateToken(player.PlayerId)
	expireAt := uint64(time.Now().Add(24 * time.Hour).Unix())

	if err := svc.SetToken(ctx, player.PlayerId, token, 24*3600); err != nil {
		svc.Log.WithTrace(traceID).Errorf("set token failed: %v", err)
	}

	sendLoginRes(conn, pkt.SeqID, 0, token, player.PlayerId, expireAt, gatewayConnID)
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

func sendLoginRes(conn *net.TCPConn, seqID uint32, code uint32, token string, playerID uint64, expireAt uint64, gatewayConnID uint64) {
	res := &loginpb.LoginRes{
		Result:   &commonpb.Result{Ok: code == 0, Code: code},
		Token:    token,
		PlayerId: playerID,
		ExpireAt: expireAt,
	}
	SendProto(conn, seqID, cmdLoginRes, res, gatewayConnID)
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

func hashPassword(pwd string) string {
	h := sha256.Sum256([]byte(pwd))
	return hex.EncodeToString(h[:])
}

func generateToken(playerID uint64) string {
	h := sha256.Sum256([]byte(fmt.Sprintf("%d:%d", playerID, time.Now().UnixNano())))
	return hex.EncodeToString(h[:16])
}
