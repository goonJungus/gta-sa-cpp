// CEventKnockOffBike.h - minimal stand-in.
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Added 2026-10-09 for CAutomobile.

#pragma once

#include "CEventSoundQuiet.h" // CEvent base
#include "CVector.h"

#include <cstdint>

class CVehicle;

// Knock-off types (gta-reversed).
// TODO(port): verify values.
enum eKnockOffType : int32_t {
    KNOCK_OFF_TYPE_FALL = 0,
    // TODO(port): other knock-off types.
};

// Minimal CEventKnockOffBike stand-in.
// TODO(port): full implementation.
class CEventKnockOffBike : public CEvent {
public:
    CEventKnockOffBike(CVehicle* vehicle, const CVector& moveSpeed, const CVector& dir, float damageIntensity,
                       float f, int32_t knockOffType, int32_t a, int32_t b, void* c, bool d, bool e) {
        (void)vehicle; (void)moveSpeed; (void)dir; (void)damageIntensity;
        (void)f; (void)knockOffType; (void)a; (void)b; (void)c; (void)d; (void)e;
    }
};
