// common/cpp/realtime/test/test_determinism.cpp
#include "../fixed/fixed.hpp"
#include "../fixed/fixed_math.hpp"
#include "../fixed/fixed_vec3.hpp"
#include "../lockstep_engine.hpp"
#include "../deterministic/prng.hpp"
#include "../deterministic/frame_hash.hpp"
#include "../deterministic/replay_recorder.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

using namespace gs::realtime;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; ++failures; } \
    else { std::cout << "PASS: " << msg << std::endl; } } while(0)

// 近似相等判断(定点 raw 差 < 容差)
static bool approx_raw(std::int64_t a, std::int64_t b, std::int64_t tol) {
    std::int64_t d = a > b ? a - b : b - a;
    return d <= tol;
}

void test_fixed_basic() {
    using namespace fixed;
    CHECK(Fixed::from_int(3) + Fixed::from_int(4) == Fixed::from_int(7), "Fixed add");
    CHECK(Fixed::from_int(10) - Fixed::from_int(3) == Fixed::from_int(7), "Fixed sub");
    CHECK(Fixed::from_int(3) * Fixed::from_int(4) == Fixed::from_int(12), "Fixed mul int");
    CHECK(Fixed::from_int(12) / Fixed::from_int(4) == Fixed::from_int(3), "Fixed div int");
    // 小数:0.5 + 0.5 = 1.0
    Fixed half = Fixed::from_float(0.5f);
    CHECK(half + half == FIXED_ONE, "Fixed 0.5+0.5=1.0");
    // 比较
    CHECK(Fixed::from_int(5) > Fixed::from_int(3), "Fixed compare");
    // 乘除往返:7 * 3 / 3 == 7
    Fixed seven = Fixed::from_int(7);
    CHECK(seven * Fixed::from_int(3) / Fixed::from_int(3) == seven, "Fixed mul-div roundtrip");
}

void test_fixed_mul_precision() {
    using namespace fixed;
    // 1.5 * 2.0 = 3.0,验证定点乘法正确性(包括小数部分)
    Fixed one_point_five = Fixed::from_float(1.5f);
    Fixed two = Fixed::from_int(2);
    Fixed result = one_point_five * two;
    CHECK(approx_raw(result.raw(), Fixed::from_int(3).raw(), FIXED_ONE.raw() / 1000),
          "Fixed 1.5*2.0=3.0 (mul precision)");
    // 0.25 * 0.5 = 0.125
    Fixed q = Fixed::from_float(0.25f) * Fixed::from_float(0.5f);
    Fixed expected = Fixed::from_float(0.125f);
    CHECK(approx_raw(q.raw(), expected.raw(), FIXED_ONE.raw() / 1000),
          "Fixed 0.25*0.5=0.125 (small mul)");
}

void test_fixed_sqrt() {
    using namespace fixed;
    Fixed result = fixed_sqrt(Fixed::from_int(4));
    // sqrt(4)=2,允许 ±0.01 误差
    CHECK(approx_raw(result.raw(), FIXED_ONE.raw() * 2, FIXED_ONE.raw() / 100),
          "Fixed sqrt(4)≈2");
    // sqrt(9)=3
    Fixed r9 = fixed_sqrt(Fixed::from_int(9));
    CHECK(approx_raw(r9.raw(), FIXED_ONE.raw() * 3, FIXED_ONE.raw() / 100),
          "Fixed sqrt(9)≈3");
}

void test_fixed_vec3() {
    using namespace fixed;
    FixedVec3 v{Fixed::from_int(3), FIXED_ZERO, Fixed::from_int(4)};
    Fixed len = v.length_xz();
    // sqrt(9+16)=5
    CHECK(approx_raw(len.raw(), Fixed::from_int(5).raw(), FIXED_ONE.raw() / 100),
          "FixedVec3 length (3,0,4)=5");
    // 归一化后长度≈1(sqrt + 三次除法链的累积误差,放宽到 2%)
    FixedVec3 n = v.normalized();
    Fixed nlen = n.length_xz();
    CHECK(approx_raw(nlen.raw(), FIXED_ONE.raw(), FIXED_ONE.raw() / 50),
          "FixedVec3 normalized length=1");
}

void test_sin_cos() {
    using namespace fixed;
    // sin(0)=0, sin(π/2)=1, sin(π)=0
    Fixed s0 = fixed_sin(0);
    CHECK(approx_raw(s0.raw(), FIXED_ZERO.raw(), FIXED_ONE.raw() / 100), "sin(0)=0");
    Fixed s90 = fixed_sin(16384); // π/2
    CHECK(approx_raw(s90.raw(), FIXED_ONE.raw(), FIXED_ONE.raw() / 100), "sin(π/2)=1");
    Fixed s180 = fixed_sin(32768); // π
    CHECK(approx_raw(s180.raw(), FIXED_ZERO.raw(), FIXED_ONE.raw() / 100), "sin(π)=0");
    // cos(0)=1
    Fixed c0 = fixed_cos(0);
    CHECK(approx_raw(c0.raw(), FIXED_ONE.raw(), FIXED_ONE.raw() / 100), "cos(0)=1");
}

