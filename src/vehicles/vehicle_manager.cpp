// vehicle_manager.cpp - see vehicle_manager.hpp.

#include "vehicles/vehicle_manager.hpp"

#include <cmath>
#include <algorithm>

namespace gta {

namespace {
// Rotate the local +Y forward axis by a unit quaternion -> world forward.
Vec3 Forward(const Quat& q) {
    return {2.0f * (q.x * q.y - q.w * q.z),
            1.0f - 2.0f * (q.x * q.x + q.z * q.z),
            2.0f * (q.y * q.z + q.w * q.x)};
}
// Integrate a yaw delta (about world +Z) into the quaternion.
Quat YawBy(const Quat& q, float dAngle) {
    const float h = dAngle * 0.5f;
    const float s = std::sin(h), c = std::cos(h);
    // q' = qz(dAngle) * q   (world-space yaw)
    return {c * q.x - s * q.y, s * q.x + c * q.y, c * q.z + s * q.w,
            c * q.w - s * q.z};
}
}  // namespace

VehicleHandle VehicleManager::Spawn(int32_t modelId, const Vec3& pos, const Quat& rot) {
    VehicleHandle h = m_next++;
    Instance in;
    in.modelId = modelId;
    in.pos = pos;
    in.rot = rot;
    m_vehicles.emplace(h, in);
    return h;
}

void VehicleManager::Despawn(VehicleHandle v) { m_vehicles.erase(v); }

void VehicleManager::SetDriver(VehicleHandle v, IVehicleDriver* driver) {
    auto it = m_vehicles.find(v);
    if (it != m_vehicles.end()) it->second.driver = driver;
}

bool VehicleManager::GetPose(VehicleHandle v, Vec3& pos, Quat& rot) const {
    auto it = m_vehicles.find(v);
    if (it == m_vehicles.end()) return false;
    pos = it->second.pos;
    rot = it->second.rot;
    return true;
}

bool VehicleManager::GetVelocity(VehicleHandle v, Vec3& linear) const {
    auto it = m_vehicles.find(v);
    if (it == m_vehicles.end()) return false;
    const Vec3 f = Forward(it->second.rot);
    const float s = it->second.speed;
    linear = {f.x * s, f.y * s, f.z * s};
    return true;
}

VehicleHandling& VehicleManager::Handling(int32_t modelId) {
    return m_handling[modelId];  // default-constructed on first access
}

void VehicleManager::Update(float dt) {
    for (auto& [h, in] : m_vehicles) {
        const VehicleHandling& hd = m_handling.count(in.modelId)
                                        ? m_handling[in.modelId]
                                        : VehicleHandling{};
        float throttle = 0.0f, steer = 0.0f, brake = 0.0f;
        if (in.driver) {
            throttle = std::clamp(in.driver->Throttle(), -1.0f, 1.0f);
            steer = std::clamp(in.driver->Steer(), -1.0f, 1.0f);
            brake = std::clamp(in.driver->Brake(), 0.0f, 1.0f);
        }
        const float maxSpeed = hd.maxVelocity / 3.6f;  // km/h -> m/s
        // Longitudinal: engine accel, braking, and a little aero/roll drag.
        in.speed += throttle * hd.engineAccel * dt;
        if (brake > 0.0f) {
            const float d = hd.brakeDecel * brake * dt;
            in.speed = (in.speed > 0.0f) ? std::max(0.0f, in.speed - d)
                                         : std::min(0.0f, in.speed + d);
        }
        in.speed -= in.speed * 0.02f * dt;  // drag
        in.speed = std::clamp(in.speed, -maxSpeed * 0.5f, maxSpeed);

        // Yaw scales with speed and steering, like a simple bicycle model.
        const float steerRad = steer * hd.steerAngleDeg * 3.14159265f / 180.0f;
        in.yawRate = (std::abs(in.speed) > 0.1f) ? (in.speed / 4.0f) * std::sin(steerRad)
                                                 : 0.0f;
        in.rot = YawBy(in.rot, in.yawRate * dt);

        const Vec3 f = Forward(in.rot);
        in.pos.x += f.x * in.speed * dt;
        in.pos.y += f.y * in.speed * dt;
        in.pos.z += f.z * in.speed * dt;
    }
}

}  // namespace gta
