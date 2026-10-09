// CClumpModelInfo - adapted from gta-reversed for clean-room C++ build
// Method implementations. Decompiled reference: src/CClumpModelInfo/*.c
// (GTA SA 1.0 @ addresses noted per method.)

#include "CClumpModelInfo.h"
#include "RenderWare.h" // RW prototypes + game helpers (GetFirstAtomic, ...)

#include <cstdint>
#include <cstring>

#include "CAnimManager.h"

// ---- PORT(renderer): custom pipeline renderers / visibility plugins not ported yet.
class CCustomBuildingRenderer {
public:
    static bool IsCBPCPipelineAttached(RpAtomic* atomic);
    static void AtomicSetup(RpAtomic* atomic);
};
class CCarFXRenderer {
public:
    static bool IsCCPCPipelineAttached(RpAtomic* atomic);
    static void CustomCarPipeAtomicSetup(RpAtomic* atomic);
};
class CVisibilityPlugins {
public:
    static void SetClumpModelInfo(RpClump* clump, CClumpModelInfo* info);
    static void SetFrameHierarchyId(RwFrame* frame, int32_t id);
    static int32_t GetFrameHierarchyId(RwFrame* frame);
    static void SetAtomicRenderCallback(RpAtomic* atomic, void* renderFunc);
};

// Search structs (tCompSearchStructByName/tCompSearchStructById) live in
// CClumpModelInfo.h - shared with CVehicleModelInfo.cpp.

// 0x4C5720
ModelInfoType CClumpModelInfo::GetModelType() {
    return MODEL_INFO_CLUMP;
}

// 0x4C4E40
void CClumpModelInfo::Init() {
    CBaseModelInfo::Init();
    m_nAnimFileIndex = -1;
}

// 0x4C4E30: nullsub (empty function) - no body needed.

// 0x4C4E70
void CClumpModelInfo::DeleteRwObject() {
    // TODO(2026-10-09): RW layer not ported. The body below needs
    // Get2DEffectAtomic(RpClump*) (only the (RpAtomic*, void*) callback
    // overload exists in RenderWare.h) plus the Rp* runtime. Restore when
    // the RW layer lands.
#if 0
    if (!GetRpClump())
        return;

    if (RpAtomic* atomic = Get2DEffectAtomic(GetRpClump()))
        m_n2dfxCount -= static_cast<uint8_t>(RpGeometryGet2dFxCount(RpAtomicGetGeometry(atomic)));

    RpClumpDestroy(GetRpClump());
    m_pRwObject = nullptr;

    CBaseModelInfo::RemoveTexDictionaryRef();
    int32_t animIndex = GetAnimFileIndex();
    if (animIndex != -1)
        CAnimManager::RemoveAnimBlockRef(animIndex);

    // Decomp checks bit 3 of the clump-flags byte (0x08 at m_nFlagsLowerByte):
    // bOwnsCollisionModel.
    if (bOwnsCollisionModel)
        CBaseModelInfo::DeleteCollisionModel();
#endif
}

// 0x4C5110
RwObject* CClumpModelInfo::CreateInstance(RwMatrix* matrix) {
    if (!GetRwObject())
        return nullptr;

    // Decomp calls the no-matrix overload via vtable+0x2c, then copies the 16
    // matrix floats into the clump frame's matrix (parent+0x10).
    // The decomp passes the instance (really an RwFrame) straight to
    // RwFrameGetParent; the port types it RwObject*, hence the cast.
    RwObject* instance = CreateInstance();
    memcpy(RwFrameGetMatrix(RwFrameGetParent(reinterpret_cast<RwFrame*>(instance))), matrix, sizeof(RwMatrix));
    return instance;
}

