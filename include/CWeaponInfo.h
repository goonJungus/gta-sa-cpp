// CWeaponInfo - minimal port for the weapons_combat subsystem
// (clean-room C++ build).
// Source: gta-reversed/source/game_sa/WeaponInfo.h
// (https://github.com/gta-reversed/gta-reversed)
//
// This is a PARTIAL port: only what the converted weapons_combat TUs need
// today. The full port (weapon.dat loading, aWeaponInfos[NUM_WEAPON_INFOS],
// Initialise/Shutdown, GetWeaponInfoIndex, skill tables) belongs to the
// file_loading/weapons pass.
// m_nSlot/m_nModelId1 added 2026-10-09 (values still TBD from weapon.dat).
// Firing/range/accuracy fields added 2026-10-09 for CWeapon (Fire/Update/etc.).

#pragma once

#include "eWeaponType.h"
#include "eWeaponSlot.h"
#include "eWeaponSkill.h"
#include "CVector.h"
#include "eStats.h" // eStats (GetSkillStatIndex return type)

#include <cstdint>

enum class eWeaponFire : uint8_t {
    WEAPON_FIRE_MELEE,
    WEAPON_FIRE_INSTANT_HIT,
    WEAPON_FIRE_PROJECTILE,
    WEAPON_FIRE_AREA_EFFECT,
    WEAPON_FIRE_CAMERA,
    WEAPON_FIRE_USE
};

struct CWeaponAimingOffset {
    uint32_t RLoadA{};
    uint32_t RLoadB{};
    uint32_t CrouchRLoadA{};
    uint32_t CrouchRLoadB{};
};

class CWeaponInfo {
public:
    uint16_t m_nDamage{}; //!< damage inflicted per hit
    eWeaponSlot m_nSlot{};   //!< weapon slot (added 2026-10-09 for CPed::GiveWeapon etc.)
    int32_t m_nModelId1{};   //!< primary model id (added 2026-10-09 for CPed::GiveWeapon etc.)
    int32_t m_nModelId2{};   //!< secondary model id (added 2026-10-09 for CPed::RequestDelayedWeapon)

    // Firing/range/accuracy (added 2026-10-09 for CWeapon; values TBD from weapon.dat)
    eWeaponFire m_nWeaponFire{};
    uint32_t    m_nAmmoClip{};
    float       m_fWeaponRange{};
    float       m_fTargetRange{};
    float       m_fAccuracy{};
    CVector     m_vecFireOffset{};
    float       m_fAnimLoopStart{};
    float       m_fAnimLoopEnd{};
    struct {
        bool bReload : 1;
    } flags{};
    uint32_t m_nFlags{}; // decomp anon-struct compat (TODO: map to real flags)
    uint32_t m_nReqStatLevel{}; //!< stat level required for this skill level (gta-reversed). Added 2026-10-09 for CPed.

public:
    static CWeaponInfo* GetWeaponInfo(eWeaponType weaponType, eWeaponSkill skill = eWeaponSkill::STD);
    // gta-reversed WeaponInfo.h. TODO(port): real mapping. Added 2026-10-09 for CPed.
    static eStats GetSkillStatIndex(eWeaponType weaponType) { (void)weaponType; return (eStats)0; }

    // TODO(data): real implementations when weapon.dat is ported
    uint32_t GetWeaponReloadTime() const { return 1000; }
    const CWeaponAimingOffset& GetAimingOffset() const {
        static CWeaponAimingOffset ao{};
        return ao;
    }
    int32_t GetCrouchReloadAnimationID() const { return 0; }
    // TODO(data): original formula is (m_nSkillLevel+2)*m_fWeaponRange*0.04; m_nSkillLevel not ported yet
    float GetTargetHeadRange() const { return m_fTargetRange; }
    static bool TypeIsWeapon(eWeaponType weaponType) { return true; }
    static bool TypeHasSkillStats(eWeaponType weaponType) { return true; }
};
