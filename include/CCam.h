// CCam - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Cam.h
// Single camera slot: one of CCamera::m_aCams[3]. All per-mode Process_*
// handlers live here; CCamera owns the slots and the RW camera.
//
// Adaptations: stripped InjectHooks(), the private Constructor() placement
// wrapper, and VALIDATE_SIZE (replaced with the guarded static_assert below).
// plugin-sdk typedefs (uint32/int32/uint16) replaced with <cstdint> types.
// `extern bool& gbFirstPersonRunThisFrame` (binary-address global in the
// original) now lives as file-static state in CCam.cpp.
// eCamMode values verified against gta-reversed/source/game_sa/Enums/eCamMode.h
// (plugin-sdk). TODO: move eCamMode/eModelID to the enums subsystem.

#pragma once

#include "CVector.h"
#include "CColSphere.h"

#include <array>
#include <cstdint>

class CEntity;
class CPed;
class CVehicle;

// eModelID: canonical minimal enum in eModelID.h (deduped 2026-10-09).
#include "eModelID.h"

enum eCamMode : uint16_t {
    MODE_NONE = 0,
    MODE_TOPDOWN = 1,
    MODE_GTACLASSIC = 2,
    MODE_BEHINDCAR = 3,
    MODE_FOLLOWPED = 4,
    MODE_AIMING = 5,
    MODE_DEBUG = 6,
    MODE_SNIPER = 7,
    MODE_ROCKETLAUNCHER = 8,
    MODE_MODELVIEW = 9,
    MODE_BILL = 10,
    MODE_SYPHON = 11,
    MODE_CIRCLE = 12,
    MODE_CHEESYZOOM = 13,
    MODE_WHEELCAM = 14,
    MODE_FIXED = 15,
    MODE_1STPERSON = 16,
    MODE_FLYBY = 17,
    MODE_CAM_ON_A_STRING = 18,
    MODE_REACTION = 19,
    MODE_FOLLOW_PED_WITH_BIND = 20,
    MODE_CHRIS = 21,
    MODE_BEHINDBOAT = 22,
    MODE_PLAYER_FALLEN_WATER = 23,
    MODE_CAM_ON_TRAIN_ROOF = 24,
    MODE_CAM_RUNNING_SIDE_TRAIN = 25,
    MODE_BLOOD_ON_THE_TRACKS = 26,
    MODE_IM_THE_PASSENGER_WOOWOO = 27,
    MODE_SYPHON_CRIM_IN_FRONT = 28,
    MODE_PED_DEAD_BABY = 29,
    MODE_PILLOWS_PAPS = 30,
    MODE_LOOK_AT_CARS = 31,
    MODE_ARRESTCAM_ONE = 32,
    MODE_ARRESTCAM_TWO = 33,
    MODE_M16_1STPERSON = 34,
    MODE_SPECIAL_FIXED_FOR_SYPHON = 35,
    MODE_FIGHT_CAM = 36,
    MODE_TOP_DOWN_PED = 37,
    MODE_LIGHTHOUSE = 38,
    MODE_SNIPER_RUNABOUT = 39,
    MODE_ROCKETLAUNCHER_RUNABOUT = 40,
    MODE_1STPERSON_RUNABOUT = 41,
    MODE_M16_1STPERSON_RUNABOUT = 42,
    MODE_FIGHT_CAM_RUNABOUT = 43,
    MODE_EDITOR = 44,
    MODE_HELICANNON_1STPERSON = 45,
    MODE_CAMERA = 46,
    MODE_ATTACHCAM = 47,
    MODE_TWOPLAYER = 48,
    MODE_TWOPLAYER_IN_CAR_AND_SHOOTING = 49,
    MODE_TWOPLAYER_SEPARATE_CARS = 50,
    MODE_ROCKETLAUNCHER_HS = 51,
    MODE_ROCKETLAUNCHER_RUNABOUT_HS = 52,
    MODE_AIMWEAPON = 53,
    MODE_TWOPLAYER_SEPARATE_CARS_TOPDOWN = 54,
    MODE_AIMWEAPON_FROMCAR = 55,
    MODE_DW_HELI_CHASE = 56,
    MODE_DW_CAM_MAN = 57,
    MODE_DW_BIRDY = 58,
    MODE_DW_PLANE_SPOTTER = 59,
    MODE_DW_DOG_FIGHT = 60,
    MODE_DW_FISH = 61,
    MODE_DW_PLANECAM1 = 62,
    MODE_DW_PLANECAM2 = 63,
    MODE_DW_PLANECAM3 = 64,
    MODE_AIMWEAPON_ATTACHED = 65
};

