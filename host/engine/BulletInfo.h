#pragma once
// CBulletInfo: the travelling bullets of the scoped sniper rifle (CWeapon::FireSniper). Eight slots,
// each flies at velocity * timestep / 2 per step for one second and tests its segment every step.
#include <functional>

class BulletInfo {
public:
    static constexpr int kSlots = 8;
    static constexpr float kLifeMs = 1000.0f;
    // Test the segment a -> b; returns true when something was hit and writes the new position (the hit
    // point for a living ped, otherwise b).
    using Trace = std::function<bool(const float* a, const float* b, int damage, float* newPos)>;

    void Clear();
    // CBulletInfo::AddBullet: false when all eight slots are in use.
    bool Add(const float pos[3], const float vel[3], int damage, float nowMs);
    // CBulletInfo::Update; step = CTimer::ms_fTimeStep (1 = 20 ms).
    void Update(float nowMs, float step, const Trace& trace);
    int Active() const;
    bool Position(int slot, float out[3]) const;

private:
    struct B { bool exists = false; float pos[3] = {0, 0, 0}, vel[3] = {0, 0, 0}; int damage = 0; float destroyMs = 0; };
    B m_b[kSlots];
};
