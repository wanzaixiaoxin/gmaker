#pragma once

#include "../packet.hpp"
#include "../iconnection.hpp"
#include "event_loop.hpp"
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>

struct uv_timer_s;
using uv_timer_t = uv_timer_s;

namespace gs {
namespace net {
namespace async {

// AsyncWriteCoalescer 异步写聚合器
// - 基于 uv_timer_t 替代后台 sleep 线程
// - 将同一帧内多个 Packet 合并为一次 write（底层仍各自发送，但统一在 timer callback 中 flush）
// - 适用于广播场景
class AsyncWriteCoalescer {
public:
    explicit AsyncWriteCoalescer(AsyncEventLoop* loop, int interval_ms = 16);
    ~AsyncWriteCoalescer();

    bool Start();
    void Stop();

    // 添加一个包到发送缓冲（线程安全）
    void Enqueue(IConnection* conn, const Packet& pkt);

    // 添加已编码的 Buffer 到发送缓冲（线程安全，零拷贝共享场景）
    void Enqueue(IConnection* conn, const Buffer& data);

    // 广播：将同一包发给多个连接
    void Broadcast(const std::vector<IConnection*>& conns, const Packet& pkt);

    // 供内部 uv_timer_t 回调调用
    void OnTimer();

private:
    void DoFlush();

    struct Pending {
        IConnection* conn = nullptr;
        Buffer data; // 已编码的 packet bytes
    };

    AsyncEventLoop* loop_ = nullptr;
    int interval_ms_ = 16;
    uv_timer_t* timer_ = nullptr;

    std::mutex mtx_;
    std::vector<Pending> pending_;

    std::atomic<bool> running_{false};
};

} // namespace async
} // namespace net
} // namespace gs
