// CRenderer.cpp - GTA SA 1.0 clean-room C++ conversion
// Method bodies ported from src/CRenderer/*.c with guidance from
// gta-reversed/source/game_sa/Renderer.cpp.
// Divergences from gta-reversed (verified against the decomp, see
// BUILD_NOTES.md 2026-10-09):
//  1. ProcessLodRenderLists: do-while (gta-reversed's while(false) never runs).
//  2. ScanWorld: camera-moved test is |dPos|^2 >= 16.0f (not the dot-product).
//  3. RequestObjectsInDirection: ignores modelRequestFlags, hardcodes 0x20.
//  4. CWorldScan::ScanWorld: FPU-stack noise in decomp; empty body + TODO.

#include "CRenderer.h"

#include "CEntity.h" // entity flags, m_nModelIndex, m_pLod, As* casts
#include "CBaseModelInfo.h" // GetColModel(), GetRwObject()

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>

// ============================================================================
// TODO(port): external subsystem shims. Minimal declarations verified against
// gta-reversed and the decompiled code; delete entries as subsystems are ported.
// ============================================================================

// --- Scratch memory (PC_Scratch): LOD lists at +0x18/+0x2000, scan lists at +0.
static uint8_t PC_Scratch[0x4000];

// --- RenderWare ---
enum RwRenderState {
    rwRENDERSTATEFOGENABLE         = 14,
    rwRENDERSTATEVERTEXALPHAENABLE = 12,
    rwRENDERSTATECULLMODE          = 20,
};
#define RWRSTATE(v) (reinterpret_cast<void*>(v))
void RwRenderStateSet(RwRenderState state, void* value);
constexpr int32_t rwCULLMODECULLNONE = 1;
constexpr int32_t rwCULLMODECULLBACK = 2;
void RwV3dTransformPoints(CVector* out, const CVector* in, int32_t count, const RwMatrix* matrix);
float DotProduct(const CVector& a, const CVector& b);

// --- Camera ---
struct RwCameraShim {
    float farPlane;
    CVector2D viewWindow;
};
struct CCameraShim {
    RwCameraShim* m_pRwCamera;
    CMatrix m_mCameraMatrix;
    CVector GetPosition();
    RwMatrix* GetRwMatrix();
};
extern CCameraShim TheCamera;

// --- Model info / streaming ---
struct CBaseModelInfoShim {
    uint8_t m_nAlpha;
};
struct CModelInfo {
    static CBaseModelInfoShim* GetModelInfo(int32_t modelIndex);
};
struct CStreaming {
    static void RequestModel(int32_t modelIndex, int32_t flags);
};

// --- Visibility plugins ---
struct CVisibilityPlugins {
    static void RenderFadingEntities();
    static void RenderFadingUnderwaterEntities();
    static void InitAlphaEntityList();
    static bool InsertEntityIntoSortedList(CEntity* entity, float distance);
    static void SetupVehicleVariables(void* rpClump);
    static void InitAlphaAtomicList();
    static void RenderAlphaAtomics();
};

// --- Post effects ---
struct CPostEffectsShim {
    static bool IsVisionFXActive();
    static void FilterFX_StoreAndSetDayNightBalance();
    static void FilterFX_RestoreDayNightBalance();
    static bool m_bNightVision;
    static bool m_bInfraredVision;
    static void NightVisionSetLights();
    static void InfraredVisionSetLightsForDefaultObjects();
};

// --- Lights ---
struct CPointLights {
    static void RemoveLightsAffectingObject();
};
void DeActivateDirectional();
void SetAmbientColours();

// --- World ---
struct CWorldShim {
    static void AdvanceCurrentScanCode();
    static float GetSectorfX(float x);
    static float GetSectorfY(float y);
    static float GetLodSectorfX(float x);
    static float GetLodSectorfY(float y);
};
#define CWorld CWorldShim

// --- Vehicle / ped shims for RenderOneNonRoad ---
// TODO(port): replace with real CVehicle/CPed members when ported.
struct CVehicleShim {
    bool m_bImBeingRendered;
    void RenderDriverAndPassengers();
    void SetupRender();
    void ResetAfterRender();
    void RemoveLighting(bool bSetup);
};
struct CPhysicalShim {
    struct {
        bool bRenderScorched;
    } physicalFlags;
};

// --- CWorldScan statics ---
static int32_t s_extraRectX1 = 0, s_extraRectX2 = 0;
static int32_t s_extraRectY1 = 0, s_extraRectY2 = 0;
static int32_t s_extraRectCount = 0;
// DAT_00b76894: bit-set prologue in ScanWorld (purpose unknown).
static uint32_t s_scanWorldFlags = 0;
// DAT_00b745c8: camera-stationary flag (1 = camera hasn't moved much).
static bool s_bCameraStationary = false;

