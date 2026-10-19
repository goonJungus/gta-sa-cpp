#pragma once
// Explosion: port of CExplosion (AddExplosion / Update) with the per-type radius, visible distance,
// expiry, propagation and fire spawning of the retail table. The damage itself (CWorld::TriggerExplosion)
// is done by the host through ExplHooks::trigger.

#include <functional>
#include <vector>

#include "Fire.h"

enum class ExplType {
    Grenade, Molotov, Rocket, WeakRocket, Car, QuickCar, Boat, Aircraft, Mine, Object, TankFire, Small, RcVehicle
};

struct ExplHooks {
    // CWorld::TriggerExplosion(pos, radius, visibleDistance, ..., damage percentage)
    std::function<void(const float* pos, float radius, float visible, float damage)> trigger;
    std::function<bool(const float* pos, float* groundOut)> groundZ;     // FindGroundZFor3DCoord(pos + 3)
    std::function<bool(const float* pos)> underwater;
    std::function<void(const float* pos, ExplType type)> fx;             // the visible blast
    std::function<void(const float* pos, float radius)> setPedsOnFire, setWorldOnFire, setCarsOnFire;
    std::function<void(const float* pos, float shake)> shake;            // TheCamera.CamShake
    std::function<void(const float* pos, float size)> fuel;              // the car blast fuel puffs
    FireManager* fire = nullptr;
    FireManager::World* fireWorld = nullptr;
};

class ExplosionManager {
public:
    // CExplosion::AddExplosion. lifetimeMs 0 = immediate damage; invisible = no radius. shake -1 = default.
    void Add(ExplType type, const float pos[3], ExplHooks& h, float lifetimeMs = 0.0f, bool invisible = false, float shake = -1.0f);
    void Update(float dt, ExplHooks& h);
    int Active() const { int n = 0; for (const E& e : m_e) n += e.active ? 1 : 0; return n; }

private:
    struct E {
        bool active = false;
        ExplType type = ExplType::Grenade;
        float pos[3] = {0, 0, 0};
        float radius = 1.0f, visible = 0.0f, damage = 1.0f, propagation = 0.0f;
        float expireMs = 0.0f, ageMs = 0.0f, fuelTimer = 0.0f;
        float particlesMs = 0.0f;
        bool creeping = false;
    };
    std::vector<E> m_e = std::vector<E>(48);
    unsigned m_rng = 77;
    unsigned Rand() { m_rng = m_rng * 1664525u + 1013904223u; return m_rng >> 8; }
    float RandF(float a, float b) { return a + (b - a) * (float)(Rand() % 10000) / 9999.0f; }
};
