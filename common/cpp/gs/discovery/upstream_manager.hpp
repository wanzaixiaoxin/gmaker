#pragma once

#include "interface.hpp"
#include "../net/async/upstream.hpp"
#include "../net/async/event_loop.hpp"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>

namespace gs {
namespace discovery {

// UpstreamManager 上游服务管理器
// 与具体服务发现后端（Registry 或 etcd）解耦，通过 ServiceDiscovery 接口操作。
class UpstreamManager {
public:
    explicit UpstreamManager(ServiceDiscovery* sd,
                             net::async::AsyncEventLoop* loop);
    ~UpstreamManager();

    // 声明对某类上游服务的兴趣
    net::async::AsyncUpstreamPool* AddInterest(
        const std::string& service_type,
        net::async::AsyncUpstreamPool::DataCallback on_data);

    // 向 ServiceDiscovery 批量订阅所有关注的服务类型，
    // 通过 Discover 获取全量快照初始化各连接池节点，并启动连接池。
    bool Start();

    // 停止所有连接池
    void Stop();

    // 获取指定服务类型的连接池
    net::async::AsyncUpstreamPool* GetPool(const std::string& service_type);

private:
    void OnNodeEvent(const NodeEvent& ev);

    ServiceDiscovery* sd_ = nullptr;
    net::async::AsyncEventLoop* loop_ = nullptr;

    std::unordered_map<std::string, std::unique_ptr<net::async::AsyncUpstreamPool>> pools_;
    std::mutex mtx_;
};

} // namespace discovery
} // namespace gs
