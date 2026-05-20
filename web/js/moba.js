/**
 * Pixel MOBA - 1v1 像素风格 MOBA 游戏引擎
 * 纯 JavaScript 实现，无外部依赖
 */

// ============================================================
// 工具函数
// ============================================================
function dist(a, b) {
    const dx = a.x - b.x;
    const dy = a.y - b.y;
    return Math.sqrt(dx * dx + dy * dy);
}

function clamp(v, min, max) {
    return v < min ? min : (v > max ? max : v);
}

function lerp(a, b, t) {
    return a + (b - a) * t;
}

function angleBetween(from, to) {
    return Math.atan2(to.y - from.y, to.x - from.x);
}

function nextId() {
    return MobaGame._idCounter++;
}

// ============================================================
// 游戏常量
// ============================================================
const MAP_SIZE = 100;
const BLUE_BASE = { x: 10, y: 90 };
const RED_BASE = { x: 90, y: 10 };

const TEAM_COLORS = {
    blue: { primary: '#4488ff', dark: '#2266cc', light: '#66aaff', name: '蓝方' },
    red: { primary: '#ff4444', dark: '#cc2222', light: '#ff6666', name: '红方' }
};

const SKILLS = {
    Q: { name: '冲锋', cooldown: 5, baseDamage: 30, levelScale: 5, range: 8, cost: 0, aoeRadius: 0, type: 'dash' },
    W: { name: '旋风斩', cooldown: 8, baseDamage: 40, levelScale: 8, range: 6, cost: 0, aoeRadius: 5, type: 'aoe' },
    E: { name: '能量弹', cooldown: 4, baseDamage: 50, levelScale: 10, range: 15, cost: 0, aoeRadius: 2, type: 'projectile' },
    R: { name: '天降正义', cooldown: 30, baseDamage: 100, levelScale: 15, range: 20, cost: 0, aoeRadius: 6, type: 'aoe_at_point' }
};

const SKILL_KEYS = ['Q', 'W', 'E', 'R'];

// ============================================================
// 实体类
// ============================================================

class Entity {
    constructor(type, team, x, y) {
        this.id = nextId();
        this.type = type;
        this.team = team;
        this.x = x;
        this.y = y;
        this.hp = 100;
        this.maxHp = 100;
        this.state = 'idle';
        this.speed = 0;
        this.alive = true;
    }

    takeDamage(amount) {
        if (!this.alive) return;
        this.hp -= amount;
        if (this.hp <= 0) {
            this.hp = 0;
            this.alive = false;
            this.state = 'dead';
        }
    }
}

class Hero extends Entity {
    constructor(team, x, y, name) {
        super('hero', team, x, y);
        this.name = name || (team === 'blue' ? '勇者' : '魔王');
        this.maxHp = 500;
        this.hp = 500;
        this.attack = 30;
        this.speed = 0.1; // 约 6 游戏单位/秒
        this.level = 1;
        this.exp = 0;
        this.gold = 0;
        this.kills = 0;
        this.deaths = 0;
        this.assists = 0;
        this.size = 24;
        this.cooldowns = { Q: 0, W: 0, E: 0, R: 0 };
        this.targetX = x;
        this.targetY = y;
        this.facing = team === 'blue' ? Math.PI / 4 : -Math.PI * 3 / 4;
        this.respawnTimer = 0;
        this.attackCooldown = 0;
        this.attackTarget = null;
        this.dashTimer = 0;
        this.dashTargetX = 0;
        this.dashTargetY = 0;
        this.state = 'idle';
        this.animationFrame = 0;
        this.animationTimer = 0;
        // 自动攻击系统
        this.autoAttackRange = 8; // 自动攻击范围（游戏单位）
        this.autoAttackCooldown = 1.0; // 1秒一次攻击
        this.autoAttackTimer = 0; // 自动攻击冷却计时器
        this.levelUpFlash = 0; // 升级闪光效果计时器
        this.isHealing = false; // 是否在基地治疗中
    }

    get expToLevel() {
        return this.level * 100;
    }

    addExp(amount) {
        this.exp += amount;
        while (this.exp >= this.expToLevel) {
            this.exp -= this.expToLevel;
            this.level++;
            this.maxHp += 30;
            this.hp = Math.min(this.hp + 30, this.maxHp);
            this.attack += 5;
        }
    }

    getEffectiveDamage() {
        return this.attack;
    }

    canCast(skillKey) {
        return this.cooldowns[skillKey] <= 0 && this.alive;
    }

    castSkill(skillKey, world, targetPos) {
        if (!this.canCast(skillKey)) return false;
        const skill = SKILLS[skillKey];
        this.cooldowns[skillKey] = skill.cooldown;

        // 技能伤害公式：基础伤害 + 等级 * 等级加成
        const getSkillDmg = () => skill.baseDamage + this.level * skill.levelScale;

        switch (skill.type) {
            case 'dash': {
                const angle = this.facing;
                const tx = this.x + Math.cos(angle) * skill.range;
                const ty = this.y + Math.sin(angle) * skill.range;
                this.dashTargetX = clamp(tx, 1, MAP_SIZE - 1);
                this.dashTargetY = clamp(ty, 1, MAP_SIZE - 1);
                this.dashTimer = 0.3;
                this.state = 'dashing';
                // 冲锋路径上造成伤害
                const enemies = world.getEnemiesOf(this.team);
                const dmg = getSkillDmg();
                for (const e of enemies) {
                    if (!e.alive) continue;
                    if (dist(this, e) < skill.range && dist(this, e) < 3) {
                        e.takeDamage(dmg);
                        if (!e.alive) world.onKill(this, e);
                    }
                }
                break;
            }
            case 'aoe': {
                const enemies = world.getEnemiesOf(this.team);
                const dmg = getSkillDmg();
                for (const e of enemies) {
                    if (!e.alive) continue;
                    if (dist(this, e) <= skill.aoeRadius) {
                        e.takeDamage(dmg);
                        if (!e.alive) world.onKill(this, e);
                    }
                }
                world.addEffect({ type: 'aoe', x: this.x, y: this.y, radius: skill.aoeRadius, timer: 0.4, team: this.team });
                break;
            }
            case 'projectile': {
                if (!targetPos) targetPos = { x: this.x + Math.cos(this.facing) * 5, y: this.y + Math.sin(this.facing) * 5 };
                const angle = angleBetween(this, targetPos);
                const proj = new Projectile(
                    this.team,
                    this.x, this.y,
                    Math.cos(angle) * 15,
                    Math.sin(angle) * 15,
                    getSkillDmg(),
                    skill.aoeRadius,
                    this
                );
                proj.maxDistance = skill.range;
                world.addProjectile(proj);
                break;
            }
            case 'aoe_at_point': {
                if (!targetPos) targetPos = { x: this.x, y: this.y };
                const clamped = {
                    x: clamp(targetPos.x, 1, MAP_SIZE - 1),
                    y: clamp(targetPos.y, 1, MAP_SIZE - 1)
                };
                if (dist(this, clamped) > skill.range) {
                    const angle = angleBetween(this, clamped);
                    clamped.x = this.x + Math.cos(angle) * skill.range;
                    clamped.y = this.y + Math.sin(angle) * skill.range;
                }
                const enemies2 = world.getEnemiesOf(this.team);
                const dmg2 = getSkillDmg();
                for (const e of enemies2) {
                    if (!e.alive) continue;
                    if (dist(clamped, e) <= skill.aoeRadius) {
                        e.takeDamage(dmg2);
                        if (!e.alive) world.onKill(this, e);
                    }
                }
                world.addEffect({ type: 'aoe_at_point', x: clamped.x, y: clamped.y, radius: skill.aoeRadius, timer: 0.8, team: this.team });
                break;
            }
        }
        return true;
    }

    update(dt, world) {
        if (!this.alive) {
            this.respawnTimer -= dt;
            if (this.respawnTimer <= 0) {
                this.alive = true;
                this.hp = this.maxHp;
                this.state = 'idle';
                const base = this.team === 'blue' ? BLUE_BASE : RED_BASE;
                this.x = base.x;
                this.y = base.y;
                this.targetX = base.x;
                this.targetY = base.y;
            }
            return;
        }

        // 冷却
        for (const k of SKILL_KEYS) {
            if (this.cooldowns[k] > 0) this.cooldowns[k] -= dt;
        }
        if (this.attackCooldown > 0) this.attackCooldown -= dt;

        // 基地治疗：在己方基地附近回复生命值
        const base = this.team === 'blue' ? BLUE_BASE : RED_BASE;
        const distToBase = dist(this, base);
        const healRadius = 6; // 治疗范围（游戏单位）
        if (distToBase <= healRadius) {
            this.isHealing = true;
            const healRate = 20; // 每秒回复 20 HP
            this.hp = Math.min(this.hp + healRate * dt, this.maxHp);
        } else {
            this.isHealing = false;
        }

        // 动画
        this.animationTimer += dt;
        if (this.animationTimer > 0.15) {
            this.animationTimer = 0;
            this.animationFrame = (this.animationFrame + 1) % 4;
        }

        // 冲锋
        if (this.state === 'dashing') {
            this.dashTimer -= dt;
            const d = dist(this, { x: this.dashTargetX, y: this.dashTargetY });
            if (d < 0.5 || this.dashTimer <= 0) {
                this.x = this.dashTargetX;
                this.y = this.dashTargetY;
                this.state = 'idle';
            } else {
                const angle = angleBetween(this, { x: this.dashTargetX, y: this.dashTargetY });
                const spd = d / this.dashTimer * dt;
                this.x += Math.cos(angle) * Math.min(spd, d);
                this.y += Math.sin(angle) * Math.min(spd, d);
                this.facing = angle;
            }
            return;
        }

        // 升级闪光效果
        if (this.levelUpFlash > 0) this.levelUpFlash -= dt;

        // 自动攻击冷却
        if (this.autoAttackTimer > 0) this.autoAttackTimer -= dt;

        // 自动攻击：当空闲且无手动攻击目标时，自动寻找附近敌人
        if ((this.state === 'idle' || this.state === 'moving') && !this.attackTarget && this.autoAttackTimer <= 0) {
            const enemies = world.getEnemiesOf(this.team).filter(e => e.alive);
            let autoTarget = null;
            // 优先级：敌方英雄 > 敌方小兵 > 敌方防御塔
            const heroes = enemies.filter(e => e.type === 'hero' && dist(this, e) <= this.autoAttackRange);
            const minions = enemies.filter(e => e.type === 'minion' && dist(this, e) <= this.autoAttackRange);
            const towers = enemies.filter(e => e.type === 'tower' && dist(this, e) <= this.autoAttackRange);

            if (heroes.length > 0) {
                // 选最近的英雄
                heroes.sort((a, b) => dist(this, a) - dist(this, b));
                autoTarget = heroes[0];
            } else if (minions.length > 0) {
                minions.sort((a, b) => dist(this, a) - dist(this, b));
                autoTarget = minions[0];
            } else if (towers.length > 0) {
                towers.sort((a, b) => dist(this, a) - dist(this, b));
                autoTarget = towers[0];
            }

            if (autoTarget) {
                const d = dist(this, autoTarget);
                if (d <= 5) {
                    // 在攻击范围内，直接攻击
                    this.state = 'attacking';
                    const dmg = this.getEffectiveDamage();
                    autoTarget.takeDamage(dmg);
                    this.facing = angleBetween(this, autoTarget);
                    if (!autoTarget.alive) {
                        world.onKill(this, autoTarget);
                    }
                    this.autoAttackTimer = this.autoAttackCooldown;
                } else {
                    // 走向目标
                    this.attackTarget = autoTarget;
                    this.targetX = autoTarget.x;
                    this.targetY = autoTarget.y;
                    this.state = 'moving';
                }
            }
        }

        // 攻击
        if (this.attackTarget && this.attackCooldown <= 0) {
            if (!this.attackTarget.alive) {
                this.attackTarget = null;
            } else {
                const d = dist(this, this.attackTarget);
                if (d <= 5) {
                    this.state = 'attacking';
                    const dmg = this.getEffectiveDamage();
                    this.attackTarget.takeDamage(dmg);
                    if (!this.attackTarget.alive) {
                        world.onKill(this, this.attackTarget);
                        this.attackTarget = null;
                    }
                    this.attackCooldown = 0.8;
                } else {
                    this.targetX = this.attackTarget.x;
                    this.targetY = this.attackTarget.y;
                    this.state = 'moving';
                }
            }
        }

        // 移动
        const dx = this.targetX - this.x;
        const dy = this.targetY - this.y;
        const d = Math.sqrt(dx * dx + dy * dy);
        if (d > 0.5) {
            this.state = 'moving';
            const moveX = (dx / d) * this.speed * dt * 60;
            const moveY = (dy / d) * this.speed * dt * 60;
            this.x += moveX;
            this.y += moveY;
            this.x = clamp(this.x, 1, MAP_SIZE - 1);
            this.y = clamp(this.y, 1, MAP_SIZE - 1);
            this.facing = Math.atan2(dy, dx);
        } else {
            if (this.state === 'moving') this.state = 'idle';
        }
    }
}

