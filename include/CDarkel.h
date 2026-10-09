// CDarkel - minimal stand-in (gta-reversed/source/game_sa/Darkel.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts needed by CAutomobile are defined here.
// Added 2026-10-09 for CAutomobile compile errors.

#pragma once

#include <cstdint>

class CVehicle;

class CDarkel {
public:
    // gta-reversed Darkel.h: static void RegisterCarBlownUpByPlayer(CVehicle& vehicle, int32 playerId);
    // TODO(port): real implementation (stats).
    // Declared here so CAutomobile links; defined in the stats batch.
    static void RegisterCarBlownUpByPlayer(CVehicle& vehicle, int32_t playerId);
};
