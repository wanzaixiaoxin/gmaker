#pragma once

#include "compute_thread.hpp"
#include <vector>
#include <mutex>
#include <unordered_map>
#include <atomic>

namespace gs {
namespace realtime {

// ComputePool：管理多个 ComputeThread，每个 Room 分配到固定线程
// - Room 按 room_id hash 分配到 N 个 ComputeThread
// - 外部接口与 ComputeThread 兼容（Start/Stop/PushMessage/CreateRoom/SetOutputCallback）
// - 线程数由构造函数参数指定，默认为硬件并发数
class ComputePool {
public:
    // thread_count=0 表示使用 std::thread::hardware_concurrency()
    explicit ComputePool(uint32_t thread_count = 0);
    ~ComputePool();

    void Start();
    void Stop();

    // 按 room_id hash 路由到对应 ComputeThread
    void PushMessage(uint32_t room_id, MessagePtr msg);

    // 创建 Room 并分配到对应线程
    bool CreateRoom(const RoomConfig& cfg);

    // 所有线程共享同一个 OutputCallback
    using OutputCallback = ComputeThread::OutputCallback;
    void SetOutputCallback(OutputCallback cb);

private:
    ComputeThread& PickThread(uint32_t room_id);

    std::vector<std::unique_ptr<ComputeThread>> threads_;
    uint32_t thread_count_;
    OutputCallback output_cb_;
};

} // namespace realtime
} // namespace gs
