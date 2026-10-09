#pragma once
// CPedClothesDesc - minimal stub.
// TODO(port): full port (added 2026-10-09 for CPed::PlayFootSteps).
// Signature from gta-reversed PedClothesDesc.h.
#include <cstdint>

class CPedClothesDesc {
public:
    uint32_t m_anModelKeys[10]{}; // TODO(port): stub; real layout with clothes batch
    // TODO(port): stub; real implementation with clothes batch.
    bool GetIsWearingBalaclava() { return false; }
};