// 0x4C5140
RwObject* CClumpModelInfo::CreateInstance() {
    // TODO(2026-10-09): RW layer not ported. The body below needs
    // RtAnimAnimation, RpAnimBlendCreateAnimationForHierarchy(RpHAnimHierarchy*)
    // (the port's overload takes (hierarchy, animId) and returns void*), and
    // the rpHANIMHIERARCHYUPDATEMODELLINGMATRICES/rpHANIMHIERARCHYUPDATELTMS
    // constants. Also CAnimManager::ms_aAnimBlocks is private (use the
    // public GetAnimBlocks() accessor when restoring). Restore when the RW
    // layer lands.
#if 0
    if (!GetRwObject())
        return nullptr;

    CBaseModelInfo::AddRef();
    RpClump* clonedClump = RpClumpClone(GetRpClump());

    RpAtomic* atomic = GetFirstAtomic(clonedClump);
    // Decomp: skin present && !(m_nFlagsLowerByte & 2), i.e. !bHasComplexHierarchy.
    if (atomic && RpSkinGeometryGetSkin(RpAtomicGetGeometry(atomic)) && !bHasComplexHierarchy) {
        RpHAnimHierarchy* hierarchy = GetAnimHierarchyFromClump(clonedClump);
        RpClumpForAllAtomics(clonedClump, SetHierarchyForSkinAtomic, hierarchy);
        RtAnimAnimation* anim = RpAnimBlendCreateAnimationForHierarchy(hierarchy);
        RtAnimInterpolatorSetCurrentAnim(hierarchy->currentAnim, anim);
        hierarchy->flags = rpHANIMHIERARCHYUPDATEMODELLINGMATRICES | rpHANIMHIERARCHYUPDATELTMS;
    }

    // Decomp: m_nFlagsLowerByte & 1, i.e. bHasAnimBlend.
    if (bHasAnimBlend) {
        RpAnimBlendClumpInit(clonedClump);
        // Decomp: CAnimManager::GetAnimation(m_nKey, ms_aAnimBlocks + m_nAnimFileIndex).
        CAnimBlendHierarchy* animBlendHier = CAnimManager::GetAnimation(
            m_nKey, &CAnimManager::ms_aAnimBlocks[m_nAnimFileIndex]);
        if (animBlendHier)
            CAnimManager::BlendAnimation(clonedClump, animBlendHier, 2, 1.0F);
    }

    CBaseModelInfo::RemoveRef();
    return reinterpret_cast<RwObject*>(clonedClump);
#else
    return nullptr;
#endif
}

// 0x4C5200
void CClumpModelInfo::SetAnimFile(const char* filename) {
    // Decomp: no-op when the name is "null" (case-insensitive); otherwise
    // heap-duplicates the string into the m_animFileName union member.
    if (_stricmp(filename, "null") == 0)
        return;

    size_t len = strlen(filename) + 1;
    m_animFileName = new char[len];
    memcpy(m_animFileName, filename, len);
}

// 0x4C5250
void CClumpModelInfo::ConvertAnimFileIndex() {
    if (m_nAnimFileIndex == -1)
        return;

    int32_t index = CAnimManager::GetAnimationBlockIndex(m_animFileName);
    delete[] m_animFileName;
    m_nAnimFileIndex = index;
}

// 0x4C5740
int32_t CClumpModelInfo::GetAnimFileIndex() {
    return m_nAnimFileIndex;
}

// 0x4C5710
CBox* CClumpModelInfo::GetBoundingBox() {
    // CBox is the first member of CColModel; the decomp returns m_pColModel
    // reinterpreted, matching gta-reversed.
    return reinterpret_cast<CBox*>(m_pColModel);
}