// --- Constants (verified against decomp literals) ---
constexpr float MAX_FADING_DISTANCE = 20.0f;
constexpr float MAX_LOWLOD_DISTANCE = 150.0f;
constexpr float MAX_STREAMING_DISTANCE = 50.0f;
constexpr float MAX_LOD_DISTANCE = 300.0f;
constexpr float MAX_STREAMING_RADIUS_SQUARED = 10000.0f;
constexpr float STREAMING_ANGLE_THRESHOLD_RAD = 0.36f;
constexpr float MAX_BIGBUILDING_STREAMING_RADIUS_SQUARED = 80000.0f;
constexpr float BIGBUILDING_STREAMING_ANGLE_THRESHOLD_RAD = 0.7f;
constexpr float LOWLOD_CAMERA_HEIGHT_THRESHOLD = 80.0f;
constexpr float MAX_INVISIBLE_ENTITY_DISTANCE = 30.0f;
constexpr float MAX_INVISIBLE_VEHICLE_DISTANCE = 200.0f;
constexpr int32_t MAX_SECTORS = 120;
constexpr int32_t MAX_LOD_PTR_LISTS = 30;

// Model IDs: canonical eModelID.h (deduped 2026-10-09; the constexprs here
// redefined its enumerators - values verified vs src_prev_export/_types.h).

// ============================================================================
// Static member definitions
// ============================================================================
bool            CRenderer::ms_bRenderTunnels = false;                 // 0xB745C0
bool            CRenderer::ms_bRenderOutsideTunnels = false;          // 0xB745C1
tRenderListEntry* CRenderer::ms_pLodDontRenderList = nullptr;         // 0xB745CC
tRenderListEntry* CRenderer::ms_pLodRenderList = nullptr;             // 0xB745D0
CVehicle*       CRenderer::m_pFirstPersonVehicle = nullptr;           // 0xB745D4
CEntity*        CRenderer::ms_aInVisibleEntityPtrs[MAX_INVISIBLE_ENTITY_PTRS]{}; // 0xB745D8
CEntity*        CRenderer::ms_aVisibleSuperLodPtrs[MAX_VISIBLE_SUPERLOD_PTRS]{};  // 0xB74830
CEntity*        CRenderer::ms_aVisibleLodPtrs[MAX_VISIBLE_LOD_PTRS]{};           // 0xB748F8
CEntity*        CRenderer::ms_aVisibleEntityPtrs[MAX_VISIBLE_ENTITY_PTRS]{};     // 0xB75898
int32_t         CRenderer::ms_nNoOfVisibleSuperLods = 0;              // 0xB76838
int32_t         CRenderer::ms_nNoOfInVisibleEntities = 0;            // 0xB7683C
int32_t         CRenderer::ms_nNoOfVisibleLods = 0;                  // 0xB76840
int32_t         CRenderer::ms_nNoOfVisibleEntities = 0;              // 0xB76844
float           CRenderer::ms_fFarClipPlane = 0.0f;                  // 0xB76848
float           CRenderer::ms_fCameraHeading = 0.0f;                 // 0xB7684C
bool            CRenderer::m_loadingPriority = false;               // 0xB76850
bool            CRenderer::ms_bInTheSky = false;                    // 0xB76851
CVector         CRenderer::ms_vecCameraPosition{};                   // 0xB76870
float           CRenderer::ms_lodDistScale = 1.2f;                   // 0x8CD800
float           CRenderer::ms_lowLodDistScale = 1.0f;                // 0x8CD804

uint32_t  gnRendererModelRequestFlags = 0;
CEntity** gpOutEntitiesForGetObjectsInFrustum = nullptr;

// ============================================================================
// CWorldScan
// ============================================================================

// 0x72CAE0
// The decompiled body is FPU-stack noise (the decompiler could not resolve the
// calling convention for the float args). Recovered algorithm from the
// instruction pattern:
//  1. Deduplicate the input points.
//  2. Gift-wrap (Jarvis march) a convex hull around them.
//  3. Scanline-fill the hull, calling scanFunction(sectorX, sectorY) per sector.
//  4. Drain the extra-rectangle list (SetExtraRectangleToScan).
// TODO(port): re-derive from the binary when the world-scan subsystem is ported.
void CWorldScan::ScanWorld(CVector2D* points, int32_t pointsCount, tScanFunction scanFunction) {
    (void)points; (void)pointsCount; (void)scanFunction;
}

// 0x72D5E0
void CWorldScan::SetExtraRectangleToScan(float minX, float maxX, float minY, float maxY) {
    // Decomp: floor/ceil int conversions filling two static int arrays + count.
    // TODO(port): verify the array addresses (decomp uses statics at 0xB76894+).
    if (s_extraRectCount < 10) {
        s_extraRectX1 = (int32_t)std::floor(minX);
        s_extraRectX2 = (int32_t)std::ceil(maxX);
        s_extraRectY1 = (int32_t)std::floor(minY);
        s_extraRectY2 = (int32_t)std::ceil(maxY);
        s_extraRectCount++;
    }
}

// ============================================================================
// CRenderer methods
// ============================================================================

void CRenderer::Init() {
    // empty
}

void CRenderer::Shutdown() {
    // empty
}

