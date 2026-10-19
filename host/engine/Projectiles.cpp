#define NOMINMAX
#include "Projectiles.h"

#include "PlayerPed.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kGrenade = 16, kTearGas = 17, kMolotov = 18, kSatchel = 39, kRocket = 19, kRocketHS = 20;
constexpr float kGravity = 20.0f;        // m/s^2 (0.008 per timestep at 50 steps/s squared)
constexpr float kRadius = 0.1f;          // resting height of the model above the ground
constexpr float kGrenadeFuseMs = 2000.0f;
constexpr float kGasLifeMs = 20000.0f;
constexpr float kGasStartMs = 20000.0f - 17500.0f;   // 0x445c before the end the cloud starts to bite
constexpr float kExplosionRadius = 9.0f;   // grenade and satchel
constexpr float kMolotovRadius = 6.0f;
constexpr float kGasRadius = 6.0f;
}  // namespace

void Projectiles::Add(int gun, int type, const float pos[3], const float vel[3], bool fell) {
    P p;
    p.gun = gun;
    p.type = type;
    for (int i = 0; i < 3; i++) { p.pos[i] = pos[i]; p.vel[i] = vel[i]; }
    p.homing = type == kRocketHS;
    p.elasticity = type == kSatchel ? 0.03f : 0.5f;   // AddProjectile: the object's bounce
    if (type == kTearGas || type == kMolotov) p.elasticity = 0.75f;
    if (fell && type != kGrenade && type != kSatchel) {   // CWeapon::FireProjectile: RemoveNotAdd, the effect at the origin
        P q = p;
        q.ageMs = type == kMolotov ? 0.0f : kGasStartMs;
        q.vel[0] = q.vel[1] = q.vel[2] = 0.0f;
        q.resting = true;
        if (type == kMolotov) q.ageMs = kGrenadeFuseMs;
        m_list.push_back(q);
        return;
    }
    m_list.push_back(p);
}

void Projectiles::Detonate() {
    for (P& p : m_list)
        if (p.type == kSatchel) p.ageMs = -1.0f;   // flagged below
}