class Tower extends Entity {
    constructor(team, x, y, towerType) {
        super('tower', team, x, y);
        this.towerType = towerType; // 'outer' or 'crystal_tower'
        if (towerType === 'outer') {
            this.maxHp = 800;
            this.attack = 60;
            this.range = 12;
        } else {
            this.maxHp = 600;
            this.attack = 40;
            this.range = 10;
        }
        this.hp = this.maxHp;
        this.speed = 0;
        this.size = 20;
        this.attackCooldown = 0;
        this.attackTarget = null;
    }

    update(dt, world) {
        if (!this.alive) return;
        if (this.attackCooldown > 0) this.attackCooldown -= dt;

        // 寻找最近敌人
        const enemies = world.getEnemiesOf(this.team).filter(e => e.alive);
        let closest = null;
        let closestDist = Infinity;
        for (const e of enemies) {
            const d = dist(this, e);
            if (d <= this.range && d < closestDist) {
                closestDist = d;
                closest = e;
            }
        }

        if (closest && this.attackCooldown <= 0) {
            // 发射弹道
            const angle = angleBetween(this, closest);
            const proj = new Projectile(
                this.team,
                this.x, this.y,
                Math.cos(angle) * 12,
                Math.sin(angle) * 12,
                this.attack,
                1,
                this
            );
            proj.maxDistance = this.range + 2;
            proj.size = 4;
            world.addProjectile(proj);
            this.attackCooldown = 1.0;
        }
    }
}

class Crystal extends Entity {
    constructor(team, x, y) {
        super('crystal', team, x, y);
        this.maxHp = 1000;
        this.hp = 1000;
        this.speed = 0;
        this.size = 24;
        this.animationTimer = 0;
    }

    update(dt, world) {
        this.animationTimer += dt;
    }
}

class Minion extends Entity {
    constructor(team, x, y) {
        super('minion', team, x, y);
        this.maxHp = 200;
        this.hp = 200;
        this.attack = 15;
        this.speed = 0.07; // 小兵速度略低于英雄
        this.size = 12;
        this.attackCooldown = 0;
        this.attackTarget = null;
        this.state = 'moving';
        this.waypointIndex = 0;
    }

    update(dt, world) {
        if (!this.alive) return;
        if (this.attackCooldown > 0) this.attackCooldown -= dt;

        // 寻找范围内敌人
        const enemies = world.getEnemiesOf(this.team).filter(e => e.alive);
        let target = null;
        let targetDist = Infinity;
        for (const e of enemies) {
            const d = dist(this, e);
            if (d <= 8 && d < targetDist) {
                targetDist = d;
                target = e;
            }
        }

        if (target) {
            if (targetDist <= 3) {
                // 攻击
                if (this.attackCooldown <= 0) {
                    target.takeDamage(this.attack);
                    if (!target.alive) {
                        world.onKill(this, target);
                    }
                    this.attackCooldown = 1.0;
                }
                this.state = 'attacking';
            } else {
                // 靠近
                this.x += ((target.x - this.x) / targetDist) * this.speed * dt * 60;
                this.y += ((target.y - this.y) / targetDist) * this.speed * dt * 60;
                this.state = 'moving';
            }
        } else {
            // 沿中路前进
            const dest = this.team === 'blue' ? RED_BASE : BLUE_BASE;
            const dx = dest.x - this.x;
            const dy = dest.y - this.y;
            const d = Math.sqrt(dx * dx + dy * dy);
            if (d > 1) {
                this.x += (dx / d) * this.speed * dt * 60;
                this.y += (dy / d) * this.speed * dt * 60;
            }
            this.state = 'moving';
        }

        this.x = clamp(this.x, 1, MAP_SIZE - 1);
        this.y = clamp(this.y, 1, MAP_SIZE - 1);
    }
}

class Projectile extends Entity {
    constructor(team, x, y, vx, vy, damage, aoeRadius, owner) {
        super('projectile', team, x, y);
        this.vx = vx;
        this.vy = vy;
        this.damage = damage;
        this.aoeRadius = aoeRadius;
        this.owner = owner;
        this.speed = 15;
        this.size = 6;
        this.maxDistance = 20;
        this.distanceTraveled = 0;
        this.alive = true;
    }

    update(dt, world) {
        if (!this.alive) return;
        const moveX = this.vx * dt;
        const moveY = this.vy * dt;
        this.x += moveX;
        this.y += moveY;
        this.distanceTraveled += Math.sqrt(moveX * moveX + moveY * moveY);

        // 超出范围
        if (this.distanceTraveled >= this.maxDistance || this.x < 0 || this.x > MAP_SIZE || this.y < 0 || this.y > MAP_SIZE) {
            this.alive = false;
            return;
        }

        // 碰撞检测
        const enemies = world.getEnemiesOf(this.team).filter(e => e.alive);
        for (const e of enemies) {
            if (dist(this, e) < 2) {
                // AOE 伤害
                if (this.aoeRadius > 1) {
                    for (const e2 of enemies) {
                        if (dist(this, e2) <= this.aoeRadius) {
                            e2.takeDamage(this.damage);
                            if (!e2.alive) world.onKill(this.owner, e2);
                        }
                    }
                    world.addEffect({ type: 'aoe', x: this.x, y: this.y, radius: this.aoeRadius, timer: 0.3, team: this.team });
                } else {
                    e.takeDamage(this.damage);
                    if (!e.alive) world.onKill(this.owner, e);
                }
                this.alive = false;
                return;
            }
        }
    }
}

// ============================================================
// 游戏世界
// ============================================================

class GameWorld {
    constructor() {
        this.entities = [];
        this.projectiles = [];
        this.effects = [];
        this.killNotifications = [];
        this.minionSpawnTimer = 5; // 5秒后第一波
        this.gameTime = 0;
        this.gameOver = false;
        this.winner = null;
        this.blueKills = 0;
        this.redKills = 0;
    }

    init() {
        this.entities = [];
        this.projectiles = [];
        this.effects = [];
        this.killNotifications = [];

        // 蓝方英雄
        this.blueHero = new Hero('blue', BLUE_BASE.x, BLUE_BASE.y, '勇者');
        this.entities.push(this.blueHero);

        // 红方英雄
        this.redHero = new Hero('red', RED_BASE.x, RED_BASE.y, '魔王');
        this.entities.push(this.redHero);

        // 蓝方塔 - 外塔
        this.entities.push(new Tower('blue', 30, 70, 'outer'));
        // 蓝方塔 - 水晶塔
        this.entities.push(new Tower('blue', 18, 82, 'crystal_tower'));
        // 蓝方水晶
        this.entities.push(new Crystal('blue', 12, 88));

        // 红方塔 - 外塔
        this.entities.push(new Tower('red', 70, 30, 'outer'));
        // 红方塔 - 水晶塔
        this.entities.push(new Tower('red', 82, 18, 'crystal_tower'));
        // 红方水晶
        this.entities.push(new Crystal('red', 88, 12));
    }

    getHero(team) {
        return team === 'blue' ? this.blueHero : this.redHero;
    }

    getEnemiesOf(team) {
        return this.entities.filter(e => e.team !== team);
    }

    getAlliesOf(team) {
        return this.entities.filter(e => e.team === team);
    }

    addProjectile(proj) {
        this.projectiles.push(proj);
    }

    addEffect(effect) {
        this.effects.push(effect);
    }

