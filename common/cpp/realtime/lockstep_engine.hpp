#pragma once

#include "fixed/fixed.hpp"
#include "fixed/fixed_math.hpp"
#include "fixed/fixed_vec3.hpp"
#include <cstdint>
#include <vector>
#include <map>
#include <set>
#include <functional>

namespace gs {
namespace realtime {

// ──────────────────────────────────────────────
// 单个玩家的帧输入（定点数版，确定性）
// ──────────────────────────────────────────────
struct PlayerInput {
    uint64_t player_id = 0;
    uint32_t input_seq = 0;
    gs::realtime::fixed::Fixed move_x;           // 移动方向 X（-1~1，定点数）
    gs::realtime::fixed::Fixed move_z;           // 移动方向 Z（-1~1，定点数）
    uint8_t  skill_slot = 0xFF;                  // 技能槽位（0xFF = 无技能）
    gs::realtime::fixed::Fixed skill_target_x;   // 技能目标 X
    gs::realtime::fixed::Fixed skill_target_z;   // 技能目标 Z
    uint64_t skill_target_eid = 0;               // 技能目标实体 ID
    bool     has_input = false;                  // 是否有输入（空帧标记）
};

// ──────────────────────────────────────────────
// 一帧的聚合输入（所有玩家输入集合）
// ──────────────────────────────────────────────
struct FrameInputs {
    uint32_t frame = 0;
    std::map<uint64_t, PlayerInput> player_inputs;  // player_id -> input（有序，确定）
    bool confirmed = false;
};

// ──────────────────────────────────────────────
// 帧同步引擎（确定性版，M1a 去墙钟）
// ──────────────────────────────────────────────
class LockstepEngine {
public:
    using OnFrameConfirmed = std::function<void(uint32_t frame, const FrameInputs& inputs)>;

    LockstepEngine() = default;

    void SetCurrentFrame(uint32_t frame);

    void SetPlayers(const std::vector<uint64_t>& player_ids);

    void PlayerDisconnected(uint64_t player_id);
    void PlayerReconnected(uint64_t player_id);

    bool SubmitInput(uint64_t player_id, const PlayerInput& input);

    // TryAdvance 循环确认所有已就绪帧（M1a：修复单帧退化）
    void TryAdvance(std::vector<FrameInputs>& out_confirmed);

    // 帧计数超时（替代原先墙钟 NowMs()），默认 12 帧（@30fps ≈ 400ms）
    void SetTimeoutFrames(uint32_t frames) { timeout_frames_ = frames; }

    // 驱动帧计数（每逻辑帧调用一次，替代 wall-clock frame_start_time_）
    void TickFrameCounter() { ++frame_counter_; }

    uint32_t CurrentFrame() const { return current_frame_; }
    uint32_t FrameCounter() const { return frame_counter_; }

    const PlayerInput* GetLastInput(uint64_t player_id) const;
    const FrameInputs* GetFrameInputs(uint32_t frame) const;

    void SetOnFrameConfirmed(OnFrameConfirmed cb) { on_confirmed_ = std::move(cb); }

    void Reset();

private:
    bool IsFrameReady(uint32_t frame) const;
    PlayerInput MakeEmptyInput(uint64_t player_id) const;

    uint32_t current_frame_ = 0;
    uint32_t timeout_frames_ = 12;          // 超时帧数（取代 timeout_ms_）
    uint32_t frame_counter_ = 0;            // 全局帧计数器（确定性驱动，取代 wall-clock）

    // 有序容器：保证遍历顺序确定（替代 unordered_set/map）
    std::set<uint64_t> active_players_;
    std::set<uint64_t> all_players_;

    // 帧输入缓冲区
    static constexpr uint32_t kHistorySize = 300;
    std::map<uint32_t, FrameInputs> frame_buffer_;

    // 玩家最近输入缓存
    std::map<uint64_t, PlayerInput> last_inputs_;

    // 帧开始时的全局帧计数（用于帧计数超时检测，取代 frame_start_time_ + NowMs()）
    std::map<uint32_t, uint32_t> frame_start_count_;

    OnFrameConfirmed on_confirmed_;
};

} // namespace realtime
} // namespace gs
