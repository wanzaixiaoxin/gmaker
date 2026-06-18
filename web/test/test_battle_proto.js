// Node 往返自测：验证 battle_proto.js 的 encode/decode 与 server 字节布局一致。
// 运行：node web/test/test_battle_proto.js
// 不依赖浏览器，直接 require 源文件（通过 vm 注入 global.BattleProto）。

const fs = require('fs');
const path = require('path');
const vm = require('vm');

// 加载 battle_proto.js：它在 typeof window !== 'undefined' 时挂 window，否则无导出。
// 用 vm 执行并人工捕获 BattleProto —— 把源码包一层，让其顶层函数声明可见。
const code = fs.readFileSync(path.join(__dirname, '..', 'js', 'battle_proto.js'), 'utf8');
const ctx = {};
vm.createContext(ctx);
vm.runInContext(code + '\nthis.BattleProto = { encodeBattleReady, encodeBattleMove, encodeBattleCast, encodeRoomEnter, decodeDownstream, decodeBattleStart, decodeBattleEnd, decodeBattleState, BattleMsgType };', ctx);
const BP = ctx.BattleProto;

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

console.log('== BATTLE_MOVE 往返 ==');
{
    const roomId = 12345, pid = 9007199254740993n; // 超 2^53 测 BigInt 精度
    const buf = BP.encodeBattleMove(roomId, pid, 0.5, -0.25, 77);
    // 校验长度
    eq('move payload length', buf.length, 24);
    // 校验字段（room@0 u32, pid@4 u64, moveX@12, moveZ@16, seq@20）
    const dv = new DataView(buf.buffer);
    eq('move roomId', dv.getUint32(0, false), roomId);
    // 9007199254740993 = 0x20000000000001 → hi32 = 0x200000, lo32 = 0x1
    eq('move pid hi', dv.getUint32(4, false), 0x200000);
    eq('move pid lo', dv.getUint32(8, false), 1);
    approx('move moveX', dv.getFloat32(12, false), 0.5);
    approx('move moveZ', dv.getFloat32(16, false), -0.25);
    eq('move seq', dv.getUint32(20, false), 77);
}

console.log('== BATTLE_CAST 往返 ==');
{
    const buf = BP.encodeBattleCast(999, 42, 2, 100.5, 200.25, 123456789, 3);
    eq('cast payload length', buf.length, 33); // 刻意非对齐
    const dv = new DataView(buf.buffer);
    eq('cast room', dv.getUint32(0, false), 999);
    eq('cast slot@12', buf[12], 2);
    approx('cast tx@13', dv.getFloat32(13, false), 100.5);
    approx('cast tz@17', dv.getFloat32(17, false), 200.25);
    eq('cast targetEid hi@21', dv.getUint32(21, false), 0);
    eq('cast targetEid lo@25', dv.getUint32(25, false), 123456789);
    eq('cast seq@29', dv.getUint32(29, false), 3);
}

console.log('== BATTLE_READY 往返 ==');
{
    const buf = BP.encodeBattleReady(7, 99);
    eq('ready length', buf.length, 12);
    const dv = new DataView(buf.buffer);
    eq('ready room', dv.getUint32(0, false), 7);
    eq('ready player', dv.getUint32(4, false), 0);
    eq('ready player lo', dv.getUint32(8, false), 99);
}

console.log('== ROOM_ENTER 往返 ==');
{
    const buf = BP.encodeRoomEnter(100, 55, 12.5, -7.25);
    eq('enter length', buf.length, 20);
    const dv = new DataView(buf.buffer);
    eq('enter room', dv.getUint32(0, false), 100);
    approx('enter spawnX BE', dv.getFloat32(12, false), 12.5);
    approx('enter spawnZ BE', dv.getFloat32(16, false), -7.25);
}

