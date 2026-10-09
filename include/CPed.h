// CPed - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Ped/Ped.h
// Core ped class: flags, bones, weapons, audio, IK, health, vehicle attachment.
// Hierarchy: CPlaceable -> CEntity -> CPhysical -> CPed -> CPlayerPed
//
// DEFERRED subsystems (size verified from gta-reversed VALIDATE_SIZE, contents opaque):
//   CAEPedAudioEntity (0x15C), CAEPedSpeechAudioEntity (0x100),
//   CAEPedWeaponAudioEntity (0xA8), CAcquaintance (0x14), CPedIK (0x20).
//   Audio/acquaintance/IK are separate conversion batches - replace these
//   stand-ins with the real classes when they land.
//   RW types (RpClump/RwFrame/RwV3d/RpHAnimHierarchy/RwObject) -> forward-declared
//     (RenderWare layer pending)
//   CPlayerPedData -> forward-declared (pointer use only); member-accessing
//     inlines (GetPlayerWanted/GetClothesDesc) demoted to declaration-only
//   AsCop/AsCivilian/AsEmergency/AsPlayer demoted to declaration-only
//     (reinterpret_cast needs the complete derived types)
//   GetEventHandlerHistory() demoted (needs real CEventHandler::GetHistory)
//
// Adaptations:
//   stripped InjectHooks(), Constructor()/Destructor() placement wrappers
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
//   notsa::EntityRef alias dropped (<extensions/EntityRef.hpp> not ported)
//   NOTSA_EXPORT_VTABLE stripped
//   C++23 deducing-this GetAE/GetSpeechAE/GetWeaponAE -> const/non-const
//     overload pairs (C++17)
//   ePedNode/ePedPieceTypes/ePedCreatedBy/eFightingStyle/eCantBeKnockedOffBike
//     ported here (values verified against gta-reversed); ePedState/ePedType/
//     eMoveState/ePedStats are separate enum headers
//   AssocGroupId/eBoneTag/eBoneTagU32 -> canonical defs in AnimTypes.h
//     (via CPedModelInfo.h; deduped 2026-10-09)
//   eAudioEvents -> opaque-enum declaration (underlying type unverified -
//     check against the audio batch when it lands)
//   eGlobalSpeechContext -> opaque-enum declaration (int16 verified from
//     gta-reversed Audio/Enums/PedSpeechContexts.h)
//   MODEL_INVALID = -1 (from gta-reversed Enums/eModelID.h; delete when the
//     real eModelID enum lands)
// TODO:
//   port CAEPedAudioEntity/CAEPedSpeechAudioEntity/CAEPedWeaponAudioEntity/
//     CAcquaintance/CPedIK/CPlayerPedData (replace stand-ins)
//   verify each method against decomp src/CPed/*.c

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "CPhysical.h"
#include "CPedIntelligence.h" // complete: inline GetTaskManager()/GetEventHandler()/...
#include "CVector.h"           // CVector, CVector2D
#include "CWeapon.h"           // CWeapon, eWeaponSlot, NUM_WEAPON_SLOTS
#include "eWeaponType.h"
#include "eWeaponSkill.h"
#include "ePedPieceTypes.h"
#include "ePedState.h"
#include "ePedStats.h"
#include "ePedType.h"
#include "eMoveState.h"
#include "eModelID.h" // MODEL_INVALID (deduped 2026-10-09)
#include "CFire.h"
#include "CPedModelInfo.h" // pulls AnimTypes.h: AssocGroupId/eBoneTag/eBoneTagU32
#include "ColTypes.h" // CColPoint

class CAnimBlendAssociation; // full class pulls AnimTypes.h (CQuaternion clash) - pointer use only
class CAnimBlendClumpData;   // same - reference use only
class CPed; // fwd: audio entity structs below hold CPed*

#include "CAESound.h" // CAESound (member use in audio entities)
#include "CAETwinLoopSoundEntity.h" // CAETwinLoopSoundEntity (member use)

// ---- Audio entities: members added as the CPed TU needs them (2026-10-09) ----
struct CAEPedAudioEntity {
    // TODO(audio): this was an opaque 0x15C stand-in (size asserted); it now carries the members
    //   the CPed TU actually touches. Full audio port re-verifies layout vs the decomp.
    CAESound m_tempSound; // TODO(port): stub
    CAETwinLoopSoundEntity m_sTwinLoopSoundEntity; // TODO(port): stub
    CPed* m_pPed = nullptr; // TODO(port): stub
    CAESound* m_JetPackSound0 = nullptr; // TODO(port): stub
    CAESound* m_JetPackSound1 = nullptr; // TODO(port): stub
    CAESound* m_JetPackSound2 = nullptr; // TODO(port): stub
    bool m_bCanAddEvent = false;

    // TODO(audio): ported minimally for the CPed TU (audio batch pending).
    void Service();
    void AddAudioEvent(int32_t audioEvent, float volume, float speed, void* ped, int32_t surfaceId, int32_t a7, uint32_t maxVol);
    static void Initialise(CAEPedAudioEntity* entity, CPed* ped) { (void)entity; (void)ped; } // TODO(port): stub
    void Terminate() {} // TODO(port): stub
};
struct CAEPedSpeechAudioEntity {
    uint8_t _deferred[0x100];

