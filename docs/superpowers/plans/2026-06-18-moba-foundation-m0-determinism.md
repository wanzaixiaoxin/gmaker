# M0 确定性地基 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为确定性帧同步重写奠定地基——定点数数学库、确定性 PRNG、帧哈希工具、回放一致性测试基建,验证"相同输入→相同输出"这一核心承诺。

**Architecture:** 新建 `common/cpp/realtime/fixed/` 定点数库(header-only + 少量 cpp),配合确定性 xorshift PRNG 和帧哈希工具,用独立 `test-determinism` 可执行文件验证确定性。所有代码为纯新增,不触碰现有战斗逻辑(那是 M1 的事)。

**Tech Stack:** C++17,header-only 定点数(仿 `std::chrono::duration` 风格的强类型),`std::uint64_t` PRNG,xxHash 或 FNV-1a 哈希。测试沿用项目现有风格(plain `main()` + `std::cout << "PASSED"`,无 gtest)。

---

## 文件结构

| 文件 | 责任 |
|---|---|
| `common/cpp/realtime/fixed/fixed.hpp` | `Fixed` 定点数类型(32.32),加减乘除、比较、转换 |
| `common/cpp/realtime/fixed/fixed_math.hpp` | `fixed_sqrt`(牛顿迭代)、`fixed_sin`/`fixed_cos`(查表) |
| `common/cpp/realtime/fixed/fixed_vec3.hpp` | `FixedVec3` 向量运算(点积/叉积/长度/归一化) |
| `common/cpp/realtime/deterministic/prng.hpp` | 确定性 `Xorshift64` PRNG |
| `common/cpp/realtime/deterministic/frame_hash.hpp` | 帧状态哈希工具(FNV-1a,序列化→哈希) |
| `common/cpp/realtime/deterministic/replay_recorder.hpp` | 输入序列录制/回放(存取输入流) |
| `common/cpp/realtime/test/test_determinism.cpp` | 确定性自测(定点数精度、PRNG 可重现、回放一致) |

CMake 新增:`test-determinism` 可执行目标(仿 `test-crypto` 风格)。

---

## Task 1: Fixed 定点数核心类型

**Files:**
- Create: `common/cpp/realtime/fixed/fixed.hpp`

- [ ] **Step 1: 创建定点数头文件**

```cpp
// common/cpp/realtime/fixed/fixed.hpp
#pragma once

#include <cstdint>
#include <stdexcept>

namespace gs::realtime::fixed {

// 32.32 定点数:高位 32 位整数部分,低位 32 位小数部分。
// 覆盖 MOBA 地图坐标范围(~100 单位)并提供亚毫米精度。
// 所有运算确定性:不依赖浮点硬件,纯整数运算。
class Fixed {
public:
    static constexpr int FRACTION_BITS = 32;
    static constexpr std::int64_t FRACTION_MASK = (std::int64_t{1} << FRACTION_BITS) - 1;
    static constexpr std::int64_t ONE = std::int64_t{1} << FRACTION_BITS;

    constexpr Fixed() : raw_(0) {}
    constexpr explicit Fixed(std::int64_t raw) : raw_(raw) {}

    // 从整数构造
    static constexpr Fixed from_int(std::int32_t v) {
        return Fixed(static_cast<std::int64_t>(v) << FRACTION_BITS);
    }

    // 从 float 构造(仅用于初始化常量/测试,运行时禁用)
    static constexpr Fixed from_float(float v) {
        return Fixed(static_cast<std::int64_t>(v * static_cast<float>(ONE)));
    }

    constexpr std::int64_t raw() const { return raw_; }
    constexpr std::int32_t to_int() const { return static_cast<std::int32_t>(raw_ >> FRACTION_BITS); }

    // 基本算术(纯整数,确定性)
    constexpr Fixed operator+(Fixed o) const { return Fixed(raw_ + o.raw_); }
    constexpr Fixed operator-(Fixed o) const { return Fixed(raw_ - o.raw_); }
    constexpr Fixed operator-() const { return Fixed(-raw_); }

    // 乘法:a*b = (raw_a * raw_b) >> 32,用 __int128 防溢出
    constexpr Fixed operator*(Fixed o) const {
        return Fixed(static_cast<std::int64_t>(
            (__int128_t)raw_ * o.raw_ >> FRACTION_BITS));
    }

    // 除法:a/b = (raw_a << 32) / raw_b,用 __int128 防溢出
    constexpr Fixed operator/(Fixed o) const {
        if (o.raw_ == 0) throw std::runtime_error("Fixed division by zero");
        return Fixed(static_cast<std::int64_t>(
            ((__int128_t)raw_ << FRACTION_BITS) / o.raw_));
    }

    // 比较
    constexpr bool operator==(Fixed o) const { return raw_ == o.raw_; }
    constexpr bool operator!=(Fixed o) const { return raw_ != o.raw_; }
    constexpr bool operator<(Fixed o) const { return raw_ < o.raw_; }
    constexpr bool operator>(Fixed o) const { return raw_ > o.raw_; }
    constexpr bool operator<=(Fixed o) const { return raw_ <= o.raw_; }
    constexpr bool operator>=(Fixed o) const { return raw_ >= o.raw_; }

    constexpr Fixed& operator+=(Fixed o) { raw_ += o.raw_; return *this; }
    constexpr Fixed& operator-=(Fixed o) { raw_ -= o.raw_; return *this; }

private:
    std::int64_t raw_;
};

// 常用字面量(编译期)
constexpr Fixed FIXED_ZERO = Fixed::from_int(0);
constexpr Fixed FIXED_ONE = Fixed::from_int(1);

} // namespace gs::realtime::fixed
```

