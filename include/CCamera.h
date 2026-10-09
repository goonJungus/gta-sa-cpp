// CCamera - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Camera.h
// The camera manager: owns the 3 CCam slots (2 = debug cam), the RenderWare
// camera, fades, shakes, splines, widescreen, and all mode switching.
// Hierarchy: CPlaceable -> CCamera.
//
// Adaptations: stripped InjectHooks(), the Constructor()/Destructor()
// placement wrappers, and VALIDATE_SIZE. sizeof(CCamera) (0xD78) stays a
// TODO until the CPlaceable/CEntity layout chain is verified (CEntity's own
// assert is still a TODO). The 6 in-class `static inline StaticRef<...>`
// tunables (m_f3rdPersonCHairMultY etc., binary addresses 0xB6EC10-0xB6EC2E)
// are removed from the class; they live as file-static state in CCamera.cpp.
// Same for the two namespace-scope StaticRefs (gpMadeInvisibleEntities,
// gNumEntitiesSetInvisible). The `extern` globals below are
// binary-address-free; their definitions live in CCamera.cpp.
// Inline bodies that touched RenderWare/CGeneral (GetRwMatrix,
// IsSphereVisibleInMirror, GetFrustumPoints, GetFrontNormal2D,
// VectorToAnglesRotXRotZ-equivalents) are demoted to declarations; trivial
// member accessors (GetActiveCam, GetViewMatrix, IsSphereVisible(CSphere))
// stay inline.
// eCamMode comes from CCam.h. ePedType/eNameState values verified against
// gta-reversed Enums/ (plugin-sdk); TODO: move both to the enums subsystem.
// eVehicleType now comes from the canonical eVehicleType.h (deduped 2026-10-09). CQueuedMode/CCamPathSplines are minimal stand-ins
// (sizes match VALIDATE_SIZE: 0xC / 0x4); TODO: split into own headers.

#pragma once

#include "CPlaceable.h"
#include "CCam.h"
#include "CVector.h"
#include "CMatrix.h"
#include "CRect.h"
#include "ColTypes.h" // CSphere

#include <array>
#include <cstdint>
#include <cstdio> // FILE (LoadPathSplines)

class CEntity;
class CPed;
class CVehicle;
class CGarage;
class CVector2D;

// Minimal RenderWare stand-ins: opaque pointers only, no RW SDK dependency.
struct RwCamera;
struct RwMatrix;

enum class eFadeFlag : uint16_t {
    FADE_IN,
    FADE_OUT
};

enum class eSwitchType : uint16_t {
    NONE,
    INTERPOLATION,
    JUMPCUT
};

/* todo:
  LOOKING_BEHIND = 0x0,
  LOOKING_LEFT = 0x1,
  LOOKING_RIGHT = 0x2,
  LOOKING_FORWARD = 0x3,
*/
enum eLookingDirection {
    LOOKING_DIRECTION_UNKNOWN_1 = 0,
    LOOKING_DIRECTION_BEHIND    = 1,
    LOOKING_DIRECTION_UNKNOWN_3 = 2,
    LOOKING_DIRECTION_FORWARD   = 3,
};

enum class eGroundHeightType : int32_t {
    ENTITY_BB_BOTTOM = 0,    // ground height + boundingBoxMin.z of colliding entity
    EXACT_GROUND_HEIGHT = 1, // ignores height of colliding entity at position
    ENTITY_BB_TOP = 2        // ground height + boundingBoxMax.z of colliding entity
};

enum class eMotionBlurType : uint32_t {
    NONE = 0,
    SNIPER,
    LIGHT_SCENE,
    SECURITY_CAM,
    CUT_SCENE,
    INTRO,
    INTRO2,
    SNIPER_ZOOM,
    INTRO3,
    INTRO4,
};

// Verified against gta-reversed/source/game_sa/Enums/eHud.h (plugin-sdk).
enum eNameState {
    NAME_DONT_SHOW = 0,
    NAME_SHOW      = 1,
    NAME_FADE_IN   = 2,
    NAME_FADE_OUT  = 3,
    NAME_SWITCH    = 4,
};

