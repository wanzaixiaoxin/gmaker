#include "battle_room.hpp"
#include "battle.pb.h"
#include <algorithm>
#include <cstring>
#include <iostream>

namespace gs {
namespace realtime {

// ──────────────────────────────────────────────
// 简易二进制序列化辅助
// ──────────────────────────────────────────────
static void AppendU32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
}

static void AppendU64(std::vector<uint8_t>& buf, uint64_t v) {
    AppendU32(buf, static_cast<uint32_t>(v >> 32));
    AppendU32(buf, static_cast<uint32_t>(v & 0xFFFFFFFF));
}

static void AppendF32(std::vector<uint8_t>& buf, float v) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    AppendU32(buf, u);
}

static void AppendU8(std::vector<uint8_t>& buf, uint8_t v) {
    buf.push_back(v);
}

// ──────────────────────────────────────────────
// 构造函数
// ──────────────────────────────────────────────
BattleRoom::BattleRoom(const BattleRoomConfig& cfg)
    : Room(cfg.base)
    , battle_cfg_(cfg)
    , checkpoint_mgr_(cfg.checkpoint_interval_frames, 10) {

    lockstep_.SetTimeoutFrames(cfg.lockstep_timeout_ms / 33);
}

// ──────────────────────────────────────────────
// 消息分发
// ──────────────────────────────────────────────
void BattleRoom::OnMessage(Message* msg) {
    if (!msg) return;

    switch (msg->Type()) {
        case MsgType::PlayerEnter: {
            // 区分正常进入和重连
            auto* reconnect = dynamic_cast<PlayerReconnectMsg*>(msg);
            if (reconnect) {
                OnPlayerReconnect(reconnect);
            } else {
                OnPlayerEnter(static_cast<PlayerEnterMsg*>(msg));
            }
            break;
        }
        case MsgType::PlayerLeave: {
            auto* dc = dynamic_cast<PlayerDisconnectMsg*>(msg);
            if (dc) {
                OnPlayerDisconnect(dc);
            } else {
                OnPlayerLeave(static_cast<PlayerLeaveMsg*>(msg));
            }
            break;
        }
        case MsgType::PlayerMove: {
            auto* move = dynamic_cast<HeroMoveInputMsg*>(msg);
            if (move) {
                OnHeroMoveInput(move);
            } else {
                OnHeroMoveInput(static_cast<HeroMoveInputMsg*>(msg));
            }
            break;
        }
        case MsgType::PlayerAction: {
            auto* skill = dynamic_cast<HeroCastSkillMsg*>(msg);
            if (skill) {
                OnHeroCastSkill(skill);
            } else {
                auto* ready = dynamic_cast<BattleReadyMsg*>(msg);
                if (ready) OnBattleReady(ready);
            }
            break;
        }
        case MsgType::RoomBroadcast: {
            auto* raw = static_cast<RoomBroadcastMsg*>(msg);
            if (raw) BroadcastToAll(raw->payload);
            break;
        }
        case MsgType::RoomTick:
            // Tick 由 ComputeThread::TickRooms 驱动，不走消息
            break;
        default:
            break;
    }
}

// ──────────────────────────────────────────────
// 帧驱动
// ──────────────────────────────────────────────
// ──────────────────────────────────────────────
// 帧驱动（M1a：内部帧计数器驱动，now_ms 保留兼容 Room 接口）
// ──────────────────────────────────────────────
void BattleRoom::Tick(uint64_t now_ms) {
    // M1a transition: now_ms 仅用于兼容 Room 接口，不再参与确定性逻辑
    (void)now_ms;
    uint32_t frame = ++battle_frame_;

    // 驱动帧同步引擎的帧计数器（替代墙钟）
    lockstep_.TickFrameCounter();

    switch (battle_state_) {
        case BattleState::Waiting:   TickWaiting(frame); break;
        case BattleState::Loading:   TickLoading(frame); break;
        case BattleState::Countdown: TickCountdown(frame); break;
        case BattleState::Fighting:  TickFighting(frame); break;
        case BattleState::Paused:    break;
        case BattleState::Finished:  TickFinished(frame); break;
    }
}

// ──────────────────────────────────────────────
// 状态机
// ──────────────────────────────────────────────
void BattleRoom::ChangeState(BattleState new_state) {
    BattleState old = battle_state_;
    battle_state_ = new_state;
    std::cerr << "[BattleRoom] room=" << RoomID()
              << " state: " << static_cast<int>(old)
              << " -> " << static_cast<int>(new_state) << std::endl;
}

void BattleRoom::TickWaiting(uint32_t frame) {
    (void)frame;
    if (players_.size() >= battle_cfg_.team_size * 2) {
        ChangeState(BattleState::Loading);
    }
}

