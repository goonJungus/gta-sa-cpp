// vehicles.hpp - Vehicle physics, handling/tuning, entry/exit.
//
// The vehicle system accepts an ARBITRARY character controller as its driver
// (Architectural Rule 3), so the physics is decoupled from CJ or any one
// actor: the host (or another module) implements IVehicleDriver.

#pragma once

#include "gta_sa/world_streaming.hpp"  // Vec3, Quat

#include <cstdint>
#include <cstddef>

namespace gta {

// A character that can drive a vehicle. Implemented host-side (CJ, an NPC, or
// an external actor). The vehicle manager polls it each update for inputs.
class IVehicleDriver {
public:
    virtual ~IVehicleDriver() = default;
    virtual float Throttle() const = 0;  // -1..1 (reverse..forward)
    virtual float Steer() const = 0;     // -1..1 (left..right)
    virtual float Brake() const = 0;     // 0..1
    virtual bool  Handbrake() const = 0;
};

// Handling/tuning for one vehicle model (seeded from handling.cfg; the host may
// override for difficulty/mods).
struct VehicleHandling {
    float mass = 1400.0f;
    float maxVelocity = 150.0f;        // km/h
    float engineAccel = 10.0f;         // m/s^2 at full throttle
    float brakeDecel = 8.0f;           // m/s^2 at full brake
    float steerAngleDeg = 35.0f;
    float tractionMultiplier = 1.0f;
    float suspensionForce = 1.0f;
};

using VehicleHandle = uint32_t;
constexpr VehicleHandle kInvalidVehicle = 0;

class IVehicleManager {
public:
    virtual ~IVehicleManager() = default;

    // Spawn/despawn a vehicle of a given IDE model id. kInvalidVehicle on fail.
    virtual VehicleHandle Spawn(int32_t modelId, const Vec3& pos, const Quat& rot) = 0;
    virtual void Despawn(VehicleHandle v) = 0;

    // Attach/detach an external driver. A driverless vehicle coasts to rest.
    virtual void SetDriver(VehicleHandle v, IVehicleDriver* driver) = 0;

    // Pose/velocity query for the host renderer + physics bridge.
    virtual bool GetPose(VehicleHandle v, Vec3& pos, Quat& rot) const = 0;
    virtual bool GetVelocity(VehicleHandle v, Vec3& linear) const = 0;

    // Editable handling data for a model (created on first access from defaults).
    virtual VehicleHandling& Handling(int32_t modelId) = 0;

    virtual std::size_t Count() const = 0;
};

}  // namespace gta
