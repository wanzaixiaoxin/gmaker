#include "tcp_conn.hpp"
#include <cstring>
#include <iostream>

namespace gs {
namespace net {

TCPConn::TCPConn(SocketType sock, uint64_t id)
    : socket_(sock), id_(id) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    last_active_ns_.store(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
}

TCPConn::~TCPConn() {
    // 确保监控线程先结束（它会负责 join read/write 线程）
    if (monitor_thread_.joinable()) monitor_thread_.join();
}

void TCPConn::SetCallbacks(DataCallback on_data, CloseCallback on_close) {
    on_data_ = on_data;
    on_close_ = on_close;
}

void TCPConn::SetSessionKey(const std::vector<uint8_t>& key) {
    session_key_ = key;
}

void TCPConn::SetHeartbeatTimeout(int ms) {
    heartbeat_timeout_ms_ = ms;
}

void TCPConn::Start() {
    // 设置接收超时，使 ReadLoop 能定期检查 closed_ 和心跳超时
    if (heartbeat_timeout_ms_ > 0) {
        int timeout_ms = std::min(heartbeat_timeout_ms_, 1000); // 至少 1s 检查一次
#ifdef _WIN32
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    }

    read_thread_ = std::thread(&TCPConn::ReadLoop, this);
    write_thread_ = std::thread(&TCPConn::WriteLoop, this);
    monitor_thread_ = std::thread(&TCPConn::MonitorLoop, this);
}

bool TCPConn::Send(const std::vector<uint8_t>& data) {
    if (closed_.load()) return false;
    {
        std::lock_guard<std::mutex> lk(write_mtx_);
        write_queue_.push(data);
    }
    write_cv_.notify_one();
    return true;
}

bool TCPConn::SendPacket(const Packet& pkt) {
    Packet encPkt = pkt; // 拷贝，避免修改调用方数据
    if (!session_key_.empty() && !pkt.payload.empty()) {
        try {
            encPkt.payload = gs::crypto::EncryptPacketPayload(session_key_, pkt.payload);
            encPkt.header.flags |= static_cast<uint32_t>(Flag::ENCRYPT);
        } catch (const std::exception&) {
            return false;
        }
    }
    encPkt.header.length = HEADER_SIZE + static_cast<uint32_t>(encPkt.payload.size());
    auto data = EncodePacket(encPkt);
    return Send(data);
}

void TCPConn::Close() {
    if (closed_.exchange(true)) return;

    // 唤醒 write 线程
    write_cv_.notify_all();

#ifdef _WIN32
    closesocket(socket_);
#else
    ::close(socket_);
#endif
    // MonitorLoop 会负责 join 线程并调用 on_close_
}

bool TCPConn::ReadN(uint8_t* buf, size_t n) {
    size_t total = 0;
    while (total < n && !closed_.load()) {
        int ret = ::recv(socket_, reinterpret_cast<char*>(buf + total), static_cast<int>(n - total), 0);
        if (ret > 0) {
            total += static_cast<size_t>(ret);
        } else if (ret == 0) {
            return false; // peer closed
        } else {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAETIMEDOUT) {
                continue; // 接收超时，继续循环，让上层检查心跳超时
            }
            if (err != WSAEWOULDBLOCK && err != WSAEINTR) {
                return false;
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                continue; // 超时或可中断，继续循环
            }
            return false;
#endif
        }
    }
    return total == n;
}

bool TCPConn::IsHeartbeatExpired() const {
    if (heartbeat_timeout_ms_ <= 0) return false;
    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    auto elapsed_ms = static_cast<int>((now_ns - last_active_ns_.load()) / 1'000'000);
    return elapsed_ms > heartbeat_timeout_ms_;
}

void TCPConn::UpdateLastActive() {
    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    last_active_ns_.store(now_ns);
}

void TCPConn::ReadLoop() {
    uint8_t header_buf[HEADER_SIZE];
    while (!closed_.load()) {
        // 心跳超时检测
        if (IsHeartbeatExpired()) {
            return;
        }

        if (!ReadN(header_buf, HEADER_SIZE)) {
            return;
        }

        Header h = DecodeHeader(header_buf);
        if (h.magic != MAGIC_VALUE || h.length < HEADER_SIZE || h.length > MAX_PACKET_LEN) {
            return;
        }

        Packet pkt;
        pkt.header = h;
        size_t payload_len = h.length - HEADER_SIZE;
        if (payload_len > 0) {
            pkt.payload.resize(payload_len);
            if (!ReadN(pkt.payload.data(), payload_len)) {
                return;
            }
        }

        UpdateLastActive();

        if (!session_key_.empty() && HasFlag(pkt.header.flags, Flag::ENCRYPT)) {
            try {
                pkt.payload = gs::crypto::DecryptPacketPayload(session_key_, pkt.payload);
                pkt.header.flags &= ~static_cast<uint32_t>(Flag::ENCRYPT);
            } catch (const std::exception&) {
                return;
            }
        }

        if (on_data_) {
            on_data_(this, pkt);
        }
    }
}

void TCPConn::WriteLoop() {
    std::unique_lock<std::mutex> lk(write_mtx_);
    while (!closed_.load()) {
        write_cv_.wait(lk, [this] { return closed_.load() || !write_queue_.empty(); });

        while (!write_queue_.empty()) {
            auto data = std::move(write_queue_.front());
            write_queue_.pop();
            lk.unlock();

            size_t total = 0;
            while (total < data.size() && !closed_.load()) {
                int ret = ::send(socket_, reinterpret_cast<const char*>(data.data() + total),
                                 static_cast<int>(data.size() - total), 0);
                if (ret > 0) {
                    total += static_cast<size_t>(ret);
                } else {
#ifdef _WIN32
                    int err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK && err != WSAEINTR) {
#else
                    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
#endif
                        return;
                    }
                }
            }
            lk.lock();
        }
    }
}

void TCPConn::MonitorLoop() {
    if (read_thread_.joinable()) read_thread_.join();
    if (write_thread_.joinable()) write_thread_.join();

    // 若尚未关闭（比如对端主动断开导致 ReadLoop 退出），先关闭 socket
    if (!closed_.exchange(true)) {
#ifdef _WIN32
        closesocket(socket_);
#else
        ::close(socket_);
#endif
    }

    auto cb = std::move(on_close_);
    if (cb) {
        cb(this);
    }
}

} // namespace net
} // namespace gs