    onKill(killer, victim) {
        if (!killer || !victim) return;

        let notification = '';

        if (victim.type === 'minion') {
            if (killer.type === 'hero') {
                killer.gold += 50;
                // 击杀小兵奖励：+1等级, +5攻击, +20最大生命
                killer.level += 1;
                killer.attack += 5;
                killer.maxHp += 20;
                killer.hp = Math.min(killer.hp + 20, killer.maxHp);
                killer.levelUpFlash = 0.8;
            }
        } else if (victim.type === 'hero') {
            victim.deaths++;
            victim.respawnTimer = 5;
            if (killer.type === 'hero') {
                killer.kills++;
                killer.gold += 200;
                // 击杀英雄奖励：+2等级, +10攻击, +30最大生命
                killer.level += 2;
                killer.attack += 10;
                killer.maxHp += 30;
                killer.hp = Math.min(killer.hp + 30, killer.maxHp);
                killer.levelUpFlash = 1.0;
            }
            notification = `${killer.name || TEAM_COLORS[killer.team].name} 击杀了 ${victim.name || TEAM_COLORS[victim.team].name}`;
            if (victim.team === 'blue') this.redKills++;
            else this.blueKills++;
        } else if (victim.type === 'tower') {
            if (killer.type === 'hero') {
                killer.gold += 100;
                // 击杀防御塔奖励：+3等级, +15攻击, +50最大生命
                killer.level += 3;
                killer.attack += 15;
                killer.maxHp += 50;
                killer.hp = Math.min(killer.hp + 50, killer.maxHp);
                killer.levelUpFlash = 1.2;
            }
            notification = `${TEAM_COLORS[killer.team].name} 摧毁了${TEAM_COLORS[victim.team].name}防御塔`;
        } else if (victim.type === 'crystal') {
            this.gameOver = true;
            this.winner = killer.team;
            notification = `${TEAM_COLORS[killer.team].name} 获胜！`;
        }

        if (notification) {
            this.killNotifications.push({ text: notification, timer: 3.0 });
        }
    }

    spawnMinions() {
        // 蓝方小兵
        for (let i = 0; i < 3; i++) {
            const m = new Minion('blue', BLUE_BASE.x + (i - 1) * 2, BLUE_BASE.y);
            this.entities.push(m);
        }
        // 红方小兵
        for (let i = 0; i < 3; i++) {
            const m = new Minion('red', RED_BASE.x + (i - 1) * 2, RED_BASE.y);
            this.entities.push(m);
        }
    }

    update(dt) {
        if (this.gameOver) return;

        this.gameTime += dt;

        // 刷兵
        this.minionSpawnTimer -= dt;
        if (this.minionSpawnTimer <= 0) {
            this.spawnMinions();
            this.minionSpawnTimer = 30;
        }

        // 更新实体
        for (const e of this.entities) {
            if (e.update) e.update(dt, this);
        }

        // 更新弹道
        for (const p of this.projectiles) {
            p.update(dt, this);
        }
        this.projectiles = this.projectiles.filter(p => p.alive);

        // 清理死亡小兵
        this.entities = this.entities.filter(e => {
            if (e.type === 'minion' && !e.alive) return false;
            return true;
        });

        // 更新特效
        for (const eff of this.effects) {
            eff.timer -= dt;
        }
        this.effects = this.effects.filter(e => e.timer > 0);

        // 更新通知
        for (const n of this.killNotifications) {
            n.timer -= dt;
        }
        this.killNotifications = this.killNotifications.filter(n => n.timer > 0);
    }
}

// ============================================================
// 游戏渲染器
// ============================================================

class GameRenderer {
    constructor(canvas) {
        this.canvas = canvas;
        this.ctx = canvas.getContext('2d');
        this.ctx.imageSmoothingEnabled = false;

        // 摄像机
        this.camX = 50;
        this.camY = 50;
        this.viewRange = 30; // 可视范围（游戏单位）

        this.time = 0;
        this.playerTeam = 'blue'; // 设置为玩家队伍，用于视角变换
    }

    // 世界坐标 -> 视图坐标（红方翻转）
    _transformWorld(wx, wy) {
        if (this.playerTeam === 'red') {
            return { x: MAP_SIZE - wx, y: MAP_SIZE - wy };
        }
        return { x: wx, y: wy };
    }

    // 视图坐标 -> 世界坐标（红方反翻转）
    _untransformWorld(tx, ty) {
        if (this.playerTeam === 'red') {
            return { x: MAP_SIZE - tx, y: MAP_SIZE - ty };
        }
        return { x: tx, y: ty };
    }

    worldToScreen(wx, wy) {
        const tw = this._transformWorld(wx, wy);
        const scale = Math.min(this.canvas.width, this.canvas.height) / (this.viewRange * 2);
        const sx = (tw.x - this.camX) * scale + this.canvas.width / 2;
        const sy = -(tw.y - this.camY) * scale + this.canvas.height / 2; // y轴翻转
        return { x: sx, y: sy, scale };
    }

    screenToWorld(sx, sy) {
        const scale = Math.min(this.canvas.width, this.canvas.height) / (this.viewRange * 2);
        const tx = (sx - this.canvas.width / 2) / scale + this.camX;
        const ty = -((sy - this.canvas.height / 2) / scale) + this.camY;
        return this._untransformWorld(tx, ty);
    }

    resize() {
        // 全屏模式：使用实际视口尺寸
        this.canvas.width = document.documentElement.clientWidth || window.innerWidth;
        this.canvas.height = document.documentElement.clientHeight || window.innerHeight;
        this.ctx.imageSmoothingEnabled = false;
    }

    updateCamera(hero) {
        if (!hero) return;
        let targetX = hero.alive ? hero.x : (hero.team === 'blue' ? BLUE_BASE.x : RED_BASE.x);
        let targetY = hero.alive ? hero.y : (hero.team === 'blue' ? BLUE_BASE.y : RED_BASE.y);
        // 视角变换
        const tw = this._transformWorld(targetX, targetY);
        this.camX = lerp(this.camX, tw.x, 0.08);
        this.camY = lerp(this.camY, tw.y, 0.08);
    }

    render(world, playerTeam) {
        const ctx = this.ctx;
        const w = this.canvas.width;
        const h = this.canvas.height;
        this.time += 1 / 60;
        this.playerTeam = playerTeam || 'blue';

        ctx.imageSmoothingEnabled = false;
        ctx.clearRect(0, 0, w, h);

        this.updateCamera(world.getHero(playerTeam));

        this.drawMap(ctx, world);
        this.drawEntities(ctx, world);
        this.drawProjectiles(ctx, world);
        this.drawEffects(ctx, world);
    }

    drawMap(ctx, world) {
        const { scale } = this.worldToScreen(0, 0);
        const pixelSize = Math.max(2, Math.floor(scale * 2));

        // 草地棋盘格
        for (let gx = 0; gx < MAP_SIZE; gx += 2) {
            for (let gy = 0; gy < MAP_SIZE; gy += 2) {
                const sp = this.worldToScreen(gx, gy);
                const sp2 = this.worldToScreen(gx + 2, gy + 2);
                const sw = sp2.x - sp.x;
                const sh = sp2.y - sp.y;

                if (sp.x + sw < 0 || sp.x > this.canvas.width || sp.y + sh < 0 || sp.y > this.canvas.height) continue;

                const checker = (gx + gy) % 4 === 0;
                ctx.fillStyle = checker ? '#3a7a2a' : '#2d6a1e';
                ctx.fillRect(Math.floor(sp.x), Math.floor(sp.y), Math.ceil(sw) + 1, Math.ceil(sh) + 1);
            }
        }

        // 中路道路 (从蓝方基地到红方基地的对角线)
        this.drawRoad(ctx);

        // 河流 (地图中间横穿)
        this.drawRiver(ctx);

        // 基地区域
        this.drawBase(ctx, BLUE_BASE.x, BLUE_BASE.y, 'blue');
        this.drawBase(ctx, RED_BASE.x, RED_BASE.y, 'red');

        // 塔平台
        const towers = world.entities.filter(e => e.type === 'tower');
        for (const t of towers) {
            const sp = this.worldToScreen(t.x, t.y);
            const platSize = 16 * (scale / (this.canvas.height / (this.viewRange * 2)));
            ctx.fillStyle = '#555566';
            ctx.fillRect(sp.x - platSize / 2, sp.y - platSize / 2, platSize, platSize);
            ctx.fillStyle = '#444455';
            ctx.fillRect(sp.x - platSize / 2 + 2, sp.y - platSize / 2 + 2, platSize - 4, platSize - 4);
        }
    }

    drawRoad(ctx) {
        // 对角线道路从蓝方基地到红方基地
        const steps = 50;
        const { scale } = this.worldToScreen(0, 0);
        const roadWidth = 4 * scale / (this.canvas.height / (this.viewRange * 2));

        for (let i = 0; i <= steps; i++) {
            const t = i / steps;
            const wx = lerp(BLUE_BASE.x, RED_BASE.x, t);
            const wy = lerp(BLUE_BASE.y, RED_BASE.y, t);
            const sp = this.worldToScreen(wx, wy);

            ctx.fillStyle = '#8B7355';
            ctx.fillRect(sp.x - roadWidth / 2, sp.y - roadWidth / 2, roadWidth, roadWidth);

            // 边缘
            ctx.fillStyle = '#7A6345';
            ctx.fillRect(sp.x - roadWidth / 2, sp.y - roadWidth / 2, 2, roadWidth);
            ctx.fillRect(sp.x + roadWidth / 2 - 2, sp.y - roadWidth / 2, 2, roadWidth);
        }
    }

    drawRiver(ctx) {
        const { scale } = this.worldToScreen(0, 0);
        const riverWidth = 3 * scale / (this.canvas.height / (this.viewRange * 2));
        const waveOffset = Math.sin(this.time * 2) * 2;

        for (let x = 0; x < MAP_SIZE; x += 1) {
            const wy = 50 + Math.sin((x + waveOffset) * 0.3) * 2;
            const sp = this.worldToScreen(x, wy);

            if (sp.x < -20 || sp.x > this.canvas.width + 20) continue;

            ctx.fillStyle = '#2266aa';
            ctx.fillRect(sp.x - riverWidth / 4, sp.y - riverWidth / 2, riverWidth / 2, riverWidth);

            // 波纹
            ctx.fillStyle = '#3388cc';
            const waveSize = 2 + Math.sin(this.time * 3 + x) * 1;
            ctx.fillRect(sp.x, sp.y - waveSize, waveSize, waveSize);
        }
    }

