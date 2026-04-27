#include "ws_connection.hpp"
#include "gs/net/packet.hpp"
#include <cstring>
#include <algorithm>

namespace gs {
namespace gateway {
namespace websocket {

namespace {

bool HeaderContains(const std::string& headers, const char* key, const char* value) {
    size_t pos = 0;
    while (pos < headers.size()) {
        size_t line_end = headers.find("\r\n", pos);
        if (line_end == std::string::npos) break;
        std::string line = headers.substr(pos, line_end - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string hkey = line.substr(0, colon);
            std::transform(hkey.begin(), hkey.end(), hkey.begin(), ::tolower);
            std::string hkey_cmp(key);
            std::transform(hkey_cmp.begin(), hkey_cmp.end(), hkey_cmp.begin(), ::tolower);
            if (hkey == hkey_cmp) {
                std::string hval = line.substr(colon + 1);
                size_t start = hval.find_first_not_of(" \t");
                if (start != std::string::npos) hval = hval.substr(start);
                std::transform(hval.begin(), hval.end(), hval.begin(), ::tolower);
                std::string vcmp(value);
                std::transform(vcmp.begin(), vcmp.end(), vcmp.begin(), ::tolower);
                if (hval.find(vcmp) != std::string::npos) return true;
            }
        }
        pos = line_end + 2;
    }
    return false;
}

std::string ExtractHeader(const std::string& headers, const char* key) {
    size_t pos = 0;
    while (pos < headers.size()) {
        size_t line_end = headers.find("\r\n", pos);
        if (line_end == std::string::npos) break;
        std::string line = headers.substr(pos, line_end - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string hkey = line.substr(0, colon);
            std::transform(hkey.begin(), hkey.end(), hkey.begin(), ::tolower);
            std::string hkey_cmp(key);
            std::transform(hkey_cmp.begin(), hkey_cmp.end(), hkey_cmp.begin(), ::tolower);
            if (hkey == hkey_cmp) {
                std::string hval = line.substr(colon + 1);
                size_t start = hval.find_first_not_of(" \t");
                if (start != std::string::npos) return hval.substr(start);
                return "";
            }
        }
        pos = line_end + 2;
    }
    return "";
}

} // anonymous namespace

// ============================================================

WebSocketConnection::WebSocketConnection(gs::net::async::AsyncEventLoop* loop, uint64_t id)
    : loop_(loop), id_(id) {}

WebSocketConnection::~WebSocketConnection() {
    if (handle_ && !closing_.load()) {
        DoClose();
    }
}

bool WebSocketConnection::InitFromAccepted(uv_tcp_t* client) {
    handle_ = client;
    handle_->data = this;
    StartRead();
    return true;
}

void WebSocketConnection::SetCallbacks(DataCallback on_data, CloseCallback on_close) {
    on_data_ = on_data;
    on_close_ = on_close;
}

void WebSocketConnection::Close() {
    if (closed_.exchange(true)) return;
    if (closing_.exchange(true)) return;
    keep_alive_ = shared_from_this();
    loop_->Post([self = shared_from_this()]() {
        self->DoClose();
    });
}

void WebSocketConnection::DoClose() {
    if (handle_ && !uv_is_closing((uv_handle_t*)handle_)) {
        uv_close((uv_handle_t*)handle_, OnCloseDone);
    }
}

void WebSocketConnection::StartRead() {
    if (!handle_) return;
    uv_read_start((uv_stream_t*)handle_, OnAlloc, OnRead);
}

void WebSocketConnection::OnAlloc(uv_handle_t* /*handle*/, size_t suggested_size, uv_buf_t* buf) {
    static thread_local std::vector<char> tls_buf;
    if (tls_buf.size() < suggested_size) tls_buf.resize(suggested_size);
    buf->base = tls_buf.data();
    buf->len = static_cast<unsigned int>(suggested_size);
}

void WebSocketConnection::OnRead(uv_stream_t* stream, intptr_t nread, const uv_buf_t* buf) {
    auto* conn = static_cast<WebSocketConnection*>(stream->data);
    if (!conn) return;
    if (nread > 0) {
        conn->OnDataReceived(reinterpret_cast<const uint8_t*>(buf->base), static_cast<size_t>(nread));
    } else if (nread < 0) {
        conn->Close();
    }
}

void WebSocketConnection::OnDataReceived(const uint8_t* data, size_t len) {
    if (state_.load() == State::kHandshaking) {
        handshake_buf_.insert(handshake_buf_.end(), data, data + len);
        if (ProcessHandshake()) {
            if (!handshake_buf_.empty()) {
                ws_read_buf_.Append(handshake_buf_.data(), handshake_buf_.size());
            }
            handshake_buf_.clear();
            handshake_buf_.shrink_to_fit();
            state_.store(State::kConnected);
            ProcessWSFrames();
        }
    } else if (state_.load() == State::kConnected) {
        if (ws_read_buf_.Readable() + len > MAX_READ_BUF_BYTES) {
            Close();
            return;
        }
        ws_read_buf_.Append(data, len);
        ProcessWSFrames();
    }
}

bool WebSocketConnection::ProcessHandshake() {
    auto it = std::search(handshake_buf_.begin(), handshake_buf_.end(),
                          std::begin("\r\n\r\n"), std::end("\r\n\r\n") - 1);
    if (it == handshake_buf_.end()) {
        if (handshake_buf_.size() > 8192) {
            Close();
        }
        return false;
    }

    std::string request(handshake_buf_.begin(), it + 4);
    size_t header_end = static_cast<size_t>(it - handshake_buf_.begin()) + 4;
    handshake_buf_.erase(handshake_buf_.begin(), handshake_buf_.begin() + header_end);

    if (!HeaderContains(request, "Upgrade", "websocket") ||
        !HeaderContains(request, "Connection", "upgrade")) {
        Close();
        return false;
    }

    std::string ws_key = ExtractHeader(request, "sec-websocket-key");
    if (ws_key.empty()) {
        Close();
        return false;
    }

    std::string accept = ComputeWSAccept(ws_key);
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n"
        "\r\n";

    SendWSFrame(gs::net::Buffer::FromVector(
        std::vector<uint8_t>(response.begin(), response.end())));
    return true;
}

void WebSocketConnection::ProcessWSFrames() {
    while (state_.load() == State::kConnected) {
        if (ws_read_buf_.Readable() < 2) break;

        // Peek frame header to calculate total length
        uint8_t hdr[2];
        ws_read_buf_.ReadAt(0, hdr, 2);
        bool masked = (hdr[1] & 0x80) != 0;
        uint64_t payload_len = hdr[1] & 0x7F;
        size_t header_len = 2;
        if (payload_len == 126) {
            header_len += 2;
        } else if (payload_len == 127) {
            header_len += 8;
        }
        if (masked) header_len += 4;

        if (payload_len == 126) {
            if (ws_read_buf_.Readable() < header_len) break;
            uint8_t ext[2];
            ws_read_buf_.ReadAt(2, ext, 2);
            payload_len = ((uint64_t)ext[0] << 8) | ext[1];
        } else if (payload_len == 127) {
            if (ws_read_buf_.Readable() < header_len) break;
            uint8_t ext[8];
            ws_read_buf_.ReadAt(2, ext, 8);
            payload_len = 0;
            for (int i = 0; i < 8; ++i) {
                payload_len = (payload_len << 8) | ext[i];
            }
        }

        size_t total_len = header_len + static_cast<size_t>(payload_len);
        if (ws_read_buf_.Readable() < total_len) break;

        // 读取 mask key
        uint8_t mask_key[4] = {0};
        if (masked) {
            size_t mask_offset = header_len - 4;
            ws_read_buf_.ReadAt(mask_offset, mask_key, 4);
        }

        // 处理帧 payload
        size_t payload_offset = header_len;
        uint8_t opcode = hdr[0] & 0x0F;

        switch (opcode) {
            case 0x2: { // binary frame
                if (payload_len >= gs::net::HEADER_SIZE) {
                    // 快速路径：如果整帧连续，直接解析
                    if (ws_read_buf_.IsContiguous(payload_offset, payload_len)) {
                        const uint8_t* p = ws_read_buf_.DataAt(payload_offset);
                        if (masked) UnmaskWSPayload(const_cast<uint8_t*>(p), static_cast<size_t>(payload_len), mask_key);
                        HandleBinaryPayload(p, static_cast<size_t>(payload_len));
                    } else {
                        std::vector<uint8_t> temp(payload_len);
                        ws_read_buf_.ReadAt(payload_offset, temp.data(), payload_len);
                        if (masked) UnmaskWSPayload(temp.data(), temp.size(), mask_key);
                        HandleBinaryPayload(temp.data(), temp.size());
                    }
                }
                break;
            }
            case 0x8: { // close
                Close();
                return;
            }
            case 0x9: { // ping
                SendWSFrame(EncodeWSPongFrame());
                break;
            }
            case 0xA: { // pong
                break;
            }
            case 0x1: { // text frame - ignore
                break;
            }
            default:
                break;
        }

        ws_read_buf_.Consume(total_len);
    }
}

void WebSocketConnection::HandleBinaryPayload(const uint8_t* data, size_t len) {
    uint16_t magic = gs::net::ReadU16BE(data + 4);
    if (magic != gs::net::MAGIC_VALUE) return;

    gs::net::Packet pkt;
    pkt.header.length = static_cast<uint32_t>(len);
    pkt.header.magic = magic;
    pkt.header.cmd_id = gs::net::ReadU32BE(data + 6);
    pkt.header.seq_id = gs::net::ReadU32BE(data + 10);
    pkt.header.flags = gs::net::ReadU32BE(data + 14);
    size_t payload_len = len - gs::net::HEADER_SIZE;
    if (payload_len > 0) {
        pkt.payload = gs::net::Buffer::Allocate(payload_len);
        std::memcpy(pkt.payload.Data(), data + gs::net::HEADER_SIZE, payload_len);
    }
    if (on_data_) {
        on_data_(this, pkt);
    }
}

// ==================== 发送接口 ====================

bool WebSocketConnection::Send(std::vector<uint8_t> data) {
    return Send(gs::net::Buffer::FromVector(std::move(data)));
}

bool WebSocketConnection::Send(const gs::net::Buffer& data) {
    if (closed_.load() || closing_.load() || !handle_) return false;
    auto frame = EncodeWSBinaryFrame(data.Data(), data.Size());
    bool should_pause_read = false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        write_queue_.push(frame);
        write_queue_bytes_ += frame.Size();
        if (write_queue_bytes_ > MAX_WRITE_QUEUE_BYTES && !read_paused_) {
            read_paused_ = true;
            should_pause_read = true;
        }
    }
    if (should_pause_read && handle_) {
        loop_->Post([self = shared_from_this()]() {
            if (self->handle_) uv_read_stop((uv_stream_t*)self->handle_);
        });
    }
    bool expected = false;
    if (writing_.compare_exchange_strong(expected, true)) {
        loop_->Post([self = shared_from_this()]() {
            self->ProcessWriteQueue();
        });
    }
    return true;
}

bool WebSocketConnection::SendBatch(const std::vector<gs::net::Buffer>& buffers) {
    if (closed_.load() || closing_.load() || !handle_ || buffers.empty()) return false;
    bool should_pause_read = false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        for (const auto& b : buffers) {
            auto frame = EncodeWSBinaryFrame(b.Data(), b.Size());
            write_queue_bytes_ += frame.Size();
            write_queue_.push(std::move(frame));
        }
        if (write_queue_bytes_ > MAX_WRITE_QUEUE_BYTES && !read_paused_) {
            read_paused_ = true;
            should_pause_read = true;
        }
    }
    if (should_pause_read && handle_) {
        loop_->Post([self = shared_from_this()]() {
            if (self->handle_) uv_read_stop((uv_stream_t*)self->handle_);
        });
    }
    bool expected = false;
    if (writing_.compare_exchange_strong(expected, true)) {
        loop_->Post([self = shared_from_this()]() {
            self->ProcessWriteQueue();
        });
    }
    return true;
}

bool WebSocketConnection::SendPacket(const gs::net::Packet& pkt) {
    auto data = gs::net::EncodePacket(pkt);
    return Send(data);
}

bool WebSocketConnection::SendFrameBuffer(const gs::net::Buffer& frame) {
    if (closed_.load() || closing_.load() || !handle_) return false;
    bool should_pause_read = false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        write_queue_.push(frame.Slice(0, frame.Size()));
        write_queue_bytes_ += frame.Size();
        if (write_queue_bytes_ > MAX_WRITE_QUEUE_BYTES && !read_paused_) {
            read_paused_ = true;
            should_pause_read = true;
        }
    }
    if (should_pause_read && handle_) {
        loop_->Post([self = shared_from_this()]() {
            if (self->handle_) uv_read_stop((uv_stream_t*)self->handle_);
        });
    }
    bool expected = false;
    if (writing_.compare_exchange_strong(expected, true)) {
        loop_->Post([self = shared_from_this()]() {
            self->ProcessWriteQueue();
        });
    }
    return true;
}

