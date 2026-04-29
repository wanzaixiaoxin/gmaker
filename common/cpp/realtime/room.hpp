#pragma once

#include "message.hpp"
#include "aoi.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <functional>

namespace gs {
namespace realtime {

// 房间配置
struct RoomConfig {
    uint32_t room_id = 0;
    uint32_t max_players = 20;       // 最大人数
    float map_size_x = 1000.0f;      // 地图尺寸
    float map_size_z = 1000.0f;
    uint32_t tick_rate_hz = 60;      // 帧率
    bool enable_aoi = true;          // 是否启用 AOI
    float aoi_radius = 200.0f;       // AOI 视野半径
};

// 广播回调：Compute Thread 产出消息，外部投递到 Gateway
using BroadcastCallback = std::function<void(const RoomSnapshot&, const std::vector<uint64_t>&)>;

class Room {
public:
    explicit Room(const RoomConfig& cfg);

    // 消息处理入口（由 Compute Thread 调用）
    void OnMessage(Message* msg);

    // 帧驱动（每 16.67ms 调用一次）
    void Tick(uint64_t now_ms);

    uint32_t RoomID() const { return cfg_.room_id; }
    size_t PlayerCount() const { return players_.size(); }
    bool IsFull() const { return players_.size() >= cfg_.max_players; }

    void SetBroadcastCallback(BroadcastCallback cb) { broadcast_cb_ = std::move(cb); }

private:
    void OnPlayerEnter(PlayerEnterMsg* msg);
    void OnPlayerLeave(PlayerLeaveMsg* msg);
    void OnPlayerMove(PlayerMoveMsg* msg);
    void OnPlayerAction(PlayerActionMsg* msg);

    void BroadcastSnapshot();

    RoomConfig cfg_;
    uint32_t frame_seq_ = 0;
    uint64_t last_tick_ms_ = 0;

    std::unordered_map<uint64_t, PlayerState> players_;
    std::unordered_map<uint64_t, uint64_t> player_to_conn_; // player_id -> conn_id

    // AOI 空间索引
    std::unique_ptr<SpatialIndex> spatial_;

    BroadcastCallback broadcast_cb_;
};

} // namespace realtime
} // namespace gs
