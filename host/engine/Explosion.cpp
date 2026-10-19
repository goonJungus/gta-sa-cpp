#include "Explosion.h"

#include <algorithm>
#include <cmath>

void ExplosionManager::Add(ExplType type, const float pos0[3], ExplHooks& h, float lifetimeMs, bool invisible, float shake) {
    E* e = nullptr;
    for (E& c : m_e) if (!c.active) { e = &c; break; }
    if (!e) return;
    *e = E();
    e->active = true; e->type = type;
    float pos[3] = {pos0[0], pos0[1], pos0[2]};
    e->pos[0] = pos[0]; e->pos[1] = pos[1]; e->pos[2] = pos[2];
    e->particlesMs = lifetimeMs;
    float life = lifetimeMs;
    if (invisible) { e->radius = 0.0f; e->visible = 0.0f; }
    bool noFire = false;
    auto set = [&](float r, float v, float expire, float prop) {
        if (!invisible) { e->radius = r; e->visible = v; }
        e->expireMs = life + expire; e->propagation = prop;
    };
    switch (type) {
    case ExplType::Grenade: set(9, 300, 750, 0.5f); break;
    case ExplType::Molotov: {
        set(6, 0, 3000, 0.0f);
        if (!invisible) e->visible = 0.0f;
        float g[3], p3[3] = {pos[0], pos[1], pos[2] + 3.0f};
        if (h.groundZ && h.groundZ(p3, g)) { pos[2] = g[2]; e->pos[2] = pos[2]; }
        if (h.underwater && h.underwater(pos)) noFire = true;
        break;
    }
    case ExplType::Rocket: set(10, 300, 750, 0.5f); break;
    case ExplType::WeakRocket: set(10, 200, 750, 0.5f); e->damage = 0.2f; break;
    case ExplType::Car: case ExplType::QuickCar: set(9, 300, 4250, 0.5f); break;
    case ExplType::Boat: case ExplType::Aircraft: set(25, 600, 3000, 0.0f); break;
    case ExplType::Mine: set(10, 150, 750, 0.5f); break;
    case ExplType::Object: set(10, 150, 750, 0.5f); break;
    case ExplType::TankFire: set(10, 150, 750, 0.5f); break;
    case ExplType::Small: set(3, 90, 750, 0.5f); break;
    case ExplType::RcVehicle: set(3, 90, 750, 0.5f); break;
    }

    if (!noFire && (type == ExplType::Molotov || type == ExplType::Rocket || type == ExplType::WeakRocket || type == ExplType::Object) && h.fire && h.fireWorld) {
        const int n = type == ExplType::Molotov ? (int)((Rand() + 2) % 4) : (int)((Rand() + 1) % 4);   // 0-3 fires, within +-4 m
        for (int i = 0; i < n; i++) {
            float fp[3] = {pos[0] + RandF(-4, 4), pos[1] + RandF(-4, 4), pos[2]};
            float p3[3] = {fp[0], fp[1], fp[2] + 3.0f}, g[3];
            if (h.groundZ && h.groundZ(p3, g) && std::fabs(g[2] - pos[2]) < 10.0f) {
                fp[2] = g[2];
                h.fire->StartFire(fp, 0.8f, false, RandF(5600.0f, 12600.0f) * 0.4f, 3);
            }
        }
    }
    if (h.fx && !noFire) h.fx(pos, type);
    if (e->visible != 0.0f && e->particlesMs == 0.0f && h.trigger) h.trigger(pos, e->radius, e->visible, e->damage);
    if (h.shake) h.shake(pos, shake == -1.0f ? (type == ExplType::Molotov ? 0.2f : 0.6f) : shake);
}

void ExplosionManager::Update(float dt, ExplHooks& h) {
    const float step = dt * 50.0f;
    for (E& e : m_e) {
        if (!e.active) continue;
        e.ageMs += dt * 1000.0f;
        if (e.type == ExplType::Molotov) {   // CExplosion::Update: the burning splash keeps setting things alight
            if (h.setPedsOnFire) h.setPedsOnFire(e.pos, 6.0f);
            if (h.setWorldOnFire) h.setWorldOnFire(e.pos, 6.0f);
            if (h.setCarsOnFire) h.setCarsOnFire(e.pos, 6.0f);
        }
        if (e.type == ExplType::Car || e.type == ExplType::QuickCar) {
            e.fuelTimer -= dt * 1000.0f;
            if (e.fuelTimer <= 0.0f && e.ageMs < 2000.0f) { e.fuelTimer = 200.0f; if (h.fuel) h.fuel(e.pos, 1.0f); }
            if (!e.creeping && e.ageMs > 1000.0f && h.fire && h.fireWorld) { e.creeping = true; h.fire->TryCreeping(e.pos, 3, false, 20000.0f, *h.fireWorld); }
        }
        if (e.type == ExplType::Aircraft && Rand() % 20 == 0) {   // secondary blasts
            const float p[3] = {e.pos[0] + RandF(-8, 8), e.pos[1] + RandF(-8, 8), e.pos[2] + RandF(0, 4)};
            Add(ExplType::Small, p, h);
        }
        if (e.particlesMs > 0.0f && e.ageMs >= e.particlesMs && e.visible != 0.0f) {   // delayed damage after the particles
            e.particlesMs = 0.0f;
            if (h.trigger) h.trigger(e.pos, e.radius, e.visible, e.damage);
        }
        if (e.ageMs >= e.expireMs) e.active = false;
    }
}