    drawBase(ctx, bx, by, team) {
        const sp = this.worldToScreen(bx, by);
        const { scale } = this.worldToScreen(0, 0);
        const baseSize = 8 * scale / (this.canvas.height / (this.viewRange * 2));
        const color = TEAM_COLORS[team];

        // 基地平台
        ctx.fillStyle = color.dark;
        ctx.fillRect(sp.x - baseSize / 2, sp.y - baseSize / 2, baseSize, baseSize);

        // 边框
        ctx.fillStyle = color.primary;
        ctx.fillRect(sp.x - baseSize / 2, sp.y - baseSize / 2, baseSize, 3);
        ctx.fillRect(sp.x - baseSize / 2, sp.y + baseSize / 2 - 3, baseSize, 3);
        ctx.fillRect(sp.x - baseSize / 2, sp.y - baseSize / 2, 3, baseSize);
        ctx.fillRect(sp.x + baseSize / 2 - 3, sp.y - baseSize / 2, 3, baseSize);

        // 内部十字
        ctx.fillStyle = color.light;
        ctx.fillRect(sp.x - 1, sp.y - baseSize / 4, 2, baseSize / 2);
        ctx.fillRect(sp.x - baseSize / 4, sp.y - 1, baseSize / 2, 2);
    }

    drawEntities(ctx, world) {
        // 按y排序实现简单遮挡
        const sorted = [...world.entities].sort((a, b) => a.y - b.y);

        for (const e of sorted) {
            if (!e.alive && e.type !== 'hero') continue;
            if (!e.alive && e.type === 'hero') continue; // 死亡英雄不显示

            const sp = this.worldToScreen(e.x, e.y);

            // 裁剪
            if (sp.x < -50 || sp.x > this.canvas.width + 50 || sp.y < -50 || sp.y > this.canvas.height + 50) continue;

            switch (e.type) {
                case 'hero': this.drawHero(ctx, e, sp); break;
                case 'tower': this.drawTower(ctx, e, sp); break;
                case 'crystal': this.drawCrystal(ctx, e, sp); break;
                case 'minion': this.drawMinion(ctx, e, sp); break;
            }

            // HP 条
            if (e.hp < e.maxHp || e.type === 'hero') {
                this.drawHpBar(ctx, e, sp);
            }
        }
    }

    drawHero(ctx, hero, sp) {
        const color = TEAM_COLORS[hero.team];
        const s = 12; // 半宽

        // 阴影
        ctx.fillStyle = 'rgba(0,0,0,0.3)';
        ctx.fillRect(sp.x - s + 2, sp.y - s + 2, s * 2, s * 2);

        // 身体
        ctx.fillStyle = color.primary;
        ctx.fillRect(sp.x - s, sp.y - s, s * 2, s * 2);

        // 深色内框
        ctx.fillStyle = color.dark;
        ctx.fillRect(sp.x - s + 3, sp.y - s + 3, s * 2 - 6, s * 2 - 6);

        // 脸部区域
        ctx.fillStyle = '#ffcc88';
        ctx.fillRect(sp.x - 4, sp.y - 8, 8, 8);

        // 眼睛
        ctx.fillStyle = '#222';
        const eyeOffX = Math.cos(hero.facing) * 2;
        ctx.fillRect(sp.x - 3 + eyeOffX, sp.y - 6, 2, 2);
        ctx.fillRect(sp.x + 1 + eyeOffX, sp.y - 6, 2, 2);

        // 武器（根据朝向）
        const weaponAngle = hero.facing;
        const wx = sp.x + Math.cos(weaponAngle) * (s + 4);
        const wy = sp.y + Math.sin(weaponAngle) * (s + 4);
        ctx.fillStyle = '#cccccc';
        ctx.fillRect(wx - 3, wy - 1, 6, 2);
        ctx.fillStyle = '#888888';
        ctx.fillRect(wx - 1, wy - 3, 2, 6);

        // 肩甲
        ctx.fillStyle = color.light;
        ctx.fillRect(sp.x - s, sp.y - s, 4, 4);
        ctx.fillRect(sp.x + s - 4, sp.y - s, 4, 4);

        // 名字
        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 10px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(hero.name, sp.x, sp.y + s + 14);

        // 等级
        ctx.fillStyle = '#ffff00';
        ctx.font = 'bold 9px monospace';
        ctx.fillText('Lv.' + hero.level, sp.x, sp.y - s - 16);

        // 升级闪光效果
        if (hero.levelUpFlash > 0) {
            const alpha = Math.min(1, hero.levelUpFlash * 2);
            ctx.fillStyle = `rgba(255,255,100,${alpha * 0.4})`;
            ctx.fillRect(sp.x - s - 4, sp.y - s - 4, s * 2 + 8, s * 2 + 8);
            ctx.strokeStyle = `rgba(255,255,0,${alpha})`;
            ctx.lineWidth = 2;
            ctx.strokeRect(sp.x - s - 2, sp.y - s - 2, s * 2 + 4, s * 2 + 4);
        }

        // 基地治疗效果 - 绿色光晕
        if (hero.isHealing) {
            const healGlow = Math.sin(hero.animationTimer * 10) * 0.2 + 0.3;
            ctx.fillStyle = `rgba(0,255,100,${healGlow})`;
            ctx.fillRect(sp.x - s - 6, sp.y - s - 6, s * 2 + 12, s * 2 + 12);
            // 十字治疗标记
            ctx.fillStyle = `rgba(0,255,100,${healGlow + 0.2})`;
            ctx.fillRect(sp.x - 1, sp.y - s - 8, 2, 6);
            ctx.fillRect(sp.x - 3, sp.y - s - 6, 6, 2);
        }
    }

    drawTower(ctx, tower, sp) {
        const s = 10;

        // 阴影
        ctx.fillStyle = 'rgba(0,0,0,0.3)';
        ctx.fillRect(sp.x - s + 2, sp.y - s + 2, s * 2, s * 2);

        // 塔身 - 灰色石头
        ctx.fillStyle = '#777788';
        ctx.fillRect(sp.x - s, sp.y - s, s * 2, s * 2);

        // 石头纹理
        ctx.fillStyle = '#666677';
        ctx.fillRect(sp.x - s + 2, sp.y - s + 4, 6, 4);
        ctx.fillRect(sp.x + 2, sp.y + 2, 6, 4);
        ctx.fillRect(sp.x - 4, sp.y + 6, 8, 3);

        // 顶部 - 阵营颜色
        const color = TEAM_COLORS[tower.team];
        ctx.fillStyle = color.primary;
        ctx.fillRect(sp.x - s, sp.y - s, s * 2, 5);

        // 塔顶装饰
        ctx.fillStyle = color.light;
        ctx.fillRect(sp.x - 2, sp.y - s - 4, 4, 4);

        // 攻击范围指示（半透明）
        // 不画了，太密

        // 塔类型标记
        if (tower.towerType === 'crystal_tower') {
            ctx.fillStyle = '#ffffff';
            ctx.fillRect(sp.x - 1, sp.y - s - 6, 2, 2);
        }
    }

    drawCrystal(ctx, crystal, sp) {
        const color = TEAM_COLORS[crystal.team];
        const s = 12;
        const glow = Math.sin(crystal.animationTimer * 3) * 0.3 + 0.7;

        // 发光效果
        ctx.fillStyle = `rgba(${crystal.team === 'blue' ? '68,136,255' : '255,68,68'},${glow * 0.2})`;
        ctx.fillRect(sp.x - s - 4, sp.y - s - 4, s * 2 + 8, s * 2 + 8);

        // 钻石形状
        ctx.fillStyle = color.primary;
        // 上三角
        ctx.beginPath();
        ctx.moveTo(sp.x, sp.y - s);
        ctx.lineTo(sp.x + s, sp.y);
        ctx.lineTo(sp.x, sp.y + s);
        ctx.lineTo(sp.x - s, sp.y);
        ctx.closePath();
        ctx.fill();

        // 内部高光
        ctx.fillStyle = color.light;
        ctx.beginPath();
        ctx.moveTo(sp.x, sp.y - s + 4);
        ctx.lineTo(sp.x + s - 6, sp.y);
        ctx.lineTo(sp.x, sp.y + 2);
        ctx.lineTo(sp.x - 3, sp.y);
        ctx.closePath();
        ctx.fill();

        // 闪光点
        if (glow > 0.8) {
            ctx.fillStyle = '#ffffff';
            ctx.fillRect(sp.x - 1, sp.y - s + 2, 2, 2);
        }
    }

    drawMinion(ctx, minion, sp) {
        const color = TEAM_COLORS[minion.team];
        const s = 6;

        // 阴影
        ctx.fillStyle = 'rgba(0,0,0,0.3)';
        ctx.fillRect(sp.x - s + 1, sp.y - s + 1, s * 2, s * 2);

        // 身体
        ctx.fillStyle = color.primary;
        ctx.fillRect(sp.x - s, sp.y - s, s * 2, s * 2);

        // 深色内框
        ctx.fillStyle = color.dark;
        ctx.fillRect(sp.x - s + 2, sp.y - s + 2, s * 2 - 4, s * 2 - 4);

        // 眼睛
        ctx.fillStyle = '#ffffff';
        ctx.fillRect(sp.x - 2, sp.y - 2, 2, 2);
        ctx.fillRect(sp.x + 1, sp.y - 2, 2, 2);
    }

    drawProjectiles(ctx, world) {
        for (const p of world.projectiles) {
            if (!p.alive) continue;
            const sp = this.worldToScreen(p.x, p.y);

            if (sp.x < -10 || sp.x > this.canvas.width + 10 || sp.y < -10 || sp.y > this.canvas.height + 10) continue;

            const color = TEAM_COLORS[p.team];
            const s = p.size / 2;

            // 弹道光晕
            ctx.fillStyle = `rgba(255,255,200,0.5)`;
            ctx.fillRect(sp.x - s - 2, sp.y - s - 2, s * 2 + 4, s * 2 + 4);

            // 弹道本体
            ctx.fillStyle = color.light;
            ctx.fillRect(sp.x - s, sp.y - s, s * 2, s * 2);

            // 核心亮点
            ctx.fillStyle = '#ffffff';
            ctx.fillRect(sp.x - 1, sp.y - 1, 2, 2);
        }
    }

    drawEffects(ctx, world) {
        for (const eff of world.effects) {
            const sp = this.worldToScreen(eff.x, eff.y);
            const { scale } = this.worldToScreen(0, 0);
            const alpha = eff.timer * 2;
            const color = TEAM_COLORS[eff.team];
            const radius = eff.radius * scale / (this.canvas.height / (this.viewRange * 2));

            ctx.fillStyle = `rgba(${eff.team === 'blue' ? '68,136,255' : '255,68,68'},${alpha * 0.3})`;
            ctx.beginPath();
            ctx.arc(sp.x, sp.y, radius, 0, Math.PI * 2);
            ctx.fill();

            ctx.strokeStyle = `rgba(${eff.team === 'blue' ? '100,170,255' : '255,100,100'},${alpha * 0.6})`;
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.arc(sp.x, sp.y, radius, 0, Math.PI * 2);
            ctx.stroke();
        }
    }

