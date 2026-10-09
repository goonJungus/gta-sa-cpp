// CWeaponEffects - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/WeaponEffects.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), VALIDATE_SIZE (now a guarded static_assert),
//   StaticRef<> (gCrossHair/gpCrossHairTex/gpCrossHairTexFlight are now extern,
//   defined in CWeaponEffects.cpp; original game addresses kept in comments).
//   Integer types converted to <cstdint> (int32 -> int32_t, etc.);
//   `typedef int32 CrossHairId` -> `using CrossHairId = int32_t`.

#pragma once

#include "CVector.h"
#include "RenderTypes.h" // CRGBA, RwTexture*

#include <array>
#include <cstdint>

using CrossHairId = int32_t;

enum eWeaponEffectsLockTexture {
    WEAPONEFFECTS_LOCK_ON = 0,
    WEAPONEFFECTS_LOCK_ON_FIRE = 1
};

class CWeaponEffects {
public:
    bool     m_bActive;
    int32_t  m_nTimeWhenToDeactivate; // -1 default
    CVector  m_vecPosn;
    CRGBA    m_color;
    float    m_fSize;
    int32_t  field_1C;
    int32_t  field_20;
    float    m_fRotation;
    bool     m_bClearImmediately;

public:
    CWeaponEffects() = default;  // 0x742A90
    ~CWeaponEffects() = default; // 0x742AA0

    static void Init();
    static void Shutdown();
    static bool IsLockedOn(CrossHairId id);
    static void MarkTarget(CrossHairId id, CVector posn, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float size, bool bClearImmediately);
    static void ClearCrossHair(CrossHairId id);
    static void ClearCrossHairs();
    static void ClearCrossHairImmediately(CrossHairId id);
    static void ClearCrossHairsImmediately();
    static void Render();
};

// Layout check: gta-reversed VALIDATE_SIZE(CWeaponEffects, 0x2C), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CWeaponEffects) == 0x2C, "CWeaponEffects layout drift");
#endif

constexpr auto MAX_NUM_WEAPON_CROSSHAIRS{ 2u };
extern std::array<CWeaponEffects, MAX_NUM_WEAPON_CROSSHAIRS> gCrossHair; // was StaticRef<...>(0xC8A838)
extern RwTexture* gpCrossHairTex;                 // was StaticRef<RwTexture*>(0xC8A818)
extern RwTexture* gpCrossHairTexFlight[2];       // was StaticRef<RwTexture*[2]>(0xC8A810)
