// CAnimManager.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CAnimManager.
// Ported from gta-reversed/source/game_sa/Animation/AnimManager.cpp,
// verified against the decompiled bodies in
// C:\Users\fufid\Documents\Decomps\gta-sa decomp\src\CAnimManager\*.c
// (original GTA SA 1.0 function addresses kept as comments).
//
// Adaptations from gta-reversed:
// - InjectHooks() stripped (plugin-sdk hooking, not needed for clean-room).
// - ZoneScoped (tracy) removed.
// - NOTSA_LOG_* removed (no logging facility in this build yet).
// - VERIFY(x) -> assert(x); NOTSA_UNREACHABLE(msg) -> assert(false).
// - notsa::ci_string_view -> StrICmp() below (the decomp uses __stricmp,
//   see GetAnimationBlock @004d3940 and GetFirstAssocGroup @004d39b0).
// - rng::views::take / rngv::enumerate (plugin-sdk) -> plain loops.
// - sscanf_s/SCANF_S_STR -> plain sscanf.
// - strcpy_s/strncpy_s (MSVC CRT) -> portable strncpy with explicit
//   null-termination (this TU is also syntax-checked with GCC).
// - LoadAnimFile_ANPK: the decomp (@004d47f0) guards the
//   uncompressed-animations scan with a null check; gta-reversed omits it.
//   The null check is kept here (faithful to the decomp).
// - notsa::contains -> explicit comparisons.

#include "CAnimManager.h"

// Ported subsystem headers used by the filled bodies below.
#include "CStreaming.h"
#include "CFileMgr.h"
#include "CFileLoader.h"
#include "CModelInfo.h"
#include "CBaseModelInfo.h"
#include "CMemoryMgr.h"
#include "CAnimBlendSequence.h"
#include "CAnimBlendClumpData.h"

#include <algorithm> // std::max, std::min
#include <cassert>
#include <cstdio>   // std::sscanf
#include <cstring>  // std::strncpy, std::strncmp
#include <tuple>    // std::make_tuple
#ifndef _MSC_VER
#include <strings.h> // strcasecmp (StrICmp fallback on GCC/Clang)
#endif

// ============================================================================
// TODO(port): external subsystem shims.
// Minimal declarations for subsystems not yet ported to cpp/. Each entry is
// verified against gta-reversed/source/game_sa and the decompiled bodies in
// src/CAnimManager/*.c. Delete entries as their subsystem lands; do not grow
// this list. None of these introduce link-time dependencies for -fsyntax-only
// or `ar` static-library builds.
// ============================================================================

// --- RenderWare stream layer (gta-reversed source/game_sa/RenderWare/rw/rwplcore.h)
// No RW layer in this build yet (see CMakeLists.txt TODO).
enum RwStreamType {
    rwSTREAMFILENAME,
    rwSTREAMMEMORY,
    rwSTREAMCUSTOM
};
enum RwStreamAccessType {
    rwNASTREAMACCESS = 0,
    rwSTREAMREAD,
    rwSTREAMWRITE,
    rwSTREAMAPPEND
};
RwStream* RwStreamOpen(RwStreamType type, RwStreamAccessType accessType, const void* pData);
void      RwStreamClose(RwStream* stream, void* pData); // SDK returns RwBool; ignored here
RwStream* RwStreamRead(RwStream* stream, void* buffer, uint32_t length);
RwStream* RwStreamSkip(RwStream* stream, uint32_t offset);

// gta-reversed/source/game_sa/RenderWare/RenderWare.h
template<typename T>
inline T RwStreamRead(RwStream* stream, size_t size = sizeof(T)) {
    T data{};
    RwStreamRead(stream, &data, size);
    return data;
}

// gta-reversed/source/game_sa/common.h
constexpr uint32_t MakeFourCC(const char fourcc[4]) {
    return fourcc[0] << 0 | fourcc[1] << 8 | fourcc[2] << 16 | fourcc[3] << 24;
}

// IFP section header. AnimTypes.h only forward-declares it (RW layer not yet
// converted); defined here. gta-reversed/source/game_sa/Animation/AnimManager.cpp
struct IFPSectionHeader {
    union {
        uint32_t ID;
        char     IDFourCC[4];
    };
    uint32_t Size;
};

// --- CKeyGen (gta-reversed source/game_sa/Core/KeyGen.h)
class CKeyGen {
public:
    static uint32_t GetUppercaseKey(const char* str);
};

// --- RpAnimBlend plugin (gta-reversed source/game_sa/Plugins/RpAnimBlendPlugin/RpAnimBlend.h)
CAnimBlendClumpData*& RpAnimBlendClumpGetData(RpClump* clump);
void RpAnimBlendClumpInit(RpClump* clump);

// --- RenderWare core (SDK; returns RwBool in the SDK, ignored here)
void RpClumpDestroy(RpClump* clump);

// --- CGeneral (gta-reversed source/game_sa/General.h)
class CGeneral {
public:
    template<typename T, size_t N>
    static const T& RandomChoice(const T(&arr)[N]);
};

// --- eModelID (gta-reversed source/game_sa/Enums/eModelID.h)
// Decomp ReadAnimAssociationDefinitions @005bc910 passes the literal 7 here.
constexpr int32_t MODEL_MALE01 = 7;