    drawHpBar(ctx, entity, sp) {
        const barWidth = entity.size || 20;
        const barHeight = 3;
        const y = sp.y - (entity.size || 10) - 8;
        const hpRatio = entity.hp / entity.maxHp;

        // 背景
        ctx.fillStyle = '#333';
        ctx.fillRect(sp.x - barWidth / 2, y, barWidth, barHeight);

        // HP
        let hpColor = '#44ff44';
        if (hpRatio < 0.3) hpColor = '#ff4444';
        else if (hpRatio < 0.6) hpColor = '#ffaa44';

        ctx.fillStyle = hpColor;
        ctx.fillRect(sp.x - barWidth / 2, y, barWidth * hpRatio, barHeight);

        // 边框
        ctx.strokeStyle = '#000';
        ctx.lineWidth = 1;
        ctx.strokeRect(sp.x - barWidth / 2, y, barWidth, barHeight);
    }

    // ---- HUD 渲染 ----

    drawHUD(ctx, world, playerTeam, input) {
        const hero = world.getHero(playerTeam);
        const w = this.canvas.width;
        const h = this.canvas.height;

        // 小地图
        this.drawMinimap(ctx, world, playerTeam, w, h);

        // 计时器和击杀数
        this.drawScoreboard(ctx, world, w, h);

        // 英雄信息
        this.drawHeroInfo(ctx, hero, w, h);

        // 技能栏
        this.drawSkillBar(ctx, hero, w, h, input);

        // 击杀通知
        this.drawKillNotifications(ctx, world, w, h);

        // 选中目标信息
        if (input && input.selectedEntity) {
            this.drawSelectedInfo(ctx, input.selectedEntity, w, h);
        }

        // 游戏结束
        if (world.gameOver) {
            this.drawGameOver(ctx, world, w, h);
        }
    }

    drawMinimap(ctx, world, playerTeam, w, h) {
        const mmSize = 140;
        const mmX = 10;
        const mmY = 10;
        const unitSize = mmSize / MAP_SIZE;

        // 背景
        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(mmX, mmY, mmSize, mmSize);

        // 草地
        ctx.fillStyle = '#1a3a0a';
        ctx.fillRect(mmX + 1, mmY + 1, mmSize - 2, mmSize - 2);

        // 中路
        ctx.strokeStyle = '#5a4a30';
        ctx.lineWidth = 2;
        ctx.beginPath();
        let blueBaseMM = { x: mmX + BLUE_BASE.x * unitSize, y: mmY + mmSize - BLUE_BASE.y * unitSize };
        let redBaseMM = { x: mmX + RED_BASE.x * unitSize, y: mmY + mmSize - RED_BASE.y * unitSize };
        if (this.playerTeam === 'red') {
            // 翻转小地图坐标
            blueBaseMM = { x: mmX + mmSize - BLUE_BASE.x * unitSize, y: mmY + BLUE_BASE.y * unitSize };
            redBaseMM = { x: mmX + mmSize - RED_BASE.x * unitSize, y: mmY + RED_BASE.y * unitSize };
        }
        ctx.moveTo(blueBaseMM.x, blueBaseMM.y);
        ctx.lineTo(redBaseMM.x, redBaseMM.y);
        ctx.stroke();

        // 实体点
        for (const e of world.entities) {
            if (!e.alive) continue;
            let ex, ey;
            if (this.playerTeam === 'red') {
                ex = mmX + (MAP_SIZE - e.x) * unitSize;
                ey = mmY + e.y * unitSize;
            } else {
                ex = mmX + e.x * unitSize;
                ey = mmY + mmSize - e.y * unitSize;
            }

            let dotSize = 2;
            let color = TEAM_COLORS[e.team].primary;

            if (e.type === 'hero') { dotSize = 4; color = TEAM_COLORS[e.team].light; }
            else if (e.type === 'tower' || e.type === 'crystal') { dotSize = 3; }
            else if (e.type === 'minion') { dotSize = 1.5; }

            ctx.fillStyle = color;
            ctx.fillRect(ex - dotSize / 2, ey - dotSize / 2, dotSize, dotSize);
        }

        // 摄像机视野
        const camLeft = (this.camX - this.viewRange) * unitSize;
        const camTop = (this.camY - this.viewRange) * unitSize;
        ctx.strokeStyle = 'rgba(255,255,255,0.5)';
        ctx.lineWidth = 1;
        ctx.strokeRect(
            mmX + camLeft,
            mmY + mmSize - camTop - this.viewRange * 2 * unitSize,
            this.viewRange * 2 * unitSize,
            this.viewRange * 2 * unitSize
        );

        // 边框
        ctx.strokeStyle = '#888';
        ctx.lineWidth = 2;
        ctx.strokeRect(mmX, mmY, mmSize, mmSize);
    }

    drawScoreboard(ctx, world, w, h) {
        const minutes = Math.floor(world.gameTime / 60);
        const seconds = Math.floor(world.gameTime % 60);
        const timeStr = `${minutes}:${seconds.toString().padStart(2, '0')}`;

        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(w / 2 - 100, 5, 200, 35);

        ctx.fillStyle = '#4488ff';
        ctx.font = 'bold 16px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(world.blueKills.toString(), w / 2 - 50, 28);

        ctx.fillStyle = '#ffffff';
        ctx.font = '14px monospace';
        ctx.fillText(timeStr, w / 2, 28);

        ctx.fillStyle = '#ff4444';
        ctx.font = 'bold 16px monospace';
        ctx.fillText(world.redKills.toString(), w / 2 + 50, 28);
    }

    drawHeroInfo(ctx, hero, w, h) {
        if (!hero) return;

        const panelX = 10;
        const panelY = h - 130;
        const panelW = 200;
        const panelH = 50;

        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(panelX, panelY, panelW, panelH);

        // 名字和等级
        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 12px monospace';
        ctx.textAlign = 'left';
        ctx.fillText(`${hero.name} Lv.${hero.level}`, panelX + 8, panelY + 16);

        // HP 条
        const hpBarW = panelW - 16;
        const hpRatio = hero.hp / hero.maxHp;
        ctx.fillStyle = '#333';
        ctx.fillRect(panelX + 8, panelY + 22, hpBarW, 8);
        ctx.fillStyle = hpRatio < 0.3 ? '#ff4444' : (hpRatio < 0.6 ? '#ffaa44' : '#44ff44');
        ctx.fillRect(panelX + 8, panelY + 22, hpBarW * hpRatio, 8);
        ctx.fillStyle = '#fff';
        ctx.font = '8px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(`${Math.ceil(hero.hp)}/${hero.maxHp}`, panelX + 8 + hpBarW / 2, panelY + 29);

        // 经验条
        const expRatio = hero.exp / hero.expToLevel;
        ctx.fillStyle = '#333';
        ctx.fillRect(panelX + 8, panelY + 34, hpBarW, 5);
        ctx.fillStyle = '#aa44ff';
        ctx.fillRect(panelX + 8, panelY + 34, hpBarW * expRatio, 5);

        // 金币
        ctx.fillStyle = '#ffdd44';
        ctx.font = '10px monospace';
        ctx.textAlign = 'left';
        ctx.fillText(`G:${hero.gold}`, panelX + 8, panelY + 48);

        // KDA
        ctx.fillStyle = '#ccc';
        ctx.fillText(`KDA: ${hero.kills}/${hero.deaths}/0`, panelX + 80, panelY + 48);

        // 死亡提示
        if (!hero.alive) {
            ctx.fillStyle = 'rgba(0,0,0,0.5)';
            ctx.fillRect(0, 0, w, h);
            ctx.fillStyle = '#ff4444';
            ctx.font = 'bold 24px monospace';
            ctx.textAlign = 'center';
            ctx.fillText(`已阵亡 ${Math.ceil(hero.respawnTimer)}s 后复活`, w / 2, h / 2);
        }
    }

    drawSkillBar(ctx, hero, w, h, input) {
        if (!hero) return;

        const barWidth = 260;
        const barHeight = 55;
        const barX = (w - barWidth) / 2;
        const barY = h - barHeight - 5;

        ctx.fillStyle = 'rgba(0,0,0,0.8)';
        ctx.fillRect(barX, barY, barWidth, barHeight);

        const skillW = 50;
        const gap = 10;
        const startX = barX + (barWidth - (skillW * 4 + gap * 3)) / 2;

        for (let i = 0; i < 4; i++) {
            const key = SKILL_KEYS[i];
            const skill = SKILLS[key];
            const sx = startX + i * (skillW + gap);
            const sy = barY + 5;

            // 技能背景
            ctx.fillStyle = '#2a2a3a';
            ctx.fillRect(sx, sy, skillW, skillW);

            // 技能名
            ctx.fillStyle = '#aaaacc';
            ctx.font = 'bold 10px monospace';
            ctx.textAlign = 'center';
            ctx.fillText(key, sx + skillW / 2, sy + 16);
            ctx.fillStyle = '#8888aa';
            ctx.font = '8px monospace';
            ctx.fillText(skill.name, sx + skillW / 2, sy + 30);

            // 冷却遮罩
            if (hero.cooldowns[key] > 0) {
                const cdRatio = hero.cooldowns[key] / skill.cooldown;
                ctx.fillStyle = 'rgba(0,0,0,0.7)';
                ctx.fillRect(sx, sy, skillW, skillW * cdRatio);

                ctx.fillStyle = '#ffffff';
                ctx.font = 'bold 14px monospace';
                ctx.fillText(Math.ceil(hero.cooldowns[key]).toString(), sx + skillW / 2, sy + skillW / 2 + 5);
            }

            // 边框
            ctx.strokeStyle = hero.cooldowns[key] <= 0 ? '#66aaff' : '#444';
            ctx.lineWidth = 2;
            ctx.strokeRect(sx, sy, skillW, skillW);

            // 按键提示
            ctx.fillStyle = '#ffffff';
            ctx.font = 'bold 12px monospace';
            ctx.textAlign = 'left';
        }

        // 操作提示
        ctx.fillStyle = '#666';
        ctx.font = '9px monospace';
        ctx.textAlign = 'center';
        ctx.fillText('右键移动 | QWER技能 | 空格停止 | 左键选中', w / 2, barY + barHeight - 2);
    }

