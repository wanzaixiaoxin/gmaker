#pragma once

#include "message.hpp"
#include "battle_types.hpp"
#include "lockstep_engine.hpp"

namespace gs {
namespace realtime {

// ──────────────────────────────────────────────
// MOBA 战斗消息类型（扩展 MsgType）
// ──────────────────────────────────────────────
enum class BattleMsgType : uint8_t {
    // 客户端 -> 服
    BattleReady      = 100,  // 玩家加载完成
    HeroMoveInput    = 101,  // 英雄移动输入（帧同步）
    HeroCastSkill    = 102,  // 英雄释放技能
    BattleLoading    = 103,  // 加载进度

    // 服 -> 客户端
    BattleStart      = 110,  // 战斗开始（倒计时结束）
    BattleEnd        = 111,  // 战斗结束
    FrameSync        = 112,  // 帧同步数据广播
    BattleStateSync  = 113,  // 状态同步广播
    ReconnectAck     = 114,  // 重连响应

    // 系统内部
    PlayerDisconnect = 120,  // 玩家掉线
    PlayerReconnect = 121,  // 玩家重连
};

// ──────────────────────────────────────────────
// 客户端 -> 服：玩家加载完成
// ──────────────────────────────────────────────
class BattleReadyMsg : public Message {
public:
    MsgType Type() const override { return MsgType::PlayerAction; }
    uint8_t BattleType() const { return static_cast<uint8_t>(BattleMsgType::BattleReady); }
    uint64_t player_id = 0;
};

// ──────────────────────────────────────────────
// 客户端 -> 服：英雄移动输入（帧同步）
// ──────────────────────────────────────────────
class HeroMoveInputMsg : public Message {
public:
    MsgType Type() const override { return MsgType::PlayerMove; }
    uint64_t player_id = 0;
    float    move_x = 0;       // 移动方向 X (-1~1)
    float    move_z = 0;       // 移动方向 Z (-1~1)
    uint32_t input_seq = 0;    // 输入序列号
};

// ──────────────────────────────────────────────
// 客户端 -> 服：英雄释放技能
// ──────────────────────────────────────────────
class HeroCastSkillMsg : public Message {
public:
    MsgType Type() const override { return MsgType::PlayerAction; }
    uint64_t player_id = 0;
    uint8_t  skill_slot = 0;
    Vec3     target_pos;
    uint64_t target_entity_id = 0;
    uint32_t input_seq = 0;
};

// ──────────────────────────────────────────────
// 服 -> 客户端：战斗开始
// ──────────────────────────────────────────────
struct BattleStartData {
    uint32_t room_id = 0;
    uint32_t countdown_sec = 3;
    std::vector<uint64_t> blue_players;
    std::vector<uint64_t> red_players;
};

// ──────────────────────────────────────────────
// 服 -> 客户端：帧同步数据
// ──────────────────────────────────────────────
struct FrameSyncData {
    uint32_t frame = 0;
    // 每个玩家的输入
    std::vector<PlayerInput> inputs;
};

// ──────────────────────────────────────────────
// 服 -> 客户端：战斗状态同步
// ──────────────────────────────────────────────
struct BattleStateSyncData {
    uint32_t frame = 0;
    uint64_t timestamp_ms = 0;
    BattleState battle_state = BattleState::Fighting;

    // 所有实体状态
    struct EntitySync {
        uint64_t    entity_id = 0;
        EntityType  type = EntityType::Invalid;
        TeamSide    team = TeamSide::None;
        Vec3        pos;
        float       yaw = 0;
        int32_t     hp = 0;
        int32_t     max_hp = 0;
        EntityState state = EntityState::Idle;
        uint32_t    kills = 0;
        uint32_t    deaths = 0;
        uint32_t    gold = 0;
    };

    std::vector<EntitySync> entities;
    uint32_t blue_kills = 0;
    uint32_t red_kills = 0;
};

// ──────────────────────────────────────────────
// 服 -> 客户端：战斗结束
// ──────────────────────────────────────────────
struct BattleEndData {
    TeamSide winner = TeamSide::None;
    uint32_t duration_sec = 0;
    uint32_t blue_kills = 0;
    uint32_t red_kills = 0;
};

// ──────────────────────────────────────────────
// 服 -> 客户端：重连响应
// ──────────────────────────────────────────────
struct ReconnectAckData {
    uint32_t room_id = 0;
    uint32_t current_frame = 0;
    BattleState battle_state = BattleState::Fighting;
    // 从快照帧开始回放
    uint32_t snapshot_frame = 0;
    // 快照数据（序列化后的二进制）
    std::vector<uint8_t> snapshot_data;
    // 从快照帧到当前帧的所有输入
    std::vector<FrameInputs> replay_inputs;
};

// ──────────────────────────────────────────────
// 系统内部：玩家掉线
// ──────────────────────────────────────────────
class PlayerDisconnectMsg : public Message {
public:
    MsgType Type() const override { return MsgType::PlayerLeave; }
    uint64_t player_id = 0;
};

// ──────────────────────────────────────────────
// 系统内部：玩家重连
// ──────────────────────────────────────────────
class PlayerReconnectMsg : public Message {
public:
    MsgType Type() const override { return MsgType::PlayerEnter; }
    uint64_t player_id = 0;
    uint64_t conn_id = 0;
};

} // namespace realtime
} // namespace gs
