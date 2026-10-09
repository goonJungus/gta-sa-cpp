// CCamera - adapted from gta-reversed for clean-room C++ build
// Method stubs. Decompiled reference: src/CCamera/*.c
// Each method below corresponds to a decompiled function - fill in from the .c file noted.

#include "CCamera.h"

#include <array>

// File-static state replacing gta-reversed's binary-address StaticRefs.
// TODO: bind to real game state when the camera subsystem goes live.
static float    s_f3rdPersonCHairMultY;   // was StaticRef<float>(0xB6EC10)
static float    s_f3rdPersonCHairMultX;   // was StaticRef<float>(0xB6EC14)
static float    s_fMouseAccelVertical;    // was StaticRef<float>(0xB6EC18)
static float    s_fMouseAccelHorzntl;     // was StaticRef<float>(0xB6EC1C)
static bool     s_bUseMouse3rdPerson;     // was StaticRef<bool>(0xB6EC2E)
static bool     s_bDidWeProcessAnyCinemaCam; // was StaticRef<bool>(0xB6EC2D)
static std::array<CEntity*, 10> s_madeInvisibleEntities; // was StaticRef @ 0x9655A0
static uint32_t s_numEntitiesSetInvisible;               // was StaticRef @ 0x9655DC

// TheCamera must be defined somewhere: CCamera.h declares it
// `extern CCamera TheCamera`, and GetActiveCamera() (above) returns
// TheCamera.GetActiveCam(). Defined here 2026-10-09; the remaining extern
// globals are still TODO (see below).
// TODO: define the remaining extern globals declared in CCamera.h
// (gbModelViewer, gbCineyCamMessageDisplayed, gPlayerPedVisible,
// gCurCamColVars, gCameraDirection, gCameraMode, gLastTime2PlayerCameraWasOK,
// gLastTime2PlayerCameraCollided, gpCamColVars, gCamColVars) here once the
// subsystem goes live.
CCamera TheCamera{};

CCamera::CCamera() {
    // TODO: src/CCamera/Constructor_0051a450.c
}

CCamera::~CCamera() {
    // TODO: src/CCamera/_dtor_CCamera_0050a870.c
    // also CCamera/_dtor_CCamera_00514010.c - two dtor .c files, verify which is scalar/vector-deleting
}

void CCamera::Init() {
    // TODO: src/CCamera/Init_005bc520.c
}

void CCamera::InitCameraVehicleTweaks() {
    // TODO: src/CCamera/InitCameraVehicleTweaks_0050a3b0.c
}

void CCamera::InitialiseScriptableComponents() {
    // TODO: src/CCamera/InitialiseScriptableComponents_0050d2d0.c
}

void CCamera::InitialiseCameraForDebugMode() {
    // TODO: src/CCamera/InitialiseCameraForDebugMode_0050af90.c
}

void CCamera::LoadPathSplines(FILE* file) {
    // TODO: src/CCamera/LoadPathSplines_005b24d0.c
    (void)file;
}

bool CCamera::IsTargetingActive() {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    return false;
}

bool CCamera::IsExtraEntityToIgnore(CEntity* entity) {
    // TODO: src/CCamera/IsExtraEntityToIgnore_0050ce80.c
    (void)entity;
    return false;
}

bool CCamera::IsItTimeForNewCamera(int32_t camSequence, int32_t startTime) {
    // TODO: src/CCamera/IsItTimeForNewcam_0051d770.c
    // .c file is named IsItTimeForNewcam (no 'era') - verify mapping
    (void)camSequence;
    (void)startTime;
    return false;
}

bool CCamera::IsSphereVisible(const CVector& origin, float radius, RwMatrix* transformMatrix) {
    // TODO: src/CCamera/IsSphereVisible_00420c40.c
    // two overloads vs IsSphereVisible_00420c40.c + IsSphereVisible_00420d40.c - verify which maps here
    (void)origin;
    (void)radius;
    (void)transformMatrix;
    return false;
}

bool CCamera::IsSphereVisible(const CVector& origin, float radius) {
    // TODO: src/CCamera/IsSphereVisible_00420d40.c
    // two overloads vs IsSphereVisible_00420c40.c + IsSphereVisible_00420d40.c - verify which maps here
    (void)origin;
    (void)radius;
    return false;
}

