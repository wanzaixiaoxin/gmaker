#include "checkpoint.hpp"

namespace gs {
namespace realtime {

CheckpointManager::CheckpointManager(uint32_t interval_frames, uint32_t max_checkpoints)
    : interval_frames_(interval_frames)
    , max_checkpoints_(max_checkpoints) {}

BattleCheckpoint CheckpointManager::MakeSnapshot(uint32_t frame, uint64_t timestamp_ms,
                                                  BattleState battle_state,
                                                  const EntityManager& entity_mgr) {
    BattleCheckpoint cp;
    cp.frame = frame;
    cp.timestamp_ms = timestamp_ms;
    cp.battle_state = battle_state;

    uint32_t blue_kills = 0, red_kills = 0;
    uint32_t blue_towers = 0, red_towers = 0;

    entity_mgr.ForEach([&](const IEntity* ent) {
        EntitySnapshot snap = SnapshotFromEntity(ent);
        cp.entities.push_back(std::move(snap));

        // 统计计分
        if (ent->Type() == EntityType::Hero) {
            auto* hero = static_cast<const HeroEntity*>(ent);
            if (hero->Team() == TeamSide::Blue) {
                blue_kills += hero->Kills();
            } else {
                red_kills += hero->Deaths();
            }
        }
        if (ent->Type() == EntityType::Tower && ent->IsAlive()) {
            if (ent->Team() == TeamSide::Blue) ++blue_towers;
            else ++red_towers;
        }
    });

    cp.blue_kills = blue_kills;
    cp.red_kills = red_kills;
    cp.blue_towers_alive = blue_towers;
    cp.red_towers_alive = red_towers;

    return cp;
}

void CheckpointManager::SaveSnapshot(const BattleCheckpoint& cp) {
    checkpoints_.push_back(cp);
    // 环形缓冲：超过上限时移除最旧的
    while (checkpoints_.size() > max_checkpoints_) {
        checkpoints_.pop_front();
    }
}

const BattleCheckpoint* CheckpointManager::GetLatestCheckpoint() const {
    if (checkpoints_.empty()) return nullptr;
    return &checkpoints_.back();
}

const BattleCheckpoint* CheckpointManager::GetCheckpointAfter(uint32_t frame) const {
    // 从最旧的开始找第一个 >= frame 的
    for (const auto& cp : checkpoints_) {
        if (cp.frame >= frame) return &cp;
    }
    return nullptr;
}

const BattleCheckpoint* CheckpointManager::GetCheckpoint(uint32_t frame) const {
    for (const auto& cp : checkpoints_) {
        if (cp.frame == frame) return &cp;
    }
    return nullptr;
}

void CheckpointManager::Clear() {
    checkpoints_.clear();
}

// ──────────────────────────────────────────────
// 从 IEntity 生成 EntitySnapshot
// ──────────────────────────────────────────────
EntitySnapshot CheckpointManager::SnapshotFromEntity(const IEntity* entity) {
    EntitySnapshot snap;
    snap.entity_id = entity->EntityId();
    snap.type = entity->Type();
    snap.team = entity->Team();
    snap.pos = entity->Pos();
    snap.vel = entity->Vel();
    snap.yaw = entity->Yaw();
    snap.hp = entity->HP();
    snap.max_hp = entity->MaxHP();
    snap.state = entity->State();

    switch (entity->Type()) {
        case EntityType::Hero: {
            auto* hero = static_cast<const HeroEntity*>(entity);
            snap.owner_player_id = hero->OwnerPlayerId();
            snap.kills = hero->Kills();
            snap.deaths = hero->Deaths();
            snap.assists = hero->Assists();
            snap.gold = hero->Gold();
            snap.level = hero->Level();
            break;
        }
        case EntityType::Tower: {
            auto* tower = static_cast<const TowerEntity*>(entity);
            snap.tower_grade = tower->Grade();
            break;
        }
        case EntityType::Minion: {
            auto* minion = static_cast<const MinionEntity*>(entity);
            snap.minion_type = minion->GetMinionType();
            break;
        }
        case EntityType::Projectile: {
            auto* proj = static_cast<const ProjectileEntity*>(entity);
            snap.caster_id = proj->CasterId();
            snap.reached = proj->HasReached();
            break;
        }
        default:
            break;
    }

    return snap;
}

void CheckpointManager::RestoreEntityFromSnapshot(const EntitySnapshot& snap, IEntity* entity) {
    entity->SetPos(snap.pos);
    entity->SetVel(snap.vel);
    entity->SetYaw(snap.yaw);
    // 注意：HP 恢复通过 Heal/TakeDamage 不太方便，直接用内部值
    // 这里用 Kill + Heal 的方式模拟
    // 实际项目中应提供 SetHP 接口
}

} // namespace realtime
} // namespace gs
