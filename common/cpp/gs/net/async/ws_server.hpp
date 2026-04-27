#pragma once

#include "ws_connection.hpp"
#include "gs/net/middleware.hpp"
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
}
}
}

namespace gs {
namespace gateway {
namespace websocket {

// 基于 libuv 的 WebSocket 服务器（多 Worker Loop，与 AsyncTCPServer 对齐）
class WebSocketServer {
public:
    using ConnectCallback = std::function<void(WebSocketConnection*)>;
    using DataCallback    = std::function<void(WebSocketConnection*, gs::net::Packet&)>;
    using CloseCallback   = std::function<void(WebSocketConnection*)>;

    struct Config {
        std::string host = "0.0.0.0";
        uint16_t    port = 0;
        int         max_conn = 10000;
    };

    explicit WebSocketServer(const Config& cfg);
    ~WebSocketServer();

    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    bool Start();
    void Stop();

    void SetCallbacks(ConnectCallback on_connect,
                      DataCallback on_data,
                      CloseCallback on_close);

    void Use(std::shared_ptr<gs::net::Middleware> mw);

    // 广播给所有 WebSocket 连接（零拷贝共享 frame）
    void Broadcast(const gs::net::Packet& pkt);

private:
    void OnAccept(uv_tcp_t* client);
    void OnConnData(WebSocketConnection* conn, gs::net::Packet& pkt);
    void OnConnClose(WebSocketConnection* conn);
    void RunEventLoop();

    Config cfg_;

    // Accept loop
    std::unique_ptr<gs::net::async::AsyncEventLoop> loop_;
    uv_tcp_t* listen_handle_ = nullptr;
    std::thread loop_thread_;

    // Worker loops（Round-Robin 分发，数量 = CPU 核心数）
    std::vector<std::unique_ptr<gs::net::async::AsyncEventLoop>> worker_loops_;
    std::vector<std::thread> worker_threads_;
    std::atomic<size_t> next_worker_{0};

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> conn_id_counter_{0};

    std::mutex conn_mtx_;
    std::unordered_map<uint64_t, std::shared_ptr<WebSocketConnection>> conns_;

    std::vector<std::shared_ptr<gs::net::Middleware>> middlewares_;

    ConnectCallback on_connect_;
    DataCallback    on_data_;
    CloseCallback   on_close_;
};

} // namespace websocket
} // namespace gateway
} // namespace gs