class CCam {
public:
    bool      m_bBelowMinDist;
    bool      m_bBehindPlayerDesired;
    bool      m_bCamLookingAtVector;
    bool      m_bCollisionChecksOn;
    bool      m_bFixingBeta;
    bool      m_bTheHeightFixerVehicleIsATrain;
    bool      m_bLookBehindCamWasInFront;
    bool      m_bLookingBehind;
    bool      m_bLookingLeft;
    bool      m_bLookingRight;
    bool      m_bResetStatics;
    bool      m_bRotating;
    eCamMode  m_nMode;
    uint32_t  m_nFinishTime;
    uint32_t  m_nDoCollisionChecksOnFrameNum;
    uint32_t  m_nDoCollisionCheckEveryNumOfFrames;
    uint32_t  m_nFrameNumWereAt;
    uint32_t  m_nRunningVectorArrayPos;
    uint32_t  m_nRunningVectorCounter;
    uint32_t  m_nDirectionWasLooking;
    float     m_fMaxRoleAngle;
    float     m_fRoll;
    float     m_fRollSpeed;
    float     m_fSyphonModeTargetZOffSet;
    float     m_fAmountFractionObscured;
    float     m_fAlphaSpeedOverOneFrame;
    float     m_fBetaSpeedOverOneFrame;
    float     m_fBufferedTargetBeta;
    float     m_fBufferedTargetOrientation;
    float     m_fBufferedTargetOrientationSpeed;
    float     m_fCamBufferedHeight;
    float     m_fCamBufferedHeightSpeed;
    float     m_fCloseInPedHeightOffset;
    float     m_fCloseInPedHeightOffsetSpeed;
    float     m_fCloseInCarHeightOffset;
    float     m_fCloseInCarHeightOffsetSpeed;
    float     m_fDimensionOfHighestNearCar;
    float     m_fDistanceBeforeChanges;
    float     m_fFovSpeedOverOneFrame;
    float     m_fMinDistAwayFromCamWhenInterPolating;
    float     m_fPedBetweenCameraHeightOffset;
    float     m_fPlayerInFrontSyphonAngleOffSet;
    float     m_fRadiusForDead;
    float     m_fRealGroundDist;
    float     m_fTargetBeta;
    float     m_fTimeElapsedFloat;
    float     m_fTilt;
    float     m_fTiltSpeed;
    float     m_fTransitionBeta;
    float     m_fTrueBeta;
    float     m_fTrueAlpha;
    float     m_fInitialPlayerOrientation;
    float     m_fVerticalAngle;  ///< The radian angle ([-pi/2, pi/2]) relative to the entity the camera is locked on (the player usually)
    float     m_fAlphaSpeed;
    float     m_fFOV;
    float     m_fFOVSpeed;
    float     m_fHorizontalAngle;
    float     m_fBetaSpeed;
    float     m_fDistance;
    float     m_fDistanceSpeed;
    float     m_fCaMinDistance;
    float     m_fCaMaxDistance;
    float     m_fSpeedVar;
    float     m_fCameraHeightMultiplier;
    float     m_fTargetZoomGroundOne;
    float     m_fTargetZoomGroundTwo;
    float     m_fTargetZoomGroundThree;
    float     m_fTargetZoomOneZExtra;
    float     m_fTargetZoomTwoZExtra;
    float     m_fTargetZoomTwoInteriorZExtra;
    float     m_fTargetZoomThreeZExtra;
    float     m_fTargetZoomZCloseIn;
    float     m_fMinRealGroundDist;
    float     m_fTargetCloseInDist;
    float     m_fBeta_Targeting;
    float     m_fX_Targetting;
    float     m_fY_Targetting;
    CVehicle* m_pCarWeAreFocussingOn;
    CVehicle* m_pCarWeAreFocussingOnI;
    float     m_fCamBumpedHorz;
    float     m_fCamBumpedVert;
    uint32_t  m_nCamBumpedTime; // TODO: Probably float
    CVector   m_vecSourceSpeedOverOneFrame;
    CVector   m_vecTargetSpeedOverOneFrame;
    CVector   m_vecUpOverOneFrame;
    CVector   m_vecTargetCoorsForFudgeInter;
    CVector   m_vecCamFixedModeVector;
    CVector   m_vecCamFixedModeSource;
    CVector   m_vecCamFixedModeUpOffSet;
    CVector   m_vecLastAboveWaterCamPosition;
    CVector   m_vecBufferedPlayerBodyOffset;
    CVector   m_vecFront;
    CVector   m_vecSource;
    CVector   m_vecSourceBeforeLookBehind;
    CVector   m_vecUp;
    std::array<CVector, 2>  m_avecPreviousVectors;
    std::array<CVector, 4>  m_avecTargetHistoryPos;
    std::array<uint32_t, 4> m_anTargetHistoryTime;
    uint32_t  m_nCurrentHistoryPoints;
    CEntity*  m_pCamTargetEntity; // Owner entity. e.g.: player
    float     m_fCameraDistance;
    float     m_fIdealAlpha;
    float     m_fPlayerVelocity;
    CVehicle* m_pLastCarEntered;
    CPed*     m_pLastPedLookedAt;
    bool      m_bFirstPersonRunAboutActive;

public:
    CCam();

