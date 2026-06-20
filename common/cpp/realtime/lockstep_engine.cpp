#include "lockstep_engine.hpp"
#include <algorithm>

namespace gs {
namespace realtime {

// ──────────────────────────────────────────────
// SetCurrentFrame：记录帧开始时的全局帧计数器（确定性，去墙钟）
// ──────────────────────────────────────────────
void LockstepEngine::SetCurrentFrame(uint32_t frame) {
    if (frame > current_frame_) {
        current_frame_ = frame;
        frame_start_count_[current_frame_] = frame_counter_;
    }
}

void LockstepEngine::SetPlayers(const std::vector<uint64_t>& player_ids) {
    active_players_.clear();
    all_players_.clear();
    for (auto pid : player_ids) {
        active_players_.insert(pid);
        all_players_.insert(pid);
    }
}

void LockstepEngine::PlayerDisconnected(uint64_t player_id) {
    active_players_.erase(player_id);
}

void LockstepEngine::PlayerReconnected(uint64_t player_id) {
    if (all_players_.count(player_id)) {
        active_players_.insert(player_id);
    }
}

bool LockstepEngine::SubmitInput(uint64_t player_id, const PlayerInput& input) {
    if (!all_players_.count(player_id)) return false;

    auto& fi = frame_buffer_[current_frame_];
    fi.frame = current_frame_;
    fi.player_inputs[player_id] = input;
    last_inputs_[player_id] = input;
    return IsFrameReady(current_frame_);
}

// ──────────────────────────────────────────────
// TryAdvance M1a 重写：循环确认所有已就绪帧（修复单帧退化）
// 超时改用帧计数器：已等待 frame_counter_ - frame_start_count_[frame] > timeout_frames_
// ──────────────────────────────────────────────
void LockstepEngine::TryAdvance(std::vector<FrameInputs>& out_confirmed) {
    out_confirmed.clear();

    // 从 current_frame_ 开始，循环确认所有已就绪帧（必须按顺序）
    for (uint32_t check_frame = current_frame_;
         check_frame <= current_frame_ + 32; // 最多追赶 32 帧，防止死循环
         ++check_frame) {

        auto it = frame_buffer_.find(check_frame);
        if (it == frame_buffer_.end()) {
            // 还没有任何输入，创建空帧占位
            auto& fi = frame_buffer_[check_frame];
            fi.frame = check_frame;
            it = frame_buffer_.find(check_frame);
        }

        auto& fi = it->second;
        if (fi.confirmed) {
            // 已确认帧不再处理（正常情况不会出现，防御）
            continue;
        }

        if (IsFrameReady(check_frame)) {
            fi.confirmed = true;
        } else {
            // 帧计数超时：替代原先 wall-clock NowMs() 判断
            auto tit = frame_start_count_.find(check_frame);
            if (tit != frame_start_count_.end()) {
                uint32_t waited = frame_counter_ - tit->second;
                if (waited >= timeout_frames_) {
                    // 超时：为未提交的在线玩家填充空输入
                    for (auto pid : active_players_) {
                        if (fi.player_inputs.find(pid) == fi.player_inputs.end()) {
                            fi.player_inputs[pid] = MakeEmptyInput(pid);
                        }
                    }
                    fi.confirmed = true;
                } else {
                    // 本帧未就绪且未超时，后面的帧也不能确认（必须按顺序）
                    break;
                }
            }
        }

        if (fi.confirmed) {
            out_confirmed.push_back(fi);

            // 清理过期历史
            if (check_frame > kHistorySize) {
                uint32_t old_frame = check_frame - kHistorySize;
                frame_buffer_.erase(old_frame);
                frame_start_count_.erase(old_frame);
            }

            if (on_confirmed_) {
                on_confirmed_(check_frame, fi);
            }
        } else {
            // 本帧未就绪，停止（按顺序确认）
            break;
        }
    }
}

bool LockstepEngine::IsFrameReady(uint32_t frame) const {
    auto it = frame_buffer_.find(frame);
    if (it == frame_buffer_.end()) return false;
    for (auto pid : active_players_) {
        if (it->second.player_inputs.find(pid) == it->second.player_inputs.end()) {
            return false;
        }
    }
    return true;
}

PlayerInput LockstepEngine::MakeEmptyInput(uint64_t player_id) const {
    PlayerInput empty;
    empty.player_id = player_id;
    empty.has_input = false;
    // 继承上次输入的方向（如果有），实现"继续之前的操作"
    auto it = last_inputs_.find(player_id);
    if (it != last_inputs_.end()) {
        empty.move_x = it->second.move_x;
        empty.move_z = it->second.move_z;
    }
    return empty;
}

const PlayerInput* LockstepEngine::GetLastInput(uint64_t player_id) const {
    auto it = last_inputs_.find(player_id);
    return it != last_inputs_.end() ? &it->second : nullptr;
}

const FrameInputs* LockstepEngine::GetFrameInputs(uint32_t frame) const {
    auto it = frame_buffer_.find(frame);
    return it != frame_buffer_.end() ? &it->second : nullptr;
}

void LockstepEngine::Reset() {
    current_frame_ = 0;
    frame_counter_ = 0;
    active_players_.clear();
    all_players_.clear();
    frame_buffer_.clear();
    last_inputs_.clear();
    frame_start_count_.clear();
}

} // namespace realtime
} // namespace gs