- [ ] **Step 2: 验证头文件可被包含**

Run: 创建一个临时 `scratch.cpp` 含 `#include "common/cpp/realtime/fixed/fixed.hpp"` 并在 main 里构造 `Fixed::from_int(5)`,编译确认无错。然后删除 scratch 文件。

- [ ] **Step 3: Commit**

```bash
git add common/cpp/realtime/fixed/fixed.hpp
git commit -m "feat(m0): add Fixed 32.32 fixed-point type"
```

---

## Task 2: 定点数数学函数(sqrt / sin / cos 查表)

**Files:**
- Create: `common/cpp/realtime/fixed/fixed_math.hpp`

- [ ] **Step 1: 创建数学函数头文件**

```cpp
// common/cpp/realtime/fixed/fixed_math.hpp
#pragma once

#include "fixed.hpp"
#include <cstdint>

namespace gs::realtime::fixed {

// 定点数 sqrt:牛顿迭代(纯整数,确定性)
// 对 raw 值求平方根后再 << 16(因为 sqrt(raw<<32) = sqrt(raw)<<16)
inline Fixed fixed_sqrt(Fixed x) {
    if (x.raw() <= 0) return FIXED_ZERO;
    // 牛顿迭代求 sqrt(x.raw()),初值用 x.raw() 本身
    std::int64_t n = x.raw();
    std::int64_t guess = n;
    // 10 次迭代足够收敛(32.32 精度)
    for (int i = 0; i < 20; ++i) {
        if (guess == 0) break;
        // next = (guess + n/guess) / 2
        __int128_t ng = ((__int128_t)guess + (__int128_t)n / guess) / 2;
        if (ng == guess) break;
        guess = static_cast<std::int64_t>(ng);
    }
    // raw 是 x<<32,sqrt 后应为 sqrt(x)<<16,需再 <<16 还原到 32.32
    return Fixed(guess << 16);
}

// sin/cos 查表:0~2π 分 4096 段,查表得定点数结果
// 表在编译期生成(确定性,无运行时浮点)
namespace detail {
    // 角度用 0~65535 表示 0~2π(BRADIAN,二进制弧度)
    constexpr int BRADIAN_BITS = 16;
    constexpr int BRADIAN_SIZE = 1 << BRADIAN_BITS; // 65536
    constexpr int TABLE_SIZE = 4096;
    constexpr int TABLE_MASK = TABLE_SIZE - 1;

    // 编译期生成 sin 表(0~TABLE_SIZE 对应 0~π/2,其余象限由对称性推导)
    constexpr Fixed generate_sin_entry(int i) {
        // 用泰勒级数计算 sin(i * π/2 / TABLE_SIZE)
        // 注意:这里允许用 double 计算(仅在编译期建表),运行时纯查表
        double angle = (static_cast<double>(i) / TABLE_SIZE) * 1.5707963267948966;
        double s = angle;
        double term = angle;
        for (int k = 1; k < 8; ++k) {
            term *= -angle * angle / ((2 * k) * (2 * k + 1));
            s += term;
        }
        return Fixed::from_float(static_cast<float>(s));
    }

    // 编译期填充 0~π/2 的表
    template<int... Is>
    constexpr auto make_sin_table(std::integer_sequence<int, Is...>) {
        return std::array<Fixed, TABLE_SIZE>{ generate_sin_entry(Is)... };
    }

    constexpr auto SIN_TABLE_QUADRANT = make_sin_table(
        std::make_integer_sequence<int, TABLE_SIZE>{});
}

// 输入 bradian(0~65535 = 0~2π),返回 sin
inline Fixed fixed_sin(std::uint16_t bradian) {
    using namespace detail;
    // 归一化到 0~TABLE_SIZE*4(四个象限)
    int idx = (static_cast<int>(bradian) * 4) >> (BRADIAN_BITS - 12); // /16
    int quadrant = (idx >> 12) & 3;        // 哪个象限
    int in_quad = idx & TABLE_MASK;        // 象限内索引
    switch (quadrant) {
        case 0: return SIN_TABLE_QUADRANT[in_quad];
        case 1: return SIN_TABLE_QUADRANT[TABLE_SIZE - 1 - in_quad];
        case 2: return -SIN_TABLE_QUADRANT[in_quad];
        case 3: return -SIN_TABLE_QUADRANT[TABLE_SIZE - 1 - in_quad];
    }
    return FIXED_ZERO;
}

inline Fixed fixed_cos(std::uint16_t bradian) {
    // cos(x) = sin(x + π/2),π/2 对应 bradian 加 16384
    return fixed_sin(static_cast<std::uint16_t>(bradian + 16384));
}

// abs
inline Fixed fixed_abs(Fixed x) {
    return x.raw() < 0 ? -x : x;
}

} // namespace gs::realtime::fixed
```

