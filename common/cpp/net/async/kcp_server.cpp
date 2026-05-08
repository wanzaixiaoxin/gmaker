#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "kcp_server.hpp"
#include "event_loop.hpp"
#include "../packet.hpp"
#include <cstring>
#include <iostream>
#include <chrono>

namespace gs {
namespace net {
namespace async {

// ==========================================================

KCPServer::KCPServer(const Config& cfg) : cfg_(cfg) {}

KCPServer::~KCPServer() {
    Stop();
}

bool KCPServer::Start() {
    if (running_.exchange(true)) return false;

    loop_ = std::make_unique<AsyncEventLoop>();
    if (!loop_->Init()) {
        running_.store(false);
        return false;
    }

    // 创建 UDP socket
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
                              auto* server = static_cast<KCPServer*>(handle->data);
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

    // 创建 update timer（驱动所有 KCP 连接的 update）
    update_timer_ = new uv_timer_t;
    uv_timer_init(loop_->RawLoop(), update_timer_);
    update_timer_->data = this;
    uv_timer_start(update_timer_,
                   [](uv_timer_t* handle) {
                       auto* server = static_cast<KCPServer*>(handle->data);
                       server->OnUpdateTimer();
                   },
                   cfg_.kcp_interval, cfg_.kcp_interval);

    loop_thread_ = std::thread([this]() {
        loop_->Run();
    });

    return true;
}

void KCPServer::Stop() {
    if (!running_.exchange(false)) return;

    if (update_timer_) {
        uv_timer_stop(update_timer_);
        uv_close(reinterpret_cast<uv_handle_t*>(update_timer_),
                 [](uv_handle_t* h) { delete reinterpret_cast<uv_timer_t*>(h); });
        update_timer_ = nullptr;
    }

    // 关闭所有连接
    std::unordered_map<uint32_t, std::shared_ptr<KCPPacketConnection>> to_close;
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        to_close.swap(conns_by_conv_);
    }
    for (auto& [conv, conn] : to_close) {
        conn->Close();
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

void KCPServer::SetCallbacks(ConnectCallback on_connect,
                             DataCallback on_data,
                             CloseCallback on_close) {
    on_connect_ = on_connect;
    on_data_    = on_data;
    on_close_   = on_close;
}

void KCPServer::Use(std::shared_ptr<Middleware> mw) {
    middlewares_.push_back(std::move(mw));
}

void KCPServer::OnUDPRecv(const sockaddr_in& addr, const uint8_t* data, size_t len) {
    if (len < 4) return;

    // 尝试读取 conv（KCP 包前 4 字节小端序）
    uint32_t conv = ikcp_getconv(reinterpret_cast<const void*>(data));

    if (conv == 0) {
        // 可能是握手请求：首字节 0x00，总大小 17 字节
        if (len == 17 && data[0] == 0x00) {
            // 检查是否已存在该地址的连接
            uint64_t addr_key = MakeAddrKey(addr);
            {
                std::lock_guard<std::mutex> lk(addr_mtx_);
                if (conv_by_addr_.find(addr_key) != conv_by_addr_.end()) {
                    // 已存在，忽略重复握手
                    return;
                }
            }

            // 连接数上限检查
            {
                std::lock_guard<std::mutex> lk(conn_mtx_);
                if (static_cast<int>(conns_by_conv_.size()) >= cfg_.max_conn) {
                    return;
                }
            }

            uint32_t new_conv = AllocateConv();
            SendHandshakeResponse(addr, new_conv);

            // 创建 KCP 连接（但此时还不触发 on_connect，等正式 Handshake 包过来）
            uint64_t id = ++conn_id_counter_;
            auto conn = std::make_shared<KCPPacketConnection>(loop_.get(), id,
                                                               new_conv, addr,
                                                               udp_handle_);

            {
                std::lock_guard<std::mutex> lk(addr_mtx_);
                conv_by_addr_[addr_key] = new_conv;
            }
            {
                std::lock_guard<std::mutex> lk(conn_mtx_);
                conns_by_conv_[new_conv] = conn;
            }

            conn->SetCallbacks(
                [this](KCPPacketConnection* c, Packet& p) {
                    // Middleware 链
                    for (auto& mw : middlewares_) {
                        if (!mw->OnPacket(c, p)) {
                            return;
                        }
                    }
                    if (on_data_) on_data_(c, p);
                },
                [this](KCPPacketConnection* c) {
                    // 清理映射
                    uint32_t cv = c->Conv();
                    uint64_t ak = MakeAddrKey(c->PeerAddr());
                    {
                        std::lock_guard<std::mutex> lk(conn_mtx_);
                        conns_by_conv_.erase(cv);
                    }
                    {
                        std::lock_guard<std::mutex> lk(addr_mtx_);
                        conv_by_addr_.erase(ak);
                    }
                    if (on_close_) on_close_(c);
                }
            );

            if (on_connect_) {
                on_connect_(conn.get());
            }
        }
        return;
    }

    // conv > 0，查找对应连接
    std::shared_ptr<KCPPacketConnection> conn;
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        auto it = conns_by_conv_.find(conv);
        if (it != conns_by_conv_.end()) {
            conn = it->second;
        }
    }

    if (conn) {
        conn->OnKCPRecv(data, len);
    }
}

void KCPServer::OnUpdateTimer() {
    uint32_t now = static_cast<uint32_t>(uv_now(loop_->RawLoop()));

    // 复制连接列表避免在 Update 中持有锁
    std::vector<std::shared_ptr<KCPPacketConnection>> conns;
    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        conns.reserve(conns_by_conv_.size());
        for (auto& [conv, conn] : conns_by_conv_) {
            conns.push_back(conn);
        }
    }

