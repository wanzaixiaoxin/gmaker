#include "upstream.hpp"
#include <uv.h>
#include <iostream>

namespace gs {
namespace net {
namespace async {

static void OnTimer(uv_timer_t* handle) {
    auto* pool = static_cast<AsyncUpstreamPool*>(handle->data);
    if (pool) pool->OnReconnectTimer();
}

AsyncUpstreamPool::AsyncUpstreamPool(AsyncEventLoop* loop, DataCallback on_data)
    : loop_(loop), on_data_(std::move(on_data)) {}

AsyncUpstreamPool::~AsyncUpstreamPool() {
    Stop();
}

void AsyncUpstreamPool::AddNode(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    for (const auto& n : nodes_) {
        if (n->host == host && n->port == port) return; // 已存在
    }
    auto node = std::make_unique<UpstreamNode>();
    node->host = host;
    node->port = port;
    nodes_.push_back(std::move(node));
}

void AsyncUpstreamPool::RemoveNode(const std::string& host, uint16_t port) {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    for (auto it = nodes_.begin(); it != nodes_.end(); ++it) {
        if ((*it)->host == host && (*it)->port == port) {
            if ((*it)->conn) {
                (*it)->conn->Close();
            }
            nodes_.erase(it);
            return;
        }
    }
}

void AsyncUpstreamPool::SetOnNodeEvent(NodeEventCallback cb) {
    on_node_event_ = std::move(cb);
}

bool AsyncUpstreamPool::Start() {
    if (running_.exchange(true)) return false;
    if (!loop_ || !loop_->RawLoop()) {
        running_.store(false);
        return false;
    }

    reconnect_timer_ = new uv_timer_t;
    uv_timer_init(loop_->RawLoop(), reconnect_timer_);
    reconnect_timer_->data = this;
    uv_timer_start(reconnect_timer_, OnTimer, 0, 5000); // 立即执行，之后每 5s

    return true;
}

void AsyncUpstreamPool::Stop() {
    if (!running_.exchange(false)) return;

    if (reconnect_timer_) {
        uv_timer_stop(reconnect_timer_);
        if (!uv_is_closing((uv_handle_t*)reconnect_timer_)) {
            uv_close((uv_handle_t*)reconnect_timer_, [](uv_handle_t* h) {
                delete (uv_timer_t*)h;
            });
        }
        reconnect_timer_ = nullptr;
    }

    std::lock_guard<std::mutex> lk(nodes_mtx_);
    for (auto& node : nodes_) {
        if (node->conn) {
            node->conn->Close();
            node->conn.reset();
        }
        node->healthy.store(false);
        node->connecting.store(false);
    }
}

bool AsyncUpstreamPool::SendPacket(const Packet& pkt) {
    UpstreamNode* target = nullptr;
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        size_t count = nodes_.size();
        if (count == 0) return false;

        // 轮询查找健康节点
        size_t start = rr_index_.fetch_add(1) % count;
        for (size_t i = 0; i < count; ++i) {
            size_t idx = (start + i) % count;
            if (nodes_[idx]->healthy.load() && nodes_[idx]->conn) {
                target = nodes_[idx].get();
                break;
            }
        }
    }

    if (!target || !target->conn) return false;
    return target->conn->SendPacket(pkt);
}

void AsyncUpstreamPool::BroadcastToHealthy(const Packet& pkt) {
    std::vector<std::shared_ptr<AsyncTCPConnection>> targets;
    {
        std::lock_guard<std::mutex> lk(nodes_mtx_);
        for (const auto& node : nodes_) {
            if (node->healthy.load() && node->conn) {
                targets.push_back(node->conn);
            }
        }
    }
    for (auto& conn : targets) {
        conn->SendPacket(pkt);
    }
}

size_t AsyncUpstreamPool::HealthyCount() const {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    size_t n = 0;
    for (const auto& node : nodes_) {
        if (node->healthy.load()) ++n;
    }
    return n;
}

size_t AsyncUpstreamPool::TotalCount() const {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    return nodes_.size();
}

void AsyncUpstreamPool::OnReconnectTimer() {
    std::lock_guard<std::mutex> lk(nodes_mtx_);
    for (auto& node : nodes_) {
        if (!node->healthy.load() && !node->connecting.load()) {
            TryConnect(node.get());
        }
    }
}

bool AsyncUpstreamPool::TryConnect(UpstreamNode* node) {
    if (!node || !loop_) return false;
    node->connecting.store(true);

    uint64_t conn_id = reinterpret_cast<uint64_t>(node); // 用地址做唯一 ID（简化）
    auto conn = std::make_shared<AsyncTCPConnection>(loop_, conn_id);
    conn->SetCallbacks(
        [this](AsyncTCPConnection* c, Packet& p) {
            if (on_data_) on_data_(c, p);
        },
        [this, node](AsyncTCPConnection* c) {
            (void)c;
            UpdateHealth(node, false);
            node->connecting.store(false);
            node->conn.reset();
        }
    );
    conn->SetConnectCallback([this, node](bool success) {
        node->connecting.store(false);
        UpdateHealth(node, success);
    });

    if (!conn->Connect(node->host, node->port)) {
        node->connecting.store(false);
        return false;
    }

    node->conn = conn;
    return true;
}

std::string AsyncUpstreamPool::NodeAddr(const UpstreamNode* node) const {
    return node->host + ":" + std::to_string(node->port);
}

void AsyncUpstreamPool::UpdateHealth(UpstreamNode* node, bool healthy) {
    bool old = node->healthy.exchange(healthy);
    if (old != healthy && on_node_event_) {
        on_node_event_(NodeAddr(node), healthy);
    }
}

} // namespace async
} // namespace net
} // namespace gs
