#pragma once

#include "tcp_conn.hpp"
#include "packet.hpp"
#include <functional>
#include <memory>

namespace gs {
namespace net {

// AsyncEventLoop 异步事件循环抽象接口
// Phase 5 将在此接口上实现基于 epoll/kqueue/IOCP/io_uring 的完整异步 I/O
// 当前阶段作为架构占位，保持与阻塞模型的兼容性
class AsyncEventLoop {
public:
    using ReadCallback  = std::function<void(TCPConn*, Packet&)>;
    using WriteCallback = std::function<void(TCPConn*)>;
    using ErrorCallback = std::function<void(TCPConn*, const char*)>;

    virtual ~AsyncEventLoop() = default;

    // 注册一个 socket 到事件循环（非阻塞模式）
    virtual bool Register(SocketType sock, TCPConn* conn) = 0;

    // 注销一个 socket
    virtual void Unregister(SocketType sock) = 0;

    // 启动事件循环（阻塞调用，直到 Stop）
    virtual void Run() = 0;

    // 停止事件循环
    virtual void Stop() = 0;

    // 工厂方法：创建平台适配的事件循环
    static std::unique_ptr<AsyncEventLoop> Create();
};

} // namespace net
} // namespace gs
