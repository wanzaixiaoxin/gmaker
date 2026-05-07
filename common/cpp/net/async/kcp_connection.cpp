#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "kcp_connection.hpp"
#include "event_loop.hpp"
#include "../packet.hpp"
#include <cstring>
#include <iostream>

namespace gs {
namespace net {
namespace async {

// ==========================================================

KCPPacketConnection::KCPPacketConnection(AsyncEventLoop* loop, uint64_t id,
                                         uint32_t conv,
                                         const sockaddr_in& peer_addr,
                                         uv_udp_t* udp_handle)
    : loop_(loop), udp_handle_(udp_handle), peer_addr_(peer_addr),
      id_(id), conv_(conv) {
    kcp_ = ikcp_create(conv, this);
    if (kcp_) {
        ikcp_setoutput(kcp_, &KCPPacketConnection::OutputCallback);
        // 游戏场景默认快速模式，关闭流控
        ikcp_nodelay(kcp_, 1, 10, 2, 1);
    }
    last_activity_ms_ = static_cast<uint32_t>(uv_now(loop_->RawLoop()));
}

KCPPacketConnection::~KCPPacketConnection() {
    if (kcp_) {
        ikcp_release(kcp_);
        kcp_ = nullptr;
    }
}

void KCPPacketConnection::SetCallbacks(DataCallback on_data, CloseCallback on_close) {
    on_data_ = on_data;
    on_close_ = on_close;
}

void KCPPacketConnection::Close() {
    bool expected = false;
    if (!closing_.compare_exchange_strong(expected, true)) return;
    DoClose();
}

void KCPPacketConnection::CloseAfterWrite() {
    close_after_write_.store(true);
    if (kcp_ && ikcp_waitsnd(kcp_) == 0) {
        Close();
    }
}

bool KCPPacketConnection::SendPacket(const Packet& pkt) {
    auto data = EncodePacket(pkt);
    return Send(data);
}

bool KCPPacketConnection::Send(std::vector<uint8_t> data) {
    return Send(Buffer::FromVector(std::move(data)));
}

bool KCPPacketConnection::Send(const Buffer& data) {
    if (closing_.load() || !kcp_) return false;

    int ret = ikcp_send(kcp_, reinterpret_cast<const char*>(data.Data()),
                        static_cast<int>(data.Size()));
    if (ret < 0) {
        return false;
    }

    if (close_after_write_.load() && ikcp_waitsnd(kcp_) == 0) {
        Close();
    }
    return true;
}

bool KCPPacketConnection::SendBatch(const std::vector<Buffer>& buffers) {
    if (closing_.load() || !kcp_) return false;

    bool ok = true;
    for (const auto& buf : buffers) {
        if (!Send(buf)) {
            ok = false;
        }
    }
    return ok;
}

// 收到 UDP 原始包（由 KCPServer 路由过来）
void KCPPacketConnection::OnKCPRecv(const uint8_t* data, size_t len) {
    if (closing_.load() || !kcp_) return;

    int ret = ikcp_input(kcp_, reinterpret_cast<const char*>(data),
                         static_cast<long>(len));
    if (ret < 0) {
        // KCP input 失败，可能是错误包或 conv 不匹配
        return;
    }

    last_activity_ms_ = static_cast<uint32_t>(uv_now(loop_->RawLoop()));

    // 尝试处理应用层数据
    ProcessKCPData();
}

void KCPPacketConnection::Update(uint32_t current_ms) {
    if (!kcp_ || closing_.load()) return;

    ikcp_update(kcp_, current_ms);

    // 处理可能因 update 而就绪的应用层数据
    ProcessKCPData();

    // 检查 close_after_write
    if (close_after_write_.load() && ikcp_waitsnd(kcp_) == 0) {
        Close();
    }
}

bool KCPPacketConnection::IsDead(uint32_t current_ms) const {
    if (closing_.load()) return true;
    // 60 秒无活动视为死亡
    return (current_ms - last_activity_ms_) > DEAD_TIMEOUT_MS;
}

// KCP output callback: KCP 需要发送底层 UDP 包时调用
int KCPPacketConnection::OutputCallback(const char* buf, int len,
                                        ikcpcb* /*kcp*/, void* user) {
    auto* conn = static_cast<KCPPacketConnection*>(user);
    conn->DoOutput(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(len));
    return 0;
}

void KCPPacketConnection::DoOutput(const uint8_t* data, size_t len) {
    if (!udp_handle_ || closing_.load()) return;

    auto* send_req = new uv_udp_send_t;
    auto* buf = new uv_buf_t;
    buf->base = new char[len];
    buf->len = static_cast<unsigned long>(len);
    std::memcpy(buf->base, data, len);

    send_req->data = buf;

    int r = uv_udp_send(send_req, udp_handle_, buf, 1,
                        reinterpret_cast<const sockaddr*>(&peer_addr_),
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

void KCPPacketConnection::ProcessKCPData() {
    if (!on_data_ || !kcp_) return;

    while (!closing_.load()) {
        int peek = ikcp_peeksize(kcp_);
        if (peek <= 0) break;

        std::vector<uint8_t> buf(peek);
        int len = ikcp_recv(kcp_, reinterpret_cast<char*>(buf.data()), peek);
        if (len <= 0) break;

        Packet pkt;
        if (!DecodePacket(Buffer::FromVector(std::move(buf)), pkt)) {
            continue;
        }

        on_data_(this, pkt);
    }
}

void KCPPacketConnection::DoClose() {
    if (closed_.exchange(true)) return;

    if (kcp_) {
        ikcp_release(kcp_);
        kcp_ = nullptr;
    }

    if (on_close_) {
        on_close_(this);
    }
}

} // namespace async
} // namespace net
} // namespace gs
