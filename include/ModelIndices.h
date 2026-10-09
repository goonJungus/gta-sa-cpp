// ModelIndices.h - GTA SA 1.0 clean-room C++ conversion
// Adapted from gta-reversed/source/game_sa/ModelIndices.h
// Runtime-resolved model indices (MI_*) + Is*/Has* model predicates used by
// the vehicle subsystem. In gta-reversed the MI_* globals are
// `extern ModelIndex&` (WEnumU16<eModelID>) resolved by ModelIndices::Initialise();
// here they are StaticRef<int32_t> to the same game addresses.
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// TODO: full port - all MI_* globals + ModelIndices::Initialise/Test.

#pragma once

#include "Common.h"   // StaticRef
#include "eModelID.h" // MODEL_* constants

#include <cstdint>

namespace ModelIndices {
    // "hydralics" upgrade component (0x8CD76C)
    static inline auto& MI_HYDRAULICS = StaticRef<int32_t>(0x8CD76C);

    inline bool IsDumper(int32_t modelId)    { return modelId == MODEL_DUMPER; }
    inline bool IsPacker(int32_t modelId)    { return modelId == MODEL_PACKER; }
    inline bool IsDozer(int32_t modelId)     { return modelId == MODEL_DOZER; }
    inline bool IsAndromada(int32_t modelId) { return modelId == MODEL_ANDROM; }
    inline bool IsForklift(int32_t modelId)  { return modelId == MODEL_FORKLIFT; }
    inline bool IsRhino(int32_t modelId)     { return modelId == MODEL_RHINO; }
    inline bool IsVortex(int32_t modelId)    { return modelId == MODEL_VORTEX; }
    inline bool IsKart(int32_t modelId)      { return modelId == MODEL_KART; }      // gta-reversed ModelIndices.h:178
    inline bool IsRCBandit(int32_t modelId)  { return modelId == MODEL_RCBANDIT; }  // gta-reversed ModelIndices.h:187
    inline bool IsCementTruck(int32_t modelId) { return modelId == MODEL_CEMENT; } // gta-reversed ModelIndices.h
    inline bool IsFireTruckLadder(int32_t modelId) { return modelId == MODEL_FIRELA; } // gta-reversed ModelIndices.h
    inline bool IsBFInjection(int32_t modelId) { return modelId == MODEL_BFINJECT; } // gta-reversed ModelIndices.h

    // ModelIndex externs (gta-reversed ModelIndices.h). Simplified to int32_t constants
    // (initialized to MODEL_INVALID in gta-reversed). Added 2026-10-09 for CAutomobile.
    // TODO(port): real StaticRef<ModelIndex> when the model-info batch lands.
    inline constexpr int32_t MI_GRASSHOUSE = -1;          // MODEL_INVALID
    inline constexpr int32_t MI_GRASSPLANT = -1;          // MODEL_INVALID
    inline constexpr int32_t MI_HARVESTERBODYPART1 = -1;  // MODEL_INVALID
    inline constexpr int32_t MI_HARVESTERBODYPART2 = -1;  // MODEL_INVALID
    inline constexpr int32_t MI_HARVESTERBODYPART3 = -1;  // MODEL_INVALID
    inline constexpr int32_t MI_HARVESTERBODYPART4 = -1;  // MODEL_INVALID

    // gta-reversed ModelIndices.h (added 2026-10-09 for CAutomobile).
    inline bool IsAmphibiousHeli(int32_t modelId) { return modelId == MODEL_SEASPAR || modelId == MODEL_LEVIATHN; }
    // gta-reversed ModelIndices.h (added 2026-10-09 for CAutomobile).
    inline bool IsSwatVan(int32_t modelId) { return modelId == MODEL_SWATVAN; }
    // gta-reversed ModelIndices.h: HasMiscComponent
    inline bool HasMiscComponent(int32_t modelId) {
        return modelId == MODEL_PACKER
            || modelId == MODEL_DOZER
            || modelId == MODEL_DUMPER
            || modelId == MODEL_CEMENT
            || modelId == MODEL_ANDROM
            || modelId == MODEL_FORKLIFT;
    }
    inline bool HasWaterCannon(int32_t modelId) {
        return modelId == MODEL_FIRETRUK || modelId == MODEL_SWATVAN;
    }
} // namespace ModelIndices
