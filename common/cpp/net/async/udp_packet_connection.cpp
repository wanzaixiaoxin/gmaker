#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "udp_packet_connection.hpp"
#include "../packet.hpp"
#include <cstring>

namespace gs {
namespace net {
namespace async {

UDPPacketConnection::UDPPacketConnection(uint64_t id, const sockaddr_in& addr,
                                          uv_udp_t* udp_handle)
    : id_(id), addr_(addr), udp_handle_(udp_handle) {}

bool UDPPacketConnection::SendPacket(const Packet& pkt) {
    auto data = EncodePacket(pkt);
    return Send(data);
}

bool UDPPacketConnection::Send(std::vector<uint8_t> data) {
    return Send(Buffer::FromVector(std::move(data)));
}

bool UDPPacketConnection::Send(const Buffer& data) {
    if (closed_.load() || !udp_handle_) return false;

    auto* send_req = new uv_udp_send_t;
    auto* buf = new uv_buf_t;
    buf->base = new char[data.Size()];
    buf->len = static_cast<unsigned long>(data.Size());
    std::memcpy(buf->base, data.Data(), data.Size());
    send_req->data = buf;

    int r = uv_udp_send(send_req, udp_handle_, buf, 1,
                        reinterpret_cast<const sockaddr*>(&addr_),
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
        return false;
    }
    return true;
}

bool UDPPacketConnection::SendBatch(const std::vector<Buffer>& buffers) {
    if (closed_.load() || !udp_handle_) return false;

    bool ok = true;
    for (const auto& buf : buffers) {
        if (!Send(buf)) {
            ok = false;
        }
    }
    return ok;
}

} // namespace async
} // namespace net
} // namespace gs