void BattleRoom::TickLoading(uint32_t frame) {
    (void)frame;
    bool all_ready = true;
    for (const auto& [pid, info] : players_) {
        if (!info.is_ready && info.is_connected) {
            all_ready = false;
            break;
        }
    }
    if (all_ready) {
        InitBattleScene();
        countdown_remaining_sec_ = 3;
        ChangeState(BattleState::Countdown);
        BroadcastToAll(SerializeBattleStart());
    }
}

void BattleRoom::TickCountdown(uint32_t frame) {
    (void)frame;
    if (countdown_remaining_sec_ > 0) {
        --countdown_remaining_sec_;
    }
    if (countdown_remaining_sec_ == 0) {
        battle_start_frame_ = frame;

        // 设置帧同步玩家列表
        std::vector<uint64_t> player_ids;
        for (const auto& [pid, info] : players_) {
            if (info.is_connected) {
                player_ids.push_back(pid);
            }
        }
        lockstep_.SetPlayers(player_ids);

        ChangeState(BattleState::Fighting);
    }
}

void BattleRoom::TickFighting(uint32_t frame) {
    // 固定步长(逻辑帧 @ 30Hz = 约 33ms)。M1a 暂用常量；M1b 整个实体链路切定点。
    constexpr uint32_t FIXED_STEP_MS = 33;

    // 1. 确定性帧推进（M1a：去墙钟，帧计数驱动）
    // 不再手动 frame+1；调用方(BattleRoom::Tick)已按帧计数推进
    lockstep_.SetCurrentFrame(frame);

    std::vector<FrameInputs> confirmed;
    lockstep_.TryAdvance(confirmed);

    for (const auto& fi : confirmed) {
        ProcessFrameInputs(fi);
        frame_history_.push_back(fi);
        if (frame_history_.size() > kReplayHistorySize) {
            frame_history_.pop_front();
        }
    }

    // 2. Tick 实体（M1a 暂用固定步长，M1b 切定点后改 Fixed 步长）
    entity_mgr_.TickAll(FIXED_STEP_MS);

    // 3. 战斗逻辑
    TickCombat(FIXED_STEP_MS);

    // 4. 刷兵
    minion_spawn_timer_ms_ += FIXED_STEP_MS;
    if (minion_spawn_timer_ms_ >= battle_cfg_.minion_spawn_interval_sec * 1000) {
        minion_spawn_timer_ms_ = 0;
        SpawnMinions();
    }

    // 5. 清理死亡弹道
    std::vector<uint64_t> to_remove;
    auto projectiles = entity_mgr_.GetByType(EntityType::Projectile);
    for (auto* ent : projectiles) {
        auto* proj = static_cast<ProjectileEntity*>(ent);
        if (proj->HasReached() || !proj->IsAlive()) {
            to_remove.push_back(proj->EntityId());
        }
    }
    for (auto eid : to_remove) {
        entity_mgr_.RemoveEntity(eid);
    }

    // 6. 快照保存
    if (frame % checkpoint_mgr_.IntervalFrames() == 0) {
        SaveCheckpoint();
    }

    // 7. 状态同步广播
    BroadcastBattleState();

    // 8. 胜负检查
    CheckWinCondition();

    // 9. 掉线超时检查（M1a：改为帧计数超时；M1b 整个 DisconnectInfo 切定点时再统一）
    // M1a 暂以固定帧计数替代墙钟：每帧 33ms 对应约 1 帧，保持原有秒数语义
    uint32_t timeout_frames = battle_cfg_.max_reconnect_wait_sec * (1000 / FIXED_STEP_MS);
    for (auto it = disconnected_players_.begin(); it != disconnected_players_.end();) {
        if (frame - it->second.disconnect_frame > timeout_frames) {
            RemovePlayer(it->first);
            it = disconnected_players_.erase(it);
        } else {
            ++it;
        }
    }

    // M1b: 计算并存储本帧的确定性哈希（用于回放验证）
    frame_hashes_.push_back(ComputeFrameHash());
}

void BattleRoom::TickFinished(uint32_t frame) {
    (void)frame;
}

