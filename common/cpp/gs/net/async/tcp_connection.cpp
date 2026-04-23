#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "tcp_connection.hpp"
#include "event_loop.hpp"

#include <uv.h>
#include <cstring>
#include <iostream>
#include "../packet.hpp"
#include "../../crypto/session.hpp"

namespace gs {
namespace net {
namespace async {

struct AsyncTCPConnection::WriteReq {
    uv_write_t req;
    std::shared_ptr<AsyncTCPConnection> conn;
    std::vector<net::Buffer> buffers;
};

// ===================== RingBuffer 实现 =====================

AsyncTCPConnection::RingBuffer::RingBuffer(size_t cap) : buf_(cap) {}

void AsyncTCPConnection::RingBuffer::Append(const uint8_t* data, size_t len) {
    EnsureSpace(len);
    size_t pos = wpos_;
    size_t remaining = len;
    while (remaining > 0) {
        size_t chunk = std::min(remaining, buf_.size() - pos);
        std::memcpy(buf_.data() + pos, data + (len - remaining), chunk);
        pos = (pos + chunk) % buf_.size();
        remaining -= chunk;
    }
    wpos_ = pos;
    size_ += len;
}

size_t AsyncTCPConnection::RingBuffer::Readable() const {
    return size_;
}

bool AsyncTCPConnection::RingBuffer::IsContiguous(size_t offset, size_t len) const {
    if (offset + len > size_) return false;
    size_t start = (rpos_ + offset) % buf_.size();
    return start + len <= buf_.size();
}

const uint8_t* AsyncTCPConnection::RingBuffer::DataAt(size_t offset) const {
    return buf_.data() + (rpos_ + offset) % buf_.size();
}

void AsyncTCPConnection::RingBuffer::ReadAt(size_t offset, uint8_t* out, size_t len) const {
    size_t start = (rpos_ + offset) % buf_.size();
    size_t copied = 0;
    while (copied < len) {
        size_t chunk = std::min(len - copied, buf_.size() - start);
        std::memcpy(out + copied, buf_.data() + start, chunk);
        copied += chunk;
        start = 0;
    }
}

void AsyncTCPConnection::RingBuffer::Consume(size_t len) {
    len = std::min(len, size_);
    rpos_ = (rpos_ + len) % buf_.size();
    size_ -= len;
}

void AsyncTCPConnection::RingBuffer::EnsureSpace(size_t len) {
    if (size_ + len <= buf_.size()) return;

    size_t new_cap = buf_.size();
    while (new_cap < size_ + len) new_cap *= 2;

    std::vector<uint8_t> new_buf(new_cap);
    if (size_ > 0) {
        size_t pos = rpos_;
        size_t copied = 0;
        while (copied < size_) {
            size_t chunk = std::min(size_ - copied, buf_.size() - pos);
            std::memcpy(new_buf.data() + copied, buf_.data() + pos, chunk);
            copied += chunk;
            pos = 0;
        }
    }

    buf_ = std::move(new_buf);
    rpos_ = 0;
    wpos_ = size_;
}

// ==========================================================

AsyncTCPConnection::AsyncTCPConnection(AsyncEventLoop* loop, uint64_t id)
    : loop_(loop), id_(id), write_req_(std::make_unique<WriteReq>()) {}

AsyncTCPConnection::~AsyncTCPConnection() {
    if (handle_ && !closing_.load()) {
        DoClose();
    }
}

bool AsyncTCPConnection::InitFromAccepted(uv_tcp_t* client) {
    handle_ = client;
    handle_->data = this;
    StartRead();
    connected_.store(true);
    return true;
}

bool AsyncTCPConnection::Connect(const std::string& host, uint16_t port) {
    if (!loop_ || !loop_->RawLoop()) return false;

    handle_ = new uv_tcp_t;
    uv_tcp_init(loop_->RawLoop(), handle_);
    handle_->data = this;

    sockaddr_in addr{};
    if (uv_ip4_addr(host.c_str(), port, &addr) != 0) {
        delete handle_;
        handle_ = nullptr;
        return false;
    }

    auto* connect_req = new uv_connect_t;
    connect_req->data = this;
    int r = uv_tcp_connect(connect_req, handle_, (const sockaddr*)&addr,
        [](uv_connect_t* req, int status) {
            auto* conn = static_cast<AsyncTCPConnection*>(req->data);
            delete req;
            if (!conn) return;
            if (status < 0) {
                if (conn->on_connect_) conn->on_connect_(false);
                conn->Close();
                return;
            }
            conn->connected_.store(true);
            conn->StartRead();
            if (conn->on_connect_) conn->on_connect_(true);
        });
    if (r != 0) {
        delete connect_req;
        delete handle_;
        handle_ = nullptr;
        return false;
    }
    return true;
}

void AsyncTCPConnection::SetCallbacks(DataCallback on_data, CloseCallback on_close) {
    on_data_ = on_data;
    on_close_ = on_close;
}

void AsyncTCPConnection::SetConnectCallback(ConnectCallback cb) {
    on_connect_ = std::move(cb);
}

void AsyncTCPConnection::SetSessionKey(const std::vector<uint8_t>& key) {
    session_key_ = key;
}

bool AsyncTCPConnection::Send(std::vector<uint8_t> data) {
    return Send(net::Buffer::FromVector(std::move(data)));
}

bool AsyncTCPConnection::Send(const Buffer& data) {
    if (closed_.load() || closing_.load() || !handle_) return false;

    bool should_pause_read = false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        write_queue_.push(data);
        write_queue_bytes_ += data.Size();

        // 背压：写队列超限，暂停读取
        if (write_queue_bytes_ > MAX_WRITE_QUEUE_BYTES && !read_paused_) {
            read_paused_ = true;
            should_pause_read = true;
        }
    }

