#include "upstream_manager.hpp"
#include "registry.pb.h"
#include <iostream>

namespace gs {
namespace registry {

UpstreamManager::UpstreamManager(RegistryClient* client,
                                 net::async::AsyncEventLoop* loop)
    : client_(client), loop_(loop) {}

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
    if (!client_) {
        std::cerr << "UpstreamManager::Start: client is null" << std::endl;
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

    ::registry::SubscribeRes res;
    if (!client_->Subscribe(types, &res,
                            [this](const ::registry::NodeEvent& ev) { OnNodeEvent(ev); })) {
        std::cerr << "UpstreamManager::Start: subscribe failed" << std::endl;
        return false;
    }

    // 根据全量快照初始化各 pool 的节点
    {
        std::lock_guard<std::mutex> lk(mtx_);
        for (const auto& [svc_type, list] : res.snapshot()) {
            auto it = pools_.find(svc_type);
            if (it == pools_.end()) continue;

            for (int i = 0; i < list.nodes_size(); ++i) {
                const auto& node = list.nodes(i);
                it->second->AddNode(node.host(), static_cast<uint16_t>(node.port()));
            }
        }

        // 启动所有连接池
        for (auto& [_, pool] : pools_) {
            if (!pool->Start()) {
                std::cerr << "UpstreamManager::Start: failed to start pool" << std::endl;
            }
        }
    }

    return true;
}

void UpstreamManager::Stop() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& [_, pool] : pools_) {
        pool->Stop();
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

void UpstreamManager::OnNodeEvent(const ::registry::NodeEvent& ev) {
    if (!ev.has_node()) return;
    const auto& node = ev.node();
    const std::string& svc_type = node.service_type();

    std::lock_guard<std::mutex> lk(mtx_);
    auto it = pools_.find(svc_type);
    if (it == pools_.end()) return;

    auto* pool = it->second.get();
    if (ev.type() == ::registry::NodeEvent::JOIN || ev.type() == ::registry::NodeEvent::UPDATE) {
        pool->AddNode(node.host(), static_cast<uint16_t>(node.port()));
    } else if (ev.type() == ::registry::NodeEvent::LEAVE) {
        pool->RemoveNode(node.host(), static_cast<uint16_t>(node.port()));
    }
}

} // namespace registry
} // namespace gs