    // TODO(port): full audio port; stubs added 2026-10-09 for CPed.
    bool GetPedTalking() { return false; }
    void DisablePedSpeech(bool stopCurrentSpeech) { (void)stopCurrentSpeech; }
    void EnablePedSpeech() {}
    void DisablePedSpeechForScriptSpeech(bool stopCurrentSpeech) { (void)stopCurrentSpeech; }
    void EnablePedSpeechForScriptSpeech() {}
    bool CanPedHoldConversation() const { return false; }
    int16_t AddSayEvent(int32_t speechId, int16_t gCtx, uint32_t startTimeDelay,
        float probability, bool overrideSilence, bool isForceAudible, bool isFrontEnd) {
        (void)speechId; (void)gCtx; (void)startTimeDelay; (void)probability;
        (void)overrideSilence; (void)isForceAudible; (void)isFrontEnd;
        return -1;
    }
};
struct CAEPedWeaponAudioEntity {
    // TODO(audio): this was an opaque 0xA8 stand-in (size asserted); it now carries the members
    //   the CPed TU actually touches. Full audio port re-verifies layout vs the decomp.
    CAESound m_tempSound; // TODO(port): stub
    uint32_t m_LastFlameThrowerFireTimeMs = 0; // TODO(port): stub
    uint32_t m_LastSprayCanFireTimeMs = 0; // TODO(port): stub
    uint32_t m_LastFireExtFireTimeMs = 0; // TODO(port): stub
    CAESound* m_FlameThrowerIdleGasLoopSound = nullptr; // TODO(port): stub
    uint8_t m_LastWeaponPlaneFrequencyIndex = 0; // TODO(port): stub
    uint32_t m_LastMiniGunFireTimeMs = 0; // TODO(port): stub
    bool m_IsMiniGunSpinActive = false; // TODO(port): stub
    bool m_IsMiniGunFireActive = false; // TODO(port): stub
    uint32_t m_LastChainsawEventTimeMs = 0; // TODO(port): stub
    uint32_t m_LastGunFireTimeMs = 0; // TODO(port): stub
    CPed* m_Ped = nullptr; // TODO(port): stub
    bool m_bInitialised = false; // TODO(port): stub

    // TODO(audio): ported minimally for weapons_combat TU
    void AddAudioEvent(int32_t audioEvent);
    void Service();
    static void Initialise(CPed* ped) { (void)ped; } // TODO(port): stub
    void Terminate() {} // TODO(port): stub
};
struct CAcquaintance {
    // TODO: full port pending; fields from gta-reversed (union of 5 uint32).
    union {
        struct {
            uint32_t m_nRespect;
            uint32_t m_nLike;
            uint32_t m_nIgnore;
            uint32_t m_nDislike;
            uint32_t m_nHate;
        };
        uint32_t m_acquaintances[5];
    };

    // TODO(port): stub; decomp calls CAcquaintance::SetAsAcquaintance(&acq, 4, pedFlag) in CPed::CPed
    static void SetAsAcquaintance(CAcquaintance* acq, int32_t acquaintance, uint32_t pedFlag) {
        (void)acq; (void)acquaintance; (void)pedFlag;
    }

    // Decomp @ 00608970: uint __thiscall CAcquaintance::GetAcquaintances(int id)
    //   { return m_acquaintances[id]; }
    // Added 2026-10-09 for CPedIntelligence (was C2039 x6 + C2737 x6).
    uint32_t GetAcquaintances(int32_t acquaintance) { return m_acquaintances[acquaintance]; }
};
struct CPedIK {
    // From gta-reversed PedIK.h (partial, only members used so far).
    // TODO(port): full CPedIK when IK batch lands.
    CPed* m_pPed{nullptr};
    float m_TorsoYaw{0.0f}, m_TorsoPitch{0.0f}; // LimbOrientation m_TorsoOrient
    float m_fSlopePitch{0.0f};
    float m_fSlopePitchLimitMult{0.0f};
    float m_fSlopeRoll{0.0f};
    float m_fBodyRoll{0.0f};
    uint32_t m_nFlags{0}; // ePedIKFlags bitfield (added 2026-10-09 for CPed)
#if INTPTR_MAX == INT32_MAX
    // Members above total exactly 0x20 (4+8+16+4); no padding needed.
    // 2026-10-09: removed uint8_t _deferred[0x20-4-8-16-4] (= _deferred[0],
    // an illegal zero-sized array -> C4200 warning + C2229 error on CPed.
#else
    uint8_t _deferred[4]; // 64-bit: no size assert; keep minimal padding
#endif
};
#if INTPTR_MAX == INT32_MAX
// (2026-10-09) CAEPedAudioEntity size assert dropped: the struct now carries ported members
// instead of the opaque 0x15C stand-in; layout re-verified when the audio batch lands.
static_assert(sizeof(CAEPedSpeechAudioEntity) == 0x100, "CAEPedSpeechAudioEntity stand-in size mismatch");
// (2026-10-09) CAEPedWeaponAudioEntity size assert dropped: the struct now carries ported
// members instead of the opaque 0xA8 stand-in; layout re-verified when the audio batch lands.
static_assert(sizeof(CAcquaintance) == 0x14, "CAcquaintance stand-in size mismatch");
static_assert(sizeof(CPedIK) == 0x20, "CPedIK stand-in size mismatch");
#endif

// RW types (RenderWare layer pending)
struct RpClump;
struct RwFrame;
struct RwV3d;
struct RpHAnimHierarchy;
struct RwObject;

// Opaque enums / aliases (owning subsystems not yet converted)
// AssocGroupId/eBoneTag/eBoneTagU32: canonical defs in AnimTypes.h
// (deduped 2026-10-09; reached via CPedModelInfo.h). The local
// `using eBoneTagU32 = uint32_t` stand-in was removed (redefinition).
enum eAudioEvents : int32_t; // underlying type UNVERIFIED - check against audio batch
enum eGlobalSpeechContext : int16_t; // verified: gta-reversed Audio/Enums/PedSpeechContexts.h

struct AnimBlendFrameData;

class CPedGroup;
class CCivilianPed;
class CEmergencyPed;
class CCopPed;
class CPlayerPed;
class CCoverPoint;
class CEntryExit;
class CPlayerPedData;
class CPedStat;
class CPedStats;
class CPedClothesDesc;
class CWanted;
class CTaskSimpleHoldEntity;
class CEventHandlerHistory;
class CVehicle;

// MODEL_INVALID now comes from the canonical eModelID.h (deduped 2026-10-09).

