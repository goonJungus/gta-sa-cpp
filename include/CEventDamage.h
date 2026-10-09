// CEventDamage.h - minimal stand-in (gta-reversed/source/game_sa/Events/EventDamage.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the parts used by the currently-converted TUs are defined here.
// Added 2026-10-09 for CAutomobile; extended 2026-10-09 for CPed
// (CPedDamageResponseCalculator members + default ctors for in-place construction,
//  CEventDamage animation members for knock-off-bike).

#pragma once

#include "CEventSoundQuiet.h" // CEvent base
#include "ePedPieceTypes.h"   // ePedPieceTypes (m_bodyPart)
#include "AnimTypes.h"        // AssocGroupId, AnimationId (m_nAnimGroup/m_nAnimID)

#include <cstdint>

class CEntity;
class CPed;

// Minimal CEventDamage stand-in.
// TODO(port): full CEventDamage (gta-reversed Events/EventDamage.h).
class CEventDamage : public CEvent {
public:
    // Damage response struct (minimal).
    struct DamageResponse {
        bool m_bDamageCalculated = false;
    };
    DamageResponse m_damageResponse;

    // TODO(port): animation members observed in the decomp (CPed knock-off-bike).
    AssocGroupId m_nAnimGroup{};
    AnimationId  m_nAnimID{};
    float        m_fAnimBlend{};
    float        m_fAnimSpeed{};
    uint8_t      bits_m_bJumpedOutOfMovingCar{}; // TODO(port): decomp bitfield container

    // TODO(port): default ctor only exists so the decomp can reserve stack
    //   space and construct in place (placement new); the events batch replaces this.
    CEventDamage() = default;

    CEventDamage(CEntity* damager, uint32_t time, int32_t weaponType, int32_t pedPiece, int32_t a, bool b, bool c) {
        (void)damager; (void)time; (void)weaponType; (void)pedPiece; (void)a; (void)b; (void)c;
    }

    bool AffectsPed(CPed* ped) { (void)ped; return false; }
};

// Minimal CPedDamageResponseCalculator stand-in.
// TODO(port): full implementation (gta-reversed).
class CPedDamageResponseCalculator {
public:
    // TODO(port): members observed in the decomp (CPed::ProcessEntityCollision).
    CEntity*       m_pDamager{};
    float          m_fDamageFactor{};
    ePedPieceTypes m_bodyPart{};

    // TODO(port): default ctor only exists so the decomp can reserve stack
    //   space and construct in place (placement new).
    CPedDamageResponseCalculator() = default;

    CPedDamageResponseCalculator(CEntity* entity, float timeStep, int32_t weaponType, int32_t pedPiece, bool b) {
        (void)entity; (void)timeStep; (void)weaponType; (void)pedPiece; (void)b;
    }
    void ComputeDamageResponse(CPed* ped, CEventDamage::DamageResponse& response, bool bSpeak) {
        (void)ped; (void)response; (void)bSpeak;
    }
};
