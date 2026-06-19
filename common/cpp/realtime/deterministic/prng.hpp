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

    // 生成 [0, max) 范围的整数
    std::uint32_t next_uint32(std::uint32_t max) {
        if (max == 0) return 0;
        std::uint32_t r = static_cast<std::uint32_t>(next() >> 32);
        return r % max;
    }

    // 生成 [0, 1) 定点数小数 raw(高 32 位作为 0.32 小数),
    // 调用方将其作为 Fixed 的 raw 使用(值域 0 ~ 0.999...)
    std::int64_t next_fixed_raw() {
        return static_cast<std::int64_t>(next() >> 32);
    }

    std::uint64_t state() const { return state_; }

private:
    std::uint64_t state_;
};

} // namespace gs::realtime::deterministic