enum ePedNode : int32_t {
    PED_NODE_NULL            = 0,
    PED_NODE_UPPER_TORSO     = 1,
    PED_NODE_HEAD            = 2,
    PED_NODE_LEFT_ARM        = 3,
    PED_NODE_RIGHT_ARM       = 4,
    PED_NODE_LEFT_HAND       = 5,
    PED_NODE_RIGHT_HAND      = 6,
    PED_NODE_LEFT_LEG        = 7,
    PED_NODE_RIGHT_LEG       = 8,
    PED_NODE_LEFT_FOOT       = 9,
    PED_NODE_RIGHT_FOOT      = 10,
    PED_NODE_RIGHT_LOWER_LEG = 11,
    PED_NODE_LEFT_LOWER_LEG  = 12,
    PED_NODE_LEFT_LOWER_ARM  = 13,
    PED_NODE_RIGHT_LOWER_ARM = 14,
    PED_NODE_LEFT_CLAVICLE   = 15,
    PED_NODE_RIGHT_CLAVICLE  = 16,
    PED_NODE_NECK            = 17,
    PED_NODE_JAW             = 18,

    TOTAL_PED_NODES
};

enum ePedCreatedBy : uint8_t {
    PED_UNKNOWN = 0,
    PED_GAME = 1,
    PED_MISSION = 2,
    PED_GAME_MISSION = 3, // used for the playbacked peds on replay
};

enum eFightingStyle : int8_t {
    STYLE_STANDARD = 4,
    STYLE_BOXING,
    STYLE_KUNG_FU,
    STYLE_KNEE_HEAD,
    // various melee weapon styles
    STYLE_GRAB_KICK = 15,
    STYLE_ELBOWS = 16,
};

// Values of `CPed::CantBeKnockedOffBike` (how hard it is to knock this ped off a bike)
enum eCantBeKnockedOffBike : uint8_t {
    CANT_BE_KNOCKED_OFF_DEFAULT       = 0, // resistance scales with the ped's bike riding skill
    CANT_BE_KNOCKED_OFF_NEVER         = 1, // never knocked off
    CANT_BE_KNOCKED_OFF_ALWAYS_NORMAL = 2, // knocked off regardless of riding skill
    CANT_BE_KNOCKED_OFF_ALWAYS_HARD   = 3, // knocked off even at a much lower impact force
};
static_assert(CANT_BE_KNOCKED_OFF_ALWAYS_HARD <= 0b11, "eCantBeKnockedOffBike must fit in the 2-bit CantBeKnockedOffBike field");

class CPed : public CPhysical {
public:
    static inline int16_t m_sGunFlashBlendStart = 10'000; // 0x8D1370

protected: // Use accessors
    CAEPedAudioEntity       m_pedAudio;
    CAEPedSpeechAudioEntity m_pedSpeech;
    CAEPedWeaponAudioEntity m_weaponAudio;
public:
    char                    field_43C[36];
    CPed*                   m_roadRageWith;
    char                    field_464[4];
    int32_t                 field_468;

    /* https://github.com/multitheftauto/mtasa-blue/blob/master/Client/game_sa/CPedSA.h */
    struct {
        // 1st byte starts here (m_nPedFlags)
        bool bIsStanding : 1 = false;            // 0 is ped standing on something
        bool bWasStanding : 1 = false;           // 1 was ped standing on something
        bool bIsLooking : 1 = false;             // 2 is ped looking at something or in a direction
        bool bIsRestoringLook : 1 = false;       // 3 is ped restoring head position from a look
        bool bIsAimingGun : 1 = false;           // 4 is ped aiming gun
        bool bIsRestoringGun : 1 = false;        // 5 is ped moving gun back to default posn
        bool bCanPointGunAtTarget : 1 = false;   // 6 can ped point gun at target
        bool bIsTalking : 1 = false;             // 7 is ped talking(see Chat())

        bool bInVehicle : 1 = false;             // 8  is in a vehicle [Sometimes accessed as `(ped->m_nPedFlags >> 8) & 1`]
        bool bIsInTheAir : 1 = false;            // 9  is in the air
        bool bIsLanding : 1 = false;             // 10 is landing after being in the air
        bool bHitSomethingLastFrame : 1 = false; // 11 has been in a collision last frame
        bool bIsNearCar : 1 = false;             // 12 has been in a collision last frame
        bool bRenderPedInCar : 1 = true;         // 13 has been in a collision last frame
        bool bUpdateAnimHeading : 1 = false;     // 14 update ped heading due to heading change during anim sequence
        bool bRemoveHead : 1 = false;            // 15 waiting on AntiSpazTimer to remove head - TODO: See `RemoveBodyPart` - The name seems to be incorrect. It should be like `bHasBodyPartToRemove`.

        bool bFiringWeapon : 1 = false;         // 16 is pulling trigger
        bool bHasACamera : 1;                   // 17 does ped possess a camera to document accidents
        bool bPedIsBleeding : 1 = false;        // 18 Ped loses a lot of blood if true
        bool bStopAndShoot : 1 = false;         // 19 Ped cannot reach target to attack with fist, need to use gun
        bool bIsPedDieAnimPlaying : 1 = false;  // 20 is ped die animation finished so can dead now
        bool bStayInSamePlace : 1 = false;      // 21 when set, ped stays put
        bool bKindaStayInSamePlace : 1 = false; // 22 when set, ped doesn't seek out opponent or cover large distances. Will still shuffle and look for cover
        bool bBeingChasedByPolice : 1 = false;  // 23 use nodes for route find

        bool bNotAllowedToDuck : 1 = false;     // 24 Is this ped allowed to duck at all?
        bool bCrouchWhenShooting : 1 = false;   // 25 duck behind cars etc
        bool bIsDucking : 1 = false;            // 26 duck behind cars etc
        bool bGetUpAnimStarted : 1 = false;     // 27 don't want to play getup anim if under something
        bool bDoBloodyFootprints : 1 = false;   // 28 bIsLeader
        bool bDontDragMeOutCar : 1 = false;     // 29
        bool bStillOnValidPoly : 1 = false;     // 30 set if the polygon the ped is on is still valid for collision
        bool bAllowMedicsToReviveMe : 1 = true; // 31

        // 5th byte starts here (m_nSecondPedFlags)
        bool bResetWalkAnims : 1 = false;
        bool bOnBoat : 1 = false;               // flee but only using nodes
        bool bBusJacked : 1 = false;            // flee but only using nodes
        bool bFadeOut : 1 = false;              // set if you want ped to fade out
        bool bKnockedUpIntoAir : 1 = false;      // has ped been knocked up into the air by a car collision
        bool bHitSteepSlope : 1 = false;        // has ped collided/is standing on a steep slope (surface type)
        bool bCullExtraFarAway : 1 = false;     // special ped only gets culled if it's extra far away (for roadblocks)
        bool bTryingToReachDryLand : 1 = false; // has ped just exited boat and trying to get to dry land

