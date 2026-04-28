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
        this.ws.onPacket = (pkt) => {
            if (pkt.cmdID === Cmd.CHAT_MSG_NOTIFY) {
                const NotifyType = this._lookup('chat.ChatMsgNotify');
                const msg = NotifyType.decode(pkt.payload);
                callback(msg);
            }
        };
    }
}
