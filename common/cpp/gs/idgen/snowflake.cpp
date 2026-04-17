#include "snowflake.hpp"

namespace gs {
namespace idgen {

Snowflake::Snowflake(int64_t node_id)
    : node_id_(node_id), sequence_(0), last_time_(-1) {
    if (node_id < 0 || node_id > kNodeMax) {
        throw std::invalid_argument("node ID out of range");
    }
}

int64_t Snowflake::NextID() {
    std::lock_guard<std::mutex> lk(mtx_);
    int64_t now = NowMs();
    if (now < last_time_) {
        throw std::runtime_error("clock moved backwards");
    }

    if (now == last_time_) {
        sequence_ = (sequence_ + 1) & kSequenceMask;
        if (sequence_ == 0) {
            while (now <= last_time_) {
                now = NowMs();
            }
        }
    } else {
        sequence_ = 0;
    }

    last_time_ = now;
    return ((now - kEpoch) << kTimeShift) | (node_id_ << kNodeShift) | sequence_;
}

int64_t Snowflake::NowMs() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    return ms.time_since_epoch().count();
}

} // namespace idgen
} // namespace gs