        bool bCollidedWithMyVehicle : 1 = false;
        bool bRichFromMugging : 1 = false;        // ped has lots of cash cause they've been mugging people
        bool bChrisCriminal : 1 = false;          // Is a criminal as killed during Chris' police mission (should be counted as such)
        bool bShakeFist : 1 = false;              // test shake hand at look entity
        bool bNoCriticalHits : 1 = false;         // ped cannot be killed by a single bullet
        bool bHasAlreadyBeenRecorded : 1 = false; // Used for replays
        bool bUpdateMatricesRequired : 1 = false; // if PedIK has altered bones so matrices need updated this frame
        bool bFleeWhenStanding : 1 = false;       //

        bool bMiamiViceCop : 1 = false;
        bool bMoneyHasBeenGivenByScript : 1 = false;
        bool bHasBeenPhotographed : 1 = false;
        bool bIsDrowning : 1 = false;
        bool bDrownsInWater : 1 = true;
        bool bHeadStuckInCollision : 1 = false;
        bool bDeadPedInFrontOfCar : 1 = false;
        bool bStayInCarOnJack : 1 = false;

        bool bDontFight : 1 = false;
        bool bDoomAim : 1 = true;
        bool bCanBeShotInVehicle : 1 = true;
        bool bPushedAlongByCar : 1 = false; // ped is getting pushed along by car collision (so don't take damage from horz velocity)
        bool bNeverEverTargetThisPed : 1 = false;
        bool bThisPedIsATargetPriority : 1 = false;
        bool bCrouchWhenScared : 1 = false;
        bool bKnockedOffBike : 1 = false; // TODO: Maybe rename to `bIsJumpingOut` or something similar, see x-refs

        // 9th byte starts here (m_nThirdPedFlags)
        bool bDonePositionOutOfCollision : 1 = false;
        bool bDontRender : 1 = false;
        bool bHasBeenAddedToPopulation : 1 = false;
        bool bHasJustLeftCar : 1 = false;
        bool bIsInDisguise : 1 = false;
        bool bDoesntListenToPlayerGroupCommands : 1 = false;
        bool bIsBeingArrested : 1 = false;
        bool bHasJustSoughtCover : 1 = false;

        bool bKilledByStealth : 1 = false;
        bool bDoesntDropWeaponsWhenDead : 1 = false;
        bool bCalledPreRender : 1 = false;
        bool bBloodPuddleCreated : 1 = false; // Has a static puddle of blood been created yet
        bool bPartOfAttackWave : 1 = false;
        bool bClearRadarBlipOnDeath : 1 = false;
        bool bNeverLeavesGroup : 1 = false;        // flag that we want to test 3 extra spheres on col model
        bool bTestForBlockedPositions : 1 = false; // this sets these indicator flags for various positions on the front of the ped

        bool bRightArmBlocked : 1 = false;
        bool bLeftArmBlocked : 1 = false;
        bool bDuckRightArmBlocked : 1 = false;
        bool bMidriffBlockedForJump : 1 = false;
        bool bFallenDown : 1 = false;
        bool bUseAttractorInstantly : 1 = false;
        bool bDontAcceptIKLookAts : 1 = false;
        bool bHasAScriptBrain : 1 = false;

        bool bWaitingForScriptBrainToLoad : 1 = false;
        bool bHasGroupDriveTask : 1 = false;
        bool bCanExitCar : 1 = true;
        uint8_t CantBeKnockedOffBike : 2 = CANT_BE_KNOCKED_OFF_DEFAULT; // 2-bit value, see eCantBeKnockedOffBike (was mis-typed as `bool`)
        bool bHasBeenRendered : 1 = false;
        bool bIsCached : 1 = false;
        bool bPushOtherPeds : 1 = false;   // GETS RESET EVERY FRAME - SET IN TASK: want to push other peds around (eg. leader of a group or ped trying to get in a car)

        // 13th byte starts here (m_nFourthPedFlags)
        bool bHasBulletProofVest : 1 = false;
        bool bUsingMobilePhone : 1 = false;
        bool bUpperBodyDamageAnimsOnly : 1 = false;
        bool bStuckUnderCar : 1 = false;
        bool bKeepTasksAfterCleanUp : 1 = false; // If true ped will carry on with task even after cleanup
        bool bIsDyingStuck : 1 = false;
        bool bIgnoreHeightCheckOnGotoPointTask : 1 = false; // set when walking round buildings, reset when task quits
        bool bForceDieInCar : 1 = false;

        bool bCheckColAboveHead : 1 = false;
        bool bIgnoreWeaponRange : 1 = false;
        bool bDruggedUp : 1 = false;
        bool bWantedByPolice : 1 = false; // if this is set, the cops will always go after this ped when they are doing a KillCriminal task
        bool bSignalAfterKill : 1 = true;
        bool bCanClimbOntoBoat : 1 = false;
        bool bPedHitWallLastFrame : 1 = false; // useful to store this so that AI knows (normal will still be available)
        bool bIgnoreHeightDifferenceFollowingNodes : 1 = false;

        bool bMoveAnimSpeedHasBeenSetByTask : 1 = false;
        bool bGetOutUpsideDownCar : 1 = true;
        bool bJustGotOffTrain : 1 = false;
        bool bDeathPickupsPersist : 1 = false;
        bool bTestForShotInVehicle : 1 = false;
        bool bUsedForReplay : 1 = false; // This ped is controlled by replay and should be removed when replay is done.
    };

