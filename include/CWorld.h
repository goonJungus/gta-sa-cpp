// CWorld - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/World.h
// Static world query API: sector grid, line-of-sight, ground height, explosions,
// area clearing. All state in the original was StaticRef globals pointing at
// fixed binary addresses; in the clean-room build that state lives in CWorld.cpp
// (TODO) instead of in this header.
//
// Adaptations: stripped InjectHooks(), friend InjectHooksMain, all StaticRef
// members, <extensions/utility.hpp> (notsa::mdarray). std::predicate (C++20)
// rewritten as plain template type parameters (project is C++17). Inline bodies
// that touched the static world state are demoted to declarations.

#pragma once

#include "CEntity.h"
#include "CPlayerInfo.h" // CPlayerInfo (Players)
#include "CPhysical.h"
#include "CRect.h"

#include <cstdint>
#include <cassert>
#include <cmath>
#include <functional>
#include <utility>

// ---- Forward declarations (ported in later subsystems) ----
class CPedGroup;
class CPlayerInfo;
class CColPoint;
class CColLine;
struct CStoredCollPoly; // defined in ColTypes.h
class CSector;
class CRepeatSector;
class CPed;
class CVehicle;
class CPlayerPed;
class CWanted;
class CBox;
template<typename T> class CPtrListDoubleLink;
template<typename T> class CPtrListSingleLink;

// NOTSA
enum class eSprayPaintState : int32_t {
    NOT_FOUND,
    DISCOVERED,
    PAINTED
};

// Values verified against decomp src/_types.h
enum eLevelName : int32_t {
    LEVEL_NAME_COUNTRY_SIDE = 0,
    LEVEL_NAME_LOS_SANTOS = 1,
    LEVEL_NAME_SAN_FIERRO = 2,
    LEVEL_NAME_LAS_VENTURAS = 3,
    NUM_LEVELS = 4
};

constexpr int32_t MAX_PLAYERS = 2;
constexpr int32_t MAX_WORLD_UNITS = 6000;

constexpr int32_t MAX_SECTORS_X = 120;
constexpr int32_t MAX_SECTORS_Y = 120;
constexpr int32_t MAX_SECTORS = MAX_SECTORS_X * MAX_SECTORS_Y;

constexpr size_t MAX_REPEAT_SECTORS_X = 16;
constexpr size_t MAX_REPEAT_SECTORS_Y = 16;
constexpr size_t MAX_REPEAT_SECTORS = MAX_REPEAT_SECTORS_X * MAX_REPEAT_SECTORS_Y;

constexpr int32_t MAX_LOD_PTR_LISTS_X = 30;
constexpr int32_t MAX_LOD_PTR_LISTS_Y = 30;
constexpr int32_t MAX_LOD_PTR_LISTS = MAX_LOD_PTR_LISTS_X * MAX_LOD_PTR_LISTS_Y;

constexpr inline float WORLD_BOUND_RANGE = 3000.0f;
constexpr inline CRect WORLD_BOUNDS{ -3000.0F, -3000.0F, 3000.0F, 3000.0F };
constexpr float MAP_Z_LOW_LIMIT = -100.0f;

class CWorld {
public:
    // NOTE: the original header declared ~25 StaticRef globals here
    // (Players, PlayerInFocus, ms_aSectors, ms_aRepeatSectors, ms_aLodPtrLists,
    //  ms_listMovingEntityPtrs, ms_nCurrentScanCode, m_aTempColPts, ...).
    // In the clean-room build all world state is owned by CWorld.cpp;
    // only the API surface is declared here.