// --- AnimAssocDescriptions (gta-reversed source/game_sa/Animation/AnimAssocDescriptions.h)
// 0x8A7788, 191 entries. Only aStdAnimDescs is referenced by this TU
// (ReadAnimAssociationDefinitions); the other tables (aDoorDescs, aBikesDescs,
// ...) belong to the full AnimAssocDefinitions.cpp port (see TODO below).
// AnimationId values added to include/AnimTypes.h, verified against
// gta-reversed/source/game_sa/Enums/AnimationEnums.h.
static AnimDescriptor aStdAnimDescs[] = {
// 0x8A7788 191
    { ANIM_ID_WALK,                      ANIMATION_IS_LOOPED | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_RUN,                       ANIMATION_IS_LOOPED | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_SPRINT,                    ANIMATION_IS_LOOPED | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_IDLE,                      ANIMATION_IS_LOOPED },
    { ANIM_ID_ROADCROSS,                 ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_WALK_START,                ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_RUN_STOP,                  ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_RUN_STOPR,                 ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_IDLE_HBHB_0,               ANIMATION_IS_LOOPED | ANIMATION_IS_PARTIAL },
    { ANIM_ID_IDLE_HBHB_1,               ANIMATION_IS_LOOPED | ANIMATION_IS_PARTIAL },
    { ANIM_ID_IDLE_TIRED,                ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_IDLE_ARMED,                ANIMATION_IS_LOOPED | ANIMATION_IS_PARTIAL },
    { ANIM_ID_IDLE_CHAT,                 ANIMATION_IS_LOOPED | ANIMATION_IS_PARTIAL },
    { ANIM_ID_IDLE_TAXI,                 ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_SWIM_TREAD,                ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_KO_SHOT_FRONT_0,           ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_IS_FRONT },
    { ANIM_ID_KO_SHOT_FRONT_1,           ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_IS_FRONT },
    { ANIM_ID_KO_SHOT_FRONT_2,           ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_IS_FRONT },
    { ANIM_ID_KO_SHOT_FRONT_3,           ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_IS_FRONT },
    { ANIM_ID_KO_SHOT_FACE,              ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_KO_SHOT_STOM,              ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_GAS_CWR,                   ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_KD_LEFT,                   ANIMATION_IS_PARTIAL },
    { ANIM_ID_KD_RIGHT,                  ANIMATION_IS_PARTIAL },
    { ANIM_ID_KO_SKID_FRONT,             ANIMATION_IS_PARTIAL },
    { ANIM_ID_KO_SPIN_R,                 ANIMATION_IS_PARTIAL },
    { ANIM_ID_KO_SKID_BACK,              ANIMATION_IS_PARTIAL | ANIMATION_IS_FRONT },
    { ANIM_ID_KO_SPIN_L,                 ANIMATION_IS_PARTIAL },
    { ANIM_ID_SHOT_PARTIAL,              ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND },
    { ANIM_ID_SHOT_LEFTP,                ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND },
    { ANIM_ID_SHOT_PARTIAL_B,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND },
    { ANIM_ID_SHOT_RIGHTP,               ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND },
    { ANIM_ID_HIT_FRONT,                 ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_HIT_L,                     ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_HIT_BACK,                  ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_HIT_R,                     ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_FLOOR_HIT,                 ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_HIT_WALK,                  ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_HIT_WALL,                  ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_FLOOR_HIT_F,               ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_IS_FRONT },
    { ANIM_ID_HIT_BEHIND,                ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_FIGHTSH_FWD,               ANIMATION_IS_LOOPED | ANIMATION_IS_PARTIAL | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_FIGHTSH_LEFT,              ANIMATION_IS_LOOPED | ANIMATION_IS_PARTIAL | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_CAN_EXTRACT_X_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_FIGHTSH_BWD,               ANIMATION_IS_LOOPED | ANIMATION_IS_PARTIAL | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_FIGHTSH_RIGHT,             ANIMATION_IS_LOOPED | ANIMATION_IS_PARTIAL | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_CAN_EXTRACT_X_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_FIGHTSHF,                  ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_FIGHTSHB,                  ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_FIGHT2IDLE,                ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_BOMBER,                    ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_GUN_STAND,                 ANIMATION_IS_LOOPED },
    { ANIM_ID_GUNMOVE_FWD,               ANIMATION_IS_LOOPED | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_GUNMOVE_L,                 ANIMATION_IS_LOOPED | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_CAN_EXTRACT_X_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_GUNMOVE_BWD,               ANIMATION_IS_LOOPED | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_GUNMOVE_R,                 ANIMATION_IS_LOOPED | ANIMATION_IS_SYNCRONISED | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_CAN_EXTRACT_X_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_GUN_2_IDLE,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_WEAPON_CROUCH,             ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_GUNCROUCHFWD,              ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_CROUCH_ROLL_L,             ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_CAN_EXTRACT_X_VELOCITY },
    { ANIM_ID_GUNCROUCHBWD,              ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_WALK },
    { ANIM_ID_CROUCH_ROLL_R,             ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_CAN_EXTRACT_X_VELOCITY },
    { ANIM_ID_CAR_SIT,                   ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_CAR_LSIT,                  ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_CAR_SITP,                  ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_CAR_SITPLO,                ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_CAR_SIT_WEAK,              ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_CAR_SIT_PRO,               ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_DRIVE_L,                   ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_R,                   ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_LO_L,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_LO_R,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_L_WEAK,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_R_WEAK,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_L_PRO,               ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_R_PRO,               ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVEBY_L,                 ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVEBY_R,                 ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVEBYL_L,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVEBYL_R,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_CAR_LB,                    ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_CAR_LB_WEAK,               ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_CAR_LB_PRO,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_BOAT,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_BOAT_L,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_BOAT_R,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_BOAT_BACK,           ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_L_SLOW,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_R_SLOW,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_L_WEAK_SLOW,         ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_R_WEAK_SLOW,         ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_L_PRO_SLOW,          ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_R_PRO_SLOW,          ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_TRUCK,               ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_DRIVE_TRUCK_L,             ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_TRUCK_R,             ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_DRIVE_TRUCK_BACK,          ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_KART_DRIVE,                ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_KART_L,                    ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_KART_R,                    ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_KART_LB,                   ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_BIKE_PICKUPR,              ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_BIKE_PICKUPL,              ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_BIKE_PULLUPR,              ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_BIKE_PULLUPL,              ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_BIKE_ELBOWL,               ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_BIKE_ELBOWR,               ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_BIKE_FALL_OFF,             ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_BIKE_FALLR,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_CAR_HOOKERTALK,            ANIMATION_IS_LOOPED | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0, ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_1, ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS,   ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_CAN_EXTRACT_X_VELOCITY | ANIMATION_IS_FRONT },
    { ANIM_ID_DEFAULT_CAR_ROLLOUT_RHS,   ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_CAN_EXTRACT_X_VELOCITY | ANIMATION_IS_FRONT },
    { ANIM_ID_GETUP_0,                   ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_GETUP_1,                   ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_GETUP_2,                   ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_GETUP_FRONT,               ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_IS_FRONT },
    { ANIM_ID_JUMP_LAUNCH,               ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_JUMP_LAUNCH_R,             ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_JUMP_GLIDE,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_JUMP_LAND,                 ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_FALL_FALL,                 ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_FALL_GLIDE,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_FALL_LAND,                 ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_FALL_COLLAPSE,             ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_FALL_BACK,                 ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_FALL_FRONT,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_IS_FRONT },
    { ANIM_ID_EV_STEP,                   ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_EV_DIVE,                   ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_IS_FRONT },
    { ANIM_ID_CLIMB_JUMP,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_CLIMB_IDLE,                ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_CLIMB_PULL,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_CLIMB_STAND,               ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_CLIMB_STAND_FINISH,        ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_CLIMB_JUMP_B,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_CLIMB_JUMP2FALL,           ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_XPRESSSCRATCH,             ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_200 },
    { ANIM_ID_TURN_180,                  ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_TURN_L,                    ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_TURN_R,                    ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE },
    { ANIM_ID_ARRESTGUN,                 ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_DROWN,                     ANIMATION_IS_PARTIAL },
    { ANIM_ID_DUCK_COWER,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_HANDSUP,                   ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_HANDSCOWER,                ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_CAN_EXTRACT_VELOCITY },
    { ANIM_ID_FUCKU,                     ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND },
    { ANIM_ID_PHONE_IN,                  ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_PHONE_OUT,                 ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND },
    { ANIM_ID_PHONE_TALK,                ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND },
    { ANIM_ID_SEAT_DOWN,                 ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_SEAT_UP,                   ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_SEAT_IDLE,                 ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_ATM,                       ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_ABSEIL,                    ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_WALK_DOORPARTIAL,          ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_FACSURP,                   ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND | ANIMATION_FACIAL },
    { ANIM_ID_FACSURPM,                  ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND | ANIMATION_FACIAL },
    { ANIM_ID_FACURIOS,                  ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND | ANIMATION_FACIAL },
    { ANIM_ID_FACANGER_0,                ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND | ANIMATION_FACIAL },
    { ANIM_ID_FACANGER_1,                ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND | ANIMATION_FACIAL },
    { ANIM_ID_FACANGER_2,                ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND | ANIMATION_FACIAL },
    { ANIM_ID_FACTALK,                   ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND | ANIMATION_FACIAL },
    { ANIM_ID_FACGUM,                    ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND | ANIMATION_FACIAL },
    { ANIM_ID_TAP_HAND,                  ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_TAP_HANDP,                 ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_SECONDARY_TASK_ANIM },
    { ANIM_ID_SHOVE_PARTIAL,             ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_FLEE_LKAROUND_01,          ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_ENDCHAT_01,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_ENDCHAT_02,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_ENDCHAT_03,                ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_SMOKE_IN_CAR,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND },
    { ANIM_ID_PASS_SMOKE_IN_CAR,         ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL | ANIMATION_DONT_ADD_TO_PARTIAL_BLEND },
    { ANIM_ID_DAM_ARML_FRMBK,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_ARML_FRMFT,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_ARML_FRMLT,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_ARMR_FRMBK,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_ARMR_FRMFT,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_ARMR_FRMRT,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_LEGL_FRMBK,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_LEGL_FRMFT,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_LEGL_FRMLT,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_LEGR_FRMBK,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_LEGR_FRMFT,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_LEGR_FRMRT,            ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_STOMACH_FRMBK,         ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_STOMACH_FRMFT,         ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_STOMACH_FRMLT,         ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_DAM_STOMACH_FRMRT,         ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_CAR_DEAD_LHS,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_CAR_DEAD_RHS,              ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_CAR_TUNE_RADIO,            ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE | ANIMATION_IS_PARTIAL },
    { ANIM_ID_GANG_GUNSTAND,             ANIMATION_IS_LOOPED | ANIMATION_IS_BLEND_AUTO_REMOVE },

};