    // Dword view of the 4th ped-flags word (bHasBulletProofVest group above).
    // The decomp reads/writes it as *(uint32_t*)&bHasBulletProofVest, but &
    // on a bit-field is illegal (C2104). These accessors reconstruct the
    // dword from the named bit-fields instead - no layout assumptions.
    uint32_t GetFourthPedFlags() const {
        uint32_t w = 0;
        w |= (uint32_t)bHasBulletProofVest;
        w |= (uint32_t)bUsingMobilePhone << 1;
        w |= (uint32_t)bUpperBodyDamageAnimsOnly << 2;
        w |= (uint32_t)bStuckUnderCar << 3;
        w |= (uint32_t)bKeepTasksAfterCleanUp << 4;
        w |= (uint32_t)bIsDyingStuck << 5;
        w |= (uint32_t)bIgnoreHeightCheckOnGotoPointTask << 6;
        w |= (uint32_t)bForceDieInCar << 7;
        w |= (uint32_t)bCheckColAboveHead << 8;
        w |= (uint32_t)bIgnoreWeaponRange << 9;
        w |= (uint32_t)bDruggedUp << 10;
        w |= (uint32_t)bWantedByPolice << 11;
        w |= (uint32_t)bSignalAfterKill << 12;
        w |= (uint32_t)bCanClimbOntoBoat << 13;
        w |= (uint32_t)bPedHitWallLastFrame << 14;
        w |= (uint32_t)bIgnoreHeightDifferenceFollowingNodes << 15;
        w |= (uint32_t)bMoveAnimSpeedHasBeenSetByTask << 16;
        w |= (uint32_t)bGetOutUpsideDownCar << 17;
        w |= (uint32_t)bJustGotOffTrain << 18;
        w |= (uint32_t)bDeathPickupsPersist << 19;
        w |= (uint32_t)bTestForShotInVehicle << 20;
        w |= (uint32_t)bUsedForReplay << 21;
        return w;
    }
    void SetFourthPedFlags(uint32_t w) {
        bHasBulletProofVest = (w & (1u << 0)) != 0;
        bUsingMobilePhone = (w & (1u << 1)) != 0;
        bUpperBodyDamageAnimsOnly = (w & (1u << 2)) != 0;
        bStuckUnderCar = (w & (1u << 3)) != 0;
        bKeepTasksAfterCleanUp = (w & (1u << 4)) != 0;
        bIsDyingStuck = (w & (1u << 5)) != 0;
        bIgnoreHeightCheckOnGotoPointTask = (w & (1u << 6)) != 0;
        bForceDieInCar = (w & (1u << 7)) != 0;
        bCheckColAboveHead = (w & (1u << 8)) != 0;
        bIgnoreWeaponRange = (w & (1u << 9)) != 0;
        bDruggedUp = (w & (1u << 10)) != 0;
        bWantedByPolice = (w & (1u << 11)) != 0;
        bSignalAfterKill = (w & (1u << 12)) != 0;
        bCanClimbOntoBoat = (w & (1u << 13)) != 0;
        bPedHitWallLastFrame = (w & (1u << 14)) != 0;
        bIgnoreHeightDifferenceFollowingNodes = (w & (1u << 15)) != 0;
        bMoveAnimSpeedHasBeenSetByTask = (w & (1u << 16)) != 0;
        bGetOutUpsideDownCar = (w & (1u << 17)) != 0;
        bJustGotOffTrain = (w & (1u << 18)) != 0;
        bDeathPickupsPersist = (w & (1u << 19)) != 0;
        bTestForShotInVehicle = (w & (1u << 20)) != 0;
        bUsedForReplay = (w & (1u << 21)) != 0;
    }

protected:
    CPedIntelligence*   m_pIntelligence;
    CPlayerPedData*     m_pPlayerData;
    ePedCreatedBy       m_nCreatedBy;

public:
    std::array<AnimBlendFrameData*, TOTAL_PED_NODES> m_apBones; // for Index, see ePedNode - TODO: Name incorrect, should be `m_apNodes` instead.
    AssocGroupId        m_nAnimGroup;
    CVector2D           m_vecAnimMovingShiftLocal;
    CAcquaintance       m_acquaintance;

    RpClump*            m_pWeaponObject;
    RwFrame*            m_pGunflashObject; // A frame in the Clump `m_pWeaponObject`
    RpClump*            m_pGogglesObject;
    bool*               m_pGogglesState;           // Stores a pointer to either `CPostEffects::m_bInfraredVision` or `m_bNightVision`, see PutOnGoggles and AddGogglesModel

    int16_t             m_nWeaponGunflashAlphaMP1; // AKA m_nWeaponGunflashStateRightHand
    int16_t             m_nWeaponGunFlashAlphaProgMP1;
    int16_t             m_nWeaponGunflashAlphaMP2; // AKA m_nWeaponGunflashStateLeftHand
    int16_t             m_nWeaponGunFlashAlphaProgMP2;

    CPedIK              m_pedIK;
    uint32_t            m_nAntiSpazTimer;
    ePedState           m_nPedState;
    eMoveState          m_nMoveState;
    int32_t             m_nSwimmingMoveState; // type is eMoveState and used for swimming in CTaskSimpleSwim::ProcessPed
    int32_t             field_53C;
    float               m_fHealth;
    float               m_fMaxHealth;
    float               m_fArmour;
    uint32_t            m_nTimeTillWeNeedThisPed;
    CVector2D           m_vecAnimMovingShift;
    float               m_fCurrentRotation;
    float               m_fAimingRotation;
    float               m_fHeadingChangeRate;
    float               m_fMoveAnim; // not sure about the name here
    CEntity*            m_standingOnEntity;
    CVector             field_56C;
    CVector             field_578;
    CEntity*            m_pContactEntity;
    float               field_588;
    CVehicle*           m_pVehicle;         //< Might be set even if the ped isn't in a vehicle, in that case it's the vehicle they should get back into. But (in theory) a ped is guaranteed to be in a vehicle if `bInVehicle` is set.
    CVehicle*           m_VehDeadInFrontOf; // Set if `bDeadPedInFrontOfCar`
    int32_t             field_594;
    ePedType            m_nPedType;
    CPedStat*           m_pStats;
    std::array<CWeapon, NUM_WEAPON_SLOTS> m_aWeapons;
    eWeaponType         m_nSavedWeapon;   // when we need to hide ped weapon, we save it temporary here
    eWeaponType         m_nDelayedWeapon; // 'delayed' weapon is like an additional weapon, f.e., simple cop has a nitestick as current and pistol as delayed weapons
    uint32_t            m_nDelayedWeaponAmmo;
    uint8_t             m_nActiveWeaponSlot;
    uint8_t             m_nWeaponShootingRate;
    uint8_t             m_nWeaponAccuracy;
    CEntity*            m_pTargetedObject; // lock-on target
    int32_t             field_720;
    int32_t             field_724;
    int32_t             field_728;
    eWeaponSkill        m_nWeaponSkill;
    eFightingStyle      m_nFightingStyle;
    char                m_nAllowedAttackMoves;
    uint8_t             field_72F; // taskId related? 0x4B5C47
    CFire*              m_pFire;
    float               m_fireDmgMult;
    CEntity*            m_pLookTarget;
    float               m_fLookDirection; // In RAD
    int32_t             m_nWeaponModelId;
    uint32_t            m_nUnconsciousTimer;
    uint32_t            m_nLookTime;
    uint32_t            m_nAttackTimer;
    int32_t             m_nDeathTimeMS; //< Death time in MS (CTimer::GetTimeMS())
    char                m_nBodypartToRemove;
    char                field_755;
    int16_t             m_nMoneyCount; // Used for money pickup when ped is killed
    float               m_Wobble;
    float               m_WobbleSpeed;
    char                m_nLastWeaponDamage; // See eWeaponType
    CEntity*            m_pLastEntityDamage;
    int32_t             field_768;

