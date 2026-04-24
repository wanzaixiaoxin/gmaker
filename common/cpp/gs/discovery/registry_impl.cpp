#include "registry_impl.hpp"
#include "registry.pb.h"
#include <iostream>

namespace gs {
namespace discovery {

RegistryImpl::RegistryImpl(net::async::AsyncEventLoop* loop,
                           const std::vector<std::pair<std::string, uint16_t>>& addrs)
    : client_(std::make_unique<gs::registry::RegistryClient>(loop, addrs)) {}

RegistryImpl::~RegistryImpl() {
    Close();
}

bool RegistryImpl::Register(const NodeInfo& node) {
    if (!client_->Connect()) {
        std::cerr << "RegistryImpl::Register: connect failed" << std::endl;
        return false;
    }

    PbNodeInfo pb;
    pb.set_service_type(node.service_type);
    pb.set_node_id(node.node_id);
    pb.set_host(node.host);
    pb.set_port(node.port);
    for (const auto& [k, v] : node.metadata) {
        (*pb.mutable_metadata())[k] = v;
    }
    pb.set_load_score(node.load_score);
    pb.set_register_at(node.register_at);

    ::registry::Result res;
    int maxRetries = 5;
    bool ok = false;
    for (int i = 0; i < maxRetries; ++i) {
        if (client_->Register(pb, &res)) {
            ok = true;
            break;
        }
        if (i < maxRetries - 1) {
            std::this_thread::sleep_for(std::chrono::seconds(i + 1));
        }
    }
    if (!ok) {
        std::cerr << "RegistryImpl::Register: register failed after retries" << std::endl;
        return false;
    }

    // 启动心跳线程
    heartbeat_stop_ = false;
    heartbeat_thread_ = std::thread([this, node_id = node.node_id]() {
        HeartbeatLoop(node_id);
    });

    return true;
}

bool RegistryImpl::Deregister(const std::string& node_id) {
    (void)node_id;
    // 自研 Registry 目前无显式注销接口，依赖心跳超时自动清理
    heartbeat_stop_ = true;
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    return true;
}

bool RegistryImpl::Discover(const std::string& service_type, std::vector<NodeInfo>& out) {
    ::registry::NodeList list;
    if (!client_->Discover(service_type, &list)) {
        return false;
    }
    for (int i = 0; i < list.nodes_size(); ++i) {
        out.push_back(PbNodeToNodeInfo(list.nodes(i)));
    }
    return true;
}

bool RegistryImpl::Watch(const std::vector<std::string>& service_types, EventCallback callback) {
    ::registry::SubscribeRes res;
    bool ok = client_->Subscribe(service_types, &res,
        [this, callback](const PbNodeEvent& ev) {
            callback(PbEventToNodeEvent(ev));
        });
    return ok;
}

void RegistryImpl::Close() {
    heartbeat_stop_ = true;
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    client_->Close();
}

void RegistryImpl::HeartbeatLoop(const std::string& node_id) {
    while (!heartbeat_stop_) {
        ::registry::Result res;
        if (!client_->Heartbeat(node_id, &res)) {
            std::cerr << "RegistryImpl::HeartbeatLoop: heartbeat failed" << std::endl;
        }
        for (int i = 0; i < 50 && !heartbeat_stop_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

NodeInfo RegistryImpl::PbNodeToNodeInfo(const PbNodeInfo& pb) {
    NodeInfo node;
    node.service_type = pb.service_type();
    node.node_id = pb.node_id();
    node.host = pb.host();
    node.port = pb.port();
    for (const auto& [k, v] : pb.metadata()) {
        node.metadata[k] = v;
    }
    node.load_score = pb.load_score();
    node.register_at = pb.register_at();
    return node;
}

NodeEvent RegistryImpl::PbEventToNodeEvent(const PbNodeEvent& pb) {
    NodeEvent ev;
    switch (pb.type()) {
        case PbNodeEvent::JOIN:   ev.type = NodeEventType::Join;   break;
        case PbNodeEvent::LEAVE:  ev.type = NodeEventType::Leave;  break;
        case PbNodeEvent::UPDATE: ev.type = NodeEventType::Update; break;
        default:                  ev.type = NodeEventType::Update; break;
    }
    ev.node = PbNodeToNodeInfo(pb.node());
    return ev;
}

} // namespace discovery
} // namespace gs
