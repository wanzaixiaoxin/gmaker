#pragma once

#include "gs/net/async/event_loop.hpp"
#include "gs/net/buffer.hpp"
#include "gs/net/ring_buffer.hpp"
#include "ws_frame.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include <uv.h>

namespace gs {
namespace net {
namespace websocket {

class WebSocketConnection : public std::enable_shared_from_this<WebSocketConnection> {
public:
    using MessageCallback = std::function<void(WebSocketConnection*, MessageType, gs::net::Buffer&)>;
    using CloseCallback = std::function<void(WebSocketConnection*)>;

    WebSocketConnection(gs::net::async::AsyncEventLoop* loop, uint64_t id);
    ~WebSocketConnection();

    WebSocketConnection(const WebSocketConnection&) = delete;
    WebSocketConnection& operator=(const WebSocketConnection&) = delete;

    bool InitFromAccepted(uv_tcp_t* client);

    uint64_t ID() const { return id_; }

    void SetCallbacks(MessageCallback on_message, CloseCallback on_close);

    void Close();
    bool Send(std::vector<uint8_t> data);
    bool Send(const gs::net::Buffer& data);
    bool SendBatch(const std::vector<gs::net::Buffer>& buffers);
    bool SendMessage(MessageType type, const gs::net::Buffer& data);
    bool SendMessageBatch(MessageType type, const std::vector<gs::net::Buffer>& messages);
    bool SendFrameBuffer(const gs::net::Buffer& frame);

    bool IsClosed() const { return closed_.load(); }
    uv_tcp_t* RawHandle() const { return handle_; }

private:
    enum class State {
        kHandshaking,
        kConnected,
        kClosing,
        kClosed
    };

    struct WriteReq {
        uv_write_t req;
        std::shared_ptr<WebSocketConnection> conn;
        std::vector<gs::net::Buffer> buffers;
    };

    static void OnAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
    static void OnRead(uv_stream_t* stream, intptr_t nread, const uv_buf_t* buf);
    static void OnWriteDone(uv_write_t* req, int status);
    static void OnTimer(uv_timer_t* timer);
    static void OnCloseDone(uv_handle_t* handle);

    void StartRead();
    void DoClose();
    void OnDataReceived(const uint8_t* data, size_t len);
    bool ProcessHandshake();
    void ProcessWSFrames();
    void QueueFrame(gs::net::Buffer frame);
    void QueueFrameParts(std::vector<gs::net::Buffer> parts);
    void ProcessWriteQueue();
    bool IsWritable() const;
    void FailProtocol(uint16_t code);
    void SendCloseAndDrain(uint16_t code);
    bool StartTimer();
    void StopTimer();
    void CheckLiveness();
    uint64_t NowMs() const;
    void MarkFrameActivity();

    gs::net::async::AsyncEventLoop* loop_ = nullptr;
    uv_tcp_t* handle_ = nullptr;
    uv_timer_t* timer_ = nullptr;
    uint64_t id_ = 0;

    std::atomic<bool> closed_{false};
    std::atomic<bool> closing_{false};
    std::atomic<bool> close_after_write_{false};
    std::atomic<State> state_{State::kHandshaking};

    MessageCallback on_message_;
    CloseCallback on_close_;

    std::vector<uint8_t> handshake_buf_;
    gs::net::RingBuffer ws_read_buf_;

    std::mutex write_mtx_;
    std::queue<gs::net::Buffer> write_queue_;
    size_t write_queue_bytes_ = 0;
    std::atomic<bool> writing_{false};
    std::unique_ptr<WriteReq> write_req_;

    std::vector<uint8_t> fragmented_payload_;
    uint8_t fragmented_opcode_ = 0;
    bool fragmenting_ = false;

    uint64_t accepted_at_ms_ = 0;
    uint64_t last_activity_ms_ = 0;
    uint64_t last_ping_ms_ = 0;
    uint64_t ping_deadline_ms_ = 0;
    bool ping_outstanding_ = false;

    static constexpr size_t MAX_WRITE_QUEUE_BYTES = 16 * 1024 * 1024;
    static constexpr size_t WRITE_RESUME_THRESHOLD = 8 * 1024 * 1024;
    static constexpr size_t MAX_READ_BUF_BYTES = 64 * 1024 * 1024;
    static constexpr size_t MAX_HANDSHAKE_BYTES = 8192;
    static constexpr size_t MAX_MESSAGE_BYTES = 16 * 1024 * 1024;
    static constexpr uint64_t TIMER_INTERVAL_MS = 1000;
    static constexpr uint64_t HANDSHAKE_TIMEOUT_MS = 10000;
    static constexpr uint64_t IDLE_TIMEOUT_MS = 120000;
    static constexpr uint64_t PING_INTERVAL_MS = 30000;
    static constexpr uint64_t PONG_TIMEOUT_MS = 15000;

    bool read_paused_ = false;

    std::shared_ptr<WebSocketConnection> keep_alive_;
};

} // namespace websocket
} // namespace net
} // namespace gs