void CCamera::LerpFOV(float zoomInFactor, float zoomOutFactor, float timeLimit, bool bEase) {
    // TODO: src/CCamera/LerpFOV_0050d280.c
    (void)zoomInFactor;
    (void)zoomOutFactor;
    (void)timeLimit;
    (void)bEase;
}

void CCamera::Process() {
    // TODO: src/CCamera/Process_0052b730.c
}

void CCamera::ProcessWideScreenOn() {
    // TODO: src/CCamera/ProcessWideScreenOn_0050b890.c
}

void CCamera::ProcessFOVLerp(float ratio) {
    // TODO: src/CCamera/ProcessFOVLerp_0050d510.c
    // two overloads vs ProcessFOVLerp_0050d510.c + ProcessFOVLerp_00516500.c - verify which maps here
    (void)ratio;
}

void CCamera::ProcessFOVLerp() {
    // TODO: src/CCamera/ProcessFOVLerp_00516500.c
    // two overloads vs ProcessFOVLerp_0050d510.c + ProcessFOVLerp_00516500.c - verify which maps here
}

void CCamera::ProcessFade() {
    // TODO: src/CCamera/ProcessFade_0050b5d0.c
}

void CCamera::ProcessMusicFade() {
    // TODO: src/CCamera/ProcessMusicFade_0050b6d0.c
}

void CCamera::ProcessScriptedCommands() {
    // TODO: src/CCamera/ProcessScriptedCommands_00516ae0.c
}

void CCamera::ProcessShake() {
    // TODO: src/CCamera/ProcessShake_00516560.c
    // two overloads vs ProcessShake_00516560.c + ProcessShake_0051a6f0.c - verify which maps here
}

CVector* CCamera::ProcessShake(float intensity) {
    // TODO: src/CCamera/ProcessShake_0051a6f0.c
    // two overloads vs ProcessShake_00516560.c + ProcessShake_0051a6f0.c - verify which maps here
    (void)intensity;
    return nullptr;
}

void CCamera::ProcessVectorMoveLinear() {
    // TODO: src/CCamera/ProcessVectorMoveLinear_0050d430.c
    // two overloads vs ProcessVectorMoveLinear_0050d430.c + ProcessVectorMoveLinear_005164a0.c - verify which maps here
}

void CCamera::ProcessVectorMoveLinear(float ratio) {
    // TODO: src/CCamera/ProcessVectorMoveLinear_005164a0.c
    // two overloads vs ProcessVectorMoveLinear_0050d430.c + ProcessVectorMoveLinear_005164a0.c - verify which maps here
    (void)ratio;
}

void CCamera::ProcessVectorTrackLinear() {
    // TODO: src/CCamera/ProcessVectorTrackLinear_0050d350.c
    // two overloads vs ProcessVectorTrackLinear_0050d350.c + ProcessVectorTrackLinear_00516440.c - verify which maps here
}

void CCamera::ProcessVectorTrackLinear(float ratio) {
    // TODO: src/CCamera/ProcessVectorTrackLinear_00516440.c
    // two overloads vs ProcessVectorTrackLinear_0050d350.c + ProcessVectorTrackLinear_00516440.c - verify which maps here
    (void)ratio;
}

void CCamera::ProcessObbeCinemaCameraBoat() {
    // TODO: src/CCamera/ProcessObbeCinemaCameraBoat_00526e20.c
}

void CCamera::ProcessObbeCinemaCameraCar() {
    // TODO: src/CCamera/ProcessObbeCinemaCameraCar_005267c0.c
}

void CCamera::ProcessObbeCinemaCameraHeli() {
    // TODO: src/CCamera/ProcessObbeCinemaCameraHeli_00526ae0.c
}

void CCamera::ProcessObbeCinemaCameraPed() {
    // TODO: src/CCamera/ProcessObbeCinemaCameraPed_0050b880.c
}

void CCamera::ProcessObbeCinemaCameraPlane() {
    // TODO: src/CCamera/ProcessObbeCinemaCameraPlane_00526c80.c
}

void CCamera::ProcessObbeCinemaCameraTrain() {
    // TODO: src/CCamera/ProcessObbeCinemaCameraTrain_00526950.c
}