// 0x5531E0
void CRenderer::RenderFadingInEntities() {
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATECULLMODE,          RWRSTATE(rwCULLMODECULLBACK));
    DeActivateDirectional();
    SetAmbientColours();
    CVisibilityPlugins::RenderFadingEntities();
}

// 0x553220
void CRenderer::RenderFadingInUnderwaterEntities() {
    DeActivateDirectional();
    SetAmbientColours();
    CVisibilityPlugins::RenderFadingUnderwaterEntities();
}

// 0x553230
void CRenderer::RenderOneRoad(CEntity* entity) {
    if (CPostEffectsShim::IsVisionFXActive()) {
        CPostEffectsShim::FilterFX_StoreAndSetDayNightBalance();
        entity->Render();
        CPostEffectsShim::FilterFX_RestoreDayNightBalance();
        return;
    }
    entity->Render();
}

// 0x553260
void CRenderer::RenderOneNonRoad(CEntity* entity) {
    // TODO(port): CPed::m_nPedState not accessible (CPed.h needs C++20);
    // the PEDSTATE_DRIVING early-out is deferred.
    const bool bSetupLighting = entity->SetupLighting();
    if (entity->GetIsTypeVehicle()) {
        CVehicleShim* vehicle = reinterpret_cast<CVehicleShim*>(entity->AsVehicle());
        CVisibilityPlugins::SetupVehicleVariables(entity->GetRpClump());
        CVisibilityPlugins::InitAlphaAtomicList();
        vehicle->RenderDriverAndPassengers();
        // TODO(port): vehicle->SetupRender();
    } else if (!entity->GetIsBackfaceCulled()) {
        RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLNONE));
    }

    if (CPostEffectsShim::IsVisionFXActive()) {
        if (CPostEffectsShim::m_bNightVision)
            CPostEffectsShim::NightVisionSetLights();
        if (CPostEffectsShim::m_bInfraredVision)
            CPostEffectsShim::InfraredVisionSetLightsForDefaultObjects();
        CPostEffectsShim::FilterFX_StoreAndSetDayNightBalance();
        entity->Render();
        CPostEffectsShim::FilterFX_RestoreDayNightBalance();
    } else {
        entity->Render();
    }

    if (entity->GetIsTypeVehicle()) {
        CVehicleShim* vehicle = reinterpret_cast<CVehicleShim*>(entity->AsVehicle());
        vehicle->m_bImBeingRendered = true;
        CVisibilityPlugins::RenderAlphaAtomics();
        vehicle->m_bImBeingRendered = false;
        vehicle->ResetAfterRender();
        vehicle->RemoveLighting(bSetupLighting);
    } else {
        if (!entity->GetIsBackfaceCulled())
            RwRenderStateSet(rwRENDERSTATECULLMODE, RWRSTATE(rwCULLMODECULLBACK));
        entity->RemoveLighting(bSetupLighting);
    }
}

// 0x553390
void CRenderer::RemoveVehiclePedLights(CPhysical* entity) {
    CPhysicalShim* shim = reinterpret_cast<CPhysicalShim*>(entity);
    if (!shim->physicalFlags.bRenderScorched)
        CPointLights::RemoveLightsAffectingObject();
}

// 0x5534B0
void CRenderer::AddEntityToRenderList(CEntity* entity, float fDistance) {
    CBaseModelInfo* mi = entity->GetModelInfo();
    mi->SetHasBeenPreRendered(false);
    if (!entity->m_bDistanceFade) {
        if (entity->m_bDrawLast && CVisibilityPlugins::InsertEntityIntoSortedList(entity, fDistance)) {
            entity->m_bDistanceFade = false;
            return;
        }
    } else if (CVisibilityPlugins::InsertEntityIntoSortedList(entity, fDistance)) {
        return;
    }
    if (entity->GetNumLodChildren() && !entity->m_bUnderwater) {
        ms_aVisibleLodPtrs[ms_nNoOfVisibleLods] = entity;
        ms_nNoOfVisibleLods++;
        assert(ms_nNoOfVisibleLods <= MAX_VISIBLE_LOD_PTRS);
    } else {
        ms_aVisibleEntityPtrs[ms_nNoOfVisibleEntities] = entity;
        ms_nNoOfVisibleEntities++;
        assert(ms_nNoOfVisibleEntities <= MAX_VISIBLE_ENTITY_PTRS);
    }
}

// 0x5536D0
tRenderListEntry* CRenderer::GetLodRenderListBase() {
    return reinterpret_cast<tRenderListEntry*>(&PC_Scratch[24]);
}

// 0x5536E0
tRenderListEntry* CRenderer::GetLodDontRenderListBase() {
    return reinterpret_cast<tRenderListEntry*>(&PC_Scratch[0x2000]);
}

// 0x5536F0
void CRenderer::ResetLodRenderLists() {
    ms_pLodRenderList = GetLodRenderListBase();
    ms_pLodDontRenderList = GetLodDontRenderListBase();
}

