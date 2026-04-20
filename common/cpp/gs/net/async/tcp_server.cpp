#include "tcp_server.hpp"
#include "event_loop.hpp"
#include "tcp_listener.hpp"
#include "tcp_connection.hpp"

#include <uv.h>
#include <iostream>

namespace gs {
namespace net {
namespace async {

AsyncTCPServer::AsyncTCPServer(const Config& cfg) : cfg_(cfg) {}

AsyncTCPServer::~AsyncTCPServer() {
    Stop();
}

bool AsyncTCPServer::Start() {
    if (running_.exchange(true)) return false;

    loop_ = std::make_unique<AsyncEventLoop>();
    if (!loop_->Init()) {
        running_.store(false);
        return false;
    }

    listener_ = std::make_unique<AsyncTCPListener>(loop_.get());
    listener_->SetConnectionCallback([this](uv_tcp_t* client) {
        this->OnAccept(client);
    });

    if (!listener_->Listen(cfg_.host, cfg_.port)) {
        running_.store(false);
        return false;
    }

    loop_thread_ = std::thread(&AsyncTCPServer::RunEventLoop, this);
    return true;
}

void AsyncTCPServer::Stop() {
    if (!running_.exchange(false)) return;

    if (loop_) {
        loop_->Stop();
    }
    if (listener_) {
        listener_->Stop();
    }

    // 关闭所有连接
    std::unordered_map<uint64_t, std::shared_ptr<AsyncTCPConnection>> to_close;
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        to_close.swap(conns_);
    }
    for (auto& [id, conn] : to_close) {
        conn->Close();
    }

    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

void AsyncTCPServer::SetCallbacks(ConnectCallback on_connect,
                                   DataCallback on_data,
                                   CloseCallback on_close) {
    on_connect_ = on_connect;
    on_data_    = on_data;
    on_close_   = on_close;
}

void AsyncTCPServer::Use(std::shared_ptr<Middleware> mw) {
    middlewares_.push_back(std::move(mw));
}

void AsyncTCPServer::Broadcast(const Packet& pkt) {
    auto data = EncodePacket(pkt);
    std::lock_guard<std::mutex> lk(conn_mtx_);
    for (auto& [id, conn] : conns_) {
        conn->Send(data);
    }
}

void AsyncTCPServer::OnAccept(uv_tcp_t* client) {
    if (!client) return;

    // 连接数上限检查
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        if (static_cast<int>(conns_.size()) >= cfg_.max_conn) {
            uv_close((uv_handle_t*)client, [](uv_handle_t* h) {
                delete (uv_tcp_t*)h;
            });
            return;
        }
    }

    uint64_t id = ++conn_id_counter_;
    auto conn = std::make_shared<AsyncTCPConnection>(loop_.get(), id);
    if (!conn->InitFromAccepted(client)) {
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) {
            delete (uv_tcp_t*)h;
        });
        return;
    }

    conn->SetCallbacks(
        [this](AsyncTCPConnection* c, Packet& p) { OnConnData(c, p); },
        [this](AsyncTCPConnection* c) { OnConnClose(c); }
    );

    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        conns_[id] = conn;
    }

    if (on_connect_) {
        on_connect_(conn.get());
    }
}

void AsyncTCPServer::OnConnData(AsyncTCPConnection* conn, Packet& pkt) {
    for (auto& mw : middlewares_) {
        if (!mw->OnPacket(conn, pkt)) {
            return;
        }
    }
    if (on_data_) {
        on_data_(conn, pkt);
    }
}

void AsyncTCPServer::OnConnClose(AsyncTCPConnection* conn) {
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        conns_.erase(conn->ID());
    }
    if (on_close_) {
        on_close_(conn);
    }
}

void AsyncTCPServer::RunEventLoop() {
    loop_->Run();
}

} // namespace async
} // namespace net
} // namespace gs