void CCamera::DontProcessObbeCinemaCamera() {
    // TODO: src/CCamera/DontProcessObbeCinemaCamera_0050ab40.c
    // static
}

void CCamera::Restore() {
    // TODO: src/CCamera/Restore_0050b930.c
}

void CCamera::RestoreCameraAfterMirror() {
    // TODO: src/CCamera/RestoreCameraAfterMirror_0051a5a0.c
}

void CCamera::RestoreWithJumpCut() {
    // TODO: src/CCamera/RestoreWithJumpCut_0050bab0.c
}

void CCamera::RenderMotionBlur() const {
    // TODO: src/CCamera/RenderMotionBlur_0050b8f0.c
    // const method
}

void CCamera::ResetDuckingSystem(CPed* ped) {
    // TODO: src/CCamera/ResetDuckingSystem_0050cef0.c
    (void)ped;
}

void CCamera::SetCamCutSceneOffSet(const CVector& offset) {
    // TODO: src/CCamera/SetCamCutSceneOffSet_0050bd20.c
    (void)offset;
}

void CCamera::SetCamPositionForFixedMode(const CVector& fixedModeSource, const CVector& fixedModeUpOffset) {
    // TODO: src/CCamera/SetCamPositionForFixedMode_0050bec0.c
    (void)fixedModeSource;
    (void)fixedModeUpOffset;
}

void CCamera::SetCameraDirectlyBehindForFollowPed_CamOnAString() {
    // TODO: src/CCamera/SetCameraDirectlyBehindForFollowPed_CamOnAString_0050bd40.c
}

void CCamera::SetCameraDirectlyInFrontForFollowPed_CamOnAString() {
    // TODO: src/CCamera/SetCameraDirectlyInFrontForFollowPed_CamOnAString_0050bd70.c
}

void CCamera::SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString(CPed* targetPed) {
    // TODO: src/CCamera/SetCameraDirectlyBehindForFollowPed_ForAPed_CamOnAString_0050bda0.c
    (void)targetPed;
}

void CCamera::SetCameraDirectlyInFrontForFollowPed_ForAPed_CamOnAString(CPed* targetPed) {
    // TODO: src/CCamera/SetCameraDirectlyInFrontForFollowPed_ForAPed_CamOnAString_0050be30.c
    (void)targetPed;
}

void CCamera::SetCameraUpForMirror() {
    // TODO: src/CCamera/SetCameraUpForMirror_0051a560.c
}

void CCamera::SetFadeColour(uint8_t red, uint8_t green, uint8_t blue) {
    // TODO: src/CCamera/SetFadeColour_0050bf00.c
    (void)red;
    (void)green;
    (void)blue;
}

void CCamera::SetMotionBlur(uint8_t red, uint8_t green, uint8_t blue, int32_t value, eMotionBlurType blurType) {
    // TODO: src/CCamera/SetMotionBlur_0050bf40.c
    (void)red;
    (void)green;
    (void)blue;
    (void)value;
    (void)blurType;
}

void CCamera::SetMotionBlurAlpha(int32_t alpha) {
    // TODO: src/CCamera/SetMotionBlurAlpha_0050bf80.c
    (void)alpha;
}

void CCamera::SetNearClipBasedOnPedCollision(float arg2) {
    // TODO: src/CCamera/SetNearClipBasedOnPedCollision_0050cb90.c
    (void)arg2;
}

void CCamera::SetNearClipScript(float nearClip) {
    // TODO: src/CCamera/SetNearClipScript_0050bf90.c
    (void)nearClip;
}

void CCamera::SetNewPlayerWeaponMode(eCamMode mode, int16_t maxZoom, int16_t minZoom) {
    // TODO: src/CCamera/SetNewPlayerWeaponMode_0050bfb0.c
    (void)mode;
    (void)maxZoom;
    (void)minZoom;
}

void CCamera::SetParametersForScriptInterpolation(float interpolationToStopMoving, float interpolationToCatchUp, uint32_t timeForInterpolation) {
    // TODO: src/CCamera/SetParametersForScriptInterpolation_0050c030.c
    (void)interpolationToStopMoving;
    (void)interpolationToCatchUp;
    (void)timeForInterpolation;
}

void CCamera::SetPercentAlongCutScene(float percent) {
    // TODO: src/CCamera/SetPercentAlongCutScene_0050c070.c
    (void)percent;
}