    // TODO: Not turret, but rather attached entity, see `AttachPedToEntity` and `AttachPedToBike`
    CVector             m_vecTurretOffset;
    uint16_t            m_fTurretAngleA;
    float               m_fTurretAngleB;
    float               m_nTurretPosnMode;
    int32_t             m_nTurretAmmo;
    // **

    CCoverPoint*        m_pCoverPoint;
    CEntryExit*         m_pEnex; // CEnEx *
    float               m_fRemovalDistMultiplier;
    int16_t             m_StreamedScriptBrainToLoad;
    int32_t             field_798;

public:
    void SetModelIndex(uint32_t modelIndex) override;
    void DeleteRwObject() override;
    void ProcessControl() override;
    void Teleport(CVector destination, bool resetRotation) override;
    void SpecialEntityPreCollisionStuff(CPhysical* colPhysical, bool bIgnoreStuckCheck, bool& bCollisionDisabled, bool& bCollidedEntityCollisionIgnored, bool& bCollidedEntityUnableToMove, bool& bThisOrCollidedEntityStuck) override;
    uint8_t SpecialEntityCalcCollisionSteps(bool& bProcessCollisionBeforeSettingTimeStep, bool& unk2) override;
    void PreRender() override;
    void Render() override;
    bool SetupLighting() override;
    void RemoveLighting(bool bRemove) override;
    void FlagToDestroyWhenNextProcessed() override;
    int32_t ProcessEntityCollision(CEntity* entity, CColPoint* colPoint) override;

    // Process applied anim 0x86C3B4
    virtual void SetMoveAnim();
    // always returns true 0x86C3B8
    virtual bool Save();
    // always returns true 0x86C3BC
    virtual bool Load();

    // class functions
    static void* operator new(std::size_t size);
    static void* operator new(std::size_t size, int32_t poolRef);
    static void operator delete(void* data);
    static void operator delete(void* data, int poolRef);

    CPed(ePedType pedType);
    ~CPed() override;

