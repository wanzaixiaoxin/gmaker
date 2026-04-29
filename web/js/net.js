// ==================== Packet Codec ====================
const HEADER_SIZE = 18;
const MAGIC_VALUE = 0x9D7F;

const Flag = {
    ENCRYPT: 1 << 0,
    COMPRESS: 1 << 1,
    BROADCAST: 1 << 2,
    TRACE: 1 << 3,
    RPC_REQ: 1 << 4,
    RPC_RES: 1 << 5,
    RPC_FF: 1 << 6,
    HEARTBEAT: 1 << 7,
    ROOM_BCAST: 1 << 8,
};

class PacketCodec {
    static encode(cmdID, seqID, flags, payload) {
        const len = HEADER_SIZE + (payload ? payload.length : 0);
        const buf = new ArrayBuffer(len);
        const dv = new DataView(buf);
        let off = 0;
        dv.setUint32(off, len, false); off += 4;
        dv.setUint16(off, MAGIC_VALUE, false); off += 2;
        dv.setUint32(off, cmdID, false); off += 4;
        dv.setUint32(off, seqID, false); off += 4;
        dv.setUint32(off, flags, false); off += 4;
        if (payload && payload.length > 0) {
            new Uint8Array(buf, off).set(payload);
        }
        return new Uint8Array(buf);
    }

    static decode(data) {
        if (data.length < HEADER_SIZE) return null;
        const dv = new DataView(data.buffer, data.byteOffset, HEADER_SIZE);
        let off = 0;
        const len = dv.getUint32(off, false); off += 4;
        const magic = dv.getUint16(off, false); off += 2;
        const cmdID = dv.getUint32(off, false); off += 4;
        const seqID = dv.getUint32(off, false); off += 4;
        const flags = dv.getUint32(off, false); off += 4;
        if (magic !== MAGIC_VALUE || len !== data.length) return null;
        const payload = new Uint8Array(data.buffer, data.byteOffset + HEADER_SIZE, data.length - HEADER_SIZE);
        return { length: len, magic, cmdID, seqID, flags, payload };
    }
}

// ==================== Crypto ====================
class Crypto {
    constructor() {
        this.sessionKey = null;
    }

    setSessionKey(key) {
        this.sessionKey = key;
    }

    async encrypt(data) {
        if (!this.sessionKey) return data;
        const iv = crypto.getRandomValues(new Uint8Array(12));
        const encrypted = await crypto.subtle.encrypt(
            { name: 'AES-GCM', iv },
            this.sessionKey,
            data
        );
        const out = new Uint8Array(12 + encrypted.byteLength);
        out.set(iv, 0);
        out.set(new Uint8Array(encrypted), 12);
        return out;
    }

    async decrypt(data) {
        if (!this.sessionKey) return data;
        if (data.length < 12 + 16) throw new Error('ciphertext too short');
        const iv = data.slice(0, 12);
        const cipher = data.slice(12);
        return new Uint8Array(await crypto.subtle.decrypt(
            { name: 'AES-GCM', iv },
            this.sessionKey,
            cipher
        ));
    }

    static async deriveSessionKey(masterKey, clientRandom, serverRandom) {
        const label = new TextEncoder().encode('gmaker-session-v1');
        const data = new Uint8Array(label.length + clientRandom.length + serverRandom.length);
        data.set(label, 0);
        data.set(clientRandom, label.length);
        data.set(serverRandom, label.length + clientRandom.length);

        const cryptoKey = await crypto.subtle.importKey(
            'raw', masterKey, { name: 'HMAC', hash: 'SHA-256' }, false, ['sign']
        );
        const sig = await crypto.subtle.sign('HMAC', cryptoKey, data);
        return new Uint8Array(sig);
    }

    static async importAESKey(rawKey) {
        return await crypto.subtle.importKey(
            'raw', rawKey, { name: 'AES-GCM' }, false, ['encrypt', 'decrypt']
        );
    }
}

// ==================== WSClient ====================
class WSClient {
    constructor(url, masterKey) {
        this.url = url;
        this.masterKey = masterKey || new Uint8Array(32);
        this.ws = null;
        this.seqID = 0;
        this.pending = new Map();
        this.crypto = new Crypto();
        this.onPacket = null;
        this.onClose = null;
        this.onKick = null;
        this.connected = false;
    }

    async connect() {
        return new Promise((resolve, reject) => {
            this.ws = new WebSocket(this.url);
            this.ws.binaryType = 'arraybuffer';

            this.ws.onopen = async () => {
                try {
                    await this.doHandshake();
                    this.connected = true;
                    resolve();
                } catch (e) {
                    reject(e);
                }
            };

            this.ws.onmessage = async (ev) => {
                const data = new Uint8Array(ev.data);
                const pkt = PacketCodec.decode(data);
                if (!pkt) return;
                try {
                    const payload = await this.crypto.decrypt(pkt.payload);
                    pkt.payload = payload;
                    // 处理服务器主动推送的踢人通知
                    if (pkt.cmdID === Cmd.GW_PLAYER_KICK) {
                        if (this.onKick) this.onKick(pkt.payload);
                        this.close();
                        return;
                    }
                    if (pkt.cmdID === Cmd.SYS_ERROR_PACKET) {
                        let errMsg = 'server error';
                        if (window.root) {
                            try {
                                const Result = window.root.lookupType('common.Result');
                                const result = Result.decode(pkt.payload);
                                errMsg = result.msg || ('server error: ' + result.code);
                            } catch (e) {
                                console.error('decode error packet failed', e);
                            }
                        }
                        if (pkt.seqID !== 0 && this.pending.has(pkt.seqID)) {
                            this.pending.get(pkt.seqID)(pkt, new Error(errMsg));
                            this.pending.delete(pkt.seqID);
                        } else if (this.onClose) {
                            this.onClose(new Error(errMsg));
                        }
                        return;
                    }
                    if (pkt.seqID !== 0 && this.pending.has(pkt.seqID)) {
                        this.pending.get(pkt.seqID)(pkt, null);
                        this.pending.delete(pkt.seqID);
                    } else if (this.onPacket) {
                        this.onPacket(pkt);
                    }
                } catch (e) {
                    console.error('decrypt error', e);
                }
            };

            this.ws.onclose = () => {
                this.connected = false;
                if (this.onClose) this.onClose();
            };

            this.ws.onerror = (e) => {
                reject(new Error('websocket error'));
            };
        });
    }

