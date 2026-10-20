#pragma once
// CShotInfo: the area-effect shots of the flamethrower, spray can and extinguisher. A hundred slots;
// each shot flies along its offset for the weapon's lifespan and acts on what it touches every step.
#include <functional>

class ShotInfo {
public:
    static constexpr int kSlots = 100;
    enum Type { Flame = 0, Spray = 1, Extinguisher = 2 };

    struct Params {              // the standard weapon.dat row
        float speed = 0, range = 0, lifeMs = 0, spread = 0, damage = 0;
        bool slowsDown = false, randSpeed = false, expands = false;
    };
    struct Env {
        // Creator's peds within sqrt(range2) of the shot origin; true when one was affected.
        std::function<bool(int type, const float* origin, float range2, float damage)> pedsNear;
        std::function<void(const float* origin)> carsAndWorldOnFire;       // flame: every 4th (frame + slot)
        std::function<bool(const float* origin, float range)> extinguish;   // true when a fire went out
        std::function<bool(const float* origin, const float* offset)> sprayHit;   // spray: world surface reached
    };

    void Clear();
    // CShotInfo::AddShot. Returns the slot or -1 when all are in use.
    int Add(int type, const Params& p, const float origin[3], const float target[3], float nowMs);
    // CShotInfo::Update; step = CTimer::ms_fTimeStep (1 = 20 ms).
    void Update(float nowMs, float step, unsigned frame, const Env& env);
    int Active() const;
    bool Position(int slot, float out[3]) const;
    int TypeOf(int slot) const;
    // CShotInfo::GetFlameThrowerShotPosn: origin of a live flame shot.
    bool FlamePosition(int slot, float out[3]) const;
    float RangeOf(int slot) const;

private:
    struct S {
        bool exists = false, executed = false, slows = false, grows = false;
        int type = 0;
        float pos[3] = {0, 0, 0}, off[3] = {0, 0, 0};
        float range = 0, damage = 0, destroyMs = 0;
    };
    S m_s[kSlots];
    unsigned m_rng = 777;
    int Rand() { m_rng = m_rng * 1664525u + 1013904223u; return (int)((m_rng >> 8) & 0xFFFF); }
};
