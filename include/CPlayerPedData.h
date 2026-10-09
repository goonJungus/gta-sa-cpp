// CPlayerPedData - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/PlayerPedData.h
//
// Adaptations:
//   stripped plugin-sdk header block, InjectHooks()
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
//   uint8/uint32 -> uint8_t/uint32_t (<cstdint>)
//   Vector.h/Vector2D.h -> CVector.h/CVector2D.h (local)
//   RenderWare.h -> local (RpAtomic fwd-declared there)
//   PedClothesDesc.h -> forward declaration (CPedClothesDesc not yet ported;
//     only used as a pointer here)
//   The player-flags union is fully anonymous (matches gta-reversed):
//     bitfields, byte views and m_nPlayerFlags are directly accessible.
//     The decompiler's synthetic __anon0/__anon1 names are not used.

#pragma once

#include "CVector.h"
#include "RenderWare.h" // struct RpAtomic (fwd)

#include <cstdint>

class CEntity;
class CPed;
class CCopPed;
class CWanted;
class CPedClothesDesc;

class CPlayerPedData {
public:
    CWanted*         m_pWanted;
    CPedClothesDesc* m_pPedClothesDesc;
    CCopPed*         m_pArrestingCop;
    CVector2D        m_vecFightMovement;
    float            m_fMoveBlendRatio;
    float            m_fTimeCanRun;
    float            m_fMoveSpeed;
    uint8_t          m_nChosenWeapon;
    uint8_t          m_nCarDangerCounter;
    uint32_t         m_nStandStillTimer;
    uint32_t         m_nHitAnimDelayTimer;
    float            m_fAttackButtonCounter;
    void*            m_pDangerCar;
    union {
        struct {
            uint32_t m_bStoppedMoving : 1;
            uint32_t m_bAdrenaline : 1;
            uint32_t m_bHaveTargetSelected : 1;       // Needed to work out whether we lost target this frame
            uint32_t m_bFreeAiming : 1;
            uint32_t m_bCanBeDamaged : 1;
            uint32_t m_bAllMeleeAttackPtsBlocked : 1; // if all of m_pMeleeAttackers[] is blocked by collision, just attack straight ahead
            uint32_t m_bJustBeenSnacking : 1;         // If this bit is true we have just bought something from a vending machine
            uint32_t m_bRequireHandleBreath : 1;

            uint32_t m_bGroupStuffDisabled : 1;             // if this is true the player can't recruit or give his group commands.
            uint32_t m_bGroupAlwaysFollow : 1;              // The group is told to always follow the player (used for girlfriend missions)
            uint32_t m_bGroupNeverFollow : 1;              // The group is told to always follow the player (used for girlfriend missions)
            uint32_t m_bInVehicleDontAllowWeaponChange : 1; // stop weapon change once driveby weapon has been given
            uint32_t m_bRenderWeapon : 1;                   // set to false during cutscenes so that knuckledusters are not rendered
        };
        struct {
            uint8_t m_bPlayerFlagsByte1;
            uint8_t m_bPlayerFlagsByte2;
            uint8_t m_bPlayerFlagsByte3;
            uint8_t m_bPlayerFlagsByte4;
        };
        uint32_t m_nPlayerFlags;
    }; // 2026-10-09: fully anonymous union (matches gta-reversed
       //   PlayerPedData.h). The decompiler's synthetic __anon0/__anon1
       //   member names are not used; bitfields, byte views and
       //   m_nPlayerFlags are all directly accessible.
    uint32_t    m_nPlayerGroup;
    uint32_t    m_nAdrenalineEndTime;
    uint8_t     m_nDrunkenness;
    uint8_t     m_nFadeDrunkenness;
    uint8_t     m_nDrugLevel;
    uint8_t     m_nScriptLimitToGangSize;
    float       m_fBreath;
    uint32_t    m_nMeleeWeaponAnimReferenced;
    uint32_t    m_nMeleeWeaponAnimReferencedExtra;
    float       m_fFPSMoveHeading;
    float       m_fLookPitch;
    float       m_fSkateBoardSpeed;
    float       m_fSkateBoardLean;
    RpAtomic*   m_pSpecialAtomic;
    float       m_fGunSpinSpeed;
    float       m_fGunSpinAngle;
    uint32_t    m_nLastTimeFiring;
    uint32_t    m_nTargetBone;
    CVector     m_vecTargetBoneOffset;
    uint32_t    m_nBusFaresCollected;
    bool        m_bPlayerSprintDisabled;
    bool        m_bDontAllowWeaponChange;
    bool        m_bForceInteriorLighting;
    uint16_t    m_nPadDownPressedInMilliseconds;
    uint16_t    m_nPadUpPressedInMilliseconds;
    uint8_t     m_nWetness;
    bool        m_bPlayersGangActive;
    uint8_t     m_nWaterCoverPerc;
    float       m_fWaterHeight;
    uint32_t    m_nFireHSMissilePressedTime;
    CEntity*    m_LastHSMissileTarget;
    uint32_t    m_nModelIndexOfLastBuildingShot;
    uint32_t    m_nLastHSMissileLOSTime : 31;
    uint32_t    m_bLastHSMissileLOS : 1;
    CPed*       m_pCurrentProstitutePed;
    CPed*       m_pLastProstituteShagged;

public:
    CPlayerPedData() = default;
    ~CPlayerPedData() = default;

    void AllocateData();
    void DeAllocateData();
    void SetInitialState();
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CPlayerPedData) == 0xAC, "CPlayerPedData layout changed");
#endif