    for (auto& conn : conns) {
        conn->Update(now);
    }

    CleanupDeadConnections();
}

void KCPServer::CleanupDeadConnections() {
    uint32_t now = static_cast<uint32_t>(uv_now(loop_->RawLoop()));
    std::vector<std::shared_ptr<KCPPacketConnection>> dead;

    {
        std::lock_guard<std::mutex> lk(conn_mtx_);
        for (auto it = conns_by_conv_.begin(); it != conns_by_conv_.end(); ) {
            if (it->second->IsDead(now)) {
                dead.push_back(it->second);
                uint64_t ak = MakeAddrKey(it->second->PeerAddr());
                {
                    std::lock_guard<std::mutex> lk2(addr_mtx_);
                    conv_by_addr_.erase(ak);
                }
                it = conns_by_conv_.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (auto& conn : dead) {
        conn->Close();
    }
}

uint32_t KCPServer::AllocateConv() {
    return conv_counter_.fetch_add(1);
}

uint64_t KCPServer::MakeAddrKey(const sockaddr_in& addr) {
    uint64_t key = static_cast<uint64_t>(addr.sin_addr.s_addr);
    key = (key << 16) | static_cast<uint64_t>(ntohs(addr.sin_port));
    return key;
}

void KCPServer::SendHandshakeResponse(const sockaddr_in& addr, uint32_t conv) {
    if (!udp_handle_) return;

    // 响应格式: [0x01][conv:4][server_random:16]
    std::vector<uint8_t> resp;
    resp.reserve(1 + 4 + 16);
    resp.push_back(0x01);
    resp.push_back(static_cast<uint8_t>((conv >> 0) & 0xFF));
    resp.push_back(static_cast<uint8_t>((conv >> 8) & 0xFF));
    resp.push_back(static_cast<uint8_t>((conv >> 16) & 0xFF));
    resp.push_back(static_cast<uint8_t>((conv >> 24) & 0xFF));

    // server_random (16 bytes, 简单用 conv 派生)
    for (int i = 0; i < 16; ++i) {
        resp.push_back(static_cast<uint8_t>((conv >> (i % 4)) ^ (i * 7)));
    }

    auto* send_req = new uv_udp_send_t;
    auto* buf = new uv_buf_t;
    buf->base = new char[resp.size()];
    buf->len = static_cast<unsigned long>(resp.size());
    std::memcpy(buf->base, resp.data(), resp.size());
    send_req->data = buf;

    int r = uv_udp_send(send_req, udp_handle_, buf, 1,
                        reinterpret_cast<const sockaddr*>(&addr),
                        [](uv_udp_send_t* req, int /*status*/) {
                            auto* b = static_cast<uv_buf_t*>(req->data);
                            delete[] b->base;
                            delete b;
                            delete req;
                        });
    if (r != 0) {
        delete[] buf->base;
        delete buf;
        delete send_req;
    }
}

} // namespace async
} // namespace net
} // namespace gs
