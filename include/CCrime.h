// CCrime.h - minimal stand-in.
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Added 2026-10-09 for CAutomobile.

#pragma once

#include <cstdint>

class CEntity;
class CPed;

// Crime types.
// TODO(port): verify values vs gta-reversed, full enum. Added 2026-10-09 for CAutomobile.
enum eCrimeType {
    CRIME_EXPLOSION = 0,
    // TODO(port): other crime types.
};

// Minimal CCrime stand-in.
// TODO(port): full crime system.
class CCrime {
public:
    // TODO(port): real crime reporting.
    static void ReportCrime(eCrimeType crimeType, CEntity* entity, CPed* ped) {
        (void)crimeType; (void)entity; (void)ped;
    }
};
