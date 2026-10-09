// CGlass.h - minimal stand-in.
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Added 2026-10-09 for CAutomobile.

#pragma once

class CVehicle;

// Minimal CGlass stand-in.
// TODO(port): full glass shattering system.
class CGlass {
public:
    // TODO(port): real windscreen shatter.
    static void CarWindscreenShatters(CVehicle* vehicle) { (void)vehicle; }
};