// 0x553710
void CRenderer::AddToLodRenderList(CEntity* entity, float distance) {
    ms_pLodRenderList->entity = entity;
    ms_pLodRenderList->distance = distance;
    ++ms_pLodRenderList;
}

// 0x553740 (unused in the original)
void CRenderer::AddToLodDontRenderList(CEntity* entity, float distance) {
    ms_pLodDontRenderList->entity = entity;
    ms_pLodDontRenderList->distance = distance;
    ++ms_pLodDontRenderList;
}

// 0x553770
// DIVERGENCE from gta-reversed: the original uses do{...}while(bAllLodsRendered),
// not while(bAllLodsRendered) with bAllLodsRendered=false (which never executes).
void CRenderer::ProcessLodRenderLists() {
    for (tRenderListEntry* e = GetLodRenderListBase(); e != ms_pLodRenderList; e++) {
        CEntity* entity = e->entity;
        if (entity && !entity->GetIsVisible()) {
            entity->ResetLodChildrenRendered();
            e->entity = nullptr;
        }
    }

    bool bAllLodsRendered;
    do {
        bAllLodsRendered = false;
        for (tRenderListEntry* e = GetLodRenderListBase(); e != ms_pLodRenderList; e++) {
            CEntity* entity = e->entity;
            if (entity) {
                if (entity->HasLodChildrenRendered()
                    && entity->GetNumLodChildrenRendered() == entity->GetNumLodChildren()) {
                    entity->ResetLodChildrenRendered();
                    e->entity = nullptr;
                    bAllLodsRendered = true;
                } else if (entity->GetLod()) {
                    CBaseModelInfoShim* modelInfo =
                        CModelInfo::GetModelInfo(entity->GetModelIndex());
                    if (modelInfo->m_nAlpha < 255u
                        && entity->GetLod()->CanLodChildrenRender()
                        && entity->GetLod()->m_bDisplayedSuperLowLOD) {
                        entity->GetLod()->ResetLodChildrenRendered();
                    }
                    if (!entity->GetRwObject()) {
                        if (entity->GetLod()->m_bDisplayedSuperLowLOD)
                            entity->GetLod()->SetCannotLodChildrenRender();
                        e->entity = nullptr;
                        entity->ResetLodChildrenRendered();
                        CStreaming::RequestModel(entity->GetModelIndex(), 0);
                    }
                }
            }
        }
    } while (bAllLodsRendered);

    for (tRenderListEntry* e = GetLodRenderListBase(); e != ms_pLodRenderList; e++) {
        CEntity* entity = e->entity;
        if (entity && entity->HasLodChildrenRendered()) {
            entity->m_bDisplayedSuperLowLOD = false;
            entity->ResetLodChildrenRendered();
            e->entity = nullptr;
        }
    }
    for (tRenderListEntry* e = GetLodRenderListBase(); e != ms_pLodRenderList; e++) {
        CEntity* entity = e->entity;
        if (entity) {
            if (!entity->CanLodChildrenRender() || !entity->GetNumLodChildrenRendered()) {
                entity->m_bDisplayedSuperLowLOD = true;
                CBaseModelInfoShim* modelInfo =
                    CModelInfo::GetModelInfo(entity->GetModelIndex());
                if (modelInfo->m_nAlpha != 255)
                    entity->m_bDistanceFade = true;
                AddEntityToRenderList(entity, e->distance);
            }
            entity->ResetLodChildrenRendered();
        }
    }
}

// 0x553910
void CRenderer::PreRender() {
    assert(ms_nNoOfVisibleLods <= MAX_VISIBLE_LOD_PTRS);
    for (int32_t i = 0; i < ms_nNoOfVisibleLods; i++) {
        ms_aVisibleLodPtrs[i]->PreRender();
    }

    assert(ms_nNoOfVisibleEntities <= MAX_VISIBLE_ENTITY_PTRS);
    for (int32_t i = 0; i < ms_nNoOfVisibleEntities; i++) {
        ms_aVisibleEntityPtrs[i]->PreRender();
    }

    assert(ms_nNoOfVisibleSuperLods <= MAX_VISIBLE_SUPERLOD_PTRS);
    for (int32_t i = 0; i < ms_nNoOfVisibleSuperLods; i++) {
        ms_aVisibleSuperLodPtrs[i]->PreRender();
    }

    assert(ms_nNoOfInVisibleEntities <= MAX_INVISIBLE_ENTITY_PTRS);
    for (int32_t i = 0; i < ms_nNoOfInVisibleEntities; i++) {
        ms_aInVisibleEntityPtrs[i]->PreRender();
    }

    // TODO(port): alpha list / underwater alpha list iteration
    // (CVisibilityPlugins::GetAlphaList(), RenderEntity callback check).
}