// 0x4C4F70
void CClumpModelInfo::SetClump(RpClump* clump) {
    // TODO(2026-10-09): RW layer not ported. The body below needs
    // Get2DEffectAtomic(RpClump*) (only the (RpAtomic*, void*) callback
    // overload exists in RenderWare.h) and the
    // rpHANIMHIERARCHYUPDATEMODELLINGMATRICES/rpHANIMHIERARCHYUPDATELTMS
    // constants. Restore when the RW layer lands.
#if 0
    if (GetRwObject()) {
        if (RpAtomic* atomic = Get2DEffectAtomic(GetRpClump()))
            m_n2dfxCount -= static_cast<uint8_t>(RpGeometryGet2dFxCount(RpAtomicGetGeometry(atomic)));
    }

    m_pRwObject = reinterpret_cast<RwObject*>(clump);
    if (clump) {
        if (RpAtomic* atomic = Get2DEffectAtomic(clump))
            m_n2dfxCount += static_cast<uint8_t>(RpGeometryGet2dFxCount(RpAtomicGetGeometry(atomic)));
    }

    CVisibilityPlugins::SetClumpModelInfo(GetRpClump(), this);
    CBaseModelInfo::AddTexDictionaryRef();

    int32_t animIndex = GetAnimFileIndex();
    if (animIndex != -1)
        CAnimManager::AddAnimBlockRef(animIndex);

    RpClumpForAllAtomics(clump, AtomicSetupLightingCB, this);

    RpAtomic* firstAtomic = GetFirstAtomic(clump);
    if (firstAtomic && RpSkinGeometryGetSkin(RpAtomicGetGeometry(firstAtomic))) {
        // Decomp: (m_nFlagsLowerByte & 2), i.e. bHasComplexHierarchy.
        if (bHasComplexHierarchy) {
            RpClumpForAllAtomics(clump, SetHierarchyForSkinAtomic, nullptr);
            return;
        }

        // Grow the skinned geometry's bounding sphere by 1.2x.
        RpMorphTarget* morphTarget = RpGeometryGetMorphTarget(RpAtomicGetGeometry(firstAtomic), 0);
        RpMorphTargetGetBoundingSphere(morphTarget)->radius *= 1.2F;

        RpHAnimHierarchy* hierarchy = GetAnimHierarchyFromClump(clump);
        RpClumpForAllAtomics(clump, SetHierarchyForSkinAtomic, hierarchy);

        // Renormalize skin vertex bone weights ("Average R* dev from 2003").
        RpGeometry* geometry = RpAtomicGetGeometry(firstAtomic);
        RpSkin* skin = RpSkinGeometryGetSkin(geometry);
        RwMatrixWeights* weights = const_cast<RwMatrixWeights*>(RpSkinGetVertexBoneWeights(skin));
        int32_t numVertices = RpGeometryGetNumVertices(geometry);
        for (int32_t i = 0; i < numVertices; ++i) {
            float recip = 1.0F / (weights[i].w0 + weights[i].w1 + weights[i].w2 + weights[i].w3);
            weights[i].w0 *= recip;
            weights[i].w1 *= recip;
            weights[i].w2 *= recip;
            weights[i].w3 *= recip;
        }

        hierarchy->flags = rpHANIMHIERARCHYUPDATEMODELLINGMATRICES | rpHANIMHIERARCHYUPDATELTMS;
    }
#else
    (void)clump;
#endif
}

// 0x4C5460
void CClumpModelInfo::SetFrameIds(RwObjectNameIdAssocation* data) {
    if (!data->m_pName)
        return;

    for (RwObjectNameIdAssocation* comp = data; comp->m_pName != nullptr; ++comp) {
        if ((comp->m_dwFlags & 1) == 0) {
            tCompSearchStructByName searchInfo{ comp->m_pName, nullptr };
            RwFrameForAllChildren(RpClumpGetFrame(GetRpClump()), FindFrameFromNameWithoutIdCB, &searchInfo);
            if (searchInfo.m_pFrame)
                CVisibilityPlugins::SetFrameHierarchyId(searchInfo.m_pFrame, comp->m_dwHierarchyId);
        }
    }
}

// 0x5B3C30 (decomp: src/CClumpModelInfo/SetClumpModelInfoFlags_005b3c30.c)
// bAnimSomething (bit 2 of the clump-flags byte) <- dwFlags & 0x20.
void SetClumpModelInfoFlags(CClumpModelInfo* modelInfo, uint32_t dwFlags) {
    ::SetBaseModelInfoFlags(modelInfo, dwFlags);
    modelInfo->bAnimSomething = (dwFlags & 0x00000020u) != 0;
}

void CClumpModelInfo::SetClumpModelInfoFlags(uint32_t flags) {
    ::SetClumpModelInfoFlags(this, flags);
}

// 0x4C5280
RpAtomic* CClumpModelInfo::SetAtomicRendererCB(RpAtomic* atomic, void* renderFunc) {
    CVisibilityPlugins::SetAtomicRenderCallback(atomic, renderFunc);
    return atomic;
}

