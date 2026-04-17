#pragma once

#include "tcp_conn.hpp"
#include "packet.hpp"
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_map>

namespace gs {
namespace net {

// WriteCoalescer 写聚合器：将同一帧内多个 Packet 合并为一次 write
// - 适用于广播场景（Realtime sync room state）
// - 非广播包直接透传
// - 后台线程每 interval_ms flush 一次
class WriteCoalescer {
public:
    explicit WriteCoalescer(int interval_ms = 16);
    ~WriteCoalescer();

    void Start();
    void Stop();

    // 添加一个包到发送缓冲（线程安全）
    void Enqueue(TCPConn* conn, const Packet& pkt);

    // 广播：将同一包发给多个连接（聚合后发送）
    void Broadcast(const std::vector<TCPConn*>& conns, const Packet& pkt);

private:
    void FlushLoop();
    void DoFlush();

    struct Pending {
        TCPConn* conn = nullptr;
        std::vector<uint8_t> data; // 已编码的 packet bytes
    };

    std::mutex mtx_;
    std::vector<Pending> pending_;

    std::atomic<bool> running_{false};
    std::thread thread_;
    int interval_ms_;
};

} // namespace net
} // namespace gs
