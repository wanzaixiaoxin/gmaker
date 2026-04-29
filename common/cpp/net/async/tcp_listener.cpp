#include "tcp_listener.hpp"
#include "event_loop.hpp"

#include <uv.h>
#include <iostream>
#include <cstring>

namespace gs {
namespace net {
namespace async {

AsyncTCPListener::AsyncTCPListener(AsyncEventLoop* loop) : loop_(loop) {}

AsyncTCPListener::~AsyncTCPListener() {
    Stop();
}

bool AsyncTCPListener::Listen(const std::string& host, uint16_t port) {
    if (!loop_ || !loop_->RawLoop()) return false;

    handle_ = new uv_tcp_t;
    uv_tcp_init(loop_->RawLoop(), handle_);
    handle_->data = this;

    sockaddr_in addr{};
    uv_ip4_addr(host.c_str(), port, &addr);

    int r = uv_tcp_bind(handle_, (const sockaddr*)&addr, 0);
    if (r != 0) {
        std::cerr << "uv_tcp_bind failed: " << uv_strerror(r) << std::endl;
        uv_close((uv_handle_t*)handle_, nullptr);
        handle_ = nullptr;
        return false;
    }

    r = uv_listen((uv_stream_t*)handle_, SOMAXCONN, OnConnection);
    if (r != 0) {
        std::cerr << "uv_listen failed: " << uv_strerror(r) << std::endl;
        uv_close((uv_handle_t*)handle_, nullptr);
        handle_ = nullptr;
        return false;
    }

    return true;
}

void AsyncTCPListener::Stop() {
    if (!handle_) return;

    if (loop_ && !loop_->IsInLoopThread()) {
        loop_->Post([this]() { Stop(); });
        return;
    }

    if (!uv_is_closing((uv_handle_t*)handle_)) {
        uv_close((uv_handle_t*)handle_, [](uv_handle_t* h) {
            delete (uv_tcp_t*)h;
        });
    }
    handle_ = nullptr;
}

void AsyncTCPListener::OnConnection(uv_stream_t* server, int status) {
    if (status < 0) return;

    auto* self = static_cast<AsyncTCPListener*>(server->data);
    if (!self || !self->loop_) return;

    uv_tcp_t* client = new uv_tcp_t;
    uv_tcp_init(self->loop_->RawLoop(), client);

    if (uv_accept(server, (uv_stream_t*)client) != 0) {
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) {
            delete (uv_tcp_t*)h;
        });
        return;
    }

    if (self->on_conn_) {
        self->on_conn_(client);
    } else {
        // 无回调时直接关闭
        uv_close((uv_handle_t*)client, [](uv_handle_t* h) {
            delete (uv_tcp_t*)h;
        });
    }
}

} // namespace async
} // namespace net
} // namespace gs
