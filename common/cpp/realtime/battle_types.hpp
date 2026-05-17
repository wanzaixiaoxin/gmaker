#pragma once

#include "message.hpp" // Vec3
#include "room.hpp"    // RoomConfig
#include <cstdint>
#include <vector>

namespace gs {
namespace realtime {

// ──────────────────────────────────────────────
// 阵营
// ──────────────────────────────────────────────
enum class TeamSide : uint8_t {
    None  = 0,
    Blue  = 1,  // 蓝方（左下）
    Red   = 2,  // 红方（右上）
};

// ──────────────────────────────────────────────
// 实体类型
// ──────────────────────────────────────────────
enum class EntityType : uint8_t {
    Invalid    = 0,
    Hero       = 1,
    Minion     = 2,
    Tower      = 3,
    Projectile = 4,
    Base       = 5,
};

// ──────────────────────────────────────────────
// 实体状态
// ──────────────────────────────────────────────
enum class EntityState : uint8_t {
    Idle      = 0,
    Moving    = 1,
    Attacking = 2,
    Casting   = 3,
    Dead      = 4,
};

// ──────────────────────────────────────────────
// 控制模式
// ──────────────────────────────────────────────
enum class ControlMode : uint8_t {
    Player = 0,
    AI     = 1,
};

// ──────────────────────────────────────────────
// 技能定义
// ──────────────────────────────────────────────
struct SkillDef {
    uint8_t  slot = 0;
    uint32_t skill_id = 0;
    float    range = 0;
    float    radius = 0;
    uint32_t damage = 0;
    uint32_t cooldown_ms = 0;
    float    projectile_speed = 0; // 0 = 瞬发
    bool     is_aoe = false;
};

// ──────────────────────────────────────────────
// 战斗房间生命周期
// ──────────────────────────────────────────────
enum class BattleState : uint8_t {
    Waiting   = 0,  // 等待玩家齐人
    Loading   = 1,  // 加载地图
    Countdown = 2,  // 倒计时
    Fighting  = 3,  // 战斗中
    Paused    = 4,  // 暂停
    Finished  = 5,  // 已结束
};

// ──────────────────────────────────────────────
// 防御塔定义
// ──────────────────────────────────────────────
enum class TowerGrade : uint8_t {
    Outer   = 0,  // 外塔
    Inner   = 1,  // 内塔
    Base    = 2,  // 高地塔
    Crystal = 3,  // 水晶/基地
};

struct TowerDef {
    uint64_t   entity_id = 0;
    TeamSide   team = TeamSide::None;
    TowerGrade grade = TowerGrade::Outer;
    Vec3       position;
};

// ──────────────────────────────────────────────
// 小兵定义
// ──────────────────────────────────────────────
enum class MinionType : uint8_t {
    Melee  = 0,
    Ranged = 1,
    Siege  = 2,
};

struct MinionWaveDef {
    TeamSide team = TeamSide::None;
    std::vector<Vec3> waypoints;
    uint8_t melee_count = 3;
    uint8_t ranged_count = 1;
    uint8_t siege_count = 0;
};

// ──────────────────────────────────────────────
// 战斗房间配置
// ──────────────────────────────────────────────
struct BattleRoomConfig {
    RoomConfig base;

    uint32_t team_size = 5;
    uint32_t minion_spawn_interval_sec = 30;
    uint32_t checkpoint_interval_frames = 60; // ~1s @60fps
    uint32_t max_reconnect_wait_sec = 60;
    uint32_t lockstep_timeout_ms = 200;

    std::vector<TowerDef> tower_defs;
    std::vector<Vec3>     blue_spawn_pts;
    std::vector<Vec3>     red_spawn_pts;
    std::vector<MinionWaveDef> minion_wave_defs;
};

} // namespace realtime
} // namespace gs