void Projectiles::Update(float dt, const Env& env, std::vector<Event>& out) {
    for (size_t i = 0; i < m_list.size();) {
        P& p = m_list[i];
        const bool detonated = p.type == kSatchel && p.ageMs < 0.0f;
        if (p.ageMs >= 0.0f) p.ageMs += dt * 1000.0f;
        bool boom = false, hit = false;

        if (p.type == kRocket || p.type == kRocketHS) {   // CProjectileInfo::Update: no gravity, +0.008 per step^2 along the nose, capped at 9.9 per step
            float sp = std::sqrt(p.vel[0] * p.vel[0] + p.vel[1] * p.vel[1] + p.vel[2] * p.vel[2]);
            if (p.homing && env.seekTarget) {
                float tp[3];
                if (env.seekTarget(p.pos, p.vel, tp) && sp > 1e-3f) {
                    float d[3] = {tp[0] - p.pos[0], tp[1] - p.pos[1], tp[2] - p.pos[2]};
                    const float dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                    if (dl > 1e-3f) {   // turn the nose a little towards the target each step
                        const float k = std::min(1.0f, 3.0f * dt);
                        for (int i = 0; i < 3; i++) p.vel[i] += (d[i] / dl * sp - p.vel[i]) * k;
                        const float nl = std::sqrt(p.vel[0] * p.vel[0] + p.vel[1] * p.vel[1] + p.vel[2] * p.vel[2]);
                        for (int i = 0; i < 3; i++) p.vel[i] *= sp / std::max(1e-4f, nl);
                    }
                }
            }
            if (sp > 1e-3f) {
                const float a = 20.0f * dt;   // m/s^2
                for (int i = 0; i < 3; i++) p.vel[i] += p.vel[i] / sp * a;
                sp += a;
                if (sp > 495.0f) for (int i = 0; i < 3; i++) p.vel[i] *= 495.0f / sp;
            }
            float np[3] = {p.pos[0] + p.vel[0] * dt, p.pos[1] + p.vel[1] * dt, p.pos[2] + p.vel[2] * dt};
            float nz = 0.0f;
            const float t = env.rayCast ? env.rayCast(p.pos, np, &nz) : 2.0f;
            if (t < 1.0f) for (int k = 0; k < 3; k++) np[k] = p.pos[k] + (np[k] - p.pos[k]) * t;
            const bool ent = env.entityHit && env.entityHit(p.pos, np);
            for (int k = 0; k < 3; k++) p.pos[k] = np[k];
            if (t < 1.0f || ent || p.ageMs >= (p.type == kRocket ? 3000.0f : 10000.0f)) {   // RemoveProjectile explodes it
                out.push_back({Kind::Explode, {p.pos[0], p.pos[1], p.pos[2]}, kExplosionRadius, 1});
                m_list.erase(m_list.begin() + (long)i);
                continue;
            }
            out.push_back({Kind::Trail, {p.pos[0], p.pos[1], p.pos[2]}, 0.0f});
            i++;
            continue;
        }
        if (!p.resting) {
            p.vel[2] -= kGravity * dt;
            float np[3] = {p.pos[0] + p.vel[0] * dt, p.pos[1] + p.vel[1] * dt, p.pos[2] + p.vel[2] * dt};
            if (env.rayCast) {   // walls and roofs: reflect off the surface
                float nz = 0.0f;
                const float t = env.rayCast(p.pos, np, &nz);
                if (t < 1.0f) {
                    hit = true;
                    const bool floorLike = nz > 0.7f || nz < -0.7f;
                    if (floorLike) p.vel[2] = -p.vel[2] * p.elasticity;
                    else { p.vel[0] = -p.vel[0] * p.elasticity; p.vel[1] = -p.vel[1] * p.elasticity; }
                    np[0] = p.pos[0] + (np[0] - p.pos[0]) * std::max(0.0f, t - 0.05f);
                    np[1] = p.pos[1] + (np[1] - p.pos[1]) * std::max(0.0f, t - 0.05f);
                    np[2] = p.pos[2] + (np[2] - p.pos[2]) * std::max(0.0f, t - 0.05f);
                }
            }
            if (env.groundAt) {
                const float g = env.groundAt(np[0], np[1], np[2] + 0.5f);
                if (g > -1e5f && np[2] < g + kRadius) {
                    np[2] = g + kRadius;
                    hit = hit || p.vel[2] < -0.5f;
                    p.vel[2] = -p.vel[2] * p.elasticity;
                    p.vel[0] *= 0.7f; p.vel[1] *= 0.7f;
                    if (std::fabs(p.vel[2]) < 1.0f) {
                        p.vel[2] = 0.0f;
                        if (std::hypot(p.vel[0], p.vel[1]) < 0.5f) { p.vel[0] = p.vel[1] = 0.0f; p.resting = true; }
                        if (p.type == kSatchel) p.elasticity = 0.03f;
                    }
                }
            }
            for (int k = 0; k < 3; k++) p.pos[k] = np[k];
            p.spin += dt * 9.0f * (p.resting ? 0.0f : 1.0f);
        }

        switch (p.type) {
        case kGrenade:
            boom = p.ageMs >= kGrenadeFuseMs;
            break;
        case kSatchel:
            boom = detonated;
            break;
        case kMolotov:
            boom = hit || p.ageMs >= kGrenadeFuseMs;   // breaks on the first impact
            break;
        case kTearGas:
            if (p.ageMs >= kGasStartMs && Rand01() < 0.1f) out.push_back({Kind::Gas, {p.pos[0], p.pos[1], p.pos[2]}, kGasRadius});
            if (p.ageMs >= kGasLifeMs) { m_list.erase(m_list.begin() + (long)i); continue; }
            break;
        }

        if (boom) {
            if (p.type == kMolotov) out.push_back({Kind::Fire, {p.pos[0], p.pos[1], p.pos[2]}, kMolotovRadius});
            else out.push_back({Kind::Explode, {p.pos[0], p.pos[1], p.pos[2]}, kExplosionRadius});
            m_list.erase(m_list.begin() + (long)i);
            continue;
        }
        i++;
    }
}

void Projectiles::Draw(D3DRenderer& r, PlayerPed& ped) {
    for (const P& p : m_list) {
        float w[4][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {p.pos[0], p.pos[1], p.pos[2], 1}};
        if (p.type == kRocket || p.type == kRocketHS) {   // nose (model +y) along the velocity
            const float sp = std::sqrt(p.vel[0] * p.vel[0] + p.vel[1] * p.vel[1] + p.vel[2] * p.vel[2]);
            if (sp > 1e-3f) {
                const float f[3] = {p.vel[0] / sp, p.vel[1] / sp, p.vel[2] / sp};
                float rx = f[1], ry = -f[0], rl = std::sqrt(rx * rx + ry * ry);
                if (rl < 1e-4f) { rx = 1.0f; ry = 0.0f; rl = 1.0f; }
                rx /= rl; ry /= rl;
                const float u[3] = {-f[2] * ry, f[2] * rx, f[0] * ry - f[1] * rx};
                w[0][0] = rx; w[0][1] = ry; w[0][2] = 0;
                w[1][0] = f[0]; w[1][1] = f[1]; w[1][2] = f[2];
                w[2][0] = -u[0]; w[2][1] = -u[1]; w[2][2] = -u[2];
            }
        } else {
            const float c = std::cos(p.spin), s = std::sin(p.spin);
            w[1][1] = c; w[1][2] = s; w[2][1] = -s; w[2][2] = c;
        }
        ped.DrawGunModel(r, p.gun, w);
    }
}
