#pragma once

#include "../net/tcp_conn.hpp"
#include "../net/packet.hpp"
#include <cstdint>
#include <future>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace gs {
namespace rpc {

class Client {
public:
    explicit Client(net::TCPConn* conn);
    ~Client();

    void SetConn(net::TCPConn* conn);

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

    net::TCPConn* conn_ = nullptr;
    std::atomic<uint32_t> seq_id_{0};

    std::mutex pending_mtx_;
    std::unordered_map<uint32_t, std::promise<net::Packet>> pending_;
};

} // namespace rpc
} // namespace gs
