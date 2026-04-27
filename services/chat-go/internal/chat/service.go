package chat

import (
	"context"
	"encoding/json"
	"fmt"
	"strconv"
	"sync"
	"time"

	"github.com/gmaker/luffa/common/go/idgen"
	"github.com/gmaker/luffa/common/go/logger"
	"github.com/gmaker/luffa/common/go/net"
	"github.com/gmaker/luffa/common/go/redis"
	"github.com/gmaker/luffa/common/go/rpc"
	goredis "github.com/redis/go-redis/v9"
	"github.com/gmaker/luffa/common/go/trace"
	chatpb "github.com/gmaker/luffa/gen/go/chat"
	commonpb "github.com/gmaker/luffa/gen/go/common"
	dbproxypb "github.com/gmaker/luffa/gen/go/dbproxy"
	protocol "github.com/gmaker/luffa/gen/go/protocol"
	"google.golang.org/protobuf/proto"
)

const (
	CmdChatCreateRoomReq = uint32(protocol.CmdChat_CMD_CHAT_CREATE_ROOM_REQ)
	CmdChatCreateRoomRes = uint32(protocol.CmdChat_CMD_CHAT_CREATE_ROOM_RES)
	CmdChatJoinRoomReq   = uint32(protocol.CmdChat_CMD_CHAT_JOIN_ROOM_REQ)
	CmdChatJoinRoomRes   = uint32(protocol.CmdChat_CMD_CHAT_JOIN_ROOM_RES)
	CmdChatLeaveRoomReq  = uint32(protocol.CmdChat_CMD_CHAT_LEAVE_ROOM_REQ)
	CmdChatLeaveRoomRes  = uint32(protocol.CmdChat_CMD_CHAT_LEAVE_ROOM_RES)
	CmdChatSendMsgReq    = uint32(protocol.CmdChat_CMD_CHAT_SEND_MSG_REQ)
	CmdChatSendMsgRes    = uint32(protocol.CmdChat_CMD_CHAT_SEND_MSG_RES)
	CmdChatMsgNotify     = uint32(protocol.CmdChat_CMD_CHAT_MSG_NOTIFY)
	CmdChatGetHistoryReq = uint32(protocol.CmdChat_CMD_CHAT_GET_HISTORY_REQ)
	CmdChatGetHistoryRes = uint32(protocol.CmdChat_CMD_CHAT_GET_HISTORY_RES)
	CmdChatCloseRoomReq  = uint32(protocol.CmdChat_CMD_CHAT_CLOSE_ROOM_REQ)
	CmdChatCloseRoomRes  = uint32(protocol.CmdChat_CMD_CHAT_CLOSE_ROOM_RES)
	CmdChatListRoomReq   = uint32(protocol.CmdChat_CMD_CHAT_LIST_ROOM_REQ)
	CmdChatListRoomRes   = uint32(protocol.CmdChat_CMD_CHAT_LIST_ROOM_RES)

	CmdMySQLExec     = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC)
	CmdMySQLExecRes  = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_EXEC_RES)
	CmdMySQLQuery    = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY)
	CmdMySQLQueryRes = uint32(protocol.CmdDBProxyInternal_CMD_DB_INT_MYSQL_QUERY_RES)

	CmdGWRoomJoin  = uint32(protocol.CmdGatewayInternal_CMD_GW_ROOM_JOIN)
	CmdGWRoomLeave = uint32(protocol.CmdGatewayInternal_CMD_GW_ROOM_LEAVE)
)

type DBProxyClient interface {
	Connect() error
	Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error)
	OnPacket(pkt *net.Packet)
}

type ChatService struct {
	DB    DBProxyClient
	Redis *redis.Client
	IDGen *idgen.Snowflake
	Log   *logger.Logger
	srv   *net.TCPServer

	// 内存回退：当 Redis 不可用时使用
	memRooms map[uint64]*chatpb.ChatRoomInfo
	memMsgs  map[uint64][]*chatpb.ChatMessage
	memMu    sync.RWMutex
}

