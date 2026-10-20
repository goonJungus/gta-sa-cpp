#include "Fire.h"

#include <algorithm>
#include <cmath>

int FireManager::Num() const {
    int n = 0;
    for (const Fire& f : m_fires) n += f.active ? 1 : 0;
    return n;
}

bool FireManager::PlentyAvailable() const {   // CFireManager::PlentyFiresAvailable: at least 6 inactive
    int free = 0;
    for (const Fire& f : m_fires) free += f.active ? 0 : 1;
    return free >= 6;
}

int FireManager::NumInRange(const float p[3], float r) const {
    int n = 0;
    for (const Fire& f : m_fires) {
        if (!f.active) continue;
        const float dx = f.pos[0] - p[0], dy = f.pos[1] - p[1], dz = f.pos[2] - p[2];
        if (dx * dx + dy * dy + dz * dz < r * r) n++;
    }
    return n;
}

void FireManager::Extinguish(int i) {
    if (i < 0 || (size_t)i >= m_fires.size()) return;
    m_fires[(size_t)i].active = false;
}

bool FireManager::ExtinguishPointWithWater(const float p[3], float radius, float strength, float dt, World* w,
                                           const std::function<void(const float*)>& onHit) {
    bool any = false;
    for (Fire& f : m_fires) {
        if (!f.active) continue;
        const float dx = f.pos[0] - p[0], dy = f.pos[1] - p[1], dz = f.pos[2] - p[2];
        if (dx * dx + dy * dy + dz * dz > radius * radius) continue;
        any = true;
        f.strength -= strength * dt;
        if (onHit) {
            const float sp[3] = {f.pos[0] + RandF(-1.28f, 1.28f), f.pos[1] + RandF(-1.28f, 1.28f), f.pos[2] + RandF(-0.64f, 0.64f)};
            onHit(sp);
        }
        if (f.strength < 0.0f) {   // CFire::Extinguish
            f.active = false;
            if (f.target == Target::Ped && w && w->pedBurning) w->pedBurning(f.index, false);
        }
    }
    return any;
}

void FireManager::ExtinguishAll() { for (Fire& f : m_fires) f.active = false; }

void FireManager::ExtinguishPoint(const float p[3], float radius) {
    for (Fire& f : m_fires) {
        if (!f.active || f.script) continue;
        const float dx = f.pos[0] - p[0], dy = f.pos[1] - p[1], dz = f.pos[2] - p[2];
        if (dx * dx + dy * dy + dz * dz < radius * radius) f.active = false;
    }
}

int FireManager::StartFire(const float pos[3], float size, bool, float burnMs, int generations, bool script) {
    for (size_t i = 0; i < m_fires.size(); i++) {
        Fire& f = m_fires[i];
        if (f.active) continue;
        f = Fire();
        f.active = true; f.target = Target::Ground;
        f.pos[0] = pos[0]; f.pos[1] = pos[1]; f.pos[2] = pos[2];
        f.strength = 1.0f; f.burnLeft = script ? burnMs : burnMs * RandF(1.0f, 1.3f); f.generations = generations; f.script = script;   // CFire::Start: strength 1, 1.0-1.3 x the time
        return (int)i;
    }
    return -1;
}

int FireManager::StartFireOn(Target t, int index, float size, float burnMs, int generations) {
    for (const Fire& f : m_fires) if (f.active && f.target == t && f.index == index) return -1;   // already burning
    for (size_t i = 0; i < m_fires.size(); i++) {
        Fire& f = m_fires[i];
        if (f.active) continue;
        f = Fire();
        f.active = true; f.target = t; f.index = index;
        f.strength = 1.0f; f.generations = generations;
        f.burnLeft = t == Target::Player ? 2333.0f : t == Target::Vehicle ? 3000.0f + RandF(0, 1000) : burnMs + RandF(0, 1000);
        return (int)i;
    }
    return -1;
}

bool FireManager::TryCreeping(const float pos[3], int generations, bool script, float burnMs, World& w) {
    unsigned char& status = m_status[(int)std::floor(pos[0]) & 31][(int)std::floor(pos[1]) & 31];
    if (status != 0) return false;   // a cell that burned recently stays blocked for a while
    if (!PlentyAvailable()) return false;
    float z[3] = {pos[0], pos[1], pos[2]};
    if (w.groundZ) { const float up[3] = {pos[0], pos[1], pos[2] + 3.0f}; float g[3]; if (!w.groundZ(up, g)) return false; z[2] = g[2]; }
    if (w.underwater && w.underwater(z)) return false;
    status = 6;
    const int id = StartFire(z, 0.8f, true, 20000.0f, generations, script);
    return id >= 0;
}

