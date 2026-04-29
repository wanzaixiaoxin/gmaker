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

    // Accept loop
    loop_ = std::make_unique<AsyncEventLoop>();
    if (!loop_->Init()) {
        running_.store(false);
        return false;
    }

    // Worker loops：数量 = CPU 核心数
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    for (unsigned i = 0; i < hw; ++i) {
        auto worker = std::make_unique<AsyncEventLoop>();
        if (!worker->Init()) {
            running_.store(false);
            return false;
        }
        worker_loops_.push_back(std::move(worker));
    }

    listener_ = std::make_unique<AsyncTCPListener>(loop_.get());
    listener_->SetConnectionCallback([this](uv_tcp_t* client) {
        this->OnAccept(client);
    });

    if (!listener_->Listen(cfg_.host, cfg_.port)) {
        running_.store(false);
        return false;
    }

    // 启动 worker 线程
    for (auto& worker : worker_loops_) {
        worker_threads_.emplace_back([w = worker.get()]() {
            w->Run();
        });
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

    // 停止 worker loops
    for (auto& worker : worker_loops_) {
        worker->Stop();
    }
    for (auto& t : worker_threads_) {
        if (t.joinable()) t.join();
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
        conn->Send(data.Slice(0, data.Size()));
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

    // 获取底层 fd/socket，并复制一份（uv_close 会关闭原 fd，worker 需要独立的 fd）
#ifdef _WIN32
    uv_os_fd_t fd;
    if (uv_fileno((uv_handle_t*)client, &fd) != 0) {
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        return;
    }
    SOCKET original_sock = (SOCKET)fd;
    WSAPROTOCOL_INFOW protocol_info;
    if (WSADuplicateSocketW(original_sock, GetCurrentProcessId(), &protocol_info) != 0) {
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        return;
    }
    SOCKET dup_sock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                  &protocol_info, 0, WSA_FLAG_OVERLAPPED);
    if (dup_sock == INVALID_SOCKET) {
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        return;
    }
#else
    int original_fd;
    if (uv_fileno((uv_handle_t*)client, &original_fd) != 0) {
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        return;
    }
    int dup_fd = dup(original_fd);
    if (dup_fd < 0) {
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        return;
    }
#endif

    // Round-robin 选择 worker loop
    size_t idx = next_worker_.fetch_add(1) % worker_loops_.size();
    auto* worker_loop = worker_loops_[idx].get();

    // 在 worker loop 上重建 uv_tcp_t
    auto* worker_client = new uv_tcp_t;
    if (uv_tcp_init(worker_loop->RawLoop(), worker_client) != 0) {
        delete worker_client;
#ifdef _WIN32
        closesocket(dup_sock);
#else
        close(dup_fd);
#endif
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        return;
    }

#ifdef _WIN32
    if (uv_tcp_open(worker_client, dup_sock) != 0) {
#else
    if (uv_tcp_open(worker_client, dup_fd) != 0) {
#endif
        uv_close((uv_handle_t*)worker_client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
#ifdef _WIN32
        closesocket(dup_sock);
#else
        close(dup_fd);
#endif
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        return;
    }

    // 关闭原 client（关闭 original_sock/original_fd，dup 的仍然有效）
    uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });

    uint64_t id = ++conn_id_counter_;
    auto conn = std::make_shared<AsyncTCPConnection>(worker_loop, id);
    if (!conn->InitFromAccepted(worker_client)) {
        uv_close((uv_handle_t*)worker_client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
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