func NewChatService(db DBProxyClient, r *redis.Client, idGen *idgen.Snowflake, log *logger.Logger) *ChatService {
	return &ChatService{
		DB: db, Redis: r, IDGen: idGen, Log: log,
		memRooms: make(map[uint64]*chatpb.ChatRoomInfo),
		memMsgs:  make(map[uint64][]*chatpb.ChatMessage),
	}
}

func (s *ChatService) SetServer(srv *net.TCPServer) {
	s.srv = srv
}

func (s *ChatService) Broadcast(pkt *net.Packet) {
	if s.srv != nil {
		s.srv.Broadcast(pkt)
	}
}

func HandlePacket(conn *net.TCPConn, pkt *net.Packet, svc *ChatService, srv *net.TCPServer) {
	ctx, traceID := trace.Ensure(context.Background())
	_ = ctx
	log := svc.Log.WithTrace(traceID)
	svc.Log.Info(fmt.Sprintf("[Flow] Gateway -> Chat: cmd=0x%08X seq=%d flags=%d payload=%d", pkt.CmdID, pkt.SeqID, pkt.Flags, len(pkt.Payload)))
	switch pkt.CmdID {
	case CmdChatCreateRoomReq:
		log.Infof("[%s] create_room req", traceID)
		HandleCreateRoom(conn, pkt, svc, traceID)
	case CmdChatJoinRoomReq:
		log.Infof("[%s] join_room req", traceID)
		HandleJoinRoom(conn, pkt, svc, traceID)
	case CmdChatLeaveRoomReq:
		log.Infof("[%s] leave_room req", traceID)
		HandleLeaveRoom(conn, pkt, svc, traceID)
	case CmdChatSendMsgReq:
		log.Infof("[%s] send_msg req", traceID)
		HandleSendMsg(conn, pkt, svc, traceID)
	case CmdChatGetHistoryReq:
		log.Infof("[%s] get_history req", traceID)
		HandleGetHistory(conn, pkt, svc, traceID)
	case CmdChatCloseRoomReq:
		log.Infof("[%s] close_room req", traceID)
		HandleCloseRoom(conn, pkt, svc, traceID)
	case CmdChatListRoomReq:
		log.Infof("[%s] list_room req", traceID)
		HandleListRooms(conn, pkt, svc, traceID)
	default:
		log.Warnf("unknown cmd_id: 0x%08X", pkt.CmdID)
	}
}

// ==================== Handler ====================

func HandleCreateRoom(conn *net.TCPConn, pkt *net.Packet, svc *ChatService, traceID string) {
	var req chatpb.ChatCreateRoomReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendProto(conn, pkt.SeqID, CmdChatCreateRoomRes, &chatpb.ChatCreateRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 400, Msg: "bad request"},
		})
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	roomID, err := svc.IDGen.NextID()
	if err != nil {
		svc.Log.WithTrace(traceID).Errorf("generate room id failed: %v", err)
		sendProto(conn, pkt.SeqID, CmdChatCreateRoomRes, &chatpb.ChatCreateRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 500, Msg: "internal error"},
		})
		return
	}

	now := uint64(time.Now().Unix())
	if svc.DB != nil {
		sqlStr := "INSERT INTO chat_rooms (room_id, name, creator_id, status, created_at, closed_at) VALUES (?, ?, ?, 0, ?, 0)"
		execReq := &dbproxypb.MySQLExecReq{Uid: uint64(roomID), Sql: sqlStr, Args: []string{
			strconv.FormatInt(roomID, 10), req.Name, strconv.FormatUint(req.CreatorId, 10), strconv.FormatUint(now, 10),
		}}
		data, _ := proto.Marshal(execReq)
		resPkt, err := svc.DB.Call(ctx, CmdMySQLExec, data)
		if err != nil {
			svc.Log.WithTrace(traceID).Errorf("create room failed: %v", err)
			sendProto(conn, pkt.SeqID, CmdChatCreateRoomRes, &chatpb.ChatCreateRoomRes{
				Result: &commonpb.Result{Ok: false, Code: 500, Msg: "db error"},
			})
			return
		}
		var execRes dbproxypb.MySQLExecRes
		if err := proto.Unmarshal(resPkt.Payload, &execRes); err != nil || !execRes.Ok {
			sendProto(conn, pkt.SeqID, CmdChatCreateRoomRes, &chatpb.ChatCreateRoomRes{
				Result: &commonpb.Result{Ok: false, Code: 500, Msg: "db exec failed"},
			})
			return
		}
	}

	// Redis 缓存 / 内存回退
	roomInfo := &chatpb.ChatRoomInfo{
		RoomId:    uint64(roomID),
		Name:      req.Name,
		CreatorId: req.CreatorId,
		Status:    0,
		CreatedAt: now,
	}
	if svc.Redis != nil {
		info, _ := json.Marshal(map[string]interface{}{
			"name":       req.Name,
			"creator_id": req.CreatorId,
			"status":     0,
			"created_at": now,
		})
		svc.Redis.Set(ctx, roomInfoKey(uint64(roomID)), string(info), 0)
		svc.Redis.RawClient().ZAdd(ctx, "chat:rooms:active", goredis.Z{Score: float64(now), Member: strconv.FormatInt(roomID, 10)})
	} else {
		svc.memMu.Lock()
		svc.memRooms[uint64(roomID)] = roomInfo
		svc.memMu.Unlock()
	}

	sendProto(conn, pkt.SeqID, CmdChatCreateRoomRes, &chatpb.ChatCreateRoomRes{
		Result: &commonpb.Result{Ok: true},
		Room: &chatpb.ChatRoomInfo{
			RoomId:    uint64(roomID),
			Name:      req.Name,
			CreatorId: req.CreatorId,
			Status:    0,
			CreatedAt: now,
		},
	})
}