    bool PedIsInvolvedInConversation();
    bool PedIsReadyForConversation(bool arg0);
    static bool PedCanPickUpPickUp();
    void CreateDeadPedMoney();
    void CreateDeadPedPickupCoors(float& outPickupX, float& outPickupY, float& outPickupZ);
    void CreateDeadPedWeaponPickups();
    static void Initialise();
    void SetPedStats(ePedStats statsType);
    void Update();
    void SetMoveState(eMoveState moveState);
    void SetMoveAnimSpeed(CAnimBlendAssociation* association);
    void StopNonPartialAnims();
    void RestartNonPartialAnims();
    bool CanUseTorsoWhenLooking() const;
    void SetLookFlag(float lookHeading, bool likeUnused, bool arg2);
    void SetLookFlag(CEntity* lookingTo, bool likeUnused, bool arg2);
    void SetAimFlag(CEntity* aimingTo);
    void ClearAimFlag();
    int32_t GetLocalDirection(const CVector2D& point) const;
    bool IsPedShootable() const;
    bool UseGroundColModel() const;
    bool CanPedReturnToState() const;
    bool CanSetPedState() const;
    bool CanBeArrested() const;
    bool CanStrafeOrMouseControl() const;
    bool CanBeDeleted();
    bool CanBeDeletedEvenInVehicle() const;
    void RemoveGogglesModel();
    int32_t GetWeaponSlot(eWeaponType weaponType);
    void GrantAmmo(eWeaponType weaponType, uint32_t ammo);
    void SetAmmo(eWeaponType weaponType, uint32_t ammo);
    bool DoWeHaveWeaponAvailable(eWeaponType weaponType);
    bool DoGunFlash(int32_t lifetime, bool bRightHand);
    void SetGunFlashAlpha(bool rightHand);
    void ResetGunFlashAlpha();
    float GetBikeRidingSkill() const;
    static void ShoulderBoneRotation(RpClump* clump);
    void SetLookTimer(uint32_t time);
    bool IsPlayer() const;
    void SetPedPositionInCar();
    void RestoreHeadingRate();
    static void RestoreHeadingRateCB(CAnimBlendAssociation* association, void* data);
    void SetRadioStation();
    void PositionAttachedPed();
    void Undress(char* modelName);
    void Dress();
    bool IsAlive() const;
    void UpdateStatEnteringVehicle();
    void UpdateStatLeavingVehicle();
    void GetTransformedBonePosition(RwV3d& inOutPos, eBoneTagU32 boneId, bool updateSkinBones = false);
    void ReleaseCoverPoint();
    CTaskSimpleHoldEntity* GetHoldingTask();
    CEntity* GetEntityThatThisPedIsHolding();
    void DropEntityThatThisPedIsHolding(bool bDeleteHeldEntity);
    bool CanThrowEntityThatThisPedIsHolding();
    bool IsPlayingHandSignal();
    void StopPlayingHandSignal();
    float GetWalkAnimSpeed();
    void SetPedDefaultDecisionMaker();
    bool CanSeeEntity(CEntity* entity, float limitAngle);
    bool PositionPedOutOfCollision(int32_t exitDoor, CVehicle* vehicle, bool findClosestNode);
    bool PositionAnyPedOutOfCollision();
    bool OurPedCanSeeThisEntity(CEntity* entity, bool isSpotted);
    void SortPeds(CPed** pedList, int32_t arg1, int32_t arg2);
    void ClearLookFlag();
    float WorkOutHeadingForMovingFirstPerson(float heading);
    void UpdatePosition();
    void ProcessBuoyancy();
    bool IsPedInControl() const;
    void RemoveWeaponModel(int32_t modelIndex = MODEL_INVALID);
    void AddGogglesModel(int32_t modelIndex, bool& inOutGogglesState);
    void PutOnGoggles();
    eWeaponSkill GetWeaponSkill(eWeaponType weaponType);
    void SetWeaponSkill(eWeaponType weaponType, eWeaponSkill skill);
    void ClearLook();
    bool TurnBody();
    bool IsPointerValid();
    CVector GetBonePosition(eBoneTag boneId, bool updateSkinBones = false);
    void GetBonePosition(CVector* outVec, eBoneTag bone, bool updateSkinBones);
    void GiveObjectToPedToHold(int32_t modelIndex, uint8_t replace);
    void SetPedState(ePedState pedState);
    ePedState GetPedState() { return m_nPedState; }
    //1 = default, 2 = scm/mission script
    void SetCharCreatedBy(ePedCreatedBy createdBy);
    void CalculateNewVelocity();
    void CalculateNewOrientation();
    void ClearAll();
    void DoFootLanded(bool leftFoot, uint8_t arg1);
    void PlayFootSteps();
    void AddWeaponModel(int32_t modelIndex);
    void TakeOffGoggles();
    eWeaponSlot GiveWeapon(eWeaponType weaponType, uint32_t ammo, bool likeUnused);
    void GiveWeaponSet1();
    void GiveWeaponSet2();
    void GiveWeaponSet3();
    void GiveWeaponSet4();
    void SetCurrentWeapon(int32_t slot);
    void SetCurrentWeapon(eWeaponType weaponType);
    void ClearWeapon(eWeaponType weaponType);
    void ClearWeapons();
    void RemoveWeaponWhenEnteringVehicle(int32_t arg0);
    void ReplaceWeaponWhenExitingVehicle();
    void ReplaceWeaponForScriptedCutscene();
    void RemoveWeaponForScriptedCutscene();
    eWeaponSkill GetWeaponSkill();
    void PreRenderAfterTest();
    void SetIdle();
    void SetLook(float heading);
    void SetLook(CEntity* entity);
    void Look();
    CEntity* AttachPedToEntity(CEntity* entity, CVector offset, uint16_t arg2, float arg3, eWeaponType weaponType);
    void AttachPedToBike(CEntity* entity, CVector offset, uint16_t arg2, float arg3, float arg4, eWeaponType weaponType);
    void DettachPedFromEntity();
    void SetAimFlag(float heading);
    bool CanWeRunAndFireWithWeapon();
    void RequestDelayedWeapon();
    void GiveDelayedWeapon(eWeaponType weaponType, uint32_t ammo);
    void GiveWeaponAtStartOfFight();
    void GiveWeaponWhenJoiningGang();
    bool GetPedTalking();
    void DisablePedSpeech(bool stopCurrentSpeech);
    void EnablePedSpeech();
    void DisablePedSpeechForScriptSpeech(bool stopCurrentSpeech);
    void EnablePedSpeechForScriptSpeech();
    bool CanPedHoldConversation() const;
    void SayScript(eAudioEvents scriptID, bool overrideSilence, bool isForceAudible, bool isFrontEnd);
    int16_t Say(eGlobalSpeechContext gCtx, uint32_t startTimeDelay = 0, float probability = 1.f, bool overrideSilence = false, bool isForceAudible = false, bool isFrontEnd = false);
    void RemoveBodyPart(ePedNode pedNode, char localDir);
    void SpawnFlyingComponent(int32_t arg0, char arg1);
    uint8_t DoesLOSBulletHitPed(CColPoint& colPoint);
    void RemoveWeaponAnims(int32_t likeUnused, float blendDelta);
    bool IsPedHeadAbovePos(float zPos);
    void KillPedWithCar(CVehicle* car, float fDamageIntensity, bool bPlayDeadAnimation);
    template<typename PtrListType>
    void MakeTyresMuddySectorList(PtrListType& ptrList);
    void DeadPedMakesTyresBloody();
    bool IsInVehicleThatHasADriver();
    void SetStayInSamePlace(bool enable) { bStayInSamePlace = enable; }
    bool IsWearingGoggles() const { return !!m_pGogglesObject; }

    // inlined
    CPlayerPedData* GetPlayerData() const { return m_pPlayerData; }

    // Demoted to declaration-only: needs real CPlayerPedData::m_pWanted
    CWanted* GetPlayerWanted() const;

    // NOTSA helpers
    void SetArmour(float v) { m_fArmour = v; }
    void SetWeaponShootingRange(uint8_t r) { m_nWeaponShootingRate = r; }
    void SetWeaponAccuracy(uint8_t acc) { m_nWeaponAccuracy = acc; }

    CAcquaintance& GetAcquaintance() { return m_acquaintance; }
    CVehicle* GetVehicleIfInOne() const { return bInVehicle ? m_pVehicle : nullptr; }

    uint8_t GetCreatedBy() const { return m_nCreatedBy; }
    void SetCreatedBy(ePedCreatedBy v) { m_nCreatedBy = v; }
    bool IsCreatedBy(ePedCreatedBy v) const noexcept { return v == m_nCreatedBy; }
    bool IsCreatedByMission() const noexcept { return IsCreatedBy(ePedCreatedBy::PED_MISSION); }

    CPedGroup* GetGroup() const;
    int32_t GetGroupId();

