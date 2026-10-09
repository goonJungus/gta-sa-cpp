// CBulletInfo - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/BulletInfo.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), VALIDATE_SIZE (now a guarded static_assert),
//   StaticRef<> (aBulletInfos/PlayerSniperBulletStart/PlayerSniperBulletEnd are now
//   static members defined in CBulletInfo.cpp; original game addresses kept in comments).
//   Integer types converted to <cstdint> (int16 -> int16_t).
//   "Base.h" include dropped (it only provided the integer aliases here).
// Forward declarations (owning subsystems not yet converted): CEntity

#pragma once

#include "CVector.h"
#include "eWeaponType.h"

#include <array>
#include <cstdint>

class CEntity;

class CBulletInfo {
public:
    eWeaponType m_nWeaponType;
    CEntity*    m_pCreator;
    float       m_nDestroyTime;
    bool        m_bExists;
    CVector     m_vecPosition;
    CVector     m_vecVelocity;
    int16_t     m_nDamage;

    static constexpr auto MAX_BULLET_INFOS{8u};

    // (were StaticRef<> globals at the noted game addresses)
    static std::array<CBulletInfo, MAX_BULLET_INFOS> aBulletInfos; // 0xC88740
    static CVector PlayerSniperBulletStart;                       // 0xC888A0
    static CVector PlayerSniperBulletEnd;                         // 0xC888AC

public:
    static void Initialise();
    static void Shutdown();
    static void AddBullet(CEntity* creator, eWeaponType weaponType, CVector position, CVector velocity);
    static void Update();

    // NOTSA funcs:
private:
    static CBulletInfo* GetFree();
    bool IsTimeToBeDestroyed() const noexcept;
};

// Layout check: gta-reversed VALIDATE_SIZE(CBulletInfo, 0x2C), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CBulletInfo) == 0x2C, "CBulletInfo layout drift");
#endif