console.log('== 下行 decode：模拟 server SerializeBattleStateSync ==');
{
    // 手工构造一个与 server 布局一致的下行字节流
    const entities = [
        { eid: 1n, type: 1, team: 1, x: 10.5, y: 0, z: 20.5, yaw: 1.5, hp: 800, maxHp: 1000, state: 1, kills: 3, deaths: 0, gold: 500 }, // Hero 47B
        { eid: 2n, type: 3, team: 2, x: 100, y: 0, z: 100, yaw: 0, hp: 2000, maxHp: 2000, state: 0 }, // Tower 35B
    ];
    // 布局：[msgType 1][frame 4][ts 8][battleState 1][count 4] + entities
    const buf = new Uint8Array(1 + 4 + 8 + 1 + 4 + 47 + 35);
    const dv = new DataView(buf.buffer);
    let off = 0;
    buf[off++] = BP.BattleMsgType.BattleStateSync; // 113
    dv.setUint32(off, 42, false); off += 4;       // frame
    // ts u64 = 9999
    dv.setUint32(off, 0, false); off += 4;
    dv.setUint32(off, 9999, false); off += 4;
    buf[off++] = 3;                                 // battleState=Fighting
    dv.setUint32(off, 2, false); off += 4;          // count
    // entity 0 (hero)
    function putU64(o, v) { dv.setUint32(o, Number((BigInt(v) >> 32n) & 0xFFFFFFFFn), false); dv.setUint32(o + 4, Number(BigInt(v) & 0xFFFFFFFFn), false); }
    putU64(off, 1); off += 8;
    buf[off++] = 1; buf[off++] = 1; // type, team
    dv.setFloat32(off, 10.5, false); off += 4;
    dv.setFloat32(off, 0, false); off += 4;
    dv.setFloat32(off, 20.5, false); off += 4;
    dv.setFloat32(off, 1.5, false); off += 4;
    dv.setUint32(off, 800, false); off += 4;
    dv.setUint32(off, 1000, false); off += 4;
    buf[off++] = 1; // state
    dv.setUint32(off, 3, false); off += 4;   // kills
    dv.setUint32(off, 0, false); off += 4;   // deaths
    dv.setUint32(off, 500, false); off += 4; // gold
    // entity 1 (tower)
    putU64(off, 2); off += 8;
    buf[off++] = 3; buf[off++] = 2;
    dv.setFloat32(off, 100, false); off += 4;
    dv.setFloat32(off, 0, false); off += 4;
    dv.setFloat32(off, 100, false); off += 4;
    dv.setFloat32(off, 0, false); off += 4;
    dv.setUint32(off, 2000, false); off += 4;
    dv.setUint32(off, 2000, false); off += 4;
    buf[off++] = 0;

    const res = BP.decodeDownstream(buf);
    eq('downstream type', res.type, 'state');
    eq('state frame', res.frame, 42);
    eq('state battleState', res.battleState, 3);
    eq('state entity count', res.entities.length, 2);
    eq('hero eid', res.entities[0].eid, '1');
    approx('hero x', res.entities[0].x, 10.5);
    eq('hero hp', res.entities[0].hp, 800);
    eq('hero kills', res.entities[0].kills, 3);
    eq('tower eid', res.entities[1].eid, '2');
    eq('tower hp', res.entities[1].hp, 2000);
    eq('tower no kills field', res.entities[1].kills, undefined);
}

console.log('== 下行 decode：BattleStart / BattleEnd ==');
{
    // Start: [113? no=110][room][countdown][blueN][pids][redN][pids]
    const buf = new Uint8Array(1 + 4 + 4 + 4 + 8 + 4);
    const dv = new DataView(buf.buffer);
    buf[0] = BP.BattleMsgType.BattleStart; // 110
    dv.setUint32(1, 42, false);
    dv.setUint32(5, 3, false);     // countdown
    dv.setUint32(9, 1, false);     // blueN
    dv.setUint32(13, 0, false);    // blue pid hi
    dv.setUint32(17, 100, false);  // blue pid lo
    dv.setUint32(21, 0, false);    // redN
    const res = BP.decodeDownstream(buf);
    eq('start type', res.type, 'start');
    eq('start room', res.roomId, 42);
    eq('start countdown', res.countdownSec, 3);
    eq('start blue[0]', res.blue[0], '100');
    eq('start red count', res.red.length, 0);
}
{
    // End: [111][winner u8][duration u32][blueKills u32][redKills u32]
    const buf = new Uint8Array(1 + 1 + 4 + 4 + 4);
    const dv = new DataView(buf.buffer);
    buf[0] = BP.BattleMsgType.BattleEnd; // 111
    buf[1] = 1; // Blue wins
    dv.setUint32(2, 600, false);
    dv.setUint32(6, 15, false);
    dv.setUint32(10, 12, false);
    const res = BP.decodeDownstream(buf);
    eq('end type', res.type, 'end');
    eq('end winner', res.winner, 1);
    eq('end duration', res.durationSec, 600);
    eq('end blueKills', res.blueKills, 15);
}

console.log('== 未知 msg-type 返回 null ==');
{
    const buf = new Uint8Array([255]);
    eq('unknown type → null', BP.decodeDownstream(buf), null);
}

console.log('\n' + (fail === 0 ? 'ALL PASS (' + pass + ')' : 'FAILURES: ' + fail + ' / ' + (pass + fail)));
process.exit(fail === 0 ? 0 : 1);