// --- Local helpers ------------------------------------------------------------

// Case-insensitive name compare. The decomp uses __stricmp for the anim-block
// and assoc-group name scans (GetAnimationBlock @004d3940,
// GetFirstAssocGroup @004d39b0).
static int StrICmp(const char* a, const char* b) {
#ifdef _MSC_VER
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}

// Portable bounded string copy (replaces strcpy_s/strncpy_s).
static void StrCopyN(char* dst, size_t dstSize, const char* src) {
    assert(dstSize > 0);
    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

namespace ANPK {
    template<size_t MaxNameLen>
    struct Info {
        uint32_t Num;
        char     Name[MaxNameLen];
    };

    template<size_t MaxLen>
    struct TString {
        char Data[MaxLen];
    };

    //! Round number to 4 byte boundary
    constexpr uint32_t RoundTo4(uint32_t num) {
        return num % 4
            ? num + 4 - (num % 4)
            : num;
    }

    template<typename T>
    auto ReadSection(RwStream* stream) {
        auto h = RwStreamRead<IFPSectionHeader>(stream);
        h.Size = RoundTo4(h.Size);
        struct Ret { T v; IFPSectionHeader h; };
        return Ret{ RwStreamRead<T>(stream, h.Size), h };
    }
};

// ============================================================================
// StaticRef -> plain statics. Original GTA SA 1.0 addresses kept as comments;
// TODO: re-resolve when the memory-layout work reaches this subsystem.
// ============================================================================
std::array<AnimAssocDefinition, NUM_ANIM_ASSOC_GROUPS> CAnimManager::ms_aAnimAssocDefinitionsX;
AnimAssocDefinition CAnimManager::ms_aAnimAssocDefinitions[NUM_ANIM_ASSOC_GROUPS]; // 0x8AA5A8
uint32_t CAnimManager::ms_numAnimAssocDefinitions;                                // 0xB4EA28
CAnimBlendAssocGroup* CAnimManager::ms_aAnimAssocGroups;                           // 0xB4EA34
std::array<CAnimBlendHierarchy, 2500> CAnimManager::ms_aAnimations;                // 0xB4EA40
int32_t CAnimManager::ms_numAnimations;                                           // 0xB4EA2C
std::array<CAnimBlock, NUM_ANIM_BLOCKS> CAnimManager::ms_aAnimBlocks;              // 0xB5D4A0
uint32_t CAnimManager::ms_numAnimBlocks;                                          // 0xB4EA30
CLinkList<CAnimBlendHierarchy*> CAnimManager::ms_AnimCache;                       // 0xB5EB20

// 0x5BF6B0
void CAnimManager::Initialise() {
    ms_numAnimations = 0;
    ms_numAnimBlocks = 0;
    ms_numAnimAssocDefinitions = 118; // ANIM_TOTAL_GROUPS aka NUM_ANIM_ASSOC_GROUPS
    ms_AnimCache.Init(50);
    ReadAnimAssociationDefinitions();
    RegisterAnimBlock("ped");
}

// 0x5BC910
void CAnimManager::ReadAnimAssociationDefinitions() {
    char groupName[32], blockName[32], type[32];
    bool                 isAnimSection = false;
    AnimAssocDefinition* def = nullptr;
    uint32_t             animCount = 0;

    CFileMgr::SetDir("");
    const auto f = CFileMgr::OpenFile("DATA\\ANIMGRP.DAT", "rb");
    for (;;) {
        const auto l = CFileLoader::LoadLine(f);
        if (!l) {
            break;
        }
        if (!*l || *l == '#') {
            continue;
        }
        if (isAnimSection) {
            if (std::sscanf(l, "%s", groupName) == 1) {
                if (!std::strncmp(groupName, "end", 4)) {
                    isAnimSection = false;
                } else {
                    AddAnimToAssocDefinition(def, groupName);
                }
            }
        } else {
            [[maybe_unused]] const auto n =
                std::sscanf(l, "%s %s %s %d", groupName, blockName, type, &animCount);
            assert(n == 4);
            def = AddAnimAssocDefinition(groupName, blockName, MODEL_MALE01, animCount, aStdAnimDescs);
            isAnimSection = true;
        }
    }
    CFileMgr::CloseFile(f);
}

// 0x4D4130
void CAnimManager::Shutdown() {
    for (auto i = 0; i < NUM_ANIM_BLOCKS; i++) {
        CStreaming::RemoveModel(IFPToModelId(i));
    }

    for (auto i = 0; i < ms_numAnimations; i++) {
        ms_aAnimations[i].Shutdown();
    }

    ms_AnimCache.Shutdown();
    delete[] ms_aAnimAssocGroups;
}

CAnimBlock* CAnimManager::GetAnimationBlock(AssocGroupId animGroup) {
    return GetAssocGroups()[animGroup].m_AnimBlock;
}

// 0x4D3940
CAnimBlock* CAnimManager::GetAnimationBlock(const char* name) {
    for (auto& ab : GetAnimBlocks()) {
        if (StrICmp(ab.Name, name) == 0) {
            return &ab;
        }
    }
    return nullptr;
}

int32_t CAnimManager::GetAnimationBlockIndex(AssocGroupId animGroup) {
    return GetAnimationBlock(animGroup) - ms_aAnimBlocks.data();
}

int32_t CAnimManager::GetAnimationBlockIndex(CAnimBlock* animBlock) {
    return animBlock - ms_aAnimBlocks.data();
}

// 0x4D3990
int32_t CAnimManager::GetAnimationBlockIndex(const char* name) {
    const auto b = GetAnimationBlock(name);
    return b
        ? static_cast<int32_t>(b - ms_aAnimBlocks.data())
        : -1;
}

// 0x4D39B0
AssocGroupId CAnimManager::GetFirstAssocGroup(const char* name) {
    for (auto i = 0; i < ANIM_GROUP_MAN; i++) {
        if (StrICmp(ms_aAnimAssocDefinitions[i].BlockName, name) == 0) {
            return static_cast<AssocGroupId>(i);
        }
    }
    return ANIM_GROUP_MAN;
}

// 0x4D39F0
CAnimBlendHierarchy* CAnimManager::GetAnimation(uint32_t hash, const CAnimBlock* animBlock) {
    auto h = &ms_aAnimations[animBlock->FirstAnimIdx];
    for (auto i = animBlock->NumAnims; i-- > 0; h++) {
        if (h->m_hashKey == hash) {
            return h;
        }
    }
    return nullptr;
}

// 0x4D42F0
CAnimBlendHierarchy* CAnimManager::GetAnimation(const char* animName, const CAnimBlock* animBlock) {
    return GetAnimation(CKeyGen::GetUppercaseKey(animName), animBlock);
}

// notsa
CAnimBlendHierarchy& CAnimManager::GetAnimation(AnimationId id) {
    return ms_aAnimations[static_cast<size_t>(id)];
}

// 0x4D3A20
const char* CAnimManager::GetAnimGroupName(AssocGroupId groupId) {
    return ms_aAnimAssocDefinitions[groupId].GroupName;
}

// 0x4D3A30
const char* CAnimManager::GetAnimBlockName(AssocGroupId groupId) {
    return ms_aAnimAssocDefinitions[groupId].BlockName;
}

// NOTSA
AssocGroupId CAnimManager::GetAnimationGroupIdByName(std::string_view name) {
    auto groups = GetAssocGroupDefs();
    for (size_t i = 0; i < groups.size(); i++) {
        // Case-insensitive: the original notsa::ci_string_view is
        // case-insensitive (see the StrICmp note at the top of this file).
        if (std::strlen(groups[i].GroupName) == name.size()
            && StrICmp(groups[i].GroupName, std::string(name).c_str()) == 0) {
            return static_cast<AssocGroupId>(i);
        }
    }
    assert(false && "Couldn't find group");
    return ANIM_GROUP_NONE;
}

// 0x4D3A40
CAnimBlendAssociation* CAnimManager::CreateAnimAssociation(AssocGroupId groupId, AnimationId animId) {
    return GetAssocGroups()[groupId].CopyAnimation(animId);
}

// 0x4D3A60
CAnimBlendStaticAssociation* CAnimManager::GetAnimAssociation(AssocGroupId groupId, AnimationId animId) {
    return GetAssocGroups()[groupId].GetAnimation(animId);
}

// 0x4D3A80
CAnimBlendStaticAssociation* CAnimManager::GetAnimAssociation(AssocGroupId groupId, const char* animName) {
    return GetAssocGroups()[groupId].GetAnimation(animName);
}

// NOTSA - Internal
CAnimBlendAssociation* CAnimManager::AddAnimationToClump(RpClump* clump, CAnimBlendAssociation* anim) {
    const auto clumpAnims = &RpAnimBlendClumpGetData(clump)->m_AnimList;

    CAnimBlendAssociation* syncWith{};
    if (anim->IsSyncronised()) {
        for (auto l = clumpAnims->next; l; l = l->next) {
            const auto a = CAnimBlendAssociation::FromLink(l);
            if (a->IsSyncronised()) {
                syncWith = a;
                break;
            }
        }
    }

    if (syncWith) {
        anim->SyncAnimation(syncWith);
        anim->m_Flags |= ANIMATION_IS_PLAYING;
    } else {
        anim->Start(0.0f);
    }

    clumpAnims->Prepend(&anim->m_Link);

    return anim;
}

// 0x4D3AA0
CAnimBlendAssociation* CAnimManager::AddAnimation(RpClump* clump, AssocGroupId groupId, AnimationId animId) {
    return AddAnimationToClump(clump, CreateAnimAssociation(groupId, animId));
}

// 0x4D4330
CAnimBlendAssociation* CAnimManager::AddAnimation(RpClump* clump, CAnimBlendHierarchy* hier, int32_t clumpAssocFlag) {
    const auto anim = new CAnimBlendAssociation(clump, hier);
    anim->m_Flags |= clumpAssocFlag;
    anim->ReferenceAnimBlock();
    UncompressAnimation(hier);
    return AddAnimationToClump(clump, anim);
}

// 0x4D3B30
CAnimBlendAssociation* CAnimManager::AddAnimationAndSync(RpClump* clump, CAnimBlendAssociation* syncWith, AssocGroupId groupId, AnimationId animId) {
    const auto a = CreateAnimAssociation(groupId, animId);
    if (a->IsSyncronised() && syncWith) {
        a->SyncAnimation(syncWith);
        a->m_Flags |= ANIMATION_IS_PLAYING;
    } else {
        a->Start(0.0f);
    }

    RpAnimBlendClumpGetData(clump)->m_AnimList.Prepend(&a->m_Link);
    return a;
}

// 0x4D3BA0
AnimAssocDefinition* CAnimManager::AddAnimAssocDefinition(const char* groupName, const char* blockName, uint32_t modelIndex, uint32_t animsCount, AnimDescriptor* descriptor) {
    const auto def = &ms_aAnimAssocDefinitions[ms_numAnimAssocDefinitions++];

    StrCopyN(def->GroupName, sizeof(def->GroupName), groupName);
    StrCopyN(def->BlockName, sizeof(def->BlockName), blockName);

    def->ModelIndex = modelIndex;
    def->NumAnims   = animsCount;
    def->AnimDescr  = descriptor;

    def->AnimNames   = new const char*[animsCount];
    const auto bufsz = AnimAssocDefinition::ANIM_NAME_BUF_SZ * animsCount;
    const auto buf   = new char[bufsz];
    memset(buf, 0, bufsz);
    for (auto i = animsCount; i-- > 0;) {
        def->AnimNames[i] = buf + i * AnimAssocDefinition::ANIM_NAME_BUF_SZ;
    }

    return def;
}

// 0x4D3C80
void CAnimManager::AddAnimToAssocDefinition(AnimAssocDefinition* def, const char* animName) {
    uint32_t i = 0;
    for (; def->AnimNames[i][0]; i++) {
        assert(i < static_cast<uint32_t>(def->NumAnims));
    }
    // `const_cast` is fine here, because it's heap allocated
    StrCopyN(const_cast<char*>(def->AnimNames[i]), AnimAssocDefinition::ANIM_NAME_BUF_SZ, animName);
}

// 0x4D3CC0
void CAnimManager::CreateAnimAssocGroups() {
    auto groups = GetAssocGroups();
    for (size_t i = 0; i < groups.size(); i++) {
        auto& group = groups[i];
        const auto def   = &ms_aAnimAssocDefinitions[i];
        const auto block = GetAnimationBlock(def->BlockName);
        if (block == nullptr || !block->IsLoaded || group.m_Anims) {
            continue;
        }

        RpClump* clump = nullptr;
        if (def->ModelIndex != MODEL_INVALID) {
            // C-style cast in gta-reversed; reinterpret_cast: both are opaque
            // RW types here (RW layer not yet converted).
            clump = reinterpret_cast<RpClump*>(CModelInfo::GetModelInfo(def->ModelIndex)->CreateInstance());
            RpAnimBlendClumpInit(clump);
        }

        group.m_GroupID = static_cast<AssocGroupId>(i);
        group.m_IdOffset = def->AnimDescr->AnimId;
        group.CreateAssociations(def->BlockName, clump, def->AnimNames, def->NumAnims);
        for (auto j = 0u; j < group.m_NumAnims; j++) {
            group.GetAnimation(def->AnimDescr[j].AnimId)->m_Flags |= def->AnimDescr[j].Flags;
        }

        if (clump) {
#ifdef SA_SKINNED_PEDS
            if (IsClumpSkinned(clump)) {
                RpClumpForAllAtomics(clump, AtomicRemoveAnimFromSkinCB, nullptr);
            }
#endif
            RpClumpDestroy(clump);
        }
    }
}

// 0x4D3E50
int32_t CAnimManager::RegisterAnimBlock(const char* name) {
    CAnimBlock* ab = GetAnimationBlock(name);
    if (ab == nullptr) { // Initialize a new anim block
        ab = &ms_aAnimBlocks[ms_numAnimBlocks++];
        StrCopyN(ab->Name, MAX_ANIM_BLOCK_NAME, name);
        ab->NumAnims = 0;
        ab->GroupId = GetFirstAssocGroup(name);
        assert(ab->RefCnt == 0);
    }

    return GetAnimationBlockIndex(ab);
}

// 0x4D3ED0
void CAnimManager::RemoveLastAnimFile() {
    const auto ab = &GetAnimBlocks()[--ms_numAnimBlocks];
    ms_numAnimations = ab->FirstAnimIdx;
    for (auto i = 0u; i < ab->NumAnims; i++) { // Remove related animations too
        ms_aAnimations[ab->FirstAnimIdx + i].Shutdown();
    }
    ab->IsLoaded = false;
}

// 0x4D3F40
void CAnimManager::RemoveAnimBlock(int32_t index) {
    const auto ab = &GetAnimBlocks()[index];

    for (auto& g : GetAssocGroups()) {
        if (g.m_AnimBlock == ab) {
            g.DestroyAssociations();
        }
    }

    for (auto i = 0u; i < ab->NumAnims; i++) { // Remove related animations too
        ms_aAnimations[ab->FirstAnimIdx + i].Shutdown();
    }

    ab->IsLoaded = false;
    ab->RefCnt  = 0;
}

// 0x4D3FB0
void CAnimManager::AddAnimBlockRef(int32_t index) {
    GetAnimBlocks()[index].RefCnt++;
}

// 0x4D3FD0
void CAnimManager::RemoveAnimBlockRef(int32_t index) {
    GetAnimBlocks()[index].RefCnt--;
    /* see RemoveAnimBlockRefWithoutDelete, logically here should be called RemoveModel or something
    if (--ms_aAnimBlocks[index].usRefs == 0) {
    CStreaming::RemoveModel(IFPToModelId(index));
    }
    */
}

// 0x4D3FF0
void CAnimManager::RemoveAnimBlockRefWithoutDelete(int32_t index) {
    ms_aAnimBlocks[index].RefCnt--;
}

// 0x4D4010
int32_t CAnimManager::GetNumRefsToAnimBlock(int32_t index) {
    return ms_aAnimBlocks[index].RefCnt;
}

// 0x4D41C0
void CAnimManager::UncompressAnimation(CAnimBlendHierarchy* h) {
    if (h->IsRunningCompressed()) { // Keep as compressed?
        if (h->GetTotalTime() == 0.f) {
            h->CalcTotalTimeCompressed();
        }
    } else if (!h->IsUncompressed()) { // Need to uncompress?
        assert(!h->m_Link); // Sanity check
        auto l = ms_AnimCache.Insert(h);
        if (!l) { // No more free links? (This is totally normal as animations aren't compressed back unless the cache is full)
            // Remove least recently used item
            const auto llr = ms_AnimCache.GetTail();
            llr->data->RemoveUncompressedData();

            // TODO: If the anim is still in use this will corrupt the animation data, and (hopefully) hit the assert in `GetKeyFrame`!
            //       There's currently no way for us to tell if the animation is in use, so there's no better solution for now.
            RemoveFromUncompressedCache(llr->data);

            // Now try again, this time it should succeed
            l = ms_AnimCache.Insert(h);
            assert(l);
        }
        h->m_Link = l;
        h->Uncompress();
    } else if (h->m_Link) { // Already uncompressed, mark as recently-used in cache
        h->m_Link->Remove(); // Remove from current position
        ms_AnimCache.Insert(*h->m_Link); // Now re-insert at head
    }
}

// 0x4D42A0
void CAnimManager::RemoveFromUncompressedCache(CAnimBlendHierarchy* h) {
    if (const auto l = h->m_Link) {
        assert(l->data == h);
        ms_AnimCache.Remove(l);
        h->m_Link = nullptr;
    }
}

// 0x4D4410
CAnimBlendAssociation* CAnimManager::BlendAnimation(RpClump* clump, CAnimBlendHierarchy* toBlendHier, int32_t toBlendFlags, float blendDelta) {
    const auto clumpAnimData = RpAnimBlendClumpGetData(clump); // Get running anim data

    CAnimBlendAssociation* running{}; // Running instance of this anim
    bool                   bFadeThisOut = false;
    for (auto l = clumpAnimData->m_AnimList.next; l; l = l->next) {
        const auto a = CAnimBlendAssociation::FromLink(l);
        assert(a->m_BlendHier);
        if (a->m_BlendHier && a->m_BlendHier == toBlendHier) { // Found an instance of this anim running
            running = a;
        } else if (((toBlendFlags & ANIMATION_IS_PARTIAL) != 0) == a->IsPartial()) {
            if (a->m_BlendAmount <= 0.f) {
                a->m_BlendDelta = -1.f;
            } else {
                const auto bd = a->GetBlendAmount() * -blendDelta;
                if ((toBlendFlags & ANIMATION_IS_PARTIAL) == 0 || bd <= a->GetBlendDelta() || (a->m_BlendHier->m_nAnimBlockId && a->m_BlendHier->m_nAnimBlockId == toBlendHier->m_nAnimBlockId)) {
                    a->m_BlendDelta = std::min(-0.05f, bd);
                }
            }

            a->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE);
            bFadeThisOut = true;
        }
    }

    // If already running just re-adjust blend delta (and re-start it if it has finished)
    if (running) {
        running->SetBlendDelta((1.f - running->m_BlendAmount) * blendDelta);
        if (running->HasFinished()) {
            running->Start();
        }
        UncompressAnimation(running->m_BlendHier); // Make sure anim doesn't get removed
        return running;
    }

    // Otherwise create new instance
    const auto a = new CAnimBlendAssociation{clump, toBlendHier};
    a->m_Flags = toBlendFlags;
    a->ReferenceAnimBlock();
    UncompressAnimation(a->m_BlendHier);
    clumpAnimData->m_AnimList.Prepend(&a->m_Link);
    a->Start();
    if (bFadeThisOut || (toBlendFlags & ANIMATION_IS_PARTIAL)) {
        a->SetBlend(0.f, blendDelta);
        UncompressAnimation(toBlendHier); // No need to call this again here, but doesn't hurt....
    } else {
        a->m_BlendAmount = 1.f;
    }
    return a;
}

