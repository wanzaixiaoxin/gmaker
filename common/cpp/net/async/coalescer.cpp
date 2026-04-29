#include "coalescer.hpp"
#include <uv.h>

namespace gs {
namespace net {
namespace async {

static void OnCoalescerTimer(uv_timer_t* handle) {
    auto* co = static_cast<AsyncWriteCoalescer*>(handle->data);
    if (co) co->OnTimer();
}

AsyncWriteCoalescer::AsyncWriteCoalescer(AsyncEventLoop* loop, int interval_ms)
    : loop_(loop), interval_ms_(interval_ms) {}

AsyncWriteCoalescer::~AsyncWriteCoalescer() {
    Stop();
}

bool AsyncWriteCoalescer::Start() {
    if (running_.exchange(true)) return false;
    if (!loop_ || !loop_->RawLoop()) {
        running_.store(false);
        return false;
    }

    timer_ = new uv_timer_t;
    uv_timer_init(loop_->RawLoop(), timer_);
    timer_->data = this;
    uv_timer_start(timer_, OnCoalescerTimer, static_cast<uint64_t>(interval_ms_), static_cast<uint64_t>(interval_ms_));
    return true;
}

void AsyncWriteCoalescer::Stop() {
    if (!running_.exchange(false)) return;

    if (timer_) {
        uv_timer_stop(timer_);
        if (!uv_is_closing((uv_handle_t*)timer_)) {
            uv_close((uv_handle_t*)timer_, [](uv_handle_t* h) {
                delete (uv_timer_t*)h;
            });
        }
        timer_ = nullptr;
    }
}

void AsyncWriteCoalescer::Enqueue(IConnection* conn, const Packet& pkt) {
    auto data = EncodePacket(pkt);
    std::lock_guard<std::mutex> lk(mtx_);
    Pending pending;
    pending.conn = conn;
    pending.data = data;
    pending_.push_back(std::move(pending));
}

void AsyncWriteCoalescer::Enqueue(std::shared_ptr<IConnection> conn, const Packet& pkt) {
    auto data = EncodePacket(pkt);
    std::lock_guard<std::mutex> lk(mtx_);
    Pending pending;
    pending.conn = conn.get();
    pending.owner = std::move(conn);
    pending.data = data;
    pending_.push_back(std::move(pending));
}

void AsyncWriteCoalescer::Enqueue(IConnection* conn, const Buffer& data) {
    std::lock_guard<std::mutex> lk(mtx_);
    Pending pending;
    pending.conn = conn;
    pending.data = data.Slice(0, data.Size());
    pending_.push_back(std::move(pending));
}

void AsyncWriteCoalescer::Enqueue(std::shared_ptr<IConnection> conn, const Buffer& data) {
    std::lock_guard<std::mutex> lk(mtx_);
    Pending pending;
    pending.conn = conn.get();
    pending.owner = std::move(conn);
    pending.data = data.Slice(0, data.Size());
    pending_.push_back(std::move(pending));
}

void AsyncWriteCoalescer::Broadcast(const std::vector<IConnection*>& conns, const Packet& pkt) {
    auto data = EncodePacket(pkt);
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto* conn : conns) {
        Pending pending;
        pending.conn = conn;
        pending.data = data.Slice(0, data.Size());
        pending_.push_back(std::move(pending));
    }
}

void AsyncWriteCoalescer::OnTimer() {
    DoFlush();
}

void AsyncWriteCoalescer::DoFlush() {
    std::vector<Pending> local;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        local.swap(pending_);
    }

    // 按 conn 聚合，批量 SendBatch 减少 syscall 和 Post 次数
    std::unordered_map<IConnection*, std::vector<Buffer>> batches;
    std::unordered_map<IConnection*, std::shared_ptr<IConnection>> owners;
    for (auto& p : local) {
        if (p.conn) {
            if (p.owner) {
                owners[p.conn] = std::move(p.owner);
            }
            batches[p.conn].push_back(std::move(p.data));
        }
    }
    for (auto& [conn, buffers] : batches) {
        conn->SendBatch(buffers);
    }
}

} // namespace async
} // namespace net
} // namespace gs
