#pragma once

#include "../net/iconnection.hpp"
#include "../net/packet.hpp"
#include <cstdint>
#include <future>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <map>

namespace gs {
namespace rpc {

class Client {
public:
    explicit Client(net::IConnection* conn);
    ~Client();

    void SetConn(net::IConnection* conn);

    // 收到数据包时调用，由外部网络层注入
    void OnPacket(net::Packet& pkt);

    // 同步调用，返回 future
    std::future<net::Packet> Call(uint32_t cmd_id,
                                   const std::vector<uint8_t>& payload,
                                   std::chrono::milliseconds timeout);

    // Fire-Forget
    bool FireForget(uint32_t cmd_id, const std::vector<uint8_t>& payload);

private:
    uint32_t NextSeqID();
    void TimeoutCleanup(uint32_t seq_id);
    void TimeoutLoop();

    net::IConnection* conn_ = nullptr;
    std::atomic<uint32_t> seq_id_{0};

    std::mutex pending_mtx_;
    std::unordered_map<uint32_t, std::promise<net::Packet>> pending_;

    // 后台定时器线程（避免每次 Call 创建 detached 线程）
    std::thread timeout_thread_;
    std::mutex timeout_mtx_;
    std::condition_variable timeout_cv_;
    std::map<std::chrono::steady_clock::time_point, uint32_t> timeouts_;
    bool timeout_stop_ = false;
};

} // namespace rpc
} // namespace gs
