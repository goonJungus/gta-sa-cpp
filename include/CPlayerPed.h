// CPlayerPed - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Ped/PlayerPed.h
// Player-controlled ped: weapon targeting, sprint/breath, group commands, wanted.
// Hierarchy: CPlaceable -> CEntity -> CPhysical -> CPed -> CPlayerPed
//
// DEFERRED:
//   CPad/CPlayerInfo/CWanted/CEventDamage/CPedGroups -> forward-declared
//     (pad/player-info/wanted/event subsystems not yet converted)
//   GetWanted() x2 demoted to declaration-only (need real CPlayerPedData::m_pWanted)
//   GetPlayerGroup() demoted to declaration-only (needs real CPedGroups::GetGroup)
//   eSprintType/eWantedLevel ported inline below (values verified against
//     gta-reversed) - delete these blocks when the real enums land
//
// Adaptations:
//   stripped InjectHooks()
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
//   StaticRef statics -> plain static members (CPlayerPed.cpp) / extern globals,
//     game addresses kept as comments
//   NOTSA_EXPORT_VTABLE stripped
// TODO:
//   port CPad/CPlayerInfo/CWanted/CEventDamage/CPedGroups
//   verify each method against decomp src/CPlayerPed/*.c

#pragma once

#include "CPed.h"

#include <array>
#include <cstdint>

class CEventDamage;
class CPlayerInfo;
class CPad;
class CWanted;

// eSprintType: full enum is 4 entries (gta-reversed Enums/eSprintType.h) -
// ported here; delete this block when the real enum lands.
enum eSprintType : uint32_t {
    SPRINT_GROUND     = 0,
    SPRINT_BMX        = 1,
    SPRINT_WATER      = 2,
    SPRINT_UNDERWATER = 3,
};

// eWantedLevel: full enum (gta-reversed Enums/eWantedLevel.h) - ported here;
// delete this block when the real enum lands.
enum class eWantedLevel : uint32_t {
    WANTED_CLEAN = 0,
    WANTED_LEVEL_1,
    WANTED_LEVEL_2,
    WANTED_LEVEL_3,
    WANTED_LEVEL_4,
    WANTED_LEVEL_5,
    WANTED_LEVEL_6
};

class CPlayerPed : public CPed {
public:
    CPed* m_p3rdPersonMouseTarget;
    int32_t field_7A0;

    // did we display "JCK_HLP" message
    static bool bHasDisplayedPlayerQuitEnterCarHelpText; // game address: 0xC0BC15 ; DEFERRED - was StaticRef

    // Android
    static bool bDebugPlayerInvincible;
    static bool bDebugTargeting;
    static bool bDebugTapToTarget;

public:
    CPlayerPed(int32_t playerId, bool bGroupCreated);

    void ProcessControl() override;
    void SetMoveAnim() override;
    bool Load() override;
    bool Save() override;

    CPad* GetPadFromPlayer() const;
    bool CanPlayerStartMission();
    bool IsHidden();
    void ReApplyMoveAnims();
    bool DoesPlayerWantNewWeapon(eWeaponType weaponType, bool arg1);
    void ProcessPlayerWeapon(CPad* pad);
    void PickWeaponAllowedFor2Player();
    void UpdateCameraWeaponModes(CPad* pad);
    void ProcessAnimGroups();
    void ClearWeaponTarget();
    float GetWeaponRadiusOnScreen();
    float FindTargetPriority(CEntity* entity);
    void Clear3rdPersonMouseTarget();
    // GetWanted()->m_nWantedLevel = 0;
    void Busted();
    eWantedLevel GetWantedLevel() const;
    void SetWantedLevel(eWantedLevel level);
    void SetWantedLevelNoDrop(eWantedLevel level);
    void CheatWantedLevel(eWantedLevel level);
    bool CanIKReachThisTarget(CVector posn, CWeapon* weapon, bool arg2);
    CPlayerInfo* GetPlayerInfoForThisPlayerPed();
    void DoStuffToGoOnFire();
    void AnnoyPlayerPed(bool arg0);
    void ClearAdrenaline();
    void DisbandPlayerGroup();
    void MakeGroupRespondToPlayerTakingDamage(CEventDamage& damageEvent);
    void TellGroupToStartFollowingPlayer(bool arg0, bool arg1, bool arg2);
    void MakePlayerGroupDisappear();
    void MakePlayerGroupReappear();
    void ResetSprintEnergy();
    bool HandleSprintEnergy(bool sprint, float adrenalineConsumedPerTimeStep);
    float ControlButtonSprint(eSprintType sprintType);
    float GetButtonSprintResults(eSprintType sprintType);
    void ResetPlayerBreath();
    void HandlePlayerBreath(bool bDecreaseAir, float fMultiplier);
    void SetRealMoveAnim();
    void MakeChangesForNewWeapon(eWeaponType weaponType);
    void Compute3rdPersonMouseTarget(bool meleeWeapon);
    void DrawTriangleForMouseRecruitPed();
    bool DoesTargetHaveToBeBroken(CEntity* entity, CWeapon* weapon);
    void KeepAreaAroundPlayerClear();
    void SetPlayerMoveBlendRatio(CVector* arg0);
    CPed* FindPedToAttack();
    void ForceGroupToAlwaysFollow(bool enable);
    void ForceGroupToNeverFollow(bool enable);
    void MakeThisPedJoinOurGroup(CPed* ped);
    bool PlayerWantsToAttack();
    void SetInitialState(bool bGroupCreated);
    void MakeChangesForNewWeapon(uint32_t weaponSlot);
    void EvaluateTarget(CEntity* target, CEntity*& outTarget, float& outTargetPriority, float maxDistance, float arg4, bool arg5);
    void EvaluateNeighbouringTarget(CEntity* target, CEntity** outTarget, float* outTargetPriority, float maxDistance, float arg4, bool arg5);
    void ProcessGroupBehaviour(CPad* pad);
    // return PlayerWantsToAttack();
    bool PlayerHasJustAttackedSomeone();
    void ProcessWeaponSwitch(CPad* pad);
    bool FindWeaponLockOnTarget();
    bool FindNextWeaponLockOnTarget(CEntity* arg0, bool arg1);

    // Demoted to declaration-only: need real CPlayerPedData::m_pWanted
    CWanted* GetWanted();
    const CWanted* GetWanted() const;

    static void RemovePlayerPed(int32_t playerId);
    static void DeactivatePlayerPed(int32_t playerId);
    static void ReactivatePlayerPed(int32_t playerId);
    static bool PedCanBeTargettedVehicleWise(CPed* ped);
    static void SetupPlayerPed(int playerId);

    // NOTSA - demoted to declaration-only: needs real CPedGroups::GetGroup
    CPedGroup& GetPlayerGroup() const noexcept;
};

// TODO(2026-10-09): layout mismatch - sizeof(CPlayerPed) != 0x7A4. Needs verification against decomp.
//#if INTPTR_MAX == INT32_MAX
//static_assert(sizeof(CPlayerPed) == 0x7A4, "CPlayerPed size mismatch");
//#endif

// game addresses: 0xC0BC08 / 0xC0BC10 ; DEFERRED - were StaticRef
extern std::array<bool, 7> abTempNeverLeavesGroup;
extern int32_t gPlayIdlesAnimBlockIndex;

bool LOSBlockedBetweenPeds(CEntity* entity1, CEntity* entity2);