// ──────────────────────────────────────────────
// M1b: 帧哈希（确定性校验）
// 按 entity_id 排序后哈希所有实体关键字段，保证迭代顺序确定。
// ──────────────────────────────────────────────
std::uint64_t BattleRoom::ComputeFrameHash() const {
    using namespace deterministic;

    // 收集所有实体，按 entity_id 排序（保证确定性）
    std::vector<const IEntity*> sorted;
    entity_mgr_.ForEach([&](const IEntity* e) { sorted.push_back(e); });
    std::sort(sorted.begin(), sorted.end(),
              [](const IEntity* a, const IEntity* b) { return a->EntityId() < b->EntityId(); });

    FrameHasher h;
    for (const auto* ent : sorted) {
        h.update_u64(ent->EntityId());
        h.update_u64(static_cast<std::uint64_t>(ent->Type()));
        h.update_u64(static_cast<std::uint64_t>(ent->Team()));
        // 位置用定点 raw 哈希（确定性，不受 float 精度影响）
        h.update_i64(fixed::Fixed::from_float(ent->Pos().x).raw());
        h.update_i64(fixed::Fixed::from_float(ent->Pos().y).raw());
        h.update_i64(fixed::Fixed::from_float(ent->Pos().z).raw());
        h.update_i64(fixed::Fixed::from_float(ent->Yaw()).raw());
        h.update_u64(static_cast<std::uint64_t>(ent->HP()));
        h.update_u64(static_cast<std::uint64_t>(ent->MaxHP()));
        h.update_u64(static_cast<std::uint64_t>(ent->State()));
    }
    return h.final_hash();
}

// ──────────────────────────────────────────────
// 玩家进入
// ──────────────────────────────────────────────
void BattleRoom::OnPlayerEnter(PlayerEnterMsg* msg) {
    if (players_.size() >= battle_cfg_.team_size * 2) {
        return; // 已满
    }

    // 自动分配阵营（蓝方先满）
    TeamSide team = (blue_players_.size() <= red_players_.size())
                        ? TeamSide::Blue : TeamSide::Red;

    Vec3 spawn_pos;
    if (team == TeamSide::Blue && !battle_cfg_.blue_spawn_pts.empty()) {
        spawn_pos = battle_cfg_.blue_spawn_pts[blue_players_.size() % battle_cfg_.blue_spawn_pts.size()];
    } else if (team == TeamSide::Red && !battle_cfg_.red_spawn_pts.empty()) {
        spawn_pos = battle_cfg_.red_spawn_pts[red_players_.size() % battle_cfg_.red_spawn_pts.size()];
    }

    AddPlayerToTeam(msg->player_id, team, spawn_pos);

    // 记录 conn_id
    auto it = players_.find(msg->player_id);
    if (it != players_.end()) {
        it->second.conn_id = msg->conn_id;
        conn_to_player_[msg->conn_id] = msg->player_id;
    }

    std::cerr << "[BattleRoom] Player " << msg->player_id
              << " joined team " << (team == TeamSide::Blue ? "Blue" : "Red")
              << " room=" << RoomID() << std::endl;
}

void BattleRoom::OnPlayerLeave(PlayerLeaveMsg* msg) {
    if (battle_state_ == BattleState::Fighting) {
        // 战斗中离开，进入掉线等待
        auto it = players_.find(msg->player_id);
        if (it != players_.end()) {
            DisconnectInfo dc;
            dc.disconnect_frame = battle_frame_;
            dc.hero_entity_id = it->second.hero_entity_id;
            disconnected_players_[msg->player_id] = dc;

            it->second.is_connected = false;
            lockstep_.PlayerDisconnected(msg->player_id);
        }
    } else {
        RemovePlayer(msg->player_id);
    }
}

// ──────────────────────────────────────────────
// 英雄移动输入（帧同步）
// ──────────────────────────────────────────────
void BattleRoom::OnHeroMoveInput(HeroMoveInputMsg* msg) {
    if (battle_state_ != BattleState::Fighting) return;

    PlayerInput input;
    input.player_id = msg->player_id;
    input.input_seq = msg->input_seq;
    // M1a bridge: HeroMoveInputMsg 仍含 float（proto→main.cpp 解析），转为 Fixed
    input.move_x = fixed::Fixed::from_float(msg->move_x);
    input.move_z = fixed::Fixed::from_float(msg->move_z);
    input.has_input = true;
    lockstep_.SubmitInput(msg->player_id, input);
}

// ──────────────────────────────────────────────
// 英雄释放技能
// ──────────────────────────────────────────────
void BattleRoom::OnHeroCastSkill(HeroCastSkillMsg* msg) {
    if (battle_state_ != BattleState::Fighting) return;

    PlayerInput input;
    input.player_id = msg->player_id;
    input.input_seq = msg->input_seq;
    input.skill_slot = msg->skill_slot;
    // M1a bridge: target_pos 是 Vec3(float)，转为 Fixed
    input.skill_target_x = fixed::Fixed::from_float(msg->target_pos.x);
    input.skill_target_z = fixed::Fixed::from_float(msg->target_pos.z);
    input.skill_target_eid = msg->target_entity_id;
    input.has_input = true;
    lockstep_.SubmitInput(msg->player_id, input);
}

