#include "room.hpp"
#include <iostream>

namespace gs {
namespace realtime {

Room::Room(const RoomConfig& cfg) : cfg_(cfg) {
    if (cfg_.enable_aoi) {
        spatial_ = std::make_unique<GridSpatialIndex>(cfg_.map_size_x, cfg_.map_size_z, cfg_.aoi_radius);
    }
}

void Room::OnMessage(Message* msg) {
    if (!msg) return;
    switch (msg->Type()) {
        case MsgType::PlayerEnter:
            OnPlayerEnter(static_cast<PlayerEnterMsg*>(msg));
            break;
        case MsgType::PlayerLeave:
            OnPlayerLeave(static_cast<PlayerLeaveMsg*>(msg));
            break;
        case MsgType::PlayerMove:
            OnPlayerMove(static_cast<PlayerMoveMsg*>(msg));
            break;
        case MsgType::PlayerAction:
            OnPlayerAction(static_cast<PlayerActionMsg*>(msg));
            break;
        case MsgType::RoomTick:
            Tick(static_cast<RoomTickMsg*>(msg)->now_ms);
            break;
        case MsgType::AsyncIOComplete:
            // TODO: 异步 IO 完成后的逻辑（如加载玩家数据后 enter）
            break;
        default:
            break;
    }
}

void Room::Tick(uint64_t now_ms) {
    if (last_tick_ms_ == 0) {
        last_tick_ms_ = now_ms;
        return;
    }
    // uint64_t delta_ms = now_ms - last_tick_ms_;
    last_tick_ms_ = now_ms;
    frame_seq_++;

    // 物理/逻辑更新（简化版：更新 spatial index）
    if (spatial_) {
        spatial_->Clear();
        for (const auto& [pid, state] : players_) {
            spatial_->Insert(pid, state.pos.x, state.pos.z);
        }
    }

    // 广播状态快照
    BroadcastSnapshot();
}

void Room::OnPlayerEnter(PlayerEnterMsg* msg) {
    if (IsFull()) {
        std::cerr << "Room " << cfg_.room_id << " is full, reject player " << msg->player_id << std::endl;
        return;
    }
    PlayerState state;
    state.player_id = msg->player_id;
    state.pos = msg->spawn_pos;
    players_[msg->player_id] = state;
    player_to_conn_[msg->player_id] = msg->conn_id;
    std::cout << "Player " << msg->player_id << " entered room " << cfg_.room_id << std::endl;
}

void Room::OnPlayerLeave(PlayerLeaveMsg* msg) {
    players_.erase(msg->player_id);
    player_to_conn_.erase(msg->player_id);
    std::cout << "Player " << msg->player_id << " left room " << cfg_.room_id << std::endl;
}

void Room::OnPlayerMove(PlayerMoveMsg* msg) {
    auto it = players_.find(msg->player_id);
    if (it == players_.end()) return;
    // 简化：直接设置目标位置（MVP 不做平滑插值）
    it->second.pos = msg->target_pos;
    it->second.yaw = msg->target_yaw;
}

void Room::OnPlayerAction(PlayerActionMsg* msg) {
    auto it = players_.find(msg->player_id);
    if (it == players_.end()) return;
    it->second.anim_state = msg->action_id;
    // TODO: 技能命中判定、伤害计算等
}

void Room::BroadcastSnapshot() {
    if (players_.empty() || !broadcast_cb_) return;

    RoomSnapshot snapshot;
    snapshot.room_id = cfg_.room_id;
    snapshot.frame_seq = frame_seq_;
    snapshot.timestamp_ms = last_tick_ms_;
    snapshot.players.reserve(players_.size());
    for (const auto& [_, state] : players_) {
        snapshot.players.push_back(state);
    }

    if (!cfg_.enable_aoi) {
        // 全图广播
        std::vector<uint64_t> conn_ids;
        conn_ids.reserve(player_to_conn_.size());
        for (const auto& [_, conn_id] : player_to_conn_) {
            conn_ids.push_back(conn_id);
        }
        broadcast_cb_(snapshot, conn_ids);
        return;
    }

    // AOI 过滤：为每个玩家单独计算视野内的其他玩家
    // 优化：SpatialIndex 的 QueryRange 一次查一批
    for (const auto& [pid, state] : players_) {
        auto nearby = spatial_->QueryRange(state.pos.x, state.pos.z, cfg_.aoi_radius);
        std::vector<uint64_t> target_conns;
        target_conns.reserve(nearby.size());
        for (uint64_t target_pid : nearby) {
            auto cit = player_to_conn_.find(target_pid);
            if (cit != player_to_conn_.end()) {
                target_conns.push_back(cit->second);
            }
        }
        // TODO: 当前实现是每个玩家发送一个 snapshot，后续改为批量合并
        RoomSnapshot per_player_snap = snapshot;
        per_player_snap.players.clear();
        for (uint64_t target_pid : nearby) {
            auto pit = players_.find(target_pid);
            if (pit != players_.end()) {
                per_player_snap.players.push_back(pit->second);
            }
        }
        broadcast_cb_(per_player_snap, target_conns);
    }
}

} // namespace realtime
} // namespace gs
