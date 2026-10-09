// CShotInfo - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/ShotInfo.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), friend InjectHooksMain,
//   VALIDATE_SIZE (now a guarded static_assert).
//   Integer types converted to <cstdint> (uint8 -> uint8_t).
// Forward declarations (owning subsystems not yet converted): CEntity

#pragma once

#include "CVector.h"
#include "eWeaponType.h"

#include <cstdint>

class CEntity;

class CShotInfo {
public:
    static void Initialise();
    static void Shutdown();
    static bool AddShot(CEntity* creator, eWeaponType weaponType, CVector origin, CVector target);
    static bool GetFlameThrowerShotPosn(uint8_t shotId, CVector& outPos);
    static void Update();

    CShotInfo() = default; // NOTSA

public:
    eWeaponType m_nWeaponType{WEAPON_PISTOL};
    CVector     m_vecOrigin{};
    CVector     m_vecTargetOffset{};
    float       m_fRange{ 1.0f };
    CEntity*    m_pCreator{};
    float       m_DestroyTime{};
    bool        m_bExist{};
    bool        m_bExecuted{};
};

// Layout check: gta-reversed VALIDATE_SIZE(CShotInfo, 0x2C), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CShotInfo) == 0x2C, "CShotInfo layout drift");
#endif