func HandleJoinRoom(conn *net.TCPConn, pkt *net.Packet, svc *ChatService, traceID string) {
	var req chatpb.ChatJoinRoomReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendProto(conn, pkt.SeqID, CmdChatJoinRoomRes, &chatpb.ChatJoinRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 400, Msg: "bad request"},
		})
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	// 检查房间状态
	room, err := svc.GetRoom(ctx, req.RoomId)
	if err != nil {
		sendProto(conn, pkt.SeqID, CmdChatJoinRoomRes, &chatpb.ChatJoinRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 404, Msg: "room not found"},
		})
		return
	}
	if room.Status != 0 {
		sendProto(conn, pkt.SeqID, CmdChatJoinRoomRes, &chatpb.ChatJoinRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 403, Msg: "room closed"},
		})
		return
	}

	// 通知 Gateway：conn_id 加入 room
	connID := conn.ID()
	sendRoomControl(conn, pkt.SeqID, CmdGWRoomJoin, req.RoomId, connID)

	// Redis 记录成员
	if svc.Redis != nil {
		svc.Redis.RawClient().SAdd(ctx, roomMembersKey(req.RoomId), req.PlayerId).Result()
		svc.Redis.RawClient().SAdd(ctx, playerRoomsKey(req.PlayerId), req.RoomId).Result()
	}

	// 获取近期消息
	var recentMsgs []*chatpb.ChatMessage
	if svc.Redis != nil {
		msgs, _ := svc.Redis.RawClient().LRange(ctx, roomMsgsKey(req.RoomId), 0, 49).Result()
		for _, m := range msgs {
			var msg chatpb.ChatMessage
			if json.Unmarshal([]byte(m), &msg) == nil {
				recentMsgs = append(recentMsgs, &msg)
			}
		}
		// 反转顺序（LRANGE 返回的是从 newest 到 oldest，但展示时 oldest 在前更自然）
		for i, j := 0, len(recentMsgs)-1; i < j; i, j = i+1, j-1 {
			recentMsgs[i], recentMsgs[j] = recentMsgs[j], recentMsgs[i]
		}
	} else {
		svc.memMu.RLock()
		list := svc.memMsgs[req.RoomId]
		startIdx := 0
		if len(list) > 50 {
			startIdx = len(list) - 50
		}
		for _, m := range list[startIdx:] {
			recentMsgs = append(recentMsgs, m)
		}
		svc.memMu.RUnlock()
	}

	sendProto(conn, pkt.SeqID, CmdChatJoinRoomRes, &chatpb.ChatJoinRoomRes{
		Result:     &commonpb.Result{Ok: true},
		Room:       room,
		RecentMsgs: recentMsgs,
	})
}