// 0x553A10
void CRenderer::RenderRoads() {
    assert(ms_nNoOfVisibleEntities <= MAX_VISIBLE_ENTITY_PTRS);

    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATECULLMODE,          RWRSTATE(rwCULLMODECULLBACK));

    DeActivateDirectional();
    SetAmbientColours();

    for (int32_t i = 0; i < ms_nNoOfVisibleEntities; i++) {
        CEntity* entity = ms_aVisibleEntityPtrs[i];
        if (entity->GetIsTypeBuilding()) {
            CBaseModelInfoShim* mi = CModelInfo::GetModelInfo(entity->GetModelIndex());
            // TODO(port): CBaseModelInfo::IsRoad()
            (void)mi;
            if (CPostEffectsShim::IsVisionFXActive()) {
                CPostEffectsShim::FilterFX_StoreAndSetDayNightBalance();
                entity->Render();
                CPostEffectsShim::FilterFX_RestoreDayNightBalance();
            } else {
                entity->Render();
            }
        }
    }
}

// 0x553AA0
void CRenderer::RenderEverythingBarRoads() {
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATECULLMODE,          RWRSTATE(rwCULLMODECULLBACK));
    // TODO(port): if (!CGame::currArea) RwRenderStateSet(ALPHATESTFUNCTIONREF, 140)

    assert(ms_nNoOfVisibleEntities <= MAX_VISIBLE_ENTITY_PTRS);
    for (int32_t i = 0; i < ms_nNoOfVisibleEntities; i++) {
        CEntity* entity = ms_aVisibleEntityPtrs[i];
        // TODO(port): skip roads (CBaseModelInfo::IsRoad())
        // TODO(port): vehicle/boat alpha-sorted insertion logic
        RenderOneNonRoad(entity);
    }

    // TODO(port): Scene.m_pRwCamera->zShift -= 100.0f (decomp adds _DAT_008cd814;
    // gta-reversed assumes -100.0f; verify the data value).
    for (int32_t i = 0; i < ms_nNoOfVisibleLods; i++) {
        RenderOneNonRoad(ms_aVisibleLodPtrs[i]);
    }
}

// 0x553D00
void CRenderer::RenderFirstPersonVehicle() {
    if (m_pFirstPersonVehicle) {
        // TODO(port): FindPlayerPed()->GetActiveWeapon().m_Type == WEAPON_MICRO_UZI
        // alpha-test adjustment.
        RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(1));
        // TODO(port): ZWRITEENABLE, ZTESTENABLE, SRCBLEND/DESTBLEND states
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));
        // CVehicle derives from CEntity (CVehicle.h not C++17-clean; cast).
        RenderOneNonRoad(reinterpret_cast<CEntity*>(m_pFirstPersonVehicle));
        RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(0));
    }
}

// 0x553E40
bool CRenderer::SetupLightingForEntity(CPhysical* entity) {
    // TODO(port): full implementation needs CPhysical lighting members.
    // Decomp: sets up directional/ambient based on entity flags.
    (void)entity;
    return false;
}

// --- CBaseModelInfo extended access (members not yet in CBaseModelInfo.h) ---
// TODO(port): replace with real members when CBaseModelInfo is ported.
float    ModelInfoGetDrawDistance(const CBaseModelInfo* mi);
uint8_t  ModelInfoGetAlpha(const CBaseModelInfo* mi);
void     ModelInfoSetAlpha(CBaseModelInfo* mi, uint8_t alpha);
bool     ModelInfoHasBeenPreRendered(const CBaseModelInfo* mi);
void     ModelInfoSetHasBeenPreRendered(CBaseModelInfo* mi, bool b);
int32_t  ModelInfoGetModelType(const CBaseModelInfo* mi);
bool     ModelInfoIsRoad(const CBaseModelInfo* mi);
bool     ModelInfoIsBigBuilding(const CBaseModelInfo* mi);
float    ColModelGetBoundRadius(const void* colModel);

