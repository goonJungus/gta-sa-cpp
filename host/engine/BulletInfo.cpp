#include "BulletInfo.h"

#include <cmath>

void BulletInfo::Clear() { for (auto& b : m_b) b = B(); }

bool BulletInfo::Add(const float pos[3], const float vel[3], int damage, float nowMs) {
    for (auto& b : m_b) {
        if (b.exists) continue;
        for (int i = 0; i < 3; i++) { b.pos[i] = pos[i]; b.vel[i] = vel[i]; }
        b.damage = damage;
        b.destroyMs = nowMs + kLifeMs;
        b.exists = true;
        return true;
    }
    return false;
}

void BulletInfo::Update(float nowMs, float step, const Trace& trace) {
    for (auto& b : m_b) {
        if (nowMs > b.destroyMs) b.exists = false;
        if (!b.exists) continue;
        float np[3];
        for (int i = 0; i < 3; i++) np[i] = b.pos[i] + b.vel[i] * (step * 0.5f);
        if (std::fabs(np[0]) > 3000.0f || std::fabs(np[1]) > 3000.0f) { b.exists = false; continue; }   // CWorld::IsInWorldBounds
        float hit[3];
        if (trace(b.pos, np, b.damage, hit)) for (int i = 0; i < 3; i++) np[i] = hit[i];
        for (int i = 0; i < 3; i++) b.pos[i] = np[i];
    }
}

int BulletInfo::Active() const { int n = 0; for (const auto& b : m_b) n += b.exists ? 1 : 0; return n; }

bool BulletInfo::Position(int slot, float out[3]) const {
    if (slot < 0 || slot >= kSlots || !m_b[slot].exists) return false;
    for (int i = 0; i < 3; i++) out[i] = m_b[slot].pos[i];
    return true;
}
