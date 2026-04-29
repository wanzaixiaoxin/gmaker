#include "replay.hpp"

namespace gs {
namespace replay {

Checker::Checker(std::chrono::milliseconds window)
    : window_(window),
      min_ts_(std::chrono::system_clock::now() - window) {}

bool Checker::Check(std::chrono::system_clock::time_point ts, const std::string& nonce) {
    auto now = std::chrono::system_clock::now();
    if (ts > now + std::chrono::seconds(30)) {
        return false; // timestamp too far in the future
    }
    if (ts < min_ts_) {
        return false; // timestamp outside replay window
    }

    std::string key = std::to_string(
        std::chrono::duration_cast<std::chrono::nanoseconds>(ts.time_since_epoch()).count())
        + ":" + nonce;

    std::lock_guard<std::mutex> lk(mtx_);
    if (nonces_.find(key) != nonces_.end()) {
        return false; // duplicate nonce
    }
    nonces_[key] = now;
    GcLocked(now);
    return true;
}

void Checker::Clean() {
    std::lock_guard<std::mutex> lk(mtx_);
    GcLocked(std::chrono::system_clock::now());
}

void Checker::GcLocked(std::chrono::system_clock::time_point now) {
    auto cutoff = now - window_;
    if (cutoff < min_ts_) return;
    min_ts_ = cutoff;
    for (auto it = nonces_.begin(); it != nonces_.end();) {
        if (it->second < cutoff) {
            it = nonces_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace replay
} // namespace gs
