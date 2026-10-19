#pragma once
// CProjectileInfo for the thrown weapons: grenade, tear gas, molotov and satchel charge in flight.
// PlayerPed hands over a Launch; this flies it under gravity, bounces it, and reports what it did.
#include <functional>
#include <vector>

class D3DRenderer;
class PlayerPed;

class Projectiles {
public:
    enum class Kind { Explode, Fire, Gas };
    struct Event { Kind kind; float pos[3]; float radius; };

    struct Env {
        std::function<float(float, float, float)> groundAt;                 // < -1e5: none
        std::function<float(const float*, const float*, float*)> rayCast;   // fraction of a -> b to the first hit (>= 1: clear), wall normal z in the third
    };

    void Add(int gun, int type, const float pos[3], const float vel[3], bool fell);
    void Detonate();   // CWorld::UseDetonator: every satchel charge goes off
    void Update(float dt, const Env& env, std::vector<Event>& out);
    void Draw(D3DRenderer& r, PlayerPed& ped);
    size_t Count() const { return m_list.size(); }

private:
    struct P {
        int gun = -1, type = 0;
        float pos[3] = {0, 0, 0}, vel[3] = {0, 0, 0};
        float ageMs = 0.0f;
        float spin = 0.0f;
        bool resting = false;
        float elasticity = 0.75f;
    };
    std::vector<P> m_list;
    unsigned m_rng = 12345;
    float Rand01() { m_rng = m_rng * 1664525u + 1013904223u; return (float)((m_rng >> 8) & 0xFFFF) / 65536.0f; }
};