// eVehicleType: canonical definition lives in eVehicleType.h (deduped 2026-10-09
// camera batch; was inlined here and in CVehicleModelInfo.h with identical values).
#include "eVehicleType.h"

// ePedType: canonical full port (deduped 2026-10-08, ped batch)
#include "ePedType.h"

struct CamTweak {
    int32_t ModelID;
    float   Dist;
    float   Alt;
    float   Angle;
};

// Layout checks: gta-reversed VALIDATE_SIZE values, enforced only on 32-bit
// targets (the original binary is 32-bit; 64-bit dev builds skip them).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CamTweak) == 0x10, "CamTweak layout drift");
#endif

// Minimal stand-in for gta-reversed/source/game_sa/QueuedMode.h.
// TODO: split into QueuedMode.h when the camera support types are ported.
class CQueuedMode {
public:
    uint16_t m_nMode;
    float    m_fDuration;
    uint16_t m_nMinZoom;
    uint16_t m_nMaxZoom;
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CQueuedMode) == 0xC, "CQueuedMode layout drift");
#endif

// Minimal stand-in for gta-reversed/source/game_sa/CamPathSplines.h.
// TODO: split into CamPathSplines.h when the camera support types are ported.
class CCamPathSplines {
public:
    float* m_pArrPathData;
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CCamPathSplines) == 0x4, "CCamPathSplines layout drift");
#endif

