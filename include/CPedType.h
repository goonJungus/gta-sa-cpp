// CPedType - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/PedType.h / PedType.cpp
// Static ped-type helpers (bit flags, acquaintance data lookups).
//
// Adaptations:
//   stripped InjectHooks()
//   GetPedFlag implemented inline (verified against gta-reversed PedType.cpp 0x608830)
//   ms_apPedTypes / Initialise / LoadPedData / FindPedType and friends are
//     ped-data-batch work - declared here, defined when that batch lands.

#pragma once

#include "ePedType.h"

#include <cstddef> // size_t
#include <cstdint>

struct CAcquaintance; // (defined in CPed.h; full port pending)

class CPedType {
public:
    // @addr 0x608830
    // @brief Get the bit flag for a ped type (bit `pedType` of a 32-bit mask)
    static uint32_t GetPedFlag(ePedType pedType) {
        if ((size_t)pedType < sizeof(uint32_t) * 8) { // don't shift more than 31 bits (UB)
            return 1u << (size_t)pedType;
        }
        return 0;
    }

    // TODO(ped-data): ms_apPedTypes / Initialise / Shutdown / LoadPedData /
    //   FindPedType / SetPedTypeAsAcquaintance etc.
    // (declared for CPed; defined when ped-data batch lands)
    static CAcquaintance* GetPedTypeAcquaintances(ePedType pedType);
    // TODO(port): stub (added 2026-10-09 for CPed)
    static void Initialise() {}
};
