#include "client.hpp"
#include <thread>

namespace gs {
namespace rpc {

Client::Client(net::TCPConn* conn) : conn_(conn) {
}

Client::~Client() {
    std::lock_guard<std::mutex> lk(pending_mtx_);
    for (auto& [seq, prom] : pending_) {
        prom.set_exception(std::make_exception_ptr(
            std::runtime_error("rpc client destroyed")));
    }
    pending_.clear();
}

void Client::SetConn(net::TCPConn* conn) {
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
    pkt.payload = payload;

    if (!conn_ || !conn_->SendPacket(pkt)) {
        std::lock_guard<std::mutex> lk(pending_mtx_);
        pending_.erase(seq);
        throw std::runtime_error("send failed");
    }

    if (timeout.count() > 0) {
        std::thread([this, seq, timeout]() {
            std::this_thread::sleep_for(timeout);
            TimeoutCleanup(seq);
        }).detach();
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
    pkt.payload = payload;
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

} // namespace rpc
} // namespace gs
