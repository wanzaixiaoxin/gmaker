#include "etcd_impl.hpp"
#include <json/json.h>
#include <iostream>
#include <chrono>
#include <sstream>

namespace gs {
namespace discovery {

EtcdImpl::EtcdImpl(const std::vector<std::string>& endpoints) {
    std::string endpoint = endpoints.empty() ? "http://127.0.0.1:2379" : endpoints[0];
    client_ = std::make_unique<etcd::SyncClient>(endpoint);
}

EtcdImpl::~EtcdImpl() {
    Close();
}

bool EtcdImpl::Register(const NodeInfo& node) {
    node_id_ = node.node_id;
    service_type_ = node.service_type;

    // 创建 10 秒 TTL 租约
    auto lease_resp = client_->leasegrant(10);
    if (!lease_resp.is_ok()) {
        std::cerr << "EtcdImpl::Register: grant lease failed: " << lease_resp.error_message() << std::endl;
        return false;
    }
    int64_t lease_id = lease_resp.value().lease;

    // 序列化节点信息为 JSON
    Json::Value json;
    json["service_type"] = node.service_type;
    json["node_id"] = node.node_id;
    json["host"] = node.host;
    json["port"] = static_cast<int>(node.port);
    json["load_score"] = static_cast<Json::UInt64>(node.load_score);
    json["register_at"] = static_cast<Json::UInt64>(node.register_at);
    Json::StreamWriterBuilder builder;
    std::string json_str = Json::writeString(builder, json);

    // Put with lease
    std::string key = "/services/" + node.service_type + "/" + node.node_id;
    auto put_resp = client_->put(key, json_str).set(lease_id);
    if (!put_resp.is_ok()) {
        std::cerr << "EtcdImpl::Register: put failed: " << put_resp.error_message() << std::endl;
        return false;
    }

    // 启动 KeepAlive 线程
    keepalive_stop_ = false;
    keepalive_thread_ = std::thread([this, lease_id]() {
        LeaseKeepAliveLoop(lease_id);
    });

    return true;
}

bool EtcdImpl::Deregister(const std::string& node_id) {
    (void)node_id;
    keepalive_stop_ = true;
    if (keepalive_thread_.joinable()) {
        keepalive_thread_.join();
    }
    return true;
}

bool EtcdImpl::Discover(const std::string& service_type, std::vector<NodeInfo>& out) {
    std::string key = "/services/" + service_type;
    auto resp = client_->get(key).keys();
    if (!resp.is_ok()) {
        std::cerr << "EtcdImpl::Discover: get failed: " << resp.error_message() << std::endl;
        return false;
    }

    auto keys = resp.value();
    for (const auto& k : keys) {
        auto val_resp = client_->get(k);
        if (val_resp.is_ok()) {
            out.push_back(JsonToNodeInfo(val_resp.value().as_string()));
        }
    }
    return true;
}

bool EtcdImpl::Watch(const std::vector<std::string>& service_types, EventCallback callback) {
    watch_stop_ = false;
    std::lock_guard<std::mutex> lk(watch_mtx_);
    for (const auto& svc : service_types) {
        watch_threads_.emplace_back([this, svc, callback]() {
            WatchLoop(svc, callback);
        });
    }
    return true;
}

void EtcdImpl::Close() {
    keepalive_stop_ = true;
    watch_stop_ = true;
    if (keepalive_thread_.joinable()) {
        keepalive_thread_.join();
    }
    std::lock_guard<std::mutex> lk(watch_mtx_);
    for (auto& t : watch_threads_) {
        if (t.joinable()) t.join();
    }
    watch_threads_.clear();
}

void EtcdImpl::LeaseKeepAliveLoop(int64_t lease_id) {
    while (!keepalive_stop_) {
        auto resp = client_->leasekeepalive(lease_id);
        if (!resp.is_ok()) {
            std::cerr << "EtcdImpl::LeaseKeepAliveLoop: keepalive failed: " << resp.error_message() << std::endl;
        }
        for (int i = 0; i < 50 && !keepalive_stop_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void EtcdImpl::WatchLoop(const std::string& service_type, EventCallback callback) {
    std::string key = "/services/" + service_type;
    auto async_client = std::make_unique<etcd::Client>(client_->get_url());
    async_client->watch(key, [callback, service_type](etcd::Response resp) {
        if (!resp.is_ok()) return;
        for (const auto& event : resp.events()) {
            NodeEvent ev;
            ev.node.service_type = service_type;
            if (event.event_type() == etcd::Event::EventType::PUT) {
                ev.type = NodeEventType::Update;
                ev.node = JsonToNodeInfo(event.kv().as_string());
            } else {
                ev.type = NodeEventType::Leave;
                std::string k = event.kv().key();
                auto pos = k.find_last_of('/');
                if (pos != std::string::npos) {
                    ev.node.node_id = k.substr(pos + 1);
                }
            }
            callback(ev);
        }
    }, true);
}

NodeInfo EtcdImpl::JsonToNodeInfo(const std::string& json_str) {
    NodeInfo node;
    Json::Value json;
    Json::CharReaderBuilder builder;
    std::string errors;
    std::istringstream iss(json_str);
    if (Json::parseFromStream(builder, iss, &json, &errors)) {
        node.service_type = json.get("service_type", "").asString();
        node.node_id = json.get("node_id", "").asString();
        node.host = json.get("host", "").asString();
        node.port = json.get("port", 0).asUInt();
        node.load_score = json.get("load_score", 0).asUInt64();
        node.register_at = json.get("register_at", 0).asUInt64();
    }
    return node;
}

} // namespace discovery
} // namespace gs