func HandleLeaveRoom(conn *net.TCPConn, pkt *net.Packet, svc *ChatService, traceID string) {
	var req chatpb.ChatLeaveRoomReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendProto(conn, pkt.SeqID, CmdChatLeaveRoomRes, &chatpb.ChatLeaveRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 400, Msg: "bad request"},
		})
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	// 通知 Gateway：conn_id 离开 room
	connID := conn.ID()
	sendRoomControl(conn, pkt.SeqID, CmdGWRoomLeave, req.RoomId, connID)

	if svc.Redis != nil {
		svc.Redis.RawClient().SRem(ctx, roomMembersKey(req.RoomId), req.PlayerId).Result()
		svc.Redis.RawClient().SRem(ctx, playerRoomsKey(req.PlayerId), req.RoomId).Result()
	}

	sendProto(conn, pkt.SeqID, CmdChatLeaveRoomRes, &chatpb.ChatLeaveRoomRes{
		Result: &commonpb.Result{Ok: true},
	})
}

func HandleSendMsg(conn *net.TCPConn, pkt *net.Packet, svc *ChatService, traceID string) {
	var req chatpb.ChatSendMsgReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendProto(conn, pkt.SeqID, CmdChatSendMsgRes, &chatpb.ChatSendMsgRes{
			Result: &commonpb.Result{Ok: false, Code: 400, Msg: "bad request"},
		})
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	// 检查房间状态
	room, err := svc.GetRoom(ctx, req.RoomId)
	if err != nil || room.Status != 0 {
		sendProto(conn, pkt.SeqID, CmdChatSendMsgRes, &chatpb.ChatSendMsgRes{
			Result: &commonpb.Result{Ok: false, Code: 403, Msg: "room not available"},
		})
		return
	}

	msgID, _ := svc.IDGen.NextID()
	now := uint64(time.Now().Unix())
	msg := &chatpb.ChatMessage{
		MsgId:      uint64(msgID),
		RoomId:     req.RoomId,
		SenderId:   req.SenderId,
		SenderName: "", // TODO: 查询玩家昵称
		Content:    req.Content,
		SentAt:     now,
	}

	// Redis 保存消息
	if svc.Redis != nil {
		data, _ := json.Marshal(msg)
		pipe := svc.Redis.RawClient().Pipeline()
		pipe.LPush(ctx, roomMsgsKey(req.RoomId), string(data))
		pipe.LTrim(ctx, roomMsgsKey(req.RoomId), 0, 99)
		pipe.Exec(ctx)
	}

	// 返回响应给发送者
	sendProto(conn, pkt.SeqID, CmdChatSendMsgRes, &chatpb.ChatSendMsgRes{
		Result: &commonpb.Result{Ok: true},
		Msg:    msg,
	})

	// 广播消息给所有 Gateway（通过 FlagRoomBcast）
	broadcastPkt := buildRoomBroadcastPacket(pkt.SeqID, CmdChatMsgNotify, req.RoomId, msg)
	svc.Broadcast(broadcastPkt)
}

func HandleGetHistory(conn *net.TCPConn, pkt *net.Packet, svc *ChatService, traceID string) {
	var req chatpb.ChatGetHistoryReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendProto(conn, pkt.SeqID, CmdChatGetHistoryRes, &chatpb.ChatGetHistoryRes{
			Result: &commonpb.Result{Ok: false, Code: 400, Msg: "bad request"},
		})
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	limit := req.Limit
	if limit == 0 || limit > 100 {
		limit = 50
	}

	var msgs []*chatpb.ChatMessage
	if svc.Redis != nil {
		rawMsgs, _ := svc.Redis.RawClient().LRange(ctx, roomMsgsKey(req.RoomId), 0, int64(limit)-1).Result()
		for _, m := range rawMsgs {
			var msg chatpb.ChatMessage
			if json.Unmarshal([]byte(m), &msg) == nil {
				msgs = append(msgs, &msg)
			}
		}
		// 反转顺序（oldest first）
		for i, j := 0, len(msgs)-1; i < j; i, j = i+1, j-1 {
			msgs[i], msgs[j] = msgs[j], msgs[i]
		}
	} else {
		svc.memMu.RLock()
		list := svc.memMsgs[req.RoomId]
		startIdx := 0
		if len(list) > int(limit) {
			startIdx = len(list) - int(limit)
		}
		for _, m := range list[startIdx:] {
			msgs = append(msgs, m)
		}
		svc.memMu.RUnlock()
	}

	sendProto(conn, pkt.SeqID, CmdChatGetHistoryRes, &chatpb.ChatGetHistoryRes{
		Result: &commonpb.Result{Ok: true},
		Msgs:   msgs,
	})
}

