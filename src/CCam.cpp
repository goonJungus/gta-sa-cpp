// CCam - adapted from gta-reversed for clean-room C++ build
// Method bodies transcribed from the decompiled sources in src/CCam/*.c
// (original addresses noted per method), cross-checked against
// gta-reversed/source/game_sa/Cam.cpp. Where the decomp and gta-reversed
// disagree on LOGIC (not just style), the decomp wins - e.g. ClipBeta:
// gta-reversed subtracts 2*PI unconditionally in the else-branch, the
// decomp only adjusts out-of-range angles.
// Ghidra-isms converted: __thiscall this-pointers -> implicit this,
// CEntity::CleanUpOldReference(e, &ref) -> e->CleanUpOldReference(&ref),
// CrossProduct(&out,&a,&b) -> out.Cross_OG(a,b), fptan/fsin/fcos -> <cmath>.

#include "CCam.h"
#include "CCamera.h" // camera tunables (g_bUseMouse3rdPerson, ...), TheCamera
#include "CWorld.h"  // FindPlayerPed / FindPlayerVehicle / ProcessLineOfSight
#include "CWeaponEffects.h" // gCrossHair, ClearCrossHairImmediately
#include "CPostEffects.h" // m_bSpeedFXUserFlagCurrentFrame
#include "CPlayerPed.h"
#include "CPed.h"
#include "CVehicle.h"
#include "CWeapon.h"
#include "CTimer.h"
#include "CPad.h"
#include "eWeaponType.h"
#include "eWeaponSkill.h"

#include <algorithm> // std::clamp
#include <cmath>     // std::sin/cos/tan/atan2/sqrt

// ============================================================================
// TODO(port): external subsystem shims (same pattern as CStreaming.cpp).
// Minimal declarations for subsystems not yet ported to cpp/. Each entry is
// verified against gta-reversed/source/game_sa and the decompiled bodies in
// src/CCam/*.c. Delete entries as their subsystem lands; do not grow this
// list. None of these introduce link-time dependencies for -fsyntax-only
// or `ar` static-library builds.
// ============================================================================

constexpr float CAM_PI = 3.141592653589793f;
constexpr float DegreesToRadians(float deg) { return deg * (CAM_PI / 180.0f); }

// --- RenderWare scene/camera: no RW layer in this build yet (see CMakeLists.txt TODO).
// Opaque declarations only; used by the DW ciney-cam finalise/cache helpers.
struct RwCamera; // (also declared in CCamera.h)
void  RwCameraSetNearClipPlane(RwCamera* camera, float nearClip);
float RwCameraGetNearClipPlane(const RwCamera* camera);
// gta-reversed `Scene` (RsGlobal). File-static stand-in until the RW layer lands.
struct RsGlobalType {
    RwCamera* m_pRwCamera{};
};
static RsGlobalType s_scene; // TODO(port): the real RsGlobal Scene

// --- CHandShaker: camera shake helper (gta-reversed/source/game_sa/HandShaker.h).
// Only Process()/m_ang/m_resultMat are touched by the camera code.
struct CHandShaker {
    CVector m_ang{};
    CVector m_lim{};
    CVector m_motion{};
    CVector m_vel{};
    CVector m_slow{};
    CMatrix m_resultMat{};
    float   m_scaleReactionMin{};
    float   m_scaleReactionMax{};
    int32_t m_twitchFreq{};
    float   m_twitchVel{};
    void Process(float degree);
};
static std::array<CHandShaker, 6> s_handShaker; // was gHandShaker @0xB6ECA0

// --- CDraw: only FadeValue/ms_fAspectRatio are touched here.
struct CDraw {
    static inline int32_t FadeValue = 0;
    static inline float   ms_fAspectRatio = 4.0f / 3.0f; // TODO(port): real aspect from RW
};

// --- CWeaponInfo: weapon stats DB (not yet ported). Only GetWeaponInfo/m_fWeaponRange used.
struct CWeaponInfo {
    float m_fWeaponRange{};
    static CWeaponInfo* GetWeaponInfo(eWeaponType weaponType, eWeaponSkill skill);
};

// --- CWorld::pIgnoreEntity: static LOS-ignore entity (gta-reversed CWorld.h).
// TODO(port): move into CWorld as a real static member when CWorld.cpp owns
// world state; the ported CWorld.h doesn't declare it yet.
static CEntity* s_pIgnoreEntity;

// ============================================================================
// File-static state (were binary-address globals in gta-reversed).
// TODO: bind to real game state when the camera subsystem goes live.
// ============================================================================
static bool s_bFirstPersonRunThisFrame; // was: extern bool& gbFirstPersonRunThisFrame

// DW ciney-cam cached settings (gta-reversed Cam.cpp StaticRefs).
static CVector  s_dwCineyCamLastPos;      // @0xB6FE8C
static CVector  s_dwCineyCamLastUp;       // @0xB6FE98
static CVector  s_dwCineyCamLastRight;    // @0xB6FEA4
static CVector  s_dwCineyCamLastFwd;      // @0xB6FEB0
static float    s_dwCineyCamLastNearClip; // @0xB6EC08
static float    s_dwCineyCamLastFov;      // @0xB6EC0C
static uint32_t s_gLastFrameProcessedDWCineyCam; // @0x8CCB9C

// IsTimeToExitThisDWCineyCamMode tables (decomp @00517400).
// NOTE: gta-reversed declares gbExitCam as std::array<bool,9> @0xB6EC5C, but
// the decomp indexes it by raw camId byte offset (camIds 0x14-0x1C reach past
// 9 entries), so this is sized to cover the observed range.
static bool     s_gbExitCam[32];        // gbExitCam
static float    s_dwCineyCamMinDist[9]; // @0x8CCBCC, indexed by (camId - 0x14)
static float    s_dwCineyCamMaxDist[9]; // @0x8CCBF0, indexed by (camId - 0x14)
static uint32_t s_dwCineyCamTimeout;    // @0x8CCBA4

// KeepTrackOfTheSpeed last-frame state (@0xB6FF5C..0xB6FF8C).
static CVector  s_lastSource;
static CVector  s_lastTarget;
static CVector  s_lastUp;
static float    s_lastBeta;
static float    s_lastAlpha;
static float    s_lastFov;
static uint32_t s_speedInitFlags;
static bool     s_resetSpeedStatics; // @0xB6F052: refresh last-frame state from current

// Lamp-post camera target distance (decomp @005161a0 reads @0x8CC8D8).
static float s_lampPostTargetDist; // TODO(port): verify initial value from binary

// Camera look-direction latch flags (decomp DAT_00b6F042/43/5F/80).
// Used by RotCamIfInFrontCar / Process_FollowCar_SA / Process_AimWeapon / CCamera::Process.
// TODO(port): identify whether these are TheCamera members or true globals.
static bool s_bCamFlag_0xB6F042; // look snap-back latch
static bool s_bCamFlag_0xB6F043; // look snap-forward latch
static bool s_bCamFlag_0xB6F05F; // transition-beta latch
static bool s_bCamFlag_0xB6F080; // beta-fix gate

// Decomp @00509be0 (CCam::unk_00509be0): wrap a radian angle into [-PI, PI).
static void WrapAnglePi(float* angle) {
    if (*angle >= CAM_PI) {
        float a = *angle;
        do {
            a -= 2.0f * CAM_PI;
        } while (a >= CAM_PI);
        *angle = a;
    }
    if (*angle < -CAM_PI) {
        float a = *angle;
        do {
            a += 2.0f * CAM_PI;
        } while (a < -CAM_PI);
        *angle = a;
    }
}

