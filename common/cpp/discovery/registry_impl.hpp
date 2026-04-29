#pragma once

#include "interface.hpp"
#include "../registry/client.hpp"
#include <memory>
#include <thread>
#include <atomic>

namespace gs {
namespace discovery {

// RegistryImpl 基于自研 Registry 的 ServiceDiscovery 实现
typedef ::registry::NodeInfo PbNodeInfo;
typedef ::registry::NodeEvent PbNodeEvent;

class RegistryImpl : public ServiceDiscovery {
public:
    explicit RegistryImpl(net::async::AsyncEventLoop* loop,
                          const std::vector<std::pair<std::string, uint16_t>>& addrs);
    ~RegistryImpl() override;

    bool Register(const NodeInfo& node) override;
    bool Deregister(const std::string& node_id) override;
    bool Discover(const std::string& service_type, std::vector<NodeInfo>& out) override;
    bool Watch(const std::vector<std::string>& service_types, EventCallback callback) override;
    void Close() override;

private:
    void HeartbeatLoop(const std::string& node_id);
    static NodeInfo PbNodeToNodeInfo(const PbNodeInfo& pb);
    static NodeEvent PbEventToNodeEvent(const PbNodeEvent& pb);

    std::unique_ptr<gs::registry::RegistryClient> client_;
    std::thread heartbeat_thread_;
    std::atomic<bool> heartbeat_stop_{false};
};

} // namespace discovery
} // namespace gs
