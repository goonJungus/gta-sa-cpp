// CIplStore - adapted from gta-reversed for clean-room C++ build
// Decompiled bodies: src/CIplStore/*.c
// TODO: verify each method against decomp.

#pragma once

#include "IplDef.h"
#include "QuadTreeNode.h"
#include "CPool.h"

#include <array>
#include <cstdint>

class CEntity;

using CIplPool = CPool<IplDef>;

class CIplStore {
public:
    static void Initialise();
    static void Shutdown();

    static int32 AddIplSlot(const char* name);
    static void AddIplsNeededAtPosn(const CVector& posn);
    static void ClearIplsNeededAtPosn();
    static void EnableDynamicStreaming(int32 iplSlotIndex, bool enable);
    static void EnsureIplsAreInMemory(const CVector& posn);
    static int32 FindIplSlot(const char* name);
    static CRect* GetBoundingBox(int32 iplSlotIndex);
    static CEntity** GetIplEntityIndexArray(int32 arrayIndex);
    static const char* GetIplName(int32 iplSlotIndex);
    static int32 GetNewIplEntityIndexArray(int32 entitiesCount);
    static bool HaveIplsLoaded(const CVector& coords, int32 playerNumber = -1);
    static void IncludeEntity(int32 iplSlotIndex, CEntity* entity);
    static void LoadAllRemainingIpls();
    static bool LoadIpl(int32 iplSlotIndex, char* data, int32 dataSize);
    static bool LoadIplBoundingBox(int32 iplSlotIndex, char* data, int32 dataSize);
    static void LoadIpls(CVector posn, bool bAvoidLoadInPlayerVehicleMovingDirection);
    static void RemoveAllIpls();
    static void RemoveIpl(int32 iplSlotIndex);
    static void RemoveIplAndIgnore(int32 iplSlotIndex);
    static void RemoveIplSlot(int32 iplSlotIndex);
    static void RemoveIplWhenFarAway(int32 iplSlotIndex);
    static void RemoveRelatedIpls(int32 entityArraysIndex);
    static void RequestIplAndIgnore(int32 iplSlotIndex);
    static void RequestIpls(const CVector& posn, int32 playerNumber = -1);
    static void SetIplsRequired(const CVector& posn, int32 playerNumber = -1);
    static void SetIsInterior(int32 iplSlotIndex, bool isInterior);
    static int32 SetupRelatedIpls(const char* iplName, int32 entityArraysIndex, CEntity** instances);
    static bool Save();
    static bool Load();

    inline static bool HasDynamicStreamingDisabled(int32 iplSlotIndex) { return GetInSlot(iplSlotIndex)->disableDynamicStreaming; } // 0x59EB20

    static IplDef* GetInSlot(int32 slot);
    static CIplPool* GetPool();
};

// Clean-room: the originals were StaticRef-bound to fixed game addresses;
// converted to plain externs. Original addresses noted per variable.
// Definitions in CIplStore.cpp.
extern CEntity** ppCurrIplInstance;            // was StaticRef<CEntity**>(0x8E3EFC)
extern int32 NumIplEntityIndexArrays;          // was StaticRef<int32>(0x8E3F00)
extern std::array<CEntity**, 40> IplEntityIndexArrays; // was StaticRef<std::array<CEntity**, 40>>(0x8E3F08)
extern bool gbIplsNeededAtPosn;                // was StaticRef<bool>(0x8E3FA8)
extern CVector gvecIplsNeededAtPosn;           // was StaticRef<CVector>(0x8E3FD0)
extern uint32 gNumLoadedBuildings;             // was StaticRef<uint32>(0xBCC0D8)
extern std::array<CEntity*, 4096> gpLoadedBuildings; // was StaticRef<std::array<CEntity*, 4096>>(0xBCC0E0)

// NOTE: the original also had GetLoadedBuildings() using rng::views::take -
// plugin-sdk range utility removed in the clean-room build; not ported.
