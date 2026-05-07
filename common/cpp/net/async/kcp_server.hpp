#pragma once

#include "kcp_connection.hpp"
#include "../middleware.hpp"
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include <thread>

namespace gs {
namespace net {
namespace async {

class AsyncEventLoop;

// KCPServer 基于 KCP (over UDP) 的服务器
// 管理多个 KCP 会话，共享单个 UDP socket
class KCPServer {
public:
    using ConnectCallback = std::function<void(KCPPacketConnection*)>;
    using DataCallback    = std::function<void(KCPPacketConnection*, Packet&)>;
    using CloseCallback   = std::function<void(KCPPacketConnection*)>;

    struct Config {
        std::string host = "0.0.0.0";
        uint16_t    port = 0;       // 0 = 不启用
        int         max_conn = 10000;
        uint32_t    kcp_interval = 10;    // ikcp_update 间隔 ms
        int         kcp_nodelay = 1;      // 0=普通, 1=快速
        int         kcp_resend = 2;       // 快速重传阈值
        int         kcp_nc = 1;           // 1=关闭流控
    };

    explicit KCPServer(const Config& cfg);
    ~KCPServer();

    KCPServer(const KCPServer&) = delete;
    KCPServer& operator=(const KCPServer&) = delete;

    bool Start();
    void Stop();

    void SetCallbacks(ConnectCallback on_connect,
                      DataCallback on_data,
                      CloseCallback on_close);

    void Use(std::shared_ptr<Middleware> mw);

    AsyncEventLoop* EventLoop() const { return loop_.get(); }

private:
    void OnUDPRecv(const sockaddr_in& addr, const uint8_t* data, size_t len);
    void OnUpdateTimer();
    void CleanupDeadConnections();
    uint32_t AllocateConv();
    static uint64_t MakeAddrKey(const sockaddr_in& addr);
    void SendHandshakeResponse(const sockaddr_in& addr, uint32_t conv);

    Config cfg_;

    std::unique_ptr<AsyncEventLoop> loop_;
    std::thread loop_thread_;

    uv_udp_t* udp_handle_ = nullptr;
    uv_timer_t* update_timer_ = nullptr;

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> conn_id_counter_{0};
    std::atomic<uint32_t> conv_counter_{1};

    // conv → connection
    std::mutex conn_mtx_;
    std::unordered_map<uint32_t, std::shared_ptr<KCPPacketConnection>> conns_by_conv_;

    // (ip,port) hash → conv（用于首次握手包路由）
    std::mutex addr_mtx_;
    std::unordered_map<uint64_t, uint32_t> conv_by_addr_;

    std::vector<std::shared_ptr<Middleware>> middlewares_;

    ConnectCallback on_connect_;
    DataCallback    on_data_;
    CloseCallback   on_close_;
};

} // namespace async
} // namespace net
} // namespace gs
