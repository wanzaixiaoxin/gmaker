#pragma once

#include "udp_packet_connection.hpp"
#include "../middleware.hpp"
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <memory>
#include <thread>
#include <functional>

namespace gs {
namespace net {
namespace async {

class AsyncEventLoop;

// UDPServer 基于 UDP 的无状态报文服务器
// 适用于高频、可丢包、低延迟的广播场景
class UDPServer {
public:
    using DataCallback = std::function<void(UDPPacketConnection*, Packet&)>;

    struct Config {
        std::string host = "0.0.0.0";
        uint16_t    port = 0;       // 0 = 不启用
        int         max_conn = 10000;
        uint32_t    session_timeout_ms = 30000;  // 无包超时回收
    };

    explicit UDPServer(const Config& cfg);
    ~UDPServer();

    UDPServer(const UDPServer&) = delete;
    UDPServer& operator=(const UDPServer&) = delete;

    bool Start();
    void Stop();

    void SetDataCallback(DataCallback cb);
    void Use(std::shared_ptr<Middleware> mw);

private:
    void OnUDPRecv(const sockaddr_in& addr, const uint8_t* data, size_t len);
    void CleanupExpiredSessions();
    static uint64_t MakeAddrKey(const sockaddr_in& addr);

    Config cfg_;

    std::unique_ptr<AsyncEventLoop> loop_;
    std::thread loop_thread_;

    uv_udp_t* udp_handle_ = nullptr;
    uv_timer_t* cleanup_timer_ = nullptr;

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> conn_id_counter_{0};

    // (ip,port) hash → connection
    std::mutex session_mtx_;
    std::unordered_map<uint64_t, std::shared_ptr<UDPPacketConnection>> sessions_;
    std::unordered_map<uint64_t, uint64_t> last_seen_ms_;  // addr_key → last recv time

    std::vector<std::shared_ptr<Middleware>> middlewares_;
    DataCallback on_data_;
};

} // namespace async
} // namespace net
} // namespace gs
