// CWeapon - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Weapon.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), friend InjectHooksMain,
//   VALIDATE_SIZE (now a guarded static_assert), StaticRef<> (now plain static
//   members defined in CWeapon.cpp; original game addresses kept in comments).
//   Integer types converted to <cstdint> (uint32 -> uint32_t, etc.).
//   notsa Constructor() helper rewritten with placement new (the original
//   `this->CWeapon::CWeapon(...)` explicit-ctor call is MSVC-only).
// Replaced includes:
//   "Vector2D.h"     -> CVector2D forward declaration (CVector.h already provides it)
//   "eWeaponType.h"  -> "eWeaponType.h" (now include/eWeaponType.h)
//   "eWeaponSkill.h" -> "eWeaponSkill.h"
// Forward declarations (owning subsystems not yet converted):
//   FxSystem_c, CColPoint, CMatrix, CEntity, CPed, CVehicle, CColModel, CWeaponInfo

#pragma once

#include "CVector.h"
#include "eWeaponType.h"
#include "eWeaponSkill.h"
#include "ePedPieceTypes.h"

#include <cstdint>
#include <new> // placement new in Constructor()

class FxSystem_c;
class CColPoint;
class CMatrix;
class CEntity;
class CPed;
class CVehicle;
class CColModel;
class CWeaponInfo;

enum eWeaponState : uint32_t {
    WEAPONSTATE_READY = 0,
    WEAPONSTATE_FIRING,
    WEAPONSTATE_RELOADING,
    WEAPONSTATE_OUT_OF_AMMO,
    WEAPONSTATE_MELEE_MADECONTACT
};

#include "eWeaponSlot.h" // eWeaponSlot, NUM_WEAPON_SLOTS (moved here 2026-10-09)

class CWeapon {
public:
    // (were StaticRef<> globals at the noted game addresses)
    static float     ms_fExtinguisherAimAngle; // default -0.34907 rad. (-pi/8) // 0x8D610C
    static bool      bPhotographHasBeenTaken;  // 0xC8A7C0
    static bool      ms_bTakePhoto;            // 0xC8A7C1
    static CColModel ms_PelletTestCol;         // 0xC8A7DC

    static inline struct DebugSettings {
        bool NoShotDelay;
    } s_DebugSettings;

public:
    CWeapon() = default;
    CWeapon(eWeaponType weaponType, uint32_t ammo);
    CWeapon(const CWeapon&) = delete;

    void Initialise(eWeaponType weaponType, int32_t ammo, CPed* owner);
    static void InitialiseWeapons();
    void Shutdown();
    static void ShutdownWeapons();

    void AddGunshell(CEntity* creator, CVector& position, const CVector2D& direction, float size);
    bool LaserScopeDot(CVector* outCoord, float* outSize);
    bool FireSniper(CPed* shooter, CEntity* victim, CVector* target);
    void Reload(CPed* owner = nullptr);

    bool IsTypeMelee();
    bool IsType2Handed();
    bool IsTypeProjectile();

    bool HasWeaponAmmoToBeUsed();
    void StopWeaponEffect();
    void DoBulletImpact(CEntity* owner, CEntity* victim, const CVector& startPoint, const CVector& endPoint, const CColPoint& colPoint, int32_t arg5);
    /*!
    * @addr 0x73C1F0
    * @brief Marks all peds and objects that are in range (125 units) and in frame (on the screen - 0.1 relative border) as photographed.
    *
    * @param owner Camera owner - unused.
    * @param point Pos of the camflash effect
    */
    bool TakePhotograph(CEntity* owner, const CVector* point);
    void SetUpPelletCol(int32_t numPellets, CEntity* owner, CEntity* victim, CVector& point, CColPoint& colPoint, CMatrix& outMatrix);
    bool CanBeUsedFor2Player();

    // outX and outY will be placed in [-1;1] ranges
    void DoWeaponEffect(CVector origin, CVector target);

    bool FireAreaEffect(CEntity* firingEntity, const CVector& origin, CEntity* targetEntity, CVector* target);
    bool FireInstantHitFromCar(CVehicle* vehicle, bool leftSide, bool rightSide);
    bool FireFromCar(CVehicle* vehicle, bool leftSide, bool rightSide);
    void FireInstantHitFromCar2(CVector startPoint, CVector endPoint, CVehicle* vehicle, CEntity* owner);
    bool FireInstantHit(CEntity* firingEntity, CVector* origin, CVector* muzzlePosn, CEntity* targetEntity = nullptr, CVector* target = nullptr, CVector* originForDriveBy = nullptr, bool arg6 = false, bool muzzle = false);
    bool FireProjectile(CEntity* firingEntity, const CVector& origin, CEntity* targetEntity = nullptr, const CVector* targetPos = nullptr, float force = 0.f);
    bool FireM16_1stPerson(CPed* owner);
    bool Fire(CEntity* firedBy, CVector* startPosn, CVector* barrelPosn, CEntity* targetEnt, CVector* targetPosn, CVector* altPosn);

