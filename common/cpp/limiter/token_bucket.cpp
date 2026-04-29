#include "token_bucket.hpp"

namespace gs {
namespace limiter {

TokenBucket::TokenBucket(int capacity, int fill_rate) {
    shards_.reserve(kShardCount);
    double perShardCap = static_cast<double>(capacity) / static_cast<double>(kShardCount);
    double perShardRate = static_cast<double>(fill_rate) / static_cast<double>(kShardCount);
    for (size_t i = 0; i < kShardCount; ++i) {
        auto s = std::make_unique<Shard>();
        s->capacity = perShardCap;
        s->tokens = perShardCap;
        s->fill_rate = perShardRate;
        s->last_update = std::chrono::steady_clock::now();
        shards_.push_back(std::move(s));
    }
}

size_t TokenBucket::ShardIndex(const std::string& key) const {
    // FNV-1a hash
    size_t h = 14695981039346656037ull;
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h % kShardCount;
}

bool TokenBucket::Allow(int n) {
    return AllowKey("__global__", n);
}

bool TokenBucket::AllowKey(const std::string& key, int n) {
    Shard* s = shards_[ShardIndex(key)].get();
    std::lock_guard<std::mutex> lk(s->mtx);
    Refill(s);
    if (s->tokens >= static_cast<double>(n)) {
        s->tokens -= static_cast<double>(n);
        return true;
    }
    return false;
}

void TokenBucket::Refill(Shard* s) {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - s->last_update).count();
    s->tokens += elapsed * s->fill_rate;
    if (s->tokens > s->capacity) {
        s->tokens = s->capacity;
    }
    s->last_update = now;
}

} // namespace limiter
} // namespace gs