// Camera-feel smoothing helper used by RotCamIfInFrontCar and others.
// src/_global/WellBufferMe_00509ae0.c (gta-reversed keeps it file-static in Cam.cpp).
// NOTE: gta-reversed's reconstruction of the speed update (`speedSoFar +=
// abs(...)`) is wrong - it always increases the speed. The decomp moves the
// speed TOWARD the target speed; transcribed faithfully here.
static void WellBufferMe(float target, float& valueToChange, float& speedSoFar,
                         float topSpeed, float speedStep, bool isAnAngle) {
    float diff = target - valueToChange;
    if (isAnAngle) {
        while (diff >= CAM_PI)
            diff -= 2.0f * CAM_PI;
        while (diff < -CAM_PI)
            diff += 2.0f * CAM_PI;
    }
    diff *= topSpeed;
    const float step = std::abs(diff - speedSoFar) * CTimer::GetTimeStep() * speedStep;
    speedSoFar = (diff <= speedSoFar) ? speedSoFar - step : speedSoFar + step;
    // Clamp so the speed never overshoots the target speed.
    if (diff > 0.0f) {
        if (speedSoFar > diff)
            speedSoFar = diff;
    } else if (diff < 0.0f) {
        if (speedSoFar < diff)
            speedSoFar = diff;
    }
    valueToChange += std::min(CTimer::GetTimeStep(), 10.0f) * speedSoFar;
}

// Decomp idiom: make sure the entity has an allocated, current matrix.
static void EnsureEntityMatrix(CEntity* entity) {
    if (!entity->m_matrix) {
        entity->AllocateMatrix();
        entity->m_placement.UpdateMatrix(entity->m_matrix);
    }
}

CCam::CCam() {
    // src/CCam/Constructor_00517730.c
    Init();
}

void CCam::Init() {
    // src/CCam/Init_0050e490.c (cross-checked vs gta-reversed Cam.cpp)
    m_vecFront = CVector(0.0f, 0.0f, -1.0f);
    m_vecUp = CVector(0.0f, 0.0f, 1.0f);
    m_nMode = MODE_FOLLOWPED;
    m_bRotating = false;
    m_nDoCollisionChecksOnFrameNum = 1;
    m_nDoCollisionCheckEveryNumOfFrames = 9;
    m_nFrameNumWereAt = 0;
    m_bCollisionChecksOn = true;
    m_fRealGroundDist = 0.0f;
    m_fBetaSpeed = 0.0f;
    m_fAlphaSpeed = 0.0f;
    m_fCameraHeightMultiplier = 0.75f;
    m_fMaxRoleAngle = DegreesToRadians(20.0f);
    m_fDistance = 30.0f;
    m_fDistanceSpeed = 0.0f;
    m_pLastCarEntered = nullptr;
    m_pLastPedLookedAt = nullptr;
    m_bResetStatics = true;
    m_fHorizontalAngle = 0.0f;
    m_fTilt = 0.0f;
    m_fTiltSpeed = 0.0f;
    m_bFixingBeta = false;
    m_fCaMinDistance = 0.0f;
    m_fCaMaxDistance = 0.0f;
    m_bLookingBehind = false;
    m_bLookingLeft = false;
    m_bLookingRight = false;
    m_fPlayerInFrontSyphonAngleOffSet = DegreesToRadians(20.0f);
    m_fSyphonModeTargetZOffSet = 0.5f;
    m_fRadiusForDead = 1.5f;
    m_nDirectionWasLooking = 3; // TODO: enum (LOOKING_FORWARD)
    m_bLookBehindCamWasInFront = false;
    m_fRoll = 0.0f;
    m_fRollSpeed = 0.0f;
    m_fCloseInPedHeightOffset = 0.0f;
    m_fCloseInPedHeightOffsetSpeed = 0.0f;
    m_fCloseInCarHeightOffset = 0.0f;
    m_fCloseInCarHeightOffsetSpeed = 0.0f;
    m_fPedBetweenCameraHeightOffset = 0.0f;
    m_fTargetBeta = 0.0f;
    m_fBufferedTargetBeta = 0.0f;
    m_fBufferedTargetOrientation = 0.0f;
    m_fBufferedTargetOrientationSpeed = 0.0f;
    m_fDimensionOfHighestNearCar = 0.0f;
    m_fBeta_Targeting = 0.0f;
    m_fX_Targetting = 0.0f;
    m_fY_Targetting = 0.0f;
    m_pCarWeAreFocussingOn = nullptr;
    m_pCarWeAreFocussingOnI = nullptr;
    m_fCamBumpedHorz = 1.0f;
    m_fCamBumpedVert = 0.0f;
    m_nCamBumpedTime = 0;
    for (int i = 0; i < 4; ++i) {
        m_anTargetHistoryTime[i] = 0;
        m_avecTargetHistoryPos[i] = CVector{};
    }
    m_nCurrentHistoryPoints = 0;
    gPlayerPedVisible = true;
    gbCineyCamMessageDisplayed = 2; // TODO: enum
    gCameraDirection = 3;          // TODO: enum
    gCameraMode = static_cast<eCamMode>(-1);
    gLastTime2PlayerCameraWasOK = 0;
    gLastTime2PlayerCameraCollided = 0;
    TheCamera.m_bCinemaCamera = false;
}

void CCam::CacheLastSettingsDWCineyCam() {
    // src/CCam/CacheLastSettingsDWCineyCam_0050d7a0.c
    s_dwCineyCamLastUp = m_vecUp;
    CVector right;
    right.Cross_OG(m_vecFront, m_vecUp); // CrossProduct(&right, &m_vecFront, &m_vecUp)
    s_dwCineyCamLastRight = right;
    s_dwCineyCamLastFwd = m_vecFront;
    s_dwCineyCamLastFov = m_fFOV;
    s_dwCineyCamLastNearClip = RwCameraGetNearClipPlane(s_scene.m_pRwCamera); // *(Scene.m_pRwCamera + 0x80)
    s_dwCineyCamLastPos = m_vecSource;
}

void CCam::DoCamBump(float horizontal, float vertical) {
    // src/CCam/DoCamBump_0050cb30.c
    m_fCamBumpedHorz = horizontal;
    m_fCamBumpedVert = vertical;
    m_nCamBumpedTime = CTimer::GetTimeInMS(); // decomp: CTimer::m_snTimeInMilliseconds
}

void CCam::Finalise_DW_CineyCams(const CVector& src, const CVector& dest, float roll, float fov, float nearClip, float shakeDegree) {
    // src/CCam/Finalise_DW_CineyCams_0050dd70.c
    // (cross-checked vs gta-reversed Cam.cpp; the decomp's hand-shaker matrix
    // application goes through an opaque helper (CMatrix::unk_0059c810), so the
    // reconstructed m_resultMat.TransformVector form is used with a note.)
    (void)nearClip; // decomp hardcodes 0.4f below; the parameter is ignored
    m_vecFront = (dest - src).Normalized();
    m_vecSource = src;

    // Build an up vector from the roll, then orthonormalise (twice, as the decomp does).
    for (int pass = 0; pass < 2; ++pass) {
        const CVector rollRef(std::sin(roll), 0.0f, std::cos(roll));
        CVector rightDir = m_vecFront.Cross(rollRef).Normalized();
        m_vecUp = rightDir.Cross(m_vecFront);
        if (m_vecFront.x == 0.0f && m_vecFront.y == 0.0f) {
            m_vecFront.x = m_vecFront.y = 0.0001f;
        }
        rightDir = m_vecFront.Cross(m_vecUp).Normalized();
        m_vecUp = rightDir.Cross(m_vecFront);
    }

    m_fFOV = fov;
    RwCameraSetNearClipPlane(s_scene.m_pRwCamera, 0.4f); // decomp: 0x3ecccccd; gta-reversed notes nearClip was meant to be used
    CacheLastSettingsDWCineyCam();
    s_gLastFrameProcessedDWCineyCam = CTimer::GetFrameCounter();

    s_handShaker[0].Process(shakeDegree);
    // Decomp applies the hand-shaker rotation via CMatrix::unk_0059c810; the
    // reconstructed equivalent:
    m_vecFront = s_handShaker[0].m_resultMat.TransformVector(m_vecFront);
    m_vecFront.Normalise();

    // Re-orthonormalise after the shake (decomp repeats the roll/up rebuild
    // with the shaken front vector; roll is unchanged here).
    for (int pass = 0; pass < 2; ++pass) {
        const CVector rollRef(std::sin(roll), 0.0f, std::cos(roll));
        CVector rightDir = m_vecFront.Cross(rollRef).Normalized();
        m_vecUp = rightDir.Cross(m_vecFront);
        if (m_vecFront.x == 0.0f && m_vecFront.y == 0.0f) {
            m_vecFront.x = m_vecFront.y = 0.0001f;
        }
        rightDir = m_vecFront.Cross(m_vecUp).Normalized();
        m_vecUp = rightDir.Cross(m_vecFront);
    }
}

