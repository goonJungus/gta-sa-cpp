// CAtomicModelInfo - adapted from gta-reversed for clean-room C++ build
// Method implementations. Decompiled reference: src/CAtomicModelInfo/*.c
// (GTA SA 1.0 @ addresses noted per method.)

#include "CAtomicModelInfo.h"
#include "RenderWare.h" // RW prototypes (RpAtomicClone, RwFrameCreate, ...)

#include <cstdint>
#include <cstring>

// ---- PORT(anim): CAnimManager not ported yet. Minimal interface used here;
//      replace with #include "CAnimManager.h" when the animation subsystem lands.
class CAnimManager {
public:
    static void AddAnimBlockRef(int32_t index);
    static void RemoveAnimBlockRef(int32_t index);
};

// ---- PORT(renderer): custom pipeline renderers not ported yet.
class CCustomBuildingRenderer {
public:
    static bool IsCBPCPipelineAttached(RpAtomic* atomic);
    static void AtomicSetup(RpAtomic* atomic);
};
class CCarFXRenderer {
public:
    static bool IsCCPCPipelineAttached(RpAtomic* atomic);
    static void SetCustomFXAtomicRenderPipelinesVMICB(RpAtomic* atomic, void* data);
};
class CTagManager {
public:
    static void SetupAtomic(RpAtomic* atomic);
};

// ---- PORT(camera): CCamera not ported yet. TheCamera.m_fLODDistMultiplier
//      lives at 0xB6F118 in the original binary.
struct CCamera {
    float m_fLODDistMultiplier;
};
extern CCamera TheCamera;

// 0x4C5560
CAtomicModelInfo* CAtomicModelInfo::AsAtomicModelInfoPtr() {
    return this;
}

// 0x4C5570
ModelInfoType CAtomicModelInfo::GetModelType() {
    return MODEL_INFO_ATOMIC;
}

// No named .c; the binary's ctor/dtor thunk (004c5540/004c6210) just chains to
// CBaseModelInfo. Init() is the virtual initializer (vtable+0x18).
void CAtomicModelInfo::Init() {
    CBaseModelInfo::Init();
}

// 0x4C4440
void CAtomicModelInfo::DeleteRwObject() {
    if (!GetRpAtomic())
        return;

    // Decomp reads the 2dFX plugin count off the atomic's geometry and
    // subtracts it from m_n2dfxCount before destroying.
    m_n2dfxCount -= static_cast<uint8_t>(RpGeometryGet2dFxCount(RpAtomicGetGeometry(GetRpAtomic())));

    RwFrame* frame = RpAtomicGetFrame(GetRpAtomic());
    RpAtomicDestroy(GetRpAtomic());
    RwFrameDestroy(frame);
    m_pRwObject = nullptr;

    CBaseModelInfo::RemoveTexDictionaryRef();
    int32_t animIndex = GetAnimFileIndex();
    if (animIndex != -1)
        CAnimManager::RemoveAnimBlockRef(animIndex);
}

// 0x4C4530
RwObject* CAtomicModelInfo::CreateInstance() {
    if (!GetRpAtomic())
        return nullptr;

    CBaseModelInfo::AddRef();
    RpAtomic* clonedAtomic = RpAtomicClone(GetRpAtomic());
    RwFrame* frame = RwFrameCreate();
    RpAtomicSetFrame(clonedAtomic, frame);
    CBaseModelInfo::RemoveRef();

    return reinterpret_cast<RwObject*>(clonedAtomic);
}

// 0x4C44D0
RwObject* CAtomicModelInfo::CreateInstance(RwMatrix* matrix) {
    if (!GetRpAtomic())
        return nullptr;

    CBaseModelInfo::AddRef();
    RpAtomic* clonedAtomic = RpAtomicClone(GetRpAtomic());
    RwFrame* frame = RwFrameCreate();
    // Decomp copies the 16 floats of the matrix into the frame's matrix
    // (RwFrame+0x10); memcpy is the faithful equivalent.
    memcpy(RwFrameGetMatrix(frame), matrix, sizeof(RwMatrix));
    RpAtomicSetFrame(clonedAtomic, frame);
    CBaseModelInfo::RemoveRef();

    return reinterpret_cast<RwObject*>(clonedAtomic);
}

// 0x4C4360
void CAtomicModelInfo::SetAtomic(RpAtomic* atomic) {
    if (m_pRwObject) {
        m_n2dfxCount -= static_cast<uint8_t>(RpGeometryGet2dFxCount(RpAtomicGetGeometry(GetRpAtomic())));
    }

    m_pRwObject = reinterpret_cast<RwObject*>(atomic);
    m_n2dfxCount += static_cast<uint8_t>(RpGeometryGet2dFxCount(RpAtomicGetGeometry(atomic)));

    CBaseModelInfo::AddTexDictionaryRef();
    int32_t animIndex = GetAnimFileIndex();
    if (animIndex != -1)
        CAnimManager::AddAnimBlockRef(animIndex);

    if (CCustomBuildingRenderer::IsCBPCPipelineAttached(atomic))
        CCustomBuildingRenderer::AtomicSetup(atomic);
    else if (CCarFXRenderer::IsCCPCPipelineAttached(atomic))
        CCarFXRenderer::SetCustomFXAtomicRenderPipelinesVMICB(atomic, nullptr);

    // Decomp: (short)m_nFlags >= 0 (i.e. !bTagDisabled, bit 15 clear) &&
    //         (m_nFlags & 0x7800) == 0x3000 (nSpecialType == TAG).
    if (!bTagDisabled && IsTagModel())
        CTagManager::SetupAtomic(atomic);

    // Decomp sets bit 0 of the flags low byte: bHasBeenPreRendered.
    SetHasBeenPreRendered(true);
}

