#pragma once

#include "room.hpp"
#include "battle_types.hpp"
#include "battle_message.hpp"
#include "entity.hpp"
#include "lockstep_engine.hpp"
#include "checkpoint.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>

namespace gs {
namespace realtime {

// ──────────────────────────────────────────────
// 战斗房间 — MOBA 战斗核心
// ──────────────────────────────────────────────
// 生命周期: Waiting -> Loading -> Countdown -> Fighting -> Finished
//
// 帧同步流程:
//   1. 客户端发送 HeroMoveInput / HeroCastSkill
//   2. LockstepEngine 收集所有玩家输入
//   3. 帧确认后驱动 BattleRoom::OnFrameTick
//   4. OnFrameTick 更新所有实体状态
//   5. 广播状态同步给所有客户端
//
// 状态同步:
//   - 每帧广播所有实体状态（BattleStateSync）
//   - 定期保存 Checkpoint（用于重连）
//
// 重连流程:
//   1. 玩家重连 -> 发送 ReconnectAck（包含最近快照 + 回放帧）
//   2. 客户端从快照恢复状态，回放帧输入
//   3. 回放完成后继续正常帧同步
class BattleRoom : public Room {
public:
    explicit BattleRoom(const BattleRoomConfig& cfg);

    // 消息处理入口（由 Compute Thread 调用）
    void OnMessage(Message* msg) override;

    // 帧驱动
    void Tick(uint64_t now_ms) override;

    // 获取战斗状态
    BattleState GetBattleState() const { return battle_state_; }

private:
    // ── 状态机 ──
    void ChangeState(BattleState new_state);
    void TickWaiting(uint64_t now_ms);
    void TickLoading(uint64_t now_ms);
    void TickCountdown(uint64_t now_ms);
    void TickFighting(uint64_t now_ms);
    void TickFinished(uint64_t now_ms);

    // ── 消息处理 ──
    void OnPlayerEnter(PlayerEnterMsg* msg);
    void OnPlayerLeave(PlayerLeaveMsg* msg);
    void OnHeroMoveInput(HeroMoveInputMsg* msg);
    void OnHeroCastSkill(HeroCastSkillMsg* msg);
    void OnBattleReady(BattleReadyMsg* msg);
    void OnPlayerDisconnect(PlayerDisconnectMsg* msg);
    void OnPlayerReconnect(PlayerReconnectMsg* msg);

    // ── 帧同步 ──
    void OnFrameConfirmed(uint32_t frame, const FrameInputs& inputs);
    void ProcessFrameInputs(const FrameInputs& inputs);
    void ProcessHeroInput(uint64_t player_id, const PlayerInput& input);

    // ── 游戏逻辑 ──
    void InitBattleScene();   // 初始化地图实体（塔、基地等）
    void SpawnMinions();      // 刷小兵
    void TickCombat(uint32_t delta_ms);  // 战斗逻辑（塔攻击、弹道命中检测）
    void CheckWinCondition(); // 胜负判定

    // ── 状态同步 & 快照 ──
    void BroadcastBattleState();
    void BroadcastFrameSync(const FrameInputs& inputs);
    void SaveCheckpoint();

    // ── 重连 ──
    void HandleReconnect(uint64_t player_id, uint64_t conn_id);

    // ── 辅助 ──
    void AddPlayerToTeam(uint64_t player_id, TeamSide team, const Vec3& spawn_pos);
    void RemovePlayer(uint64_t player_id);
    TeamSide GetPlayerTeam(uint64_t player_id) const;
    HeroEntity* GetPlayerHero(uint64_t player_id) const;
    void BroadcastToTeam(TeamSide team, const std::vector<uint8_t>& data);
    void BroadcastToAll(const std::vector<uint8_t>& data);

    // ── 简单序列化 ──
    std::vector<uint8_t> SerializeBattleStart() const;
    std::vector<uint8_t> SerializeBattleEnd(TeamSide winner) const;
    std::vector<uint8_t> SerializeBattleStateSync() const;
    std::vector<uint8_t> SerializeFrameSync(const FrameInputs& inputs) const;
    std::vector<uint8_t> SerializeReconnectAck(const BattleCheckpoint& cp,
                                                uint32_t from_frame,
                                                uint32_t to_frame) const;

private:
    BattleRoomConfig battle_cfg_;
    BattleState battle_state_ = BattleState::Waiting;

    // 玩家管理
    struct PlayerInfo {
        uint64_t player_id = 0;
        uint64_t conn_id = 0;         // Gateway 连接 ID
        TeamSide team = TeamSide::None;
        uint64_t hero_entity_id = 0;
        bool is_connected = true;
        bool is_ready = false;        // 加载完成
        Vec3 spawn_pos;
    };
    std::unordered_map<uint64_t, PlayerInfo> players_; // player_id -> info
    std::unordered_map<uint64_t, uint64_t> conn_to_player_; // conn_id -> player_id

    // 实体管理
    EntityManager entity_mgr_;

    // 帧同步引擎
    LockstepEngine lockstep_;

    // 快照管理器
    CheckpointManager checkpoint_mgr_;

    // 战斗计时
    uint64_t battle_start_time_ms_ = 0;
    uint64_t battle_duration_ms_ = 0;
    uint32_t countdown_remaining_sec_ = 3;

    // 小兵刷新计时
    uint32_t minion_spawn_timer_ms_ = 0;
    uint32_t minion_wave_count_ = 0;

    // 上一次 Tick 时间
    uint64_t last_tick_ms_ = 0;

    // 蓝方/红方玩家列表（用于广播）
    std::vector<uint64_t> blue_players_;
    std::vector<uint64_t> red_players_;

    // 玩家掉线等待重连
    struct DisconnectInfo {
        uint64_t disconnect_time_ms = 0;
        uint64_t hero_entity_id = 0;
    };
    std::unordered_map<uint64_t, DisconnectInfo> disconnected_players_;

    // 帧同步历史（用于重连回放）
    static constexpr uint32_t kReplayHistorySize = 600; // ~10s @60fps
    std::deque<FrameInputs> frame_history_;
};

} // namespace realtime
} // namespace gs