    void Update(CPed* owner);
    static void UpdateWeapons();

    static bool GenerateDamageEvent(CPed* victim, CEntity* creator, eWeaponType weaponType, int32_t damageFactor, ePedPieceTypes pedPiece, uint8_t direction);
    static bool CanBeUsedFor2Player(eWeaponType weaponType);
    static float TargetWeaponRangeMultiplier(CEntity* target, CEntity* weaponOwner);

    /*!
    * @addr 0x73CDC0
    * @brief Find closest entity in range that is visible to `owner` (Eg.: Is in [-PI/8, PI/8] angle) and modify `end->z` to be pointing at it. idk..
    *
    * @param end out Z axis is modified
    */
    static void DoDoomAiming(CEntity* owner, CVector* start, CVector* end);
    static void DoTankDoomAiming(CEntity* vehicle, CEntity* owner, CVector* startPoint, CVector* endPoint);
    static void DoDriveByAutoAiming(CEntity* owner, CVehicle* vehicle, CVector* startPoint, CVector* endPoint, bool canAimVehicles);
    static CEntity* FindNearestTargetEntityWithScreenCoors(float screenX, float screenY, float range, CVector point, float* outX = nullptr, float* outY = nullptr);
    static float EvaluateTargetForHeatSeekingMissile(CEntity* potentialTarget, const CVector& origin, const CVector& aimingDir, float tolerance, bool arePlanesPriority, CEntity* preferredExistingTarget);
    static bool CheckForShootingVehicleOccupant(CEntity** pCarEntity, CColPoint* colPoint, eWeaponType weaponType, const CVector& origin, const CVector& target);
    static CEntity* PickTargetForHeatSeekingMissile(CVector origin, CVector direction, float distanceMultiplier, CEntity* ignoreEntity, bool arePlanesPriority, CEntity* preferredExistingTarget);
    static bool ProcessLineOfSight(const CVector& startPoint, const CVector& endPoint, CColPoint& outColPoint, CEntity*& outEntity, eWeaponType weaponType, CEntity* arg5, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool arg11, bool doIgnoreCameraCheck);

    CWeaponInfo& GetWeaponInfo(CPed* owner = nullptr) const;
    CWeaponInfo& GetWeaponInfo(eWeaponSkill skill) const;

    auto GetType()          const noexcept { return m_Type; }
    auto GetState()         const noexcept { return m_State; }
    auto GetAmmoInClip()    const noexcept { return m_AmmoInClip; }
    auto GetTotalAmmo()     const noexcept { return m_TotalAmmo; }

    //! @notsa
    float GetWeaponRange(CPed* owner, CEntity* target = nullptr) const noexcept;

private:
    //! @notsa
    //! @brief Get the projectile type of this weapon - Only valid for weapons that fire a projectile [like rlaunchers, etc], or are itself a projectile [ex.: grenades]
    auto GetProjectileType();

    //! @notsa
    CWeapon* Constructor(eWeaponType weaponType, uint32_t ammo) {
        return new (this) CWeapon(weaponType, ammo);
    }

public: // TODO: Eventually make this private
    eWeaponType  m_Type{};                            //< Weapon's type
    eWeaponState m_State{};                           //< Current weapon state
    uint32_t     m_AmmoInClip{};                      //< Count of ammo in the clip currently
    uint32_t     m_TotalAmmo{};                       //< The total amount of ammo (Anything above 25k is considered infinite in case of player peds)
    uint32_t     m_TimeForNextShotMs{};               //< When the next shot is allowed to be fired
    bool         m_IsFirstPersonWeaponModeSelected{}; //< Fuck knows, unused
    bool         m_DontPlaceInHand{};                 //< Used in case of goggles (infrared/nightvision) : When they're put on the weapon model isn't [and shouldn't be] loaded.
    FxSystem_c*  m_FxSystem{};                        //< Fx system [flamethrower, spraycan, extinguisher]
};

// Layout check: gta-reversed VALIDATE_SIZE(CWeapon, 0x1C), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CWeapon) == 0x1C, "CWeapon layout drift");
#endif

void FireOneInstantHitRound(const CVector& startPoint, const CVector& endPoint, int32_t intensity);
