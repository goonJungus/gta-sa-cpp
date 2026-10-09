// CAtomicModelInfo - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Models/AtomicModelInfo.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), NOTSA_EXPORT_VTABLE,
//   VALIDATE_SIZE (now a guarded static_assert).

#pragma once

#include "CBaseModelInfo.h"

#include <cstdint>

struct tVehicleComponentFlag {
    const char* m_ucName;
    uint32_t    m_nFlag;
};

class CAtomicModelInfo : public CBaseModelInfo {
public:
    CAtomicModelInfo() : CBaseModelInfo() {}

public:
    // vtable overrides
    CAtomicModelInfo* AsAtomicModelInfoPtr() override;
    ModelInfoType GetModelType() override;
    void Init() override;
    void DeleteRwObject() override;
    uint32_t GetRwModelType() const override { return rpATOMIC; }
    RwObject* CreateInstance() override;
    RwObject* CreateInstance(RwMatrix* matrix) override;

    // vtable added methods
    virtual void SetAtomic(RpAtomic* atomic);

    // class methods
    struct RpAtomic* GetAtomicFromDistance(float distance);
    void SetupVehicleUpgradeFlags(const char* name);
};

void SetAtomicModelInfoFlags(CAtomicModelInfo* modelInfo, uint32_t dwFlags);

// Layout check: gta-reversed VALIDATE_SIZE(CAtomicModelInfo, 0x20), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAtomicModelInfo) == 0x20, "CAtomicModelInfo layout drift");
#endif
