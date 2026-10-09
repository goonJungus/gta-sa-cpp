#pragma once
// CCustomBuildingDNPipeline - minimal stub (gta-reversed/source/game_sa/Pipelines/CustomBuilding/CustomBuildingDNPipeline.h).
// Only the static used by CPed is defined. Full implementation lands with the pipeline batch.
// TODO(port)
#include <cstdint>

class CCustomBuildingDNPipeline {
public:
    // gta-reversed: static inline auto& m_fDNBalanceParam = StaticRef<float>(0x8D12C0); // 1.0f
    // TODO(port): verify address, implement pipeline
    static inline float m_fDNBalanceParam = 1.0f;
};