class CCamera : public CPlaceable {
public:
    bool            m_bAboveGroundTrainNodesLoaded{};
    bool            m_bBelowGroundTrainNodesLoaded{};
    bool            m_bCamDirectlyBehind{};
    bool            m_bCamDirectlyInFront{};
    bool            m_bCameraJustRestored{};
    bool            m_bCutsceneFinished{};
    bool            m_bCullZoneChecksOn{};
    bool            m_bFirstPersonBeingUsed{};
    bool            m_bJustJumpedOutOf1stPersonBecauseOfTarget{};
    bool            m_bIdleOn{};
    bool            m_bInATunnelAndABigVehicle{};
    bool            m_bInitialNodeFound{};
    bool            m_bInitialNoNodeStaticsSet{};
    bool            m_bIgnoreFadingStuffForMusic{};
    bool            m_bPlayerIsInGarage{};
    bool            m_bPlayerWasOnBike{};
    bool            m_bJustCameOutOfGarage{};
    bool            m_bJustInitialized{true};
    bool            m_bJust_Switched{};
    bool            m_bLookingAtPlayer{true};
    bool            m_bLookingAtVector{};
    bool            m_bMoveCamToAvoidGeom{};
    bool            m_bObbeCinematicPedCamOn{};
    bool            m_bObbeCinematicCarCamOn{};
    bool            m_bRestoreByJumpCut{};
    bool            m_bUseNearClipScript{};
    bool            m_bStartInterScript{};
    bool            m_bStartingSpline{};
    bool            m_bTargetJustBeenOnTrain{};
    bool            m_bTargetJustCameOffTrain{};
    bool            m_bUseSpecialFovTrain{};
    bool            m_bUseTransitionBeta{};
    bool            m_bUseScriptZoomValuePed{};
    bool            m_bUseScriptZoomValueCar{};
    bool            m_bWaitForInterpolToFinish{};
    bool            m_bItsOkToLookJustAtThePlayer{};
    bool            m_bWantsToSwitchWidescreenOff{};
    bool            m_bWideScreenOn{};
    bool            m_b1rstPersonRunCloseToAWall{};
    bool            m_bHeadBob{};
    bool            m_bVehicleSuspenHigh{};
    bool            m_bEnable1rstPersonCamCntrlsScript{};
    bool            m_bAllow1rstPersonWeaponsCamera{};
    bool            m_bCooperativeCamMode{};
    bool            m_bAllowShootingWith2PlayersInCar{true};
    bool            m_bDisableFirstPersonInCar{};
    eCamMode        m_nModeForTwoPlayersSeparateCars{ MODE_TWOPLAYER_SEPARATE_CARS };
    eCamMode        m_nModeForTwoPlayersSameCarShootingAllowed{ MODE_TWOPLAYER_IN_CAR_AND_SHOOTING };
    eCamMode        m_nModeForTwoPlayersSameCarShootingNotAllowed{ MODE_BEHINDCAR };
    eCamMode        m_nModeForTwoPlayersNotBothInCar{ MODE_TWOPLAYER };
    bool            m_bGarageFixedCamPositionSet{};
    bool            m_bDoingSpecialInterp{};
    bool            m_bScriptParametersSetForInterp{};
    bool            m_bFading{};
    bool            m_bMusicFading{};
    bool            m_bMusicFadedOut{};
    bool            m_bFailedCullZoneTestPreviously{};
    bool            m_bFadeTargetIsSplashScreen{};
    bool            m_bWorldViewerBeingUsed{};
    bool            m_bTransitionJUSTStarted{};
    bool            m_bTransitionState{};
    uint8_t         m_nActiveCam{};
    uint32_t        m_nCamShakeStart{};
    uint32_t        m_nFirstPersonCamLastInputTime{};
    uint32_t        m_nLongestTimeInMill{ 5000 };
    uint32_t        m_nNumberOfTrainCamNodes{};
    uint32_t        m_nTimeLastChange{};
    uint32_t        m_nTimeWeLeftIdle_StillNoInput{};
    uint32_t        m_nTimeWeEnteredIdle{};
    uint32_t        m_nTimeTransitionStart{};
    uint32_t        m_nTransitionDuration{};
    uint32_t        m_nTransitionDurationTargetCoors{};
    uint32_t        m_nBlurBlue{};
    uint32_t        m_nBlurGreen{};
    uint32_t        m_nBlurRed{};
    eMotionBlurType m_nBlurType{};
    uint32_t        m_nWorkOutSpeedThisNumFrames{4};
    uint32_t        m_nNumFramesSoFar{};
    uint32_t        m_nCurrentTrainCamNode{};
    uint32_t        m_nMotionBlur{};
    uint32_t        m_nMotionBlurAddAlpha{};
    uint32_t        m_nCheckCullZoneThisNumFrames{6};
    uint32_t        m_nZoneCullFrameNumWereAt{};
    uint32_t        m_nWhoIsInControlOfTheCamera{};
    uint32_t        m_nCarZoom{2};
    float           m_fCarZoomBase{};
    float           m_fCarZoomTotal{};
    float           m_fCarZoomSmoothed{};
    float           m_fCarZoomValueScript{};
    uint32_t        m_nPedZoom{2};
    float           m_fPedZoomBase{};
    float           m_fPedZoomTotal{};
    float           m_fPedZoomSmoothed{};
    float           m_fPedZoomValueScript{};
    float           m_fCamFrontXNorm{};
    float           m_fCamFrontYNorm{};
    float           m_fDistanceToWater{};
    float           m_fHeightOfNearestWater{};
    float           m_fFOVDuringInter{};
    float           m_fLODDistMultiplier{1.f};
    float           m_fGenerationDistMultiplier{};
    float           m_fAlphaSpeedAtStartInter{};
    float           m_fAlphaWhenInterPol{};
    float           m_fAlphaDuringInterPol{};
    float           m_fBetaDuringInterPol{};
    float           m_fBetaSpeedAtStartInter{};
    float           m_fBetaWhenInterPol{};
    float           m_fFOVWhenInterPol{};
    float           m_fFOVSpeedAtStartInter{};
    float           m_fStartingBetaForInterPol{};
    float           m_fStartingAlphaForInterPol{};
    float           m_fPedOrientForBehindOrInFront{};
    float           m_fCameraAverageSpeed{};
    float           m_fCameraSpeedSoFar{};
    float           m_fCamShakeForce{};
    float           m_fFovForTrain{70.f};
    float           m_fFOV_Wide_Screen{};
    float           m_fNearClipScript{ 0.9f };
    float           m_fOldBetaDiff{};
    float           m_fPositionAlongSpline{};
    float           m_fScreenReductionPercentage{};
    float           m_fScreenReductionSpeed{};
    float           m_fAlphaForPlayerAnim1rstPerson{};
    float           m_fOrientation{};
    float           m_fPlayerExhaustion{1.f};
    float           m_fSoundDistUp{};
    float           m_fSoundDistUpAsRead{};
    float           m_fSoundDistUpAsReadOld{};
    float           m_fAvoidTheGeometryProbsTimer{};
    uint16_t        m_nAvoidTheGeometryProbsDirn{};
    float           m_fWideScreenReductionAmount{};
    float           m_fStartingFOVForInterPol{};
    std::array<CCam, 3>            m_aCams{}; /* 2 = debug cam */
    CGarage*        m_pToGarageWeAreIn{};
    CGarage*        m_pToGarageWeAreInForHackAvoidFirstPerson{};
    CQueuedMode     m_PlayerMode{};
    CQueuedMode     m_PlayerWeaponMode{};
    CVector         m_vecPreviousCameraPosition{};
    CVector         m_vecRealPreviousCameraPosition{};
    CVector         m_vecAimingTargetCoors{};
    CVector         m_vecFixedModeVector{};
    CVector         m_vecFixedModeSource{};
    CVector         m_vecFixedModeUpOffSet{};
    CVector         m_vecCutSceneOffset{};
    CVector         m_vecStartingSourceForInterPol{};
    CVector         m_vecStartingTargetForInterPol{};
    CVector         m_vecStartingUpForInterPol{};
    CVector         m_vecSourceSpeedAtStartInter{};
    CVector         m_vecTargetSpeedAtStartInter{};
    CVector         m_vecUpSpeedAtStartInter{};
    CVector         m_vecSourceWhenInterPol{};
    CVector         m_vecTargetWhenInterPol{};
    CVector         m_vecUpWhenInterPol{};
    CVector         m_vecClearGeometryVec{};
    CVector         m_vecGameCamPos{};
    CVector         m_vecSourceDuringInter{};
    CVector         m_vecTargetDuringInter{};
    CVector         m_vecUpDuringInter{};
    CVector         m_vecAttachedCamOffset{};
    CVector         m_vecAttachedCamLookAt{};
    float           m_fAttachedCamAngle{};
    RwCamera*       m_pRwCamera{};
    CEntity*        m_pTargetEntity{};
    CEntity*        m_pAttachedEntity{};
    std::array<CCamPathSplines, 4> m_aPathArray{};
    bool            m_bMirrorActive{};
    bool            m_bResetOldMatrix{};
    CMatrix         m_mCameraMatrix{}; // TODO: gta-reversed default-initializes to CMatrix::Identity()
    CMatrix         m_mCameraMatrixOld{};
    CMatrix         m_mViewMatrix{};
    CMatrix         m_mMatInverse{};
    CMatrix         m_mMatMirrorInverse{};
    CMatrix         m_mMatMirror{};
    std::array<CVector, 4> m_avecFrustumNormals{};
    std::array<CVector, 4> m_avecFrustumWorldNormals{};
    std::array<CVector, 4> m_avecFrustumWorldNormals_Mirror{};
    std::array<float, 4>   m_fFrustumPlaneOffsets{};
    std::array<float, 4>   m_fFrustumPlaneOffsets_Mirror{};
    CVector         m_vecRightFrustumNormal{};  //!< unused?
    CVector         m_vecBottomFrustumNormal{}; //!< unused?
    CVector         m_vecTopFrustumNormal{};    //!< unused?
    float           field_BF8{};                //!< unused?
    float           m_fFadeAlpha{};
    float           m_fEffectsFaderScalingFactor{};
    float           m_fFadeDuration{};
    float           m_fTimeToFadeMusic{};
    float           m_fTimeToWaitToFadeMusic{};
    float           m_fFractionInterToStopMoving{0.25f};
    float           m_fFractionInterToStopCatchUp{0.75f};
    float           m_fFractionInterToStopMovingTarget{};
    float           m_fFractionInterToStopCatchUpTarget{};
    float           m_fGaitSwayBuffer{0.85f};
    float           m_fScriptPercentageInterToStopMoving{};
    float           m_fScriptPercentageInterToCatchUp{};
    uint32_t        m_nScriptTimeForInterpolation{};
    eFadeFlag       m_nFadeInOutFlag{};
    int32_t         m_nModeObbeCamIsInForCar{30};
    eCamMode        m_nModeToGoTo{ MODE_FOLLOWPED };
    eFadeFlag       m_nMusicFadingDirection{};
    eSwitchType     m_nTypeOfSwitch{ eSwitchType::INTERPOLATION };
    char            _alignC40[2]{};
    uint32_t        m_nFadeStartTime{};
    uint32_t        m_nFadeTimeStartedMusic{};
    int32_t         m_nExtraEntitiesCount{};
    CEntity*        m_pExtraEntity[2]{};
    float           m_fDuckCamMotionFactor{};
    float           m_fDuckAimCamMotionFactor{};
    float           m_fTrackLinearStartTime{};
    float           m_fTrackLinearEndTime{};
    CVector         m_vecTrackLinearEndPoint{};
    CVector         m_vecTrackLinearStartPoint{};
    bool            m_bTrackLinearWithEase{};
    CVector         m_vecTrackLinear{};
    bool            m_bVecTrackLinearProcessed{};
    float           m_fShakeIntensity{};
    float           m_fStartShakeTime{}; ///< In MS [Obtained from `CTimer::GetTimeInMS()`]
    float           m_fEndShakeTime{};
    int32_t         field_C9C{};
    int32_t         m_nShakeType{};
    float           m_fStartZoomTime{};
    float           m_fEndZoomTime{};
    float           m_fZoomInFactor{};
    float           m_fZoomOutFactor{};
    uint8_t         m_nZoomMode{};
    bool            m_bFOVLerpProcessed{};
    float           m_fFOVNew{};
    float           m_fMoveLinearStartTime{};
    float           m_fMoveLinearEndTime{};
    CVector         m_vecMoveLinearPosnStart{};
    CVector         m_vecMoveLinearPosnEnd{};
    bool            m_bMoveLinearWithEase{};
    CVector         m_vecMoveLinear{};
    bool            m_bVecMoveLinearProcessed{};
    bool            m_bBlockZoom{};
    bool            m_bCameraPersistPosition{};
    bool            m_bCameraPersistTrack{};
    bool            m_bCinemaCamera{};
    CamTweak        m_aCamTweak[5]{};
    bool            m_bCameraVehicleTweaksInitialized{};
    float           m_fCurrentTweakDistance{};
    float           m_fCurrentTweakAltitude{};
    float           m_fCurrentTweakAngle{};
    int32_t         m_nCurrentTweakModelIndex{};
    // the following are unused?
    int32_t         field_D58{};
    int32_t         field_D5C{};
    int32_t         field_D60{};
    int32_t         field_D64{};
    int32_t         field_D68{};
    int32_t         field_D6C{};
    int32_t         field_D70{};
    int32_t         field_D74{};