void CCam::GetCoreDataForDWCineyCamMode(CEntity*& entity, CVehicle*& vehicle, CVector& dest, CVector& src, CVector& targetUp, CVector& targetRight, CVector& targetFwd, CVector& targetVel, float& targetSpeed, CVector& targetAngVel, float& targetAngSpeed, CColSphere& colSphere) {
    // No named .c in src/CCam/; body from gta-reversed Cam.cpp (0x517130).
    // TODO: verify against the binary when a named decomp surfaces.
    entity         = m_pCamTargetEntity;
    vehicle        = entity->AsVehicle();
    dest           = entity->GetPosition();
    src            = s_dwCineyCamLastPos;
    targetUp       = entity->GetUpVector();
    targetRight    = entity->GetRightVector();
    targetFwd      = entity->GetForwardVector();
    targetVel      = entity->AsPhysical()->GetMoveSpeed();
    targetSpeed    = targetVel.Magnitude();
    targetAngVel   = entity->AsPhysical()->GetTurnSpeed();
    targetAngSpeed = targetAngVel.Magnitude();

    colSphere.Set(
        entity->GetModelInfo()->GetColModel()->GetBoundRadius(),
        entity->GetBoundCentre(),
        static_cast<eColSurfaceType>(SURFACE_DEFAULT),
        0
    );
}

// --- CTimeCycle: water colour for ApplyUnderwaterMotionBlur (not yet ported).
struct CTimeCycle {
    static float GetWaterRed();
    static float GetWaterGreen();
    static float GetWaterBlue();
};

void CCam::GetLookFromLampPostPos(CEntity* target, CPed* cop, const CVector& vecTarget, const CVector& vecSource) {
    // src/CCam/GetLookFromLampPostPos_005161a0.c
    // NOTE: vecSource is an OUT parameter here (despite the const ref); the
    // header signature is kept as-is. Writes go through a local copy.
    (void)target;
    (void)cop;
    CVector& outSource = const_cast<CVector&>(vecSource);

    int16_t numFound[2] = {};
    CEntity* found[16] = {};
    CWorld::FindObjectsInRange(vecTarget, 30.0f, true, numFound, 15, found,
                               false, false, false, true, true);

    CEntity* best = nullptr;
    float bestDistDiff = 10000.0f;
    for (int16_t i = 0; i < numFound[0]; ++i) {
        CEntity* entity = found[i];
        if (!entity->m_bIsStatic && !entity->m_bIsStaticWaitingForCollision)
            continue;
        EnsureEntityMatrix(entity);
        if (entity->m_matrix->GetUp().z <= 0.9f)
            continue;
        if (!IsLampPost(static_cast<eModelID>(entity->GetModelIndex())))
            continue;
        const CVector& pos = entity->GetPosition();
        const float dx = pos.x - vecTarget.x;
        const float dy = pos.y - vecTarget.y;
        const float dist2D = std::sqrt(dx * dx + dy * dy);
        if (dist2D <= 5.0f)
            continue;
        if (std::abs(s_lampPostTargetDist - dist2D) >= bestDistDiff)
            continue;
        // Lamp-top world position = col-model bbox max transformed by the entity matrix.
        CVector lampTop = entity->m_matrix->TransformPoint(
            entity->GetModelInfo()->GetColModel()->GetBoundingBox().m_vecMax);
        CVector dir = (lampTop - vecTarget).Normalized() + vecTarget;
        if (CWorld::GetIsLineOfSightClear(lampTop, dir, true, false, false, false, false, true, true)) {
            best = entity;
            bestDistDiff = std::abs(s_lampPostTargetDist - dist2D);
            outSource = lampTop;
        }
    }
    // Decomp returns here either way; best==nullptr leaves vecSource untouched.
}

void CCam::GetVectorsReadyForRW() {
    // src/CCam/GetVectorsReadyForRW_00509ce0.c
    m_vecFront.Normalise();
    if (m_vecFront.x == 0.0f && m_vecFront.y == 0.0f) {
        m_vecFront.x = m_vecFront.y = 0.0001f;
    }
    const CVector right = m_vecFront.Cross(CVector(0.0f, 0.0f, 1.0f)).Normalized();
    m_vecUp = right.Cross(m_vecFront);
}

void CCam::Get_TwoPlayer_AimVector(CVector& out) {
    // src/CCam/Get_TwoPlayer_AimVector_00513e40.c
    // NOTE: gta-reversed marks this "not tested" and its reconstruction
    // differs (it multiplies X*Y into the right term); the decomp below wins.
    CPlayerPed* player = FindPlayerPed(0); // CWorld::Players[0].m_pPed
    if (player->m_pVehicle && player->m_pVehicle->m_pDriver != player) {
        player = FindPlayerPed(1); // CWorld::Players[1].m_pPed
    }
    const CWeaponInfo* weaponInfo = CWeaponInfo::GetWeaponInfo(
        player->GetActiveWeapon().m_Type, player->GetWeaponSkill());

    const CEntity* target = CWeapon::FindNearestTargetEntityWithScreenCoors(
        m_fX_Targetting, m_fY_Targetting,
        weaponInfo->m_fWeaponRange + weaponInfo->m_fWeaponRange,
        player->GetPosition(), nullptr, nullptr);

    if (target) {
        out = target->GetPosition() - m_vecSource;
    } else {
        const CVector right = m_vecFront.Cross(m_vecUp); // CrossProduct(&right, &m_vecFront, &m_vecUp)
        const float tanFov = std::tan(m_fFOV * (CAM_PI / 360.0f)); // fptan(m_fFOV * 0.008726646)
        // Decomp: out = (X*tan)*right + front - ((tan/aspect)*Y)*up
        out = right * (m_fX_Targetting * tanFov) + m_vecFront
            - m_vecUp * ((tanFov / CDraw::ms_fAspectRatio) * m_fY_Targetting);
    }
    out.Normalise();
}

bool CCam::IsTimeToExitThisDWCineyCamMode(int32_t camId, const CVector& src, const CVector& dst, float t, bool lineOfSightCheck) {
    // src/CCam/IsTimeToExitThisDWCineyCamMode_00517400.c
    // (gta-reversed leaves this NOTSA_UNREACHABLE; the decomp is the only source.)
    (void)t;
    if (s_gbExitCam[camId])
        return true;
    const float dist = (dst - src).Magnitude();
    // Decomp condition: (dist < min) || ((dist < max) == (dist == max)).
    // (dist<max)==(dist==max) is only true when dist>max, so: out of [min,max].
    const bool outOfRange = dist < s_dwCineyCamMinDist[camId - 0x14]
        || dist > s_dwCineyCamMaxDist[camId - 0x14];
    bool blocked = false;
    if (lineOfSightCheck) {
        s_pIgnoreEntity = m_pCamTargetEntity; // CWorld::pIgnoreEntity (TODO: real member)
        CColPoint colPoint;
        CEntity* outEntity = nullptr;
        blocked = CWorld::ProcessLineOfSight(dst, src, colPoint, outEntity,
                                            true, true, false, false, false,
                                            false, false, false);
        s_pIgnoreEntity = nullptr;
    }
    if (camId > 0x13 && camId < 0x1D
        && (outOfRange || (blocked || s_dwCineyCamTimeout < CTimer::GetTimeInMS()))) {
        return true;
    }
    return false;
}

