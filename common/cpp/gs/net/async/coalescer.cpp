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
    pending_.push_back({conn, std::move(data)});
}

void AsyncWriteCoalescer::Broadcast(const std::vector<IConnection*>& conns, const Packet& pkt) {
    auto data = EncodePacket(pkt);
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto* conn : conns) {
        pending_.push_back({conn, data});
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
    for (auto& p : local) {
        if (p.conn) {
            p.conn->Send(std::move(p.data));
        }
    }
}

} // namespace async
} // namespace net
} // namespace gs
