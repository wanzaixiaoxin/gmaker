#pragma once

#include "../iconnection.hpp"
#include "../packet.hpp"
#include <cstdint>
#include <atomic>
#include <uv.h>

namespace gs {
namespace net {
namespace async {

// UDPPacketConnection 基于 UDP 的伪连接实现
// UDP 本身是无连接的，这里用 (ip, port) 作为会话标识
class UDPPacketConnection : public IConnection {
public:
    UDPPacketConnection(uint64_t id, const sockaddr_in& addr, uv_udp_t* udp_handle);

    // IConnection 接口
    uint64_t ID() const override { return id_; }
    void Close() override { closed_.store(true); }
    void CloseAfterWrite() override { closed_.store(true); }
    bool SendPacket(const Packet& pkt) override;
    bool Send(std::vector<uint8_t> data) override;
    bool Send(const Buffer& data) override;
    bool SendBatch(const std::vector<Buffer>& buffers) override;

    const sockaddr_in& PeerAddr() const { return addr_; }
    bool IsClosed() const { return closed_.load(); }

private:
    uint64_t id_ = 0;
    sockaddr_in addr_;
    uv_udp_t* udp_handle_ = nullptr;
    std::atomic<bool> closed_{false};
};

} // namespace async
} // namespace net
} // namespace gs
