// vehicle_manager.hpp - concrete IVehicleManager (private).
//
// Foundation stage: an arcade longitudinal/yaw integrator driven by an external
// IVehicleDriver, plus editable per-model handling. Full raycast suspension /
// chassis dynamics belong to the host's physics engine (Jolt at integration);
// the module provides the driver seam, handling data, and pose bookkeeping.

#pragma once

#include "gta_sa/vehicles.hpp"

#include <unordered_map>

namespace gta {

class VehicleManager final : public IVehicleManager {
public:
    VehicleHandle Spawn(int32_t modelId, const Vec3& pos, const Quat& rot) override;
    void Despawn(VehicleHandle v) override;
    void SetDriver(VehicleHandle v, IVehicleDriver* driver) override;

    bool GetPose(VehicleHandle v, Vec3& pos, Quat& rot) const override;
    bool GetVelocity(VehicleHandle v, Vec3& linear) const override;

    VehicleHandling& Handling(int32_t modelId) override;
    std::size_t Count() const override { return m_vehicles.size(); }

    void Update(float dt);  // internal, called by GtaWorld

private:
    struct Instance {
        int32_t modelId = -1;
        Vec3 pos;
        Quat rot;
        float speed = 0.0f;    // signed m/s along forward
        float yawRate = 0.0f;  // rad/s
        IVehicleDriver* driver = nullptr;
    };
    std::unordered_map<VehicleHandle, Instance> m_vehicles;
    std::unordered_map<int32_t, VehicleHandling> m_handling;
    VehicleHandle m_next = 1;
};

}  // namespace gta
