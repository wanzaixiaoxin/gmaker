#pragma once

#include "interface.hpp"
#include <etcd/SyncClient.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>

namespace gs {
namespace discovery {

// EtcdImpl 基于 etcd-cpp-apiv3 的 ServiceDiscovery 实现
class EtcdImpl : public ServiceDiscovery {
public:
    explicit EtcdImpl(const std::vector<std::string>& endpoints);
    ~EtcdImpl() override;

    bool Register(const NodeInfo& node) override;
    bool Deregister(const std::string& node_id) override;
    bool Discover(const std::string& service_type, std::vector<NodeInfo>& out) override;
    bool Watch(const std::vector<std::string>& service_types, EventCallback callback) override;
    void Close() override;

private:
    void LeaseKeepAliveLoop(int64_t lease_id);
    void WatchLoop(const std::string& service_type, EventCallback callback);
    static NodeInfo JsonToNodeInfo(const std::string& json_str);

    std::unique_ptr<etcd::SyncClient> client_;
    std::thread keepalive_thread_;
    std::atomic<bool> keepalive_stop_{false};

    std::mutex watch_mtx_;
    std::vector<std::thread> watch_threads_;
    std::atomic<bool> watch_stop_{false};

    std::string node_id_;
    std::string service_type_;
};

} // namespace discovery
} // namespace gs