void CCamera::SetRwCamera(RwCamera* camera) {
    // TODO: src/CCamera/SetRwCamera_0050c100.c
    (void)camera;
}

void CCamera::SetWideScreenOff() {
    // TODO: src/CCamera/SetWideScreenOff_0050c150.c
}

void CCamera::SetWideScreenOn() {
    // TODO: src/CCamera/SetWideScreenOn_0050c140.c
}

void CCamera::SetZoomValueCamStringScript(int16_t zoomMode) {
    // TODO: src/CCamera/SetZoomValueCamStringScript_0050c1b0.c
    (void)zoomMode;
}

void CCamera::SetZoomValueFollowPedScript(int16_t zoomMode) {
    // TODO: src/CCamera/SetZoomValueFollowPedScript_0050c160.c
    (void)zoomMode;
}

void CCamera::SetCamCollisionVarDataSet(int32_t index) {
    // TODO: src/CCamera/SetCamCollisionVarDataSet_0050cb60.c
    // static
    (void)index;
}

void CCamera::SetColVarsAimWeapon(int32_t aimingType) {
    // TODO: src/CCamera/SetColVarsAimWeapon_0050cbf0.c
    // static
    (void)aimingType;
}

void CCamera::SetColVarsPed(ePedType pedType, int32_t nCamPedZoom) {
    // TODO: src/CCamera/SetColVarsPed_0050cc50.c
    // static
    (void)pedType;
    (void)nCamPedZoom;
}

void CCamera::SetColVarsVehicle(eVehicleType vehicleType, int32_t camVehicleZoom) {
    // TODO: src/CCamera/SetColVarsVehicle_0050cca0.c
    // static
    (void)vehicleType;
    (void)camVehicleZoom;
}

void CCamera::StartCooperativeCamMode() {
    // TODO: src/CCamera/StartCooperativeCamMode_0050c260.c
}

void CCamera::StopCooperativeCamMode() {
    // TODO: src/CCamera/StopCooperativeCamMode_0050c270.c
}

void CCamera::StartTransition(eCamMode newCamMode) {
    // TODO: src/CCamera/StartTransition_00515200.c
    (void)newCamMode;
}

void CCamera::StartTransitionWhenNotFinishedInter(eCamMode newCamMode) {
    // TODO: src/CCamera/StartTransitionWhenNotFinishedInter_00515bc0.c
    (void)newCamMode;
}

void CCamera::StoreValuesDuringInterPol(CVector* sourceDuringInter, CVector* targetDuringInter, CVector* upDuringInter, float* FOVDuringInter) {
    // TODO: src/CCamera/StoreValuesDuringInterPol_0050c290.c
    (void)sourceDuringInter;
    (void)targetDuringInter;
    (void)upDuringInter;
    (void)FOVDuringInter;
}

void CCamera::TakeControl(CEntity* target, eCamMode modeToGoTo, eSwitchType switchType, int32_t whoIsInControlOfTheCamera) {
    // TODO: src/CCamera/TakeControl_0050c7c0.c
    (void)target;
    (void)modeToGoTo;
    (void)switchType;
    (void)whoIsInControlOfTheCamera;
}

void CCamera::TakeControlNoEntity(const CVector& fixedModeVector, eSwitchType switchType, int32_t whoIsInControlOfTheCamera) {
    // TODO: src/CCamera/TakeControlNoEntity_0050c8b0.c
    (void)fixedModeVector;
    (void)switchType;
    (void)whoIsInControlOfTheCamera;
}

void CCamera::TakeControlAttachToEntity(CEntity* target, CEntity* attached, CVector* attachedCamOffset, CVector* attachedCamLookAt, float attachedCamAngle, eSwitchType switchType, int32_t whoIsInControlOfTheCamera) {
    // TODO: src/CCamera/TakeControlAttachToEntity_0050c910.c
    (void)target;
    (void)attached;
    (void)attachedCamOffset;
    (void)attachedCamLookAt;
    (void)attachedCamAngle;
    (void)switchType;
    (void)whoIsInControlOfTheCamera;
}

void CCamera::TakeControlWithSpline(eSwitchType switchType) {
    // TODO: src/CCamera/TakeControlWithSpline_0050cae0.c
    (void)switchType;
}

