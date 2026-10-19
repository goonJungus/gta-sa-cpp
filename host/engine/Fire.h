#pragma once
// Fire: port of CFire / CFireManager / CCreepingFire.
//
// A pool of 60 fires. A fire burns on the ground or follows a ped or vehicle. Its strength grows
// +step / 500 up to 3 (flame size: fire, fire_med above 1, fire_large above 2); when its burn time runs
// out the strength drops by 1 or it goes out. A fire with generations left spreads 2-3 m away (1 time in
// 128 per update, never above 25 fires), merges with a weak neighbour within 3.5 m, sets the player,
// vehicles (2 m, 1 in 32) and objects (3 m, 1 in 4) alight. Time is in 20 ms steps like CTimer.

#include <cstdint>
#include <functional>
#include <vector>

class FireManager {
public:
    enum class Target { Ground, Ped, Vehicle, Player };
    struct Fire {
        bool active = false;
        Target target = Target::Ground;
        int index = -1;                  // ped / vehicle index
        float pos[3] = {0, 0, 0};
        float strength = 0.8f;
        float burnLeft = 0.0f;           // ms
        int generations = 0;
        float lightPhase = 0.0f;
        bool script = false;
    };
    // Everything a fire needs from the world.
    struct World {
        std::function<bool(const float* from, float* out)> groundZ;                // vertical line hit under pos (x, y, z+ )
        std::function<bool(const float* pos)> underwater;
        std::function<bool(int ped, float* out)> pedPos;                           // false when gone / dead handled by caller
        std::function<bool(int ped)> pedAlive;
        std::function<void(int ped, bool on)> pedBurning;
        std::function<bool(int ped, float dt)> pedBurnDamage;                      // true if it died
        std::function<bool(int car, float* out)> carPos;
        std::function<bool(int car)> carWrecked;
        std::function<bool(int car)> carOnFire;
        std::function<void(int car, float amount)> carDamage;                      // InflictDamage(FLAMETHROWER, step * 1.2)
        std::function<void(int car)> carBurning;
        std::function<bool(float* out)> player;                                    // CJ position (feet), false if in a vehicle
        std::function<void(float dt)> playerBurn;
        std::function<bool()> playerBurning;
        std::function<void(const float* pos, float step8)> objectFire;             // ObjectFireDamage
        std::function<void(const float* pos, float strength, float phase)> emit;   // flames / light
        std::function<void(const float* pos, float r, float g)> light;
        size_t carCount = 0;
        std::function<int()> pedCount;
    };

    // CFireManager::StartFire(pos, size, propagation, ..., nTimeToBurn, nGenerations). Returns the fire or -1.
    int StartFire(const float pos[3], float size, bool propagation, float burnMs, int generations, bool script = false);
    // CFireManager::StartFire(entity, ...).
    int StartFireOn(Target t, int index, float size, float burnMs, int generations);
    int Num() const;
    bool IsBurning(Target t, int index) const { for (const Fire& f : m_fires) if (f.active && f.target == t && f.index == index) return true; return false; }
    int NumInRange(const float pos[3], float r) const;
    bool PlentyAvailable() const;
    void Extinguish(int fire);
    void ExtinguishPoint(const float pos[3], float radius);
    void ExtinguishAll();
    void Update(float dt, World& w);
    // CCreepingFire::TryToStartFireAtCoors: needs ground below and plenty of free fires.
    bool TryCreeping(const float pos[3], int generations, bool script, float burnMs, World& w);
    const std::vector<Fire>& Fires() const { return m_fires; }
    // Combined flame strength for the camera-flash style glow.
    unsigned Rand() { m_rng = m_rng * 1664525u + 1013904223u; return m_rng >> 8; }
    float RandF(float a, float b) { return a + (b - a) * (float)(Rand() % 10000) / 9999.0f; }

private:
    std::vector<Fire> m_fires = std::vector<Fire>(60);
    unsigned m_rng = 4242;
    float m_clock = 0.0f;   // ms
    unsigned char m_status[32][32] = {};   // CCreepingFire::m_aFireStatus
    unsigned m_frame = 0;
    float m_frameAcc = 0.0f;
};