func HandleCloseRoom(conn *net.TCPConn, pkt *net.Packet, svc *ChatService, traceID string) {
	var req chatpb.ChatCloseRoomReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendProto(conn, pkt.SeqID, CmdChatCloseRoomRes, &chatpb.ChatCloseRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 400, Msg: "bad request"},
		})
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	// 检查权限
	room, err := svc.GetRoom(ctx, req.RoomId)
	if err != nil {
		sendProto(conn, pkt.SeqID, CmdChatCloseRoomRes, &chatpb.ChatCloseRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 404, Msg: "room not found"},
		})
		return
	}
	if room.CreatorId != req.OperatorId {
		sendProto(conn, pkt.SeqID, CmdChatCloseRoomRes, &chatpb.ChatCloseRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 403, Msg: "not creator"},
		})
		return
	}

	now := uint64(time.Now().Unix())
	if svc.DB != nil {
		sqlStr := "UPDATE chat_rooms SET status = 1, closed_at = ? WHERE room_id = ?"
		execReq := &dbproxypb.MySQLExecReq{Uid: req.RoomId, Sql: sqlStr, Args: []string{
			strconv.FormatUint(now, 10), strconv.FormatUint(req.RoomId, 10),
		}}
		data, _ := proto.Marshal(execReq)
		resPkt, err := svc.DB.Call(ctx, CmdMySQLExec, data)
		if err != nil {
			sendProto(conn, pkt.SeqID, CmdChatCloseRoomRes, &chatpb.ChatCloseRoomRes{
				Result: &commonpb.Result{Ok: false, Code: 500, Msg: "db error"},
			})
			return
		}
		var execRes dbproxypb.MySQLExecRes
		if err := proto.Unmarshal(resPkt.Payload, &execRes); err != nil || !execRes.Ok {
			sendProto(conn, pkt.SeqID, CmdChatCloseRoomRes, &chatpb.ChatCloseRoomRes{
				Result: &commonpb.Result{Ok: false, Code: 500, Msg: "db exec failed"},
			})
			return
		}
	}

	// 更新 Redis / 内存
	if svc.Redis != nil {
		info, _ := json.Marshal(map[string]interface{}{
			"name":       room.Name,
			"creator_id": room.CreatorId,
			"status":     1,
			"created_at": room.CreatedAt,
			"closed_at":  now,
		})
		svc.Redis.Set(ctx, roomInfoKey(req.RoomId), string(info), 0)
		svc.Redis.RawClient().Del(ctx, roomMembersKey(req.RoomId)).Result()
		svc.Redis.RawClient().ZRem(ctx, "chat:rooms:active", strconv.FormatUint(req.RoomId, 10)).Result()
	}
	svc.memMu.Lock()
	if r, ok := svc.memRooms[req.RoomId]; ok {
		r.Status = 1
		r.ClosedAt = now
	}
	svc.memMu.Unlock()

	// 通知 Gateway 清理该房间的所有成员（发送 GW_ROOM_LEAVE 给所有成员对应的 Gateway）
	// 简化：只给当前 Gateway 发送（实际多 Gateway 部署时需要在所有 Gateway 上执行）
	connID := conn.ID()
	sendRoomControl(conn, pkt.SeqID, CmdGWRoomLeave, req.RoomId, connID)

	sendProto(conn, pkt.SeqID, CmdChatCloseRoomRes, &chatpb.ChatCloseRoomRes{
		Result: &commonpb.Result{Ok: true},
	})
}