void CCam::KeepTrackOfTheSpeed(const CVector& a, const CVector& b, const CVector& c, const float& d, const float& e, const float& f) {
    // src/CCam/KeepTrackOfTheSpeed_00509df0.c
    // a=source, b=target, c=up, d=alpha, e=beta, f=fov. Tracks per-frame deltas.
    if ((s_speedInitFlags & 1) == 0) {
        s_lastSource = a;
        s_speedInitFlags |= 1;
    }
    if ((s_speedInitFlags & 2) == 0) {
        s_lastTarget = b;
        s_speedInitFlags |= 2;
    }
    if ((s_speedInitFlags & 4) == 0) {
        s_lastUp = c;
        s_speedInitFlags |= 4;
    }
    float lastBeta = s_lastBeta;
    if ((s_speedInitFlags & 8) == 0) {
        s_speedInitFlags |= 8;
        lastBeta = e; // NOTE: decomp does NOT store e here on first call (stores at the end)
    }
    if ((s_speedInitFlags & 0x10) == 0) {
        s_lastAlpha = d;
        s_speedInitFlags |= 0x10;
    }
    float lastFov = s_lastFov;
    if ((s_speedInitFlags & 0x20) == 0) {
        s_speedInitFlags |= 0x20;
        lastFov = f; // NOTE: same first-call quirk as beta
    }
    if (s_resetSpeedStatics) {
        s_lastSource = a;
        s_lastTarget = b;
        s_lastUp = c;
    }
    m_vecSourceSpeedOverOneFrame = a - s_lastSource;
    m_vecTargetSpeedOverOneFrame = b - s_lastTarget;
    m_vecUpOverOneFrame = c - s_lastUp;
    m_fFovSpeedOverOneFrame = f - lastFov;
    m_fBetaSpeedOverOneFrame = e - lastBeta;
    WrapAnglePi(&m_fBetaSpeedOverOneFrame);  // unk_00509be0
    m_fAlphaSpeedOverOneFrame = d - s_lastAlpha;
    WrapAnglePi(&m_fAlphaSpeedOverOneFrame); // unk_00509be0
    s_lastSource = a;
    s_lastTarget = b;
    s_lastUp = c; // decomp re-stores via a redundant EDX copy; single store here
    s_lastBeta = e;
    s_lastAlpha = d;
    s_lastFov = f;
}

void CCam::RotCamIfInFrontCar(const CVector& v, float f) {
    // src/CCam/RotCamIfInFrontCar_0050a4f0.c
    // NOTE: the decomp reads the vehicle's move speed through Ghidra's
    // `this_00[1].m_placement` idiom (vehicle+0x38+0x04+field); the _types.h
    // layout decodes those slots as m_vecMoveSpeed.{x,y,z}. Transcribed with
    // the real member.
    CVehicle* vehicle = static_cast<CVehicle*>(m_pCamTargetEntity);
    if (!vehicle->GetIsTypeVehicle())
        return;
    EnsureEntityMatrix(vehicle);
    const CVector& speed = vehicle->GetMoveSpeed();
    float angle = f;
    if (speed.x * speed.x + speed.y * speed.y > 0.0036f) {
        angle = std::atan2(-speed.x, speed.y) - CAM_PI / 2.0f; // fpatan(-x, y) - pi/2
    }
    const float dx = m_vecSource.x - v.x;
    const float dy = m_vecSource.y - v.y;
    const float dist2D = std::sqrt(dx * dx + dy * dy);

    float dBeta = angle - m_fHorizontalAngle;
    WrapAnglePi(&dBeta);
    const CVector& fwd = vehicle->m_matrix->GetForward();
    if (std::abs(dBeta) > 0.34906578f /* 20 deg */
        && 0.1f < fwd.x * speed.x + fwd.y * speed.y + fwd.z * speed.z
        && !s_bCamFlag_0xB6F080) {
        m_bFixingBeta = true;
    }

    CPad* pad = CPad::GetPad(0);
    if (!pad->GetLookBehindForCar() && !pad->GetLookBehindForPed()
        && !pad->GetLookLeft() && !pad->GetLookRight()
        && m_nDirectionWasLooking != 3) {
        s_bCamFlag_0xB6F042 = true;
    }
    if (!m_bFixingBeta && !s_bCamFlag_0xB6F05F
        && !s_bCamFlag_0xB6F042 && !s_bCamFlag_0xB6F043) {
        return;
    }
    bool doFix = false;
    if ((s_bCamFlag_0xB6F042 || s_bCamFlag_0xB6F043 || s_bCamFlag_0xB6F05F)
        && &TheCamera.GetActiveCam() == this) {
        doFix = true;
    }
    if (m_bFixingBeta || doFix) {
        WellBufferMe(angle, m_fHorizontalAngle, m_fBetaSpeed,
                     0.1f /* 0x3dcccccd */, 0.003f /* 0x3b449ba6 */, true);
        if (s_bCamFlag_0xB6F042 && &TheCamera.GetActiveCam() == this)
            m_fHorizontalAngle = angle;
        if (s_bCamFlag_0xB6F043 && &TheCamera.GetActiveCam() == this)
            m_fHorizontalAngle = angle + CAM_PI;
        if (s_bCamFlag_0xB6F05F && &TheCamera.GetActiveCam() == this)
            m_fHorizontalAngle = m_fTransitionBeta;
        m_vecSource.x = v.x + std::cos(m_fHorizontalAngle) * dist2D;
        m_vecSource.y = v.y + std::sin(m_fHorizontalAngle) * dist2D;
        dBeta = angle - m_fHorizontalAngle;
        WrapAnglePi(&dBeta);
        if (std::abs(dBeta) < 0.034906585f /* 2 deg */)
            m_bFixingBeta = false;
    }
    s_bCamFlag_0xB6F042 = false;
    s_bCamFlag_0xB6F043 = false;
}

bool CCam::Using3rdPersonMouseCam() const {
    // src/CCam/Using3rdPersonMouseCam_0050a850.c
    return g_bUseMouse3rdPerson && m_nMode == MODE_FOLLOWPED;
}

bool CCam::GetWeaponFirstPersonOn() {
    // src/CCam/GetWeaponFirstPersonOn_00509dc0.c
    // Decomp checks (m_nType & 7) == 3 (ENTITY_TYPE_PED) via the flags byte,
    // then reads the active weapon's first-person flag.
    return m_pCamTargetEntity
        && m_pCamTargetEntity->GetIsTypePed()
        && m_pCamTargetEntity->AsPed()->GetActiveWeapon().m_IsFirstPersonWeaponModeSelected;
}

void CCam::ClipAlpha() {
    // Was inline in gta-reversed; no named .c in src/CCam/.
    // Body from gta-reversed Cam.cpp (alpha = vertical angle).
    m_fVerticalAngle = std::clamp(
        m_fVerticalAngle,
        DegreesToRadians(-85.5f),
        DegreesToRadians(+60.0f)
    );
}

void CCam::ClipBeta() {
    // src/CCam/ClipBeta_00509c50.c (beta = horizontal angle, offset 0xBC).
    // NOTE: gta-reversed's reconstruction is WRONG here (it subtracts 2*PI
    // unconditionally in the else branch); the decomp only wraps out-of-range.
    if (m_fHorizontalAngle > CAM_PI) {
        m_fHorizontalAngle -= 2.0f * CAM_PI;
    } else if (m_fHorizontalAngle < -CAM_PI) {
        m_fHorizontalAngle += 2.0f * CAM_PI;
    }
}

