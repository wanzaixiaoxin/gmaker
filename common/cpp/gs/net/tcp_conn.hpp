#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "packet.hpp"

namespace gs {
namespace net {

using SocketType = SOCKET;
constexpr SocketType INVALID_SOCKET_HANDLE = INVALID_SOCKET;

class TCPConn {
public:
    using DataCallback = std::function<void(TCPConn*, Packet&)>;
    using CloseCallback = std::function<void(TCPConn*)>;

    TCPConn(SocketType sock, uint64_t id);
    ~TCPConn();

    uint64_t ID() const { return id_; }
    SocketType Socket() const { return socket_; }

    void SetCallbacks(DataCallback on_data, CloseCallback on_close);
    void Start();

    bool Send(const std::vector<uint8_t>& data);
    bool SendPacket(const Packet& pkt);

    void Close();
    bool IsClosed() const { return closed_.load(); }

private:
    void ReadLoop();
    void WriteLoop();
    bool ReadN(uint8_t* buf, size_t n);

    SocketType socket_;
    uint64_t id_;
    std::atomic<bool> closed_{false};

    DataCallback on_data_;
    CloseCallback on_close_;

    std::thread read_thread_;
    std::thread write_thread_;

    std::mutex write_mtx_;
    std::condition_variable write_cv_;
    std::queue<std::vector<uint8_t>> write_queue_;
};

} // namespace net
} // namespace gs
