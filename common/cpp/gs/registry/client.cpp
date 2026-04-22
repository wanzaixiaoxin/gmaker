#include "client.hpp"
#include "registry.pb.h"
#include <iostream>
#include <thread>

namespace gs {
namespace registry {

RegistryClient::RegistryClient(net::async::AsyncEventLoop* loop,
                               const std::vector<std::pair<std::string, uint16_t>>& addrs) {
    if (loop) {
        loop_ = loop;
    } else {
        owned_loop_ = std::make_unique<net::async::AsyncEventLoop>();
        if (owned_loop_->Init()) {
            loop_ = owned_loop_.get();
        }
    }

    pool_ = std::make_unique<net::async::AsyncUpstreamPool>(
        loop_,
        [this](net::IConnection* c, net::Packet& p) { OnPacket(c, p); }
    );
    pool_->SetOnNodeEvent(
        [this](const std::string& addr, bool healthy) { OnNodeEvent(addr, healthy); }
    );
    for (const auto& [host, port] : addrs) {
        pool_->AddNode(host, port);
    }
}

RegistryClient::~RegistryClient() {
    Close();
}

bool RegistryClient::Connect() {
    if (owned_loop_ && loop_) {
        // 启动内部事件循环线程
        std::thread([this]() {
            owned_loop_->Run();
        }).detach();
    }
    if (!pool_->Start()) {
        return false;
    }
    // 等待连接建立（异步连接最多等 3 秒）
    for (int i = 0; i < 30; ++i) {
        if (IsConnected()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return true;
}

void RegistryClient::Close() {
    pool_->Stop();

    // 清理所有 pending promise，避免阻塞
    std::lock_guard<std::mutex> lk(pending_mtx_);
    for (auto& [seq, prom] : pending_) {
        prom.set_exception(std::make_exception_ptr(
            std::runtime_error("registry client closed")));
    }
    pending_.clear();

    if (owned_loop_ && loop_) {
        loop_->Stop();
    }
}

bool RegistryClient::IsConnected() const {
    return pool_->HealthyCount() > 0;
}

bool RegistryClient::Register(const ::registry::NodeInfo& node, ::registry::Result* out) {
    std::vector<uint8_t> payload(node.ByteSizeLong());
    if (!node.SerializeToArray(payload.data(), static_cast<int>(payload.size()))) {
        std::cerr << "RegistryClient::Register: serialize failed" << std::endl;
        return false;
    }

    net::Packet res_pkt;
    if (!Call(CMD_REGISTER, payload, &res_pkt)) {
        return false;
    }
    return out->ParseFromArray(res_pkt.payload.Data(), static_cast<int>(res_pkt.payload.Size()));
}

bool RegistryClient::Heartbeat(const std::string& node_id, ::registry::Result* out) {
    ::registry::NodeId req;
    req.set_node_id(node_id);
    std::vector<uint8_t> payload(req.ByteSizeLong());
    if (!req.SerializeToArray(payload.data(), static_cast<int>(payload.size()))) {
        std::cerr << "RegistryClient::Heartbeat: serialize failed" << std::endl;
        return false;
    }

    net::Packet res_pkt;
    if (!Call(CMD_HEARTBEAT, payload, &res_pkt)) {
        return false;
    }
    return out->ParseFromArray(res_pkt.payload.Data(), static_cast<int>(res_pkt.payload.Size()));
}

bool RegistryClient::Discover(const std::string& service_type, ::registry::NodeList* out) {
    ::registry::ServiceType req;
    req.set_service_type(service_type);
    std::vector<uint8_t> payload(req.ByteSizeLong());
    if (!req.SerializeToArray(payload.data(), static_cast<int>(payload.size()))) {
        std::cerr << "RegistryClient::Discover: serialize failed" << std::endl;
        return false;
    }

    net::Packet res_pkt;
    if (!Call(CMD_DISCOVER, payload, &res_pkt)) {
        return false;
    }
    return out->ParseFromArray(res_pkt.payload.Data(), static_cast<int>(res_pkt.payload.Size()));
}

bool RegistryClient::Watch(const std::string& service_type, EventCallback on_event) {
    {
        std::lock_guard<std::mutex> lk(watch_mtx_);
        event_cb_ = on_event;
        watch_types_.insert(service_type);
    }

    ::registry::ServiceType req;
    req.set_service_type(service_type);
    std::vector<uint8_t> payload(req.ByteSizeLong());
    if (!req.SerializeToArray(payload.data(), static_cast<int>(payload.size()))) {
        std::cerr << "RegistryClient::Watch: serialize failed" << std::endl;
        return false;
    }
    return FireForget(CMD_WATCH, payload);
}

void RegistryClient::OnPacket(net::IConnection* conn, net::Packet& pkt) {
    (void)conn;
    // NodeEvent 是服务端推送，不走 pending 映射
    if (pkt.header.cmd_id == CMD_NODE_EVENT) {
        ::registry::NodeEvent ev;
        if (ev.ParseFromArray(pkt.payload.Data(), static_cast<int>(pkt.payload.Size()))) {
            std::lock_guard<std::mutex> lk(watch_mtx_);
            if (event_cb_) {
                event_cb_(ev);
            }
        }
        return;
    }

    // RPC 响应分发到 pending
    if (pkt.header.seq_id == 0) return;

    std::lock_guard<std::mutex> lk(pending_mtx_);
    auto it = pending_.find(pkt.header.seq_id);
    if (it != pending_.end()) {
        it->second.set_value(std::move(pkt));
        pending_.erase(it);
    }
}

void RegistryClient::OnClose(net::IConnection* conn) {
    (void)conn;
}

void RegistryClient::OnNodeEvent(const std::string& addr, bool healthy) {
    (void)addr;
    if (healthy) {
        // 节点恢复后，重新发送所有 Watch 请求
        ResendWatches();
    }
}

uint32_t RegistryClient::NextSeqID() {
    return ++seq_id_;
}

bool RegistryClient::Call(uint32_t cmd_id, const std::vector<uint8_t>& payload,
                          net::Packet* out_pkt, std::chrono::milliseconds timeout) {
    uint32_t seq = NextSeqID();
    std::promise<net::Packet> prom;
    auto fut = prom.get_future();

    {
        std::lock_guard<std::mutex> lk(pending_mtx_);
        pending_[seq] = std::move(prom);
    }

    net::Packet pkt;
    pkt.header.length = net::HEADER_SIZE + static_cast<uint32_t>(payload.size());
    pkt.header.magic  = net::MAGIC_VALUE;
    pkt.header.cmd_id = cmd_id;
    pkt.header.seq_id = seq;
    pkt.header.flags  = static_cast<uint32_t>(net::Flag::RPC_REQ);
    pkt.payload = net::Buffer::FromVector(payload);

    // 发送重试：异步连接可能尚未就绪，最多重试 5 次
    bool sent = false;
    for (int retry = 0; retry < 5; ++retry) {
        if (pool_->SendPacket(pkt)) {
            sent = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    if (!sent) {
        std::lock_guard<std::mutex> lk(pending_mtx_);
        pending_.erase(seq);
        std::cerr << "RegistryClient::Call: no healthy node available" << std::endl;
        return false;
    }

    if (fut.wait_for(timeout) != std::future_status::ready) {
        std::lock_guard<std::mutex> lk(pending_mtx_);
        pending_.erase(seq);
        std::cerr << "RegistryClient::Call: timeout" << std::endl;
        return false;
    }

    try {
        *out_pkt = fut.get();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "RegistryClient::Call: exception: " << e.what() << std::endl;
        return false;
    }
}

bool RegistryClient::FireForget(uint32_t cmd_id, const std::vector<uint8_t>& payload) {
    net::Packet pkt;
    pkt.header.length = net::HEADER_SIZE + static_cast<uint32_t>(payload.size());
    pkt.header.magic  = net::MAGIC_VALUE;
    pkt.header.cmd_id = cmd_id;
    pkt.header.seq_id = 0;
    pkt.header.flags  = static_cast<uint32_t>(net::Flag::RPC_FF);
    pkt.payload = net::Buffer::FromVector(payload);
    return pool_->SendPacket(pkt);
}

void RegistryClient::ResendWatches() {
    std::lock_guard<std::mutex> lk(watch_mtx_);
    for (const auto& service_type : watch_types_) {
        ::registry::ServiceType req;
        req.set_service_type(service_type);
        std::vector<uint8_t> payload(req.ByteSizeLong());
        if (req.SerializeToArray(payload.data(), static_cast<int>(payload.size()))) {
            FireForget(CMD_WATCH, payload);
        }
    }
}

} // namespace registry
} // namespace gs
