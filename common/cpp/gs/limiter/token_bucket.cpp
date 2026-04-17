#include "token_bucket.hpp"

namespace gs {
namespace limiter {

TokenBucket::TokenBucket(int capacity, int fill_rate)
    : capacity_(static_cast<double>(capacity)),
      tokens_(static_cast<double>(capacity)),
      fill_rate_(static_cast<double>(fill_rate)),
      last_update_(std::chrono::steady_clock::now()) {}

bool TokenBucket::Allow(int n) {
    std::lock_guard<std::mutex> lk(mtx_);
    Refill();
    if (tokens_ >= static_cast<double>(n)) {
        tokens_ -= static_cast<double>(n);
        return true;
    }
    return false;
}

void TokenBucket::Refill() {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - last_update_).count();
    tokens_ += elapsed * fill_rate_;
    if (tokens_ > capacity_) {
        tokens_ = capacity_;
    }
    last_update_ = now;
}

} // namespace limiter
} // namespace gs
