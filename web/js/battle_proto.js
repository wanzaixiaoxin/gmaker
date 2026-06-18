// ============================================================
// battle_proto.js — MOBA 战斗二进制协议编解码
//
// 对齐 server (common/cpp/realtime) 的字节布局，全大端 (big-endian)。
//
// 上行（客户端 -> Gateway -> Realtime）：
//   Gateway 会在 payload 前自动前置 8 字节 conn_id，因此客户端只编码
//   "房间级 payload"（不含 conn_id）。server 读取时偏移 +8 对应 conn_id，
//   +8 之后才是本文件编码的字段。
//
// 下行（Realtime -> Gateway -> 客户端）：
//   Gateway 剥掉 8 字节 conn_id 后，客户端直接收到 SerializeBattle* 的输出。
//   BattleStart/BattleEnd/StateSync 共用 cmd 0x00020005，payload 首字节为
//   msg-type 区分符（见 battle_message.hpp BattleMsgType）。
// ============================================================

// 与 server battle_message.hpp BattleMsgType 严格对齐
const BattleMsgType = {
    BattleStart:     110,
    BattleEnd:       111,
    BattleStateSync: 113,
};

// 与 server battle_types.hpp 对齐
const EntityType = { Invalid: 0, Hero: 1, Minion: 2, Tower: 3, Projectile: 4, Base: 5 };
const TeamSide   = { None: 0, Blue: 1, Red: 2 };
const EntityState= { Idle: 0, Moving: 1, Attacking: 2, Casting: 3, Dead: 4 };
const BattleState= { Waiting: 0, Loading: 1, Countdown: 2, Fighting: 3, Paused: 4, Finished: 5 };

// ---- 大端写入小工具 ----
function writeU32BE(buf, off, v) {
    buf[off]     = (v >>> 24) & 0xFF;
    buf[off + 1] = (v >>> 16) & 0xFF;
    buf[off + 2] = (v >>> 8) & 0xFF;
    buf[off + 3] = v & 0xFF;
}
function writeU64BE(buf, off, v) {
    // v 可能超过 32 位精度上限；用 BigInt 安全处理
    const lo = BigInt(v) & 0xFFFFFFFFn;
    const hi = (BigInt(v) >> 32n) & 0xFFFFFFFFn;
    writeU32BE(buf, off, Number(hi));
    writeU32BE(buf, off + 4, Number(lo));
}
function writeF32BE(buf, off, v) {
    const tmp = new ArrayBuffer(4);
    new DataView(tmp).setFloat32(0, v, false); // big-endian
    const u = new Uint8Array(tmp);
    buf[off]     = u[0];
    buf[off + 1] = u[1];
    buf[off + 2] = u[2];
    buf[off + 3] = u[3];
}

// ---- 大端读取小工具 ----
function readU32BE(buf, off) {
    return ((buf[off] * 0x1000000) + ((buf[off + 1] << 16) | (buf[off + 2] << 8) | buf[off + 3])) >>> 0;
}
function readU64BE(buf, off) {
    const hi = readU32BE(buf, off);
    const lo = readU32BE(buf, off + 4);
    // 返回字符串避免精度丢失（player_id 可超 2^53）
    return String((BigInt(hi) << 32n) | BigInt(lo));
}
function readF32BE(buf, off) {
    const tmp = new ArrayBuffer(4);
    const u = new Uint8Array(tmp);
    u[0] = buf[off]; u[1] = buf[off + 1]; u[2] = buf[off + 2]; u[3] = buf[off + 3];
    return new DataView(tmp).getFloat32(0, false);
}

// ============================================================
// 上行编码（房间级 payload，不含 Gateway 前置的 conn_id）
// ============================================================

// RT_BATTLE_READY (0x00020030)：server main.cpp 读 room@+8, player@+12（min 20）
// 客户端 payload: [room u32][player u64]  = 12 字节
function encodeBattleReady(roomId, playerId) {
    const buf = new Uint8Array(12);
    writeU32BE(buf, 0, Number(roomId) >>> 0);
    writeU64BE(buf, 4, playerId);
    return buf;
}

// RT_BATTLE_MOVE (0x00020031)：server 读 room@+8, player@+12, moveX@+20, moveZ@+24, seq@+28（min 32）
// 客户端 payload: [room u32][player u64][moveX f32][moveZ f32][seq u32] = 24 字节
function encodeBattleMove(roomId, playerId, moveX, moveZ, inputSeq) {
    const buf = new Uint8Array(24);
    writeU32BE(buf, 0, Number(roomId) >>> 0);
    writeU64BE(buf, 4, playerId);
    writeF32BE(buf, 12, moveX);
    writeF32BE(buf, 16, moveZ);
    writeU32BE(buf, 20, inputSeq >>> 0);
    return buf;
}