    drawKillNotifications(ctx, world, w, h) {
        ctx.textAlign = 'center';
        for (let i = 0; i < world.killNotifications.length; i++) {
            const n = world.killNotifications[i];
            const alpha = Math.min(1, n.timer);
            ctx.fillStyle = `rgba(255,255,255,${alpha})`;
            ctx.font = 'bold 18px monospace';
            ctx.fillText(n.text, w / 2, h / 2 - 50 + i * 30);
        }
    }

    drawSelectedInfo(ctx, entity, w, h) {
        if (!entity || !entity.alive) return;

        const panelX = w - 180;
        const panelY = 50;
        const panelW = 170;
        const panelH = 80;

        ctx.fillStyle = 'rgba(0,0,0,0.8)';
        ctx.fillRect(panelX, panelY, panelW, panelH);

        ctx.fillStyle = TEAM_COLORS[entity.team].primary;
        ctx.font = 'bold 12px monospace';
        ctx.textAlign = 'left';
        const typeName = { hero: '英雄', tower: '防御塔', crystal: '水晶', minion: '小兵' }[entity.type] || entity.type;
        ctx.fillText(`${typeName} (${TEAM_COLORS[entity.team].name})`, panelX + 8, panelY + 18);

        ctx.fillStyle = '#aaa';
        ctx.font = '10px monospace';
        ctx.fillText(`HP: ${Math.ceil(entity.hp)}/${entity.maxHp}`, panelX + 8, panelY + 35);

        if (entity.type === 'hero') {
            ctx.fillText(`Lv.${entity.level} ATK:${Math.floor(entity.getEffectiveDamage())}`, panelX + 8, panelY + 50);
            ctx.fillText(`KDA: ${entity.kills}/${entity.deaths}/0`, panelX + 8, panelY + 65);
        }

        // 选中圈
        const sp = this.worldToScreen(entity.x, entity.y);
        const r = (entity.size || 12) + 4;
        ctx.strokeStyle = '#ffff00';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.arc(sp.x, sp.y, r, 0, Math.PI * 2);
        ctx.stroke();
    }

    drawGameOver(ctx, world, w, h) {
        ctx.fillStyle = 'rgba(0,0,0,0.7)';
        ctx.fillRect(0, 0, w, h);

        ctx.fillStyle = world.winner === 'blue' ? '#4488ff' : '#ff4444';
        ctx.font = 'bold 36px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(`${TEAM_COLORS[world.winner].name} 胜利！`, w / 2, h / 2 - 40);

        ctx.fillStyle = '#ffffff';
        ctx.font = '16px monospace';
        ctx.fillText(`${TEAM_COLORS.blue.name} ${world.blueKills} : ${world.redKills} ${TEAM_COLORS.red.name}`, w / 2, h / 2 + 10);

        // 双方英雄 KDA
        if (world.blueHero && world.redHero) {
            ctx.font = '12px monospace';
            ctx.fillText(`${world.blueHero.name} ${world.blueHero.kills}/${world.blueHero.deaths}/0`, w / 2, h / 2 + 40);
            ctx.fillText(`${world.redHero.name} ${world.redHero.kills}/${world.redHero.deaths}/0`, w / 2, h / 2 + 60);
        }

        ctx.fillStyle = '#aaa';
        ctx.font = '14px monospace';
        ctx.fillText('点击任意位置返回', w / 2, h / 2 + 100);
    }
}

// ============================================================
// 游戏输入
// ============================================================

class GameInput {
    constructor(canvas, renderer, world) {
        this.canvas = canvas;
        this.renderer = renderer;
        this.world = world;
        this.playerTeam = 'blue';
        this.selectedEntity = null;
        this.mouseWorldX = 0;
        this.mouseWorldY = 0;
        this.mouseScreenX = 0;
        this.mouseScreenY = 0;
        this.keys = {};
        this.onSkillCast = null;
        this.onMoveCommand = null;

        this._boundMouseMove = this._onMouseMove.bind(this);
        this._boundMouseDown = this._onMouseDown.bind(this);
        this._boundKeyDown = this._onKeyDown.bind(this);
        this._boundKeyUp = this._onKeyUp.bind(this);
        this._boundContextMenu = this._onContextMenu.bind(this);
    }

    bind(playerTeam) {
        this.playerTeam = playerTeam;
        this.canvas.addEventListener('mousemove', this._boundMouseMove);
        this.canvas.addEventListener('mousedown', this._boundMouseDown);
        this.canvas.addEventListener('contextmenu', this._boundContextMenu);
        window.addEventListener('keydown', this._boundKeyDown);
        window.addEventListener('keyup', this._boundKeyUp);
    }

    unbind() {
        this.canvas.removeEventListener('mousemove', this._boundMouseMove);
        this.canvas.removeEventListener('mousedown', this._boundMouseDown);
        this.canvas.removeEventListener('contextmenu', this._boundContextMenu);
        window.removeEventListener('keydown', this._boundKeyDown);
        window.removeEventListener('keyup', this._boundKeyUp);
    }

    _onMouseMove(e) {
        const rect = this.canvas.getBoundingClientRect();
        this.mouseScreenX = e.clientX - rect.left;
        this.mouseScreenY = e.clientY - rect.top;
        const wp = this.renderer.screenToWorld(this.mouseScreenX, this.mouseScreenY);
        this.mouseWorldX = wp.x;
        this.mouseWorldY = wp.y;
    }

    _onMouseDown(e) {
        if (e.button === 0) {
            // 左键选中
            this._selectEntity();
        } else if (e.button === 2) {
            // 右键移动
            this._moveHero();
        }
    }

    _onContextMenu(e) {
        e.preventDefault();
        this._moveHero();
    }

    _onKeyDown(e) {
        const key = e.key.toUpperCase();
        this.keys[key] = true;

        const hero = this.world.getHero(this.playerTeam);
        if (!hero || !hero.alive) return;

        if (SKILL_KEYS.includes(key)) {
            const skill = SKILLS[key];
            let targetPos = { x: this.mouseWorldX, y: this.mouseWorldY };

            if (skill.type === 'dash') {
                targetPos = null; // 冲锋用英雄朝向
            }

            hero.castSkill(key, this.world, targetPos);
            if (this.onSkillCast) this.onSkillCast(SKILL_KEYS.indexOf(key), targetPos);
        }

        if (key === ' ') {
            e.preventDefault();
            hero.targetX = hero.x;
            hero.targetY = hero.y;
            hero.attackTarget = null;
            hero.state = 'idle';
        }
    }

    _onKeyUp(e) {
        this.keys[e.key.toUpperCase()] = false;
    }

    _selectEntity() {
        const wp = this.renderer.screenToWorld(this.mouseScreenX, this.mouseScreenY);
        let closest = null;
        let closestDist = 3; // 选择范围

        for (const e of this.world.entities) {
            if (!e.alive) continue;
            const d = dist(wp, e);
            if (d < closestDist) {
                closestDist = d;
                closest = e;
            }
        }

        this.selectedEntity = closest;

        // 如果选中的是敌方，设置攻击目标
        if (closest && closest.team !== this.playerTeam) {
            const hero = this.world.getHero(this.playerTeam);
            if (hero && hero.alive) {
                hero.attackTarget = closest;
            }
        }
    }

    _moveHero() {
        const hero = this.world.getHero(this.playerTeam);
        if (!hero || !hero.alive) return;

        const wp = this.renderer.screenToWorld(this.mouseScreenX, this.mouseScreenY);
        hero.targetX = clamp(wp.x, 1, MAP_SIZE - 1);
        hero.targetY = clamp(wp.y, 1, MAP_SIZE - 1);
        hero.attackTarget = null;
        hero.state = 'moving';
        if (this.onMoveCommand) this.onMoveCommand(hero.targetX, hero.targetY);
    }
}

// ============================================================
// 游戏 AI
// ============================================================

class GameAI {
    constructor(world, team) {
        this.world = world;
        this.team = team;
        this.thinkTimer = 0;
        this.state = 'follow_minions'; // follow_minions, retreat, attack, push
        this.stateTimer = 0;
    }

    update(dt) {
        const hero = this.world.getHero(this.team);
        if (!hero || !hero.alive) return;

        this.thinkTimer -= dt;
        this.stateTimer -= dt;

        if (this.thinkTimer <= 0) {
            this.thinkTimer = 0.5; // 每0.5秒决策一次
            this.think(hero);
        }

        // 执行行为
        this.execute(hero, dt);
    }

    think(hero) {
        const hpRatio = hero.hp / hero.maxHp;
        const enemyHero = this.world.getHero(hero.team === 'blue' ? 'red' : 'blue');
        const base = this.team === 'blue' ? RED_BASE : BLUE_BASE;

        // 血量低回撤
        if (hpRatio < 0.25) {
            this.state = 'retreat';
            this.stateTimer = 3;
            return;
        }

        // 技能释放
        this.tryUseSkills(hero, enemyHero);

        // 寻找敌方单位
        const enemies = this.world.getEnemiesOf(this.team).filter(e => e.alive);
        const nearbyEnemies = enemies.filter(e => dist(hero, e) < 12);

        if (nearbyEnemies.length > 0 && hpRatio > 0.5) {
            // 优先攻击血量最低的
            nearbyEnemies.sort((a, b) => a.hp / a.maxHp - b.hp / b.maxHp);
            hero.attackTarget = nearbyEnemies[0];
            this.state = 'attack';
        } else if (hpRatio > 0.6) {
            // 跟随小兵
            const allies = this.world.entities.filter(e => e.team === this.team && e.type === 'minion' && e.alive);
            if (allies.length > 0) {
                // 跟随最前方的小兵
                allies.sort((a, b) => {
                    const da = dist(a, base);
                    const db = dist(b, base);
                    return da - db;
                });
                const leader = allies[0];
                hero.targetX = leader.x + (Math.random() - 0.5) * 3;
                hero.targetY = leader.y + (Math.random() - 0.5) * 3;
                hero.attackTarget = null;
                this.state = 'follow_minions';
            } else {
                // 没有小兵，自己推
                hero.targetX = base.x;
                hero.targetY = base.y;
                this.state = 'push';
            }
        }
    }