void CCam::ApplyUnderwaterMotionBlur() {
    // Was inline in gta-reversed; no named .c in src/CCam/.
    // Body from gta-reversed Cam.cpp (0x4D5xxx region).
    static constexpr uint32_t UNDERWATER_CAM_BLUR = 20;    // 0x8CC7A4
    static constexpr float UNDERWATER_CAM_MAG_LIMIT = 10.0f; // 0x8CC7A8

    const float colorMag = std::sqrt(
        CTimeCycle::GetWaterRed() * CTimeCycle::GetWaterRed()
        + CTimeCycle::GetWaterGreen() * CTimeCycle::GetWaterGreen()
        + CTimeCycle::GetWaterBlue() * CTimeCycle::GetWaterBlue());
    const float factor = (colorMag <= UNDERWATER_CAM_MAG_LIMIT)
        ? 1.0f : UNDERWATER_CAM_MAG_LIMIT / colorMag;

    TheCamera.SetMotionBlur(
        static_cast<uint8_t>(factor * CTimeCycle::GetWaterRed()),
        static_cast<uint8_t>(factor * CTimeCycle::GetWaterGreen()),
        static_cast<uint8_t>(factor * CTimeCycle::GetWaterBlue()),
        UNDERWATER_CAM_BLUR,
        eMotionBlurType::LIGHT_SCENE);
}

// ---------------------------------------------------------------------------
// CCam::Process @ 00526fc0
// Per-frame camera update: resolves the look-at target point (with vehicle/
// ped-specific smoothing), dispatches to the mode-specific Process_* handler,
// then applies look-behind / look-right / look-left adjustments.
// ---------------------------------------------------------------------------

// --- File-static shims for CCam::Process ----------------------------------
// Camera-scope globals (binary addresses). TODO(port): consolidate these into
// CCamera.cpp as proper members/externs when the camera globals are owned.
static CEntity* s_pDefaultCamTarget;    // 0xB6F980: fallback m_pCamTargetEntity
static float    s_flt_00B6FE34;         // 0xB6FE34: crosshair/weapon-effect timer
static float    s_flt_00B6FDC8;         // 0xB6FDC8: compared against 0xB6FE34
static uint32_t s_u32_00C0B184;         // 0xC0B184: flags (bit 0 cleared here)
static CVector  s_vec_008CCC3C;         // 0x8CCC3C: last smoothed target position
static CVector  s_vec_00B6EC7C;         // 0xB6EC7C: smoothed target velocity (x,y)
static bool     s_b_00B6F052;           // 0xB6F052: look direction changed
static uint8_t  s_u8_00B6F062;          // 0xB6F062: look-state flags
static uint8_t  s_u8_00B6F077;          // 0xB6F077: look-state flags
static uint8_t  s_u8_00B6F080;          // 0xB6F080: look-state flags
static uint8_t  s_u8_00B6F059;          // 0xB6F059: DW dog-fight/fish flag
static uint8_t  s_u8_00B6F04E;          // 0xB6F04E: ped-dead-baby flags
static uint8_t  s_u8_00B6F050;          // 0xB6F050: ped-dead-baby flags
static uint8_t  s_u8_008CCF00;          // 0x8CCF00: follow-ped mouse override
static uint32_t s_u32_008CC488;         // 0x8CC488: DW ciney-cam state (-1 = none)
static uint8_t  s_u8_00B6F999;          // 0xB6F999: set when direction changes
static float    s_flt_gCurDistForCam;   // _gCurDistForCam
static uint8_t  s_u8_00B6FCDD;          // 0xB6FCDD: FOV override flag
static float    s_flt_00B6FCE0;         // 0xB6FCE0: FOV override value
static uint8_t  s_u8_00B6FD14;          // 0xB6FD14: source override flag
static CVector  s_vec_00B6FD08;         // 0xB6FD08: source override position
static uint8_t  s_u8_00B6FCB4;          // 0xB6FCB4: front override flag
static CVector  s_vec_00B6FCA8;         // 0xB6FCA8: front override look target
// CRT pow() tuning constants for ped-target smoothing (see 0x822130).
// The two bases sit 4 bytes apart; the exponent is shared. One of these may
// be CTimer::ms_fTimeStep - the decompiler didn't tag it. TODO: verify.
static float    s_flt_008CC394;         // 0x8CC394: pow() base (smoothing 1)
static float    s_flt_008CC398;         // 0x8CC398: pow() base (smoothing 2)
static float    s_flt_00B7CB5C;         // 0xB7CB5C: pow() exponent

// CGeneral::GetATanOfXY: fast atan2 used all over the camera. TODO(port): move
// to CGeneral.cpp with the real approximation; atan2f is the reference.
static float GetATanOfXY(float x, float y) { return ::atan2f(x, y); }

// CTaskSimpleClimb (not ported). Only GetCameraTargetPos is needed here.
class CTaskSimpleClimb {
public:
    void GetCameraTargetPos(CPed* ped, CVector* outPos); // TODO(port): real body
};