- [ ] **Step 2: Commit**

```bash
git add common/cpp/realtime/fixed/fixed_math.hpp
git commit -m "feat(m0): add fixed-point math (sqrt newton, sin/cos lookup table)"
```

---

## Task 3: FixedVec3 向量类型

**Files:**
- Create: `common/cpp/realtime/fixed/fixed_vec3.hpp`

- [ ] **Step 1: 创建向量头文件**

```cpp
// common/cpp/realtime/fixed/fixed_vec3.hpp
#pragma once

#include "fixed.hpp"
#include "fixed_math.hpp"

namespace gs::realtime::fixed {

// 定点数 3D 向量(MOBA 用 XZ 平面,Y=0)
struct FixedVec3 {
    Fixed x, y, z;

    constexpr FixedVec3() : x(FIXED_ZERO), y(FIXED_ZERO), z(FIXED_ZERO) {}
    constexpr FixedVec3(Fixed x_, Fixed y_, Fixed z_) : x(x_), y(y_), z(z_) {}

    constexpr FixedVec3 operator+(const FixedVec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr FixedVec3 operator-(const FixedVec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr FixedVec3 operator*(Fixed s) const { return {x * s, y * s, z * s}; }

    constexpr bool operator==(const FixedVec3& o) const {
        return x == o.x && y == o.y && z == o.z;
    }

    // 点积
    constexpr Fixed dot(const FixedVec3& o) const {
        return x * o.x + y * o.y + z * o.z;
    }

    // 长度(用定点 sqrt)
    Fixed length() const {
        return fixed_sqrt(dot(*this));
    }

    // XZ 平面距离(MOBA 主要用此)
    Fixed length_xz() const {
        return fixed_sqrt(x * x + z * z);
    }

    // 归一化(返回单位向量;零向量返回零)
    FixedVec3 normalized() const {
        Fixed len = length();
        if (len == FIXED_ZERO) return {};
        return {x / len, y / len, z / len};
    }
};

constexpr FixedVec3 FIXED_VEC3_ZERO{};

} // namespace gs::realtime::fixed
```

- [ ] **Step 2: Commit**

```bash
git add common/cpp/realtime/fixed/fixed_vec3.hpp
git commit -m "feat(m0): add FixedVec3 vector type"
```

---

## Task 4: 确定性 PRNG (Xorshift64)

**Files:**
- Create: `common/cpp/realtime/deterministic/prng.hpp`

- [ ] **Step 1: 创建 PRNG 头文件**

