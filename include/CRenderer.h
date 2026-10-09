// CRenderer - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Renderer.h
// Decompiled bodies: src/CRenderer/*.c
// TODO: verify each method against decomp.

#pragma once

#include "CVector.h" // CVector, CVector2D, RwV3d
#include "CMatrix.h" // RwMatrix (RwMatrix* params in frustum functions)

#include <array>
#include <cstdint>
#include <functional> // std::invoke (tScanLists::VisitLists)

class CEntity;
class CVehicle;
class CPhysical;
class CPed;
class CDummy;
class CBuilding;
class CObject;
class CBaseModelInfo;

// TODO: port Core/PtrList.h (CPtrListSingleLink, CPtrListDoubleLink) with the
// core-containers subsystem; forward-declared here only.
template<typename T> class CPtrListSingleLink;
template<typename T> class CPtrListDoubleLink;

enum eRendererVisibility {
    RENDERER_INVISIBLE = 0,
    RENDERER_VISIBLE,
    RENDERER_CULLED,
    RENDERER_STREAMME
};

struct tScanLists {
    CPtrListSingleLink<CBuilding*>* buildingsList;
    CPtrListDoubleLink<CObject*>*   objectsList;
    CPtrListDoubleLink<CVehicle*>*  vehiclesList;
    CPtrListDoubleLink<CPed*>*      pedsList;
    CPtrListDoubleLink<CDummy*>*    dummiesList;

    template<typename Visitor>
    void VisitLists(Visitor&& visitor) {
        if (buildingsList) {
            std::invoke(visitor, *buildingsList);
        }
        if (objectsList) {
            std::invoke(visitor, *objectsList);
        }
        if (vehiclesList) {
            std::invoke(visitor, *vehiclesList);
        }
        if (pedsList) {
            std::invoke(visitor, *pedsList);
        }
        if (dummiesList) {
            std::invoke(visitor, *dummiesList);
        }
    }
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tScanLists) == 0x14, "tScanLists layout changed");
#endif

struct tRenderListEntry {
    CEntity* entity;
    float distance;
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tRenderListEntry) == 8, "tRenderListEntry layout changed");
#endif

constexpr auto MAX_INVISIBLE_ENTITY_PTRS   = 150u;
constexpr auto MAX_VISIBLE_ENTITY_PTRS     = 1000u;
constexpr auto MAX_VISIBLE_LOD_PTRS        = 1000u;
constexpr auto MAX_VISIBLE_SUPERLOD_PTRS   = 50u;

class CWorldScan {
public:
    // __cdecl dropped: on the Win32 target the default calling convention for a
    // function pointer IS __cdecl, so this is semantically identical.
    using tScanFunction = void(*)(int32_t, int32_t);
    static void ScanWorld(CVector2D* points, int32_t pointsCount, tScanFunction scanFunction);
    static void SetExtraRectangleToScan(float minX, float maxX, float minY, float maxY);
};

