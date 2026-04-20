#pragma once

#include "../packet.hpp"
#include "../iconnection.hpp"
#include "event_loop.hpp"
#include "tcp_connection.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <unordered_map>

namespace gs {
namespace net {
namespace async {

// 上游节点描述
struct UpstreamNode {
    std::string host;
    uint16_t    port = 0;

    // 运行时状态
    std::atomic<bool> healthy{false};
    std::atomic<bool> connecting{false};
    std::shared_ptr<AsyncTCPConnection> conn;
};

// AsyncUpstreamPool 基于 libuv 的异步上游连接池
// - 轮询负载均衡（Round-Robin）
// - 自动重连（由 uv_timer_t 驱动）
// - 连接级健康状态跟踪
class AsyncUpstreamPool {
public:
    using DataCallback = std::function<void(IConnection*, Packet&)>;
    using NodeEventCallback = std::function<void(const std::string& addr, bool healthy)>;

    explicit AsyncUpstreamPool(AsyncEventLoop* loop, DataCallback on_data);
    ~AsyncUpstreamPool();

    // 添加上游节点（Start 前调用）
    void AddNode(const std::string& host, uint16_t port);

    // 移除上游节点
    void RemoveNode(const std::string& host, uint16_t port);

    // 设置节点健康状态变更回调
    void SetOnNodeEvent(NodeEventCallback cb);

    // 启动重连定时器
    bool Start();

    // 停止所有连接与定时器
    void Stop();

    // 发送 Packet 到选中的健康节点（轮询）
    bool SendPacket(const Packet& pkt);

    // 广播给所有健康节点
    void BroadcastToHealthy(const Packet& pkt);

    // 当前健康节点数
    size_t HealthyCount() const;

    // 总节点数
    size_t TotalCount() const;

    // 供内部 uv_timer_t 回调调用
    void OnReconnectTimer();

private:
    bool TryConnect(UpstreamNode* node);
    std::string NodeAddr(const UpstreamNode* node) const;
    void UpdateHealth(UpstreamNode* node, bool healthy);

    AsyncEventLoop* loop_ = nullptr;
    std::vector<std::unique_ptr<UpstreamNode>> nodes_;
    mutable std::mutex nodes_mtx_;

    DataCallback on_data_;
    NodeEventCallback on_node_event_;

    std::atomic<bool> running_{false};
    std::atomic<size_t> rr_index_{0}; // Round-Robin 索引

    uv_timer_t* reconnect_timer_ = nullptr;
};

} // namespace async
} // namespace net
} // namespace gs
