#include "tcp_server.hpp"
#include <thread>

namespace gs {
namespace net {

TCPServer::TCPServer(const Config& cfg) : cfg_(cfg) {
}

TCPServer::~TCPServer() {
    Stop();
}

bool TCPServer::Start() {
    listen_sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_ == INVALID_SOCKET_HANDLE) return false;

    int reuse = 1;
    ::setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(cfg_.port);
    ::inet_pton(AF_INET, cfg_.host.c_str(), &addr.sin_addr);

    if (::bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(listen_sock_);
        return false;
    }
    if (::listen(listen_sock_, SOMAXCONN) != 0) {
        closesocket(listen_sock_);
        return false;
    }

    running_ = true;
    std::thread(&TCPServer::AcceptLoop, this).detach();
    return true;
}

void TCPServer::Stop() {
    running_ = false;
    if (listen_sock_ != INVALID_SOCKET_HANDLE) {
#ifdef _WIN32
        closesocket(listen_sock_);
#else
        ::close(listen_sock_);
#endif
        listen_sock_ = INVALID_SOCKET_HANDLE;
    }

    std::vector<TCPConn*> to_close;
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        for (auto& [id, conn] : conns_) {
            to_close.push_back(conn);
        }
        conns_.clear(); // 先清空，避免 OnConnClose 再次 erase 导致问题
    }
    for (auto* conn : to_close) {
        conn->Close();
    }
}

void TCPServer::SetCallbacks(ConnectCallback on_connect,
                             DataCallback on_data,
                             CloseCallback on_close) {
    on_connect_ = on_connect;
    on_data_    = on_data;
    on_close_   = on_close;
}

void TCPServer::Use(std::shared_ptr<Middleware> mw) {
    middlewares_.push_back(std::move(mw));
}

void TCPServer::Broadcast(const Packet& pkt) {
    auto data = EncodePacket(pkt);
    std::lock_guard<std::mutex> lk(conn_mtx_);
    for (auto& [id, conn] : conns_) {
        conn->Send(data);
    }
}

void TCPServer::AcceptLoop() {
    while (running_.load()) {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);
        SocketType client_sock = ::accept(listen_sock_,
                                          reinterpret_cast<sockaddr*>(&client_addr), &addr_len);
        if (client_sock == INVALID_SOCKET_HANDLE) {
            continue;
        }

        // 连接数上限检查
        {
            std::lock_guard<std::mutex> lk(conn_mtx_);
            if (static_cast<int>(conns_.size()) >= cfg_.max_conn) {
#ifdef _WIN32
                closesocket(client_sock);
#else
                ::close(client_sock);
#endif
                continue;
            }
        }

        uint64_t id = ++conn_id_counter_;
        auto conn = new TCPConn(client_sock, id);
        conn->SetCallbacks(
            [this](TCPConn* c, Packet& p) { OnConnPacket(c, p); },
            [this](TCPConn* c) { OnConnClose(c); }
        );

        {
            std::lock_guard<std::mutex> lk(conn_mtx_);
            conns_[id] = conn;
        }

        conn->Start();
        if (on_connect_) {
            on_connect_(conn);
        }
    }
}

void TCPServer::OnConnPacket(TCPConn* conn, Packet& pkt) {
    for (auto& mw : middlewares_) {
        if (!mw->OnPacket(conn, pkt)) {
            return;
        }
    }
    if (on_data_) {
        on_data_(conn, pkt);
    }
}

void TCPServer::OnConnClose(TCPConn* conn) {
    std::lock_guard<std::mutex> lk(conn_mtx_);
    conns_.erase(conn->ID());
    if (on_close_) {
        on_close_(conn);
    }
    delete conn;
}

} // namespace net
} // namespace gs