func HandleListRooms(conn *net.TCPConn, pkt *net.Packet, svc *ChatService, traceID string) {
	var req chatpb.ChatListRoomReq
	if err := proto.Unmarshal(pkt.Payload, &req); err != nil {
		sendProto(conn, pkt.SeqID, CmdChatListRoomRes, &chatpb.ChatListRoomRes{
			Result: &commonpb.Result{Ok: false, Code: 400, Msg: "bad request"},
		})
		return
	}

	ctx := trace.WithContext(context.Background(), traceID)
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()

	page := req.Page
	if page == 0 {
		page = 1
	}
	limit := req.Limit
	if limit == 0 || limit > 100 {
		limit = 20
	}
	offset := (page - 1) * limit

	var rooms []*chatpb.ChatRoomInfo
	var total uint32

	if svc.DB != nil {
		// 先查总数
		countSql := "SELECT COUNT(*) AS cnt FROM chat_rooms WHERE status = 0"
		countReq := &dbproxypb.MySQLQueryReq{Uid: 1, Sql: countSql}
		countData, _ := proto.Marshal(countReq)
		countResPkt, err := svc.DB.Call(ctx, CmdMySQLQuery, countData)
		if err == nil && countResPkt != nil {
			var countRes dbproxypb.MySQLQueryRes
			if proto.Unmarshal(countResPkt.Payload, &countRes) == nil && countRes.Ok && len(countRes.Rows) > 0 {
				for _, col := range countRes.Rows[0].Columns {
					if col.Name == "cnt" {
						cnt, _ := strconv.ParseUint(col.Value, 10, 32)
						total = uint32(cnt)
						break
					}
				}
			}
		}

		// 查列表
		listSql := "SELECT room_id, name, creator_id, status, created_at, closed_at FROM chat_rooms WHERE status = 0 ORDER BY created_at DESC LIMIT ? OFFSET ?"
		listReq := &dbproxypb.MySQLQueryReq{Uid: 1, Sql: listSql, Args: []string{strconv.FormatUint(uint64(limit), 10), strconv.FormatUint(uint64(offset), 10)}}
		listData, _ := proto.Marshal(listReq)
		listResPkt, err := svc.DB.Call(ctx, CmdMySQLQuery, listData)
		if err == nil && listResPkt != nil {
			var listRes dbproxypb.MySQLQueryRes
			if proto.Unmarshal(listResPkt.Payload, &listRes) == nil && listRes.Ok {
				for _, row := range listRes.Rows {
					rooms = append(rooms, parseRoomRow(row))
				}
			}
		}
	} else if svc.Redis != nil {
		// Redis 模式：从有序集合 chat:rooms:active 获取
		roomIDs, _ := svc.Redis.RawClient().ZRevRange(ctx, "chat:rooms:active", int64(offset), int64(offset+limit)-1).Result()
		for _, ridStr := range roomIDs {
			rid, _ := strconv.ParseUint(ridStr, 10, 64)
			if room, err := svc.GetRoom(ctx, rid); err == nil && room.Status == 0 {
				rooms = append(rooms, room)
			}
		}
		// 总数
		cnt, _ := svc.Redis.RawClient().ZCard(ctx, "chat:rooms:active").Result()
		total = uint32(cnt)
	} else {
		// 内存回退
		svc.memMu.RLock()
		var all []*chatpb.ChatRoomInfo
		for _, r := range svc.memRooms {
			if r.Status == 0 {
				all = append(all, r)
			}
		}
		svc.memMu.RUnlock()
		// 简单按 created_at 降序
		for i := 0; i < len(all)-1; i++ {
			for j := i + 1; j < len(all); j++ {
				if all[i].CreatedAt < all[j].CreatedAt {
					all[i], all[j] = all[j], all[i]
				}
			}
		}
		total = uint32(len(all))
		start := offset
		if start > uint32(len(all)) {
			start = uint32(len(all))
		}
		end := start + limit
		if end > uint32(len(all)) {
			end = uint32(len(all))
		}
		rooms = all[start:end]
	}

	sendProto(conn, pkt.SeqID, CmdChatListRoomRes, &chatpb.ChatListRoomRes{
		Result: &commonpb.Result{Ok: true},
		Rooms:  rooms,
		Total:  total,
	})
}

// ==================== Repository ====================

