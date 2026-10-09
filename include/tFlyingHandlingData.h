// tFlyingHandlingData.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/tFlyingHandlingData.h (plugin-sdk file)
// Per-aircraft handling.cfg data (thrust, yaw/pitch/roll, lift).
//
// Adaptations:
//   stripped plugin-sdk header comment
//   "Vector.h" -> "CVector.h"
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
// TODO:
//   port InitFromData body from decomp when cHandlingDataMgr is ported
//
#pragma once

#include "GrTypes.h"
#include "CVector.h"

struct tFlyingHandlingData {
    int32 m_nVehicleId;
    float m_fThrust;
    float m_fThrustFallOff;
    float m_fYaw;
    float m_fYawStab;
    float m_fSideSlip;
    float m_fRoll;
    float m_fRollStab;
    float m_fPitch;
    float m_fPitchStab;
    float m_fFormLift;
    float m_fAttackLift;
    float m_fGearUpR;
    float m_fGearDownL;
    float m_fWindMult;
    float m_fMoveRes;
    CVector m_vecTurnRes;
    CVector m_vecSpeedRes;

    int32 InitFromData(int32 id, const char* line);
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tFlyingHandlingData) == 0x58, "tFlyingHandlingData size mismatch");
#endif