// ──────────────────────────────────────────────
// 加载完成
// ──────────────────────────────────────────────
void BattleRoom::OnBattleReady(BattleReadyMsg* msg) {
    auto it = players_.find(msg->player_id);
    if (it != players_.end()) {
        it->second.is_ready = true;
    }
}

// ──────────────────────────────────────────────
// 掉线
// ──────────────────────────────────────────────
void BattleRoom::OnPlayerDisconnect(PlayerDisconnectMsg* msg) {
    auto it = players_.find(msg->player_id);
    if (it != players_.end()) {
        it->second.is_connected = false;
        lockstep_.PlayerDisconnected(msg->player_id);

        DisconnectInfo dc;
        dc.disconnect_frame = battle_frame_;
        dc.hero_entity_id = it->second.hero_entity_id;
        disconnected_players_[msg->player_id] = dc;
    }
}

// ──────────────────────────────────────────────
// 重连
// ──────────────────────────────────────────────
void BattleRoom::OnPlayerReconnect(PlayerReconnectMsg* msg) {
    HandleReconnect(msg->player_id, msg->conn_id);
}

// ──────────────────────────────────────────────
// 帧确认回调
// ──────────────────────────────────────────────
void BattleRoom::OnFrameConfirmed(uint32_t frame, const FrameInputs& inputs) {
    (void)frame;
    (void)inputs;
}

// ──────────────────────────────────────────────
// 处理帧输入
// ──────────────────────────────────────────────
void BattleRoom::ProcessFrameInputs(const FrameInputs& inputs) {
    for (const auto& [pid, input] : inputs.player_inputs) {
        ProcessHeroInput(pid, input);
    }
}

void BattleRoom::ProcessHeroInput(uint64_t player_id, const PlayerInput& input) {
    auto* hero = GetPlayerHero(player_id);
    if (!hero || !hero->IsAlive()) return;

    using namespace fixed;

    // 移动（M1a 桥接：Fixed 输入 → float 实体，M1b 整个实体链路切 Fixed 后去掉转换）
    if (input.has_input && (input.move_x != FIXED_ZERO || input.move_z != FIXED_ZERO)) {
        // 将方向归一化（定点数）
        Fixed mx = input.move_x;
        Fixed mz = input.move_z;
        Fixed len = fixed_sqrt(mx * mx + mz * mz);
        if (len > FIXED_ONE) { mx = mx / len; mz = mz / len; }

        // M1a bridge: Fixed → float
        float fmx = mx.to_float();
        float fmz = mz.to_float();

        // 方向 * 每帧步长（M1b 改 Fixed 步长）
        float speed = 5.0f;
        float step = speed * (33.0f / 1000.0f); // 固定步长@30fps
        Vec3 target;
        target.x = hero->Pos().x + fmx * step;
        target.z = hero->Pos().z + fmz * step;
        hero->MoveTo(target);
    }

    // 技能（M1a bridge：Fixed 坐标 → float）
    if (input.has_input && input.skill_slot != 0xFF) {
        Vec3 target_pos = {input.skill_target_x.to_float(), 0, input.skill_target_z.to_float()};
        if (hero->CastSkill(input.skill_slot, target_pos, input.skill_target_eid)) {
            const auto& skills = hero->Skills();
            if (input.skill_slot < skills.size()) {
                const auto& skill = skills[input.skill_slot];
                if (skill.projectile_speed > 0) {
                    auto eid = entity_mgr_.NextEntityId();
                    entity_mgr_.CreateProjectile(
                        eid, hero->Team(),
                        hero->EntityId(), hero->Pos(), target_pos,
                        skill.projectile_speed, skill.damage, skill.radius
                    );
                } else {
                    if (input.skill_target_eid != 0) {
                        auto* target = entity_mgr_.FindEntity(input.skill_target_eid);
                        if (target && target->IsAlive()) {
                            target->TakeDamage(skill.damage);
                        }
                    }
                }
            }
        }
    }
}