bool CCamera::TryToStartNewCamMode(int32_t camSequence) {
    // TODO: src/CCamera/TryToStartNewCamMode_0051e560.c
    (void)camSequence;
    return false;
}

void CCamera::UpdateAimingCoors(const CVector& aimingTargetCoors) {
    // TODO: src/CCamera/UpdateAimingCoors_0050cb10.c
    (void)aimingTargetCoors;
}

void CCamera::UpdateSoundDistances() {
    // TODO: src/CCamera/UpdateSoundDistances_00515bd0.c
}

void CCamera::UpdateTargetEntity() {
    // TODO: src/CCamera/UpdateTargetEntity_0050c360.c
}

bool CCamera::Using1stPersonWeaponMode() const {
    // TODO: src/CCamera/Using1stPersonWeaponMode_0050bff0.c
    // const method
    return false;
}

bool CCamera::VectorMoveRunning() const {
    // TODO: src/CCamera/VectorMoveRunning_004748a0.c
    // const method
    return false;
}

void CCamera::VectorMoveLinear(CVector* to, CVector* from, float duration, bool bMoveLinearWithEase) {
    // TODO: src/CCamera/VectorMoveLinear_0050d160.c
    (void)to;
    (void)from;
    (void)duration;
    (void)bMoveLinearWithEase;
}

bool CCamera::VectorTrackRunning() const {
    // TODO: src/CCamera/VectorTrackRunning_00474870.c
    // const method
    return false;
}

void CCamera::VectorTrackLinear(CVector* to, CVector* from, float duration, bool bEase) {
    // TODO: src/CCamera/VectorTrackLinear_0050d1d0.c
    (void)to;
    (void)from;
    (void)duration;
    (void)bEase;
}

void CCamera::AllowShootingWith2PlayersInCar(bool bAllow) {
    // TODO: src/CCamera/AllowShootingWith2PlayersInCar_0050c280.c
    (void)bAllow;
}

void CCamera::ApplyVehicleCameraTweaks(CVehicle* vehicle) {
    // TODO: src/CCamera/ApplyVehicleCameraTweaks_0050a480.c
    (void)vehicle;
}

void CCamera::AvoidTheGeometry(const CVector* arg2, const CVector* arg3, CVector* arg4, float FOV) {
    // TODO: src/CCamera/AvoidTheGeometry_00514030.c
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)FOV;
}

void CCamera::CalculateDerivedValues(bool bForMirror, bool bOriented) {
    // TODO: src/CCamera/CalculateDerivedValues_005150e0.c
    (void)bForMirror;
    (void)bOriented;
}

void CCamera::CalculateFrustumPlanes(bool bForMirror) {
    // TODO: src/CCamera/CalculateFrustumPlanes_00514d60.c
    (void)bForMirror;
}

float CCamera::CalculateGroundHeight(eGroundHeightType type) {
    // TODO: src/CCamera/CalculateGroundHeight_00514b80.c
    (void)type;
    return {};
}

void CCamera::CalculateMirroredMatrix(CVector posn, float mirrorV, CMatrix* camMatrix, CMatrix* mirrorMatrix) {
    // TODO: src/CCamera/CalculateMirroredMatrix_0050b380.c
    (void)posn;
    (void)mirrorV;
    (void)camMatrix;
    (void)mirrorMatrix;
}

void CCamera::CamControl() {
    // TODO: src/CCamera/CamControl_00527fa0.c
}

void CCamera::AddShake(float duration, float a2, float a3, float a4, float a5) {
    // TODO: src/CCamera/AddShake_00516400.c
    (void)duration;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
}

void CCamera::AddShakeSimple(float duration, int32_t type, float intensity) {
    // TODO: src/CCamera/AddShakeSimple_0050d240.c
    (void)duration;
    (void)type;
    (void)intensity;
}

void CCamera::CamShake(float strength, CVector from) {
    // TODO: src/CCamera/CamShake_0050a9f0.c
    (void)strength;
    (void)from;
}

void CCamera::CameraColDetAndReact(CVector* source, CVector* target) {
    // TODO: src/CCamera/CameraColDetAndReact_00520190.c
    (void)source;
    (void)target;
}

