#include "client.hpp"

namespace gs {
namespace rpc {

Client::Client(net::IConnection* conn) : conn_(conn) {
    timeout_thread_ = std::thread([this]() { TimeoutLoop(); });
}

Client::~Client() {
    {
        std::lock_guard<std::mutex> lk(timeout_mtx_);
        timeout_stop_ = true;
    }
    timeout_cv_.notify_all();
    if (timeout_thread_.joinable()) {
        timeout_thread_.join();
    }

    std::lock_guard<std::mutex> lk(pending_mtx_);
    for (auto& [seq, prom] : pending_) {
        prom.set_exception(std::make_exception_ptr(
            std::runtime_error("rpc client destroyed")));
    }
    pending_.clear();
}

void Client::SetConn(net::IConnection* conn) {
    conn_ = conn;
}

void Client::OnPacket(net::Packet& pkt) {
    if (pkt.header.seq_id == 0) return;

    std::lock_guard<std::mutex> lk(pending_mtx_);
    auto it = pending_.find(pkt.header.seq_id);
    if (it != pending_.end()) {
        it->second.set_value(std::move(pkt));
        pending_.erase(it);
    }
}

std::future<net::Packet> Client::Call(uint32_t cmd_id,
                                      const std::vector<uint8_t>& payload,
                                      std::chrono::milliseconds timeout) {
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

    if (!conn_ || !conn_->SendPacket(pkt)) {
        std::lock_guard<std::mutex> lk(pending_mtx_);
        pending_.erase(seq);
        throw std::runtime_error("send failed");
    }

    if (timeout.count() > 0) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        {
            std::lock_guard<std::mutex> lk(timeout_mtx_);
            timeouts_[deadline] = seq;
        }
        timeout_cv_.notify_one();
    }

    return fut;
}

bool Client::FireForget(uint32_t cmd_id, const std::vector<uint8_t>& payload) {
    net::Packet pkt;
    pkt.header.length = net::HEADER_SIZE + static_cast<uint32_t>(payload.size());
    pkt.header.magic  = net::MAGIC_VALUE;
    pkt.header.cmd_id = cmd_id;
    pkt.header.seq_id = 0;
    pkt.header.flags  = static_cast<uint32_t>(net::Flag::RPC_FF);
    pkt.payload = net::Buffer::FromVector(payload);
    return conn_ && conn_->SendPacket(pkt);
}

uint32_t Client::NextSeqID() {
    return ++seq_id_;
}

void Client::TimeoutCleanup(uint32_t seq_id) {
    std::lock_guard<std::mutex> lk(pending_mtx_);
    auto it = pending_.find(seq_id);
    if (it != pending_.end()) {
        it->second.set_exception(std::make_exception_ptr(
            std::runtime_error("rpc call timeout")));
        pending_.erase(it);
    }
}

void Client::TimeoutLoop() {
    std::unique_lock<std::mutex> lk(timeout_mtx_);
    while (!timeout_stop_) {
        if (timeouts_.empty()) {
            timeout_cv_.wait(lk, [this] { return timeout_stop_ || !timeouts_.empty(); });
            continue;
        }

        auto it = timeouts_.begin();
        if (timeout_cv_.wait_until(lk, it->first, [this] { return timeout_stop_ || !timeouts_.empty(); })) {
            // 被唤醒，检查是否因为新 timeout 加入或停止
            if (timeout_stop_) break;
            continue;
        }

        // 超时到达
        uint32_t seq = it->second;
        timeouts_.erase(it);
        lk.unlock();
        TimeoutCleanup(seq);
        lk.lock();
    }

    // 退出前清理所有剩余超时
    lk.unlock();
    for (const auto& [deadline, seq] : timeouts_) {
        (void)deadline;
        TimeoutCleanup(seq);
    }
}

} // namespace rpc
} // namespace gs