// ──────────────────────────────────────────────
// 初始化战斗场景
// ──────────────────────────────────────────────
void BattleRoom::InitBattleScene() {
    // 创建英雄实体
    for (auto& [pid, info] : players_) {
        uint64_t eid = entity_mgr_.NextEntityId();
        auto* hero = entity_mgr_.CreateHero(eid, info.team, pid);
        hero->SetPos(info.spawn_pos);

        // 默认技能配置
        std::vector<SkillDef> skills(4);
        skills[0] = {0, 1001, 6.0f, 0, 80, 0, 15.0f, false};     // Q: 近战
        skills[1] = {1, 1002, 8.0f, 3.0f, 120, 2000, 12.0f, true}; // W: AOE
        skills[2] = {2, 1003, 10.0f, 0, 200, 5000, 20.0f, false};  // E: 远程
        skills[3] = {3, 1004, 15.0f, 5.0f, 300, 10000, 0, true};   // R: 大招 AOE
        hero->SetSkills(std::move(skills));

        info.hero_entity_id = eid;
    }

    // 创建防御塔
    for (const auto& def : battle_cfg_.tower_defs) {
        entity_mgr_.CreateTower(def.entity_id, def.team, def.grade);
    }

    // 默认防御塔配置（如果没有配置）
    if (battle_cfg_.tower_defs.empty()) {
        // 蓝方塔
        entity_mgr_.CreateTower(entity_mgr_.NextEntityId(), TeamSide::Blue, TowerGrade::Outer);
        entity_mgr_.CreateTower(entity_mgr_.NextEntityId(), TeamSide::Blue, TowerGrade::Inner);
        entity_mgr_.CreateTower(entity_mgr_.NextEntityId(), TeamSide::Blue, TowerGrade::Base);
        entity_mgr_.CreateTower(entity_mgr_.NextEntityId(), TeamSide::Blue, TowerGrade::Crystal);

        // 红方塔
        entity_mgr_.CreateTower(entity_mgr_.NextEntityId(), TeamSide::Red, TowerGrade::Outer);
        entity_mgr_.CreateTower(entity_mgr_.NextEntityId(), TeamSide::Red, TowerGrade::Inner);
        entity_mgr_.CreateTower(entity_mgr_.NextEntityId(), TeamSide::Red, TowerGrade::Base);
        entity_mgr_.CreateTower(entity_mgr_.NextEntityId(), TeamSide::Red, TowerGrade::Crystal);
    }
}

// ──────────────────────────────────────────────
// 刷小兵
// ──────────────────────────────────────────────
void BattleRoom::SpawnMinions() {
    ++minion_wave_count_;

    // 为每方各刷一波小兵
    for (auto team : {TeamSide::Blue, TeamSide::Red}) {
        // 近战小兵
        for (int i = 0; i < 3; ++i) {
            auto eid = entity_mgr_.NextEntityId();
            auto* minion = entity_mgr_.CreateMinion(eid, team, MinionType::Melee);

            // 设置路径点（简化：直线前进）
            Vec3 spawn = (team == TeamSide::Blue)
                ? Vec3{10.0f, 0, 10.0f}
                : Vec3{90.0f, 0, 90.0f};
            minion->SetPos(spawn);

            std::vector<Vec3> waypoints;
            if (team == TeamSide::Blue) {
                waypoints = {{30, 0, 30}, {50, 0, 50}, {70, 0, 70}, {90, 0, 90}};
            } else {
                waypoints = {{70, 0, 70}, {50, 0, 50}, {30, 0, 30}, {10, 0, 10}};
            }
            minion->SetWaypoints(std::move(waypoints));
        }

        // 远程小兵
        {
            auto eid = entity_mgr_.NextEntityId();
            auto* minion = entity_mgr_.CreateMinion(eid, team, MinionType::Ranged);
            Vec3 spawn = (team == TeamSide::Blue)
                ? Vec3{12.0f, 0, 8.0f}
                : Vec3{88.0f, 0, 92.0f};
            minion->SetPos(spawn);

            std::vector<Vec3> waypoints;
            if (team == TeamSide::Blue) {
                waypoints = {{30, 0, 30}, {50, 0, 50}, {70, 0, 70}, {90, 0, 90}};
            } else {
                waypoints = {{70, 0, 70}, {50, 0, 50}, {30, 0, 30}, {10, 0, 10}};
            }
            minion->SetWaypoints(std::move(waypoints));
        }
    }
}

