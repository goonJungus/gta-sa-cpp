// CBuoyancy.h - minimal stand-in (gta-reversed/source/game_sa/Buoyancy.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Added 2026-10-09 for CAutomobile.

#pragma once

#include "Common.h"

class CPhysical;
class CVector;

// Minimal CBuoyancy stand-in.
// TODO(port): full CBuoyancy implementation.
class CBuoyancy {
public:
    float m_fWaterLevel{}; // gta-reversed Buoyancy.h; TODO(port): verify address. Added 2026-10-09 for CPed.

    // TODO(port): real buoyancy calculation.
    bool ProcessBuoyancy(CPhysical* physical, float fBuoyancyConstant, CVector* vecBuoyancyTurnPoint, CVector* vecBuoyancyForce) {
        (void)physical; (void)fBuoyancyConstant; (void)vecBuoyancyTurnPoint; (void)vecBuoyancyForce;
        return false;
    }
};

// gta-reversed Buoyancy.h (global instance).
// TODO(port): verify address.
extern CBuoyancy mod_Buoyancy;
