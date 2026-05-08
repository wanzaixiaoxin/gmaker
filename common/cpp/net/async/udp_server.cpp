#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "udp_server.hpp"
#include "event_loop.hpp"
#include "../packet.hpp"
#include <cstring>
#include <iostream>

namespace gs {
namespace net {
namespace async {

// ==========================================================

UDPServer::UDPServer(const Config& cfg) : cfg_(cfg) {}

UDPServer::~UDPServer() {
    Stop();
}

bool UDPServer::Start() {
    if (running_.exchange(true)) return false;

    loop_ = std::make_unique<AsyncEventLoop>();
    if (!loop_->Init()) {
        running_.store(false);
        return false;
    }

    udp_handle_ = new uv_udp_t;
    if (uv_udp_init(loop_->RawLoop(), udp_handle_) != 0) {
        delete udp_handle_;
        udp_handle_ = nullptr;
        running_.store(false);
        return false;
    }

    sockaddr_in bind_addr{};
    if (uv_ip4_addr(cfg_.host.c_str(), cfg_.port, &bind_addr) != 0) {
        uv_close(reinterpret_cast<uv_handle_t*>(udp_handle_),
                 [](uv_handle_t* h) { delete reinterpret_cast<uv_udp_t*>(h); });
        udp_handle_ = nullptr;
        running_.store(false);
        return false;
    }

    if (uv_udp_bind(udp_handle_, reinterpret_cast<const sockaddr*>(&bind_addr),
                    UV_UDP_REUSEADDR) != 0) {
        uv_close(reinterpret_cast<uv_handle_t*>(udp_handle_),
                 [](uv_handle_t* h) { delete reinterpret_cast<uv_udp_t*>(h); });
        udp_handle_ = nullptr;
        running_.store(false);
        return false;
    }

    udp_handle_->data = this;

    if (uv_udp_recv_start(udp_handle_,
                          [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
                              buf->base = new char[suggested_size];
                              buf->len = static_cast<unsigned long>(suggested_size);
                          },
                          [](uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
                             const sockaddr* addr, unsigned /*flags*/) {
                              auto* server = static_cast<UDPServer*>(handle->data);
                              if (nread > 0 && addr) {
                                  server->OnUDPRecv(*reinterpret_cast<const sockaddr_in*>(addr),
                                                    reinterpret_cast<const uint8_t*>(buf->base),
                                                    static_cast<size_t>(nread));
                              }
                              delete[] buf->base;
                          }) != 0) {
        uv_close(reinterpret_cast<uv_handle_t*>(udp_handle_),
                 [](uv_handle_t* h) { delete reinterpret_cast<uv_udp_t*>(h); });
        udp_handle_ = nullptr;
        running_.store(false);
        return false;
    }

    // 创建清理 timer（周期性回收过期会话）
    cleanup_timer_ = new uv_timer_t;
    uv_timer_init(loop_->RawLoop(), cleanup_timer_);
    cleanup_timer_->data = this;
    uv_timer_start(cleanup_timer_,
                   [](uv_timer_t* handle) {
                       auto* server = static_cast<UDPServer*>(handle->data);
                       server->CleanupExpiredSessions();
                   },
                   cfg_.session_timeout_ms, cfg_.session_timeout_ms);

    loop_thread_ = std::thread([this]() {
        loop_->Run();
    });

    return true;
}

void UDPServer::Stop() {
    if (!running_.exchange(false)) return;

    if (cleanup_timer_) {
        uv_timer_stop(cleanup_timer_);
        uv_close(reinterpret_cast<uv_handle_t*>(cleanup_timer_),
                 [](uv_handle_t* h) { delete reinterpret_cast<uv_timer_t*>(h); });
        cleanup_timer_ = nullptr;
    }

    if (udp_handle_) {
        uv_udp_recv_stop(udp_handle_);
        uv_close(reinterpret_cast<uv_handle_t*>(udp_handle_),
                 [](uv_handle_t* h) { delete reinterpret_cast<uv_udp_t*>(h); });
        udp_handle_ = nullptr;
    }

    if (loop_) {
        loop_->Stop();
    }
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

void UDPServer::SetDataCallback(DataCallback cb) {
    on_data_ = std::move(cb);
}

void UDPServer::Use(std::shared_ptr<Middleware> mw) {
    middlewares_.push_back(std::move(mw));
}

void UDPServer::OnUDPRecv(const sockaddr_in& addr, const uint8_t* data, size_t len) {
    if (len < gs::net::HEADER_SIZE) return;

    uint64_t addr_key = MakeAddrKey(addr);
    uint64_t now_ms = uv_now(loop_->RawLoop());

    std::shared_ptr<UDPPacketConnection> conn;
    {
        std::lock_guard<std::mutex> lk(session_mtx_);
        auto it = sessions_.find(addr_key);
        if (it != sessions_.end()) {
            conn = it->second;
            last_seen_ms_[addr_key] = now_ms;
        } else {
            // 连接数上限检查
            if (static_cast<int>(sessions_.size()) >= cfg_.max_conn) {
                return;
            }
            uint64_t id = ++conn_id_counter_;
            conn = std::make_shared<UDPPacketConnection>(id, addr, udp_handle_);
            sessions_[addr_key] = conn;
            last_seen_ms_[addr_key] = now_ms;
        }
    }

    if (!conn || conn->IsClosed()) return;

    Packet pkt;
    if (!DecodePacket(Buffer(std::vector<uint8_t>(data, data + len)), pkt)) {
        return;
    }

    // Middleware 链
    for (auto& mw : middlewares_) {
        if (!mw->OnPacket(conn.get(), pkt)) {
            return;
        }
    }

    if (on_data_) {
        on_data_(conn.get(), pkt);
    }
}

void UDPServer::CleanupExpiredSessions() {
    uint64_t now_ms = uv_now(loop_->RawLoop());
    std::vector<uint64_t> expired;

    {
        std::lock_guard<std::mutex> lk(session_mtx_);
        for (auto& [addr_key, last_ms] : last_seen_ms_) {
            if (now_ms - last_ms > cfg_.session_timeout_ms) {
                expired.push_back(addr_key);
            }
        }
        for (auto key : expired) {
            sessions_.erase(key);
            last_seen_ms_.erase(key);
        }
    }
}

uint64_t UDPServer::MakeAddrKey(const sockaddr_in& addr) {
    uint64_t key = static_cast<uint64_t>(addr.sin_addr.s_addr);
    key = (key << 16) | static_cast<uint64_t>(ntohs(addr.sin_port));
    return key;
}

} // namespace async
} // namespace net
} // namespace gs