// 0x4C4F30
RpAtomic* CClumpModelInfo::AtomicSetupLightingCB(RpAtomic* atomic, void* data) {
    (void)data;
    if (CCustomBuildingRenderer::IsCBPCPipelineAttached(atomic))
        CCustomBuildingRenderer::AtomicSetup(atomic);
    else if (CCarFXRenderer::IsCCPCPipelineAttached(atomic))
        CCarFXRenderer::CustomCarPipeAtomicSetup(atomic);

    return atomic;
}

// 0x4C4EF0
RpAtomic* CClumpModelInfo::SetHierarchyForSkinAtomic(RpAtomic* atomic, void* data) {
    if (data) {
        RpSkinAtomicSetHAnimHierarchy(atomic, static_cast<RpHAnimHierarchy*>(data));
        return nullptr;
    }

    RpHAnimHierarchy* hierarchy = GetAnimHierarchyFromFrame(RpAtomicGetFrame(atomic));
    RpSkinAtomicSetHAnimHierarchy(atomic, hierarchy);
    return atomic;
}

// 0x4C52A0
RwFrame* CClumpModelInfo::FindFrameFromNameCB(RwFrame* frame, void* searchData) {
    auto* searchInfo = static_cast<tCompSearchStructByName*>(searchData);
    if (strcmp(searchInfo->m_pName, GetFrameNodeName(frame)) == 0) {
        searchInfo->m_pFrame = frame;
        return nullptr;
    }

    RwFrameForAllChildren(frame, FindFrameFromNameCB, searchData);
    return searchInfo->m_pFrame ? nullptr : frame;
}

// 0x4C52F0
RwFrame* CClumpModelInfo::FindFrameFromNameWithoutIdCB(RwFrame* frame, void* searchData) {
    auto* searchInfo = static_cast<tCompSearchStructByName*>(searchData);
    if (CVisibilityPlugins::GetFrameHierarchyId(frame) == 0 &&
        strcmp(searchInfo->m_pName, GetFrameNodeName(frame)) == 0) {
        searchInfo->m_pFrame = frame;
        return nullptr;
    }

    RwFrameForAllChildren(frame, FindFrameFromNameWithoutIdCB, searchData);
    return searchInfo->m_pFrame ? nullptr : frame;
}

// 0x4C5350
RwFrame* CClumpModelInfo::FindFrameFromIdCB(RwFrame* frame, void* searchData) {
    auto* searchInfo = static_cast<tCompSearchStructById*>(searchData);
    if (searchInfo->m_nId == CVisibilityPlugins::GetFrameHierarchyId(frame)) {
        searchInfo->m_pFrame = frame;
        return nullptr;
    }

    RwFrameForAllChildren(frame, FindFrameFromIdCB, searchData);
    return searchInfo->m_pFrame ? nullptr : frame;
}

// 0x4C5390
RwFrame* CClumpModelInfo::FillFrameArrayCB(RwFrame* frame, void* data) {
    int32_t id = CVisibilityPlugins::GetFrameHierarchyId(frame);
    if (id > 0)
        static_cast<RwFrame**>(data)[id] = frame;

    RwFrameForAllChildren(frame, FillFrameArrayCB, data);
    return frame;
}

// 0x4C53C0
RwFrame* CClumpModelInfo::GetFrameFromId(RpClump* clump, int32_t id) {
    tCompSearchStructById assoc{ id, nullptr };
    RwFrameForAllChildren(RpClumpGetFrame(clump), FindFrameFromIdCB, &assoc);
    return assoc.m_pFrame;
}

// 0x4C5400
RwFrame* CClumpModelInfo::GetFrameFromName(RpClump* clump, const char* name) {
    tCompSearchStructByName searchInfo{ name, nullptr };
    RwFrameForAllChildren(RpClumpGetFrame(clump), FindFrameFromNameCB, &searchInfo);
    return searchInfo.m_pFrame;
}

// 0x4C5440
void CClumpModelInfo::FillFrameArray(RpClump* clump, RwFrame** frames) {
    RwFrameForAllChildren(RpClumpGetFrame(clump), FillFrameArrayCB, frames);
}
