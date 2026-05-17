#include "lockstep_engine.hpp"
#include <chrono>
#include <algorithm>

namespace gs {
namespace realtime {

static uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

void LockstepEngine::SetCurrentFrame(uint32_t frame) {
    if (frame > current_frame_) {
        current_frame_ = frame;
        // 记录新帧开始时间
        frame_start_time_[current_frame_] = NowMs();
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

    // 缓存最近输入
    last_inputs_[player_id] = input;

    // 检查当前帧是否已就绪
    return IsFrameReady(current_frame_);
}

void LockstepEngine::TryAdvance(std::vector<FrameInputs>& out_confirmed) {
    out_confirmed.clear();

    // 从当前帧开始检查，确认所有可以确认的帧
    // 注意：帧必须按顺序确认
    uint32_t check_frame = current_frame_;

    auto it = frame_buffer_.find(check_frame);
    if (it == frame_buffer_.end()) {
        // 还没有任何输入，创建空帧
        auto& fi = frame_buffer_[check_frame];
        fi.frame = check_frame;
        it = frame_buffer_.find(check_frame);
    }

    auto& fi = it->second;

    if (IsFrameReady(check_frame)) {
        fi.confirmed = true;
    } else {
        // 检查超时
        auto tit = frame_start_time_.find(check_frame);
        if (tit != frame_start_time_.end()) {
            uint64_t elapsed = NowMs() - tit->second;
            if (elapsed >= timeout_ms_) {
                // 超时：为未提交的在线玩家填充空输入
                for (auto pid : active_players_) {
                    if (fi.player_inputs.find(pid) == fi.player_inputs.end()) {
                        fi.player_inputs[pid] = MakeEmptyInput(pid);
                    }
                }
                fi.confirmed = true;
            }
        }
    }

    if (fi.confirmed) {
        out_confirmed.push_back(fi);

        // 清理过旧的帧历史
        if (check_frame > kHistorySize) {
            uint32_t old_frame = check_frame - kHistorySize;
            frame_buffer_.erase(old_frame);
            frame_start_time_.erase(old_frame);
        }

        // 通知回调
        if (on_confirmed_) {
            on_confirmed_(check_frame, fi);
        }
    }
}

bool LockstepEngine::IsFrameReady(uint32_t frame) const {
    auto it = frame_buffer_.find(frame);
    if (it == frame_buffer_.end()) return false;

    // 所有在线玩家都已提交输入
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
    // 使用上一次输入的方向（如果有），实现"继续之前的操作"
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
    active_players_.clear();
    all_players_.clear();
    frame_buffer_.clear();
    last_inputs_.clear();
    frame_start_time_.clear();
}

} // namespace realtime
} // namespace gs
