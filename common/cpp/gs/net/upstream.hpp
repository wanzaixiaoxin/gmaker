#pragma once

#include "tcp_client.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gs {
namespace net {

// 上游节点描述
struct UpstreamNode {
    std::string host;
    uint16_t    port = 0;

    // 运行时状态（由 UpstreamPool 维护）
    std::atomic<bool> healthy{false};
    std::atomic<bool> connecting{false};
    std::chrono::steady_clock::time_point last_active;

    std::unique_ptr<TCPClient> client;
};

// UpstreamPool 维护到多个后端节点的连接池
// - 轮询负载均衡（Round-Robin）
// - 自动重连（指数退避）
// - 连接级健康状态跟踪
class UpstreamPool {
public:
    using DataCallback = std::function<void(TCPConn*, Packet&)>;

    explicit UpstreamPool(DataCallback on_data);
    ~UpstreamPool();

    // 添加上游节点（Start 前调用）
    void AddNode(const std::string& host, uint16_t port);

    // 启动后台重连/健康检查线程
    bool Start();

    // 停止所有连接与后台线程
    void Stop();

    // 轮询选择一个健康节点，返回 nullptr 表示无可用节点
    UpstreamNode* Pick();

    // 发送 Packet 到选中的健康节点
    bool SendPacket(const Packet& pkt);

    // 广播给所有健康节点
    void BroadcastToHealthy(const Packet& pkt);

    // 当前健康节点数
    size_t HealthyCount() const;

    // 总节点数
    size_t TotalCount() const;

private:
    void ReconnectLoop();
    bool TryConnect(UpstreamNode* node);

    std::vector<std::unique_ptr<UpstreamNode>> nodes_;
    mutable std::mutex nodes_mtx_;

    DataCallback on_data_;

    std::atomic<bool> running_{false};
    std::thread reconnect_thread_;
    std::condition_variable reconnect_cv_;
    std::mutex reconnect_mtx_;

    std::atomic<size_t> rr_index_{0}; // Round-Robin 索引
};

} // namespace net
} // namespace gs
