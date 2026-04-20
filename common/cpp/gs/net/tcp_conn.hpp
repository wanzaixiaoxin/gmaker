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
#include <cerrno>

using SOCKET = int;
constexpr int INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
#endif

#include <algorithm>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>
#include "packet.hpp"
#include "../crypto/session.hpp"

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

    // 设置会话密钥，启用 AES-GCM 加密通信
    void SetSessionKey(const std::vector<uint8_t>& key);

    // 设置心跳超时（毫秒，0 表示不检测）
    void SetHeartbeatTimeout(int ms);

    bool Send(const std::vector<uint8_t>& data);
    bool SendPacket(const Packet& pkt);

    void Close();
    bool IsClosed() const { return closed_.load(); }

private:
    void ReadLoop();
    void WriteLoop();
    void MonitorLoop();
    bool ReadN(uint8_t* buf, size_t n);
    bool IsHeartbeatExpired() const;
    void UpdateLastActive();

    SocketType socket_;
    uint64_t id_;
    std::atomic<bool> closed_{false};

    DataCallback on_data_;
    CloseCallback on_close_;

    std::thread read_thread_;
    std::thread write_thread_;
    std::thread monitor_thread_;

    std::mutex write_mtx_;
    std::condition_variable write_cv_;
    std::queue<std::vector<uint8_t>> write_queue_;

    std::vector<uint8_t> session_key_; // 若非空则启用 AES-GCM 加密

    // 心跳超时（存储 steady_clock 纳秒数，避免 atomic<time_point> 兼容性问题）
    int heartbeat_timeout_ms_ = 0;
    std::atomic<int64_t> last_active_ns_{0};
};

} // namespace net
} // namespace gs