void WebSocketConnection::SendWSFrame(gs::net::Buffer frame) {
    if (closed_.load() || closing_.load() || !handle_) return;
    bool should_pause_read = false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        write_queue_.push(std::move(frame));
        write_queue_bytes_ += write_queue_.back().Size();
        if (write_queue_bytes_ > MAX_WRITE_QUEUE_BYTES && !read_paused_) {
            read_paused_ = true;
            should_pause_read = true;
        }
    }
    if (should_pause_read && handle_) {
        loop_->Post([self = shared_from_this()]() {
            if (self->handle_) uv_read_stop((uv_stream_t*)self->handle_);
        });
    }
    bool expected = false;
    if (writing_.compare_exchange_strong(expected, true)) {
        loop_->Post([self = shared_from_this()]() {
            self->ProcessWriteQueue();
        });
    }
}

void WebSocketConnection::ProcessWriteQueue() {
    if (!handle_ || uv_is_closing((uv_handle_t*)handle_)) {
        writing_.store(false);
        return;
    }

    constexpr size_t MAX_BATCH = 64;
    std::vector<gs::net::Buffer> batch;
    batch.reserve(MAX_BATCH);
    static thread_local std::vector<uv_buf_t> tls_ubufs;
    tls_ubufs.clear();

    bool should_resume_read = false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        while (!write_queue_.empty() && batch.size() < MAX_BATCH) {
            auto& front = write_queue_.front();
            write_queue_bytes_ -= front.Size();
            batch.push_back(std::move(front));
            write_queue_.pop();
        }
        if (batch.empty()) {
            writing_.store(false);
            return;
        }
        if (read_paused_ && write_queue_bytes_ < WRITE_RESUME_THRESHOLD) {
            read_paused_ = false;
            should_resume_read = true;
        }
    }

    if (should_resume_read && handle_) {
        uv_read_start((uv_stream_t*)handle_, OnAlloc, OnRead);
    }

    auto* req = new uv_write_t;
    req->data = this;
    for (auto& b : batch) {
        uv_buf_t uvbuf;
        uvbuf.base = reinterpret_cast<char*>(const_cast<uint8_t*>(b.Data()));
        uvbuf.len = static_cast<unsigned int>(b.Size());
        tls_ubufs.push_back(uvbuf);
    }

    int r = uv_write(req, (uv_stream_t*)handle_, tls_ubufs.data(),
                     static_cast<unsigned int>(tls_ubufs.size()), OnWriteDone);
    if (r != 0) {
        delete req;
        writing_.store(false);
        Close();
    }
}

void WebSocketConnection::OnWriteDone(uv_write_t* req, int status) {
    auto* conn = static_cast<WebSocketConnection*>(req->data);
    delete req;
    if (!conn) return;
    if (status < 0) {
        conn->Close();
        return;
    }
    conn->ProcessWriteQueue();
}

void WebSocketConnection::OnCloseDone(uv_handle_t* handle) {
    auto* conn = static_cast<WebSocketConnection*>(handle->data);
    delete (uv_tcp_t*)handle;
    if (!conn) return;
    auto cb = std::move(conn->on_close_);
    conn->handle_ = nullptr;
    conn->state_.store(State::kClosed);
    if (cb) cb(conn);
    conn->keep_alive_.reset();
}

} // namespace websocket
} // namespace gateway
} // namespace gs