    // Line-test options (were StaticRef<> globals at the noted game addresses;
    // owned by CWorld.cpp). Needed by weapons_combat (CBulletInfo::Update).
    static bool     bIncludeDeadPeds; // 0xB7CD71
    static bool     bIncludeCarTyres; // 0xB7CD70
    static bool     bIncludeBikers;   // 0xB7CD6F
    static CEntity* pIgnoreEntity;    // 0xB7CD68
    static float    fWeaponSpreadRate; // 0xC8A7D4 (set by CWeapon::FireInstantHit for shotguns)
    static CColPoint m_aTempColPts[32]; // temp col points for pellet tests etc.
    static bool     bForceProcessControl; // 0xB7CD6E (set by scripts; read by CAutomobile::ProcessControl)
    // TODO(port): decomp CWorld::Players (added 2026-10-09 for CPed); moves to CWorld.cpp with real state
    inline static CPlayerInfo Players[MAX_PLAYERS]{};

    static void ResetLineTestOptions();

    static void Initialise();
    static void ShutDown();
    static void ClearForRestart();

    // TODO: need the static sector arrays (CWorld.cpp owns them)
    static CSector& GetSector(int32_t x, int32_t y);
    static CRepeatSector& GetRepeatSector(int32_t x, int32_t y);
    static CPtrListSingleLink<CEntity*>& GetLodPtrList(int32_t x, int32_t y);

    template<typename PtrListType>
    static PtrListType& GetMovingEntityPtrList(); // TODO: needs world state
    template<typename PtrListType>
    static PtrListType& GetObjectsWithControlCodePtrList(); // TODO: needs world state

    static void AdvanceCurrentScanCode();

    // 0x407250
    static uint16_t GetCurrentScanCode(); // TODO: needs world state

    static void Add(CEntity* entity);
    static void Remove(CEntity* entity);

    static bool GetIsLineOfSightClear(const CVector& origin, const CVector& target, bool buildings, bool vehicles, bool peds, bool objects, bool dummies = false, bool doSeeThroughCheck = false, bool doCameraIgnoreCheck = false);
    static bool ProcessLineOfSight(const CVector& origin, const CVector& target, CColPoint& outColPoint, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doCameraIgnoreCheck, bool doShootThroughCheck);
    static bool ProcessVerticalLine(const CVector& origin, float distance, CColPoint& outColPoint, CEntity*& outEntity, bool buildings = false, bool vehicles = false, bool peds = false, bool objects = false, bool dummies = false, bool doSeeThroughCheck = false, CStoredCollPoly* outCollPoly = nullptr);
    static bool ProcessVerticalLine_FillGlobeColPoints(const CVector& origin, float distance, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly);
    // static float GetLightingAtPoint(const CVector&, float); // unknown

    static void TriggerExplosion(const CVector& point, float radius, float visibleDistance, CEntity* victim, CEntity* creator, bool processVehicleBombTimer, float damage);
    static void CastShadow(float x1, float y1, float x2, float y2);

    static void ProcessForAnimViewer();
    static void Process();
    // static void Render(); // unknown

    static void FindObjectsInRange(const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies);
    template<typename PtrListType>
    static void FindObjectsInRangeSectorList(PtrListType& arg0, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities);

    static void FindObjectsOfTypeInRange(uint32_t modelId, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies);
    static void FindLodOfTypeInRange(uint32_t modelId, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities);
    template<typename PtrListType>
    static void FindObjectsOfTypeInRangeSectorList(uint32_t modelId, PtrListType& ptrList, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities);

    /*!
    * Find object that are "kinda" colliding at `point`
    *
    * @param point             The point to scan at
    * @param radius            The radius of the scan
    * @param b2D               Whenever the distance checks should be 2D (if false they're 3D)
    * @param outCount          The number of entities colliding (Never more than `maxCount`)
    * @param maxCount          The maximum number of entities to scan for
    * @param outEntities [opt] Enitites that are colliding are stored here, array should be the same size as `maxCount`
    * @param buildings         Check buildings?
    * @param vehicles          Check vehicles?
    * @param peds              Check peds?
    * @param objects           Check objects?
    * @param dummies           Check dummies?
    */
    static void FindObjectsKindaColliding(const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies);
    template<typename PtrListType>
    static void FindObjectsKindaCollidingSectorList(PtrListType& ptrList, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities);

