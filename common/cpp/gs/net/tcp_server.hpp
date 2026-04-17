#pragma once

#include "tcp_conn.hpp"
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace gs {
namespace net {

class TCPServer {
public:
    using ConnectCallback = std::function<void(TCPConn*)>;
    using DataCallback    = std::function<void(TCPConn*, Packet&)>;
    using CloseCallback   = std::function<void(TCPConn*)>;

    struct Config {
        std::string host = "0.0.0.0";
        uint16_t    port = 0;
        int         max_conn = 10000;
    };

    TCPServer(const Config& cfg);
    ~TCPServer();

    bool Start();
    void Stop();

    void SetCallbacks(ConnectCallback on_connect,
                      DataCallback on_data,
                      CloseCallback on_close);

    void Broadcast(const Packet& pkt);

private:
    void AcceptLoop();
    void OnConnClose(TCPConn* conn);

    Config cfg_;
    SocketType listen_sock_ = INVALID_SOCKET_HANDLE;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> conn_id_counter_{0};

    std::mutex conn_mtx_;
    std::unordered_map<uint64_t, TCPConn*> conns_;

    ConnectCallback on_connect_;
    DataCallback    on_data_;
    CloseCallback   on_close_;
};

} // namespace net
} // namespace gs
