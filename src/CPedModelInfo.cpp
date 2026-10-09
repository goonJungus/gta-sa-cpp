// CPedModelInfo - adapted from gta-reversed for clean-room C++ build
// Method implementations. Decompiled reference: src/CPedModelInfo/*.c
// (GTA SA 1.0 @ addresses noted per method.)

#include "CPedModelInfo.h"
#include "AnimTypes.h"  // eBoneTag::BONE_SPINE1
#include "RenderWare.h" // RW prototypes + game helpers

#include <cstdint>
#include <cstring>

// Static data members (game addresses recorded from gta-reversed StaticRef).
RwObjectNameIdAssocation CPedModelInfo::m_pPedIds[NUM_PED_NAME_ID_ASSOC]{}; // 0x8A6268
tPedColNodeInfo CPedModelInfo::m_pColNodeInfos[NUM_PED_COL_NODE_INFOS]{};   // 0x8A6308

// ---- PORT(game): CGame not ported yet. Working matrices used by the
//      skinned collision builders; 0xC930B0 / 0xC930C0 in the original.
class CGame {
public:
    static RwMatrix* m_pWorkingMatrix1;
    static RwMatrix* m_pWorkingMatrix2;
};
RwMatrix* CGame::m_pWorkingMatrix1 = nullptr; // 0xC930B0 (clean-room: null until CGame lands)
RwMatrix* CGame::m_pWorkingMatrix2 = nullptr; // 0xC930C0 (clean-room: null until CGame lands)

// ---- PORT(renderer): visibility plugins not ported yet.
class CVisibilityPlugins {
public:
    static void* RenderPedCB;
};
void* CVisibilityPlugins::RenderPedCB = nullptr;

// 0x4C57C0
ModelInfoType CPedModelInfo::GetModelType() {
    return MODEL_INFO_PED;
}

// 0x4C6C50
void CPedModelInfo::DeleteRwObject() {
    CClumpModelInfo::DeleteRwObject();
    delete m_pHitColModel;
    m_pHitColModel = nullptr;
}

// 0x4C7340
void CPedModelInfo::SetClump(RpClump* clump) {
    CClumpModelInfo::SetClump(clump);
    CClumpModelInfo::SetFrameIds(m_pPedIds);

    if (!m_pHitColModel)
        CreateHitColModelSkinned(clump);

    RpClumpForAllAtomics(GetRpClump(), CClumpModelInfo::SetAtomicRendererCB,
                         CVisibilityPlugins::RenderPedCB);
    // Decomp calls GetAnimHierarchyFromClump here and discards the result.
    GetAnimHierarchyFromClump(clump);
}

// 0x4C6D40
void CPedModelInfo::AddXtraAtomics(RpClump* clump) {
    // NOP in the original.
    (void)clump;
}

// 0x4C6D50
void CPedModelInfo::SetFaceTexture(RwTexture* texture) {
    // NOP in the original.
    (void)texture;
}

// 0x4C6D90
void CPedModelInfo::CreateHitColModelSkinned(RpClump* clump) {
    RpHAnimHierarchy* hierarchy = GetAnimHierarchyFromSkinClump(clump);

    CColModel* cm = new CColModel();
    cm->AllocateData(NUM_PED_COL_NODE_INFOS, 0, 0, 0, 0, false);

    RwMatrixInvert(CGame::m_pWorkingMatrix1, RwFrameGetMatrix(RpClumpGetFrame(clump)));

    for (int32_t i = 0; i < NUM_PED_COL_NODE_INFOS; ++i) {
        CColSphere& sphere = cm->m_pColData->m_pSpheres[i];
        tPedColNodeInfo& nodeInfo = m_pColNodeInfos[i];
        memcpy(CGame::m_pWorkingMatrix2, CGame::m_pWorkingMatrix1, sizeof(RwMatrix));

        CVector vecCenter = nodeInfo.m_vecCenter;
        int32_t animId = RpHAnimIDGetIndex(hierarchy, nodeInfo.m_nBoneID);
        RwMatrix* animMat = &RpHAnimHierarchyGetMatrixArray(hierarchy)[animId];
        RwMatrixTransform(CGame::m_pWorkingMatrix2, animMat, rwCOMBINEPRECONCAT);
        RwV3dTransformPoints(&vecCenter, &vecCenter, 1, CGame::m_pWorkingMatrix2);

        sphere.m_vecCenter = vecCenter;
        sphere.m_fRadius = nodeInfo.m_fRadius;
        // Decomp writes 0x3e (62 = SURFACE_PED) to the material byte.
        sphere.m_Surface.m_nMaterial = static_cast<eColSurfaceType>(62); // SURFACE_PED
        sphere.m_Surface.m_nPiece = static_cast<uint8_t>(nodeInfo.m_nFlags);
    }

    // Decomp calls CSphere::Set(1.5, origin) / CBox::Set(min, max); the
    // clean-room stand-ins have no Set() yet, so assign members directly.
    cm->m_boundSphere.m_fRadius = 1.5F;
    cm->m_boundSphere.m_vecCenter = CVector(0.0F, 0.0F, 0.0F);
    cm->m_boundBox.m_vecMin = CVector(-0.5F, -0.5F, -1.2F);
    cm->m_boundBox.m_vecMax = CVector(0.5F, 0.5F, 1.2F);
    cm->m_nColSlot = 0;

    m_pHitColModel = cm;
}