    if (should_pause_read && handle_) {
        uv_read_stop((uv_stream_t*)handle_);
    }

    // 如果当前没有在写，触发写
    bool expected = false;
    if (writing_.compare_exchange_strong(expected, true)) {
        loop_->Post([self = shared_from_this()]() {
            self->ProcessWriteQueue();
        });
    }
    return true;
}

bool AsyncTCPConnection::SendBatch(const std::vector<Buffer>& buffers) {
    if (closed_.load() || closing_.load() || !handle_ || buffers.empty()) return false;

    bool should_pause_read = false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        for (const auto& b : buffers) {
            write_queue_.push(b);
            write_queue_bytes_ += b.Size();
        }
        if (write_queue_bytes_ > MAX_WRITE_QUEUE_BYTES && !read_paused_) {
            read_paused_ = true;
            should_pause_read = true;
        }
    }

    if (should_pause_read && handle_) {
        uv_read_stop((uv_stream_t*)handle_);
    }

    bool expected = false;
    if (writing_.compare_exchange_strong(expected, true)) {
        loop_->Post([self = shared_from_this()]() {
            self->ProcessWriteQueue();
        });
    }
    return true;
}

bool AsyncTCPConnection::SendPacket(const Packet& pkt) {
    Packet encPkt = pkt;
    if (!session_key_.empty() && !pkt.payload.Empty()) {
        try {
            encPkt.payload = gs::crypto::EncryptPacketPayload(session_key_, pkt.payload.ToVector());
            encPkt.header.flags |= static_cast<uint32_t>(Flag::ENCRYPT);
        } catch (const std::exception&) {
            return false;
        }
    }
    encPkt.header.length = HEADER_SIZE + static_cast<uint32_t>(encPkt.payload.Size());
    auto data = EncodePacket(encPkt);
    return Send(std::move(data));
}

void AsyncTCPConnection::Close() {
    if (closed_.exchange(true)) return;
    if (closing_.exchange(true)) return;

    // 保持对象存活直到 close_cb 完成
    keep_alive_ = shared_from_this();

    loop_->Post([self = shared_from_this()]() {
        self->DoClose();
    });
}

void AsyncTCPConnection::DoClose() {
    if (handle_ && !uv_is_closing((uv_handle_t*)handle_)) {
        uv_close((uv_handle_t*)handle_, OnCloseDone);
    }
}

void AsyncTCPConnection::StartRead() {
    if (!handle_) return;
    uv_read_start((uv_stream_t*)handle_, OnAlloc, OnRead);
}

void AsyncTCPConnection::OnAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    (void)handle;
    // 分配固定大小缓冲区
    static thread_local std::vector<char> tls_buf;
    if (tls_buf.size() < suggested_size) {
        tls_buf.resize(suggested_size);
    }
    buf->base = tls_buf.data();
    buf->len = static_cast<unsigned int>(suggested_size);
}