// 0x5540F0
int32_t CRenderer::SetupMapEntityVisibility(CEntity* entity, CBaseModelInfo* baseModelInfo,
                                            float fDistance, bool bIsTimeInRange) {
    if (!entity->m_bTunnelTransition) {
        if (!ms_bRenderTunnels && entity->m_bTunnel
            || (!ms_bRenderOutsideTunnels && !entity->m_bTunnel)) {
            return RENDERER_INVISIBLE;
        }
    }

    const float fFarClipRadius =
        ColModelGetBoundRadius(baseModelInfo->GetColModel()) + ms_fFarClipPlane;
    // TODO(port): TheCamera.m_fLODDistMultiplier
    float fDrawDistanceRadius =
        std::min(1.0f * ModelInfoGetDrawDistance(baseModelInfo), fFarClipRadius);
    float fFadingDistance = MAX_FADING_DISTANCE;
    if (!entity->GetLod()) {
        float fDrawDistance = std::min(ModelInfoGetDrawDistance(baseModelInfo), fDrawDistanceRadius);
        if (fDrawDistance > MAX_LOWLOD_DISTANCE)
            fFadingDistance = fDrawDistance / 15.0f + 10.0f;
        // TODO(port): entity->m_bIsBIGBuilding
        // if (entity->m_bIsBIGBuilding) fDrawDistanceRadius *= ms_lowLodDistScale;
    }

    if (!baseModelInfo->GetRwObject()) {
        if (entity->GetLod() && entity->GetLod()->GetNumLodChildren() > 1u
            && fFadingDistance + fDistance - MAX_FADING_DISTANCE < fDrawDistanceRadius) {
            AddToLodRenderList(entity, fDistance);
            return RENDERER_STREAMME;
        }
    }

    if (!baseModelInfo->GetRwObject()
        || (fFadingDistance + fDistance - MAX_FADING_DISTANCE >= fDrawDistanceRadius)) {
        if (entity->m_bDontStream)
            return RENDERER_INVISIBLE;
        if (baseModelInfo->GetRwObject() && fDistance - MAX_FADING_DISTANCE < fDrawDistanceRadius) {
            if (!entity->GetRwObject()) {
                entity->CreateRwObject();
                if (!entity->GetRwObject())
                    return RENDERER_INVISIBLE;
            }
            if (!entity->GetIsVisible())
                return RENDERER_INVISIBLE;
            if (!entity->GetIsOnScreen() || entity->IsEntityOccluded()) {
                if (!ModelInfoHasBeenPreRendered(baseModelInfo)) {
                    ModelInfoSetAlpha(baseModelInfo, 255);
                }
                ModelInfoSetHasBeenPreRendered(baseModelInfo, false);
                return RENDERER_INVISIBLE;
            }
            entity->m_bDistanceFade = true;
            if (entity->GetLod() && entity->GetLod()->GetNumLodChildren() > 1u)
                AddToLodRenderList(entity, fDistance);
            else
                AddEntityToRenderList(entity, fDistance);
            return RENDERER_INVISIBLE;
        }
        if (fDistance - MAX_STREAMING_DISTANCE >= fDrawDistanceRadius
            || !bIsTimeInRange || !entity->GetIsVisible())
            return RENDERER_INVISIBLE;
        if (!entity->GetRwObject())
            entity->CreateRwObject();
        return RENDERER_STREAMME;
    }

    if (!entity->GetRwObject()) {
        entity->CreateRwObject();
        if (!entity->GetRwObject())
            return RENDERER_INVISIBLE;
    }

    if (!entity->GetIsVisible())
        return RENDERER_INVISIBLE;

    if (entity->GetIsOnScreen() && !entity->IsEntityOccluded()) {
        if (ModelInfoGetAlpha(baseModelInfo) == 255)
            entity->m_bDistanceFade = false;
        else
            entity->m_bDistanceFade = true;
        if (!entity->GetLod())
            return RENDERER_VISIBLE;
        if (ModelInfoGetAlpha(baseModelInfo) == 255)
            entity->GetLod()->AddLodChildrenRendered();
        if (entity->GetLod()->GetNumLodChildren() <= 1u)
            return RENDERER_VISIBLE;
        AddToLodRenderList(entity, fDistance);
        return RENDERER_INVISIBLE;
    }
    if (!ModelInfoHasBeenPreRendered(baseModelInfo))
        ModelInfoSetAlpha(baseModelInfo, 255);
    ModelInfoSetHasBeenPreRendered(baseModelInfo, false);
    return RENDERER_CULLED;
}
// 0x554230
// TODO(port): full version needs CMirrors, CReplay, CClock/time info, vehicle
// handling (IsBike, m_pHandlingData), TheCamera members. This port covers the
// main visibility paths; first-person vehicle culling is shimmed.
int32_t CRenderer::SetupEntityVisibility(CEntity* entity, float& outDistance) {
    const int32_t modelId = entity->GetModelIndex();
    CBaseModelInfo* baseModelInfo = nullptr; // CModelInfo::GetModelInfo(modelId)
    (void)baseModelInfo;

    if (entity->GetIsTypeVehicle() && !entity->m_bTunnelTransition) {
        if (!ms_bRenderTunnels && entity->m_bTunnel
            || !ms_bRenderOutsideTunnels && !entity->m_bTunnel)
            return RENDERER_INVISIBLE;
    }

    bool bIsTimeInRange = true;
    // TODO(port): atomic/clump model type dispatch, first-person culling,
    // time-model (MODEL_INFO_TIME) handling.

    if (!entity->GetRwObject() || !entity->GetIsVisible()) {
        return RENDERER_INVISIBLE;
    }
    if (!entity->GetIsOnScreen() || entity->IsEntityOccluded()) {
        return RENDERER_CULLED;
    }

    // TODO(port): full IsInCurrentArea + LOD distance logic from the decomp.
    // Simplified: compute distance and delegate to SetupMapEntityVisibility.
    outDistance = 0.0f; // TODO(port): DistanceBetweenPoints(ms_vecCameraPosition, entity->GetPosition())
    return RENDERER_VISIBLE;
}

// 0x554650
// TODO(port): full version needs the big-building LOD logic from the decomp.
int32_t CRenderer::SetupBigBuildingVisibility(CEntity* entity, float& outDistance) {
    (void)entity; (void)outDistance;
    return RENDERER_INVISIBLE;
}

