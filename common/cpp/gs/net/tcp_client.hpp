#pragma once

#include "tcp_conn.hpp"
#include <string>

namespace gs {
namespace net {

class TCPClient {
public:
    using DataCallback = std::function<void(TCPConn*, Packet&)>;
    using CloseCallback = std::function<void(TCPConn*)>;

    TCPClient(const std::string& host, uint16_t port);
    ~TCPClient();

    bool Connect();
    void Close();
    bool IsConnected() const { return conn_ != nullptr && !conn_->IsClosed(); }

    void SetCallbacks(DataCallback on_data, CloseCallback on_close);

    TCPConn* Conn() { return conn_; }

private:
    void OnConnClose(TCPConn* conn);

    std::string host_;
    uint16_t    port_;
    TCPConn*    conn_ = nullptr;

    DataCallback  on_data_;
    CloseCallback on_close_;
    CloseCallback user_on_close_;
};

} // namespace net
} // namespace gs
