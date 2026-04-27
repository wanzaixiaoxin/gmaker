#pragma once

#include "gs/net/iconnection.hpp"
#include "gs/net/packet.hpp"
#include "gs/net/ring_buffer.hpp"
#include "gs/net/async/event_loop.hpp"
#include "ws_frame.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>
#include <queue>
#include <uv.h>

namespace gs {
namespace gateway {
namespace websocket {

// WebSocket 连接，实现 IConnection 接口
// 内部状态机: Handshaking -> Connected -> Closed
class WebSocketConnection : public gs::net::IConnection,
                            public std::enable_shared_from_this<WebSocketConnection> {
public:
    using DataCallback = std::function<void(WebSocketConnection*, gs::net::Packet&)>;
    using CloseCallback = std::function<void(WebSocketConnection*)>;

    WebSocketConnection(gs::net::async::AsyncEventLoop* loop, uint64_t id);
    ~WebSocketConnection();

    WebSocketConnection(const WebSocketConnection&) = delete;
    WebSocketConnection& operator=(const WebSocketConnection&) = delete;

    // 从已接受的 uv_tcp_t 初始化
    bool InitFromAccepted(uv_tcp_t* client);

    uint64_t ID() const override { return id_; }

    void SetCallbacks(DataCallback on_data, CloseCallback on_close);

    // IConnection 接口
    void Close() override;
    bool Send(std::vector<uint8_t> data) override;
    bool Send(const gs::net::Buffer& data) override;
    bool SendBatch(const std::vector<gs::net::Buffer>& buffers) override;
    bool SendPacket(const gs::net::Packet& pkt) override;

    bool IsClosed() const { return closed_.load(); }
    uv_tcp_t* RawHandle() const { return handle_; }

    // 直接发送已编码的 WebSocket frame（用于广播零拷贝共享）
    bool SendFrameBuffer(const gs::net::Buffer& frame);

private:
    enum class State {
        kHandshaking,
        kConnected,
        kClosing,
        kClosed
    };

    static void OnAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void OnRead(uv_stream_t* stream, intptr_t nread, const uv_buf_t* buf);
    static void OnWriteDone(uv_write_t* req, int status);
    static void OnCloseDone(uv_handle_t* handle);

    void StartRead();
    void DoClose();
    void OnDataReceived(const uint8_t* data, size_t len);
    bool ProcessHandshake();
    void ProcessWSFrames();
    void SendWSFrame(gs::net::Buffer frame);
    void HandleBinaryPayload(const uint8_t* data, size_t len);
    void ProcessWriteQueue();

    gs::net::async::AsyncEventLoop* loop_ = nullptr;
    uv_tcp_t* handle_ = nullptr;
    uint64_t id_ = 0;

    std::atomic<bool> closed_{false};
    std::atomic<bool> closing_{false};
    std::atomic<State> state_{State::kHandshaking};

    DataCallback on_data_;
    CloseCallback on_close_;

    // 握手阶段缓冲
    std::vector<uint8_t> handshake_buf_;

    // WebSocket 阶段缓冲（使用 RingBuffer 避免数据移动）
    gs::net::RingBuffer ws_read_buf_;

    // 写队列（线程安全）
    std::mutex write_mtx_;
    std::queue<gs::net::Buffer> write_queue_;
    size_t write_queue_bytes_ = 0;
    std::atomic<bool> writing_{false};

    static constexpr size_t MAX_WRITE_QUEUE_BYTES = 16 * 1024 * 1024;
    static constexpr size_t WRITE_RESUME_THRESHOLD = 8 * 1024 * 1024;
    static constexpr size_t MAX_READ_BUF_BYTES = 64 * 1024 * 1024;

    bool read_paused_ = false;

    std::shared_ptr<WebSocketConnection> keep_alive_;
};

} // namespace websocket
} // namespace gateway
} // namespace gs