    static void FindObjectsIntersectingCube(const CVector& cornerA, const CVector& cornerB, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies);
    template<typename PtrListType>
    static void FindObjectsIntersectingCubeSectorList(PtrListType& ptrList, const CVector& cornerA, const CVector& cornerB, int16_t* outCount, int16_t maxCount, CEntity** outEntities);

    static void FindObjectsIntersectingAngledCollisionBox(const CBox& box, const CMatrix& transform, const CVector& point, float x1, float y1, float x2, float y2, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies);
    template<typename PtrListType>
    static void FindObjectsIntersectingAngledCollisionBoxSectorList(PtrListType& ptrList, const CBox& box, const CMatrix& transform, const CVector& point, int16_t* outCount, int16_t maxCount, CEntity** outEntities);

    static void FindMissionEntitiesIntersectingCube(const CVector& cornerA, const CVector& cornerB, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool vehicles, bool peds, bool objects);
    template<typename PtrListType>
    static void FindMissionEntitiesIntersectingCubeSectorList(PtrListType& ptrList, const CVector& cornerA, const CVector& cornerB, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool vehiclesList, bool pedsList, bool objectsList);

    static CEntity* FindNearestObjectOfType(int32_t modelId, const CVector& point, float radius, bool b2D, bool buildings, bool vehicles, bool peds, bool objects, bool dummies);
    template<typename PtrListType>
    static void FindNearestObjectOfTypeSectorList(int32_t modelId, PtrListType& ptrList, const CVector& point, float radius, bool b2D, CEntity*& outEntity, float& outDistance);

    static float FindGroundZForCoord(float x, float y);

    static float FindGroundZFor3DCoord(CVector coord, bool* outResult = nullptr, CEntity** outEntity = nullptr);

    static float FindRoofZFor3DCoord(float x, float y, float z, bool* outResult);

    static float FindLowestZForCoord(float x, float y);

    static void CheckBlockListIntegrity();

    static void RemoveReferencesToDeletedObject(CEntity* entity);

    static void SetPedsOnFire(float x, float y, float z, float radius, CEntity* fireCreator);

    static void SetPedsChoking(float x, float y, float z, float radius, CEntity* gasCreator);

    static void SetCarsOnFire(CVector pos, float radius, CEntity* fireCreator);

    static void SetWorldOnFire(CVector pos, float radius, CEntity* fireCreator);

    static eSprayPaintState SprayPaintWorld(CVector& posn, CVector& outDir, float radius, bool processTagAlphaState);

    static void CheckBuildingOrientations(); // in III

    static void RemoveFallenPeds();

    static void RemoveFallenCars();

    static void RepositionCertainDynamicObjects();

    static void RepositionOneObject(CEntity* object);

    static void UseDetonator(CPed* creator);

    static void PrintCarChanges();

    static void RemoveStaticObjects();

    static CEntity* TestSphereAgainstWorld(CVector sphereCenter, float sphereRadius, CEntity* ignoreEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doCameraIgnoreCheck);
    template<typename PtrListType>
    static CEntity* TestSphereAgainstSectorList(PtrListType& ptrList, CVector sphereCenter, float sphereRadius, CEntity* ignoreEntity, bool doCameraIgnoreCheck);

    static void TestForBuildingsOnTopOfEachOther();
    template<typename PtrListType>
    static void TestForBuildingsOnTopOfEachOther(PtrListType& ptrList);
    static void TestForUnusedModels();
    template<typename PtrListType>
    static void TestForUnusedModels(PtrListType& ptrList, int32_t* models);

    static void RemoveEntityInsteadOfProcessingIt(CEntity* entity);

    static void ClearExcitingStuffFromArea(const CVector& point, float radius, uint8_t bRemoveProjectilesAndShadows);

    static void ClearCarsFromArea(float x1, float y1, float z1, float x2, float y2, float z2);

