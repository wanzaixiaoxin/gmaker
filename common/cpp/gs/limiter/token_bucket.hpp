#pragma once

#include <atomic>
#include <chrono>
#include <mutex>

namespace gs {
namespace limiter {

class TokenBucket {
public:
    TokenBucket(int capacity, int fill_rate);

    bool Allow(int n = 1);

private:
    void Refill();

    std::mutex mtx_;
    double capacity_;
    double tokens_;
    double fill_rate_;
    std::chrono::steady_clock::time_point last_update_;
};

} // namespace limiter
} // namespace gs
