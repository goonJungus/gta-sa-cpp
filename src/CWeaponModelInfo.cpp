// CWeaponModelInfo - adapted from gta-reversed for clean-room C++ build
// Method implementations. Decompiled reference: src/CWeaponModelInfo/*.c
// (GTA SA 1.0 @ addresses noted per method.)

#include "CWeaponModelInfo.h"
#include "RenderWare.h" // RW prototypes + game helpers (GetFirstObject, ...)
#include "CVehicleModelInfo.h" // RwSurfaceProperties definition (deduped 2026-10-09)

#include <cstdint>

// ---- PORT(renderer): visibility plugins not ported yet.
class CVisibilityPlugins {
public:
    static void* RenderWeaponCB;
};
void* CVisibilityPlugins::RenderWeaponCB = nullptr;

// ---- PORT(vehicle): CVehicle not ported yet (weapons_combat batch owns it).
class CVehicle {
public:
    static void SetComponentAtomicAlpha(RpAtomic* atomic, int32_t alpha);
};

// 0x4C98F0
void CWeaponModelInfo::Init() {
    CClumpModelInfo::Init();
    m_weaponInfo = WEAPON_UNARMED;
}

// 0x4C5780
ModelInfoType CWeaponModelInfo::GetModelType() {
    return MODEL_INFO_WEAPON;
}

// 0x4C9910
void CWeaponModelInfo::SetClump(RpClump* clump) {
    CClumpModelInfo::SetClump(clump);
    RpClumpForAllAtomics(clump, CClumpModelInfo::SetAtomicRendererCB,
                         CVisibilityPlugins::RenderWeaponCB);

    // The "gunflash" frame's first atomic is prepped for the muzzle-flash
    // render: fully transparent, flags cleared, first material ambient 16.0.
    RwFrame* flashFrame = CClumpModelInfo::GetFrameFromName(clump, "gunflash");
    if (!flashFrame)
        return;

    // GetFirstObject returns the frame's first child as RwObject*; the decomp
    // treats it as the atomic, hence reinterpret_cast (was static_cast,
    // which MSVC rejects for unrelated types).
    RpAtomic* firstAtomic = reinterpret_cast<RpAtomic*>(GetFirstObject(flashFrame));
    if (!firstAtomic)
        return;

    CVehicle::SetComponentAtomicAlpha(firstAtomic, 0);
    RpAtomicSetFlags(firstAtomic, 0);
    RpMaterialGetSurfaceProperties(RpGeometryGetMaterial(RpAtomicGetGeometry(firstAtomic), 0))->ambient = 16.0F;
}
