#pragma once
// CEventHitByWaterCannon - minimal stub.
// TODO(port): full port (added 2026-10-09 for CPed).
#include "CEventSoundQuiet.h"

class CEventHitByWaterCannon : public CEvent {
public:
    // TODO(port): minimal
    CEventHitByWaterCannon() = default;
    // TODO(port): decomp constructs with a float (0.75f / acceleration); real layout pending.
    // Added 2026-10-09 for CPed.
    explicit CEventHitByWaterCannon(float f) { (void)f; }
};
