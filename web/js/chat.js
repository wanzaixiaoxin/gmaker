class ChatAPI {
    constructor(wsClient, root) {
        this.ws = wsClient;
        this.root = root;
    }

    _lookup(typeName) {
        return this.root.lookupType(typeName);
    }

    async _call(cmdReq, cmdRes, reqTypeName, resTypeName, reqObj) {
        const ReqType = this._lookup(reqTypeName);
        const payload = ReqType.encode(ReqType.create(reqObj)).finish();
        const res = await this.ws.call(cmdReq, payload);
        const ResType = this._lookup(resTypeName);
        return ResType.decode(res.payload);
    }

    async login(account, password) {
        return await this._call(
            Cmd.CMN_LOGIN_REQ, Cmd.CMN_LOGIN_RES,
            'login.LoginReq', 'login.LoginRes',
            { account, password, platform: 'web', version: '1.0.0' }
        );
    }

    async register(account, password) {
        return await this._call(
            Cmd.CMN_REGISTER_REQ, Cmd.CMN_REGISTER_RES,
            'login.RegisterReq', 'login.RegisterRes',
            { account, password, platform: 'web' }
        );
    }

    async listRooms(page = 1, limit = 20) {
        return await this._call(
            Cmd.CHAT_LIST_ROOM_REQ, Cmd.CHAT_LIST_ROOM_RES,
            'chat.ChatListRoomReq', 'chat.ChatListRoomRes',
            { page, limit }
        );
    }

    async createRoom(name, creatorId) {
        return await this._call(
            Cmd.CHAT_CREATE_ROOM_REQ, Cmd.CHAT_CREATE_ROOM_RES,
            'chat.ChatCreateRoomReq', 'chat.ChatCreateRoomRes',
            { name, creatorId }
        );
    }

    async joinRoom(roomId, playerId) {
        return await this._call(
            Cmd.CHAT_JOIN_ROOM_REQ, Cmd.CHAT_JOIN_ROOM_RES,
            'chat.ChatJoinRoomReq', 'chat.ChatJoinRoomRes',
            { roomId, playerId }
        );
    }

    async leaveRoom(roomId, playerId) {
        return await this._call(
            Cmd.CHAT_LEAVE_ROOM_REQ, Cmd.CHAT_LEAVE_ROOM_RES,
            'chat.ChatLeaveRoomReq', 'chat.ChatLeaveRoomRes',
            { roomId, playerId }
        );
    }

    async sendMsg(roomId, senderId, content, senderName) {
        return await this._call(
            Cmd.CHAT_SEND_MSG_REQ, Cmd.CHAT_SEND_MSG_RES,
            'chat.ChatSendMsgReq', 'chat.ChatSendMsgRes',
            { roomId, senderId, content, senderName }
        );
    }

    async getHistory(roomId, limit = 50) {
        return await this._call(
            Cmd.CHAT_GET_HISTORY_REQ, Cmd.CHAT_GET_HISTORY_RES,
            'chat.ChatGetHistoryReq', 'chat.ChatGetHistoryRes',
            { roomId, limit }
        );
    }

    async closeRoom(roomId, operatorId) {
        return await this._call(
            Cmd.CHAT_CLOSE_ROOM_REQ, Cmd.CHAT_CLOSE_ROOM_RES,
            'chat.ChatCloseRoomReq', 'chat.ChatCloseRoomRes',
            { roomId, operatorId }
        );
    }

    onNotify(callback) {
        const handler = (pkt) => {
            if (pkt.cmdID === Cmd.CHAT_MSG_NOTIFY) {
                const NotifyType = this._lookup('chat.ChatMsgNotify');
                const msg = NotifyType.decode(pkt.payload);
                callback(msg);
            }
        };
        if (this.ws.addPacketHandler) {
            return this.ws.addPacketHandler(handler);
        }
        this.ws.onPacket = handler;
        return () => {
            if (this.ws.onPacket === handler) this.ws.onPacket = null;
        };
    }
}

class MatchAPI {
    constructor(wsClient, root) {
        this.ws = wsClient;
        this.root = root;
    }

    _lookup(typeName) {
        return this.root.lookupType(typeName);
    }

    async _call(cmdReq, reqTypeName, resTypeName, reqObj, timeout = 5000) {
        const ReqType = this._lookup(reqTypeName);
        const payload = ReqType.encode(ReqType.create(reqObj)).finish();
        const res = await this.ws.call(cmdReq, payload, timeout);
        const ResType = this._lookup(resTypeName);
        return ResType.decode(res.payload);
    }

    async enqueue(playerId, mmr = 1000, mode = 3, teamSize = 1) {
        return await this._call(
            Cmd.MATCH_REQ,
            'match.MatchReq',
            'match.MatchRes',
            { playerId, mmr, mode, teamSize }
        );
    }

    async cancel(playerId) {
        return await this._call(
            Cmd.MATCH_CANCEL_REQ,
            'match.MatchCancelReq',
            'match.MatchCancelRes',
            { playerId }
        );
    }

    async status(playerId) {
        return await this._call(
            Cmd.MATCH_STATUS_REQ,
            'match.MatchStatusReq',
            'match.MatchStatusRes',
            { playerId }
        );
    }
}

class RealtimeAPI {
    constructor(wsClient) {
        this.ws = wsClient;
    }

    async enterRoom(roomId, playerId, spawnX = 0, spawnY = 0) {
        // spawn float 现用大端，与 server ReadF32BE 对齐（原 LE 是巧合通）
        const payload = BattleProto.encodeRoomEnter(roomId, playerId, spawnX, spawnY);
        return await this.ws.call(Cmd.RT_ROOM_ENTER_REQ, payload, 5000);
    }

    async leaveRoom(roomId, playerId) {
        const payload = BattleProto.encodeRoomLeave(roomId, playerId);
        return await this.ws.call(Cmd.RT_ROOM_LEAVE_REQ, payload, 3000);
    }

    // ===== MOBA 二进制战斗输入（替代旧的 JSON sendInput）=====

    // 玩家加载完成（RT_BATTLE_READY）
    sendBattleReady(roomId, playerId) {
        const payload = BattleProto.encodeBattleReady(roomId, playerId);
        return this.ws.send(Cmd.RT_BATTLE_READY, payload).catch(() => null);
    }

    // 英雄移动输入（RT_BATTLE_MOVE）：moveX/moveZ 为方向向量
    sendBattleMove(roomId, playerId, moveX, moveZ, inputSeq) {
        const payload = BattleProto.encodeBattleMove(roomId, playerId, moveX, moveZ, inputSeq);
        return this.ws.send(Cmd.RT_BATTLE_MOVE, payload).catch(() => null);
    }

    // 释放技能（RT_BATTLE_CAST）
    sendBattleCast(roomId, playerId, skillSlot, targetX, targetZ, targetEid, inputSeq) {
        const payload = BattleProto.encodeBattleCast(roomId, playerId, skillSlot, targetX, targetZ, targetEid, inputSeq);
        return this.ws.send(Cmd.RT_BATTLE_CAST, payload).catch(() => null);
    }
}
