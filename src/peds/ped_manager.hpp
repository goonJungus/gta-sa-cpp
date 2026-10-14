// ped_manager.hpp - concrete IPedManager (private).
//
// Foundation stage: owns spawn bookkeeping + crowd density. Full routing/AI and
// density-driven ambient spawning land in a later pass; the interface and
// handle lifetime are final so the host can wire against it now.

#pragma once

#include "gta_sa/peds.hpp"

#include <unordered_map>

namespace gta {

class PedManager final : public IPedManager {
public:
    void SetDensity(float density) override { m_density = density; }
    float Density() const override { return m_density; }

    PedHandle Spawn(int32_t modelId, const Vec3& pos) override;
    void Despawn(PedHandle p) override;

    bool GetState(PedHandle p, PedState& out) const override;
    std::size_t Count() const override { return m_peds.size(); }

    void Update(float dt);  // internal, called by GtaWorld

private:
    std::unordered_map<PedHandle, PedState> m_peds;
    PedHandle m_next = 1;
    float m_density = 0.0f;
};

}  // namespace gta
