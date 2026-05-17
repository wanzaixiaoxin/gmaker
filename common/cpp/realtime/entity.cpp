#include "entity.hpp"
#include <cmath>
#include <algorithm>

namespace gs {
namespace realtime {

static Vec3 DirectionXZ(const Vec3& from, const Vec3& to) {
    float dx = to.x - from.x, dz = to.z - from.z;
    float len = std::sqrt(dx * dx + dz * dz);
    if (len < 0.001f) return {0, 0, 0};
    return {dx / len, 0, dz / len};
}

// ──────────────────────────────────────────────
// HeroEntity
// ──────────────────────────────────────────────

HeroEntity::HeroEntity(uint64_t eid, TeamSide team, uint64_t owner) {
    entity_id_ = eid;
    team_ = team;
    owner_player_id_ = owner;
    max_hp_ = 1000;
    hp_ = max_hp_;
}

void HeroEntity::Tick(uint32_t delta_ms) {
    if (!IsAlive()) return;

    // 技能冷却
    for (int i = 0; i < 4; ++i) {
        if (skill_cooldowns_[i] > 0) {
            skill_cooldowns_[i] -= std::min(skill_cooldowns_[i], delta_ms);
        }
    }

    // 移动
    if (state_ == EntityState::Moving) {
        float dist = DistanceXZ(pos_, move_target_);
        float step = move_speed_ * delta_ms / 1000.0f;
        if (dist <= step) {
            pos_ = move_target_;
            state_ = EntityState::Idle;
        } else {
            auto dir = DirectionXZ(pos_, move_target_);
            pos_.x += dir.x * step;
            pos_.z += dir.z * step;
            yaw_ = std::atan2(dir.x, dir.z);
        }
    }
}

void HeroEntity::MoveTo(const Vec3& target) {
    move_target_ = target;
    state_ = EntityState::Moving;
}

bool HeroEntity::CastSkill(uint8_t slot, const Vec3& target, uint64_t target_eid) {
    if (slot >= skills_.size()) return false;
    if (skill_cooldowns_[slot] > 0) return false;
    skill_cooldowns_[slot] = skills_[slot].cooldown_ms;
    state_ = EntityState::Casting;
    return true;
}

// ──────────────────────────────────────────────
// MinionEntity
// ──────────────────────────────────────────────

MinionEntity::MinionEntity(uint64_t eid, TeamSide team, MinionType type) {
    entity_id_ = eid;
    team_ = team;
    minion_type_ = type;
    switch (type) {
        case MinionType::Melee:
            max_hp_ = 450; attack_damage_ = 20; move_speed_ = 3.5f; attack_range_ = 1.5f;
            break;
        case MinionType::Ranged:
            max_hp_ = 280; attack_damage_ = 25; move_speed_ = 3.0f; attack_range_ = 5.0f;
            break;
        case MinionType::Siege:
            max_hp_ = 800; attack_damage_ = 40; move_speed_ = 2.5f; attack_range_ = 5.0f;
            break;
    }
    hp_ = max_hp_;
}

void MinionEntity::Tick(uint32_t delta_ms) {
    if (!IsAlive()) return;

    // 冷却
    if (attack_cooldown_ > 0) {
        attack_cooldown_ -= std::min(attack_cooldown_, delta_ms);
    }

    // 沿路径点移动
    if (current_wp_ < waypoints_.size()) {
        const auto& wp = waypoints_[current_wp_];
        float dist = DistanceXZ(pos_, wp);
        float step = move_speed_ * delta_ms / 1000.0f;
        if (dist <= step) {
            pos_ = wp;
            ++current_wp_;
        } else {
            auto dir = DirectionXZ(pos_, wp);
            pos_.x += dir.x * step;
            pos_.z += dir.z * step;
            yaw_ = std::atan2(dir.x, dir.z);
        }
        state_ = EntityState::Moving;
    } else {
        state_ = EntityState::Idle;
    }
}

// ──────────────────────────────────────────────
// TowerEntity
// ──────────────────────────────────────────────

TowerEntity::TowerEntity(uint64_t eid, TeamSide team, TowerGrade grade) {
    entity_id_ = eid;
    team_ = team;
    grade_ = grade;
    switch (grade) {
        case TowerGrade::Outer:
            max_hp_ = 3000; attack_damage_ = 100; attack_range_ = 8.0f;
            break;
        case TowerGrade::Inner:
            max_hp_ = 2500; attack_damage_ = 120; attack_range_ = 8.0f;
            break;
        case TowerGrade::Base:
            max_hp_ = 2000; attack_damage_ = 140; attack_range_ = 8.0f;
            break;
        case TowerGrade::Crystal:
            max_hp_ = 4000; attack_damage_ = 0; attack_range_ = 0;
            break;
    }
    hp_ = max_hp_;
}

void TowerEntity::Tick(uint32_t delta_ms) {
    if (!IsAlive()) return;
    if (attack_cooldown_ > 0) {
        attack_cooldown_ -= std::min(attack_cooldown_, delta_ms);
    }
    // 塔不移动，AI 选择目标在 BattleRoom 层处理
}

// ──────────────────────────────────────────────
// ProjectileEntity
// ──────────────────────────────────────────────

ProjectileEntity::ProjectileEntity(uint64_t eid, TeamSide team,
                                   uint64_t caster, const Vec3& start,
                                   const Vec3& target, float speed,
                                   uint32_t damage, float radius) {
    entity_id_ = eid;
    team_ = team;
    caster_id_ = caster;
    pos_ = start;
    target_pos_ = target;
    speed_ = speed;
    damage_ = damage;
    radius_ = radius;
    hp_ = 1;
    max_hp_ = 1;
}

void ProjectileEntity::Tick(uint32_t delta_ms) {
    if (reached_ || !IsAlive()) return;

    float dist = DistanceXZ(pos_, target_pos_);
    float step = speed_ * delta_ms / 1000.0f;

    if (dist <= step) {
        pos_ = target_pos_;
        reached_ = true;
        hp_ = 0; // 标记为需要清理
    } else {
        auto dir = DirectionXZ(pos_, target_pos_);
        pos_.x += dir.x * step;
        pos_.z += dir.z * step;
    }
}

// ──────────────────────────────────────────────
// EntityManager
// ──────────────────────────────────────────────

HeroEntity* EntityManager::CreateHero(uint64_t eid, TeamSide team, uint64_t owner) {
    auto hero = std::make_unique<HeroEntity>(eid, team, owner);
    auto* ptr = hero.get();
    entities_[eid] = std::move(hero);
    return ptr;
}

MinionEntity* EntityManager::CreateMinion(uint64_t eid, TeamSide team, MinionType type) {
    auto m = std::make_unique<MinionEntity>(eid, team, type);
    auto* ptr = m.get();
    entities_[eid] = std::move(m);
    return ptr;
}

TowerEntity* EntityManager::CreateTower(uint64_t eid, TeamSide team, TowerGrade grade) {
    auto t = std::make_unique<TowerEntity>(eid, team, grade);
    auto* ptr = t.get();
    entities_[eid] = std::move(t);
    return ptr;
}

ProjectileEntity* EntityManager::CreateProjectile(uint64_t eid, TeamSide team,
                                                  uint64_t caster, const Vec3& start,
                                                  const Vec3& target, float speed,
                                                  uint32_t damage, float radius) {
    auto p = std::make_unique<ProjectileEntity>(eid, team, caster, start, target, speed, damage, radius);
    auto* ptr = p.get();
    entities_[eid] = std::move(p);
    return ptr;
}

IEntity* EntityManager::FindEntity(uint64_t eid) const {
    auto it = entities_.find(eid);
    return it != entities_.end() ? it->second.get() : nullptr;
}

HeroEntity* EntityManager::FindHeroByPlayer(uint64_t player_id) const {
    for (const auto& [_, ent] : entities_) {
        if (ent->Type() == EntityType::Hero) {
            auto* hero = static_cast<HeroEntity*>(ent.get());
            if (hero->OwnerPlayerId() == player_id) return hero;
        }
    }
    return nullptr;
}

void EntityManager::RemoveEntity(uint64_t eid) {
    entities_.erase(eid);
}

void EntityManager::ForEach(std::function<void(const IEntity*)> fn) const {
    for (const auto& [_, ent] : entities_) {
        fn(ent.get());
    }
}

void EntityManager::TickAll(uint32_t delta_ms) {
    for (auto& [_, ent] : entities_) {
        ent->Tick(delta_ms);
    }
}

std::vector<IEntity*> EntityManager::GetByTeam(TeamSide team) const {
    std::vector<IEntity*> result;
    for (const auto& [_, ent] : entities_) {
        if (ent->Team() == team) result.push_back(ent.get());
    }
    return result;
}

std::vector<IEntity*> EntityManager::GetByType(EntityType type) const {
    std::vector<IEntity*> result;
    for (const auto& [_, ent] : entities_) {
        if (ent->Type() == type) result.push_back(ent.get());
    }
    return result;
}

} // namespace realtime
} // namespace gs
