// CVisibilityPlugins - minimal clean-room header for clean-room C++ build
// Adapted from gta-reversed/source/game_sa/VisibilityPlugins.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
//
// MINIMAL PORT: only the clump-alpha API used by CPed::ProcessControl.
// TODO: port the full plugin surface (RenderEntity, etc.) when the
// RenderWare visibility batch lands.

#pragma once

#include <cstdint>

struct RpClump; // RenderWare layer pending
class CPed;

class CVisibilityPlugins {
public:
    static void SetClumpAlpha(RpClump* clump, int32_t alpha);

    // gta-reversed VisibilityPlugins.h: static CClumpModelInfo* GetClumpModelInfo(RpClump* clump);
    // TODO(port): stub; inline so CPed links (added 2026-10-09 for CPed).
    static class CClumpModelInfo* GetClumpModelInfo(RpClump* clump) { (void)clump; return nullptr; }
    static int32_t GetClumpAlpha(RpClump* clump);

    // gta-reversed VisibilityPlugins.h: static void SetClumpForAllAtomicsFlag(RpClump* clump, int32 id);
    // TODO(port): real implementation.
    // Added 2026-10-09 for CAutomobile.
    // Declared here so CAutomobile links; defined in the RenderWare batch.
    static void SetClumpForAllAtomicsFlag(RpClump* clump, int32_t id);

    // gta-reversed VisibilityPlugins.h: static void AddWeaponPedForPC(CPed* ped);
    // Added 2026-10-09 for CPed. TODO(port): real implementation.
    // Declared here so CPed links; defined in the RenderWare batch.
    static void AddWeaponPedForPC(CPed* ped);

    // gta-reversed VisibilityPlugins.h: static void SetAtomicRenderCallback(RpAtomic* atomic, RpAtomicCallBackRender renderCB);
    // Added 2026-10-09 for CAutomobile. TODO(port): real implementation.
    // NOTE: RpAtomicCallBackRender not yet in RenderWare.h; using void* for the callback.
    static void SetAtomicRenderCallback(RpAtomic* atomic, void* renderCB) { (void)atomic; (void)renderCB; }
};