// ──────────────────────────────────────────────
// 战斗逻辑
// ──────────────────────────────────────────────
void BattleRoom::TickCombat(uint32_t delta_ms) {
    // 塔攻击逻辑
    auto towers = entity_mgr_.GetByType(EntityType::Tower);
    auto heroes = entity_mgr_.GetByType(EntityType::Hero);
    auto minions = entity_mgr_.GetByType(EntityType::Minion);

    for (auto* ent : towers) {
        auto* tower = static_cast<TowerEntity*>(ent);
        if (!tower->IsAlive()) continue;
        if (tower->Grade() == TowerGrade::Crystal) continue; // 水晶不攻击

        // 寻找最近的敌方单位
        IEntity* best_target = nullptr;
        float best_dist = tower->GetAttackRange(); // 使用 TowerEntity 内部 range

        // 优先攻击小兵，其次英雄
        auto check_target = [&](const std::vector<IEntity*>& candidates) {
            for (auto* cand : candidates) {
                if (!cand->IsAlive() || cand->Team() == tower->Team()) continue;
                float dist = DistanceXZ(tower->Pos(), cand->Pos());
                if (dist < best_dist) {
                    best_dist = dist;
                    best_target = cand;
                }
            }
        };

        check_target(minions);
        check_target(heroes);

        if (best_target) {
            // 简化：直接造成伤害（实际应有攻击动画和冷却）
            best_target->TakeDamage(tower->GetAttackDamage());
        }
    }

    // 弹道命中检测
    auto projectiles = entity_mgr_.GetByType(EntityType::Projectile);
    for (auto* ent : projectiles) {
        auto* proj = static_cast<ProjectileEntity*>(ent);
        if (proj->HasReached()) {
            // 弹道到达目标位置，对范围内敌人造成伤害
            auto* caster = entity_mgr_.FindEntity(proj->CasterId());
            TeamSide caster_team = caster ? caster->Team() : TeamSide::None;

            // 检查范围内所有敌方单位
            auto check_hit = [&](const std::vector<IEntity*>& candidates) {
                for (auto* cand : candidates) {
                    if (!cand->IsAlive() || cand->Team() == caster_team) continue;
                    float dist = DistanceXZ(proj->Pos(), cand->Pos());
                    if (dist <= proj->Radius() + 0.5f) {
                        cand->TakeDamage(proj->Damage());
                        // 击杀统计
                        if (!cand->IsAlive() && caster && caster->Type() == EntityType::Hero) {
                            auto* hero = static_cast<HeroEntity*>(caster);
                            hero->AddKill();
                        }
                    }
                }
            };

            check_hit(heroes);
            check_hit(minions);
            check_hit(towers);
        }
    }
}

// ──────────────────────────────────────────────
// 胜负判定
// ──────────────────────────────────────────────
void BattleRoom::CheckWinCondition() {
    // 检查水晶是否被摧毁
    bool blue_crystal_alive = false;
    bool red_crystal_alive = false;

    auto towers = entity_mgr_.GetByType(EntityType::Tower);
    for (auto* ent : towers) {
        auto* tower = static_cast<TowerEntity*>(ent);
        if (tower->Grade() == TowerGrade::Crystal && tower->IsAlive()) {
            if (tower->Team() == TeamSide::Blue) blue_crystal_alive = true;
            else red_crystal_alive = true;
        }
    }

    TeamSide winner = TeamSide::None;
    if (!blue_crystal_alive && red_crystal_alive) {
        winner = TeamSide::Red;
    } else if (blue_crystal_alive && !red_crystal_alive) {
        winner = TeamSide::Blue;
    }

    if (winner != TeamSide::None) {
        ChangeState(BattleState::Finished);
        BroadcastToAll(SerializeBattleEnd(winner));
    }
}

// ──────────────────────────────────────────────
// 状态同步广播
// ──────────────────────────────────────────────
void BattleRoom::BroadcastBattleState() {
    auto data = SerializeBattleStateSync();
    BroadcastToAll(data);
}

void BattleRoom::BroadcastFrameSync(const FrameInputs& inputs) {
    auto data = SerializeFrameSync(inputs);
    BroadcastToAll(data);
}

void BattleRoom::SaveCheckpoint() {
    auto cp = checkpoint_mgr_.MakeSnapshot(
        lockstep_.CurrentFrame(), last_tick_ms_, battle_state_, entity_mgr_);
    checkpoint_mgr_.SaveSnapshot(cp);
}

// ──────────────────────────────────────────────
// 重连处理
// ──────────────────────────────────────────────
void BattleRoom::HandleReconnect(uint64_t player_id, uint64_t conn_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;

    // 更新连接信息
    it->second.conn_id = conn_id;
    it->second.is_connected = true;
    conn_to_player_[conn_id] = player_id;

    // 恢复帧同步
    lockstep_.PlayerReconnected(player_id);

    // 从掉线列表中移除
    disconnected_players_.erase(player_id);

    // 发送重连响应（包含最近快照 + 帧输入回放）
    const auto* cp = checkpoint_mgr_.GetLatestCheckpoint();
    if (cp) {
        auto data = SerializeReconnectAck(*cp, cp->frame, lockstep_.CurrentFrame());
        // 直接通过 broadcast 回调发送给该玩家
        if (broadcast_cb_) {
            RoomSnapshot snap;
            snap.room_id = RoomID();
            snap.frame_seq = lockstep_.CurrentFrame();
            snap.timestamp_ms = last_tick_ms_;
            broadcast_cb_(snap, {conn_id});
        }
    }

    std::cerr << "[BattleRoom] Player " << player_id
              << " reconnected in room " << RoomID() << std::endl;
}

