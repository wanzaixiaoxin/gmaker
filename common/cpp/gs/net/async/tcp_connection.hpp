#pragma once

#include "../iconnection.hpp"
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
// 实现 IConnection 接口，与上层 Middleware 解耦
class AsyncTCPConnection : public IConnection, public std::enable_shared_from_this<AsyncTCPConnection> {
public:
    using DataCallback = std::function<void(AsyncTCPConnection*, Packet&)>;
    using CloseCallback = std::function<void(AsyncTCPConnection*)>;
    using ConnectCallback = std::function<void(bool success)>;

    AsyncTCPConnection(AsyncEventLoop* loop, uint64_t id);
    ~AsyncTCPConnection();  // 定义在 cpp 中，因为 unique_ptr<WriteReq> 需要完整类型

    // 禁止拷贝
    AsyncTCPConnection(const AsyncTCPConnection&) = delete;
    AsyncTCPConnection& operator=(const AsyncTCPConnection&) = delete;

    // 从已接受的 uv_tcp_t 初始化（由 Listener 调用）
    bool InitFromAccepted(uv_tcp_t* client);

    // 作为客户端主动连接
    bool Connect(const std::string& host, uint16_t port);

    uint64_t ID() const override { return id_; }

    void SetCallbacks(DataCallback on_data, CloseCallback on_close);
    void SetConnectCallback(ConnectCallback cb);

    // 设置会话密钥，启用 AES-GCM 加密通信
    void SetSessionKey(const std::vector<uint8_t>& key);

    // 异步发送字节流（线程安全）
    bool Send(std::vector<uint8_t> data);

    // 发送 Buffer（线程安全，支持零拷贝共享）
    bool Send(const Buffer& data);

    // 批量发送 Buffer（减少跨线程 Post 次数）
    bool SendBatch(const std::vector<Buffer>& buffers);

    // 发送一个 Packet（若有 sessionKey 则自动加密）
    bool SendPacket(const Packet& pkt);

    // 关闭连接（线程安全）
    void Close() override;

    bool IsClosed() const { return closed_.load(); }

    // 获取底层 uv_tcp_t（仅用于高级操作）
    uv_tcp_t* RawHandle() const { return handle_; }

    // 判断是否已连接（客户端模式）
    bool IsConnected() const { return connected_.load(); }

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
    std::unique_ptr<WriteReq> write_req_;  // 预分配，避免每次 new/delete

    AsyncEventLoop* loop_ = nullptr;
    uv_tcp_t* handle_ = nullptr;
    uint64_t id_ = 0;

    std::atomic<bool> closed_{false};
    std::atomic<bool> closing_{false};
    std::atomic<bool> connected_{false};

    DataCallback on_data_;
    CloseCallback on_close_;
    ConnectCallback on_connect_;

    // 环形读缓冲区：替代 vector + memmove compact
    class RingBuffer {
    public:
        explicit RingBuffer(size_t cap = 1024 * 1024);  // 1MB 初始容量，减少扩容
        void Append(const uint8_t* data, size_t len);
        size_t Readable() const;
        bool IsContiguous(size_t offset, size_t len) const;
        const uint8_t* DataAt(size_t offset) const;
        void ReadAt(size_t offset, uint8_t* out, size_t len) const;
        void Consume(size_t len);
    private:
        void EnsureSpace(size_t len);
        std::vector<uint8_t> buf_;
        size_t rpos_ = 0;
        size_t wpos_ = 0;
        size_t size_ = 0;
    };

    std::mutex read_mtx_;
    RingBuffer read_ring_buf_;
    bool read_paused_ = false;

    // 写队列（批量 gather write）
    std::mutex write_mtx_;
    std::queue<Buffer> write_queue_;
    size_t write_queue_bytes_ = 0;
    std::atomic<bool> writing_{false};

    // 背压 / 安全阈值
    static constexpr size_t MAX_READ_BUF_BYTES = 64 * 1024 * 1024;   // 64MB
    static constexpr size_t MAX_WRITE_QUEUE_BYTES = 16 * 1024 * 1024;  // 16MB
    static constexpr size_t WRITE_RESUME_THRESHOLD = 8 * 1024 * 1024;  // 8MB

    std::vector<uint8_t> session_key_;

    // 用于在 uv_close 完成前保持对象存活
    std::shared_ptr<AsyncTCPConnection> keep_alive_;
};

// unique_ptr<WriteReq> 的析构在 AsyncTCPConnection 的 cpp 析构函数中实例化

} // namespace async
} // namespace net
} // namespace gs