    tryUseSkills(hero, enemyHero) {
        if (!enemyHero || !enemyHero.alive) return;
        const d = dist(hero, enemyHero);

        // R - 大招（远距离）
        if (hero.canCast('R') && d < SKILLS.R.range) {
            hero.castSkill('R', this.world, { x: enemyHero.x, y: enemyHero.y });
        }

        // W - AOE（近距离）
        if (hero.canCast('W') && d < SKILLS.W.range) {
            hero.castSkill('W', this.world);
        }

        // Q - 冲锋
        if (hero.canCast('Q') && d < SKILLS.Q.range) {
            hero.facing = angleBetween(hero, enemyHero);
            hero.castSkill('Q', this.world);
        }

        // E - 远程弹道
        if (hero.canCast('E') && d < SKILLS.E.range) {
            hero.castSkill('E', this.world, { x: enemyHero.x, y: enemyHero.y });
        }
    }

    execute(hero, dt) {
        if (this.state === 'retreat') {
            const base = this.team === 'blue' ? BLUE_BASE : RED_BASE;
            hero.targetX = base.x;
            hero.targetY = base.y;
            hero.attackTarget = null;
        }
    }
}

// ============================================================
// 游戏阶段
// ============================================================

class GamePhase {
    static MATCHING = 'matching';
    static LOADING = 'loading';
    static PLAYING = 'playing';
    static RESULT = 'result';
}

// ============================================================
// 主游戏类
// ============================================================

class MobaGame {
    constructor(canvas, minimapCanvas, onGameEnd) {
        // 支持两种调用方式：
        // 1. new MobaGame(canvasElement, minimapCanvasElement, callback)
        // 2. new MobaGame(canvasIdString, callback)
        if (typeof canvas === 'string') {
            this.canvas = document.getElementById(canvas);
            this.minimapCanvas = null;
            this.onGameEnd = minimapCanvas || function () {};
        } else {
            this.canvas = canvas;
            this.minimapCanvas = minimapCanvas || null;
            this.onGameEnd = onGameEnd || function () {};
        }
        this.running = false;
        this.phase = GamePhase.MATCHING;
        this.playerTeam = 'blue';

        this.world = null;
        this.renderer = null;
        this.input = null;
        this.ai = null;
        this.networkSync = null;
        this.isNetworkMode = false;
        this.remoteHeroState = null;
        this.opponentReady = false;
        this.localReady = false;

        this.phaseTimer = 0;
        this.loadingTimer = 1.5;
        this.animFrame = null;
        this.lastTime = 0;

        this._boundLoop = this._gameLoop.bind(this);
        this._boundResize = this._onResize.bind(this);
        this._boundClick = this._onResultClick.bind(this);

        MobaGame._idCounter = 1;
    }

    start(playerTeam, networkConfig) {
        this.playerTeam = playerTeam || 'blue';
        this.running = true;
        this.phase = GamePhase.MATCHING;
        this.phaseTimer = 0.5; // VS AI 模式快速进入

        if (networkConfig) {
            this.isNetworkMode = true;
            this.networkSync = new NetworkSync(
                networkConfig.realtimeAPI || networkConfig.chatAPI,
                networkConfig.playerID,
                networkConfig.playerName,
                networkConfig.battleRoomId,
                networkConfig.isHost,
                (type, data) => this._onRemoteData(type, data)
            );
        }

        this.world = new GameWorld();
        this.renderer = new GameRenderer(this.canvas);
        this.renderer.resize();
        this.input = new GameInput(this.canvas, this.renderer, this.world);
        this.input.onMoveCommand = (x, y) => {
            if (this.networkSync) this.networkSync.sendCommand({ kind: 'move', x, y });
        };
        this.input.onSkillCast = (slot, targetPos) => {
            if (this.networkSync) this.networkSync.sendCommand({ kind: 'skill', slot, target: targetPos });
        };

        window.addEventListener('resize', this._boundResize);

        this.lastTime = performance.now();
        this._gameLoop(this.lastTime);
    }

    stop() {
        this.running = false;
        if (this.animFrame) {
            cancelAnimationFrame(this.animFrame);
            this.animFrame = null;
        }
        if (this.input) this.input.unbind();
        window.removeEventListener('resize', this._boundResize);
        this.canvas.removeEventListener('click', this._boundClick);
    }

    destroy() {
        this.stop();
        if (this.networkSync) {
            this.networkSync.destroy();
            this.networkSync = null;
        }
        this.world = null;
        this.renderer = null;
        this.input = null;
        this.ai = null;
    }

    _onResize() {
        if (this.renderer) this.renderer.resize();
    }

    _onResultClick() {
        if (this.phase === GamePhase.RESULT) {
            this.canvas.removeEventListener('click', this._boundClick);
            const blueHero = this.world.blueHero;
            const redHero = this.world.redHero;
            const playerHero = this.playerTeam === 'blue' ? blueHero : redHero;
            const elapsed = this.world.gameTime || 0;
            const minutes = Math.floor(elapsed / 60);
            const seconds = Math.floor(elapsed % 60);
            this.onGameEnd({
                winner: this.world.winner,
                kills: playerHero ? playerHero.kills : 0,
                deaths: playerHero ? playerHero.deaths : 0,
                gold: playerHero ? playerHero.gold : 0,
                time: `${String(minutes).padStart(2,'0')}:${String(seconds).padStart(2,'0')}`,
                blueKills: this.world.blueKills,
                redKills: this.world.redKills
            });
        }
    }

    // 外部接口：施放技能
    castSkill(slot) {
        if (this.phase !== GamePhase.PLAYING || !this.world) return;
        const hero = this.playerTeam === 'blue' ? this.world.blueHero : this.world.redHero;
        if (!hero || hero.hp <= 0 || !hero.alive) return;
        const keys = ['Q', 'W', 'E', 'R'];
        const key = keys[slot];
        if (!key) return;
        const skill = hero.skills ? hero.skills[slot] : null;
        if (!skill) return;
        // 获取鼠标位置作为目标
        let targetPos = null;
        if (this.input) {
            targetPos = { x: this.input.mouseWorldX, y: this.input.mouseWorldY };
        }
        const skillDef = SKILLS[key];
        if (skillDef && skillDef.type === 'dash') targetPos = null;
        hero.castSkill(key, this.world, targetPos);
        if (this.networkSync) {
            this.networkSync.sendCommand({ kind: 'skill', slot, target: targetPos });
        }
    }

    // 外部接口：调整大小
    resize(w, h) {
        if (this.canvas) {
            if (w !== undefined) {
                this.canvas.width = w;
                this.canvas.height = h;
            }
            if (this.renderer) this.renderer.resize();
        }
    }

    _gameLoop(timestamp) {
        if (!this.running) return;

        const dt = Math.min((timestamp - this.lastTime) / 1000, 0.05);
        this.lastTime = timestamp;

        switch (this.phase) {
            case GamePhase.MATCHING:
                this._updateMatching(dt);
                break;
            case GamePhase.LOADING:
                this._updateLoading(dt);
                break;
            case GamePhase.PLAYING:
                this._updatePlaying(dt);
                break;
            case GamePhase.RESULT:
                this._updateResult(dt);
                break;
        }

        this.animFrame = requestAnimationFrame(this._boundLoop);
    }

    _updateMatching(dt) {
        this.phaseTimer -= dt;
        this._renderMatching();

        if (this.phaseTimer <= 0) {
            this.phase = GamePhase.LOADING;
            this.loadingTimer = 1.5;
            this.world.init();
        }
    }

    _updateLoading(dt) {
        this.loadingTimer -= dt;
        this._renderLoading();

        if (this.loadingTimer <= 0) {
            this.phase = GamePhase.PLAYING;
            this.input.bind(this.playerTeam);
            if (!this.isNetworkMode) {
                this.ai = new GameAI(this.world, this.playerTeam === 'blue' ? 'red' : 'blue');
            }
            // In network mode, send ready signal
            if (this.networkSync) {
                this.networkSync.sendReady();
            }
        }
    }

    _updatePlaying(dt) {
        // 固定步长
        const fixedDt = 1 / 60;

        this.world.update(fixedDt);

        if (this.isNetworkMode) {
            // Network mode: sync hero state
            this._syncNetworkHero(fixedDt);
        } else {
            this.ai.update(fixedDt);
        }

        this.renderer.render(this.world, this.playerTeam);
        this.renderer.drawHUD(this.renderer.ctx, this.world, this.playerTeam, this.input);

        // 更新 HTML HUD
        this._updateHtmlHud();

        if (this.world.gameOver) {
            this.phase = GamePhase.RESULT;
            this.input.unbind();
            if (this.networkSync) this.networkSync.destroy();
            this.canvas.addEventListener('click', this._boundClick);
            // 自动触发结算
            setTimeout(() => this._onResultClick(), 500);
        }
    }

    // 网络同步：控制对手英雄
    _syncNetworkHero(dt) {
        if (!this.networkSync) return;

        // 获取对手英雄
        const opponentTeam = this.playerTeam === 'blue' ? 'red' : 'blue';
        const opponentHero = this.world.getHero(opponentTeam);

        // 应用远程英雄状态
        if (this.remoteHeroState && opponentHero) {
            const rs = this.remoteHeroState;
            // 平滑插值位置
            opponentHero.x = lerp(opponentHero.x, rs.x, 0.3);
            opponentHero.y = lerp(opponentHero.y, rs.y, 0.3);
            opponentHero.hp = rs.hp;
            if (rs.mhp) opponentHero.maxHp = rs.mhp;
            opponentHero.state = rs.st || 'idle';
            opponentHero.facing = rs.f || opponentHero.facing;
            if (rs.tx !== undefined) opponentHero.targetX = rs.tx;
            if (rs.ty !== undefined) opponentHero.targetY = rs.ty;
            if (rs.lv) opponentHero.level = rs.lv;
            if (rs.dmg) opponentHero.attack = rs.dmg;
            if (rs.k !== undefined) opponentHero.kills = rs.k;
            // 处理攻击目标
            if (rs.atk && rs.atk > 0) {
                const target = this.world.entities.find(e => e.id === rs.atk);
                if (target) opponentHero.attackTarget = target;
            } else {
                opponentHero.attackTarget = null;
            }
        }

        // 发送本地英雄状态
        const localHero = this.world.getHero(this.playerTeam);
        if (localHero) {
            this.networkSync.sendHeroState(localHero);
        }
    }

    // 接收远程数据
    _onRemoteData(type, data) {
        if (type === 'heroState') {
            this.remoteHeroState = data;
        } else if (type === 'command') {
            this._applyRemoteCommand(data);
        } else if (type === 'event') {
            // Handle game events
        }
    }

