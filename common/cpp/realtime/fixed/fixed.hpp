// common/cpp/realtime/fixed/fixed.hpp
#pragma once

#include <cstdint>
#include <stdexcept>

namespace gs::realtime::fixed {

// 32.32 定点数:高位 32 位整数部分,低位 32 位小数部分。
// 覆盖 MOBA 地图坐标范围(~100 单位)并提供亚毫米精度。
// 所有运算确定性:不依赖浮点硬件,纯整数运算。
// 注意:不使用 __int128(MSVC 不支持),用 portable 128-bit 乘法。
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
    // M1a bridge: 转换为 float（仅转换层用，M1b 实体切定点后移除）
    constexpr float to_float() const { return static_cast<float>(raw_) / static_cast<float>(ONE); }

    // 基本算术(纯整数,确定性)
    constexpr Fixed operator+(Fixed o) const { return Fixed(raw_ + o.raw_); }
    constexpr Fixed operator-(Fixed o) const { return Fixed(raw_ - o.raw_); }
    constexpr Fixed operator-() const { return Fixed(-raw_); }

    // 乘法:a*b = (raw_a * raw_b) >> 32
    // portable 64x64->128 乘法(MSVC 无 __int128),取中间 64 位
    constexpr Fixed operator*(Fixed o) const {
        return Fixed(mul_shift_right32(raw_, o.raw_));
    }

    // 除法:a/b = (raw_a << 32) / raw_b,portable 拆分避免溢出
    constexpr Fixed operator/(Fixed o) const {
        if (o.raw_ == 0) throw std::runtime_error("Fixed division by zero");
        return Fixed(div_shift_left32(raw_, o.raw_));
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

    // portable 64x64 乘法后右移 32 位,等价于 (a*b)>>32 但不依赖 __int128。
    // 128 位积 P 的位布局:
    //   P[0..63]   = ll = a_lo*b_lo
    //   P[32..95]  += lh + hl = a_lo*b_hi + a_hi*b_lo  (跨 32 位对齐)
    //   P[64..127] += hh = a_hi*b_hi
    // 取 P>>32 的低 64 位(bits 32..95),需正确累加所有贡献项与进位。
    static constexpr std::int64_t mul_shift_right32(std::int64_t a, std::int64_t b) {
        bool neg = (a < 0) != (b < 0);
        std::uint64_t ua = to_unsigned_abs(a);
        std::uint64_t ub = to_unsigned_abs(b);
        std::uint64_t a_lo = ua & 0xFFFFFFFFull;
        std::uint64_t a_hi = ua >> 32;
        std::uint64_t b_lo = ub & 0xFFFFFFFFull;
        std::uint64_t b_hi = ub >> 32;
        std::uint64_t ll = a_lo * b_lo;
        std::uint64_t lh = a_lo * b_hi;
        std::uint64_t hl = a_hi * b_lo;
        std::uint64_t hh = a_hi * b_hi;
        // P>>32 的低 64 位构成:
        //   低 32 位(bits 32..63)= ll>>32 + (lh 低32) + (hl 低32),取其低32并记录进位
        std::uint64_t mid = (ll >> 32) + (lh & 0xFFFFFFFFull) + (hl & 0xFFFFFFFFull);
        std::uint64_t res_lo32 = mid & 0xFFFFFFFFull;
        std::uint64_t carry = mid >> 32;
        //   高 32 位(bits 64..95)= hh + (lh>>32) + (hl>>32) + carry
        std::uint64_t res_hi32 = hh + (lh >> 32) + (hl >> 32) + carry;
        std::uint64_t result = (res_hi32 << 32) | res_lo32;
        return neg ? -static_cast<std::int64_t>(result) : static_cast<std::int64_t>(result);
    }

    // portable 除法:result = (a << 32) / b,即 Q32.32 除法。
    // 委托给 div_q32_32(逐位长除,无溢出,确定性)。
    static constexpr std::int64_t div_shift_left32(std::int64_t a, std::int64_t b) {
        bool neg = (a < 0) != (b < 0);
        std::uint64_t result = div_q32_32(to_unsigned_abs(a), to_unsigned_abs(b));
        return neg ? -static_cast<std::int64_t>(result) : static_cast<std::int64_t>(result);
    }

    // Q32.32 无符号除法:返回 (numer << 32) / denom 的低 64 位。
    // 用逐位长除避免单次 <<32 溢出。int_part = numer/denom(MOBA 范围内小),
    // 小数部分用 32 次移位减得到。
    static constexpr std::uint64_t div_q32_32(std::uint64_t numer, std::uint64_t denom) {
        if (denom == 0) return 0;
        std::uint64_t int_part = numer / denom;
        std::uint64_t rem = numer % denom;
        std::uint64_t frac_part = 0;
        for (int i = 0; i < 32; ++i) {
            rem <<= 1;
            frac_part <<= 1;
            if (rem >= denom) {
                rem -= denom;
                frac_part |= 1;
            }
        }
        return (int_part << 32) | (frac_part & 0xFFFFFFFFull);
    }

    // 取绝对值的无符号表示(INT64_MIN 安全)
    static constexpr std::uint64_t to_unsigned_abs(std::int64_t v) {
        return v < 0 ? static_cast<std::uint64_t>(-(v + 1)) + 1 : static_cast<std::uint64_t>(v);
    }
};

// 常用字面量(编译期)
constexpr Fixed FIXED_ZERO = Fixed::from_int(0);
constexpr Fixed FIXED_ONE = Fixed::from_int(1);

} // namespace gs::realtime::fixed