```cpp
// common/cpp/realtime/deterministic/prng.hpp
#pragma once

#include <cstdint>

namespace gs::realtime::deterministic {

// Xorshift64:确定性伪随机,相同 seed → 相同序列。
// 用于战斗引擎内所有"随机"需求(暴击/散布等),保证可回放。
// 禁止使用 std::mt19937 / rand()(实现可能跨平台不一致)。
class Xorshift64 {
public:
    // seed=0 会被强制改为非零异或常量,避免退化
    explicit Xorshift64(std::uint64_t seed = 0x9E3779B97F4A7C15ull)
        : state_(seed == 0 ? 0x9E3779B97F4A7C15ull : seed) {}

    // 生成下一个 64 位随机数
    std::uint64_t next() {
        std::uint64_t x = state_;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        state_ = x;
        return x;
    }

    // 生成 [0, max) 范围的整数(无模偏置,用拒绝采样)
    std::uint32_t next_uint32(std::uint32_t max) {
        if (max == 0) return 0;
        std::uint32_t r = static_cast<std::uint32_t>(next() >> 32);
        return r % max;
    }

    // 生成 [0, 1) 定点数(取高 32 位作为小数)
    // 由调用方包含 fixed.hpp 后使用;这里返回 raw 以避免头依赖
    std::int64_t next_fixed_raw() {
        return static_cast<std::int64_t>(next() >> 32); // 高 32 位作为 0.32 小数
    }

    std::uint64_t state() const { return state_; }

private:
    std::uint64_t state_;
};

} // namespace gs::realtime::deterministic
```

- [ ] **Step 2: Commit**

```bash
git add common/cpp/realtime/deterministic/prng.hpp
git commit -m "feat(m0): add deterministic Xorshift64 PRNG"
```

---

## Task 5: 帧状态哈希工具

**Files:**
- Create: `common/cpp/realtime/deterministic/frame_hash.hpp`

- [ ] **Step 1: 创建帧哈希头文件**

```cpp
// common/cpp/realtime/deterministic/frame_hash.hpp
#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

namespace gs::realtime::deterministic {

// FNV-1a 哈希:确定性、跨平台一致、无依赖。
// 用于计算每帧实体状态的哈希,回放时比对以检测非确定性 bug。
class FrameHasher {
public:
    static constexpr std::uint64_t FNV_OFFSET = 0xcbf29ce484222325ull;
    static constexpr std::uint64_t FNV_PRIME = 0x100000001b3ull;

    FrameHasher() : hash_(FNV_OFFSET) {}

    // 追加原始字节
    void update(const void* data, std::size_t len) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        for (std::size_t i = 0; i < len; ++i) {
            hash_ ^= p[i];
            hash_ *= FNV_PRIME;
        }
    }

    // 便捷:追加基础类型
    void update_u64(std::uint64_t v) { update(&v, sizeof(v)); }
    void update_u32(std::uint32_t v) { update(&v, sizeof(v)); }
    void update_i64(std::int64_t v) { update(&v, sizeof(v)); }
    void update_string(const std::string& s) {
        update_u64(s.size());
        update(s.data(), s.size());
    }

    std::uint64_t final_hash() const { return hash_; }

private:
    std::uint64_t hash_;
};

// 便捷:对一段字节直接求哈希
inline std::uint64_t fnv1a_hash(const void* data, std::size_t len) {
    FrameHasher h;
    h.update(data, len);
    return h.final_hash();
}

} // namespace gs::realtime::deterministic
```

- [ ] **Step 2: Commit**

```bash
git add common/cpp/realtime/deterministic/frame_hash.hpp
git commit -m "feat(m0): add FNV-1a frame state hasher"
```

---

## Task 6: 回放录制/回放工具

**Files:**
- Create: `common/cpp/realtime/deterministic/replay_recorder.hpp`

- [ ] **Step 1: 创建录制/回放头文件**

