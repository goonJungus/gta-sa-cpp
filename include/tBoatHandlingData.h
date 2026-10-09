// tBoatHandlingData.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/tBoatHandlingData.h (plugin-sdk file)
// Per-boat handling.cfg data (thrust, aquaplane force/limit).
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

struct tBoatHandlingData {
    int32   m_nVehicleId;
    float   m_fThrustY;
    float   m_fThrustZ;
    float   m_fThrustAppZ;
    float   m_fAqPlaneForce;
    float   m_fAqPlaneLimit;
    float   m_fAqPlaneOffset;
    float   m_fWaveAudioMult;
    float   m_fLookLRBehindCamHeight;
    CVector m_vecMoveRes;
    CVector m_vecTurnRes;

    int32 InitFromData(int32 id, const char* line);
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tBoatHandlingData) == 0x3C, "tBoatHandlingData size mismatch");
#endif