void test_prng_deterministic() {
    using namespace deterministic;
    Xorshift64 a(12345);
    Xorshift64 b(12345);
    bool same = true;
    for (int i = 0; i < 1000; ++i) {
        if (a.next() != b.next()) { same = false; break; }
    }
    CHECK(same, "PRNG same seed → same sequence (1000 draws)");
    Xorshift64 c(99999);
    CHECK(a.next() != c.next(), "PRNG different seed → different");
}

void test_frame_hash_deterministic() {
    using namespace deterministic;
    FrameHasher h1, h2;
    std::uint64_t v = 0x123456789ABCDEF0ull;
    h1.update_u64(v);
    h1.update_string("hero1");
    h2.update_u64(v);
    h2.update_string("hero1");
    CHECK(h1.final_hash() == h2.final_hash(), "FrameHash same input → same hash");
    FrameHasher h3;
    h3.update_string("hero1");
    h3.update_u64(v);
    CHECK(h3.final_hash() != h1.final_hash(), "FrameHash order matters");
}

void test_replay_roundtrip() {
    using namespace deterministic;
    ReplayRecorder rec(42);
    rec.record_frame(1, {0x01, 0x02}, 0xAAAABBBBCCCCDDDDull);
    rec.record_frame(2, {0x03}, 0x1111222233334444ull);
    auto bytes = rec.serialize();
    auto rec2 = deserialize_replay(bytes);
    CHECK(rec2.seed() == 42, "Replay seed roundtrip");
    CHECK(rec2.frames().size() == 2, "Replay frame count roundtrip");
    CHECK(rec2.frames()[0].frame_no == 1, "Replay frame[0] no");
    CHECK(rec2.frames()[0].expected_hash == 0xAAAABBBBCCCCDDDDull, "Replay frame[0] hash");
    CHECK(rec2.frames()[1].input_bytes == std::vector<std::uint8_t>{0x03}, "Replay frame[1] input");
}

// LockstepEngine 确定性验证：相同输入序列两次执行，TryAdvance 产出完全一致的帧哈希
void test_lockstep_determinism() {
    using namespace deterministic;
    auto run_session = [](std::uint64_t seed) -> std::uint64_t {
        LockstepEngine engine;
        engine.SetTimeoutFrames(12); // ~400ms timeout
        std::vector<std::uint64_t> players = {1, 2};
        engine.SetPlayers(players);

        FrameHasher session_hash;
        Xorshift64 rng(seed);

        // 模拟 100 帧对局，每帧每个玩家提交一个输入
        for (uint32_t f = 1; f <= 100; ++f) {
            engine.SetCurrentFrame(f);
            engine.TickFrameCounter();

            for (auto pid : players) {
                PlayerInput inp;
                inp.player_id = pid;
                inp.input_seq = f;
                inp.has_input = true;
                // 用 PRNG 生成确定性输入
                inp.move_x = fixed::Fixed(rng.next_fixed_raw());
                inp.move_z = fixed::Fixed(rng.next_fixed_raw());
                engine.SubmitInput(pid, inp);
            }

            std::vector<FrameInputs> confirmed;
            engine.TryAdvance(confirmed);
            for (const auto& fi : confirmed) {
                // 哈希每帧确认的输入（多轮应该完全一致）
                FrameHasher fh;
                for (const auto& [pid, inp] : fi.player_inputs) {
                    fh.update_u64(pid);
                    fh.update_i64(inp.move_x.raw());
                    fh.update_i64(inp.move_z.raw());
                }
                session_hash.update_u64(fh.final_hash());
            }
        }
        return session_hash.final_hash();
    };

    std::uint64_t h1 = run_session(555);
    std::uint64_t h2 = run_session(555);
    CHECK(h1 == h2, "LockstepEngine: 100帧同输入→同hash（确定性帧同步验证）");
}
void test_end_to_end_determinism() {
    using namespace fixed;
    using namespace deterministic;
    auto compute_hash = [](std::uint64_t seed) -> std::uint64_t {
        Xorshift64 rng(seed);
        FixedVec3 pos{FIXED_ZERO, FIXED_ZERO, FIXED_ZERO};
        FrameHasher h;
        for (int frame = 0; frame < 100; ++frame) {
            // 模拟确定性运动:rng 给方向,pos 累加
            Fixed dx = Fixed(rng.next_fixed_raw()); // 0~1
            Fixed dz = Fixed(rng.next_fixed_raw());
            pos.x += dx;
            pos.z += dz;
            h.update_i64(pos.x.raw());
            h.update_i64(pos.z.raw());
        }
        return h.final_hash();
    };
    std::uint64_t h1 = compute_hash(777);
    std::uint64_t h2 = compute_hash(777);
    CHECK(h1 == h2, "End-to-end: same input → same hash (determinism core)");
}

int main() {
    std::cout << "=== M0 Determinism Tests ===" << std::endl;
    test_fixed_basic();
    test_fixed_mul_precision();
    test_fixed_sqrt();
    test_fixed_vec3();
    test_sin_cos();
    test_prng_deterministic();
    test_frame_hash_deterministic();
    test_lockstep_determinism();
    test_replay_roundtrip();
    test_end_to_end_determinism();
    std::cout << "=== " << (failures == 0 ? "ALL PASSED" : "HAS FAILURES") << " ===" << std::endl;
    return failures == 0 ? 0 : 1;
}