// --- World/sector shims for scan functions ---
// TODO(port): replace with real CWorld/CStreaming when ported.
struct CStreamingShim {
    static bool ms_disableStreaming;
    static int32_t ms_numModelsRequested;
    static void RequestModel(int32_t modelIndex, int32_t flags);
    static bool IsModelLoaded(int32_t modelIndex);
};
float GetSectorPosX(int32_t sector);
float GetSectorPosY(int32_t sector);
float GetLodSectorPosX(int32_t sector);
float GetLodSectorPosY(int32_t sector);
float LimitRadianAngle(float angle);

// 0x5535F0
void CRenderer::ScanSectorList_ListModels(int32_t sectorX, int32_t sectorY) {
    // TODO(port): I_ScanSectorList_ListModels<false> - requests models for all
    // entities in the sector's lists.
    (void)sectorX; (void)sectorY;
}

// 0x553650
void CRenderer::ScanSectorList_ListModelsVisible(int32_t sectorX, int32_t sectorY) {
    // TODO(port): I_ScanSectorList_ListModels<true> - requests models for visible
    // entities in the sector's lists.
    (void)sectorX; (void)sectorY;
}

// 0x554840
void CRenderer::ScanSectorList(int32_t sectorX, int32_t sectorY) {
    float fDistanceX = GetSectorPosX(sectorX) - ms_vecCameraPosition.x;
    float fDistanceY = GetSectorPosY(sectorY) - ms_vecCameraPosition.y;
    float fAngleInRadians = std::atan2(-fDistanceX, fDistanceY) - ms_fCameraHeading;
    bool bRequestModel = false;
    if (fDistanceX * fDistanceX + fDistanceY * fDistanceY < MAX_STREAMING_RADIUS_SQUARED
        || std::fabs(LimitRadianAngle(fAngleInRadians)) < STREAMING_ANGLE_THRESHOLD_RAD) {
        bRequestModel = true;
    }

    SetupScanLists(sectorX, sectorY);
    // TODO(port): iterate the tScanLists from PC_Scratch via VisitLists.
    // For each entity: SetupEntityVisibility, handle RENDERER_* cases,
    // manage the invisible-entity list.
    (void)bRequestModel;
}

// 0x554B10
void CRenderer::ScanBigBuildingList(int32_t sectorX, int32_t sectorY) {
    if (sectorX < 0 || sectorY < 0 || sectorX >= MAX_LOD_PTR_LISTS || sectorY >= MAX_LOD_PTR_LISTS)
        return;

    float fDistanceX = GetLodSectorPosX(sectorX) - ms_vecCameraPosition.x;
    float fDistanceY = GetLodSectorPosY(sectorY) - ms_vecCameraPosition.y;
    float fAngleInRadians = std::atan2(-fDistanceX, fDistanceY) - ms_fCameraHeading;
    bool bRequestModel = false;
    if (fDistanceX * fDistanceX + fDistanceY * fDistanceY < MAX_BIGBUILDING_STREAMING_RADIUS_SQUARED
        || std::fabs(LimitRadianAngle(fAngleInRadians)) <= BIGBUILDING_STREAMING_ANGLE_THRESHOLD_RAD) {
        bRequestModel = true;
    }

    // TODO(port): iterate CWorld::GetLodPtrList(sectorX, sectorY),
    // SetupBigBuildingVisibility, handle RENDERER_* cases.
    (void)bRequestModel;
}

// 0x554EB0
bool CRenderer::ShouldModelBeStreamed(CEntity* entity, const CVector& point, float farClip) {
    // TODO(port): full implementation from the decomp.
    (void)entity; (void)point; (void)farClip;
    return false;
}

// 0x5555A0 (template, defined in header)
template<typename PtrListType>
void CRenderer::ScanPtrList_RequestModels(PtrListType& list) {
    // TODO(port): for each entity in list, CStreaming::RequestModel if needed.
    (void)list;
}

// 0x5556E0
void CRenderer::ConstructRenderList() {
    // TODO(port): full implementation from the decomp.
    // Calls ScanWorld, ProcessLodRenderLists, etc.
    ms_nNoOfVisibleEntities = 0;
    ms_nNoOfVisibleLods = 0;
    ms_nNoOfVisibleSuperLods = 0;
    ms_nNoOfInVisibleEntities = 0;
    ResetLodRenderLists();
    ScanWorld();
    ProcessLodRenderLists();
}

// 0x555B30
void CRenderer::ScanSectorList_RequestModels(int32_t sectorX, int32_t sectorY) {
    // TODO(port): request models for the sector's lists.
    (void)sectorX; (void)sectorY;
}

