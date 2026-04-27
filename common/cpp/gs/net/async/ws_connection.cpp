#include "ws_connection.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace gs {
namespace net {
namespace websocket {

namespace {

std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return s;
}

std::string Trim(std::string s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool HeaderContains(const std::string& headers, const char* key, const char* value) {
    const std::string key_cmp = ToLower(key);
    const std::string value_cmp = ToLower(value);
    size_t pos = 0;
    while (pos < headers.size()) {
        size_t line_end = headers.find("\r\n", pos);
        if (line_end == std::string::npos) break;
        std::string line = headers.substr(pos, line_end - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string hkey = ToLower(line.substr(0, colon));
            if (hkey == key_cmp) {
                std::string hval = ToLower(Trim(line.substr(colon + 1)));
                if (hval.find(value_cmp) != std::string::npos) return true;
            }
        }
        pos = line_end + 2;
    }
    return false;
}

std::string ExtractHeader(const std::string& headers, const char* key) {
    const std::string key_cmp = ToLower(key);
    size_t pos = 0;
    while (pos < headers.size()) {
        size_t line_end = headers.find("\r\n", pos);
        if (line_end == std::string::npos) break;
        std::string line = headers.substr(pos, line_end - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string hkey = ToLower(line.substr(0, colon));
            if (hkey == key_cmp) {
                return Trim(line.substr(colon + 1));
            }
        }
        pos = line_end + 2;
    }
    return "";
}

bool IsHttpGetUpgrade(const std::string& request) {
    size_t line_end = request.find("\r\n");
    if (line_end == std::string::npos) return false;
    std::string line = request.substr(0, line_end);
    return line.rfind("GET ", 0) == 0 && line.find(" HTTP/1.1") != std::string::npos;
}

bool IsControlOpcode(uint8_t opcode) {
    return opcode == OPCODE_CLOSE || opcode == OPCODE_PING || opcode == OPCODE_PONG;
}

bool IsMessageOpcode(uint8_t opcode) {
    return opcode == OPCODE_TEXT || opcode == OPCODE_BINARY;
}

MessageType ToMessageType(uint8_t opcode) {
    return opcode == OPCODE_TEXT ? MessageType::Text : MessageType::Binary;
}

} // namespace

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

void WebSocketConnection::SetCallbacks(MessageCallback on_message, CloseCallback on_close) {
    on_message_ = std::move(on_message);
    on_close_ = std::move(on_close);
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
    closing_.store(true);
    state_.store(State::kClosing);
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
        if (handshake_buf_.size() + len > MAX_HANDSHAKE_BYTES) {
            Close();
            return;
        }
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
            FailProtocol(1009);
            return;
        }
        ws_read_buf_.Append(data, len);
        ProcessWSFrames();
    }
}

