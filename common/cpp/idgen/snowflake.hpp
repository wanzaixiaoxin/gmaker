#pragma once

#include <cstdint>
#include <chrono>
#include <mutex>
#include <stdexcept>

namespace gs {
namespace idgen {

class Snowflake {
public:
    explicit Snowflake(int64_t node_id);
    int64_t NextID();

private:
    static int64_t NowMs();

    static constexpr int kNodeBits = 10;
    static constexpr int kSequenceBits = 12;
    static constexpr int64_t kNodeMax = (1LL << kNodeBits) - 1;
    static constexpr int64_t kSequenceMask = (1LL << kSequenceBits) - 1;
    static constexpr int kTimeShift = kNodeBits + kSequenceBits;
    static constexpr int kNodeShift = kSequenceBits;
    static constexpr int64_t kEpoch = 1704067200000LL; // 2024-01-01 00:00:00 UTC

    std::mutex mtx_;
    int64_t node_id_;
    int64_t sequence_;
    int64_t last_time_;
};

} // namespace idgen
} // namespace gs