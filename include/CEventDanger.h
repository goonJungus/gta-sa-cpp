// CEventDanger.h - minimal stand-in.
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Added 2026-10-09 for CAutomobile.

#pragma once

#include "CEventSoundQuiet.h" // CEvent base

class CEntity;

// Minimal CEventDanger stand-in.
// TODO(port): full implementation.
class CEventDanger : public CEvent {
public:
    CEventDanger(CEntity* entity, float radius) {
        (void)entity; (void)radius;
    }
};
