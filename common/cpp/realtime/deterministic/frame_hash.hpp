// common/cpp/realtime/deterministic/frame_hash.hpp
#pragma once

#include <cstddef>
#include <cstdint>
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
        update_u64(static_cast<std::uint64_t>(s.size()));
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
