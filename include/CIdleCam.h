// CIdleCam - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/IdleCam.h
// Idle/auto camera: kicks in when the player leaves the controls alone,
// slerps between points of interest and zooms the FOV.
//
// Adaptations: stripped InjectHooks() and VALIDATE_SIZE (replaced with the
// guarded static_assert below). plugin-sdk typedefs (uint32/int32/uint16)
// replaced with <cstdint> types. The inline VectorToAnglesRotXRotZ body
// (touched CGeneral/DegreesToRadians) is demoted to a declaration.
// `extern CIdleCam& gIdleCam` / `extern uint32& gbCineyCamProcessedOnFrame`
// are binary-address-free already; their definitions live in CIdleCam.cpp.

#pragma once

#include "CVector.h"
#include "CCam.h"

#include <cstdint>
#include <utility>

class CEntity;

enum class eIdleCamZoomState {
    UNK_2 = 2,
    UNK_3 = 3,
};

class CIdleCam {
public:
    CIdleCam();

    void  Init();
    void  Reset(bool resetControls);
    void  ProcessIdleCamTicker();
    bool  IsItTimeForIdleCam();
    void  IdleCamGeneralProcess();
    void  GetLookAtPositionOnTarget(const CEntity* target, CVector& outPos);
    void  ProcessFOVZoom(float time);
    bool  IsTargetValid(CEntity* target);
    void  SetTarget(CEntity* target);
    void  SetTargetPlayer();
    void  ProcessTargetSelection();
    float ProcessSlerp(float& outX, float& outZ);
    void  FinaliseIdleCamera(float curAngleX, float curAngleY, float shakeDegree);
    void  Run();
    bool  Process();

    // Was inline in gta-reversed (NOTSA); body touched CGeneral, so it is
    // demoted to a declaration. TODO: verify from decomp (no named .c in
    // src/CIdleCam/ - identify from unk_*.c / binary).
    std::pair<float, float> VectorToAnglesRotXRotZ(const CVector& pos);

public:
    CEntity*          m_Target;
    CVector           m_PositionToSlerpFrom;
    float             m_TimeSlerpStarted;
    float             m_SlerpDuration;
    CVector           m_LastIdlePos;
    float             m_SlerpTime;
    float             m_TimeControlsIdleForIdleToKickIn;
    float             m_TimeIdleCamStarted;
    uint32_t          m_LastFrameProcessed;
    float             m_TimeLastTargetSelected;
    float             m_TimeMinimumToLookAtSomething;
    float             m_TimeTargetEntityWasLastVisible;
    float             m_TimeToConsiderNonVisibleEntityAsOccluded;
    float             m_DistTooClose;
    float             m_DistStartFOVZoom;
    float             m_DistTooFar;
    int32_t           m_TargetLOSFramestoReject;
    int32_t           m_TargetLOSCounter;
    eIdleCamZoomState m_ZoomState;
    float             m_ZoomFrom;
    float             m_ZoomTo;
    float             m_TimeZoomStarted;
    float             m_ZoomNearest;
    float             m_ZoomFarthest;
    float             m_CurFOV;
    float             m_DurationFOVZoom;
    bool              m_nForceAZoomOut;
    bool              m_bHasZoomedIn;
    uint16_t          _pad;
    float             m_TimeBeforeNewZoomIn;
    float             m_TimeLastZoomIn;
    float             m_IncreaseMinimumTimeFactorForZoomedIn;
    float             m_DegreeShakeIdleCam;
    float             m_ShakeBuildUpTime;
    int32_t           m_LastTimePadTouched;
    int32_t           m_IdleTickerFrames;
    CCam*             m_Cam;
};

// Layout checks: gta-reversed VALIDATE_SIZE values, enforced only on 32-bit
// targets (the original binary is 32-bit; 64-bit dev builds skip them).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CIdleCam) == 0x9C, "CIdleCam layout drift");
#endif

extern CIdleCam& gIdleCam;
extern uint32_t& gbCineyCamProcessedOnFrame;
