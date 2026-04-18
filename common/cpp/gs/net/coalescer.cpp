#include "coalescer.hpp"
#include <iostream>

namespace gs {
namespace net {

WriteCoalescer::WriteCoalescer(int interval_ms) : interval_ms_(interval_ms) {}

WriteCoalescer::~WriteCoalescer() {
    Stop();
}

void WriteCoalescer::Start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread([this] { FlushLoop(); });
}

void WriteCoalescer::Stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) {
        thread_.join();
    }
    DoFlush();
}

void WriteCoalescer::Enqueue(TCPConn* conn, const Packet& pkt) {
    std::lock_guard<std::mutex> lk(mtx_);
    Pending p;
    p.conn = conn;
    p.data = EncodePacket(pkt);
    pending_.push_back(std::move(p));
}

void WriteCoalescer::Broadcast(const std::vector<TCPConn*>& conns, const Packet& pkt) {
    auto encoded = EncodePacket(pkt);
    std::lock_guard<std::mutex> lk(mtx_);
    for (TCPConn* conn : conns) {
        Pending p;
        p.conn = conn;
        p.data = encoded;
        pending_.push_back(std::move(p));
    }
}

void WriteCoalescer::FlushLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
        DoFlush();
    }
}

void WriteCoalescer::DoFlush() {
    std::vector<Pending> local;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        local.swap(pending_);
    }

    // 按连接合并：同一个 conn 的多个包合并成一个 buffer
    std::unordered_map<TCPConn*, std::vector<uint8_t>> merged;
    for (auto& p : local) {
        if (!p.conn || p.conn->IsClosed()) continue;
        auto& buf = merged[p.conn];
        buf.insert(buf.end(), p.data.begin(), p.data.end());
    }

    for (auto& [conn, buf] : merged) {
        if (buf.empty() || !conn || conn->IsClosed()) continue;
        conn->Send(buf);
    }
}

} // namespace net
} // namespace gs