void CCam::Process() {
    // --- weapon-effect / crosshair prologue
    if (s_flt_00B6FE34 <= s_flt_00B6FDC8)
        s_u32_00C0B184 &= ~1u;
    if (TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode != MODE_FOLLOWPED) {
        s_u32_00C0B184 &= ~1u;
        s_flt_00B6FE34 = 0.0f;
    }

    // --- default target entity
    if (!m_pCamTargetEntity) {
        m_pCamTargetEntity = s_pDefaultCamTarget;
        if (m_pCamTargetEntity)
            m_pCamTargetEntity->SafeRegisterRef(m_pCamTargetEntity);
    }

    // --- clear the crosshair immediately (unless in a vehicle with model 520)
    if (gCrossHair[0].m_bClearImmediately) {
        CPlayerPed* player = FindPlayerPed(-1);
        if (!player || !player->m_pVehicle || player->m_pVehicle->GetModelIndex() != 0x208)
            CWeaponEffects::ClearCrossHairImmediately(0);
    }

    // --- collision-check frame counter
    m_nFrameNumWereAt++;
    if (m_nFrameNumWereAt > m_nDoCollisionCheckEveryNumOfFrames)
        m_nFrameNumWereAt = 1;
    m_bCollisionChecksOn = (m_nFrameNumWereAt == m_nDoCollisionChecksOnFrameNum);

    CVector targetPoint{};   // local_6c: the look-at target for this frame
    float   targetHeading = 0.0f; // local_78
    float   speedVar      = 0.0f;  // local_7c

    if (!m_bCamLookingAtVector) {
        CEntity* target = m_pCamTargetEntity;
        if (target->GetType() == ENTITY_TYPE_VEHICLE) {
            // --- vehicle target: point = vehicle position, heading = forward
            EnsureEntityMatrix(target);
            targetPoint = target->m_matrix->GetPosition();

            const CVector& fwd = target->m_matrix->GetForward();
            targetHeading = (fwd.x == 0.0f && fwd.y == 0.0f)
                ? 0.0f : GetATanOfXY(fwd.x, fwd.y);

            // 2D forward, normalised
            CVector fwd2D(fwd.x, fwd.y, 0.0f);
            fwd2D.Normalise();
            const float len2D = std::sqrt(fwd2D.x * fwd2D.x + fwd2D.y * fwd2D.y);
            if (len2D != 0.0f) {
                fwd2D.x /= len2D;
                fwd2D.y /= len2D;
            }

            // speed aligned with forward: CPhysical::m_vecMoveSpeed lives at
            // vehicle+0x44 in the binary (decoded from the decomp's
            // `(*ppCVar19)[1].m_placement` idiom - see RotCamIfInFrontCar).
            const CVehicle* veh = static_cast<const CVehicle*>(target);
            const float sx = fwd2D.x * veh->m_vecMoveSpeed.x;
            const float sy = fwd2D.y * veh->m_vecMoveSpeed.y;
            speedVar = std::sqrt(sx * sx + sy * sy);
            if (sx + sy <= 0.0f) {
                speedVar = -std::min(speedVar * 0.5555556f, 0.5f);
            } else {
                speedVar = std::min(speedVar * 1.1111112f, 1.0f);
            }
            m_fSpeedVar = m_fSpeedVar * 0.895f + speedVar * 0.10500002f;

            // flag the camera if the player isn't just looking behind
            if (m_nDirectionWasLooking != 3) {
                CPad* pad = CPad::GetPad(0);
                const bool justLookingBehind = pad->GetLookBehindForCar()
                    && !pad->GetLookLeft() && !pad->GetLookRight();
                if (!justLookingBehind)
                    s_bCamFlag_0xB6F042 = true;
            }
        } else {
            // --- ped target
            CPlayerPed* playerTarget = static_cast<CPlayerPed*>(target);
            if (playerTarget == FindPlayerPed(-1)) {
                CVector pedPos = target->GetPosition();
                if (CTaskSimpleClimb* climb = playerTarget->GetIntelligence()->GetTaskClimb())
                    climb->GetCameraTargetPos(playerTarget, &pedPos);
                targetPoint = pedPos;

                CPad* pad = CPad::GetPad(0);
                const float dx = s_vec_008CCC3C.x - pedPos.x;
                const float dy = s_vec_008CCC3C.y - pedPos.y;
                const float dz = s_vec_008CCC3C.z - pedPos.z;
                const bool teleported = (dx*dx + dy*dy + dz*dz) > 9.0f;
                if (teleported || CTimer::ms_fTimeStep < 0.2f
                    || Using3rdPersonMouseCam()
                    || s_bCamFlag_0xB6F042 || s_bCamFlag_0xB6F043) {
                    // snap: no smoothing
                    s_vec_00B6EC7C = CVector(0.0f, 0.0f, 0.0f);
                } else if (CTaskSimpleFight* fight = playerTarget->GetIntelligence()->GetTaskFighting()) {
                    if (m_nMode == MODE_AIMWEAPON) {
                        // lerp toward the last position while fighting/aiming
                        const float t = std::pow(s_flt_008CC394, s_flt_00B7CB5C);
                        targetPoint = pedPos * (1.0f - t) + s_vec_008CCC3C * t;
                        s_vec_00B6EC7C = CVector(0.0f, 0.0f, 0.0f);
                    } else {
                        goto SMOOTH_PED_TARGET;
                    }
                } else {
SMOOTH_PED_TARGET:
                    // exponential smoothing with velocity prediction
                    const float t1 = std::pow(s_flt_008CC394, s_flt_00B7CB5C);
                    const float t2 = std::pow(s_flt_008CC398, s_flt_00B7CB5C);
                    const float timeStep = CTimer::ms_fTimeStep;
                    // predicted = lastPos + lastVel * timeStep
                    const CVector predicted = s_vec_008CCC3C + s_vec_00B6EC7C * timeStep;
                    targetPoint = predicted * (1.0f - t1) + pedPos * t1;
                    const CVector delta = targetPoint - s_vec_008CCC3C;
                    const float invStep = std::max(1.0f, timeStep);
                    s_vec_00B6EC7C = s_vec_00B6EC7C * (1.0f - t2)
                                   + delta * (t2 / invStep);
                    s_vec_00B6EC7C.z = 0.0f;
                    targetPoint.z = pedPos.z;
                }
                s_vec_008CCC3C = targetPoint;
            } else {
                targetPoint = target->GetPosition();
            }

            // heading from the target's forward
            EnsureEntityMatrix(target);
            const CVector& fwd = target->m_matrix->GetForward();
            targetHeading = (fwd.x == 0.0f && fwd.y == 0.0f)
                ? 0.0f : GetATanOfXY(fwd.x, fwd.y);
            speedVar = 0.0f;
            m_fSpeedVar = 0.0f;
        }
    } else {
        targetPoint = m_vecCamFixedModeVector;
    }

    // --- look direction (behind / left / right)
    m_nDirectionWasLooking = gCameraDirection;
    gCameraDirection = 3; // forward
    if (&TheCamera.GetActiveCam() == this) {
        const eCamMode mode = m_nMode;
        const bool isVehicleCam = (mode == MODE_CAM_ON_A_STRING
            || mode == MODE_1STPERSON || mode == MODE_BEHINDBOAT
            || mode == MODE_BEHINDCAR)
            && m_pCamTargetEntity->GetIsTypeVehicle();
        if (isVehicleCam) {
            // don't allow look-left/right in helis and planes
            const CVehicle* veh = static_cast<const CVehicle*>(m_pCamTargetEntity);
            const bool isAir = veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI
                            || veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE;
            CPad* pad = CPad::GetPad(0);
            if (pad->GetLookBehindForCar()) {
                s_u8_00B6F080 = 0;
                s_u8_00B6F077 = 0;
                s_u8_00B6F062 = 0;
                if (m_nDirectionWasLooking != 0)
                    s_b_00B6F052 = true;
                gCameraDirection = 0;
            } else if (pad->GetLookLeft() && !isAir) {
                gCameraDirection = 1;
                s_u8_00B6F062 = 0;
                s_u8_00B6F077 = 0;
                s_u8_00B6F080 = 0;
                if (m_nDirectionWasLooking != gCameraDirection)
                    s_b_00B6F052 = true;
            } else if (pad->GetLookRight() && !isAir) {
                gCameraDirection = 2;
                s_u8_00B6F062 = 0;
                s_u8_00B6F077 = 0;
                s_u8_00B6F080 = 0;
                if (m_nDirectionWasLooking != gCameraDirection)
                    s_b_00B6F052 = true;
            } else {
                gCameraDirection = 3;
                if (m_nDirectionWasLooking != gCameraDirection)
                    s_b_00B6F052 = true;
            }
        } else if (mode == MODE_FOLLOWPED && m_pCamTargetEntity->GetIsTypePed()) {
            if (CPad::GetPad(0)->GetLookBehindForPed()) {
                if (m_nDirectionWasLooking != 0)
                    s_b_00B6F052 = true;
                gCameraDirection = 0;
            }
        } else if (mode == MODE_AIMWEAPON) {
            gCameraDirection = 3;
            if (m_nDirectionWasLooking != 3)
                s_flt_gCurDistForCam = 1.0f;
        }
    }

    if (s_b_00B6F052) {
        s_flt_gCurDistForCam = 1.0f;
        s_u8_00B6F999 = 1;
    }

    // --- speed FX only for vehicle-follow modes
    if (m_nMode != MODE_BEHINDCAR && m_nMode != MODE_CAM_ON_A_STRING
        && m_nMode != MODE_BEHINDBOAT && m_nMode != MODE_1STPERSON
        && m_nMode != MODE_TWOPLAYER_IN_CAR_AND_SHOOTING) {
        CPostEffects::m_bSpeedFXUserFlagCurrentFrame = false;
    }
    s_bFirstPersonRunThisFrame = false;

    // --- dispatch to the mode handler
    switch (m_nMode) {
    case MODE_BEHINDCAR:
    case MODE_CAM_ON_A_STRING:
    case MODE_BEHINDBOAT:
        Process_FollowCar_SA(targetPoint, targetHeading, m_fSpeedVar, speedVar, false);
        break;
    case MODE_FOLLOWPED:
        if (!g_bUseMouse3rdPerson || s_u8_008CCF00)
            Process_FollowPed_SA(targetPoint, targetHeading, m_fSpeedVar, speedVar, false);
        else
            Process_FollowPedWithMouse(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_SNIPER:
    case MODE_M16_1STPERSON:
    case MODE_HELICANNON_1STPERSON:
    case MODE_CAMERA:
        Process_M16_1stPerson(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_ROCKETLAUNCHER:
        Process_Rocket(targetPoint, targetHeading, m_fSpeedVar, speedVar, false);
        break;
    case MODE_WHEELCAM:
        Process_WheelCam(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_FIXED:
        Process_Fixed(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_1STPERSON:
        Process_1stPerson(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_FLYBY:
        Process_FlyBy(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_PED_DEAD_BABY:
        ProcessPedsDeadBaby();
        s_u8_00B6F04E = 0;
        s_u8_00B6F050 = 0;
        break;
    case MODE_ARRESTCAM_ONE:
        ProcessArrestCamOne();
        break;
    case MODE_ARRESTCAM_TWO:
        break;
    case MODE_SPECIAL_FIXED_FOR_SYPHON:
        Process_SpecialFixedForSyphon(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_SNIPER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT:
    case MODE_1STPERSON_RUNABOUT:
    case MODE_M16_1STPERSON_RUNABOUT:
    case MODE_FIGHT_CAM_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
        Process_1rstPersonPedOnPC(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_EDITOR:
        Process_Editor(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_ATTACHCAM:
        Process_AttachedCam();
        break;
    case MODE_TWOPLAYER:
        Process_Cam_TwoPlayer();
        break;
    case MODE_TWOPLAYER_IN_CAR_AND_SHOOTING:
        Process_Cam_TwoPlayer_InCarAndShooting();
        break;
    case MODE_TWOPLAYER_SEPARATE_CARS:
        Process_Cam_TwoPlayer_Separate_Cars();
        break;
    case MODE_ROCKETLAUNCHER_HS:
        Process_Rocket(targetPoint, targetHeading, m_fSpeedVar, speedVar, true);
        break;
    case MODE_AIMWEAPON:
    case MODE_AIMWEAPON_FROMCAR:
    case MODE_AIMWEAPON_ATTACHED:
        Process_AimWeapon(targetPoint, targetHeading, m_fSpeedVar, speedVar);
        break;
    case MODE_TWOPLAYER_SEPARATE_CARS_TOPDOWN:
        Process_Cam_TwoPlayer_Separate_Cars_TopDown();
        break;
    case MODE_DW_HELI_CHASE:
        Process_DW_HeliChaseCam(false);
        break;
    case MODE_DW_CAM_MAN:
        Process_DW_CamManCam(false);
        break;
    case MODE_DW_BIRDY:
        Process_DW_BirdyCam(false);
        break;
    case MODE_DW_PLANE_SPOTTER:
        Process_DW_PlaneSpotterCam(false);
        break;
    case MODE_DW_DOG_FIGHT:
    case MODE_DW_FISH:
        s_u8_00B6F059 = 0;
        break;
    case MODE_DW_PLANECAM1:
        Process_DW_PlaneCam1(false);
        break;
    case MODE_DW_PLANECAM2:
        Process_DW_PlaneCam2(false);
        break;
    case MODE_DW_PLANECAM3:
        Process_DW_PlaneCam3(false);
        break;
    default:
        // no handler: park the camera looking along +Y
        m_vecSource = CVector(0.0f, 0.0f, 0.0f);
        m_vecFront  = CVector(0.0f, 1.0f, 0.0f);
        m_vecUp     = CVector(0.0f, 0.0f, 1.0f);
        break;
    }

    // --- DW ciney-cam bookkeeping
    if (m_nMode < MODE_DW_HELI_CHASE || m_nMode > MODE_DW_PLANECAM3)
        s_u32_008CC488 = 0xFFFFFFFF;
    gCameraMode = m_nMode;

    // --- true beta/alpha from source->target, then speed tracking
    const float dx = m_vecSource.x - m_vecTargetCoorsForFudgeInter.x;
    const float dy = m_vecSource.y - m_vecTargetCoorsForFudgeInter.y;
    const float dz = m_vecSource.z - m_vecTargetCoorsForFudgeInter.z;
    m_fTrueBeta  = GetATanOfXY(dx, dy);
    m_fTrueAlpha = GetATanOfXY(std::sqrt(dx*dx + dy*dy), dz);
    if (!s_u8_00B6F080) {
        KeepTrackOfTheSpeed(m_vecSource, m_vecTargetCoorsForFudgeInter,
                            m_vecUp, m_fTrueAlpha, m_fTrueBeta, m_fFOV);
    }

    // --- stash source before look-behind, clear look flags
    m_vecSourceBeforeLookBehind = m_vecSource;
    m_bLookingRight  = false;
    m_bLookingLeft   = false;
    m_bLookingBehind = false;
    if (&TheCamera.GetActiveCam() == this) {
        if (gCameraDirection == 0)
            LookBehind();
        else if (gCameraDirection == 1)
            LookRight(false);
        else if (gCameraDirection == 2)
            LookRight(true);
        m_nDirectionWasLooking = gCameraDirection;
    }

    // --- FOV / source / front overrides (script-controlled)
    if (s_u8_00B6FCDD) {
        m_fFOV = s_flt_00B6FCE0;
        s_u8_00B6FCDD = 0;
    }
    if (s_u8_00B6FD14) {
        m_vecSource = s_vec_00B6FD08;
        s_u8_00B6FD14 = 0;
    }
    if (s_u8_00B6FCB4) {
        m_vecFront = s_vec_00B6FCA8 - m_vecSource;
        m_vecFront.Normalise();
        GetVectorsReadyForRW();
        s_u8_00B6FCB4 = 0;
    }
}

// ---------------------------------------------------------------------------
// CCam::LookBehind @ 00520690
// Repositions the camera to look behind the target (vehicle or ped).
// The ped branch uses several camera-tuning globals; opaque reads are
// shimmed with addresses (TODO: identify).
// ---------------------------------------------------------------------------

// Shims for CCam::LookBehind/LookRight (TODO(port): identify/consolidate)
static CVector  s_vec_00B6F018;         // 0xB6F018: vehicle look-behind anchor
static uint8_t  s_u8_00B6FC70;          // 0xB6FC70: ignore-entity count
static CEntity* s_apIgnoreEnts_00B6FC74[8]; // 0xB6FC74: ignore entities
static int32_t  s_nPedCameraView;       // PedCameraView
static float    s_flt_008CCE4C;         // 0x8CCE4C: ped look-behind distance
static float    s_flt_00B6F0FC;         // 0xB6F0FC: added to the above
static float    s_flt_008CCE50;         // 0x8CCE50: near clip for 1st-person
static void*    s_pRwCamera_00C1703C;   // 0xC1703C: RwCamera*
static float    s_flt_008CC394_b;       // 0x8CC394 (see Process shims)

void CCam::LookBehind() {
    const eCamMode mode = m_nMode;
    const bool isVehicle = m_pCamTargetEntity->GetIsTypeVehicle();
    const bool bVehicleCam = (mode == MODE_CAM_ON_A_STRING
                           || mode == MODE_BEHINDBOAT
                           || mode == MODE_BEHINDCAR) && isVehicle;
    const bool bFirstPersonVeh = (mode == MODE_1STPERSON) && isVehicle;
    const uint8_t type = m_pCamTargetEntity->GetType();
    if (!bVehicleCam && !bFirstPersonVeh && type != ENTITY_TYPE_PED)
        return;

    CVector targetPos = m_pCamTargetEntity->GetPosition();
    m_vecFront = targetPos - m_vecSource;

    if (bVehicleCam) {
        // anchor the behind-view at the vehicle look-behind point
        targetPos = s_vec_00B6F018;
        m_bLookingBehind = true;
        const float maxDist = (mode == MODE_CAM_ON_A_STRING)
            ? m_fCaMaxDistance : 15.5f;

        EnsureEntityMatrix(m_pCamTargetEntity);
        const CVector& fwd = m_pCamTargetEntity->m_matrix->GetForward();
        // source = anchor + maxDist * (fwd.x, fwd.y, fwd.z + 0.2)
        m_vecSource = targetPos
            + CVector(fwd.x * maxDist, fwd.y * maxDist, (fwd.z + 0.2f) * maxDist);

        s_pIgnoreEntity = m_pCamTargetEntity;
        s_u8_00B6FC70 = 0;
        TheCamera.CameraVehicleModeSpecialCases(
            static_cast<CVehicle*>(m_pCamTargetEntity));
        TheCamera.CameraColDetAndReact(&m_vecSource, &targetPos);
        m_vecFront = targetPos - m_vecSource;
        GetVectorsReadyForRW();
        TheCamera.ImproveNearClip(static_cast<CVehicle*>(m_pCamTargetEntity),
                                  nullptr, &m_vecSource, &targetPos);
        s_pIgnoreEntity = nullptr;
    }

    if (bFirstPersonVeh) {
        m_bLookingBehind = true;
        // RwCameraSetNearClipPlane(s_pRwCamera_00C1703C, 0.05f); // TODO: RW
        EnsureEntityMatrix(m_pCamTargetEntity);
        m_vecFront = m_pCamTargetEntity->m_matrix->GetForward();
        m_vecFront.Normalise();
        CVehicle* veh = static_cast<CVehicle*>(m_pCamTargetEntity);
        if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT)
            m_vecSource.z -= 0.5f;
        const eVehicleAppearance app = veh->GetVehicleAppearance();
        if (app == VEHICLE_APPEARANCE_BIKE) {
            m_vecSource = m_vecSource + m_vecFront * 2.3f;
            m_vecFront = m_vecFront * -1.0f;
            GetVectorsReadyForRW();
        } else if (app == VEHICLE_APPEARANCE_HELI) {
            const CVector& up = m_pCamTargetEntity->m_matrix->GetUp();
            m_vecFront = up * -1.0f;
            m_vecUp = m_pCamTargetEntity->m_matrix->GetForward();
            m_vecSource = m_vecSource + m_vecFront * 0.25f;
        } else {
            m_vecSource = m_vecSource + m_vecFront * 0.25f;
            m_vecFront = m_vecFront * -1.0f;
        }
    }

    if (type == ENTITY_TYPE_PED) {
        // Behind the ped: source points opposite the horizontal angle.
        // The z is adjusted by dotting a ped-space vector (ped+0x5A8 in the
        // binary; field not identified in the port) with the look direction.
        // TODO(port): identify ped+0x5A8 (CVector).
        const float horiz = m_fHorizontalAngle;
        m_vecSource = CVector(-std::cos(horiz), -std::sin(horiz), 0.0f);
        // Opaque: m_vecSource.z = 0.3 - dot(pedVec_0x5A8, m_vecSource)
        // (shimmed as 0.3f until the field is identified).
        m_vecSource.z = 0.3f; // TODO: restore the dot-product term
        m_vecSource.Normalise();

        float dist = s_flt_008CCE4C + s_flt_00B6F0FC;
        if (dist < 0.6f) dist = 0.6f;
        m_vecSource = targetPos + m_vecSource * dist;

        // TODO(port): PedCameraView-indexed tuning arrays at 0x8CCE18-0x8CCE3C
        // and the swim-task adjustment; shimmed as no-ops.
        TheCamera.HandleCameraMotionForDucking(
            static_cast<CPed*>(m_pCamTargetEntity), &m_vecSource, &targetPos, false);
        s_u8_00B6FC70 = 0;
        // TODO(port): GetTaskHold -> s_apIgnoreEnts_00B6FC74 population
        TheCamera.CameraColDetAndReact(&m_vecSource, &targetPos);
        m_vecFront = targetPos - m_vecSource;
        GetVectorsReadyForRW();
        TheCamera.ImproveNearClip(nullptr,
            static_cast<CPed*>(m_pCamTargetEntity), &m_vecSource, &targetPos);
    }

    GetVectorsReadyForRW();
}

// ---------------------------------------------------------------------------
// CCam::LookRight @ 00520e40
// Side view (left/right) for vehicle cameras; 1st-person vehicle adjusts the
// source to the ped's neck bone. The 1st-person ped-bone section is shimmed
// (TODO: port CPed bone helpers).
// ---------------------------------------------------------------------------
void CCam::LookRight(bool bLookRight) {
    const eCamMode mode = m_nMode;
    const bool isVehicle = m_pCamTargetEntity->GetIsTypeVehicle();
    const bool bVehicleCam = (mode == MODE_CAM_ON_A_STRING
                           || mode == MODE_BEHINDBOAT
                           || mode == MODE_BEHINDCAR) && isVehicle;
    const bool bFirstPersonVeh = (mode == MODE_1STPERSON) && isVehicle;

    const float sideSign = bLookRight ? 1.0f : -1.0f;
    if (bLookRight)
        m_bLookingRight = true;
    else
        m_bLookingLeft = true;

    if (!bVehicleCam) {
        if (bFirstPersonVeh) {
            // TODO(port): full 1st-person ped-in-vehicle branch needs
            // CPed::SetPedPositionInCar / GetBonePosition(BONE_NECK) and the
            // vehicle-right/up vector adjustments. Shimmed: nudge the source
            // along the vehicle's right vector.
            EnsureEntityMatrix(m_pCamTargetEntity);
            const CVector& right = m_pCamTargetEntity->m_matrix->GetRight();
            m_vecSource = m_vecSource - right * (0.35f * sideSign);
            const CVector& up = m_pCamTargetEntity->m_matrix->GetUp();
            m_vecUp = up;
            m_vecUp.Normalise();
            CVector fwd = m_pCamTargetEntity->m_matrix->GetForward();
            fwd.Normalise();
            CVector side;
            if (bLookRight)
                side.Cross_OG(fwd, m_vecUp);
            else
                side.Cross_OG(m_vecUp, fwd);
            m_vecFront = side;
            m_vecFront.Normalise();
            if (static_cast<CVehicle*>(m_pCamTargetEntity)->GetVehicleAppearance()
                == VEHICLE_APPEARANCE_BIKE) {
                m_vecSource = m_vecSource - m_vecFront * 1.45f;
            }
        }
        return;
    }

    // --- vehicle side view
    CVector targetPos = m_pCamTargetEntity->GetPosition();
    float maxDist = (mode == MODE_CAM_ON_A_STRING) ? m_fCaMaxDistance : 9.0f;
    // TODO(port): MODE_BEHINDBOAT z-adjust via GetBoatPointer/CCullZones.

    EnsureEntityMatrix(m_pCamTargetEntity);
    CVector fwd = m_pCamTargetEntity->m_matrix->GetForward();
    fwd.Normalise();
    const float angle = GetATanOfXY(fwd.x, fwd.y) + sideSign * 1.5707964f;
    const float savedZ = m_vecSource.z;
    m_vecSource.x = std::cos(angle) * maxDist + targetPos.x;
    m_vecSource.y = std::sin(angle) * maxDist + targetPos.y;

    s_pIgnoreEntity = m_pCamTargetEntity;
    s_u8_00B6FC70 = 0;
    TheCamera.CameraVehicleModeSpecialCases(
        static_cast<CVehicle*>(m_pCamTargetEntity));
    TheCamera.CameraColDetAndReact(&m_vecSource, &targetPos);
    s_pIgnoreEntity = nullptr;

    // z from the collision bounding box, clamped against the fudge target
    targetPos = m_pCamTargetEntity->GetPosition();
    EnsureEntityMatrix(m_pCamTargetEntity);
    const CVector& right = m_pCamTargetEntity->m_matrix->GetRight();
    const CVector& up = m_pCamTargetEntity->m_matrix->GetUp();
    // TODO(port): exact bbox terms (m_boundBox.m_vecMin/Max.x * right.z etc.)
    float z = targetPos.z + up.z; // shimmed
    if (z < m_vecTargetCoorsForFudgeInter.z)
        z = m_vecTargetCoorsForFudgeInter.z;
    float newZ = savedZ;
    if (z + 0.1f <= savedZ)
        newZ = z + 0.1f;
    if (newZ <= m_vecSource.z)
        newZ = m_vecSource.z;
    m_vecSource.z = newZ;

    m_vecFront = targetPos - m_vecSource;
    m_vecFront.z += (mode == MODE_BEHINDBOAT) ? 2.3f : 1.1f;
    GetVectorsReadyForRW();
}
