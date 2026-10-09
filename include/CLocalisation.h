// CLocalisation.h - minimal stand-in.
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Added 2026-10-09 for CAutomobile.

#pragma once

// Minimal CLocalisation stand-in.
// TODO(port): full localisation system.
class CLocalisation {
public:
    // TODO(port): real localisation checks.
    static bool ShootLimbs() { return false; }
    static bool Blood() { return true; }
    // TODO(port): stub (added 2026-10-09 for CPed)
    static bool StealFromDeadPed() { return false; }
    static bool KnockDownPeds() { return false; } // TODO(port): stub (added 2026-10-09 for CPed)
};