    // NOTE: the 6 in-class `static inline StaticRef<...>` tunables from
    // gta-reversed (m_f3rdPersonCHairMultY/X @ 0xB6EC10/14, m_fMouseAccelVertical/
    // m_fMouseAccelHorzntl @ 0xB6EC18/1C, m_bUseMouse3rdPerson @ 0xB6EC2E,
    // bDidWeProcessAnyCinemaCam @ 0xB6EC2D) are REMOVED from the class.
    // They live as the namespace-scope globals declared at the bottom of this
    // header (defined in CCamera.cpp) so CCam.cpp can share them.

public:
    CCamera();
    ~CCamera() override;

    void Init();
    void InitCameraVehicleTweaks();
    void InitialiseScriptableComponents();
    void InitialiseCameraForDebugMode();

    void LoadPathSplines(FILE* file);

    bool IsTargetingActive();
    bool IsExtraEntityToIgnore(CEntity* entity);
    bool IsItTimeForNewCamera(int32_t camSequence, int32_t startTime); // IsItTimeForNewcam
    bool IsSphereVisible(const CVector& origin, float radius, RwMatrix* transformMatrix);
    bool IsSphereVisible(const CVector& origin, float radius);
    bool IsSphereVisible(const CSphere& sphere) { return IsSphereVisible(sphere.m_vecCenter, sphere.m_fRadius); }
    void LerpFOV(float zoomInFactor, float zoomOutFactor, float timeLimit, bool bEase);