// 0x554FE0
// DIVERGENCE from gta-reversed: the camera-moved test is
// |camPos-lastCamPos|^2 >= 16.0f (decomp), NOT
// DotProduct(distance, lastCameraForward) >= 16.0f.
void CRenderer::ScanWorld() {
    // DAT_00b76894 bit-set prologue (purpose unknown).
    if ((s_scanWorldFlags & 1) == 0)
        s_scanWorldFlags |= 1;
    if ((s_scanWorldFlags & 2) == 0)
        s_scanWorldFlags |= 2;

    const float farPlane = TheCamera.m_pRwCamera->farPlane;
    const float width  = TheCamera.m_pRwCamera->viewWindow.x;
    const float height = TheCamera.m_pRwCamera->viewWindow.y;

    CVector frustumPoints[13]{};
    frustumPoints[0] = CVector(0.0f, 0.0f, 0.0f);

    frustumPoints[1].x = frustumPoints[4].x = -(farPlane * width);
    frustumPoints[1].y = frustumPoints[2].y = farPlane * height;
    frustumPoints[2].x = frustumPoints[3].x = farPlane * width;
    frustumPoints[3].y = frustumPoints[4].y = -(farPlane * height);
    frustumPoints[1].z = frustumPoints[2].z = frustumPoints[3].z = frustumPoints[4].z = farPlane;
    // frustumPoints[5..12] zero-initialized.

    m_pFirstPersonVehicle = nullptr;
    CVisibilityPlugins::InitAlphaEntityList();
    CWorld::AdvanceCurrentScanCode();

    // TODO(port): static CVector lastCameraPosition (0xB76888),
    // static CVector lastCameraForward (0xB7687C).
    static CVector lastCameraPosition{};
    static CVector lastCameraForward{};

    const CVector camPos = TheCamera.GetPosition();
    const CVector camForward = TheCamera.m_mCameraMatrix.GetForward();
    const CVector distance = camPos - lastCameraPosition;
    // Decomp: s_bCameraStationary = !(distSq >= 16.0f || dot <= 0.98f).
    const float distSq = distance.x * distance.x + distance.y * distance.y + distance.z * distance.z;
    const float dot = DotProduct(camForward, lastCameraForward);
    s_bCameraStationary = !(distSq >= 16.0f || dot <= 0.98f);

    lastCameraPosition = camPos;
    lastCameraForward = camForward;

    // LOD frustum points (scaled by MAX_LOD_DISTANCE / farPlane).
    // Decomp uses * 0.2f for the /5 divisions.
    frustumPoints[5]  = (frustumPoints[1] * MAX_LOD_DISTANCE) / farPlane;
    frustumPoints[7].x = frustumPoints[5].x * 0.2f;
    frustumPoints[7].y = frustumPoints[5].y * 0.2f;
    frustumPoints[7].z = frustumPoints[5].z;

    frustumPoints[6]  = (frustumPoints[2] * MAX_LOD_DISTANCE) / farPlane;
    frustumPoints[8].x = frustumPoints[6].x * 0.2f;
    frustumPoints[8].y = frustumPoints[6].y * 0.2f;
    frustumPoints[8].z = frustumPoints[6].z;

    frustumPoints[9]  = (frustumPoints[3] * MAX_LOD_DISTANCE) / farPlane;
    frustumPoints[11].x = frustumPoints[9].x * 0.2f;
    frustumPoints[11].y = frustumPoints[9].y * 0.2f;
    frustumPoints[11].z = frustumPoints[9].z;

    frustumPoints[10]  = (frustumPoints[4] * MAX_LOD_DISTANCE) / farPlane;
    frustumPoints[12].x = frustumPoints[10].x * 0.2f;
    frustumPoints[12].y = frustumPoints[10].y * 0.2f;
    frustumPoints[12].z = frustumPoints[10].z;

    RwV3dTransformPoints(frustumPoints, frustumPoints, 13, TheCamera.GetRwMatrix());
    m_loadingPriority = false;

    // TODO(port): CWorld::GetSectorfX/Y, GetLodSectorfX/Y.
    CVector2D points[5]{};
    CWorldScan::ScanWorld(points, 5, ScanSectorList);
    CWorldScan::ScanWorld(points, 5, ScanBigBuildingList);
}

// 0x554C60
int32_t CRenderer::GetObjectsInFrustum(CEntity** outEntities, float farPlane, RwMatrix* transformMatrix) {
    // TODO(port): full implementation from the decomp.
    // Builds frustum points, scans sectors, fills outEntities.
    (void)outEntities; (void)farPlane; (void)transformMatrix;
    return 0;
}

// 0x555960
void CRenderer::RequestObjectsInFrustum(RwMatrix* transformMatrix, int32_t modelRequestFlags) {
    // TODO(port): full implementation from the decomp.
    (void)transformMatrix; (void)modelRequestFlags;
}

// 0x555CB0
// DIVERGENCE from gta-reversed: the decomp IGNORES the modelRequestFlags param
// and hardcodes 0x20.
void CRenderer::RequestObjectsInDirection(const CVector& posn, float angle, int32_t modelRequestFlags) {
    (void)modelRequestFlags; // ignored; decomp hardcodes 0x20
    // TODO(port): full implementation from the decomp.
    (void)posn; (void)angle;
}

// 0x555D60
void CRenderer::SetupScanLists(int32_t sectorX, int32_t sectorY) {
    // TODO(port): fills the tScanLists in PC_Scratch from CWorld sector lists.
    (void)sectorX; (void)sectorY;
}