// 0x4D4610
CAnimBlendAssociation* CAnimManager::BlendAnimation(RpClump* clump, AssocGroupId groupId, AnimationId animId, float blendDelta) {
    const auto clumpAnimData = RpAnimBlendClumpGetData(clump); // Get running anim data

    const auto toBlendAnim             = GetAssocGroups()[groupId].GetAnimation(animId);
    const bool toBlendIsMoving         = toBlendAnim->m_Flags & ANIMATION_IS_SYNCRONISED;
    const bool toBlendIsPartial        = toBlendAnim->m_Flags & ANIMATION_IS_PARTIAL;
    const bool toBlendIsIndestructible = toBlendAnim->m_Flags & ANIMATION_FACIAL;

    CAnimBlendAssociation *running{}, *movingAnim{}; // Running instance of this anim, and the last moving anim in the chain
    bool                   bFadeThisOut = false;
    for (auto l = clumpAnimData->m_AnimList.next; l; l = l->next) {
        const auto a = CAnimBlendAssociation::FromLink(l);

        if (toBlendIsMoving && a->IsSyncronised()) {
            movingAnim = a;
        }

        if (a->m_AnimId == animId && a->m_AnimGroupId == groupId) {
            running = a;
        } else if (toBlendIsPartial == a->IsPartial() && toBlendIsIndestructible == a->IsFacial()) {
            if (a->m_BlendAmount <= 0.f) {
                a->m_BlendDelta = -1.f;
            } else {
                const auto bd = a->GetBlendAmount() * -blendDelta;
                if (bd <= a->GetBlendDelta() || !toBlendIsPartial) {
                    a->m_BlendDelta = std::min(-0.05f, bd);
                }
            }
            a->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE);
            bFadeThisOut = true;
        }
    }

    // If already running just re-adjust blend delta (and start it if it has finished)
    if (running) {
        running->SetBlendDelta((1.f - running->GetBlendAmount()) * blendDelta);
        if (running->HasFinished()) {
            running->Start();
        }
        UncompressAnimation(running->m_BlendHier);
        return running;
    }

    // Otherwise create new anim, and possibly sync it with the last moving anim of the clump
    const auto anim = AddAnimationAndSync(clump, movingAnim, groupId, animId);
    if (bFadeThisOut || toBlendIsPartial) {
        anim->SetBlend(0.f, blendDelta);
        UncompressAnimation(anim->m_BlendHier);
    } else {
        anim->m_BlendAmount = 1.f;
    }

    return anim;
}

