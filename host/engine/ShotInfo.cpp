#include "ShotInfo.h"

#include <cmath>

void ShotInfo::Clear() { for (S& s : m_s) s = S(); }

int ShotInfo::Add(int type, const Params& p, const float o[3], const float t[3], float nowMs) {
    int slot = -1;
    for (int i = 0; i < kSlots; i++) if (!m_s[i].exists) { slot = i; break; }
    if (slot < 0) return -1;
    S& s = m_s[slot];
    s = S();
    s.exists = true;
    s.type = type;
    s.range = p.range;
    s.damage = p.damage;
    s.slows = p.slowsDown;
    s.grows = p.expands;
    for (int i = 0; i < 3; i++) { s.pos[i] = o[i]; s.off[i] = t[i] - o[i]; }
    auto rt = [&]() { return -0.05f + 0.005f * (float)(Rand() % 20); };   // CWeapon::ms_afRandTable
    if (p.spread != 0.0f) {
        s.off[0] += rt() * p.spread;
        s.off[1] += rt() * p.spread;
        s.off[2] += rt();
    }
    float l = std::sqrt(s.off[0] * s.off[0] + s.off[1] * s.off[1] + s.off[2] * s.off[2]);
    if (l < 1e-6f) { s.off[0] = 1.0f; l = 1.0f; }
    float sp = p.speed;
    if (p.randSpeed) sp += rt();
    for (int i = 0; i < 3; i++) s.off[i] = s.off[i] / l * sp;
    s.destroyMs = nowMs + p.lifeMs;
    return slot;
}

void ShotInfo::Update(float nowMs, float step, unsigned frame, const Env& env) {
    for (int i = 0; i < kSlots; i++) {
        S& s = m_s[i];
        if (!s.exists) continue;
        if (nowMs > s.destroyMs) { s.exists = false; continue; }
        if (s.slows) { const float k = std::pow(0.96f, step); for (float& v : s.off) v *= k; }
        if (s.grows) s.range += step * 0.0075f;
        for (int k = 0; k < 3; k++) s.pos[k] += s.off[k] * step;
        if (!s.executed && env.pedsNear) {
            const float r = s.range > 1.0f ? s.range : 1.0f;
            const bool hit = env.pedsNear(s.type, s.pos, r, s.damage);
            if (hit && s.type != Flame) s.executed = true;   // paint and foam hurt once
        }
        if (s.type == Spray && !s.executed && env.sprayHit && env.sprayHit(s.pos, s.off)) {
            s.executed = true;
            for (float& v : s.off) v = -v * 0.5f;   // the stream bounces off what it painted
        }
        if (s.type == Extinguisher && !s.executed && env.extinguish && env.extinguish(s.pos, s.range)) s.executed = true;
        if (s.type == Flame && env.carsAndWorldOnFire && ((frame + (unsigned)i) & 3u) == 0) env.carsAndWorldOnFire(s.pos);
    }
}

int ShotInfo::Active() const { int n = 0; for (const S& s : m_s) n += s.exists; return n; }
bool ShotInfo::Position(int i, float o[3]) const {
    if (i < 0 || i >= kSlots || !m_s[i].exists) return false;
    for (int k = 0; k < 3; k++) o[k] = m_s[i].pos[k];
    return true;
}
int ShotInfo::TypeOf(int i) const { return (i < 0 || i >= kSlots) ? -1 : m_s[i].type; }
bool ShotInfo::FlamePosition(int i, float o[3]) const { return Position(i, o) && m_s[i].type == Flame; }
float ShotInfo::RangeOf(int i) const { return (i < 0 || i >= kSlots) ? 0.0f : m_s[i].range; }