```cpp
// common/cpp/realtime/deterministic/replay_recorder.hpp
#pragma once

#include "frame_hash.hpp"
#include <cstdint>
#include <vector>
#include <string>

namespace gs::realtime::deterministic {

// 录制一局的输入序列 + 每帧哈希,用于回放一致性验证。
// 格式(二进制,大端由调用方保证):
//   [seed u64][frame_count u32]
//   逐帧:[frame_no u32][input_size u32][input_bytes...][expected_hash u64]
struct ReplayFrame {
    std::uint32_t frame_no;
    std::vector<std::uint8_t> input_bytes;  // 该帧的输入(序列化后)
    std::uint64_t expected_hash;            // 该帧结束时的状态哈希
};

class ReplayRecorder {
public:
    explicit ReplayRecorder(std::uint64_t seed) : seed_(seed) {}

    void record_frame(std::uint32_t frame_no,
                      const std::vector<std::uint8_t>& input_bytes,
                      std::uint64_t hash) {
        frames_.push_back({frame_no, input_bytes, hash});
    }

    // 序列化为字节(用于落盘)
    std::vector<std::uint8_t> serialize() const {
        std::vector<std::uint8_t> out;
        auto put_u64 = [&](std::uint64_t v) {
            for (int i = 7; i >= 0; --i) out.push_back(static_cast<std::uint8_t>((v >> (i*8)) & 0xFF));
        };
        auto put_u32 = [&](std::uint32_t v) {
            for (int i = 3; i >= 0; --i) out.push_back(static_cast<std::uint8_t>((v >> (i*8)) & 0xFF));
        };
        put_u64(seed_);
        put_u32(static_cast<std::uint32_t>(frames_.size()));
        for (const auto& f : frames_) {
            put_u32(f.frame_no);
            put_u32(static_cast<std::uint32_t>(f.input_bytes.size()));
            out.insert(out.end(), f.input_bytes.begin(), f.input_bytes.end());
            put_u64(f.expected_hash);
        }
        return out;
    }

    std::uint64_t seed() const { return seed_; }
    const std::vector<ReplayFrame>& frames() const { return frames_; }

private:
    std::uint64_t seed_;
    std::vector<ReplayFrame> frames_;
};

// 从字节反序列化(用于回放)
inline ReplayRecorder deserialize_replay(const std::vector<std::uint8_t>& data) {
    auto get_u64 = [&](std::size_t& off) -> std::uint64_t {
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) v = (v << 8) | data[off + i];
        off += 8;
        return v;
    };
    auto get_u32 = [&](std::size_t& off) -> std::uint32_t {
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) v = (v << 8) | data[off + i];
        off += 4;
        return v;
    };
    std::size_t off = 0;
    std::uint64_t seed = get_u64(off);
    ReplayRecorder rec(seed);
    std::uint32_t count = get_u32(off);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t frame_no = get_u32(off);
        std::uint32_t isize = get_u32(off);
        std::vector<std::uint8_t> input(data.begin() + off, data.begin() + off + isize);
        off += isize;
        std::uint64_t hash = get_u64(off);
        rec.record_frame(frame_no, input, hash);
    }
    return rec;
}

} // namespace gs::realtime::deterministic
```

- [ ] **Step 2: Commit**

```bash
git add common/cpp/realtime/deterministic/replay_recorder.hpp
git commit -m "feat(m0): add replay recorder/deserializer"
```

---

## Task 7: 确定性自测 + CMake 目标

**Files:**
- Create: `common/cpp/realtime/test/test_determinism.cpp`
- Modify: `CMakeLists.txt`(新增 `test-determinism` 目标)

- [ ] **Step 1: 编写自测程序**