//! @notsa
uint32_t CAnimManager::GetAnimIndex(const CAnimBlendHierarchy* h) {
    const auto idx = h - ms_aAnimations.data();
    assert(idx >= 0 && idx <= ms_numAnimations);
    return static_cast<uint32_t>(idx);
}

//! @notsa
bool CAnimManager::IsAnimInBlock(const CAnimBlendHierarchy* h, const CAnimBlock* b) {
    const auto animIdx = static_cast<int32_t>(h->GetIndex());
    return animIdx >= b->FirstAnimIdx && animIdx <= b->FirstAnimIdx + static_cast<int32_t>(b->NumAnims);
}

//! @notsa
void CAnimManager::StreamAnimBlock(const char* blck, bool shouldBeLoaded, bool& isLoaded) {
    if (shouldBeLoaded && !isLoaded) {
        RemoveAnimBlockRef(GetAnimationBlockIndex(blck));
        isLoaded = false;
    } else if (!shouldBeLoaded && isLoaded) {
        const auto blkIdx = GetAnimationBlockIndex(blck);
        if (GetAnimBlocks()[blkIdx].IsLoaded) {
            AddAnimBlockRef(blkIdx);
            isLoaded = true;
        } else {
            CStreaming::RequestModel(IFPToModelId(blkIdx), STREAMING_KEEP_IN_MEMORY);
        }
    }
}