// ──────────────────────────────────────────────
// 辅助方法
// ──────────────────────────────────────────────
void BattleRoom::AddPlayerToTeam(uint64_t player_id, TeamSide team, const Vec3& spawn_pos) {
    PlayerInfo info;
    info.player_id = player_id;
    info.team = team;
    info.spawn_pos = spawn_pos;
    players_[player_id] = info;

    if (team == TeamSide::Blue) {
        blue_players_.push_back(player_id);
    } else {
        red_players_.push_back(player_id);
    }
}

void BattleRoom::RemovePlayer(uint64_t player_id) {
    auto it = players_.find(player_id);
    if (it == players_.end()) return;

    conn_to_player_.erase(it->second.conn_id);

    auto& list = (it->second.team == TeamSide::Blue) ? blue_players_ : red_players_;
    list.erase(std::remove(list.begin(), list.end(), player_id), list.end());

    players_.erase(it);
}

TeamSide BattleRoom::GetPlayerTeam(uint64_t player_id) const {
    auto it = players_.find(player_id);
    return it != players_.end() ? it->second.team : TeamSide::None;
}

HeroEntity* BattleRoom::GetPlayerHero(uint64_t player_id) const {
    auto it = players_.find(player_id);
    if (it == players_.end()) return nullptr;
    return static_cast<HeroEntity*>(entity_mgr_.FindEntity(it->second.hero_entity_id));
}

void BattleRoom::BroadcastToAll(const std::vector<uint8_t>& data) {
    // 使用基类的 broadcast_cb_ 发送
    if (!broadcast_cb_) return;

    std::vector<uint64_t> conn_ids;
    for (const auto& [pid, info] : players_) {
        if (info.is_connected) {
            conn_ids.push_back(info.conn_id);
        }
    }

    if (conn_ids.empty()) return;

    RoomSnapshot snap;
    snap.room_id = RoomID();
    snap.frame_seq = lockstep_.CurrentFrame();
    snap.timestamp_ms = last_tick_ms_;
    snap.raw_payload = data;
    broadcast_cb_(snap, conn_ids);
}

void BattleRoom::BroadcastToTeam(TeamSide team, const std::vector<uint8_t>& data) {
    // 同上，筛选同一队伍的连接
    if (!broadcast_cb_) return;

    std::vector<uint64_t> conn_ids;
    for (const auto& [pid, info] : players_) {
        if (info.is_connected && info.team == team) {
            conn_ids.push_back(info.conn_id);
        }
    }
    if (conn_ids.empty()) return;

    RoomSnapshot snap;
    snap.room_id = RoomID();
    snap.raw_payload = data;
    broadcast_cb_(snap, conn_ids);
}

// ──────────────────────────────────────────────
// 序列化
// ──────────────────────────────────────────────
// ──────────────────────────────────────────────
// 序列化（protobuf）
// 三类下行消息用 BattleDownstream oneof 包裹，客户端据此分支解码，
// 取代原先手写的首字节 msg-type 区分符。
// ──────────────────────────────────────────────

// gs::realtime 枚举 -> ::battle proto 枚举映射
static ::battle::EntityType ToProtoEntityType(EntityType t) {
    return static_cast<::battle::EntityType>(t);
}
static ::battle::TeamSide ToProtoTeam(TeamSide t) {
    return static_cast<::battle::TeamSide>(t);
}
static ::battle::EntityState ToProtoEntityState(EntityState s) {
    return static_cast<::battle::EntityState>(s);
}
static ::battle::BattleState ToProtoBattleState(BattleState s) {
    return static_cast<::battle::BattleState>(s);
}

std::vector<uint8_t> BattleRoom::SerializeBattleStart() const {
    battle::BattleDownstream ds;
    auto* start = ds.mutable_start();
    start->set_room_id(RoomID());
    start->set_countdown_sec(countdown_remaining_sec_);
    for (auto pid : blue_players_) start->add_blue_players(pid);
    for (auto pid : red_players_)  start->add_red_players(pid);

    std::vector<uint8_t> buf(ds.ByteSizeLong());
    if (!ds.SerializeToArray(buf.data(), static_cast<int>(buf.size()))) {
        buf.clear(); // 序列化失败返回空，BroadcastToAll 会跳过空 payload
    }
    return buf;
}

std::vector<uint8_t> BattleRoom::SerializeBattleEnd(TeamSide winner) const {
    battle::BattleDownstream ds;
    auto* end = ds.mutable_end();
    end->set_winner(ToProtoTeam(winner));
    end->set_duration_sec(static_cast<uint32_t>((battle_frame_ - battle_start_frame_) / 30));

    uint32_t blue_kills = 0, red_kills = 0;
    auto heroes = entity_mgr_.GetByType(EntityType::Hero);
    for (auto* ent : heroes) {
        auto* hero = static_cast<HeroEntity*>(ent);
        if (hero->Team() == TeamSide::Blue) blue_kills += hero->Kills();
        else red_kills += hero->Kills();
    }
    end->set_blue_kills(blue_kills);
    end->set_red_kills(red_kills);

    std::vector<uint8_t> buf(ds.ByteSizeLong());
    if (!ds.SerializeToArray(buf.data(), static_cast<int>(buf.size()))) {
        buf.clear(); // 序列化失败返回空，BroadcastToAll 会跳过空 payload
    }
    return buf;
}