void CCamera::CameraGenericModeSpecialCases(CPed* targetPed) {
    // TODO: src/CCamera/CameraGenericModeSpecialCases_0050cd30.c
    (void)targetPed;
}

void CCamera::CameraPedAimModeSpecialCases(CPed* ped) {
    // TODO: src/CCamera/CameraPedAimModeSpecialCases_0050cda0.c
    (void)ped;
}

void CCamera::CameraPedModeSpecialCases() {
    // TODO: src/CCamera/CameraPedModeSpecialCases_0050cd80.c
}

void CCamera::CameraVehicleModeSpecialCases(CVehicle* vehicle) {
    // TODO: src/CCamera/CameraVehicleModeSpecialCases_0050cde0.c
    (void)vehicle;
}

void CCamera::ClearPlayerWeaponMode() {
    // TODO: src/CCamera/ClearPlayerWeaponMode_0050ab10.c
}

bool CCamera::ConeCastCollisionResolve(const CVector& pos, const CVector& lookAt, CVector& outDest, float rad, float minDist, float& outDist) {
    // TODO: src/CCamera/ConeCastCollisionResolve_0051a5d0.c
    (void)pos;
    (void)lookAt;
    (void)outDest;
    (void)rad;
    (void)minDist;
    (void)outDist;
    return false;
}

bool CCamera::ConsiderPedAsDucking(CPed* ped) {
    // TODO: src/CCamera/ConsiderPedAsDucking_0050ceb0.c
    (void)ped;
    return false;
}

void CCamera::CopyCameraMatrixToRWCam(bool bUpdateMatrix) {
    // TODO: src/CCamera/CopyCameraMatrixToRWCam_0050afa0.c
    (void)bUpdateMatrix;
}

void CCamera::DealWithMirrorBeforeConstructRenderList(bool bActiveMirror, CVector mirrorNormal, float mirrorV, CMatrix* matMirror) {
    // TODO: src/CCamera/DealWithMirrorBeforeConstructRenderList_0050b510.c
    (void)bActiveMirror;
    (void)mirrorNormal;
    (void)mirrorV;
    (void)matMirror;
}

void CCamera::DeleteCutSceneCamDataMemory() {
    // TODO: src/CCamera/DeleteCutSceneCamDataMemory_005b24a0.c
}

void CCamera::DrawBordersForWideScreen() {
    // TODO: src/CCamera/DrawBordersForWideScreen_00514860.c
}

void CCamera::Enable1rstPersonCamCntrlsScript() {
    // TODO: src/CCamera/Enable1rstPersonCamCntrlsScript_0050ac00.c
}

void CCamera::Enable1rstPersonWeaponsCamera() {
    // TODO: src/CCamera/Enable1rstPersonWeaponsCamera_0050ac10.c
}

void CCamera::Fade(float duration, eFadeFlag direction) {
    // TODO: src/CCamera/Fade_0050ac20.c
    (void)duration;
    (void)direction;
}

void CCamera::Find3rdPersonCamTargetVector(float range, CVector vecGunMuzzle, CVector& outSource, CVector& outTarget) {
    // TODO: src/CCamera/Find3rdPersonCamTargetVector_00514970.c
    (void)range;
    (void)vecGunMuzzle;
    (void)outSource;
    (void)outTarget;
}

float CCamera::Find3rdPersonQuickAimPitch() const {
    // TODO: src/CCamera/Find3rdPersonQuickAimPitch_0050ad40.c
    // const method
    return {};
}

float CCamera::FindCamFOV() const {
    // TODO: src/CCamera/FindCamFOV_0050ad20.c
    // const method
    return {};
}

void CCamera::FinishCutscene() {
    // TODO: src/CCamera/FinishCutscene_00514950.c
}

bool CCamera::GetArrPosForVehicleType(eVehicleType type, int32_t& arrPos) {
    // TODO: src/CCamera/GetArrPosForVehicleType_0050af00.c
    (void)type;
    (void)arrPos;
    return false;
}

uint32_t CCamera::GetCutSceneFinishTime() {
    // TODO: src/CCamera/GetCutSceneFinishTime_0050ad90.c
    return {};
}

bool CCamera::GetFading() const {
    // TODO: src/CCamera/GetFading_0050ade0.c
    // const method
    return false;
}

int32_t CCamera::GetFadingDirection() const {
    // TODO: src/CCamera/GetFadingDirection_0050adf0.c
    // const method
    return {};
}