    _applyRemoteCommand(data) {
        if (!data || !this.world) return;
        const opponentTeam = this.playerTeam === 'blue' ? 'red' : 'blue';
        const hero = this.world.getHero(opponentTeam);
        if (!hero || !hero.alive) return;

        if (data.kind === 'move') {
            hero.targetX = clamp(Number(data.x) || hero.x, 1, MAP_SIZE - 1);
            hero.targetY = clamp(Number(data.y) || hero.y, 1, MAP_SIZE - 1);
            hero.attackTarget = null;
            hero.state = 'moving';
        } else if (data.kind === 'skill') {
            const key = SKILL_KEYS[data.slot];
            if (key) hero.castSkill(key, this.world, data.target || null);
        }
    }

    _updateHtmlHud() {
        const hero = this.playerTeam === 'blue' ? this.world.blueHero : this.world.redHero;
        if (!hero) return;

        // HP
        const hpPct = Math.max(0, hero.hp / hero.maxHp * 100);
        const hpFill = document.getElementById('mobaHpFill');
        const hpText = document.getElementById('mobaHpText');
        if (hpFill) hpFill.style.width = hpPct + '%';
        if (hpText) hpText.textContent = Math.ceil(hero.hp) + '/' + hero.maxHp;

        // MP (用技能冷却的"能量"代替)
        const mpFill = document.getElementById('mobaMpFill');
        const mpText = document.getElementById('mobaMpText');
        if (mpFill) mpFill.style.width = '100%';
        if (mpText) mpText.textContent = '100/100';

        // 等级和攻击力
        const lvEl = document.getElementById('mobaHeroLevel');
        if (lvEl) lvEl.textContent = 'Lv.' + hero.level + ' ATK:' + hero.attack;

        // 计时器
        const elapsed = this.world.gameTime || 0;
        const mins = Math.floor(elapsed / 60);
        const secs = Math.floor(elapsed % 60);
        const timerEl = document.getElementById('mobaTimer');
        if (timerEl) timerEl.textContent = String(mins).padStart(2, '0') + ':' + String(secs).padStart(2, '0');

        // 击杀数
        const blueEl = document.getElementById('mobaBlueKills');
        const redEl = document.getElementById('mobaRedKills');
        if (blueEl) blueEl.textContent = this.world.blueKills;
        if (redEl) redEl.textContent = this.world.redKills;

        // 技能冷却
        const keys = ['Q', 'W', 'E', 'R'];
        for (let i = 0; i < 4; i++) {
            const cdEl = document.getElementById('mobaCd' + keys[i]);
            if (cdEl) {
                if (hero.cooldowns[keys[i]] > 0) {
                    cdEl.classList.remove('hidden');
                    cdEl.textContent = Math.ceil(hero.cooldowns[keys[i]]);
                } else {
                    cdEl.classList.add('hidden');
                }
            }
        }

        // 击杀通知
        const feedEl = document.getElementById('mobaKillFeed');
        if (feedEl && this.world.killNotifications) {
            for (const note of this.world.killNotifications) {
                if (!note._shown) {
                    note._shown = true;
                    const div = document.createElement('div');
                    div.className = 'moba-kill-item';
                    div.textContent = note.text;
                    feedEl.appendChild(div);
                    setTimeout(() => { if (div.parentNode) div.parentNode.removeChild(div); }, 3000);
                }
            }
        }
    }

    _updateResult(dt) {
        this.renderer.render(this.world, this.playerTeam);
        this.renderer.drawHUD(this.renderer.ctx, this.world, this.playerTeam, this.input);
    }

    // ---- 阶段渲染 ----

    _renderMatching() {
        const ctx = this.renderer.ctx;
        const w = this.canvas.width;
        const h = this.canvas.height;

        ctx.fillStyle = '#0a0a1a';
        ctx.fillRect(0, 0, w, h);

        // 匹配动画 - 旋转的点
        const cx = w / 2;
        const cy = h / 2;
        const dots = 8;
        const t = (2 - this.phaseTimer) * 2;

        for (let i = 0; i < dots; i++) {
            const angle = (i / dots) * Math.PI * 2 + t;
            const r = 40;
            const x = cx + Math.cos(angle) * r;
            const y = cy + Math.sin(angle) * r;
            const alpha = (i === Math.floor(t * 2) % dots) ? 1 : 0.3;

            ctx.fillStyle = `rgba(68, 136, 255, ${alpha})`;
            ctx.fillRect(x - 4, y - 4, 8, 8);
        }

        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 20px monospace';
        ctx.textAlign = 'center';
        ctx.fillText('匹配中...', cx, cy + 80);

        // VS 预览
        ctx.fillStyle = '#4488ff';
        ctx.font = 'bold 16px monospace';
        ctx.fillText('勇者', cx - 80, cy - 80);
        ctx.fillStyle = '#ff4444';
        ctx.fillText('魔王', cx + 80, cy - 80);
        ctx.fillStyle = '#ffcc00';
        ctx.font = 'bold 24px monospace';
        ctx.fillText('VS', cx, cy - 78);
    }

    _renderLoading() {
        const ctx = this.renderer.ctx;
        const w = this.canvas.width;
        const h = this.canvas.height;

        ctx.fillStyle = '#0a0a1a';
        ctx.fillRect(0, 0, w, h);

        const cx = w / 2;
        const cy = h / 2;

        // 蓝方英雄信息
        ctx.fillStyle = '#4488ff';
        ctx.font = 'bold 24px monospace';
        ctx.textAlign = 'center';
        ctx.fillText('勇者', cx - 100, cy - 40);

        // 蓝方像素英雄预览
        this._drawLoadingHero(ctx, cx - 100, cy + 20, 'blue');

        // VS
        ctx.fillStyle = '#ffcc00';
        ctx.font = 'bold 32px monospace';
        ctx.fillText('VS', cx, cy);

        // 红方英雄信息
        ctx.fillStyle = '#ff4444';
        ctx.font = 'bold 24px monospace';
        ctx.fillText('魔王', cx + 100, cy - 40);

        // 红方像素英雄预览
        this._drawLoadingHero(ctx, cx + 100, cy + 20, 'red');

        // 倒计时
        ctx.fillStyle = '#ffffff';
        ctx.font = 'bold 48px monospace';
        ctx.fillText(Math.ceil(this.loadingTimer).toString(), cx, cy + 100);

        // 地图名
        ctx.fillStyle = '#888';
        ctx.font = '14px monospace';
        ctx.fillText('像素峡谷 - 中路对决', cx, cy + 140);
    }

    _drawLoadingHero(ctx, x, y, team) {
        const color = TEAM_COLORS[team];
        const s = 16;

        // 身体
        ctx.fillStyle = color.primary;
        ctx.fillRect(x - s, y - s, s * 2, s * 2);
        ctx.fillStyle = color.dark;
        ctx.fillRect(x - s + 4, y - s + 4, s * 2 - 8, s * 2 - 8);

        // 头
        ctx.fillStyle = '#ffcc88';
        ctx.fillRect(x - 6, y - s - 8, 12, 8);

        // 眼睛
        ctx.fillStyle = '#222';
        ctx.fillRect(x - 3, y - s - 5, 2, 2);
        ctx.fillRect(x + 2, y - s - 5, 2, 2);

        // 剑
        ctx.fillStyle = '#ccc';
        ctx.fillRect(x + s, y - 4, 12, 3);
        ctx.fillStyle = '#888';
        ctx.fillRect(x + s - 2, y - 6, 3, 12);
    }
}

// 静态 ID 计数器
MobaGame._idCounter = 1;

// 注册到全局
window.MobaGame = MobaGame;

// ============================================================
// 网络同步类 - PvP 对战通信
// ============================================================
class NetworkSync {
    constructor(realtimeAPI, playerID, playerName, battleRoomId, isHost, onRemoteInput) {
        this.realtimeAPI = realtimeAPI;
        this.playerID = playerID;
        this.playerName = playerName;
        this.battleRoomId = battleRoomId;
        this.isHost = isHost;
        this.onRemoteInput = onRemoteInput;
        this.lastSendTime = 0;
        this.sendInterval = 100; // 10Hz
        this.connected = true;
    }

    // Send local hero state to opponent
    sendHeroState(hero) {
        if (!this.connected) return;
        const now = Date.now();
        if (now - this.lastSendTime < this.sendInterval) return;
        this.lastSendTime = now;

        const state = {
            t: 'hs',
            x: Math.round(hero.x * 100) / 100,
            y: Math.round(hero.y * 100) / 100,
            hp: Math.round(hero.hp),
            mhp: hero.maxHp,
            st: hero.state,
            tx: Math.round(hero.targetX * 100) / 100,
            ty: Math.round(hero.targetY * 100) / 100,
            f: Math.round(hero.facing * 100) / 100,
            lv: hero.level || 1,
            atk: hero.attackTarget ? hero.attackTarget.id : 0,
            dmg: hero.attack,
            k: hero.kills
        };
        this._send(JSON.stringify(state));
    }

    // Send input command to opponent
    sendCommand(cmd) {
        if (!this.connected) return;
        this._send(JSON.stringify({t: 'cmd', ...cmd}));
    }

    // Send game event
    sendEvent(eventType, data) {
        if (!this.connected) return;
        this._send(JSON.stringify({t: 'ev', e: eventType, d: data}));
    }

    // Send ready signal
    sendReady() {
        this._send(JSON.stringify({t: 'ready', pid: this.playerID}));
    }

    _send(content) {
        if (!this.realtimeAPI || !this.realtimeAPI.sendInput) return;
        this.realtimeAPI.sendInput(this.battleRoomId, { senderId: this.playerID, content });
    }

    // Handle incoming message from opponent
    handleMessage(content) {
        try {
            const data = JSON.parse(content);
            if (data.t === 'hs' && this.onRemoteInput) {
                this.onRemoteInput('heroState', data);
            } else if (data.t === 'cmd' && this.onRemoteInput) {
                this.onRemoteInput('command', data);
            } else if (data.t === 'ev' && this.onRemoteInput) {
                this.onRemoteInput('event', data);
            } else if (data.t === 'ready' && this.onRemoteInput) {
                this.onRemoteInput('ready', data);
            }
        } catch(e) {}
    }

    destroy() {
        this.connected = false;
        if (this.battleRoomId && this.realtimeAPI && this.realtimeAPI.leaveRoom) {
            this.realtimeAPI.leaveRoom(this.battleRoomId, this.playerID).catch(() => {});
        }
    }
}
