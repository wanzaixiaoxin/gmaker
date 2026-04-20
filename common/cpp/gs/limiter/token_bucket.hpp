#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace gs {
namespace limiter {

// TokenBucket 分片令牌桶限流器，按 key hash 分片降低锁竞争
class TokenBucket {
public:
    TokenBucket(int capacity, int fill_rate);

    bool Allow(int n = 1);
    bool AllowKey(const std::string& key, int n = 1);

private:
    struct Shard {
        std::mutex mtx;
        double capacity;
        double tokens;
        double fill_rate;
        std::chrono::steady_clock::time_point last_update;
    };

    static constexpr size_t kShardCount = 64;

    std::vector<std::unique_ptr<Shard>> shards_;

    size_t ShardIndex(const std::string& key) const;
    void Refill(Shard* s);
};

} // namespace limiter
} // namespace gs
