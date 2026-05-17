#pragma once

#include "battle_types.hpp"
#include "entity.hpp"
#include <cstdint>
#include <vector>
#include <deque>
#include <unordered_map>

namespace gs {
namespace realtime {

// ──────────────────────────────────────────────
// 单个实体的快照数据
// ──────────────────────────────────────────────
struct EntitySnapshot {
    uint64_t     entity_id = 0;
    EntityType   type = EntityType::Invalid;
    TeamSide     team = TeamSide::None;
    Vec3         pos;
    Vec3         vel;
    float        yaw = 0;
    int32_t      hp = 0;
    int32_t      max_hp = 0;
    EntityState  state = EntityState::Idle;

    // 英雄特有
    uint64_t     owner_player_id = 0;
    uint32_t     kills = 0;
    uint32_t     deaths = 0;
    uint32_t     assists = 0;
    uint32_t     gold = 0;
    uint32_t     level = 1;

    // 防御塔特有
    TowerGrade   tower_grade = TowerGrade::Outer;

    // 小兵特有
    MinionType   minion_type = MinionType::Melee;

    // 弹道特有
    uint64_t     caster_id = 0;
    bool         reached = false;
};

// ──────────────────────────────────────────────
// 完整的战斗快照
// ──────────────────────────────────────────────
struct BattleCheckpoint {
    uint32_t frame = 0;
    uint64_t timestamp_ms = 0;
    BattleState battle_state = BattleState::Fighting;

    std::vector<EntitySnapshot> entities;

    // 计分信息
    uint32_t blue_kills = 0;
    uint32_t red_kills = 0;
    uint32_t blue_towers_alive = 0;
    uint32_t red_towers_alive = 0;
};

// ──────────────────────────────────────────────
// 快照管理器
// ──────────────────────────────────────────────
// 职责：
//   1. 每隔 N 帧保存一次完整快照（环形缓冲）
//   2. 提供最近快照查询（用于重连）
//   3. 从 EntityManager 生成快照
//   4. 从快照恢复 EntityManager 状态
class CheckpointManager {
public:
    explicit CheckpointManager(uint32_t interval_frames = 60, uint32_t max_checkpoints = 10);

    // 从当前实体状态生成快照
    BattleCheckpoint MakeSnapshot(uint32_t frame, uint64_t timestamp_ms,
                                  BattleState battle_state,
                                  const EntityManager& entity_mgr);

    // 保存快照到环形缓冲
    void SaveSnapshot(const BattleCheckpoint& cp);

    // 获取最近的快照（用于重连）
    const BattleCheckpoint* GetLatestCheckpoint() const;

    // 获取指定帧之后最近的快照
    const BattleCheckpoint* GetCheckpointAfter(uint32_t frame) const;

    // 获取指定帧的快照（精确匹配）
    const BattleCheckpoint* GetCheckpoint(uint32_t frame) const;

    // 获取所有保存的快照数量
    size_t CheckpointCount() const { return checkpoints_.size(); }

    // 清空所有快照
    void Clear();

    uint32_t IntervalFrames() const { return interval_frames_; }

private:
    static EntitySnapshot SnapshotFromEntity(const IEntity* entity);
    static void RestoreEntityFromSnapshot(const EntitySnapshot& snap, IEntity* entity);

    uint32_t interval_frames_;
    uint32_t max_checkpoints_;
    std::deque<BattleCheckpoint> checkpoints_; // 环形缓冲
};

} // namespace realtime
} // namespace gs
