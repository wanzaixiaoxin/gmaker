#include "compute_pool.hpp"
#include <algorithm>
#include <thread>

namespace gs {
namespace realtime {

ComputePool::ComputePool(uint32_t thread_count)
    : thread_count_(thread_count > 0 ? thread_count :
                     std::max(1u, std::thread::hardware_concurrency())) {
    threads_.reserve(thread_count_);
    for (uint32_t i = 0; i < thread_count_; ++i) {
        threads_.push_back(std::make_unique<ComputeThread>());
    }
}

ComputePool::~ComputePool() {
    Stop();
}

void ComputePool::Start() {
    for (auto& t : threads_) {
        t->Start();
    }
}

void ComputePool::Stop() {
    for (auto& t : threads_) {
        t->Stop();
    }
}

void ComputePool::PushMessage(uint32_t room_id, MessagePtr msg) {
    PickThread(room_id).PushMessage(room_id, std::move(msg));
}

bool ComputePool::CreateRoom(const RoomConfig& cfg) {
    return PickThread(cfg.room_id).CreateRoom(cfg);
}

void ComputePool::SetOutputCallback(OutputCallback cb) {
    output_cb_ = std::move(cb);
    for (auto& t : threads_) {
        t->SetOutputCallback(output_cb_);
    }
}

ComputeThread& ComputePool::PickThread(uint32_t room_id) {
    return *threads_[room_id % thread_count_];
}

} // namespace realtime
} // namespace gs
