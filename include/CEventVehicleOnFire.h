// CEventVehicleOnFire.h - minimal stand-in (gta-reversed/source/game_sa/Events/EventVehicleOnFire.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the constructor used by the currently-converted TUs is defined here.
// The full event lands with the events batch. Added 2026-10-09 for CAutomobile.

#pragma once

#include "Common.h"
#include "CEventSoundQuiet.h" // CEvent base (minimal stand-in)

class CVehicle;

// Minimal CEventVehicleOnFire stand-in (gta-reversed Events/EventVehicleOnFire.h).
// TODO(port): full CEventVehicleOnFire (inherits CEventEditableResponse).
class CEventVehicleOnFire : public CEvent {
public:
    CVehicle* m_vehicle{};

    explicit CEventVehicleOnFire(CVehicle* vehicle) : m_vehicle(vehicle) {}
};
