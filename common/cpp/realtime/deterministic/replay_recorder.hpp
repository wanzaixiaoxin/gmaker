// common/cpp/realtime/deterministic/replay_recorder.hpp
#pragma once

#include "frame_hash.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace gs::realtime::deterministic {

// 录制一局的输入序列 + 每帧哈希,用于回放一致性验证。
// 二进制格式(大端):
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
            for (int i = 7; i >= 0; --i) out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
        };
        auto put_u32 = [&](std::uint32_t v) {
            for (int i = 3; i >= 0; --i) out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
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
