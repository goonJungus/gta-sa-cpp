#define NOMINMAX
#include "Projectiles.h"

#include "PlayerPed.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr int kGrenade = 16, kTearGas = 17, kMolotov = 18, kSatchel = 39;
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
        const float c = std::cos(p.spin), s = std::sin(p.spin);
        const float w[4][4] = {{1, 0, 0, 0}, {0, c, s, 0}, {0, -s, c, 0}, {p.pos[0], p.pos[1], p.pos[2], 1}};
        ped.DrawGunModel(r, p.gun, w);
    }
}