    static void ClearPedsFromArea(float x1, float y1, float z1, float x2, float y2, float z2);

    static void SetAllCarsCanBeDamaged(bool enable);

    static void ExtinguishAllCarFiresInArea(CVector point, float radius);

    static void CallOffChaseForArea(float x1, float y1, float x2, float y2);
    static void CallOffChaseForAreaSectorListVehicles(CPtrListDoubleLink<CVehicle*>& ptrList, float x1, float y1, float x2, float y2, float minX, float minY, float maxX, float maxY);
    static void CallOffChaseForAreaSectorListPeds(CPtrListDoubleLink<CPed*>& list, float minX, float minY, float maxX, float maxY, float biggerMinX, float biggerMinY, float biggerMaxX, float biggerMaxY);

    static void HandleCollisionZoneChange(eLevelName oldZone, eLevelName newZone);
    // static void FindZoneRespawnPoint(eLevelName oldZone, eLevelName newZone, CVector* out); // in III, dont SA
    static void DoZoneTestForChaser(CPhysical* physical);

    static void StopAllLawEnforcersInTheirTracks();

    static int32_t FindPlayerSlotWithPedPointer(void* ptr);
    static int32_t FindPlayerSlotWithRemoteVehiclePointer(void* ptr);
    static int32_t FindPlayerSlotWithVehiclePointer(CEntity* vehiclePtr);

    // static bool IsPointInWorld(float x, float y); // unknown

    static void ProcessAttachedEntities();
    static void ProcessPedsAfterPreRender();

    static CVehicle* FindUnsuspectingTargetCar(CVector point, CVector playerPosn);

    static CPed* FindUnsuspectingTargetPed(CVector point, CVector playerPosn);

protected:
    static void ClearScanCodes();

    static bool GetIsLineOfSightSectorClear(CSector& sector, CRepeatSector& repeatSector, const CColLine& colLine, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doIgnoreCameraCheck);
    static bool ProcessLineOfSightSector(CSector& sector, CRepeatSector& repeatSector, const CColLine& colLine, CColPoint& outColPoint, float& maxTouchDistance, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doCameraIgnoreCheck, bool doShootThroughCheck);
    static bool ProcessVerticalLineSector(CSector& sector, CRepeatSector& repeatSector, const CColLine& colLine, CColPoint& outColPoint, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly);
    static bool ProcessVerticalLineSector_FillGlobeColPoints(CSector& sector, CRepeatSector& repeatSector, const CColLine& colLine, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly);

    template<typename PtrListType>
    static bool GetIsLineOfSightSectorListClear(PtrListType& ptrList, const CColLine& colLine, bool doSeeThroughCheck, bool doCameraIgnoreCheck);
    template<typename PtrListType>
    static bool ProcessLineOfSightSectorList(PtrListType& ptrList, const CColLine& colLine, CColPoint& outColPoint, float& minTouchDistance, CEntity*& outEntity, bool doSeeThroughCheck, bool doIgnoreCameraCheck, bool doShootThroughCheck);
    template<typename PtrListType>
    static bool ProcessVerticalLineSectorList(PtrListType& ptrList, const CColLine& colLine, CColPoint& colPoint, float& maxTouchDistance, CEntity*& outEntity, bool doSeeThroughCheck, CStoredCollPoly* collPoly);
    template<typename PtrListType>
    static bool ProcessVerticalLineSectorList_FillGlobeColPoints(PtrListType& ptrList, const CColLine& colLine, CEntity*& outEntity, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly);

    template<typename PtrListType>
    static void TriggerExplosionSectorList(PtrListType& ptrList, const CVector& point, float radius, float visibleDistance, CEntity* victim, CEntity* creator, bool processVehicleBombTimer, float damage);
    template<typename PtrListType>
    static void CastShadowSectorList(PtrListType& ptrList, float xmin, float ymin, float xmax, float ymax);

public:
    static bool CameraToIgnoreThisObject(CEntity* entity);

public:
    // Returns sector index in range -60 to 60 (Example: -3000 => -60, 3000 => 60)
    static float GetHalfMapSectorX(float x) { return x / static_cast<float>(MAX_WORLD_UNITS / MAX_SECTORS_X); }
    static float GetHalfMapSectorY(float y) { return y / static_cast<float>(MAX_WORLD_UNITS / MAX_SECTORS_Y); }

