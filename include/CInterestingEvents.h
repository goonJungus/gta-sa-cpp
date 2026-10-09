// CInterestingEvents - minimal stand-in (gta-reversed/source/game_sa/InterestingEvents.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts needed by CAutomobile are defined here.
// Added 2026-10-09 for CAutomobile compile errors.

#pragma once

#include <cstdint>

class CEntity;

class CInterestingEvents {
public:
    // gta-reversed InterestingEvents.h EType (partial; only what CAutomobile uses).
    // TODO(port): full EType enum.
    enum class EType : int32_t {
        VEHICLE_DAMAGE = 0, // TODO(port): verify value vs gta-reversed
    };

    // TODO(port): real implementation.
    // Declared here so CAutomobile links; defined in the events batch.
    void Add(EType type, CEntity* entity);
};

// gta-reversed: extern CInterestingEvents& g_InterestingEvents;
// TODO(port): real global. Defined in the events batch.
extern CInterestingEvents& g_InterestingEvents;
