#pragma once

#include "../iconnection.hpp"
#include "../packet.hpp"
#include "event_loop.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <uv.h>

extern "C" {
#include "../../../3rd/kcp/ikcp.h"
}

namespace gs {
namespace net {
namespace async {

// KCPPacketConnection 基于 KCP (over UDP) 的连接实现
// 实现 IConnection 接口，与上层 Middleware 解耦
class KCPPacketConnection : public IConnection,
                            public std::enable_shared_from_this<KCPPacketConnection> {
public:
    using DataCallback = std::function<void(KCPPacketConnection*, Packet&)>;
    using CloseCallback = std::function<void(KCPPacketConnection*)>;

    KCPPacketConnection(AsyncEventLoop* loop, uint64_t id, uint32_t conv,
                        const sockaddr_in& peer_addr, uv_udp_t* udp_handle);
    ~KCPPacketConnection();

    KCPPacketConnection(const KCPPacketConnection&) = delete;
    KCPPacketConnection& operator=(const KCPPacketConnection&) = delete;

    // IConnection 接口
    uint64_t ID() const override { return id_; }
    void Close() override;
    void CloseAfterWrite() override;
    bool SendPacket(const Packet& pkt) override;
    bool Send(std::vector<uint8_t> data) override;
    bool Send(const Buffer& data) override;
    bool SendBatch(const std::vector<Buffer>& buffers) override;

    // KCP 特有
    void SetCallbacks(DataCallback on_data, CloseCallback on_close);
    void OnKCPRecv(const uint8_t* data, size_t len);  // KCP 产出的应用层数据
    void Update(uint32_t current_ms);
    bool IsDead(uint32_t current_ms) const;
    uint32_t Conv() const { return conv_; }
    const sockaddr_in& PeerAddr() const { return peer_addr_; }
    bool IsClosing() const { return closing_.load(); }

private:
    static int OutputCallback(const char* buf, int len, ikcpcb* kcp, void* user);
    void DoOutput(const uint8_t* data, size_t len);
    void ProcessKCPData();
    void DoClose();

    AsyncEventLoop* loop_ = nullptr;
    uv_udp_t* udp_handle_ = nullptr;
    sockaddr_in peer_addr_;
    uint64_t id_ = 0;
    uint32_t conv_ = 0;
    ikcpcb* kcp_ = nullptr;

    std::atomic<bool> closed_{false};
    std::atomic<bool> closing_{false};
    std::atomic<bool> close_after_write_{false};

    DataCallback on_data_;
    CloseCallback on_close_;

    // 写队列（KCP 发送）
    std::mutex write_mtx_;
    std::queue<Packet> write_queue_;

    // 用于 uv_close 完成前保持对象存活
    std::shared_ptr<KCPPacketConnection> keep_alive_;

    static constexpr uint32_t DEAD_TIMEOUT_MS = 60000;  // 60s 无活动视为死亡
    uint32_t last_activity_ms_ = 0;
};

} // namespace async
} // namespace net
} // namespace gs