    // Returns sector index in range 0 to 120 (Example: -3000 => 0, 3000 => 120)
    static float GetSectorfX(float x) { return GetHalfMapSectorX(x) + static_cast<float>(MAX_SECTORS_X / 2); }
    static float GetSectorfY(float y) { return GetHalfMapSectorY(y) + static_cast<float>(MAX_SECTORS_Y / 2); }

    // returns sector index in range 0 to 120 (covers full map)
    static int32_t GetSectorX(float x) { return static_cast<int32_t>(std::floor(GetSectorfX(x))); }
    static int32_t GetSectorY(float y) { return static_cast<int32_t>(std::floor(GetSectorfY(y))); }

    static float GetSectorPosX(int32_t sector) {
        constexpr auto HalfOfTotalSectorsX = MAX_SECTORS_X / 2;
        constexpr auto fTotalMapUnitsX = MAX_WORLD_UNITS / MAX_SECTORS_X;
        return static_cast<float>((sector - HalfOfTotalSectorsX) * fTotalMapUnitsX + (fTotalMapUnitsX / 2));
    }

    static float GetSectorPosY(int32_t sector) {
        constexpr auto HalfOfTotalSectorsY = MAX_SECTORS_Y / 2;
        constexpr auto fTotalMapUnitsY = MAX_WORLD_UNITS / MAX_SECTORS_Y;
        return static_cast<float>((sector - HalfOfTotalSectorsY) * fTotalMapUnitsY + (fTotalMapUnitsY / 2));
    }

    static CVector2D GetSectorPos(int32_t sector) { return { GetSectorPosX(sector), GetSectorPosY(sector) }; }

    // returns sector index in range 0 to 15 (covers half of the map)
    static float GetHalfMapLodSectorX(float sector) { return sector / static_cast<float>(MAX_WORLD_UNITS / MAX_LOD_PTR_LISTS_X); }
    static float GetHalfMapLodSectorY(float sector) { return sector / static_cast<float>(MAX_WORLD_UNITS / MAX_LOD_PTR_LISTS_Y); }
    static float GetLodSectorfX(float sector) { return GetHalfMapLodSectorX(sector) + static_cast<float>(MAX_LOD_PTR_LISTS_X / 2); }
    static float GetLodSectorfY(float sector) { return GetHalfMapLodSectorY(sector) + static_cast<float>(MAX_LOD_PTR_LISTS_Y / 2); }
    // returns sector index in range 0 to 30 (covers full map)
    static int32_t GetLodSectorX(float fSector) { return static_cast<int32_t>(std::floor(GetLodSectorfX(fSector))); }
    static int32_t GetLodSectorY(float fSector) { return static_cast<int32_t>(std::floor(GetLodSectorfY(fSector))); }
    static float GetLodSectorPosX(int32_t sector) {
        const int32_t HalfOfTotalSectorsX = MAX_LOD_PTR_LISTS_X / 2;
        const float fTotalMapUnitsX = static_cast<float>(MAX_WORLD_UNITS / MAX_LOD_PTR_LISTS_X);
        return (sector - HalfOfTotalSectorsX) * fTotalMapUnitsX + (fTotalMapUnitsX / 2);
    }
    static float GetLodSectorPosY(int32_t sector) {
        const int32_t HalfOfTotalSectorsY = MAX_LOD_PTR_LISTS_Y / 2;
        const float fTotalMapUnitsY = static_cast<float>(MAX_WORLD_UNITS / MAX_LOD_PTR_LISTS_Y);
        return (sector - HalfOfTotalSectorsY) * fTotalMapUnitsY + (fTotalMapUnitsY / 2);
    }
    static bool IsInWorldBounds(CVector2D pos) {
        return pos.x > -3000.0f && pos.x < 3000.0f
            && pos.y > -3000.0f && pos.y < 3000.0f;
    }