// 0x4D5620
void CAnimManager::LoadAnimFiles() {
    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, "ANIM\\PED.IFP");
    assert(stream);
    LoadAnimFile(stream, true);
    RwStreamClose(stream, nullptr);

    ms_aAnimAssocGroups = new CAnimBlendAssocGroup[ms_numAnimAssocDefinitions];
    CreateAnimAssocGroups();
}

//! @notsa - Helper
auto CAnimManager::GetOrCreateAnimBlock(const char* name, uint32_t numAnims) {
    CAnimBlock* ab;
    if ((ab = GetAnimationBlock(name))) { // Block already exists, initialize it if necessary
        if (!ab->NumAnims) {
            ab->NumAnims     = numAnims;
            ab->FirstAnimIdx = ms_numAnimations;
        }
    } else { // Create block
        ab = &ms_aAnimBlocks[ms_numAnimBlocks++];

        StrCopyN(ab->Name, MAX_ANIM_BLOCK_NAME, name);
        ab->NumAnims     = numAnims;
        ab->FirstAnimIdx = ms_numAnimations;
        ab->GroupId      = GetFirstAssocGroup(ab->Name);
    }
    ab->IsLoaded = true;
    return std::make_tuple(ab, GetAnimationBlockIndex(ab));
}

// 0x4D47F0
void CAnimManager::LoadAnimFile(RwStream* stream, bool loadCompressed, const char (*uncompressedAnimations)[32]) {
    IFPSectionHeader h;
    RwStreamRead(stream, &h, sizeof(h));
    switch (h.ID) {
    case MakeFourCC("ANP3"):
    case MakeFourCC("ANP2"):
        LoadAnimFile_ANP23(stream, h, loadCompressed, h.ID == MakeFourCC("ANP3"));
        return;
    case MakeFourCC("ANPK"):
        LoadAnimFile_ANPK(stream, h, loadCompressed, uncompressedAnimations);
        return;
    default:
        assert(false && "Unknown IFP section");
        return;
    }
}