    void Process();
    void ProcessWideScreenOn();
    void ProcessFOVLerp(float ratio);
    void ProcessFOVLerp();
    void ProcessFade();
    void ProcessMusicFade();
    void ProcessScriptedCommands();
    void ProcessShake();
    CVector* ProcessShake(float intensity);
    void ProcessVectorMoveLinear();
    void ProcessVectorMoveLinear(float ratio);
    void ProcessVectorTrackLinear();
    void ProcessVectorTrackLinear(float ratio);
    void ProcessObbeCinemaCameraBoat();
    void ProcessObbeCinemaCameraCar();
    void ProcessObbeCinemaCameraHeli();
    void ProcessObbeCinemaCameraPed();
    void ProcessObbeCinemaCameraPlane();
    void ProcessObbeCinemaCameraTrain();
    static void DontProcessObbeCinemaCamera();

    void Restore();
    void RestoreCameraAfterMirror();
    void RestoreWithJumpCut();
    void RenderMotionBlur() const;
    void ResetDuckingSystem(CPed* ped);

    void SetCamCutSceneOffSet(const CVector& offset);
    void SetCamPositionForFixedMode(const CVector& fixedModeSource, const CVector& fixedModeUpOffset);
    void SetCameraDirectlyBehindForFollowPed_CamOnAString();
    void SetCameraDirectlyInFrontForFollowPed_CamOnAString();
    void SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString(CPed* targetPed);
    void SetCameraDirectlyInFrontForFollowPed_ForAPed_CamOnAString(CPed* targetPed);
    void SetCameraUpForMirror();
    void SetFadeColour(uint8_t red, uint8_t green, uint8_t blue);
    void SetMotionBlur(uint8_t red, uint8_t green, uint8_t blue, int32_t value, eMotionBlurType blurType);
    void SetMotionBlurAlpha(int32_t alpha);
    void SetNearClipBasedOnPedCollision(float arg2);
    void SetNearClipScript(float nearClip);
    void SetNewPlayerWeaponMode(eCamMode mode, int16_t maxZoom = 0, int16_t minZoom = 0);
    void SetParametersForScriptInterpolation(float interpolationToStopMoving, float interpolationToCatchUp, uint32_t timeForInterpolation);
    void SetPercentAlongCutScene(float percent);
    void SetRwCamera(RwCamera* camera);
    void SetWideScreenOff();
    void SetWideScreenOn();
    void SetZoomValueCamStringScript(int16_t zoomMode);
    void SetZoomValueFollowPedScript(int16_t zoomMode);