// 0x4C44B0
RpAtomic* CAtomicModelInfo::GetAtomicFromDistance(float distance) {
    // _DAT_00b6f118 is TheCamera.m_fLODDistMultiplier.
    if (TheCamera.m_fLODDistMultiplier * m_fDrawDistance <= distance)
        return nullptr;

    return GetRpAtomic();
}

// 0x4C4570
void CAtomicModelInfo::SetupVehicleUpgradeFlags(const char* name) {
    // Decomp guard: (short)m_nFlags >= 0, i.e. bit 15 clear, i.e.
    // !bUseCommonVehicleDictionary in the vehicle-flags view.
    if (bUseCommonVehicleDictionary)
        return;

    // Dummy-part prefixes (set bUsesVehDummy).
    static const tVehicleComponentFlag aDummyComps[] = {
        { "chss_",   0x01 },
        { "wheel_",  0x02 },
        { "exh_",    0x13 },
        { "fbmp_",   0x0C },
        { "rbmp_",   0x0D },
        { "misc_a_", 0x14 },
        { "misc_b_", 0x15 },
        { "misc_c_", 0x16 },
        { nullptr,   0x00 },
    };

    // Chassis-part prefixes. Order and flag values verified against the decomp
    // (stack-built table at 0x4C4570).
    static const tVehicleComponentFlag aChassisComps[] = {
        { "bnt_",      0x00 },
        { "bntl_",     0x01 },
        { "bntr_",     0x02 },
        { "spl_",      0x06 },
        { "wg_l_",     0x08 },
        { "wg_r",      0x09 },
        { "fbb_",      0x0A },
        { "bbb_",      0x0B },
        { "lgt_",      0x0C },
        { "rf_",       0x0E },
        { "nto_",      0x0F },
        { "hydralics", 0x10 },
        { "stereo",    0x11 },
        { nullptr,     0x00 },
    };

    for (const tVehicleComponentFlag* comp = aChassisComps; comp->m_ucName != nullptr; ++comp) {
        if (strncmp(comp->m_ucName, name, strlen(comp->m_ucName)) == 0) {
            bUseCommonVehicleDictionary = true;
            CarMod = comp->m_nFlag;
            return;
        }
    }

    for (const tVehicleComponentFlag* comp = aDummyComps; comp->m_ucName != nullptr; ++comp) {
        if (strncmp(comp->m_ucName, name, strlen(comp->m_ucName)) == 0) {
            bUseCommonVehicleDictionary = true;
            bUsesVehDummy = true;
            CarMod = comp->m_nFlag;
            return;
        }
    }
}

// 0x5B3B20 (recovered from src/_global/SetAtomicModelInfoFlags_005b3b20.c)
// Bit mapping verified against the decomp:
//   bIsRoad               <- dwFlags & 0x00000001 (set/clear)
//   nSpecialType=GLASS_TYPE_1 <- dwFlags & 0x00000200
//   nSpecialType=GLASS_TYPE_2 <- dwFlags & 0x00000400
//   nSpecialType=GARAGE_DOOR  <- dwFlags & 0x00000800
//   nSpecialType=TREE         <- dwFlags & 0x00002000
//   nSpecialType=PALM         <- dwFlags & 0x00004000
//   bDontCollideWithFlyer <- dwFlags & 0x00008000 (set/clear)
//   nSpecialType=UNKNOWN       <- dwFlags & 0x00080000
//   nSpecialType=TAG          <- dwFlags & 0x00100000
//   nSpecialType=BREAKABLE_STATUE <- dwFlags & 0x00400000
void SetAtomicModelInfoFlags(CAtomicModelInfo* modelInfo, uint32_t dwFlags) {
    ::SetBaseModelInfoFlags(modelInfo, dwFlags);

    modelInfo->bIsRoad = (dwFlags & 0x00000001u) != 0;

    if (dwFlags & 0x00000200u) modelInfo->nSpecialType = eModelInfoSpecialType::GLASS_TYPE_1;
    if (dwFlags & 0x00000400u) modelInfo->nSpecialType = eModelInfoSpecialType::GLASS_TYPE_2;
    if (dwFlags & 0x00000800u) modelInfo->nSpecialType = eModelInfoSpecialType::GARAGE_DOOR;
    if (dwFlags & 0x00002000u) modelInfo->nSpecialType = eModelInfoSpecialType::TREE;
    if (dwFlags & 0x00004000u) modelInfo->nSpecialType = eModelInfoSpecialType::PALM;

    modelInfo->bDontCollideWithFlyer = (dwFlags & 0x00008000u) != 0;

    if (dwFlags & 0x00080000u) modelInfo->nSpecialType = eModelInfoSpecialType::UNKNOWN;
    if (dwFlags & 0x00100000u) modelInfo->nSpecialType = eModelInfoSpecialType::TAG;
    if (dwFlags & 0x00400000u) modelInfo->nSpecialType = eModelInfoSpecialType::BREAKABLE_STATUE;
}
