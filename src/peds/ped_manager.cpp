// ped_manager.cpp - see ped_manager.hpp.

#include "peds/ped_manager.hpp"

namespace gta {

PedHandle PedManager::Spawn(int32_t modelId, const Vec3& pos) {
    PedHandle h = m_next++;
    PedState s;
    s.modelId = modelId;
    s.position = pos;
    m_peds.emplace(h, s);
    return h;
}

void PedManager::Despawn(PedHandle p) { m_peds.erase(p); }

bool PedManager::GetState(PedHandle p, PedState& out) const {
    auto it = m_peds.find(p);
    if (it == m_peds.end()) return false;
    out = it->second;
    return true;
}

void PedManager::Update(float /*dt*/) {
    // Foundation: no ambient spawning or routing yet. Spawned peds hold their
    // state until the host moves/despawns them.
}

}  // namespace gta
