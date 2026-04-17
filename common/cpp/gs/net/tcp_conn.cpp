#include "tcp_conn.hpp"
#include <cstring>
#include <iostream>

namespace gs {
namespace net {

TCPConn::TCPConn(SocketType sock, uint64_t id)
    : socket_(sock), id_(id) {
}

TCPConn::~TCPConn() {
    Close();
}

void TCPConn::SetCallbacks(DataCallback on_data, CloseCallback on_close) {
    on_data_ = on_data;
    on_close_ = on_close;
}

void TCPConn::Start() {
    read_thread_ = std::thread(&TCPConn::ReadLoop, this);
    write_thread_ = std::thread(&TCPConn::WriteLoop, this);
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
    auto data = EncodePacket(pkt);
    return Send(data);
}

void TCPConn::Close() {
    if (closed_.exchange(true)) return;

    // wake write thread
    write_cv_.notify_all();

#ifdef _WIN32
    closesocket(socket_);
#else
    close(socket_);
#endif

    if (read_thread_.joinable()) read_thread_.join();
    if (write_thread_.joinable()) write_thread_.join();

    if (on_close_) {
        on_close_(this);
    }
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
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAEINTR) {
                return false;
            }
        }
    }
    return total == n;
}

void TCPConn::ReadLoop() {
    uint8_t header_buf[HEADER_SIZE];
    while (!closed_.load()) {
        if (!ReadN(header_buf, HEADER_SIZE)) {
            Close();
            return;
        }

        Header h = DecodeHeader(header_buf);
        if (h.magic != MAGIC_VALUE || h.length < HEADER_SIZE || h.length > MAX_PACKET_LEN) {
            Close();
            return;
        }

        Packet pkt;
        pkt.header = h;
        size_t payload_len = h.length - HEADER_SIZE;
        if (payload_len > 0) {
            pkt.payload.resize(payload_len);
            if (!ReadN(pkt.payload.data(), payload_len)) {
                Close();
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
                    int err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK && err != WSAEINTR) {
                        Close();
                        return;
                    }
                }
            }
            lk.lock();
        }
    }
}

} // namespace net
} // namespace gs
