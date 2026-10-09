// CWeaponModelInfo - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Models/WeaponModelInfo.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), friend InjectHooksMain, NOTSA_EXPORT_VTABLE,
//   VALIDATE_SIZE (now a guarded static_assert).
// Replaced includes:
//   "ClumpModelInfo.h" -> "CClumpModelInfo.h"
//   "eWeaponType.h" -> include/eWeaponType.h (enum moved out 2026-10-08 with the
//       weapons_combat batch; values verified against gta-reversed
//       source/game_sa/Enums/eWeaponType.h).

#pragma once

#include "CClumpModelInfo.h"
#include "eWeaponType.h"

#include <cstddef>
#include <cstdint>

class CWeaponModelInfo : public CClumpModelInfo {
private:
    eWeaponType m_weaponInfo;

public:
    CWeaponModelInfo() : CClumpModelInfo() {}

    // VTable

    void Init() override;

    ModelInfoType GetModelType() override;
    void SetClump(RpClump* clump) override;

    // inlined
    void SetWeaponInfo(eWeaponType weapon) { m_weaponInfo = weapon; }
    eWeaponType GetWeaponInfo() { return m_weaponInfo; }
};

// Layout check: gta-reversed VALIDATE_SIZE(CWeaponModelInfo, 0x28), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CWeaponModelInfo) == 0x28, "CWeaponModelInfo layout drift");
#endif