    // Demoted to declaration-only: needs real CPlayerPedData::m_pPedClothesDesc
    CPedClothesDesc* GetClothesDesc();

    CPedIntelligence* GetIntelligence() const { return m_pIntelligence; }
    CTaskManager& GetTaskManager() { return GetIntelligence()->m_TaskMgr; }
    CTaskManager& GetTaskManager() const { return GetIntelligence()->m_TaskMgr; }
    CEventGroup& GetEventGroup() { return GetIntelligence()->m_eventGroup; }
    CEventHandler& GetEventHandler() { return GetIntelligence()->m_eventHandler; }
    // Demoted to declaration-only: needs real CEventHandler::GetHistory()
    CEventHandlerHistory& GetEventHandlerHistory();
    CPedStuckChecker& GetStuckChecker() { return GetIntelligence()->m_pedStuckChecker; }

    CWeapon& GetWeaponInSlot(size_t slot) noexcept { return m_aWeapons[slot]; }
    CWeapon& GetWeaponInSlot(eWeaponSlot slot) noexcept { return m_aWeapons[(size_t)slot]; }
    CWeapon& GetActiveWeapon() noexcept { return GetWeaponInSlot(m_nActiveWeaponSlot); }
    CWeapon& GetWeapon(eWeaponType wt) noexcept { return GetWeaponInSlot(GetWeaponSlot(wt)); }

    eWeaponType GetSavedWeapon() const { return m_nSavedWeapon; }
    void SetSavedWeapon(eWeaponType weapon) { m_nSavedWeapon = weapon; }
    bool IsStateDriving() const noexcept { return m_nPedState == PEDSTATE_DRIVING; }
    bool IsStateDead() const noexcept { return m_nPedState == PEDSTATE_DEAD; }
    bool IsStateDying() const noexcept { return m_nPedState == PEDSTATE_DEAD || m_nPedState == PEDSTATE_DIE; }
    bool IsStateDeadForScript()  const noexcept { return m_nPedState == PEDSTATE_DEAD || m_nPedState == PEDSTATE_DIE || m_nPedState == PEDSTATE_DIE_BY_STEALTH; }
    bool IsInVehicleAsPassenger() const noexcept;

    bool IsCop()      const noexcept { return m_nPedType == PED_TYPE_COP; }
    bool IsGangster() const noexcept { return IsPedTypeGang(m_nPedType); }
    bool IsCivilian() const noexcept { return m_nPedType == PED_TYPE_CIVMALE || m_nPedType == PED_TYPE_CIVFEMALE; }

    // Demoted to declaration-only: reinterpret_cast needs the complete derived types
    // (CCopPed/CCivilianPed/CEmergencyPed/CPlayerPed land in later batches)
    CCopPed*       AsCop();
    CCivilianPed*  AsCivilian();
    CEmergencyPed* AsEmergency();
    CPlayerPed*    AsPlayer();

    bool IsFollowerOfGroup(const CPedGroup& group) const;
    RpHAnimHierarchy& GetAnimHierarchy() const;
    CAnimBlendClumpData& GetAnimBlendData() const;
    bool IsInVehicle() const { return bInVehicle && m_pVehicle; }
    bool IsInVehicle(const CVehicle* veh) const { return bInVehicle && m_pVehicle == veh; }
    int32_t GetPadNumber() const;
    bool IsCurrentlyUnarmed() { return GetActiveWeapon().m_Type == WEAPON_UNARMED; }

    CAEPedAudioEntity& GetAE() { return m_pedAudio; }
    const CAEPedAudioEntity& GetAE() const { return m_pedAudio; }
    CAEPedSpeechAudioEntity& GetSpeechAE() { return m_pedSpeech; }
    const CAEPedSpeechAudioEntity& GetSpeechAE() const { return m_pedSpeech; }
    CAEPedWeaponAudioEntity& GetWeaponAE() { return m_weaponAudio; }
    const CAEPedWeaponAudioEntity& GetWeaponAE() const { return m_weaponAudio; }

    CVector GetSeatPositionInVehicle() const;

    /*!
     * @notsa
     * @brief Is the ped jogging, running or sprinting
     */
    bool IsJoggingOrFaster() const;

    /*!
     * @notsa
     * @brief Is the ped running or sprinting
     */
    bool IsRunningOrSprinting() const;

    /*!
     * @notsa
     * @brief Is the ped standing in place (might still be moving, but in place)
     */
    bool IsPedStandingInPlace() const;

    /*!
     * @notsa
     * @brief Is the ped's right arm blocked right now
     */
    bool IsRightArmBlockedNow() const;

    /*!
     * @notsa
     * @brief Give weapon according to given CWeapon struct.
     */
    eWeaponSlot GiveWeapon(const CWeapon& weapon, bool likeUnused) {
        return GiveWeapon(weapon.m_Type, weapon.m_TotalAmmo, likeUnused);
    }

    CPedModelInfo* GetPedModelInfo() const { return reinterpret_cast<CPedModelInfo*>(GetModelInfo()); }

    /*!
     * @notsa
     * @brief Returns vehicle's position if ped is in one, ped's otherwise.
     * Demoted to declaration-only: needs complete CVehicle (m_pVehicle->GetPosition()).
     */
    CVector GetRealPosition() const;

    /*!
    * @notsa
    * Can this ped be ever considered as a criminal
    */
    bool CanBeCriminal() const;

private:
    void RenderThinBody() const;
    void RenderBigHead() const;
};

// TODO(2026-10-09): layout mismatch - sizeof(CPed) != 0x79C. Needs verification against decomp.
//#if INTPTR_MAX == INT32_MAX
//static_assert(sizeof(CPed) == 0x79C, "CPed size mismatch");
//#endif

RwObject* SetPedAtomicVisibilityCB(RwObject* rwObject, void* data);
bool IsPedPointerValid(CPed* ped);
bool IsPedPointerValid_NotInWorld(CPed* ped);
bool SayJacked(CPed* jacked, CVehicle* vehicle, uint32_t offset = 0);
bool SayJacking(CPed* jacker, CPed* jacked, CVehicle* vehicle, uint32_t offset = 0);
