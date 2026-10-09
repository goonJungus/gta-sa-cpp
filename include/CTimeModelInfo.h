// CTimeModelInfo - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Models/TimeModelInfo.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), NOTSA_EXPORT_VTABLE,
//   VALIDATE_SIZE (now a guarded static_assert).
// Minimal port (created 2026-10-09): CModelInfo.h includes this header and
// instantiates CStore<CTimeModelInfo, 169> by value, so the class must be
// complete here. Time on/off + related-model fields are the decomp layout;
// the vtable overrides are stubs until the models subsystem ports the
// time-model logic - see BUILD_NOTES.md.

#pragma once

#include "CBaseModelInfo.h"

#include <cstdint>

class CTimeModelInfo : public CBaseModelInfo {
public:
    int32_t m_nTimeOn{};         // 0x20: time the model becomes visible
    int32_t m_nTimeOff{};        // 0x24: time the model becomes invisible
    int16_t m_nOtherTimeModel{}; // 0x28: model index of the day/night counterpart

public:
    // vtable overrides (stubs until the models subsystem ports them)
    ModelInfoType GetModelType() override { return MODEL_INFO_TIME; }
    void DeleteRwObject() override { /* TODO: port from decomp */ }
    uint32_t GetRwModelType() const override { return 0; } // TODO: verify against decomp
    RwObject* CreateInstance() override { return nullptr; } // TODO: port from decomp
    RwObject* CreateInstance(RwMatrix* matrix) override { (void)matrix; return nullptr; } // TODO: port from decomp
};
