#pragma once

#include "battle_types.hpp"
#include "fixed/fixed.hpp"
#include "fixed/fixed_vec3.hpp"
#include <cstdint>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <algorithm>

namespace gs {
namespace realtime {

// ──────────────────────────────────────────────
// 辅助函数
// ──────────────────────────────────────────────
inline float DistanceXZ(const Vec3& a, const Vec3& b) {
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

// ──────────────────────────────────────────────
// IEntity — 实体基类接口
// ──────────────────────────────────────────────
class IEntity {
public:
    virtual ~IEntity() = default;
    virtual EntityType Type() const = 0;
    virtual void Tick(uint32_t delta_ms) = 0;

    uint64_t  EntityId() const { return entity_id_; }
    TeamSide  Team() const { return team_; }
    const Vec3& Pos() const { return pos_; }
    const Vec3& Vel() const { return vel_; }
    float     Yaw() const { return yaw_; }
    int32_t   HP() const { return hp_; }
    int32_t   MaxHP() const { return max_hp_; }
    EntityState State() const { return state_; }
    bool      IsAlive() const { return hp_ > 0; }

    void SetPos(const Vec3& p) { pos_ = p; }
    void SetVel(const Vec3& v) { vel_ = v; }
    void SetYaw(float y) { yaw_ = y; }
    void TakeDamage(int32_t dmg) { hp_ = (std::max)(0, hp_ - dmg); }
    void Heal(int32_t amount) { hp_ = (std::min)(max_hp_, hp_ + amount); }
    void Kill() { hp_ = 0; state_ = EntityState::Dead; }

    // M2b: 定点数桥接——定点位置内部转 float 存到 pos_（过渡，M3 全切 FixedVec3）
    void SetPosFixed(const fixed::FixedVec3& p) {
        pos_.x = p.x.to_float(); pos_.y = p.y.to_float(); pos_.z = p.z.to_float();
    }
    void MoveTo(const fixed::FixedVec3& target) {
        pos_.x = target.x.to_float(); pos_.z = target.z.to_float();
    }
    fixed::FixedVec3 PosFixed() const {
        return {fixed::Fixed::from_float(pos_.x), fixed::Fixed::from_float(pos_.y), fixed::Fixed::from_float(pos_.z)};
    }

    // 定点数距离（纯定点，确定性）
    static fixed::Fixed DistanceXZFixed(const fixed::FixedVec3& a, const fixed::FixedVec3& b) {
        auto dx = a.x - b.x;
        auto dz = a.z - b.z;
        return fixed::fixed_sqrt(dx * dx + dz * dz);
    }

protected:
    uint64_t     entity_id_ = 0;
    TeamSide     team_ = TeamSide::None;
    Vec3         pos_;
    Vec3         vel_;
    float        yaw_ = 0;
    int32_t      hp_ = 100;
    int32_t      max_hp_ = 100;
    EntityState  state_ = EntityState::Idle;
};

// ──────────────────────────────────────────────
// HeroEntity — 英雄实体
// ──────────────────────────────────────────────
class HeroEntity : public IEntity {
public:
    HeroEntity(uint64_t entity_id, TeamSide team, uint64_t owner_player_id);

    EntityType Type() const override { return EntityType::Hero; }
    void Tick(uint32_t delta_ms) override;

    uint64_t     OwnerPlayerId() const { return owner_player_id_; }
    ControlMode  GetControlMode() const { return control_mode_; }
    void         SetControlMode(ControlMode mode) { control_mode_ = mode; }

    void MoveTo(const Vec3& target);
    bool CastSkill(uint8_t slot, const Vec3& target, uint64_t target_entity_id);

    const std::vector<SkillDef>& Skills() const { return skills_; }
    void SetSkills(std::vector<SkillDef> skills) { skills_ = std::move(skills); }

    uint32_t Kills() const { return kills_; }
    uint32_t Deaths() const { return deaths_; }
    uint32_t Assists() const { return assists_; }
    uint32_t Gold() const { return gold_; }
    uint32_t Level() const { return level_; }

    void AddKill()   { ++kills_;  gold_ += 200; }
    void AddDeath()  { ++deaths_; }
    void AddAssist() { ++assists_; gold_ += 100; }
    void AddGold(uint32_t g) { gold_ += g; }

private:
    uint64_t     owner_player_id_ = 0;
    ControlMode  control_mode_ = ControlMode::Player;
    Vec3         move_target_;
    float        move_speed_ = 5.0f;

    std::vector<SkillDef> skills_;
    uint32_t     skill_cooldowns_[4] = {};

    uint32_t kills_ = 0;
    uint32_t deaths_ = 0;
    uint32_t assists_ = 0;
    uint32_t gold_ = 500;
    uint32_t level_ = 1;
};

// ──────────────────────────────────────────────
// MinionEntity — 小兵实体
// ──────────────────────────────────────────────
class MinionEntity : public IEntity {
public:
    MinionEntity(uint64_t entity_id, TeamSide team, MinionType type);

    EntityType Type() const override { return EntityType::Minion; }
    void Tick(uint32_t delta_ms) override;

    MinionType GetMinionType() const { return minion_type_; }
    void SetWaypoints(std::vector<Vec3> wps) { waypoints_ = std::move(wps); }

private:
    MinionType   minion_type_;
    std::vector<Vec3> waypoints_;
    size_t       current_wp_ = 0;
    float        move_speed_ = 3.5f;
    float        attack_range_ = 1.5f;
    uint32_t     attack_damage_ = 20;
    uint32_t     attack_cooldown_ = 0;
    uint32_t     attack_interval_ = 1000; // ms
};

// ──────────────────────────────────────────────
// TowerEntity — 防御塔实体
// ──────────────────────────────────────────────
class TowerEntity : public IEntity {
public:
    TowerEntity(uint64_t entity_id, TeamSide team, TowerGrade grade);

    EntityType Type() const override { return EntityType::Tower; }
    void Tick(uint32_t delta_ms) override;

    TowerGrade Grade() const { return grade_; }
    float      GetAttackRange() const { return attack_range_; }
    uint32_t   GetAttackDamage() const { return attack_damage_; }

    // M3a: 塔攻击冷却
    bool CanAttack() const { return attack_cooldown_ == 0; }
    void OnAttack() { attack_cooldown_ = attack_interval_; }
    void TickCooldown(uint32_t delta_ms) {
        if (attack_cooldown_ > delta_ms) attack_cooldown_ -= delta_ms;
        else attack_cooldown_ = 0;
    }

private:
    TowerGrade   grade_;
    float        attack_range_ = 8.0f;
    uint32_t     attack_damage_ = 100;
    uint32_t     attack_cooldown_ = 0;
    uint32_t     attack_interval_ = 1250; // ms
};

// ──────────────────────────────────────────────
// ProjectileEntity — 技能弹道实体
// ──────────────────────────────────────────────
class ProjectileEntity : public IEntity {
public:
    ProjectileEntity(uint64_t entity_id, TeamSide team,
                     uint64_t caster_id, const Vec3& start, const Vec3& target,
                     float speed, uint32_t damage, float radius);

    EntityType Type() const override { return EntityType::Projectile; }
    void Tick(uint32_t delta_ms) override;

    uint64_t CasterId() const { return caster_id_; }
    bool     HasReached() const { return reached_; }
    uint32_t Damage() const { return damage_; }
    float    Radius() const { return radius_; }

private:
    uint64_t caster_id_ = 0;
    Vec3     target_pos_;
    float    speed_ = 15.0f;
    uint32_t damage_ = 0;
    float    radius_ = 0;
    bool     reached_ = false;
};

// ──────────────────────────────────────────────
// EntityManager — 实体管理器
// ──────────────────────────────────────────────
class EntityManager {
public:
    HeroEntity*       CreateHero(uint64_t eid, TeamSide team, uint64_t owner);
    MinionEntity*     CreateMinion(uint64_t eid, TeamSide team, MinionType type);
    TowerEntity*      CreateTower(uint64_t eid, TeamSide team, TowerGrade grade);
    ProjectileEntity* CreateProjectile(uint64_t eid, TeamSide team,
                                       uint64_t caster, const Vec3& start,
                                       const Vec3& target, float speed,
                                       uint32_t damage, float radius);

    IEntity*    FindEntity(uint64_t eid) const;
    HeroEntity* FindHeroByPlayer(uint64_t player_id) const;
    void        RemoveEntity(uint64_t eid);

    void ForEach(std::function<void(const IEntity*)> fn) const;
    void TickAll(uint32_t delta_ms);

    std::vector<IEntity*> GetByTeam(TeamSide team) const;
    std::vector<IEntity*> GetByType(EntityType type) const;

    uint64_t NextEntityId() { return next_entity_id_++; }

private:
    std::unordered_map<uint64_t, std::unique_ptr<IEntity>> entities_;
    uint64_t next_entity_id_ = 1;
};

} // namespace realtime
} // namespace gs
