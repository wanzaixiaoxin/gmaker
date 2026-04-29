#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace gs {
namespace replay {

class Checker {
public:
    explicit Checker(std::chrono::milliseconds window);

    // Check returns true if (ts, nonce) is acceptable
    bool Check(std::chrono::system_clock::time_point ts, const std::string& nonce);

    void Clean();

private:
    void GcLocked(std::chrono::system_clock::time_point now);

    std::mutex mtx_;
    std::chrono::milliseconds window_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> nonces_;
    std::chrono::system_clock::time_point min_ts_;
};

} // namespace replay
} // namespace gs
