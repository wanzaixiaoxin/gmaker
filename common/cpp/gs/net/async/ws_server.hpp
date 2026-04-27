#pragma once

#include "ws_connection.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace gs {
namespace net {
namespace async {
class AsyncEventLoop;
}
}
}

namespace gs {
namespace net {
namespace websocket {

class WebSocketServer {
public:
    using ConnectCallback = std::function<void(WebSocketConnection*)>;
    using MessageCallback = std::function<void(WebSocketConnection*, MessageType, gs::net::Buffer&)>;
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
                      MessageCallback on_message,
                      CloseCallback on_close);

    void Broadcast(MessageType type, const gs::net::Buffer& message);
    void BroadcastBinary(const gs::net::Buffer& message);
    size_t ConnectionCount() const;

private:
    void OnAccept(uv_tcp_t* client);
    void OnConnMessage(WebSocketConnection* conn, MessageType type, gs::net::Buffer& message);
    void OnConnClose(WebSocketConnection* conn);
    void RunEventLoop();

    Config cfg_;

    std::unique_ptr<gs::net::async::AsyncEventLoop> loop_;
    uv_tcp_t* listen_handle_ = nullptr;
    std::thread loop_thread_;

    std::vector<std::unique_ptr<gs::net::async::AsyncEventLoop>> worker_loops_;
    std::vector<std::thread> worker_threads_;
    std::atomic<size_t> next_worker_{0};

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> conn_id_counter_{0};

    mutable std::mutex conn_mtx_;
    std::unordered_map<uint64_t, std::shared_ptr<WebSocketConnection>> conns_;

    ConnectCallback on_connect_;
    MessageCallback on_message_;
    CloseCallback   on_close_;
};

} // namespace websocket
} // namespace net
} // namespace gs