bool WebSocketConnection::ProcessHandshake() {
    auto it = std::search(handshake_buf_.begin(), handshake_buf_.end(),
                          std::begin("\r\n\r\n"), std::end("\r\n\r\n") - 1);
    if (it == handshake_buf_.end()) return false;

    std::string request(handshake_buf_.begin(), it + 4);
    size_t header_end = static_cast<size_t>(it - handshake_buf_.begin()) + 4;
    handshake_buf_.erase(handshake_buf_.begin(), handshake_buf_.begin() + header_end);

    if (!IsHttpGetUpgrade(request) ||
        !HeaderContains(request, "Upgrade", "websocket") ||
        !HeaderContains(request, "Connection", "upgrade")) {
        Close();
        return false;
    }

    std::string ws_key = ExtractHeader(request, "sec-websocket-key");
    std::string version = ExtractHeader(request, "sec-websocket-version");
    if (ws_key.empty() || version != "13") {
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

    QueueFrame(gs::net::Buffer::FromVector(std::vector<uint8_t>(response.begin(), response.end())));
    return true;
}

void WebSocketConnection::ProcessWSFrames() {
    while (state_.load() == State::kConnected) {
        size_t readable = ws_read_buf_.Readable();
        if (readable < 2) break;

        WSFrame frame;
        uint8_t hdr[2];
        ws_read_buf_.ReadAt(0, hdr, 2);
        frame.fin = (hdr[0] & 0x80) != 0;
        frame.opcode = hdr[0] & 0x0F;
        frame.masked = (hdr[1] & 0x80) != 0;
        uint64_t payload_len = hdr[1] & 0x7F;
        size_t header_len = 2;

        if ((hdr[0] & 0x70) != 0 || !frame.masked) {
            FailProtocol(1002);
            return;
        }

        if (payload_len == 126) {
            if (readable < 4) break;
            uint8_t ext[2];
            ws_read_buf_.ReadAt(2, ext, 2);
            payload_len = ((uint64_t)ext[0] << 8) | ext[1];
            if (payload_len < 126) {
                FailProtocol(1002);
                return;
            }
            header_len = 4;
        } else if (payload_len == 127) {
            if (readable < 10) break;
            uint8_t ext[8];
            ws_read_buf_.ReadAt(2, ext, 8);
            if ((ext[0] & 0x80) != 0) {
                FailProtocol(1002);
                return;
            }
            payload_len = 0;
            for (int i = 0; i < 8; ++i) {
                payload_len = (payload_len << 8) | ext[i];
            }
            if (payload_len < 65536) {
                FailProtocol(1002);
                return;
            }
            header_len = 10;
        }

        header_len += 4;
        if (payload_len > MAX_MESSAGE_BYTES || payload_len > static_cast<uint64_t>(SIZE_MAX - header_len)) {
            FailProtocol(1009);
            return;
        }

        size_t total_len = header_len + static_cast<size_t>(payload_len);
        if (readable < total_len) break;

        size_t payload_offset = header_len;
        frame.payload_len = payload_len;
        ws_read_buf_.ReadAt(header_len - 4, frame.mask_key, 4);

        uint8_t opcode = frame.opcode;
        if (opcode != OPCODE_CONTINUATION &&
            opcode != OPCODE_TEXT &&
            opcode != OPCODE_BINARY &&
            opcode != OPCODE_CLOSE &&
            opcode != OPCODE_PING &&
            opcode != OPCODE_PONG) {
            FailProtocol(1002);
            return;
        }

        bool is_control = opcode == OPCODE_CLOSE || opcode == OPCODE_PING || opcode == OPCODE_PONG;
        if (is_control && (!frame.fin || payload_len > 125)) {
            FailProtocol(1002);
            return;
        }

        std::vector<uint8_t> payload_vec;
        payload_vec.resize(static_cast<size_t>(payload_len));
        if (payload_len > 0) {
            ws_read_buf_.ReadAt(payload_offset, payload_vec.data(), payload_vec.size());
            UnmaskWSPayload(payload_vec.data(), payload_vec.size(), frame.mask_key);
        }
        uint8_t* payload = payload_vec.data();
        size_t payload_size = payload_vec.size();

        if (opcode == OPCODE_CLOSE) {
            SendCloseAndDrain(1000);
            return;
        }

        if (opcode == OPCODE_PING) {
            QueueFrame(EncodeWSPongFrame(payload, payload_size));
            ws_read_buf_.Consume(total_len);
            continue;
        }

        if (opcode == OPCODE_PONG) {
            ws_read_buf_.Consume(total_len);
            continue;
        }

        if (IsMessageOpcode(opcode)) {
            if (fragmenting_) {
                FailProtocol(1002);
                return;
            }
            if (frame.fin) {
                auto msg = gs::net::Buffer::FromVector(std::move(payload_vec));
                if (on_message_) {
                    on_message_(this, ToMessageType(opcode), msg);
                }
            } else {
                fragmenting_ = true;
                fragmented_opcode_ = opcode;
                fragmented_payload_ = std::move(payload_vec);
            }
            ws_read_buf_.Consume(total_len);
            continue;
        }

        if (opcode == OPCODE_CONTINUATION) {
            if (!fragmenting_) {
                FailProtocol(1002);
                return;
            }
            if (fragmented_payload_.size() + payload_size > MAX_MESSAGE_BYTES) {
                FailProtocol(1009);
                return;
            }
            fragmented_payload_.insert(fragmented_payload_.end(), payload, payload + payload_size);
            if (frame.fin) {
                auto msg = gs::net::Buffer::FromVector(std::move(fragmented_payload_));
                auto msg_type = ToMessageType(fragmented_opcode_);
                fragmented_payload_.clear();
                fragmented_opcode_ = 0;
                fragmenting_ = false;
                if (on_message_) {
                    on_message_(this, msg_type, msg);
                }
            }
            ws_read_buf_.Consume(total_len);
            continue;
        }

        FailProtocol(1002);
        return;
    }
}

bool WebSocketConnection::Send(std::vector<uint8_t> data) {
    return Send(gs::net::Buffer::FromVector(std::move(data)));
}

bool WebSocketConnection::Send(const gs::net::Buffer& data) {
    return SendMessage(MessageType::Binary, data);
}

bool WebSocketConnection::SendBatch(const std::vector<gs::net::Buffer>& buffers) {
    return SendMessageBatch(MessageType::Binary, buffers);
}

bool WebSocketConnection::SendMessage(MessageType type, const gs::net::Buffer& data) {
    if (!IsWritable()) return false;
    uint8_t opcode = type == MessageType::Text ? OPCODE_TEXT : OPCODE_BINARY;
    std::vector<gs::net::Buffer> parts;
    parts.reserve(2);
    parts.push_back(EncodeWSFrameHeader(opcode, data.Size()));
    if (data.Size() > 0) {
        parts.push_back(data.Slice(0, data.Size()));
    }
    QueueFrameParts(std::move(parts));
    return true;
}

bool WebSocketConnection::SendMessageBatch(MessageType type, const std::vector<gs::net::Buffer>& messages) {
    if (!IsWritable() || messages.empty()) return false;
    uint8_t opcode = type == MessageType::Text ? OPCODE_TEXT : OPCODE_BINARY;
    std::vector<gs::net::Buffer> parts;
    parts.reserve(messages.size() * 2);
    for (const auto& msg : messages) {
        parts.push_back(EncodeWSFrameHeader(opcode, msg.Size()));
        if (msg.Size() > 0) {
            parts.push_back(msg.Slice(0, msg.Size()));
        }
    }
    QueueFrameParts(std::move(parts));
    return true;
}

bool WebSocketConnection::SendFrameBuffer(const gs::net::Buffer& frame) {
    if (!IsWritable()) return false;
    QueueFrame(frame.Slice(0, frame.Size()));
    return true;
}

void WebSocketConnection::QueueFrame(gs::net::Buffer frame) {
    std::vector<gs::net::Buffer> parts;
    parts.push_back(std::move(frame));
    QueueFrameParts(std::move(parts));
}

void WebSocketConnection::QueueFrameParts(std::vector<gs::net::Buffer> parts) {
    if (parts.empty() || closed_.load() || closing_.load() || !handle_) return;

    bool should_pause_read = false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        for (auto& part : parts) {
            write_queue_bytes_ += part.Size();
            write_queue_.push(std::move(part));
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
}

void WebSocketConnection::ProcessWriteQueue() {
    if (!handle_ || uv_is_closing((uv_handle_t*)handle_)) {
        writing_.store(false);
        return;
    }

    constexpr size_t MAX_BATCH = 128;
    auto* req = new WriteReq;
    req->conn = shared_from_this();
    req->buffers.reserve(MAX_BATCH);
    std::vector<uv_buf_t> ubufs;
    ubufs.reserve(MAX_BATCH);

    bool should_resume_read = false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        while (!write_queue_.empty() && req->buffers.size() < MAX_BATCH) {
            auto& front = write_queue_.front();
            write_queue_bytes_ -= front.Size();
            req->buffers.push_back(std::move(front));
            write_queue_.pop();
        }
        if (req->buffers.empty()) {
            writing_.store(false);
            if (close_after_write_.exchange(false)) {
                DoClose();
            }
            delete req;
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

    req->req.data = req;
    for (auto& b : req->buffers) {
        if (b.Size() == 0) continue;
        uv_buf_t uvbuf;
        uvbuf.base = reinterpret_cast<char*>(const_cast<uint8_t*>(b.Data()));
        uvbuf.len = static_cast<unsigned int>(b.Size());
        ubufs.push_back(uvbuf);
    }

    if (ubufs.empty()) {
        delete req;
        ProcessWriteQueue();
        return;
    }

    int r = uv_write(&req->req, (uv_stream_t*)handle_, ubufs.data(),
                     static_cast<unsigned int>(ubufs.size()), OnWriteDone);
    if (r != 0) {
        writing_.store(false);
        auto self = req->conn;
        delete req;
        if (self) self->Close();
    }
}

void WebSocketConnection::OnWriteDone(uv_write_t* uv_req, int status) {
    auto* req = static_cast<WriteReq*>(uv_req->data);
    std::shared_ptr<WebSocketConnection> conn;
    if (req) {
        conn = std::move(req->conn);
        delete req;
    }
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

bool WebSocketConnection::IsWritable() const {
    return !closed_.load() && !closing_.load() && !close_after_write_.load() && handle_ != nullptr;
}

void WebSocketConnection::FailProtocol(uint16_t code) {
    SendCloseAndDrain(code);
}

void WebSocketConnection::SendCloseAndDrain(uint16_t code) {
    if (closed_.load() || closing_.load()) return;
    if (handle_) {
        QueueFrame(EncodeWSCloseFrame(code));
        closed_.store(true);
        close_after_write_.store(true);
        if (!writing_.load()) {
            DoClose();
        }
        return;
    }
    Close();
}

} // namespace websocket
} // namespace net
} // namespace gs