CVector* CCamera::GetGameCamPosition() {
    // TODO: src/CCamera/GetGameCamPosition_0050ae50.c
    return nullptr;
}

// Reads the file-static s_bUseMouse3rdPerson above (was StaticRef<bool>(0xB6EC2E)).
// Added for CPad (GetPedWalk*/LookAround*/AimWeapon*); see BUILD_NOTES.md.
bool CCamera::GetUseMouse3rdPerson() {
    return s_bUseMouse3rdPerson;
}

int32_t CCamera::GetLookDirection() const {
    // TODO: src/CCamera/GetLookDirection_0050ae90.c
    // const method
    return {};
}

bool CCamera::GetLookingForwardFirstPerson() const {
    // TODO: src/CCamera/GetLookingForwardFirstPerson_0050aed0.c
    // const method
    return false;
}

bool CCamera::GetLookingLRBFirstPerson() const {
    // TODO: src/CCamera/GetLookingLRBFirstPerson_0050ae60.c
    // const method
    return false;
}

float CCamera::GetPositionAlongSpline() const {
    // TODO: src/CCamera/GetPositionAlongSpline_0050af80.c
    // const method
    return {};
}

float CCamera::GetRoughDistanceToGround() {
    // TODO: src/CCamera/GetRoughDistanceToGround_00516b00.c
    return {};
}

eNameState CCamera::GetScreenFadeStatus() const {
    // TODO: src/CCamera/GetScreenFadeStatus_0050ae20.c
    // const method
    return {};
}

void CCamera::GetScreenRect(CRect* rect) const {
    // TODO: src/CCamera/GetScreenRect_0050ab50.c
    // const method
    (void)rect;
}

bool CCamera::Get_Just_Switched_Status() const {
    // TODO: src/CCamera/Get_Just_Switched_Status_0050ae10.c
    // const method
    return false;
}

void CCamera::HandleCameraMotionForDucking(CPed* ped, CVector* source, CVector* targPosn, bool arg5) {
    // TODO: src/CCamera/HandleCameraMotionForDucking_0050cfa0.c
    (void)ped;
    (void)source;
    (void)targPosn;
    (void)arg5;
}

void CCamera::HandleCameraMotionForDuckingDuringAim(CPed* ped, CVector* source, CVector* targPosn, bool arg5) {
    // TODO: src/CCamera/HandleCameraMotionForDuckingDuringAim_0050d090.c
    (void)ped;
    (void)source;
    (void)targPosn;
    (void)arg5;
}

void CCamera::ImproveNearClip(CVehicle* vehicle, CPed* ped, CVector* source, CVector* targPosn) {
    // TODO: src/CCamera/ImproveNearClip_00516b20.c
    (void)vehicle;
    (void)ped;
    (void)source;
    (void)targPosn;
}

bool CCamera::ShouldPedControlsBeRelative() {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    return false;
}

void CCamera::SetToSphereMap(float f) {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    (void)f;
}

float CCamera::GetCutsceneBarHeight() {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    return {};
}

int32_t CCamera::GetCamDirectlyBehind() {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    return {};
}

std::array<CVector, 5> CCamera::GetFrustumPoints() {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    // was @notsa inline in gta-reversed; demoted - no named .c in src/CCamera/
    return {};
}

CVector2D CCamera::GetFrontNormal2D() const {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    // was inline (CVector2D not ported yet); demoted - no named .c in src/CCamera/
    return {};
}

RwMatrix* CCamera::GetRwMatrix() {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    // was inline RenderWare call; demoted - no named .c in src/CCamera/
    return nullptr;
}

bool CCamera::IsSphereVisibleInMirror(const CVector& origin, float radius) {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    // was inline; demoted - no named .c in src/CCamera/
    (void)origin;
    (void)radius;
    return false;
}

CCam& CCamera::GetActiveCamera() {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    // static; no named .c in src/CCamera/ - identify from unk_*.c / binary
    return TheCamera.GetActiveCam();
}

void CamShakeNoPos(CCamera* camera, float strength) {
    // TODO: no named .c in src/CCamera/ - identify from unk_*.c / binary
    // free function - identify from unk_*.c / binary
    (void)camera;
    (void)strength;
}