    void Init();

    void CacheLastSettingsDWCineyCam();
    void DoCamBump(float horizontal, float vertical);
    void Finalise_DW_CineyCams(const CVector& src, const CVector& dest, float roll, float fov, float nearClip, float shakeDegree);
    void GetCoreDataForDWCineyCamMode(CEntity*& entity, CVehicle*& vehicle, CVector& dest, CVector& src, CVector& targetUp, CVector& targetRight, CVector& targetFwd, CVector& targetVel, float& targetSpeed, CVector& targetAngVel, float& targetAngSpeed, CColSphere& colSphere);
    void GetLookFromLampPostPos(CEntity* target, CPed* cop, const CVector& vecTarget, const CVector& vecSource);
    void GetVectorsReadyForRW();
    void Get_TwoPlayer_AimVector(CVector&);
    bool IsTimeToExitThisDWCineyCamMode(int32_t camId, const CVector& src, const CVector& dst, float t, bool lineOfSightCheck);
    void KeepTrackOfTheSpeed(const CVector&, const CVector&, const CVector&, const float&, const float&, const float&);
    void LookBehind();
    void LookRight(bool bLookRight);
    void RotCamIfInFrontCar(const CVector&, float);
    bool Using3rdPersonMouseCam() const;
    bool GetWeaponFirstPersonOn();
    void ClipAlpha();
    void ClipBeta();

    // Was marked inlined in gta-reversed; declared here, defined in CCam.cpp.
    void ApplyUnderwaterMotionBlur();

    void Process();
    void ProcessArrestCamOne();
    void ProcessPedsDeadBaby();
    void Process_1rstPersonPedOnPC(const CVector&, float, float, float);
    void Process_1stPerson(const CVector&, float, float, float);
    void Process_AimWeapon(const CVector&, float, float, float);
    void Process_AttachedCam();
    void Process_Cam_TwoPlayer();
    void Process_Cam_TwoPlayer_InCarAndShooting();
    void Process_Cam_TwoPlayer_Separate_Cars();
    void Process_Cam_TwoPlayer_Separate_Cars_TopDown();
    void Process_DW_BirdyCam(bool);
    void Process_DW_CamManCam(bool);
    void Process_DW_HeliChaseCam(bool);
    void Process_DW_PlaneCam1(bool);
    void Process_DW_PlaneCam2(bool);
    void Process_DW_PlaneCam3(bool);
    void Process_DW_PlaneSpotterCam(bool);
    void Process_Editor(const CVector& target, float orientation, float speedVar, float speedVarWanted);
    void Process_Fixed(const CVector& target, float orientation, float speedVar, float speedVarWanted);
    void Process_FlyBy(const CVector& target, float orientation, float speedVar, float speedVarWanted);
    void Process_FollowCar_SA(const CVector& target, float orientation, float speedVar, float speedVarWanted, bool);
    void Process_FollowPedWithMouse(const CVector& target, float orientation, float speedVar, float speedVarWanted);
    void Process_FollowPed_SA(const CVector& target, float orientation, float speedVar, float speedVarWanted, bool);
    void Process_M16_1stPerson(const CVector& target, float orientation, float speedVar, float speedVarWanted);
    void Process_Rocket(const CVector& target, float orientation, float speedVar, float speedVarWanted, bool isHeatSeeking);
    void Process_SpecialFixedForSyphon(const CVector& target, float orientation, float speedVar, float speedVarWanted);
    bool Process_WheelCam(const CVector& target, float orientation, float speedVar, float speedVarWanted);
};

// Layout checks: gta-reversed VALIDATE_SIZE values, enforced only on 32-bit
// targets (the original binary is 32-bit; 64-bit dev builds skip them).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CCam) == 0x238, "CCam layout drift");
#endif

int32_t ConvertPedNode2BoneTag(int32_t simpleId);
bool    IsLampPost(eModelID modelId);
