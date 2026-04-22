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

AsyncTCPConnection::AsyncTCPConnection(AsyncEventLoop* loop, uint64_t id)
    : loop_(loop), id_(id) {}

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
            conn->read_buf_.insert(conn->read_buf_.end(),
                                   reinterpret_cast<uint8_t*>(buf->base),
                                   reinterpret_cast<uint8_t*>(buf->base) + nread);
            // 读缓冲区上限检查（慢连接攻击防护）
            if (conn->read_buf_.size() - conn->read_consumed_ > MAX_READ_BUF_BYTES) {
                conn->read_mtx_.unlock();
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
        size_t avail = read_buf_.size() - read_consumed_;
        if (avail < 4) break; // 无法读取 length

        const uint8_t* p = read_buf_.data() + read_consumed_;
        uint32_t length = (static_cast<uint32_t>(p[0]) << 24) |
                          (static_cast<uint32_t>(p[1]) << 16) |
                          (static_cast<uint32_t>(p[2]) << 8) |
                          static_cast<uint32_t>(p[3]);

        if (length < HEADER_SIZE || length > MAX_PACKET_LEN) {
            Close();
            return;
        }

        if (avail < length) break; // 数据不足

        // 解析 header
        uint16_t magic = (static_cast<uint16_t>(p[4]) << 8) |
                         static_cast<uint16_t>(p[5]);
        if (magic != MAGIC_VALUE) {
            Close();
            return;
        }

        Header h;
        h.length = length;
        h.magic = magic;
        h.cmd_id = (static_cast<uint32_t>(p[6]) << 24) |
                   (static_cast<uint32_t>(p[7]) << 16) |
                   (static_cast<uint32_t>(p[8]) << 8) |
                   static_cast<uint32_t>(p[9]);
        h.seq_id = (static_cast<uint32_t>(p[10]) << 24) |
                   (static_cast<uint32_t>(p[11]) << 16) |
                   (static_cast<uint32_t>(p[12]) << 8) |
                   static_cast<uint32_t>(p[13]);
        h.flags = (static_cast<uint32_t>(p[14]) << 24) |
                  (static_cast<uint32_t>(p[15]) << 16) |
                  (static_cast<uint32_t>(p[16]) << 8) |
                  static_cast<uint32_t>(p[17]);

        size_t payload_len = length - HEADER_SIZE;
        net::Buffer payload;
        if (payload_len > 0) {
            std::vector<uint8_t> tmp(p + HEADER_SIZE, p + HEADER_SIZE + payload_len);
            payload = net::Buffer::FromVector(std::move(tmp));
        }

        read_consumed_ += length;

        // 定期 compact：累积消费超过阈值后，把未消费数据移到前端
        if (read_consumed_ >= READ_COMPACT_THRESHOLD) {
            size_t remain = read_buf_.size() - read_consumed_;
            if (remain > 0) {
                std::memmove(read_buf_.data(), read_buf_.data() + read_consumed_, remain);
            }
            read_buf_.resize(remain);
            read_consumed_ = 0;
        }

        // 构造 Packet
        Packet pkt;
        pkt.header = h;
        pkt.payload = std::move(payload);

        // 解密
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

void AsyncTCPConnection::ProcessWriteQueue() {
    if (!handle_ || uv_is_closing((uv_handle_t*)handle_)) return;

    // 批量收集：一次 uv_write 发送多个 buffer，减少 syscall
    constexpr size_t MAX_BATCH = 64;
    std::vector<Buffer> batch;
    std::vector<uv_buf_t> ubufs;
    batch.reserve(MAX_BATCH);
    ubufs.reserve(MAX_BATCH);

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

    // 创建 WriteReq（批量版本）
    auto* req = new WriteReq;
    req->req.data = this;
    req->conn = shared_from_this();
    req->buffers = std::move(batch);

    for (auto& b : req->buffers) {
        uv_buf_t uvbuf;
        uvbuf.base = reinterpret_cast<char*>(const_cast<uint8_t*>(b.Data()));
        uvbuf.len = static_cast<unsigned int>(b.Size());
        ubufs.push_back(uvbuf);
    }

    int r = uv_write(&req->req, (uv_stream_t*)handle_, ubufs.data(),
                     static_cast<unsigned int>(ubufs.size()), OnWriteDone);
    if (r != 0) {
        delete req;
        writing_.store(false);
        Close();
    }
}

void AsyncTCPConnection::OnWriteDone(uv_write_t* req, int status) {
    auto* wr = reinterpret_cast<WriteReq*>(req);
    auto* conn = static_cast<AsyncTCPConnection*>(wr->req.data);
    delete wr;

    if (!conn) return;
    if (status < 0) {
        conn->Close();
        return;
    }

    // 继续发送队列中的下一个
    conn->ProcessWriteQueue();
}

void AsyncTCPConnection::OnCloseDone(uv_handle_t* handle) {
    auto* conn = static_cast<AsyncTCPConnection*>(handle->data);
    delete (uv_tcp_t*)handle;
    if (!conn) return;

    auto cb = std::move(conn->on_close_);
    conn->handle_ = nullptr;
    // 释放 keep_alive，允许对象被销毁
    conn->keep_alive_.reset();

    if (cb) {
        cb(conn);
    }
}

} // namespace async
} // namespace net
} // namespace gs
