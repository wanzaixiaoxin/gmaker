#include "compute_thread.hpp"
#include <iostream>
#include <chrono>

namespace gs {
namespace realtime {

constexpr uint64_t kTickIntervalMs = 16; // ~60fps

ComputeThread::ComputeThread() = default;

ComputeThread::~ComputeThread() {
    Stop();
}

void ComputeThread::Start() {
    if (running_.exchange(true)) return;
    thread_ = std::thread([this] { RunLoop(); });
}

void ComputeThread::Stop() {
    if (!running_.exchange(false)) return;
    {
        std::lock_guard<std::mutex> lk(msg_mtx_);
        msg_cv_.notify_all();
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

void ComputeThread::PushMessage(uint32_t room_id, MessagePtr msg) {
    {
        std::lock_guard<std::mutex> lk(msg_mtx_);
        msg_queue_.push({room_id, std::move(msg)});
    }
    msg_cv_.notify_one();
}

bool ComputeThread::CreateRoom(const RoomConfig& cfg) {
    if (rooms_.find(cfg.room_id) != rooms_.end()) {
        return false;
    }
    auto room = std::make_unique<Room>(cfg);
    room->SetBroadcastCallback([this, rid = cfg.room_id](const RoomSnapshot& snap, const std::vector<uint64_t>& conns) {
        if (output_cb_) {
            output_cb_(rid, snap, conns);
        }
    });
    rooms_[cfg.room_id] = std::move(room);
    return true;
}

void ComputeThread::SetOutputCallback(OutputCallback cb) {
    output_cb_ = std::move(cb);
}

void ComputeThread::RunLoop() {
    auto next_tick = std::chrono::steady_clock::now();
    while (running_.load()) {
        // 1. 处理所有待处理消息
        ProcessMessages();

        // 2. 驱动 Room Tick
        auto now = std::chrono::steady_clock::now();
        uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        TickRooms(now_ms);

        // 3. 精确睡眠到下一帧
        next_tick += std::chrono::milliseconds(kTickIntervalMs);
        std::this_thread::sleep_until(next_tick);
    }
}

void ComputeThread::ProcessMessages() {
    std::queue<Envelope> local;
    {
        std::lock_guard<std::mutex> lk(msg_mtx_);
        local.swap(msg_queue_);
    }
    while (!local.empty()) {
        auto& env = local.front();
        auto it = rooms_.find(env.room_id);
        if (it != rooms_.end()) {
            it->second->OnMessage(env.msg.get());
        }
        local.pop();
    }
}

void ComputeThread::TickRooms(uint64_t now_ms) {
    for (auto& [_, room] : rooms_) {
        room->Tick(now_ms);
    }
}

} // namespace realtime
} // namespace gs