    static void RemoveVehicleAndItsOccupants(CVehicle* veh);

    /*!
    * @brief Call `fn` with the `x, y` grid position of all sectors between the specified grid positions
    *
    * @return `fn` may return `false` to stop the iteration in which case
    *         this function also returns `false`. If no area was iterated, or the `fn` returned
    *         `true` for all invocations `true` is returned.
    */
    // NOTE: original used std::predicate (C++20); plain template parameter for C++17.
    template<typename Fn>
    static bool IterateSectors(int32_t minX, int32_t minY, int32_t maxX, int32_t maxY, Fn&& fn) {
        assert(maxX >= minX && maxY >= minY);

        for (auto y = minY; y <= maxY; ++y) {
            for (auto x = minX; x <= maxX; ++x) {
                if (!std::invoke(fn, x, y)) {
                    return false;
                }
            }
        }
        return true;
    }

    /*
    * @brief Call `fn` with the `x, y` grid position of all sectors that are overlapped by the rect
    *
    * @param rect The rect. Use it's constructor to ease your life (for example iterating areas in a given radius can be achieved by `CRect{point, 340.f}`)
    *
    * @copyreturn `IterateSectors`
    */
    template<typename Fn>
    static bool IterateSectorsOverlappedByRect(CRect rect, Fn&& fn) {
        return IterateSectors(
            GetSectorX(rect.left),
            GetSectorY(rect.bottom),
            GetSectorX(rect.right),
            GetSectorY(rect.top),
            std::forward<Fn>(fn)
        );
    }

    /*
    * @brief Call `fn` with the `x, y` grid position of all lod sectors that are overlapped by the rect
    *
    * @param rect The rect. Use it's constructor to ease your life (for example iterating areas in a given radius can be achieved by `CRect{point, 340.f}`)
    *
    * @copyreturn `IterateSectors`
    */
    template<typename Fn>
    static bool IterateLodSectorsOverlappedByRect(CRect rect, Fn&& fn) {
        return IterateSectors(
            GetLodSectorX(rect.left),
            GetLodSectorY(rect.bottom),
            GetLodSectorX(rect.right),
            GetLodSectorY(rect.top),
            std::forward<Fn>(fn)
        );
    }

    static void PutToGroundIfTooLow(CVector& pos) {
        if (pos.z <= MAP_Z_LOW_LIMIT) {
            pos.z = CWorld::FindGroundZForCoord(pos.x, pos.y);
        }
    }

    static CVector AddGroundZToCoord(CVector2D xy) {
        return CVector{ xy, FindGroundZForCoord(xy.x, xy.y) };
    }
};

CPlayerInfo&   FindPlayerInfo(int32_t playerId = -1);
CPlayerPed*    FindPlayerPed(int32_t playerId = -1);
CVehicle*      FindPlayerVehicle(int32_t playerId = -1, bool bIncludeRemote = false);
CVector        FindPlayerCoors(int32_t playerId = -1);
CVector&       FindPlayerSpeed(int32_t playerId = -1);
CEntity*       FindPlayerEntity(int32_t playerId = -1);
CTrain*        FindPlayerTrain(int32_t playerId = -1);
const CVector& FindPlayerCentreOfWorld(int32_t playerId = -1);
const CVector& FindPlayerCentreOfWorld_NoSniperShift(int32_t playerId = -1);
CVector        FindPlayerCentreOfWorldForMap(int32_t playerId = -1);
float          FindPlayerHeading(int32_t playerId = -1);
float          FindPlayerHeight(int32_t playerId = -1);
CWanted*       FindPlayerWanted(int32_t playerId = -1);
CPedGroup&     FindPlayerGroup(int32_t playerId = -1);
