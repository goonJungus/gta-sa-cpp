// eWeaponSlot - adapted from gta-reversed for clean-room C++ build
// Moved here from CWeapon.h (2026-10-09): CWeaponInfo also needs the slot
// enum, and CWeapon.h including it keeps the layering clean.

#pragma once

#include <cstddef> // size_t
#include <cstdint>

/* Source: https://wiki.multitheftauto.com/wiki/GetPedWeaponSlot */
enum class eWeaponSlot : uint32_t {
    UNARMED,
    MELEE,
    HANDGUN,
    SHOTGUN,
    SMG,        // Used for drive-by's
    RIFLE,
    SNIPER,
    HEAVY,
    THROWN,
    SPECIAL,
    GIFT,
    PARACHUTE,
    DETONATOR,
};
constexpr auto NUM_WEAPON_SLOTS = static_cast<size_t>(eWeaponSlot::DETONATOR) + 1u;
