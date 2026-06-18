// Node 往返自测：验证 battle_proto.js 的 protobuf 编解码。
// 使用自动生成的 proto_descriptor.js（与前端 index.html 一致），
// 确保 .proto → descriptor 生成链 + battle_proto 门面端到端正确。
// 运行：node web/test/test_battle_proto.js

const path = require('path');
const fs = require('fs');
const protobuf = require('protobufjs');
const Long = require('long');

// protobufjs 需要 Long 处理 uint64
protobuf.util.Long = Long;
protobuf.configure();

let pass = 0, fail = 0;
function eq(name, got, want) {
    const g = JSON.stringify(got), w = JSON.stringify(want);
    if (g === w) { pass++; console.log('  PASS ' + name); }
    else { fail++; console.log('  FAIL ' + name + '\n    got  = ' + g + '\n    want = ' + w); }
}
function approx(name, got, want, eps = 0.001) {
    if (Math.abs(got - want) <= eps) { pass++; console.log('  PASS ' + name + ' (≈' + got + ')'); }
    else { fail++; console.log('  FAIL ' + name + ' got=' + got + ' want=' + want); }
}

async function main() {
    // 加载自动生成的 descriptor（与 index.html 一致：window.PROTO_DESCRIPTOR）
    const descriptorPath = path.join(__dirname, '..', 'js', 'proto_descriptor.js');
    const descriptorSrc = fs.readFileSync(descriptorPath, 'utf8')
        .replace('window.PROTO_DESCRIPTOR = ', 'globalThis.PROTO_DESCRIPTOR = ');
    eval(descriptorSrc);
    if (!globalThis.PROTO_DESCRIPTOR) throw new Error('PROTO_DESCRIPTOR 未加载');
    const root = protobuf.Root.fromJSON(globalThis.PROTO_DESCRIPTOR);

    // 加载 battle_proto.js 并注入 root
    const BP = require(path.join(__dirname, '..', 'js', 'battle_proto.js'));
    BP.init(root);

    console.log('== 上行 encode 返回 protobuf 字节 ==');
    {
        const buf = BP.encodeBattleMove(12345, '9007199254740993', 0.5, -0.25, 77);
        console.log('  move payload bytes: ' + buf.length);
        if (buf.length > 0) { pass++; console.log('  PASS move returns non-empty bytes'); }
        else { fail++; console.log('  FAIL move returns empty'); }
    }
    {
        const buf = BP.encodeBattleCast(999, '42', 2, 100.5, 200.25, '123456789', 3);
        if (buf.length > 0) { pass++; console.log('  PASS cast returns non-empty bytes'); }
        else { fail++; console.log('  FAIL cast returns empty'); }
    }
    {
        const buf = BP.encodeBattleReady(7, '99');
        if (buf.length > 0) { pass++; console.log('  PASS ready returns non-empty bytes'); }
        else { fail++; console.log('  FAIL ready returns empty'); }
    }
    {
        const buf = BP.encodeRoomEnter(100, '55', 12.5, -7.25);
        if (buf.length > 0) { pass++; console.log('  PASS enter returns non-empty bytes'); }
        else { fail++; console.log('  FAIL enter returns empty'); }
    }

    console.log('== 下行 decode：BattleDownstream oneof（StateSync）==');
    {
        // 构造一个 BattleStateSync，序列化后交给 decodeDownstream 解
        const BattleDownstream = root.lookupType('battle.BattleDownstream');
        const ds = BattleDownstream.create({
            msg: 'state',
            state: {
                frame: 42,
                timestampMs: Long.fromString('9999'),
                battleState: 3, // Fighting
                entities: [
                    { entityId: 1, type: 1, team: 1, x: 10.5, y: 0, z: 20.5, yaw: 1.5, hp: 800, maxHp: 1000, state: 1, kills: 3, deaths: 0, gold: 500 },
                    { entityId: 2, type: 3, team: 2, x: 100, y: 0, z: 100, yaw: 0, hp: 2000, maxHp: 2000, state: 0 },
                ],
            },
        });
        const bytes = BattleDownstream.encode(ds).finish();

        const res = BP.decodeDownstream(bytes);
        eq('state type', res.type, 'state');
        eq('state frame', res.frame, 42);
        eq('state battleState', res.battleState, 3);
        eq('state entity count', res.entities.length, 2);
        eq('hero eid', res.entities[0].eid, '1');
        approx('hero x', res.entities[0].x, 10.5);
        eq('hero hp', res.entities[0].hp, 800);
        eq('hero kills', res.entities[0].kills, 3);
        eq('tower eid', res.entities[1].eid, '2');
        eq('tower hp', res.entities[1].hp, 2000);
        // 非 Hero 的 kills 字段：proto3 默认 0
        eq('tower kills default 0', res.entities[1].kills, 0);
    }

    console.log('== 下行 decode：BattleStart ==');
    {
        const BattleDownstream = root.lookupType('battle.BattleDownstream');
        const ds = BattleDownstream.create({
            msg: 'start',
            start: { roomId: 42, countdownSec: 3, bluePlayers: ['100'], redPlayers: [] },
        });
        const bytes = BattleDownstream.encode(ds).finish();
        const res = BP.decodeDownstream(bytes);
        eq('start type', res.type, 'start');
        eq('start roomId', res.roomId, 42);
        eq('start countdown', res.countdownSec, 3);
        eq('start blue[0]', res.blue[0], '100');
        eq('start red count', res.red.length, 0);
    }

    console.log('== 下行 decode：BattleEnd ==');
    {
        const BattleDownstream = root.lookupType('battle.BattleDownstream');
        const ds = BattleDownstream.create({
            msg: 'end',
            end: { winner: 1, durationSec: 600, blueKills: 15, redKills: 12 },
        });
        const bytes = BattleDownstream.encode(ds).finish();
        const res = BP.decodeDownstream(bytes);
        eq('end type', res.type, 'end');
        eq('end winner', res.winner, 1);
        eq('end duration', res.durationSec, 600);
        eq('end blueKills', res.blueKills, 15);
    }

    console.log('== 空 payload 返回 null ==');
    {
        eq('empty payload → null', BP.decodeDownstream(new Uint8Array(0)), null);
        eq('null payload → null', BP.decodeDownstream(null), null);
    }

    console.log('== 完整往返：encode 上行 → server 侧 decode ==');
    {
        // 模拟 server 端用同样的 schema 解客户端 encode 的字节
        const HeroMoveInput = root.lookupType('battle.HeroMoveInput');
        const bytes = BP.encodeBattleMove(555, '888888', 0.7, -0.3, 9);
        const decoded = HeroMoveInput.decode(bytes);
        eq('server-side roomId', decoded.roomId, 555);
        eq('server-side playerId', decoded.playerId.toString(), '888888');
        approx('server-side moveX', decoded.moveX, 0.7);
        approx('server-side moveZ', decoded.moveZ, -0.3);
        eq('server-side inputSeq', decoded.inputSeq, 9);
    }

    console.log('\n' + (fail === 0 ? 'ALL PASS (' + pass + ')' : 'FAILURES: ' + fail + ' / ' + (pass + fail)));
    process.exit(fail === 0 ? 0 : 1);
}

main().catch(e => { console.error('TEST ERROR:', e); process.exit(1); });
