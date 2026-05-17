#pragma once

#include "message.hpp"
#include "room.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <atomic>
#include <memory>
#include <functional>

namespace gs {
namespace realtime {

// 投递到 Compute Thread 的外部消息
struct Envelope {
    uint32_t room_id = 0;
    MessagePtr msg;
};

// Room 工厂类型：接受 RoomConfig 返回 Room 实例
using RoomFactory = std::function<std::unique_ptr<Room>(const RoomConfig&)>;

// ComputeThread：单线程事件循环，管理多个 Room
// - 所有 Room 逻辑都在同一线程执行，无锁
// - 外部通过 PushMessage 投递消息（线程安全队列）
// - 内部定时器驱动 Tick（60fps）
class ComputeThread {
public:
    ComputeThread();
    ~ComputeThread();

    // 启动事件循环
    void Start();

    // 停止事件循环
    void Stop();

    // 投递消息（线程安全，可从 IO Thread 调用）
    void PushMessage(uint32_t room_id, MessagePtr msg);

    // 创建 Room（必须在 Start 前调用，Start 后调用会返回 false）
    bool CreateRoom(const RoomConfig& cfg);

    // 使用自定义工厂创建 Room
    bool CreateRoom(const RoomConfig& cfg, RoomFactory factory);

    // 设置广播回调（Compute Thread 产出消息 -> 外部投递到 Gateway）
    using OutputCallback = std::function<void(uint32_t room_id, const RoomSnapshot&, const std::vector<uint64_t>&)>;
    void SetOutputCallback(OutputCallback cb);

    // 设置默认 Room 工厂
    void SetRoomFactory(RoomFactory factory);

private:
    void RunLoop();
    void ProcessMessages();
    void TickRooms(uint64_t now_ms);

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> started_{false};

    // 消息队列（外部 -> Compute Thread）
    std::mutex msg_mtx_;
    std::condition_variable msg_cv_;
    std::queue<Envelope> msg_queue_;

    // Room 管理（Start 前由外部线程写入，Start 后仅由 RunLoop 读取）
    std::unordered_map<uint32_t, std::unique_ptr<Room>> rooms_;

    OutputCallback output_cb_;
    RoomFactory room_factory_;
};

} // namespace realtime
} // namespace gs
