// peds.hpp - Pedestrian crowd simulation + CJ body stats.
//
// CJStats is the hookable player component (Architectural Rule 2): a plain data
// struct the host reads and writes directly. The ped manager owns ambient
// crowd density/routing; the host renders and animates from the reported states.

#pragma once

#include "gta_sa/world_streaming.hpp"  // Vec3, Quat

#include <cstdint>
#include <cstddef>

namespace gta {

// CJ's physical stats. Gym/running/driving/swimming progression raises these
// (host-side or in the unified ECS); the module only stores them so gameplay
// that lives here (e.g. sprint endurance) can read them. Retail range 0..1000;
// kept as floats for smooth host interpolation.
struct CJStats {
    float muscle = 0.0f;        // melee damage + build
    float fat = 0.0f;           // stamina penalty + build
    float stamina = 0.0f;       // sprint / cycle / swim endurance
    float lungCapacity = 0.0f;  // underwater breath time
    float drivingSkill = 0.0f;  // vehicle control + less damage
};

using PedHandle = uint32_t;
constexpr PedHandle kInvalidPed = 0;

struct PedState {
    Vec3 position;
    Quat rotation;
    int32_t modelId = -1;
    float health = 100.0f;
};

class IPedManager {
public:
    virtual ~IPedManager() = default;

    // Target ambient crowd density, 0..1 (0 none .. 1 busy city). Host policy.
    virtual void SetDensity(float density) = 0;
    virtual float Density() const = 0;

    virtual PedHandle Spawn(int32_t modelId, const Vec3& pos) = 0;
    virtual void Despawn(PedHandle p) = 0;

    virtual bool GetState(PedHandle p, PedState& out) const = 0;
    virtual std::size_t Count() const = 0;
};

}  // namespace gta