std::vector<uint8_t> BattleRoom::SerializeBattleStateSync() const {
    battle::BattleDownstream ds;
    auto* state = ds.mutable_state();
    state->set_frame(lockstep_.CurrentFrame());
    state->set_timestamp_ms(last_tick_ms_);
    state->set_battle_state(ToProtoBattleState(battle_state_));

    entity_mgr_.ForEach([&](const IEntity* ent) {
        auto* e = state->add_entities();
        e->set_entity_id(ent->EntityId());
        e->set_type(ToProtoEntityType(ent->Type()));
        e->set_team(ToProtoTeam(ent->Team()));
        e->set_x(ent->Pos().x);
        e->set_y(ent->Pos().y);
        e->set_z(ent->Pos().z);
        e->set_yaw(ent->Yaw());
        e->set_hp(static_cast<uint32_t>(ent->HP()));
        e->set_max_hp(static_cast<uint32_t>(ent->MaxHP()));
        e->set_state(ToProtoEntityState(ent->State()));
        if (ent->Type() == EntityType::Hero) {
            auto* hero = static_cast<const HeroEntity*>(ent);
            e->set_kills(hero->Kills());
            e->set_deaths(hero->Deaths());
            e->set_gold(hero->Gold());
        }
    });

    std::vector<uint8_t> buf(ds.ByteSizeLong());
    if (!ds.SerializeToArray(buf.data(), static_cast<int>(buf.size()))) {
        buf.clear(); // 序列化失败返回空，BroadcastToAll 会跳过空 payload
    }
    return buf;
}

std::vector<uint8_t> BattleRoom::SerializeFrameSync(const FrameInputs& inputs) const {
    std::vector<uint8_t> buf;
    AppendU32(buf, inputs.frame);
    AppendU32(buf, static_cast<uint32_t>(inputs.player_inputs.size()));

    for (const auto& [pid, input] : inputs.player_inputs) {
        AppendU64(buf, pid);
        AppendU8(buf, input.has_input ? 1 : 0);
        AppendF32(buf, input.move_x.to_float());
        AppendF32(buf, input.move_z.to_float());
        AppendU8(buf, input.skill_slot);
        AppendF32(buf, input.skill_target_x.to_float());
        AppendF32(buf, input.skill_target_z.to_float());
        AppendU64(buf, input.skill_target_eid);
    }

    return buf;
}

std::vector<uint8_t> BattleRoom::SerializeReconnectAck(const BattleCheckpoint& cp,
                                                        uint32_t from_frame,
                                                        uint32_t to_frame) const {
    std::vector<uint8_t> buf;
    AppendU32(buf, RoomID());
    AppendU32(buf, lockstep_.CurrentFrame());
    AppendU8(buf, static_cast<uint8_t>(battle_state_));
    AppendU32(buf, cp.frame);

    // 快照实体数量
    AppendU32(buf, static_cast<uint32_t>(cp.entities.size()));
    for (const auto& ent : cp.entities) {
        AppendU64(buf, ent.entity_id);
        AppendU8(buf, static_cast<uint8_t>(ent.type));
        AppendU8(buf, static_cast<uint8_t>(ent.team));
        AppendF32(buf, ent.pos.x);
        AppendF32(buf, ent.pos.z);
        AppendU32(buf, static_cast<uint32_t>(ent.hp));
    }

    // 回放帧数量
    uint32_t replay_count = 0;
    for (const auto& fi : frame_history_) {
        if (fi.frame > from_frame && fi.frame <= to_frame) {
            ++replay_count;
        }
    }
    AppendU32(buf, replay_count);

    for (const auto& fi : frame_history_) {
        if (fi.frame > from_frame && fi.frame <= to_frame) {
            AppendU32(buf, fi.frame);
            AppendU32(buf, static_cast<uint32_t>(fi.player_inputs.size()));
            for (const auto& [pid, input] : fi.player_inputs) {
                AppendU64(buf, pid);
                AppendF32(buf, input.move_x.to_float());
                AppendF32(buf, input.move_z.to_float());
                AppendU8(buf, input.skill_slot);
                AppendF32(buf, input.skill_target_x.to_float());
                AppendF32(buf, input.skill_target_z.to_float());
                AppendU64(buf, input.skill_target_eid);
            }
        }
    }

    return buf;
}

} // namespace realtime
} // namespace gs
