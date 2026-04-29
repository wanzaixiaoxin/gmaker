#include "upstream_manager.hpp"
#include <iostream>
#include <sstream>

namespace gs {
namespace discovery {

UpstreamManager::UpstreamManager(ServiceDiscovery* sd,
                                 net::async::AsyncEventLoop* loop)
    : sd_(sd), loop_(loop) {}

UpstreamManager::~UpstreamManager() {
    Stop();
}

net::async::AsyncUpstreamPool* UpstreamManager::AddInterest(
    const std::string& service_type,
    net::async::AsyncUpstreamPool::DataCallback on_data) {

    std::lock_guard<std::mutex> lk(mtx_);
    auto pool = std::make_unique<net::async::AsyncUpstreamPool>(loop_, on_data);
    auto* raw = pool.get();
    pools_[service_type] = std::move(pool);
    return raw;
}

bool UpstreamManager::Start() {
    if (!sd_) {
        std::cerr << "UpstreamManager::Start: discovery is null" << std::endl;
        return false;
    }

    std::vector<std::string> types;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& [svc, _] : pools_) {
            types.push_back(svc);
        }
    }

    if (types.empty()) {
        return true;
    }

    std::stringstream ss;
    for (size_t i = 0; i < types.size(); ++i) {
        if (i > 0) ss << ",";
        ss << types[i];
    }
    std::cout << "[UpstreamManager] Subscribing to services: " << ss.str() << std::endl;

    // 先通过 Discover 获取全量快照初始化节点
    for (const auto& svc : types) {
        std::vector<NodeInfo> nodes;
        if (!sd_->Discover(svc, nodes)) {
            std::cerr << "[UpstreamManager] Discover " << svc << " failed" << std::endl;
            continue;
        }
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = pools_.find(svc);
        if (it == pools_.end()) continue;
        for (const auto& node : nodes) {
            it->second->AddNode(node.host, static_cast<uint16_t>(node.port));
            std::cout << "[UpstreamManager] Snapshot add node: " << svc << "/" << node.node_id
                      << " @ " << node.host << ":" << node.port << std::endl;
        }
    }

    // 启动所有连接池
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (auto& [svc_type, pool] : pools_) {
            if (!pool->Start()) {
                std::cerr << "UpstreamManager::Start: failed to start pool: " << svc_type << std::endl;
            } else {
                std::cout << "[UpstreamManager] Pool started: " << svc_type
                          << " (healthy=" << pool->HealthyCount() << "/" << pool->TotalCount() << ")" << std::endl;
            }
        }
    }

    // 启动 Watch 监听增量变更（在后台线程）
    std::thread([this, types]() {
        if (!sd_->Watch(types, [this](const NodeEvent& ev) { OnNodeEvent(ev); })) {
            std::cerr << "[UpstreamManager] Watch failed" << std::endl;
        }
    }).detach();

    return true;
}

void UpstreamManager::Stop() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& [svc_type, pool] : pools_) {
        pool->Stop();
        std::cout << "[UpstreamManager] Pool stopped: " << svc_type << std::endl;
    }
}

net::async::AsyncUpstreamPool* UpstreamManager::GetPool(const std::string& service_type) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = pools_.find(service_type);
    if (it != pools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void UpstreamManager::OnNodeEvent(const NodeEvent& ev) {
    if (ev.node.host.empty()) return;
    const std::string& svc_type = ev.node.service_type;

    std::lock_guard<std::mutex> lk(mtx_);
    auto it = pools_.find(svc_type);
    if (it == pools_.end()) return;

    auto* pool = it->second.get();
    switch (ev.type) {
        case NodeEventType::Join:
        case NodeEventType::Update:
            std::cout << "[UpstreamManager] Node JOIN/UPDATE: " << svc_type << "/" << ev.node.node_id
                      << " @ " << ev.node.host << ":" << ev.node.port << std::endl;
            pool->AddNode(ev.node.host, static_cast<uint16_t>(ev.node.port));
            break;
        case NodeEventType::Leave:
            std::cout << "[UpstreamManager] Node LEAVE: " << svc_type << "/" << ev.node.node_id
                      << " @ " << ev.node.host << ":" << ev.node.port << std::endl;
            pool->RemoveNode(ev.node.host, static_cast<uint16_t>(ev.node.port));
            break;
    }
}

} // namespace discovery
} // namespace gs
