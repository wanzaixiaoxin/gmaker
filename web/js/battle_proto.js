// ============================================================
// battle_proto.js — MOBA 战斗协议编解码（protobuf）
//
// 替代之前的手写大端二进制，改用 protobufjs。
// schema 定义在 web/index.html 的 PROTO_DESCRIPTOR（battle 包），
// 对应 spec/proto/battle.proto（三端单一 schema 源）。
//
// 对外保持与旧手写版本相同的 BattleProto 命名空间与函数签名，
// chat.js / moba.js 调用方无需改动。
//
// 上行：encode* 返回 Uint8Array（protobuf 字节），由 WSClient.send 带出。
// 下行：decodeDownstream 解析 BattleDownstream oneof，按 type 分发。
// ============================================================

(function () {
    let _root = null;

    // 初始化：注入 protobuf Root（由 index.html 在 Root.fromJSON 后调用）。
    // 若未调用，回退到 window.root。
    function init(root) {
        _root = root || (typeof window !== 'undefined' ? window.root : null);
    }

    function root() {
        if (_root) return _root;
        if (typeof window !== 'undefined' && window.root) { _root = window.root; return _root; }
        throw new Error('BattleProto not initialized: call BattleProto.init(root)');
    }

    function lookup(name) {
        return root().lookupType('battle.' + name);
    }

    // 编码辅助：create + encode + finish → Uint8Array
    function encode(typeName, obj) {
        const T = lookup(typeName);
        const msg = T.create(obj);
        return T.encode(msg).finish();
    }

    // ============================================================
    // 上行编码（返回 protobuf 字节，不含 Gateway 前置的 8 字节 conn_id）
    // ============================================================

    function encodeBattleReady(roomId, playerId) {
        return encode('BattleReadyReq', {
            roomId: Number(roomId) >>> 0,
            playerId: playerId,
        });
    }

    function encodeBattleMove(roomId, playerId, moveX, moveZ, inputSeq) {
        return encode('HeroMoveInput', {
            roomId: Number(roomId) >>> 0,
            playerId: playerId,
            moveX: moveX,
            moveZ: moveZ,
            inputSeq: inputSeq >>> 0,
        });
    }

    function encodeBattleCast(roomId, playerId, skillSlot, targetX, targetZ, targetEid, inputSeq) {
        return encode('HeroCastSkill', {
            roomId: Number(roomId) >>> 0,
            playerId: playerId,
            skillSlot: skillSlot >>> 0,
            targetX: targetX,
            targetZ: targetZ,
            targetEntityId: targetEid,
            inputSeq: inputSeq >>> 0,
        });
    }

    function encodeRoomEnter(roomId, playerId, spawnX, spawnZ) {
        return encode('RoomEnterReq', {
            roomId: Number(roomId) >>> 0,
            playerId: playerId,
            spawnX: spawnX,
            spawnZ: spawnZ,
        });
    }

    function encodeRoomLeave(roomId, playerId) {
        return encode('RoomLeaveReq', {
            roomId: Number(roomId) >>> 0,
            playerId: playerId,
        });
    }

    // ============================================================
    // 下行解码：BattleDownstream oneof
    // 返回 { type: 'start'|'end'|'state', ...字段 }
    // ============================================================

    function decodeDownstream(payload) {
        if (!payload || payload.length === 0) return null;
        const T = lookup('BattleDownstream');
        const msg = T.decode(payload);
        // protobufjs oneof：msg.msg 标识哪个分支被设置（'start'|'end'|'state'）
        const which = msg.msg;
        if (which === 'start') {
            const s = msg.start;
            return {
                type: 'start',
                roomId: s.roomId,
                countdownSec: s.countdownSec,
                blue: s.bluePlayers.map(String),
                red: s.redPlayers.map(String),
            };
        } else if (which === 'end') {
            const e = msg.end;
            return {
                type: 'end',
                winner: e.winner,
                durationSec: e.durationSec,
                blueKills: e.blueKills,
                redKills: e.redKills,
            };
        } else if (which === 'state') {
            const st = msg.state;
            return {
                type: 'state',
                frame: st.frame,
                timestampMs: String(st.timestampMs),
                battleState: st.battleState,
                entities: st.entities.map(function (e) {
                    return {
                        eid: String(e.entityId),
                        type: e.type,
                        team: e.team,
                        x: e.x, y: e.y, z: e.z,
                        yaw: e.yaw,
                        hp: e.hp,
                        maxHp: e.maxHp,
                        state: e.state,
                        kills: e.kills,
                        deaths: e.deaths,
                        gold: e.gold,
                    };
                }),
            };
        }
        return null;
    }

    const BattleProto = {
        init: init,
        encodeBattleReady: encodeBattleReady,
        encodeBattleMove: encodeBattleMove,
        encodeBattleCast: encodeBattleCast,
        encodeRoomEnter: encodeRoomEnter,
        encodeRoomLeave: encodeRoomLeave,
        decodeDownstream: decodeDownstream,
    };

    if (typeof window !== 'undefined') {
        window.BattleProto = BattleProto;
    }
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = BattleProto;
    }
})();