func (s *ChatService) GetRoom(ctx context.Context, roomID uint64) (*chatpb.ChatRoomInfo, error) {
	// 先查 Redis
	if s.Redis != nil {
		data, err := s.Redis.Get(ctx, roomInfoKey(roomID))
		if err == nil && data != "" {
			var info map[string]interface{}
			if json.Unmarshal([]byte(data), &info) == nil {
				room := &chatpb.ChatRoomInfo{RoomId: roomID}
				if v, ok := info["name"].(string); ok {
					room.Name = v
				}
				if v, ok := info["creator_id"].(float64); ok {
					room.CreatorId = uint64(v)
				}
				if v, ok := info["status"].(float64); ok {
					room.Status = uint32(v)
				}
				if v, ok := info["created_at"].(float64); ok {
					room.CreatedAt = uint64(v)
				}
				if v, ok := info["closed_at"].(float64); ok {
					room.ClosedAt = uint64(v)
				}
				return room, nil
			}
		}
	}

	// 查 MySQL
	if s.DB != nil {
		sqlStr := "SELECT room_id, name, creator_id, status, created_at, closed_at FROM chat_rooms WHERE room_id = ?"
		req := &dbproxypb.MySQLQueryReq{Uid: roomID, Sql: sqlStr, Args: []string{strconv.FormatUint(roomID, 10)}}
		data, _ := proto.Marshal(req)
		resPkt, err := s.DB.Call(ctx, CmdMySQLQuery, data)
		if err != nil {
			return nil, err
		}
		var res dbproxypb.MySQLQueryRes
		if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
			return nil, err
		}
		if !res.Ok || len(res.Rows) == 0 {
			return nil, fmt.Errorf("room not found")
		}
		return parseRoomRow(res.Rows[0]), nil
	}
	// 内存回退
	s.memMu.RLock()
	if r, ok := s.memRooms[roomID]; ok {
		s.memMu.RUnlock()
		return r, nil
	}
	s.memMu.RUnlock()
	return nil, fmt.Errorf("room not found")
}

func parseRoomRow(row *dbproxypb.MySQLRow) *chatpb.ChatRoomInfo {
	m := make(map[string]string)
	for _, col := range row.Columns {
		m[col.Name] = col.Value
	}
	roomID, _ := strconv.ParseUint(m["room_id"], 10, 64)
	creatorID, _ := strconv.ParseUint(m["creator_id"], 10, 64)
	status, _ := strconv.ParseUint(m["status"], 10, 32)
	createdAt, _ := strconv.ParseUint(m["created_at"], 10, 64)
	closedAt, _ := strconv.ParseUint(m["closed_at"], 10, 64)
	return &chatpb.ChatRoomInfo{
		RoomId:    roomID,
		Name:      m["name"],
		CreatorId: creatorID,
		Status:    uint32(status),
		CreatedAt: createdAt,
		ClosedAt:  closedAt,
	}
}

func roomInfoKey(roomID uint64) string {
	return fmt.Sprintf("chat:room:%d:info", roomID)
}

func roomMsgsKey(roomID uint64) string {
	return fmt.Sprintf("chat:room:%d:msgs", roomID)
}

func roomMembersKey(roomID uint64) string {
	return fmt.Sprintf("chat:room:%d:members", roomID)
}

func playerRoomsKey(playerID uint64) string {
	return fmt.Sprintf("player:%d:rooms", playerID)
}

// ==================== Helper ====================

