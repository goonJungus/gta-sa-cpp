// CRepeatSector.h - minimal stand-in.
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Added 2026-10-09 for CAutomobile.

#pragma once

#include "CPtrListDoubleLink.h"

class CVehicle;

// Minimal CRepeatSector stand-in.
// TODO(port): full sector system (gta-reversed).
struct CRepeatSector {
    CPtrListDoubleLink<CVehicle*> Vehicles;
    // TODO(port): other sector lists (Peds, Objects, etc.)
};
