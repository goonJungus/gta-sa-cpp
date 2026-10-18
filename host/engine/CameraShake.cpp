#include "CameraShake.h"

#include <algorithm>
#include <cmath>

namespace {
// Seeded 1D value noise, range -1..1: hashed lattice values blended with a
// smootherstep. chan selects an independent lattice.
uint32_t Hash(uint32_t n) {
    n ^= n >> 16; n *= 0x7feb352du;
    n ^= n >> 15; n *= 0x846ca68bu;
    n ^= n >> 16;
    return n;
}

float Lattice(uint32_t n) {
    return (float)(Hash(n) & 0xffffffu) * 1.192093e-7f - 1.0f;
}

float Noise(uint32_t seed, float x, int chan) {
    const float fl = std::floor(x);
    const uint32_t i = (uint32_t)(int)fl - (uint32_t)chan * 0x61c88647u + seed;
    const float f = x - fl;
    const float a = Lattice(i), b = Lattice(i + 1);
    const float s = f * f * f * ((f * 6.0f - 15.0f) * f + 10.0f);
    return a + (b - a) * s;
}

// Two octaves (the second at 1.93x + 11.31, weighted 0.42), normalised by 1.42.
float Layer(uint32_t seed, float x) {
    return (Noise(seed, x * 1.93f + 11.31f, 1) * 0.42f + Noise(seed, x, 0)) / 1.42f;
}
}  // namespace

void CameraShake::Trigger(float strength, uint32_t nowMs) {
    const float old = m_force - (float)(int32_t)(nowMs - m_start) / 1000.0f;
    if (strength > std::clamp(old, 0.0f, 2.0f)) {
        m_force = strength;
        m_start = nowMs;
    }
}

void CameraShake::TriggerAt(float strength, float distance, uint32_t nowMs) {
    const float percent = 1.0f - std::clamp(distance, 0.0f, 100.0f) / 100.0f;
    const float cur = (m_force - (float)(int32_t)(nowMs - m_start) / 1000.0f) * percent;
    const float to = percent * strength * 0.35f;
    if (to > std::clamp(cur, 0.0f, 2.0f)) {
        m_force = to;
        m_start = nowMs;
    }
}

void CameraShake::Update(uint32_t nowMs, float timestep, const float right[3], const float forward[3],
                         const float up[3], float offset[3]) {
    offset[0] = offset[1] = offset[2] = 0.0f;
    if (!m_active || nowMs < m_lastMs) m_level = 0.0f;
    m_active = true;

    const float dt = std::clamp(timestep * 0.02f, 0.0f, 0.1f);
    const float target = std::clamp(m_force - (float)(int32_t)(nowMs - m_start) * 0.001f * 0.28f, 0.0f, 2.0f);
    const float k = (m_level <= target && target != m_level) ? 28.0f : 16.0f;
    m_level = (1.0f - std::exp(-k * dt)) * (target - m_level) + m_level;
    if (target <= 0.0f && m_level < 1e-4f) m_level = 0.0f;
    m_lastMs = nowMs;
    if (m_level <= 0.0f) return;

    const float amp = m_level * params.intensity * params.amplitude;
    const float t = (float)nowMs * 0.001f * params.speed;
    const float r1 = Layer(0xad90777du, t + 43.71f) * 0.24f;   // forward
    const float r2 = Layer(0xc8013ea4u, t + 19.17f) * 0.66f;   // up
    const float r3 = Layer(0xa341316cu, t) * 0.78f;             // right
    for (int i = 0; i < 3; i++) offset[i] = (r3 * right[i] + r2 * up[i] + r1 * forward[i]) * amp;
}
