// The --aimtest shooting range, see AimTest.h.

#include "AimTest.h"

#include <algorithm>
#include <cmath>

void AimTest::Update(float dt) {
    m_time += dt;
    for (AimDummy& d : m_dummies) {
        if (d.respawn > 0.0f) {
            d.respawn -= dt;
            if (d.respawn <= 0.0f) d.health = d.maxHealth;
            continue;
        }
        if (d.strafe > 0.0f) {
            const float s = std::sin(m_time * 0.6f + d.phase) * d.strafe;
            d.x = d.baseX + m_rx * s;
            d.y = d.baseY + m_ry * s;
        }
    }
    for (AimFloat& f : m_floats) f.age += dt;
    m_floats.erase(std::remove_if(m_floats.begin(), m_floats.end(), [](const AimFloat& f) { return f.age > 0.9f; }),
                   m_floats.end());
}

int AimTest::Trace(const float a[3], const float dir[3], float maxLen, float* tHit, bool* head) const {
    int best = -1;
    float bestT = maxLen;
    for (size_t i = 0; i < m_dummies.size(); i++) {
        const AimDummy& d = m_dummies[i];
        if (!d.alive()) continue;
        // ray against the upright cylinder: solve in the horizontal plane, then check the height
        const float ox = a[0] - d.x, oy = a[1] - d.y;
        const float A = dir[0] * dir[0] + dir[1] * dir[1];
        if (A < 1e-8f) continue;
        const float B = 2.0f * (ox * dir[0] + oy * dir[1]);
        const float C = ox * ox + oy * oy - kRadius * kRadius;
        const float disc = B * B - 4.0f * A * C;
        if (disc < 0.0f) continue;
        const float t = (-B - std::sqrt(disc)) / (2.0f * A);
        if (t < 0.0f || t >= bestT) continue;
        const float z = a[2] + dir[2] * t - d.z;
        if (z < 0.0f || z > kHeight) continue;
        best = (int)i;
        bestT = t;
        if (head) *head = z >= kHeadZ;
    }
    if (best >= 0 && tHit) *tHit = bestT;
    return best;
}

bool AimTest::Damage(int index, float amount, bool head, const float at[3]) {
    if (index < 0 || index >= (int)m_dummies.size()) return false;
    AimDummy& d = m_dummies[(size_t)index];
    if (!d.alive()) return false;
    hits++;
    if (head) { headshots++; amount *= 3.0f; }
    d.health -= amount;
    AimFloat f = {};
    f.x = at[0]; f.y = at[1]; f.z = at[2];
    f.amount = (int)(amount + 0.5f);
    f.head = head;
    m_floats.push_back(f);
    if (d.health <= 0.0f) {
        d.health = 0.0f;
        d.respawn = 2.5f;
        kills++;
        return true;
    }
    return false;
}