    static void SetCamCollisionVarDataSet(int32_t index);
    static void SetColVarsAimWeapon(int32_t aimingType);
    static void SetColVarsPed(ePedType pedType, int32_t nCamPedZoom);
    static void SetColVarsVehicle(eVehicleType vehicleType, int32_t camVehicleZoom);

    void StartCooperativeCamMode();
    void StopCooperativeCamMode();
    void StartTransition(eCamMode newCamMode);
    void StartTransitionWhenNotFinishedInter(eCamMode newCamMode);

    void StoreValuesDuringInterPol(CVector* sourceDuringInter, CVector* targetDuringInter, CVector* upDuringInter, float* FOVDuringInter);

    void TakeControl(CEntity* target, eCamMode modeToGoTo, eSwitchType switchType, int32_t whoIsInControlOfTheCamera);
    void TakeControlNoEntity(const CVector& fixedModeVector, eSwitchType switchType, int32_t whoIsInControlOfTheCamera);
    void TakeControlAttachToEntity(CEntity* target, CEntity* attached, CVector* attachedCamOffset, CVector* attachedCamLookAt, float attachedCamAngle, eSwitchType switchType, int32_t whoIsInControlOfTheCamera);
    void TakeControlWithSpline(eSwitchType switchType);

    bool TryToStartNewCamMode(int32_t camSequence);