func sendProto(conn *net.TCPConn, seqID uint32, cmdID uint32, msg proto.Message) {
	data, err := proto.Marshal(msg)
	if err != nil {
		logger.Errorf("marshal error: %v", err)
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

func sendRoomControl(conn *net.TCPConn, seqID uint32, cmdID uint32, roomID uint64, connID uint64) {
	payload := make([]byte, 16)
	for i := 0; i < 8; i++ {
		payload[i] = byte(roomID >> (56 - i*8))
		payload[8+i] = byte(connID >> (56 - i*8))
	}
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

func sendRoomBroadcast(conn *net.TCPConn, seqID uint32, cmdID uint32, roomID uint64, msg proto.Message) {
	pkt := buildRoomBroadcastPacket(seqID, cmdID, roomID, msg)
	if pkt != nil {
		conn.SendPacket(pkt)
	}
}

func buildRoomBroadcastPacket(seqID uint32, cmdID uint32, roomID uint64, msg proto.Message) *net.Packet {
	data, err := proto.Marshal(msg)
	if err != nil {
		logger.Errorf("marshal error: %v", err)
		return nil
	}
	payload := make([]byte, 8+len(data))
	for i := 0; i < 8; i++ {
		payload[i] = byte(roomID >> (56 - i*8))
	}
	copy(payload[8:], data)
	return &net.Packet{
		Header: net.Header{
			Magic:  net.MagicValue,
			CmdID:  cmdID,
			SeqID:  seqID,
			Flags:  uint32(net.FlagRPCRes | net.FlagRoomBcast),
			Length: uint32(net.HeaderSize + len(payload)),
		},
		Payload: payload,
	}
}

// SendProto 是导出的辅助函数，供 main.go 使用
func SendProto(conn *net.TCPConn, seqID uint32, cmdID uint32, msg proto.Message) {
	sendProto(conn, seqID, cmdID, msg)
}

// NewDBProxyClient 创建 DBProxy 客户端（兼容旧接口：传入地址列表自建 pool）
func NewDBProxyClient(addrs []string) *dbProxyClient {
	return &dbProxyClient{addrs: addrs}
}

// NewDBProxyClientWithPool 使用外部 pool 创建 DBProxy 客户端（Registry 发现模式）
func NewDBProxyClientWithPool(pool *net.UpstreamPool) *dbProxyClient {
	c := &dbProxyClient{pool: pool}
	c.rpc = rpc.NewClientWithPool(pool)
	pool.SetOnData(func(_ *net.TCPConn, pkt *net.Packet) {
		if c.rpc != nil {
			c.rpc.OnPacket(pkt)
		}
	})
	return c
}

type dbProxyClient struct {
	addrs []string
	pool  *net.UpstreamPool
	rpc   *rpc.Client
}

func (c *dbProxyClient) OnPacket(pkt *net.Packet) {
	if c.rpc != nil {
		c.rpc.OnPacket(pkt)
	}
}

func (c *dbProxyClient) Connect() error {
	if c.pool != nil {
		// 外部 pool 模式：rpc 已在构造时创建
		return nil
	}
	c.pool = net.NewUpstreamPool(func(_ *net.TCPConn, pkt *net.Packet) {
		if c.rpc != nil {
			c.rpc.OnPacket(pkt)
		}
	})
	for _, addr := range c.addrs {
		c.pool.AddNode(addr)
	}
	c.pool.Start()
	c.rpc = rpc.NewClientWithPool(c.pool)
	return nil
}

func (c *dbProxyClient) Close() {
	if c.pool != nil && len(c.addrs) > 0 {
		// 仅自建 pool 时才停止
		c.pool.Stop()
	}
}

func (c *dbProxyClient) Call(ctx context.Context, cmdID uint32, payload []byte) (*net.Packet, error) {
	return c.rpc.Call(ctx, cmdID, payload)
}

// EnsureChatRoomTable 确保 chat_rooms 表存在
func EnsureChatRoomTable(svc *ChatService) error {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	sqlStr := `CREATE TABLE IF NOT EXISTS chat_rooms (
		room_id     BIGINT PRIMARY KEY,
		name        VARCHAR(128) NOT NULL,
		creator_id  BIGINT NOT NULL,
		status      TINYINT DEFAULT 0,
		created_at  BIGINT DEFAULT 0,
		closed_at   BIGINT DEFAULT 0
	)`
	req := &dbproxypb.MySQLExecReq{Uid: 1, Sql: sqlStr}
	data, _ := proto.Marshal(req)
	resPkt, err := svc.DB.Call(ctx, CmdMySQLExec, data)
	if err != nil {
		return err
	}
	var res dbproxypb.MySQLExecRes
	if err := proto.Unmarshal(resPkt.Payload, &res); err != nil {
		return err
	}
	if !res.Ok {
		return fmt.Errorf("create table failed: %s", res.Error)
	}
	return nil
}
