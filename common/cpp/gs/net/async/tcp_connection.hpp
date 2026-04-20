#pragma once

#include "../packet.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <queue>
#include <uv.h>

namespace gs {
namespace net {
namespace async {

class AsyncEventLoop;

// AsyncTCPConnection 基于 libuv 的异步 TCP 连接
// 与现有 TCPConn 保持接口兼容，未来可无缝替换
class AsyncTCPConnection : public std::enable_shared_from_this<AsyncTCPConnection> {
public:
    using DataCallback = std::function<void(AsyncTCPConnection*, Packet&)>;
    using CloseCallback = std::function<void(AsyncTCPConnection*)>;

    AsyncTCPConnection(AsyncEventLoop* loop, uint64_t id);
    ~AsyncTCPConnection();

    // 禁止拷贝
    AsyncTCPConnection(const AsyncTCPConnection&) = delete;
    AsyncTCPConnection& operator=(const AsyncTCPConnection&) = delete;

    // 从已接受的 uv_tcp_t 初始化（由 Listener 调用）
    bool InitFromAccepted(uv_tcp_t* client);

    uint64_t ID() const { return id_; }

    void SetCallbacks(DataCallback on_data, CloseCallback on_close);

    // 设置会话密钥，启用 AES-GCM 加密通信
    void SetSessionKey(const std::vector<uint8_t>& key);

    // 异步发送字节流（线程安全）
    bool Send(std::vector<uint8_t> data);

    // 发送一个 Packet（若有 sessionKey 则自动加密）
    bool SendPacket(const Packet& pkt);

    // 关闭连接（线程安全）
    void Close();

    bool IsClosed() const { return closed_.load(); }

    // 获取底层 uv_tcp_t（仅用于高级操作）
    uv_tcp_t* RawHandle() const { return handle_; }

private:
    static void OnAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf);
    static void OnWriteDone(uv_write_t* req, int status);
    static void OnCloseDone(uv_handle_t* handle);

    void StartRead();
    void ProcessReadBuffer();
    void ProcessWriteQueue();
    void DoClose();

    // 内部写请求结构（定义在 cpp 中，避免暴露 libuv 细节）
    struct WriteReq;

    AsyncEventLoop* loop_ = nullptr;
    uv_tcp_t* handle_ = nullptr;
    uint64_t id_ = 0;

    std::atomic<bool> closed_{false};
    std::atomic<bool> closing_{false};

    DataCallback on_data_;
    CloseCallback on_close_;

    std::mutex read_mtx_;
    std::vector<uint8_t> read_buf_;

    std::mutex write_mtx_;
    std::queue<std::vector<uint8_t>> write_queue_;
    std::atomic<bool> writing_{false};

    std::vector<uint8_t> session_key_;

    // 用于在 uv_close 完成前保持对象存活
    std::shared_ptr<AsyncTCPConnection> keep_alive_;
};

} // namespace async
} // namespace net
} // namespace gs
