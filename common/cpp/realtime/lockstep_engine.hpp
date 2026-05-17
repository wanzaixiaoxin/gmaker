#pragma once

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>

namespace gs {
namespace realtime {

// ──────────────────────────────────────────────
// 单个玩家的帧输入
// ──────────────────────────────────────────────
struct PlayerInput {
    uint64_t player_id = 0;
    uint32_t input_seq = 0;       // 客户端输入序列号
    float     move_x = 0;         // 移动方向 X
    float     move_z = 0;         // 移动方向 Z
    uint8_t   skill_slot = 0xFF;  // 技能槽位 (0xFF = 无技能)
    float     skill_target_x = 0; // 技能目标位置 X
    float     skill_target_z = 0; // 技能目标位置 Z
    uint64_t  skill_target_eid = 0;// 技能目标实体 ID
    bool      has_input = false;   // 是否有输入（空帧标记）
};

// ──────────────────────────────────────────────
// 一帧的聚合输入（所有玩家输入集合）
// ──────────────────────────────────────────────
struct FrameInputs {
    uint32_t frame = 0;
    std::unordered_map<uint64_t, PlayerInput> player_inputs; // player_id -> input
    bool confirmed = false;  // 是否已确认（所有玩家都提交了）
};

// ──────────────────────────────────────────────
// 帧同步引擎
// ──────────────────────────────────────────────
// 核心思路：
//   1. 每帧收集所有在线玩家的输入
//   2. 所有玩家都提交了（或超时）后，该帧确认
//   3. 确认的帧输入会回调给 BattleRoom 驱动逻辑
//   4. 超时未提交的玩家使用上一帧的输入（或空输入）
class LockstepEngine {
public:
    using OnFrameConfirmed = std::function<void(uint32_t frame, const FrameInputs& inputs)>;

    LockstepEngine() = default;

    // 设置当前帧（每 Tick 调用一次）
    void SetCurrentFrame(uint32_t frame);

    // 设置参与帧同步的玩家列表
    void SetPlayers(const std::vector<uint64_t>& player_ids);

    // 玩家掉线时从帧同步中移除（使用空输入代替）
    void PlayerDisconnected(uint64_t player_id);

    // 玩家重连后恢复帧同步
    void PlayerReconnected(uint64_t player_id);

    // 提交玩家输入（Gateway 转发过来）
    // 返回 true 表示该帧因此输入而确认
    bool SubmitInput(uint64_t player_id, const PlayerInput& input);

    // 尝试推进帧（每 Tick 调用）
    // 返回已确认的帧输入列表（可能为空或多帧）
    void TryAdvance(std::vector<FrameInputs>& out_confirmed);

    // 设置超时阈值（毫秒），默认 200ms
    void SetTimeoutMs(uint32_t ms) { timeout_ms_ = ms; }

    // 获取当前帧号
    uint32_t CurrentFrame() const { return current_frame_; }

    // 获取某个玩家最近已确认的输入
    const PlayerInput* GetLastInput(uint64_t player_id) const;

    // 获取指定帧的输入（用于重连回放）
    const FrameInputs* GetFrameInputs(uint32_t frame) const;

    // 设置帧确认回调
    void SetOnFrameConfirmed(OnFrameConfirmed cb) { on_confirmed_ = std::move(cb); }

    // 重置（新战斗开始时调用）
    void Reset();

private:
    bool IsFrameReady(uint32_t frame) const;
    PlayerInput MakeEmptyInput(uint64_t player_id) const;

    uint32_t current_frame_ = 0;
    uint32_t timeout_ms_ = 200;

    // 在线玩家集合
    std::unordered_set<uint64_t> active_players_;
    // 所有注册玩家（包含掉线的）
    std::unordered_set<uint64_t> all_players_;

    // 帧输入缓冲区：frame -> inputs
    // 只保留最近 N 帧（用于重连回放）
    static constexpr uint32_t kHistorySize = 300; // ~5s @60fps
    std::unordered_map<uint32_t, FrameInputs> frame_buffer_;

    // 玩家最近输入缓存（用于超时回退）
    std::unordered_map<uint64_t, PlayerInput> last_inputs_;

    // 帧开始时间戳（用于超时检测）
    std::unordered_map<uint32_t, uint64_t> frame_start_time_;

    OnFrameConfirmed on_confirmed_;
};

} // namespace realtime
} // namespace gs