// 0x4C6F70
CColModel* CPedModelInfo::AnimatePedColModelSkinned(RpClump* clump) {
    if (!m_pHitColModel) {
        CreateHitColModelSkinned(clump);
        return m_pHitColModel;
    }

    RpHAnimHierarchy* hierarchy = GetAnimHierarchyFromSkinClump(clump);
    RwMatrixInvert(CGame::m_pWorkingMatrix1, RwFrameGetMatrix(RpClumpGetFrame(clump)));

    for (int32_t i = 0; i < NUM_PED_COL_NODE_INFOS; ++i) {
        CColSphere& sphere = m_pHitColModel->m_pColData->m_pSpheres[i];
        tPedColNodeInfo& nodeInfo = m_pColNodeInfos[i];
        memcpy(CGame::m_pWorkingMatrix2, CGame::m_pWorkingMatrix1, sizeof(RwMatrix));

        CVector vecCenter = nodeInfo.m_vecCenter;
        int32_t animId = RpHAnimIDGetIndex(hierarchy, nodeInfo.m_nBoneID);
        RwMatrix* animMat = &RpHAnimHierarchyGetMatrixArray(hierarchy)[animId];
        RwMatrixTransform(CGame::m_pWorkingMatrix2, animMat, rwCOMBINEPRECONCAT);
        RwV3dTransformPoints(&vecCenter, &vecCenter, 1, CGame::m_pWorkingMatrix2);

        sphere.m_vecCenter = vecCenter;
    }

    // Refit the bounding sphere/box around the spine joint.
    memcpy(CGame::m_pWorkingMatrix2, CGame::m_pWorkingMatrix1, sizeof(RwMatrix));
    int32_t animId = RpHAnimIDGetIndex(hierarchy, eBoneTag::BONE_SPINE1);
    RwMatrix* animMat = &RpHAnimHierarchyGetMatrixArray(hierarchy)[animId];
    RwMatrixTransform(CGame::m_pWorkingMatrix2, animMat, rwCOMBINEPRECONCAT);
    CVector vecSpine(0.0F, 0.0F, 0.0F);
    RwV3dTransformPoints(&vecSpine, &vecSpine, 1, CGame::m_pWorkingMatrix2);

    m_pHitColModel->m_boundSphere.m_fRadius = 1.5F;
    m_pHitColModel->m_boundSphere.m_vecCenter = vecSpine;
    m_pHitColModel->m_boundBox.m_vecMin = CVector(vecSpine.x - 1.2F, vecSpine.y - 1.2F, vecSpine.z - 1.2F);
    m_pHitColModel->m_boundBox.m_vecMax = CVector(vecSpine.x + 1.2F, vecSpine.y + 1.2F, vecSpine.z + 1.2F);

    return m_pHitColModel;
}

// 0x4C7170
CColModel* CPedModelInfo::AnimatePedColModelSkinnedWorld(RpClump* clump) {
    if (!m_pHitColModel)
        CreateHitColModelSkinned(clump);

    RpHAnimHierarchy* hierarchy = GetAnimHierarchyFromSkinClump(clump);

    // World-space variant: transforms by the bone matrices directly (no
    // working-matrix / clump-frame inversion).
    for (int32_t i = 0; i < NUM_PED_COL_NODE_INFOS; ++i) {
        CColSphere& sphere = m_pHitColModel->m_pColData->m_pSpheres[i];
        tPedColNodeInfo& nodeInfo = m_pColNodeInfos[i];

        CVector vecCenter = nodeInfo.m_vecCenter;
        int32_t animId = RpHAnimIDGetIndex(hierarchy, nodeInfo.m_nBoneID);
        RwMatrix* animMat = &RpHAnimHierarchyGetMatrixArray(hierarchy)[animId];
        RwV3dTransformPoints(&vecCenter, &vecCenter, 1, animMat);

        sphere.m_vecCenter = vecCenter;
    }

    int32_t animId = RpHAnimIDGetIndex(hierarchy, eBoneTag::BONE_SPINE1);
    RwMatrix* animMat = &RpHAnimHierarchyGetMatrixArray(hierarchy)[animId];
    CVector vecSpine(0.0F, 0.0F, 0.0F);
    RwV3dTransformPoints(&vecSpine, &vecSpine, 1, animMat);

    m_pHitColModel->m_boundSphere.m_fRadius = 1.5F;
    m_pHitColModel->m_boundSphere.m_vecCenter = vecSpine;
    m_pHitColModel->m_boundBox.m_vecMin = CVector(vecSpine.x - 1.2F, vecSpine.y - 1.2F, vecSpine.z - 1.2F);
    m_pHitColModel->m_boundBox.m_vecMax = CVector(vecSpine.x + 1.2F, vecSpine.y + 1.2F, vecSpine.z + 1.2F);

    return m_pHitColModel;
}

// 0x4C7300
void CPedModelInfo::IncrementVoice() {
    // Decomp: if either bound is negative, invalidate; else cycle m_nVoiceId
    // through [m_nVoiceMin, m_nVoiceMax].
    if (m_nVoiceMin < 0 || m_nVoiceMax < 0) {
        m_nVoiceId = -1;
        return;
    }

    ++m_nVoiceId;
    if (m_nVoiceId > m_nVoiceMax || m_nVoiceId < m_nVoiceMin)
        m_nVoiceId = m_nVoiceMin;
}