    void UpdateAimingCoors(const CVector& aimingTargetCoors);
    void UpdateSoundDistances();
    void UpdateTargetEntity();
    bool Using1stPersonWeaponMode() const;

    bool VectorMoveRunning() const;
    void VectorMoveLinear(CVector* to, CVector* from, float duration, bool bMoveLinearWithEase);

    bool VectorTrackRunning() const;
    void VectorTrackLinear(CVector* to, CVector* from, float duration, bool bEase);

    void AllowShootingWith2PlayersInCar(bool bAllow);
    void ApplyVehicleCameraTweaks(CVehicle* vehicle);
    void AvoidTheGeometry(const CVector* arg2, const CVector* arg3, CVector* arg4, float FOV);

    void CalculateDerivedValues(bool bForMirror, bool bOriented);
    void CalculateFrustumPlanes(bool bForMirror);
    float CalculateGroundHeight(eGroundHeightType type);
    void CalculateMirroredMatrix(CVector posn, float mirrorV, CMatrix* camMatrix, CMatrix* mirrorMatrix);
    void CamControl();

    void AddShake(float duration, float a2, float a3, float a4, float a5);
    void AddShakeSimple(float duration, int32_t type, float intensity);
    void CamShake(float strength, CVector from);
    void CameraColDetAndReact(CVector* source, CVector* target);
    void CameraGenericModeSpecialCases(CPed* targetPed);
    void CameraPedAimModeSpecialCases(CPed* ped);
    void CameraPedModeSpecialCases();
    void CameraVehicleModeSpecialCases(CVehicle* vehicle);
    void ClearPlayerWeaponMode();
    bool ConeCastCollisionResolve(const CVector& pos, const CVector& lookAt, CVector& outDest, float rad, float minDist, float& outDist);
    bool ConsiderPedAsDucking(CPed* ped);
    void CopyCameraMatrixToRWCam(bool bUpdateMatrix);
    void DealWithMirrorBeforeConstructRenderList(bool bActiveMirror, CVector mirrorNormal, float mirrorV, CMatrix* matMirror);
    void DeleteCutSceneCamDataMemory();
    void DrawBordersForWideScreen();

    void Enable1rstPersonCamCntrlsScript();
    void Enable1rstPersonWeaponsCamera();

    void Fade(float duration, eFadeFlag direction);
    void Find3rdPersonCamTargetVector(float range, CVector vecGunMuzzle, CVector& outSource, CVector& outTarget);
    float Find3rdPersonQuickAimPitch() const;
    float FindCamFOV() const;
    void FinishCutscene();

