// CEventVehicleDamageCollision - minimal stand-in (gta-reversed/source/game_sa/Events/EventVehicleDamageCollision.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// The events subsystem is not ported yet; this covers CAutomobile's use.
// Added 2026-10-09 for CAutomobile compile errors.

#pragma once

#include "CEventSoundQuiet.h" // CEvent (minimal base)
#include "eWeaponType.h"      // eWeaponType, WEAPON_RAMMEDBYCAR

#include <cstdint>

class CVehicle;
class CEntity;

class CEventVehicleDamageCollision : public CEvent {
public:
    // gta-reversed: CEventVehicleDamageCollision(CVehicle* vehicle, CEntity* attacker, eWeaponType weaponType);
    // TODO(port): real implementation stores the params.
    CEventVehicleDamageCollision(CVehicle* vehicle, CEntity* attacker, eWeaponType weaponType) {
        (void)vehicle; (void)attacker; (void)weaponType;
    }
};
