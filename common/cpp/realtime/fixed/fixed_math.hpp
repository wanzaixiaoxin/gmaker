// common/cpp/realtime/fixed/fixed_math.hpp
#pragma once

#include "fixed.hpp"
#include <array>
#include <cstdint>
#include <utility>

namespace gs::realtime::fixed {

// 定点数 sqrt:牛顿迭代(纯整数,确定性)
// 对 raw 值求平方根,结果为 Q32.32。
// x.raw() 是 Q32.32,即 value<<32。sqrt 后应为 sqrt(value)<<16,
// 故对 raw 求 sqrt 后再 <<16 还原。
inline Fixed fixed_sqrt(Fixed x) {
    if (x.raw() <= 0) return FIXED_ZERO;
    // 牛顿迭代求 sqrt(n)(n 是 64 位整数),初值取 n
    std::uint64_t n = static_cast<std::uint64_t>(x.raw());
    std::uint64_t guess = n;
    // 迭代 next = (guess + n/guess)/2,收敛后稳定
    for (int i = 0; i < 40; ++i) {
        if (guess == 0) break;
        std::uint64_t q = n / guess;
        std::uint64_t ng = (guess + q) / 2;
        if (ng >= guess) break; // 收敛(单调不再下降)
        guess = ng;
    }
    // guess ≈ sqrt(n) = sqrt(value<<32) = sqrt(value)<<16,需 <<16 还原到 Q32.32
    return Fixed(static_cast<std::int64_t>(guess) << 16);
}

// sin/cos 查表:角度用 bradian(0~65535 = 0~2π)。
// 表覆盖 0~π/2(4096 段),其余象限由对称性推导。
namespace detail {
    constexpr int TABLE_SIZE = 4096;
    constexpr int TABLE_MASK = TABLE_SIZE - 1;

    // 编译期用泰勒级数生成 0~π/2 的 sin 表(此处允许 double,仅建表期)
    constexpr Fixed generate_sin_entry(int i) {
        double angle = (static_cast<double>(i) / TABLE_SIZE) * 1.5707963267948966;
        double s = angle;
        double term = angle;
        for (int k = 1; k < 8; ++k) {
            term *= -angle * angle / ((2.0 * k) * (2.0 * k + 1.0));
            s += term;
        }
        return Fixed::from_float(static_cast<float>(s));
    }

    template<int... Is>
    constexpr std::array<Fixed, TABLE_SIZE> make_sin_table(std::integer_sequence<int, Is...>) {
        return std::array<Fixed, TABLE_SIZE>{ generate_sin_entry(Is)... };
    }

    constexpr std::array<Fixed, TABLE_SIZE> SIN_TABLE_QUADRANT =
        make_sin_table(std::make_integer_sequence<int, TABLE_SIZE>{});
}

// 输入 bradian(0~65535 = 0~2π),返回 sin
inline Fixed fixed_sin(std::uint16_t bradian) {
    using namespace detail;
    // 映射到 0..TABLE_SIZE*4 的四象限索引
    int idx = (static_cast<int>(bradian) * 4) >> 4; // bradian/16 → 0..16383
    int quadrant = (idx >> 12) & 3;                 // 0..3
    int in_quad = idx & TABLE_MASK;                 // 象限内 0..4095
    switch (quadrant) {
        case 0: return SIN_TABLE_QUADRANT[in_quad];
        case 1: return SIN_TABLE_QUADRANT[TABLE_SIZE - 1 - in_quad];
        case 2: return Fixed(-SIN_TABLE_QUADRANT[in_quad].raw());
        case 3: return Fixed(-SIN_TABLE_QUADRANT[TABLE_SIZE - 1 - in_quad].raw());
    }
    return FIXED_ZERO;
}

// cos(x) = sin(x + π/2),π/2 对应 bradian 16384
inline Fixed fixed_cos(std::uint16_t bradian) {
    return fixed_sin(static_cast<std::uint16_t>(bradian + 16384));
}

inline Fixed fixed_abs(Fixed x) {
    return x.raw() < 0 ? Fixed(-x.raw()) : x;
}

} // namespace gs::realtime::fixed