void AsyncTCPConnection::OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    (void)buf;
    auto* conn = static_cast<AsyncTCPConnection*>(stream->data);
    if (!conn) return;

    if (nread > 0) {
        {
            std::lock_guard<std::mutex> lk(conn->read_mtx_);
            conn->read_ring_buf_.Append(reinterpret_cast<uint8_t*>(buf->base), nread);
            // 读缓冲区上限检查（慢连接攻击防护）
            if (conn->read_ring_buf_.Readable() > MAX_READ_BUF_BYTES) {
                conn->Close();
                return;
            }
        }
        conn->ProcessReadBuffer();
    } else if (nread < 0) {
        // EOF 或错误
        conn->Close();
    }
}

void AsyncTCPConnection::ProcessReadBuffer() {
    std::lock_guard<std::mutex> lk(read_mtx_);

    while (true) {
        if (read_ring_buf_.Readable() < 4) break;

        uint8_t len_bytes[4];
        read_ring_buf_.ReadAt(0, len_bytes, 4);
        uint32_t length = ReadU32BE(len_bytes);

        if (length < HEADER_SIZE || length > MAX_PACKET_LEN) {
            Close();
            return;
        }

        if (read_ring_buf_.Readable() < length) break; // 数据不足

        // 快速路径：数据在连续区域，直接指针访问
        if (read_ring_buf_.IsContiguous(0, length)) {
            const uint8_t* p = read_ring_buf_.DataAt(0);

            uint16_t magic = ReadU16BE(p + 4);
            if (magic != MAGIC_VALUE) {
                Close();
                return;
            }

            Header h;
            h.length = length;
            h.magic = magic;
            h.cmd_id = ReadU32BE(p + 6);
            h.seq_id = ReadU32BE(p + 10);
            h.flags = ReadU32BE(p + 14);

            size_t payload_len = length - HEADER_SIZE;
            net::Buffer payload;
            if (payload_len > 0) {
                payload = net::Buffer::Allocate(payload_len);
                std::memcpy(payload.Data(), p + HEADER_SIZE, payload_len);
            }

            read_ring_buf_.Consume(length);

            Packet pkt;
            pkt.header = h;
            pkt.payload = std::move(payload);

            if (!session_key_.empty() && HasFlag(pkt.header.flags, Flag::ENCRYPT)) {
                try {
                    pkt.payload = gs::crypto::DecryptPacketPayload(session_key_, pkt.payload.ToVector());
                    pkt.header.flags &= ~static_cast<uint32_t>(Flag::ENCRYPT);
                } catch (const std::exception&) {
                    Close();
                    return;
                }
            }

            if (on_data_) {
                on_data_(this, pkt);
            }
        } else {
            // 慢路径：数据跨越环形缓冲区末尾，读取到临时缓冲区
            uint8_t temp_buf[64 * 1024];
            if (length > sizeof(temp_buf)) {
                std::vector<uint8_t> large_buf(length);
                read_ring_buf_.ReadAt(0, large_buf.data(), length);
                read_ring_buf_.Consume(length);

                uint16_t magic = ReadU16BE(large_buf.data() + 4);
                if (magic != MAGIC_VALUE) { Close(); return; }

                Header h;
                h.length = length;
                h.magic = magic;
                h.cmd_id = ReadU32BE(large_buf.data() + 6);
                h.seq_id = ReadU32BE(large_buf.data() + 10);
                h.flags = ReadU32BE(large_buf.data() + 14);

                size_t payload_len = length - HEADER_SIZE;
                net::Buffer payload;
                if (payload_len > 0) {
                    payload = net::Buffer::Allocate(payload_len);
                    std::memcpy(payload.Data(), large_buf.data() + HEADER_SIZE, payload_len);
                }

                Packet pkt;
                pkt.header = h;
                pkt.payload = std::move(payload);

                if (!session_key_.empty() && HasFlag(pkt.header.flags, Flag::ENCRYPT)) {
                    try {
                        pkt.payload = gs::crypto::DecryptPacketPayload(session_key_, pkt.payload.ToVector());
                        pkt.header.flags &= ~static_cast<uint32_t>(Flag::ENCRYPT);
                    } catch (const std::exception&) {
                        Close();
                        return;
                    }
                }

                if (on_data_) {
                    on_data_(this, pkt);
                }
                continue;
            }

            read_ring_buf_.ReadAt(0, temp_buf, length);
            read_ring_buf_.Consume(length);

            uint16_t magic = ReadU16BE(temp_buf + 4);
            if (magic != MAGIC_VALUE) { Close(); return; }

            Header h;
            h.length = length;
            h.magic = magic;
            h.cmd_id = ReadU32BE(temp_buf + 6);
            h.seq_id = ReadU32BE(temp_buf + 10);
            h.flags = ReadU32BE(temp_buf + 14);

            size_t payload_len = length - HEADER_SIZE;
            net::Buffer payload;
            if (payload_len > 0) {
                payload = net::Buffer::Allocate(payload_len);
                std::memcpy(payload.Data(), temp_buf + HEADER_SIZE, payload_len);
            }

            Packet pkt;
            pkt.header = h;
            pkt.payload = std::move(payload);

            if (!session_key_.empty() && HasFlag(pkt.header.flags, Flag::ENCRYPT)) {
                try {
                    pkt.payload = gs::crypto::DecryptPacketPayload(session_key_, pkt.payload.ToVector());
                    pkt.header.flags &= ~static_cast<uint32_t>(Flag::ENCRYPT);
                } catch (const std::exception&) {
                    Close();
                    return;
                }
            }

            if (on_data_) {
                on_data_(this, pkt);
            }
        }
    }
}

