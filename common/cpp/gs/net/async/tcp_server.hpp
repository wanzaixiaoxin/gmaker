#pragma once

#include "tcp_listener.hpp"
#include "tcp_connection.hpp"
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

// AsyncTCPServer 基于 libuv 的高层 TCP 服务器
// 与现有 TCPServer 接口对齐，未来可无缝替换
class AsyncTCPServer {
public:
    using ConnectCallback = std::function<void(AsyncTCPConnection*)>;
    using DataCallback    = std::function<void(AsyncTCPConnection*, Packet&)>;
    using CloseCallback   = std::function<void(AsyncTCPConnection*)>;

    struct Config {
        std::string host = "0.0.0.0";
        uint16_t    port = 0;
        int         max_conn = 10000;
    };

    AsyncTCPServer(const Config& cfg);
    ~AsyncTCPServer();

    AsyncTCPServer(const AsyncTCPServer&) = delete;
    AsyncTCPServer& operator=(const AsyncTCPServer&) = delete;

    // 启动服务器（非阻塞，内部启动事件循环线程）
    bool Start();
    void Stop();

    void SetCallbacks(ConnectCallback on_connect,
                      DataCallback on_data,
                      CloseCallback on_close);

    // 注册中间件（按注册顺序执行，返回 false 则拦截）
    void Use(std::shared_ptr<Middleware> mw);

    // 广播给所有连接
    void Broadcast(const Packet& pkt);

private:
    void OnAccept(uv_tcp_t* client);
    void OnConnData(AsyncTCPConnection* conn, Packet& pkt);
    void OnConnClose(AsyncTCPConnection* conn);
    void RunEventLoop();

    Config cfg_;
    std::unique_ptr<AsyncEventLoop> loop_;
    std::unique_ptr<AsyncTCPListener> listener_;
    std::thread loop_thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> conn_id_counter_{0};

    std::mutex conn_mtx_;
    std::unordered_map<uint64_t, std::shared_ptr<AsyncTCPConnection>> conns_;

    std::vector<std::shared_ptr<Middleware>> middlewares_;

    ConnectCallback on_connect_;
    DataCallback    on_data_;
    CloseCallback   on_close_;
};

} // namespace async
} // namespace net
} // namespace gs
