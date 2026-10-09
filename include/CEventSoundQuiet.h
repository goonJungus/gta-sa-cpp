// CEventSoundQuiet - minimal stand-in (gta-reversed/source/game_sa/Events/EventSoundQuiet.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// The events subsystem is not ported yet; this covers CAutomobile's use.
// Added 2026-10-09 for CAutomobile compile errors.
//
// NOTE: CEventGroup comes from CPedIntelligence.h (0x4C stand-in with Add()).
// CEvent is defined here token-identical to src/CWeapon.cpp's minimal version
// (the events batch replaces both with the real hierarchy).

#pragma once

#include "CPedIntelligence.h" // struct CEventGroup (with Add)
#include "CVector.h"

#include <cstdint>

class CEntity;

// Minimal CEvent base - token-identical to src/CWeapon.cpp (ODR-safe).
class CEvent {
public:
    virtual ~CEvent() = default;
};

// TODO(port): real global event group (gta-reversed RwHelper.h).
// Declared here so CAutomobile links; defined in the events batch.
CEventGroup* GetEventGlobalGroup();

class CEventSoundQuiet : public CEvent {
public:
    // TODO(port): decomp helper (gta-reversed EventSoundQuiet.cpp); real impl with events batch.
    // Added 2026-10-09 for CPed::operator new.
    static void unk_005e0540(int32_t poolRef) { (void)poolRef; }

public:
    // gta-reversed EventSoundQuiet.h: CEventSoundQuiet(CEntity*, float, uint32, const CVector&)
    // TODO(port): real implementation stores the params and implements the event interface.
    CEventSoundQuiet(CEntity* entity, float fLocalSoundLevel, uint32 startTime, const CVector& position) {
        (void)entity; (void)fLocalSoundLevel; (void)startTime; (void)position;
    }
};
