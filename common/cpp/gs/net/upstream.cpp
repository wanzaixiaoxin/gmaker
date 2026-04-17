#include "upstream.hpp"
#include <iostream>

namespace gs {
namespace net {

UpstreamPool::UpstreamPool(DataCallback on_data) : on_data_(std::move(on_data)) {}

UpstreamPool::~UpstreamPool() {
    Stop();
}

void UpstreamPool::AddNode(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    auto node = std::make_unique<UpstreamNode>();
    node->host = host;
    node->port = port;
    node->client = std::make_unique<TCPClient>(host, port);
    nodes_.push_back(std::move(node));
}

bool UpstreamPool::Start() {
    if (nodes_.empty()) {
        std::cerr << "UpstreamPool: no nodes configured" << std::endl;
        return false;
    }

    // 尝试初始连接所有节点
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        for (auto& node : nodes_) {
            TryConnect(node.get());
        }
    }

    running_ = true;
    reconnect_thread_ = std::thread([this] { ReconnectLoop(); });
    return true;
}

void UpstreamPool::Stop() {
    running_ = false;
    reconnect_cv_.notify_all();
    if (reconnect_thread_.joinable()) {
        reconnect_thread_.join();
    }

    std::lock_guard<std::mutex> lk(nodes_mtx_);
    for (auto& node : nodes_) {
        if (node->client) {
            node->client->Close();
        }
    }
}

UpstreamNode* UpstreamPool::Pick() {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    if (nodes_.empty()) return nullptr;

    // 轮询：从当前索引开始找第一个健康的
    size_t start = rr_index_.fetch_add(1) % nodes_.size();
    for (size_t i = 0; i < nodes_.size(); ++i) {
        size_t idx = (start + i) % nodes_.size();
        if (nodes_[idx]->healthy.load() && nodes_[idx]->client && nodes_[idx]->client->IsConnected()) {
            nodes_[idx]->last_active = std::chrono::steady_clock::now();
            return nodes_[idx].get();
        }
    }
    return nullptr;
}

bool UpstreamPool::SendPacket(const Packet& pkt) {
    auto* node = Pick();
    if (!node || !node->client || !node->client->Conn()) {
        return false;
    }
    return node->client->Conn()->SendPacket(pkt);
}

void UpstreamPool::BroadcastToHealthy(const Packet& pkt) {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    for (auto& node : nodes_) {
        if (node->healthy.load() && node->client && node->client->Conn()) {
            node->client->Conn()->SendPacket(pkt);
        }
    }
}

size_t UpstreamPool::HealthyCount() const {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    size_t count = 0;
    for (const auto& node : nodes_) {
        if (node->healthy.load()) ++count;
    }
    return count;
}

size_t UpstreamPool::TotalCount() const {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    return nodes_.size();
}

void UpstreamPool::ReconnectLoop() {
    int interval_ms = 1000; // 初始 1s，指数退避到 30s
    const int max_interval_ms = 30000;

    while (running_.load()) {
        std::unique_lock<std::mutex> lk(reconnect_mtx_);
        reconnect_cv_.wait_for(lk, std::chrono::milliseconds(interval_ms),
                               [this] { return !running_.load(); });
        if (!running_.load()) break;

        bool any_reconnected = false;
        {
            std::lock_guard<std::mutex> node_lk(nodes_mtx_);
            for (auto& node : nodes_) {
                if (!node->healthy.load() && !node->connecting.load()) {
                    if (TryConnect(node.get())) {
                        any_reconnected = true;
                    }
                }
            }
        }

        if (any_reconnected) {
            interval_ms = 1000;
        } else if (HealthyCount() == 0) {
            interval_ms = std::min(interval_ms * 2, max_interval_ms);
        } else {
            interval_ms = 1000;
        }
    }
}

bool UpstreamPool::TryConnect(UpstreamNode* node) {
    node->connecting.store(true);
    bool ok = node->client->Connect();
    if (ok) {
        node->healthy.store(true);
        node->last_active = std::chrono::steady_clock::now();
        std::cout << "UpstreamPool: connected to " << node->host << ":" << node->port << std::endl;

        // 设置关闭回调：节点断开时标记为不健康
        node->client->SetCallbacks(
            on_data_,
            [node](TCPConn*) {
                node->healthy.store(false);
                std::cerr << "UpstreamPool: disconnected from " << node->host << ":" << node->port << std::endl;
            }
        );
    } else {
        node->healthy.store(false);
    }
    node->connecting.store(false);
    return ok;
}

} // namespace net
} // namespace gs
