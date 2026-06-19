// common/cpp/realtime/fixed/fixed_vec3.hpp
#pragma once

#include "fixed.hpp"
#include "fixed_math.hpp"

namespace gs::realtime::fixed {

// 定点数 3D 向量(MOBA 主要用 XZ 平面,Y=0)
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
        if (len == FIXED_ZERO) return FixedVec3{};
        return FixedVec3{x / len, y / len, z / len};
    }
};

constexpr FixedVec3 FIXED_VEC3_ZERO{};

} // namespace gs::realtime::fixed