    async doHandshake() {
        const clientRandom = crypto.getRandomValues(new Uint8Array(16));
        const nonce = crypto.getRandomValues(new Uint8Array(8));
        const timestamp = Math.floor(Date.now() / 1000);

        const payload = new Uint8Array(1 + 8 + 8 + 16);
        payload[0] = 1;
        const dv = new DataView(payload.buffer);
        dv.setBigUint64(1, BigInt(timestamp), false);
        payload.set(nonce, 9);
        payload.set(clientRandom, 17);

        const pkt = PacketCodec.encode(0x00000002, 1, 0, payload);
        this.ws.send(pkt);

        const res = await this.waitHandshakeResponse();
        if (res.payload.length < 1 + 16) throw new Error('handshake response too short');
        if (res.payload[0] !== 1) throw new Error('unsupported handshake version');

        const serverRandom = res.payload.slice(1, 17);
        const encryptedChallenge = res.payload.slice(17);

        const sessionKeyRaw = await Crypto.deriveSessionKey(this.masterKey, clientRandom, serverRandom);
        const aesKey = await Crypto.importAESKey(sessionKeyRaw);
        this.crypto.setSessionKey(aesKey);

        const challenge = await this.crypto.decrypt(encryptedChallenge);
        if (!this.arrayEqual(challenge, clientRandom)) {
            throw new Error('handshake challenge mismatch');
        }
    }

    waitHandshakeResponse() {
        return new Promise((resolve, reject) => {
            const handler = (ev) => {
                const data = new Uint8Array(ev.data);
                const pkt = PacketCodec.decode(data);
                if (pkt && pkt.cmdID === 0x00000002) {
                    this.ws.removeEventListener('message', handler);
                    resolve(pkt);
                }
            };
            this.ws.addEventListener('message', handler);
            setTimeout(() => {
                this.ws.removeEventListener('message', handler);
                reject(new Error('handshake timeout'));
            }, 5000);
        });
    }

    async call(cmdID, payload, timeout = 5000) {
        if (!this.connected) throw new Error('not connected');
        this.seqID = (this.seqID + 1) & 0xFFFFFFFF;
        const seq = this.seqID;

        const encrypted = await this.crypto.encrypt(payload);
        const pkt = PacketCodec.encode(cmdID, seq, Flag.RPC_REQ, encrypted);

        return new Promise((resolve, reject) => {
            const timer = setTimeout(() => {
                this.pending.delete(seq);
                reject(new Error('request timeout'));
            }, timeout);

            this.pending.set(seq, (res, err) => {
                clearTimeout(timer);
                if (err) {
                    reject(err);
                    return;
                }
                resolve(res);
            });

            this.ws.send(pkt);
        });
    }

    close() {
        if (this.ws) {
            this.ws.close();
            this.ws = null;
        }
        this.connected = false;
    }

    arrayEqual(a, b) {
        if (a.length !== b.length) return false;
        for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
        return true;
    }
}

// ==================== CmdIDs ====================
const Cmd = {
    SYS_HANDSHAKE: 0x00000002,
    SYS_ERROR_PACKET: 0x00000003,
    CMN_LOGIN_REQ: 0x00001000,
    CMN_LOGIN_RES: 0x00001001,
    CMN_REGISTER_REQ: 0x00001002,
    CMN_REGISTER_RES: 0x00001003,
    GW_PLAYER_BIND: 0x00001020,
    GW_PLAYER_KICK: 0x00001021,
    BIZ_GET_PLAYER_REQ: 0x00010000,
    BIZ_GET_PLAYER_RES: 0x00010001,
    BIZ_PING: 0x00010004,
    BIZ_PONG: 0x00010005,
    CHAT_CREATE_ROOM_REQ: 0x00030000,
    CHAT_CREATE_ROOM_RES: 0x00030001,
    CHAT_JOIN_ROOM_REQ: 0x00030002,
    CHAT_JOIN_ROOM_RES: 0x00030003,
    CHAT_LEAVE_ROOM_REQ: 0x00030004,
    CHAT_LEAVE_ROOM_RES: 0x00030005,
    CHAT_SEND_MSG_REQ: 0x00030006,
    CHAT_SEND_MSG_RES: 0x00030007,
    CHAT_MSG_NOTIFY: 0x00030008,
    CHAT_GET_HISTORY_REQ: 0x00030009,
    CHAT_GET_HISTORY_RES: 0x0003000A,
    CHAT_CLOSE_ROOM_REQ: 0x0003000B,
    CHAT_CLOSE_ROOM_RES: 0x0003000C,
    CHAT_LIST_ROOM_REQ: 0x0003000D,
    CHAT_LIST_ROOM_RES: 0x0003000E,
};
