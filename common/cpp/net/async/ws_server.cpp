#include "ws_server.hpp"
#include "net/async/event_loop.hpp"
#include "ws_frame.hpp"
#include <uv.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#endif

namespace gs {
namespace net {
namespace websocket {

WebSocketServer::WebSocketServer(const Config& cfg) : cfg_(cfg) {}

WebSocketServer::~WebSocketServer() {
    Stop();
}

bool WebSocketServer::Start() {
    if (running_.exchange(true)) return false;

    loop_ = std::make_unique<gs::net::async::AsyncEventLoop>();
    if (!loop_->Init()) {
        running_.store(false);
        return false;
    }

    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    for (unsigned i = 0; i < hw; ++i) {
        auto worker = std::make_unique<gs::net::async::AsyncEventLoop>();
        if (!worker->Init()) {
            running_.store(false);
            return false;
        }
        worker_loops_.push_back(std::move(worker));
    }

    listen_handle_ = new uv_tcp_t;
    uv_tcp_init(loop_->RawLoop(), listen_handle_);
    listen_handle_->data = this;

    sockaddr_in addr{};
    if (uv_ip4_addr(cfg_.host.c_str(), cfg_.port, &addr) != 0) {
        uv_close((uv_handle_t*)listen_handle_, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        listen_handle_ = nullptr;
        running_.store(false);
        return false;
    }

    if (uv_tcp_bind(listen_handle_, (const sockaddr*)&addr, 0) != 0) {
        uv_close((uv_handle_t*)listen_handle_, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        listen_handle_ = nullptr;
        running_.store(false);
        return false;
    }

    int r = uv_listen((uv_stream_t*)listen_handle_, 128,
        [](uv_stream_t* server, int status) {
            if (status < 0) return;
            auto* self = static_cast<WebSocketServer*>(server->data);
            auto* client = new uv_tcp_t;
            uv_tcp_init(self->loop_->RawLoop(), client);
            if (uv_accept(server, (uv_stream_t*)client) == 0) {
                self->OnAccept(client);
            } else {
                uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
            }
        });

    if (r != 0) {
        uv_close((uv_handle_t*)listen_handle_, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        listen_handle_ = nullptr;
        running_.store(false);
        return false;
    }

    for (auto& worker : worker_loops_) {
        worker_threads_.emplace_back([w = worker.get()]() {
            w->Run();
        });
    }

    loop_thread_ = std::thread(&WebSocketServer::RunEventLoop, this);
    return true;
}

void WebSocketServer::Stop() {
    if (!running_.exchange(false)) return;

    if (loop_) loop_->Stop();
    if (listen_handle_ && !uv_is_closing((uv_handle_t*)listen_handle_)) {
        uv_close((uv_handle_t*)listen_handle_, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
        listen_handle_ = nullptr;
    }

    std::unordered_map<uint64_t, std::shared_ptr<WebSocketConnection>> to_close;
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        to_close.swap(conns_);
    }
    for (auto& [id, conn] : to_close) {
        conn->Close();
    }

    if (loop_thread_.joinable()) loop_thread_.join();

    for (auto& worker : worker_loops_) {
        worker->Stop();
    }
    for (auto& t : worker_threads_) {
        if (t.joinable()) t.join();
    }
}

void WebSocketServer::SetCallbacks(ConnectCallback on_connect,
                                   MessageCallback on_message,
                                   CloseCallback on_close) {
    on_connect_ = std::move(on_connect);
    on_message_ = std::move(on_message);
    on_close_   = std::move(on_close);
}

void WebSocketServer::Broadcast(MessageType type, const gs::net::Buffer& message) {
    uint8_t opcode = type == MessageType::Text ? OPCODE_TEXT : OPCODE_BINARY;
    auto ws_frame = EncodeWSFrame(opcode, message.Data(), message.Size());

    std::vector<std::shared_ptr<WebSocketConnection>> snapshot;
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        snapshot.reserve(conns_.size());
        for (auto& [id, conn] : conns_) {
            snapshot.push_back(conn);
        }
    }

    for (auto& conn : snapshot) {
        conn->SendFrameBuffer(ws_frame);
    }
}

void WebSocketServer::BroadcastBinary(const gs::net::Buffer& message) {
    Broadcast(MessageType::Binary, message);
}

size_t WebSocketServer::ConnectionCount() const {
    std::lock_guard<std::mutex> lk(conn_mtx_);
    return conns_.size();
}

void WebSocketServer::OnAccept(uv_tcp_t* client) {
    if (!client) return;

    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        if (static_cast<int>(conns_.size()) >= cfg_.max_conn) {
            uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
            return;
        }
    }

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

    size_t idx = next_worker_.fetch_add(1) % worker_loops_.size();
    auto* worker_loop = worker_loops_[idx].get();

#ifdef _WIN32
    uv_os_sock_t os_sock = dup_sock;
#else
    uv_os_sock_t os_sock = dup_fd;
#endif

    uv_close((uv_handle_t*)client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });

    uint64_t id = ++conn_id_counter_;
    auto conn = std::make_shared<WebSocketConnection>(worker_loop, id);
    conn->SetCallbacks(
        [this](WebSocketConnection* c, MessageType t, gs::net::Buffer& m) { OnConnMessage(c, t, m); },
        [this](WebSocketConnection* c) { OnConnClose(c); }
    );

    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        conns_[id] = conn;
    }

    worker_loop->Post([this, worker_loop, conn, os_sock]() {
        if (!running_.load()) {
            std::lock_guard<std::mutex> lk(conn_mtx_);
            conns_.erase(conn->ID());
#ifdef _WIN32
            closesocket(os_sock);
#else
            close(os_sock);
#endif
            return;
        }

        auto* worker_client = new uv_tcp_t;
        if (uv_tcp_init(worker_loop->RawLoop(), worker_client) != 0) {
            delete worker_client;
            {
                std::lock_guard<std::mutex> lk(conn_mtx_);
                conns_.erase(conn->ID());
            }
#ifdef _WIN32
            closesocket(os_sock);
#else
            close(os_sock);
#endif
            return;
        }

        if (uv_tcp_open(worker_client, os_sock) != 0) {
            {
                std::lock_guard<std::mutex> lk(conn_mtx_);
                conns_.erase(conn->ID());
            }
            uv_close((uv_handle_t*)worker_client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
#ifdef _WIN32
            closesocket(os_sock);
#else
            close(os_sock);
#endif
            return;
        }

        if (!conn->InitFromAccepted(worker_client)) {
            {
                std::lock_guard<std::mutex> lk(conn_mtx_);
                conns_.erase(conn->ID());
            }
            uv_close((uv_handle_t*)worker_client, [](uv_handle_t* h) { delete (uv_tcp_t*)h; });
            return;
        }
        if (on_connect_) {
            on_connect_(conn.get());
        }
    });
}

void WebSocketServer::OnConnMessage(WebSocketConnection* conn, MessageType type, gs::net::Buffer& message) {
    if (on_message_) {
        on_message_(conn, type, message);
    }
}

void WebSocketServer::OnConnClose(WebSocketConnection* conn) {
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        conns_.erase(conn->ID());
    }
    if (on_close_) {
        on_close_(conn);
    }
}

void WebSocketServer::RunEventLoop() {
    loop_->Run();
}

} // namespace websocket
} // namespace net
} // namespace gs