    bool GetArrPosForVehicleType(eVehicleType type, int32_t& arrPos);
    uint32_t GetCutSceneFinishTime();
    [[nodiscard]] bool GetFading() const;
    [[nodiscard]] int32_t GetFadingDirection() const;
    CVector* GetGameCamPosition();
    int32_t GetLookDirection() const;
    bool GetLookingForwardFirstPerson() const;
    bool GetLookingLRBFirstPerson() const;
    [[nodiscard]] float GetPositionAlongSpline() const;
    float GetRoughDistanceToGround();
    [[nodiscard]] eNameState GetScreenFadeStatus() const;
    void GetScreenRect(CRect* rect) const;
    [[nodiscard]] bool Get_Just_Switched_Status() const;
    // Added for CPad (GetPedWalk*/LookAround*/AimWeapon*); defined in CCamera.cpp.
    static bool GetUseMouse3rdPerson();

    void HandleCameraMotionForDucking(CPed* ped, CVector* source, CVector* targPosn, bool arg5);
    void HandleCameraMotionForDuckingDuringAim(CPed* ped, CVector* source, CVector* targPosn, bool arg5);
    void ImproveNearClip(CVehicle* vehicle, CPed* ped, CVector* source, CVector* targPosn);

    bool ShouldPedControlsBeRelative();
    void SetToSphereMap(float);
    float GetCutsceneBarHeight();
    int32_t GetCamDirectlyBehind();

    CCam& GetActiveCam() { return m_aCams[m_nActiveCam]; }

    /*!
    * @brief Get frustum points of the camera in world space: top left, top
    * right, bottom right, bottom left + the center (0, 0, 0). Was @notsa
    * inline in gta-reversed; demoted to a declaration (body unknown).
    * TODO: verify from decomp (no named .c in src/CCamera/).
    */
    std::array<CVector, 5> GetFrustumPoints();

    //! Get the camera's front normal (Whatever that is). Demoted from inline
    //! (CVector2D not ported yet). TODO: verify from decomp.
    CVector2D GetFrontNormal2D() const;

public:
    static CCam& GetActiveCamera(); // TODO: Replace this with `TheCamera.GetActiveCam()`

    RwMatrix* GetRwMatrix(); // demoted: was RwFrameGetMatrix(RwCameraGetFrame(m_pRwCamera))
    CMatrix& GetViewMatrix() { return m_mViewMatrix; }

    bool IsSphereVisibleInMirror(const CVector& origin, float radius); // demoted from inline
};

// TODO: static_assert(sizeof(CCamera) == 0xD78) once the CPlaceable/CEntity
// layout chain is verified (CEntity's own assert is still a TODO).

extern CCamera TheCamera; // was `extern CCamera&` (binary-address ref); port defines the object
extern bool& gbModelViewer;
extern int8_t& gbCineyCamMessageDisplayed;
extern bool& gPlayerPedVisible;
extern uint8_t& gCurCamColVars;
extern int32_t& gCameraDirection;
extern eCamMode& gCameraMode;
extern uint32_t& gLastTime2PlayerCameraWasOK;
extern uint32_t& gLastTime2PlayerCameraCollided;
extern float*& gpCamColVars;
extern float (&gCamColVars)[28][6];
// Camera feel tunables: were `static inline StaticRef<...>` members of CCamera
// in gta-reversed (binary addresses 0xB6EC10-0xB6EC2E). The port keeps them as
// plain namespace-scope globals (defined in CCamera.cpp) instead of
// class-static or file-static state, so both CCamera.cpp and CCam.cpp - which
// reads m_bUseMouse3rdPerson/m_fMouseAccel* - can share them.
extern float g_f3rdPersonCHairMultY;
extern float g_f3rdPersonCHairMultX;
extern float g_fMouseAccelVertical;
extern float g_fMouseAccelHorzntl;
extern bool  g_bUseMouse3rdPerson;
extern bool  g_bDidWeProcessAnyCinemaCam;
// NOTE: gpMadeInvisibleEntities (was StaticRef<std::array<CEntity*, 10>> @
// 0x9655A0) and gNumEntitiesSetInvisible (was StaticRef<uint32> @ 0x9655DC)
// live as file-static state in CCamera.cpp (TODO).

void CamShakeNoPos(CCamera* camera, float strength);
