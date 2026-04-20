#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>

namespace gs {
namespace realtime {

// 消息类型
enum class MsgType : uint8_t {
    Invalid = 0,
    PlayerEnter,      // 玩家进入房间
    PlayerLeave,      // 玩家离开房间
    PlayerMove,       // 玩家移动
    PlayerAction,     // 玩家动作/技能
    SyncState,        // 状态同步广播（Compute Thread -> Gateway）
    AsyncIOComplete,  // 异步 IO 完成回调
    RoomTick,         // 房间帧 tick（内部驱动）
};

// Vec3 位置
struct Vec3 {
    float x = 0, y = 0, z = 0;
};

// 玩家状态
struct PlayerState {
    uint64_t player_id = 0;
    Vec3 pos;
    Vec3 vel;
    float yaw = 0;     // 朝向
    uint32_t hp = 100;
    uint32_t anim_state = 0;
};

// 房间状态快照（每帧广播）
struct RoomSnapshot {
    uint32_t room_id = 0;
    uint32_t frame_seq = 0;
    uint64_t timestamp_ms = 0;
    std::vector<PlayerState> players;
};

// 消息基类（多态消息，投递到 Compute Thread）
class Message {
public:
    virtual ~Message() = default;
    virtual MsgType Type() const = 0;
};

// PlayerEnterMsg
class PlayerEnterMsg : public Message {
public:
    MsgType Type() const override { return MsgType::PlayerEnter; }
    uint64_t player_id = 0;
    Vec3 spawn_pos;
    uint64_t conn_id = 0; // 对应 Gateway 的 client conn id
};

// PlayerLeaveMsg
class PlayerLeaveMsg : public Message {
public:
    MsgType Type() const override { return MsgType::PlayerLeave; }
    uint64_t player_id = 0;
};

// PlayerMoveMsg
class PlayerMoveMsg : public Message {
public:
    MsgType Type() const override { return MsgType::PlayerMove; }
    uint64_t player_id = 0;
    Vec3 target_pos;
    float target_yaw = 0;
};

// PlayerActionMsg
class PlayerActionMsg : public Message {
public:
    MsgType Type() const override { return MsgType::PlayerAction; }
    uint64_t player_id = 0;
    uint32_t action_id = 0;
    Vec3 target_pos;
};

// SyncStateMsg（Compute Thread -> Gateway，需要广播）
class SyncStateMsg : public Message {
public:
    MsgType Type() const override { return MsgType::SyncState; }
    RoomSnapshot snapshot;
    std::vector<uint64_t> target_conn_ids; // AOI 过滤后的目标连接
};

// AsyncIOCompleteMsg
class AsyncIOCompleteMsg : public Message {
public:
    MsgType Type() const override { return MsgType::AsyncIOComplete; }
    uint64_t req_id = 0;
    bool success = false;
    std::vector<uint8_t> data;
};

// RoomTickMsg（内部定时器触发）
class RoomTickMsg : public Message {
public:
    MsgType Type() const override { return MsgType::RoomTick; }
    uint64_t now_ms = 0;
};

using MessagePtr = std::unique_ptr<Message>;

} // namespace realtime
} // namespace gs