// RT_BATTLE_CAST (0x00020032)：server 读 room@+8, player@+12, slot@+20(u8),
//   targetX@+21(f32), targetZ@+25(f32), targetEid@+29(u64), seq@+37(u32)（min 41）
// 客户端 payload（刻意非对齐，与 server 一致）：
//   [room u32][player u64][slot u8][targetX f32][targetZ f32][targetEid u64][seq u32] = 33 字节
function encodeBattleCast(roomId, playerId, skillSlot, targetX, targetZ, targetEid, inputSeq) {
    const buf = new Uint8Array(33);
    writeU32BE(buf, 0, Number(roomId) >>> 0);
    writeU64BE(buf, 4, playerId);
    buf[12] = skillSlot & 0xFF;
    writeF32BE(buf, 13, targetX);
    writeF32BE(buf, 17, targetZ);
    writeU64BE(buf, 21, targetEid);
    writeU32BE(buf, 29, inputSeq >>> 0);
    return buf;
}

// RT_ROOM_ENTER_REQ (0x00020000)：server 读 gw_conn@+8, room@+8, player@+12, spawnX@+20, spawnZ@+24（min 28）
// 客户端 payload: [room u32][player u64][spawnX f32][spawnZ f32] = 20 字节
// 注意 spawn float 现在用 BE（与 server ReadF32BE 对齐）
function encodeRoomEnter(roomId, playerId, spawnX, spawnZ) {
    const buf = new Uint8Array(20);
    writeU32BE(buf, 0, Number(roomId) >>> 0);
    writeU64BE(buf, 4, playerId);
    writeF32BE(buf, 12, spawnX);
    writeF32BE(buf, 16, spawnZ);
    return buf;
}

// ============================================================
// 下行解码（payload 首字节为 msg-type 区分符）
// ============================================================

// 统一入口：根据首字节 msg-type 分发
// 返回 { type: 'start'|'end'|'state', ... }
function decodeDownstream(payload) {
    if (!payload || payload.length < 1) return null;
    const msgType = payload[0];
    if (msgType === BattleMsgType.BattleStart)     return { type: 'start',  ...decodeBattleStart(payload) };
    if (msgType === BattleMsgType.BattleEnd)       return { type: 'end',    ...decodeBattleEnd(payload) };
    if (msgType === BattleMsgType.BattleStateSync) return { type: 'state',  ...decodeBattleState(payload) };
    return null; // 未知 msg-type
}

// SerializeBattleStart: [msgType u8][room u32][countdown u32][blueN u32][playerId u64]*[redN u32][playerId u64]*
function decodeBattleStart(buf) {
    let off = 1; // 跳过 msgType
    const roomId = readU32BE(buf, off); off += 4;
    const countdownSec = readU32BE(buf, off); off += 4;
    const blueN = readU32BE(buf, off); off += 4;
    const blue = [];
    for (let i = 0; i < blueN; i++) { blue.push(readU64BE(buf, off)); off += 8; }
    const redN = readU32BE(buf, off); off += 4;
    const red = [];
    for (let i = 0; i < redN; i++) { red.push(readU64BE(buf, off)); off += 8; }
    return { roomId, countdownSec, blue, red };
}

// SerializeBattleEnd: [msgType u8][winner u8][duration u32][blueKills u32][redKills u32]
function decodeBattleEnd(buf) {
    let off = 1;
    const winner = buf[off]; off += 1;
    const durationSec = readU32BE(buf, off); off += 4;
    const blueKills = readU32BE(buf, off); off += 4;
    const redKills = readU32BE(buf, off); off += 4;
    return { winner, durationSec, blueKills, redKills };
}

// SerializeBattleStateSync:
//   [msgType u8][frame u32][ts u64][battleState u8][count u32]
//   每个实体：[eid u64][type u8][team u8][pos.x f32][pos.y f32][pos.z f32][yaw f32][hp u32][maxHp u32][state u8]
//   若 type==Hero：再 [kills u32][deaths u32][gold u32]
//   非英雄记录 35 字节，英雄 47 字节（无长度前缀，按 type 判断）
function decodeBattleState(buf) {
    let off = 1; // 跳过 msgType
    const frame = readU32BE(buf, off); off += 4;
    const timestampMs = readU64BE(buf, off); off += 8;
    const battleState = buf[off]; off += 1;
    const count = readU32BE(buf, off); off += 4;

    const entities = [];
    for (let i = 0; i < count; i++) {
        const eid = readU64BE(buf, off); off += 8;
        const type = buf[off]; off += 1;
        const team = buf[off]; off += 1;
        const x = readF32BE(buf, off); off += 4;
        const y = readF32BE(buf, off); off += 4;
        const z = readF32BE(buf, off); off += 4;
        const yaw = readF32BE(buf, off); off += 4;
        const hp = readU32BE(buf, off); off += 4;
        const maxHp = readU32BE(buf, off); off += 4;
        const state = buf[off]; off += 1;
        const ent = { eid, type, team, x, y, z, yaw, hp, maxHp, state };
        if (type === EntityType.Hero) {
            ent.kills = readU32BE(buf, off); off += 4;
            ent.deaths = readU32BE(buf, off); off += 4;
            ent.gold = readU32BE(buf, off); off += 4;
        }
        entities.push(ent);
    }
    return { frame, timestampMs, battleState, entities };
}

// 暴露
if (typeof window !== 'undefined') {
    window.BattleProto = {
        BattleMsgType, EntityType, TeamSide, EntityState, BattleState,
        encodeBattleReady, encodeBattleMove, encodeBattleCast, encodeRoomEnter,
        decodeDownstream, decodeBattleStart, decodeBattleEnd, decodeBattleState,
    };
}