class CRenderer {
public:
    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments. Definitions in CRenderer.cpp.
    // TODO: re-resolve for the clean-room build.
    static bool          ms_bRenderTunnels;                                  // 0xB745C0
    static bool          ms_bRenderOutsideTunnels;                           // 0xB745C1
    static tRenderListEntry* ms_pLodDontRenderList;                          // 0xB745CC
    static tRenderListEntry* ms_pLodRenderList;                              // 0xB745D0
    static CVehicle*     m_pFirstPersonVehicle;                              // 0xB745D4
    static CEntity*      ms_aInVisibleEntityPtrs[MAX_INVISIBLE_ENTITY_PTRS]; // 0xB745D8
    static CEntity*      ms_aVisibleSuperLodPtrs[MAX_VISIBLE_SUPERLOD_PTRS]; // 0xB74830
    static CEntity*      ms_aVisibleLodPtrs[MAX_VISIBLE_LOD_PTRS];            // 0xB748F8
    static CEntity*      ms_aVisibleEntityPtrs[MAX_VISIBLE_ENTITY_PTRS];     // 0xB75898
    static int32_t       ms_nNoOfVisibleSuperLods;                          // 0xB76838
    static int32_t       ms_nNoOfInVisibleEntities;                          // 0xB7683C
    static int32_t       ms_nNoOfVisibleLods;                               // 0xB76840
    static int32_t       ms_nNoOfVisibleEntities;                            // 0xB76844
    static float         ms_fFarClipPlane;                                   // 0xB76848
    static float         ms_fCameraHeading;                                  // 0xB7684C
    static bool          m_loadingPriority;                                  // 0xB76850
    static bool          ms_bInTheSky;                                       // 0xB76851
    static CVector       ms_vecCameraPosition;                               // 0xB76870
    static float         ms_lodDistScale;                                    // 0x8CD800 ; = 1.2f
    static float         ms_lowLodDistScale;                                 // 0x8CD804 ; = 1.0f

public:
    static void Init();
    static void Shutdown();
    static void RenderFadingInEntities();
    static void RenderFadingInUnderwaterEntities();
    static void RenderOneRoad(CEntity* entity);
    static void RenderOneNonRoad(CEntity* entity);
    static void RemoveVehiclePedLights(CPhysical* entity);
    static void AddEntityToRenderList(CEntity* entity, float distance);
    static tRenderListEntry* GetLodRenderListBase();
    static tRenderListEntry* GetLodDontRenderListBase();
    static void ResetLodRenderLists();
    static void AddToLodRenderList(CEntity* entity, float distance);
    static void AddToLodDontRenderList(CEntity* entity, float distance);
    static void ProcessLodRenderLists();
    static void PreRender();
    static void RenderRoads();
    static void RenderEverythingBarRoads();
    static void RenderFirstPersonVehicle();
    static bool SetupLightingForEntity(CPhysical* entity);
    static int32_t SetupMapEntityVisibility(CEntity* entity, CBaseModelInfo* modelInfo, float distance, bool bIsTimeInRange);
    static int32_t SetupEntityVisibility(CEntity* entity, float& outDistance);
    static int32_t SetupBigBuildingVisibility(CEntity* entity, float& outDistance);
    static void ScanSectorList_ListModels(int32_t sectorX, int32_t sectorY);
    static void ScanSectorList_ListModelsVisible(int32_t sectorX, int32_t sectorY);
    static void ScanSectorList(int32_t sectorX, int32_t sectorY);
    static void ScanBigBuildingList(int32_t sectorX, int32_t sectorY);
    static bool ShouldModelBeStreamed(CEntity* entity, const CVector& origin, float farClip);
    template<typename PtrListType>
    static void ScanPtrList_RequestModels(PtrListType& list);
    static void ConstructRenderList();
    static void ScanSectorList_RequestModels(int32_t sectorX, int32_t sectorY);
    static void ScanWorld();
    static int32_t GetObjectsInFrustum(CEntity** outEntities, float farPlane, RwMatrix* transformMatrix);
    static void RequestObjectsInFrustum(RwMatrix* transformMatrix, int32_t modelRequestFlags);
    static void RequestObjectsInDirection(const CVector& posn, float angle, int32_t modelRequestFlags);
    static void SetupScanLists(int32_t sectorX, int32_t sectorY);

    static void SetLoadingPriority(int8_t priority) noexcept { m_loadingPriority = priority; } // 0x407370

    // C++17: gta-reversed returned std::span (C++20); replaced with pointer
    // accessors (same convention as the collision subsystem). Counts live in
    // ms_nNoOfVisibleLods / ms_nNoOfVisibleEntities / ms_nNoOfVisibleSuperLods /
    // ms_nNoOfInVisibleEntities.
    static CEntity** GetVisibleLodPtrs()       { return ms_aVisibleLodPtrs; }
    static CEntity** GetVisibleEntityPtrs()    { return ms_aVisibleEntityPtrs; }
    static CEntity** GetVisibleSuperLodPtrs()  { return ms_aVisibleSuperLodPtrs; }
    static CEntity** GetInVisibleEntityPtrs()  { return ms_aInVisibleEntityPtrs; }
};

// Clean-room: these were plugin-sdk address-bound globals in Renderer.cpp;
// converted to plain externs. Definitions in CRenderer.cpp.
// TODO: find the original addresses from the decomp.
extern uint32_t gnRendererModelRequestFlags;
extern CEntity** gpOutEntitiesForGetObjectsInFrustum;
