#include "tcp_client.hpp"

namespace gs {
namespace net {

TCPClient::TCPClient(const std::string& host, uint16_t port)
    : host_(host), port_(port) {
}

TCPClient::~TCPClient() {
    Close();
}

void TCPClient::SetCallbacks(DataCallback on_data, CloseCallback on_close) {
    on_data_ = on_data;
    user_on_close_ = on_close;
}

bool TCPClient::Connect() {
    SocketType sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET_HANDLE) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port_);
    ::inet_pton(AF_INET, host_.c_str(), &addr.sin_addr);

    if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        return false;
    }

    uint64_t id = 1; // client 通常只有一个连接
    conn_ = new TCPConn(sock, id);
    conn_->SetCallbacks(
        on_data_,
        [this](TCPConn* c) { OnConnClose(c); }
    );
    conn_->Start();
    return true;
}

void TCPClient::Close() {
    if (conn_) {
        conn_->Close();
        conn_ = nullptr;
    }
}

void TCPClient::OnConnClose(TCPConn* conn) {
    if (conn_ == conn) {
        conn_ = nullptr;
    }
    if (user_on_close_) {
        user_on_close_(conn);
    }
    delete conn;
}

} // namespace net
} // namespace gs