// Code from 0x4D4863
// NOTE (from gta-reversed): this was written based off the docs on gtamods;
// no ANPK anims were found to test it against, so treat it as unverified.
void CAnimManager::LoadAnimFile_ANPK(RwStream* s, const IFPSectionHeader& h, bool bLoadCompressed, const char (*pLoadUncompressed)[32]) {
    (void)h;
    using namespace ANPK;

    // Read ANPK section
    const auto hANPK    = RwStreamRead<IFPSectionHeader>(s);
    const auto infoAnim = RwStreamRead<Info<MAX_ANIM_BLOCK_NAME>>(s, RoundTo4(hANPK.Size)); // information : INFO<TAnimation>

    const auto [ablock, ablockIdx] = GetOrCreateAnimBlock(infoAnim.Name, infoAnim.Num);

    // Read TAnimation's for this block
    for (size_t animN = 0; animN < infoAnim.Num; animN++) {
        const auto hier = &ms_aAnimations[ablock->FirstAnimIdx + animN];

        /** TAnimation - Each entry represents an in-game animation
        * AnimName : NAME
        * AnimData : DGAN
        **/

        //
        // Read `TAnimation::AnimName`
        //
        {
            char name[64];
            const auto hAnimName = RwStreamRead<IFPSectionHeader>(s);
            assert(hAnimName.ID == MakeFourCC("NAME"));
            RwStreamRead(s, name, RoundTo4(hAnimName.Size));
            hier->SetName(name);
        }

        // Check if this animation is compressed.
        // (Decomp @004d47f0 guards this scan with a null check on the list.)
        bool isCompressed = bLoadCompressed;
        if (pLoadUncompressed) {
            for (size_t k = 0; pLoadUncompressed[k][0]; k++) {
                if (CKeyGen::GetUppercaseKey(pLoadUncompressed[k]) == hier->GetHashKey()) {
                    isCompressed = false;
                    break;
                }
            }
        }
        hier->m_nAnimBlockId    = ablockIdx;
        hier->m_bIsCompressed   = isCompressed;
        hier->m_bKeepCompressed = false;

        //
        // Read `TAnimation::AnimData`
        //

        /** DGAN
        * AnimInfo : INFO<CPAN>
        **/

        const auto hAnimData = RwStreamRead<IFPSectionHeader>(s); // DGAN header
        assert(hAnimData.ID == MakeFourCC("DGAN"));

        //
        // Read `DGAN::AnimInfo`
        //
        const auto hAnimInfo = RwStreamRead<IFPSectionHeader>(s); // INFO header
        assert(hAnimInfo.ID == MakeFourCC("INFO"));
        const auto animInfo = RwStreamRead<Info<64>>(s, RoundTo4(hAnimInfo.Size)); // INFO data

        hier->m_nSeqCount  = animInfo.Num;
        hier->m_pSequences = new CAnimBlendSequence[hier->m_nSeqCount]; // Yes, they used `new`

        // Read `DGAN::AnimInfo` (CPAN) entries (The sequences of the animation)
        for (size_t seqN = 0; seqN < animInfo.Num; seqN++) {
            const auto seq = &hier->m_pSequences[seqN];

            /** CPAN
            * ObjectInfo : ANIM
            **/

            const auto hObjectInfo = RwStreamRead<IFPSectionHeader>(s); // CPAN header
            assert(hObjectInfo.ID == MakeFourCC("CPAN"));

            //
            // Read `CPAN::ObjectInfo`
            //

            /** ANIM
            * ObjectName : TString            // Also the name of the bone (Because of this fact that this string uses 28 bytes by default.)
            * Frames     : INT32              // Number of frames
            * Unknown    : INT32              // Usually 0
            * Next       : INT32              // Next sibling
            * Prev       : INT32              // Previous sibling
            * FrameData  : KRTS / KRT0 / KR00 // Key frames
            **/

            struct Anim {
                char       ObjName[28]; // 00 - Name of this sequence
                uint32_t   NumFrames;   // 28 - Number of (key)frames
                uint32_t   Next;        // 32 - Next sibling
                uint32_t   Prev;        // 36 - Previous sibling
                eBoneTag32 BoneTag;     // 40 - Only present if `Header.Size == 44`
            };
            const auto [objInfo, hObjInfo] = ReadSection<Anim>(s);
            assert(hObjInfo.ID == MakeFourCC("ANIM"));

            // Set sequence name
            seq->SetName(objInfo.ObjName);

            // Set bone tag if available
            if (hObjInfo.Size == sizeof(Anim)) {
                seq->SetBoneTag(objInfo.BoneTag);
            }

            //
            // Read `ANIM::FrameData` (If any)
            //

            if (!objInfo.NumFrames) {
                continue;
            }

            /** KR00
            * DeltaTime : FLOAT
            * Rot       : CQuaternion
            **/

            /** KRT0 : KR00
            * Pos : CVector
            **/

            /** KRTS : KRT0
            * Scale : CVector // Read, but ignored
            **/

            // Frame data header ("KFRM" -> Key Frame)
            const auto hKFRM = RwStreamRead<IFPSectionHeader>(s);
            assert(hKFRM.ID == MakeFourCC("KRTS") || hKFRM.ID == MakeFourCC("KRT0") || hKFRM.ID == MakeFourCC("KR00"));

            // Frame properties
            const auto hasRotation    = hKFRM.IDFourCC[1] == 'R';
            const auto hasTranslation = hKFRM.IDFourCC[2] == 'T';
            const auto hasScale       = hKFRM.IDFourCC[3] == 'S';

            assert(hasRotation); // Rotation must always be present

            // Allocate frame data
            seq->SetNumFrames(objInfo.NumFrames, hasTranslation, isCompressed, nullptr);

            // Read frame data from the stream
            for (size_t kfN = 0; kfN < objInfo.NumFrames; kfN++) {
                const auto SetKF = [&](auto* kf) {
                    kf->Rot = RwStreamRead<CQuaternion>(s).Conjugated();
                    if (hasTranslation) {
                        kf->Trans = RwStreamRead<CVector>(s);
                        if (hasScale) {
                            RwStreamSkip(s, sizeof(CVector)); // Scale ignored
                        }
                    }
                    kf->DeltaTime = RwStreamRead<float>(s);
                };
                if (isCompressed) {
                    SetKF(seq->GetCKeyFrame(kfN));
                } else {
                    SetKF(seq->GetUKeyFrame(kfN));
                }
            }

        }

        if (!hier->m_bIsCompressed) {
            hier->RemoveQuaternionFlips();
            hier->CalcTotalTime();
        }
    }
    ms_numAnimations = std::max<int32_t>(ablock->FirstAnimIdx + infoAnim.Num, ms_numAnimations);
}

