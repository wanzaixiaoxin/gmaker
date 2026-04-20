#pragma once

#include "tcp_connection.hpp"
#include <string>
#include <functional>
#include <memory>

struct uv_tcp_s;
using uv_tcp_t = uv_tcp_s;

namespace gs {
namespace net {
namespace async {

class AsyncEventLoop;

// AsyncTCPListener 基于 libuv 的异步 TCP 监听器
class AsyncTCPListener {
public:
    // 回调参数：已 uv_accept 成功的 client uv_tcp_t*，由上层创建 AsyncTCPConnection
    using ConnectionCallback = std::function<void(uv_tcp_t* client)>;

    AsyncTCPListener(AsyncEventLoop* loop);
    ~AsyncTCPListener();

    AsyncTCPListener(const AsyncTCPListener&) = delete;
    AsyncTCPListener& operator=(const AsyncTCPListener&) = delete;

    // 绑定并监听地址
    bool Listen(const std::string& host, uint16_t port);
    void Stop();

    void SetConnectionCallback(ConnectionCallback cb) { on_conn_ = std::move(cb); }

private:
    static void OnConnection(uv_stream_t* server, int status);

    AsyncEventLoop* loop_ = nullptr;
    uv_tcp_t* handle_ = nullptr;
    ConnectionCallback on_conn_;
};

} // namespace async
} // namespace net
} // namespace gs