void FireManager::Update(float dt, World& w) {
    const float step = dt * 50.0f;   // CTimer::GetTimeStep: 1 = 20 ms
    m_clock += dt * 1000.0f;
    m_frameAcc += step;
    while (m_frameAcc >= 1.0f) {   // CCreepingFire::Update: one cell per frame, 6 -> 5 -> 4 -> 0
        m_frameAcc -= 1.0f;
        unsigned char& s = m_status[m_frame % 32][(m_frame / 32) % 32];
        if (s == 4) s = 0; else if (s > 4 && s <= 6) s -= 1;
        m_frame++;
    }
    auto chance = [&](unsigned n) { return (float)(Rand() % 100000) / 100000.0f < dt * 30.0f / (float)n; };   // 1 in n per retail frame (30 fps)
    for (size_t i = 0; i < m_fires.size(); i++) {
        Fire& f = m_fires[i];
        if (!f.active) continue;

        const float ns = std::min(3.0f, f.strength + step / 500.0f);   // retail only commits it when the whole part changes
        if ((unsigned)ns != (unsigned)f.strength) f.strength = ns;

        float p[3];
        if (f.target == Target::Ped) {
            if (!w.pedPos || !w.pedAlive || !w.pedPos(f.index, p)) { f.active = false; continue; }
            f.pos[0] = p[0]; f.pos[1] = p[1]; f.pos[2] = p[2];
            if (!w.pedAlive(f.index)) f.pos[2] -= 1.0f;   // the corpse lies on the ground
            if (w.pedBurnDamage && w.pedAlive(f.index)) w.pedBurnDamage(f.index, dt);
        } else if (f.target == Target::Player) {
            float pp[3];
            if (!w.player || !w.player(pp)) { f.active = false; continue; }
            f.pos[0] = pp[0]; f.pos[1] = pp[1]; f.pos[2] = pp[2];
            if (w.playerBurn) w.playerBurn(dt);
        } else if (f.target == Target::Vehicle) {
            if (!w.carPos || !w.carPos(f.index, p) || (w.carWrecked && w.carWrecked(f.index))) { f.active = false; continue; }
            f.pos[0] = p[0]; f.pos[1] = p[1]; f.pos[2] = p[2] + 0.15f;
            if (!f.script && w.carDamage) w.carDamage(f.index, step * 1.2f);
        }
        if ((f.target == Target::Ped || f.target == Target::Player) && w.underwater && w.underwater(f.pos)) { f.burnLeft = 0; f.strength = 0; }   // submerged: put out

        // The player stands in a ground fire: he catches alight.
        float pl[3];
        if (f.target != Target::Vehicle && w.player && w.player(pl) && w.playerBurning && !w.playerBurning()) {
            const float dx = pl[0] - f.pos[0], dy = pl[1] + 1.0f - f.pos[1], dz = pl[2] + 1.0f - f.pos[2];
            if (f.target != Target::Player && dx * dx + dy * dy + dz * dz < 1.2f) StartFireOn(Target::Player, 0, 0.8f, 7000.0f, 100);
        }

        if (chance(32)) {   // vehicles close by catch fire
            for (size_t c = 0; c < w.carCount; c++) {
                float cp[3];
                if (!w.carPos || !w.carPos((int)c, cp)) continue;
                const float dx = cp[0] - f.pos[0], dy = cp[1] - f.pos[1], dz = cp[2] - f.pos[2];
                if (dx * dx + dy * dy + dz * dz >= 4.0f) continue;
                if (w.carWrecked && w.carWrecked((int)c)) continue;
                if (StartFireOn(Target::Vehicle, (int)c, 0.8f, 7000.0f, 100) >= 0 && w.carBurning) w.carBurning((int)c);
            }
        }
        if (chance(4) && w.objectFire) w.objectFire(f.pos, step * 8.0f);

        if (f.generations > 0 && chance(128) && Num() < 25) {   // creeping
            const float dir[2] = {RandF(-1, 1), RandF(-1, 1)};
            const float len = RandF(2.0f, 3.0f);
            const float np[3] = {f.pos[0] + dir[0] * len, f.pos[1] + dir[1] * len, f.pos[2]};
            TryCreeping(np, f.generations - 1, f.script, 10000.0f, w);
        }
        if (f.strength <= 2.0f && f.generations && chance(16)) {   // merge a weak neighbour
            Fire& n = m_fires[Rand() % m_fires.size()];
            if (&n != &f && n.active && !n.script && n.strength <= 1.0f) {
                const float dx = n.pos[0] - f.pos[0], dy = n.pos[1] - f.pos[1], dz = n.pos[2] - f.pos[2];
                if (std::sqrt(dx * dx + dy * dy + dz * dz) < 3.5f) {
                    f.strength += 1.0f;
                    f.burnLeft = std::max(f.burnLeft, 7000.0f);
                    f.generations = std::max(f.generations, n.generations);
                    n.active = false;
                }
            }
        }

        if (f.script || f.burnLeft > 0.0f) {
            if (!f.script) f.burnLeft -= dt * 1000.0f;   // (an entity fire keeps burning its owner while this runs)
            if (w.light) w.light(f.pos, (float)(Rand() % 128) / 512.0f, (float)(Rand() % 128) / 512.0f);
            if (w.emit) w.emit(f.pos, f.strength, f.lightPhase);
        } else if (f.strength <= 1.0f) {
            f.active = false;
            if (f.target == Target::Ped && w.pedBurning) w.pedBurning(f.index, false);
        } else {
            f.strength -= 1.0f;
            f.burnLeft = 7000.0f;
        }
    }
}
