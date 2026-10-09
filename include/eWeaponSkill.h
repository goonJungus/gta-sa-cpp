// eWeaponSkill - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Enums/eWeaponSkill.h
// (uint8 -> uint8_t; otherwise verbatim)

#pragma once

#include <cstdint>

enum class eWeaponSkill : uint8_t {
    POOR,
    STD,  // standard
    PRO,
    COP
};
constexpr auto NUM_WEAPON_SKILLS = 4;