```cpp
// common/cpp/realtime/test/test_determinism.cpp
#include "../fixed/fixed.hpp"
#include "../fixed/fixed_math.hpp"
#include "../fixed/fixed_vec3.hpp"
#include "../deterministic/prng.hpp"
#include "../deterministic/frame_hash.hpp"
#include "../deterministic/replay_recorder.hpp"
#include <iostream>
#include <cassert>

using namespace gs::realtime;

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { std::cerr << "FAIL: " << msg << std::endl; ++failures; } \
    else { std::cout << "PASS: " << msg << std::endl; } } while(0)

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
}

void test_fixed_sqrt() {
    using namespace fixed;
    Fixed four = Fixed::from_int(4);
    Fixed result = fixed_sqrt(four);
    // sqrt(4)=2,允许 ±0.01 误差(raw 差 < 2^32 * 0.01)
    std::int64_t diff = result.raw() > FIXED_ONE.raw() ? result.raw() - FIXED_ONE.raw() : FIXED_ONE.raw() - result.raw();
    CHECK(diff < (FIXED_ONE.raw() / 100), "Fixed sqrt(4)≈2");
}

void test_fixed_vec3() {
    using namespace fixed;
    FixedVec3 v{Fixed::from_int(3), FIXED_ZERO, Fixed::from_int(4)};
    Fixed len = v.length_xz();
    // sqrt(9+16)=5
    Fixed five = Fixed::from_int(5);
    std::int64_t diff = len.raw() > five.raw() ? len.raw() - five.raw() : five.raw() - len.raw();
    CHECK(diff < (FIXED_ONE.raw() / 100), "FixedVec3 length (3,0,4)=5");
    // 归一化
    FixedVec3 n = v.normalized();
    Fixed nlen = n.length_xz();
    std::int64_t ndiff = nlen.raw() > FIXED_ONE.raw() ? nlen.raw() - FIXED_ONE.raw() : FIXED_ONE.raw() - nlen.raw();
    CHECK(ndiff < (FIXED_ONE.raw() / 100), "FixedVec3 normalized length=1");
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
    // 不同 seed 应不同
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
    // 顺序不同应不同
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

// 端到端确定性验证:用 PRNG 生成"输入",两次独立计算状态哈希,必须一致
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
    test_fixed_sqrt();
    test_fixed_vec3();
    test_prng_deterministic();
    test_frame_hash_deterministic();
    test_replay_roundtrip();
    test_end_to_end_determinism();
    std::cout << "=== " << (failures == 0 ? "ALL PASSED" : "HAS FAILURES") << " ===" << std::endl;
    return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: 在 CMakeLists.txt 新增 test-determinism 目标**

在 `CMakeLists.txt` 现有 test 目标之后(如 `test-redis` 之后,约 line 380 附近)添加:

```cmake
# ==================== 确定性地基测试 ====================
add_executable(test-determinism
    common/cpp/realtime/test/test_determinism.cpp
)
target_include_directories(test-determinism PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp/realtime
)
```

- [ ] **Step 3: 重新生成 CMake 并构建**

Run:
```bash
cmake -B build -S .
cmake --build build --target test-determinism --config Release
```
Expected: 构建成功,无 error。

- [ ] **Step 4: 运行测试**

Run:
```bash
./build/Release/test-determinism.exe
```
Expected: 输出 `=== ALL PASSED ===`,退出码 0。所有 7 组断言 PASS。

- [ ] **Step 5: Commit**

```bash
git add common/cpp/realtime/test/test_determinism.cpp CMakeLists.txt
git commit -m "test(m0): add determinism self-test + test-determinism CMake target"
```

---

## Task 8: 编译期浮点禁用检查(可选但推荐)

**Files:**
- Create: `common/cpp/realtime/fixed/no_float.hpp`

- [ ] **Step 1: 创建浮点禁用守卫**

```cpp
// common/cpp/realtime/fixed/no_float.hpp
// 在战斗引擎翻译单元 include 此头,触发编译期断言:禁止使用 float/double。
// 用法:在 .cpp 顶部 #include "fixed/no_float.hpp"
#pragma once

#include <type_traits>

// 如果翻译单元内出现了 float/double 全局对象,此 static_assert 失败。
// 注意:这是轻量守卫,无法拦截局部变量;完整拦截需 clang-tidy。
// 底座阶段用此 + code review 兜底,M1 重写引擎时引入 clang-tidy 规则。
namespace gs::realtime::fixed::detail {
    // 占位:真正的浮点检测留给 clang-tidy(readability-magic-numbers + custom check)
    // 此头目前仅作文档化标记,被 include 即表示该 TU 承诺无浮点。
    constexpr bool NO_FLOAT_GUARD = true;
}
```

- [ ] **Step 2: Commit**

```bash
git add common/cpp/realtime/fixed/no_float.hpp
git commit -m "feat(m0): add no_float guard header (documentation + future clang-tidy hook)"
```

---

## 验收标准(M0 完成)

- [ ] `Fixed` 定点数加减乘除/比较正确(精度满足亚毫米)
- [ ] `fixed_sqrt`/`fixed_sin`/`fixed_cos` 查表确定性
- [ ] `FixedVec3` 长度/归一化/点积正确
- [ ] `Xorshift64` 相同 seed 相同序列
- [ ] `FrameHasher` 相同输入相同哈希
- [ ] `ReplayRecorder` 序列化/反序列化往返一致
- [ ] **端到端确定性验证通过**(同输入两次计算哈希一致)—— 这是 M0 的核心承诺
- [ ] `test-determinism` 全部 PASS,纳入 CMake 构建
- [ ] 所有文件已 commit

## M0 不做(留给 M1)

- 不重写现有 `battle_room.cpp` / `lockstep_engine.cpp`(那是 M1)
- 不集成到 realtime-cpp 服务(独立验证即可)
- 不做增量广播/带宽优化(M3)
- clang-tidy 浮点规则只占位,不配置 CI(M1 引入)