// NOTE (from gta-reversed): this was written based off the docs on gtamods,
// but it does seem to work.
void CAnimManager::LoadAnimFile_ANP23(RwStream* s, const IFPSectionHeader& h, bool bLoadCompressed, bool isANP3) {
    (void)h;
    (void)bLoadCompressed;
    char blockName[24];
    RwStreamRead(s, blockName, sizeof(blockName));
    const auto numAnims = RwStreamRead<uint32_t>(s);

    const auto [ablock, ablockId] = GetOrCreateAnimBlock(blockName, numAnims);

    for (size_t animN = 0; animN < numAnims; animN++) {
        const auto hier = &ms_aAnimations[ablock->FirstAnimIdx + animN];

        // Animation name
        char aname[24];
        RwStreamRead(s, aname, sizeof(aname));
        hier->SetName(aname);

        // Number of sequences
        const auto numSeq = RwStreamRead<uint32_t>(s);

        // In ANP3 a big chunk of memory is allocated for all frames
        // instead of allocating lots of small chunks
        char* frames = nullptr;
        if (isANP3) {
            const auto size  = RwStreamRead<uint32_t>(s);
            const auto flags = RwStreamRead<uint32_t>(s);

            hier->m_bIsCompressed = flags & 1;

            frames = static_cast<char*>(CMemoryMgr::Malloc(size));
        }

        hier->m_nAnimBlockId    = ablockId;
        hier->m_bKeepCompressed = false;

        // Allocate sequences now
        hier->SetNumSequences(numSeq);

        // Read sequences
        for (size_t seqN = 0; seqN < numSeq; seqN++) {
            const auto seq = &hier->m_pSequences[seqN];

            char seqName[24];
            RwStreamRead(s, seqName, sizeof(seqName));
            const auto frameType = RwStreamRead<uint32_t>(s);
            const auto numFrames = RwStreamRead<uint32_t>(s);
            const auto boneTag   = RwStreamRead<eBoneTag32>(s);

            // Only 1 of these will be valid in the end
            // If BoneTag != -1 then it overwrites the name.
            seq->SetName(seqName);
            seq->SetBoneTag(boneTag);

            // Read frames
            const auto ReadFrames = [&](size_t kfSize, bool hasTranslation, bool compressed) {
                seq->SetNumFrames(numFrames, hasTranslation, compressed, frames);

                const auto memSz = kfSize * numFrames;
                RwStreamRead(s, seq->m_Frames, memSz);
                if (isANP3) {
                    frames += memSz;
                }
            };

            switch (frameType) {
            case 1:  ReadFrames(sizeof(KeyFrame), false, false); break;
            case 2:  ReadFrames(sizeof(KeyFrameTrans), true, false); break;
            case 3:  ReadFrames(sizeof(KeyFrameCompressed), false, true); break;
            case 4:  ReadFrames(sizeof(KeyFrameTransCompressed), true, true); break;
            default: assert(false && "Invalid FrameType"); break;
            }

            if (isANP3) {
                seq->m_bUsingExternalMemory = true;
            }
        }

        if (!hier->m_bIsCompressed) {
            hier->RemoveQuaternionFlips();
            hier->CalcTotalTime();
        }
    }

    ms_numAnimations = std::max<int32_t>(ablock->FirstAnimIdx + numAnims, ms_numAnimations);
}

AnimationId CAnimManager::GetRandomGangTalkAnim() {
    constexpr AnimationId gangTalkAnims[]{
        ANIM_ID_PRTIAL_GNGTLKA,
        ANIM_ID_PRTIAL_GNGTLKB,
        ANIM_ID_PRTIAL_GNGTLKC,
        ANIM_ID_PRTIAL_GNGTLKD,

        ANIM_ID_PRTIAL_GNGTLKE,
        ANIM_ID_PRTIAL_GNGTLKF,
        ANIM_ID_PRTIAL_GNGTLKG,
        ANIM_ID_PRTIAL_GNGTLKH,
    };
    return CGeneral::RandomChoice(gangTalkAnims);
}