void AsyncTCPConnection::ProcessWriteQueue() {
    if (!handle_ || uv_is_closing((uv_handle_t*)handle_)) return;

    // 批量收集：一次 uv_write 发送多个 buffer，减少 syscall
    constexpr size_t MAX_BATCH = 64;
    std::vector<Buffer> batch;
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

        // 背压恢复：写队列下降到阈值以下，恢复读取
        if (read_paused_ && write_queue_bytes_ < WRITE_RESUME_THRESHOLD) {
            read_paused_ = false;
            should_resume_read = true;
        }
    }

    if (should_resume_read && handle_) {
        uv_read_start((uv_stream_t*)handle_, OnAlloc, OnRead);
    }

    // 复用预分配的 WriteReq，避免每次 new/delete
    auto* req = write_req_.get();
    req->req.data = this;
    req->conn = shared_from_this();
    req->buffers = std::move(batch);

    for (auto& b : req->buffers) {
        uv_buf_t uvbuf;
        uvbuf.base = reinterpret_cast<char*>(const_cast<uint8_t*>(b.Data()));
        uvbuf.len = static_cast<unsigned int>(b.Size());
        tls_ubufs.push_back(uvbuf);
    }

    int r = uv_write(&req->req, (uv_stream_t*)handle_, tls_ubufs.data(),
                     static_cast<unsigned int>(tls_ubufs.size()), OnWriteDone);
    if (r != 0) {
        req->buffers.clear();
        writing_.store(false);
        Close();
    }
}

void AsyncTCPConnection::OnWriteDone(uv_write_t* req, int status) {
    auto* wr = reinterpret_cast<WriteReq*>(req);
    // 通过 shared_ptr 保持对象存活，防止 OnCloseDone 已销毁对象后出现 use-after-free
    auto conn = wr->conn;
    wr->buffers.clear();  // 释放 Buffer 引用，保留 vector capacity 供下次复用

    if (!conn) return;
    if (status < 0) {
        conn->Close();
        return;
    }

    // 继续发送队列中的下一个
    conn->ProcessWriteQueue();
}

void AsyncTCPConnection::OnCloseDone(uv_handle_t* handle) {
    auto* raw_conn = static_cast<AsyncTCPConnection*>(handle->data);
    delete (uv_tcp_t*)handle;
    if (!raw_conn) return;

    auto cb = std::move(raw_conn->on_close_);
    raw_conn->handle_ = nullptr;

    // 先执行回调，再释放 keep_alive，避免回调中访问已销毁的对象
    if (cb) {
        cb(raw_conn);
    }

    raw_conn->keep_alive_.reset();
}

} // namespace async
} // namespace net
} // namespace gs
