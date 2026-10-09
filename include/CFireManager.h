// CFireManager - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/FireManager.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), friend InjectHooksMain,
//   VALIDATE_SIZE (now a guarded static_assert), StaticRef<> (gFireManager is now
//   extern, defined in CFireManager.cpp; original game address kept in comment).
//   Integer types converted to <cstdint> (uint32 -> uint32_t, etc.).
//   _IGNORED_ param annotations dropped (no-op macro in gta-reversed).

#pragma once

#include "CFire.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>

class CEntity;

#define MAX_NUM_FIRES 60

class CFireManager {
public:
    std::array<CFire, MAX_NUM_FIRES> m_aFires;
    uint32_t m_nMaxFireGenerationsAllowed;

public:
    CFireManager();
    ~CFireManager() = default; // 0x538BB0

    void Init();
    void Shutdown();

    uint32_t GetNumOfNonScriptFires();
    CFire* FindNearestFire(const CVector& point, bool bCheckWasExtinguished = false, bool bCheckWasCreatedByScript = false);

    /*!
     * @return True when there are enough free or reusable fire slots available for allocation (i.e. according to a similar slot-availability logic used by `GetNextFreeFire`).
     */
    bool PlentyFiresAvailable();

    void ExtinguishPoint(CVector point, float fRadiusSq);
    bool ExtinguishPointWithWater(CVector point, float fRadiusSq, float fFireSize);

    bool IsScriptFireExtinguished(int16_t id);

    void RemoveScriptFire(uint16_t fireId);
    void RemoveAllScriptFires();
    void ClearAllScriptFireFlags();

    void SetScriptFireAudio(int16_t fireId, bool bFlag);

    const CVector& GetScriptFireCoords(int16_t fireId);
    uint32_t GetNumFiresInRange(const CVector& point, float fRadiusSq);
    uint32_t GetNumFiresInArea(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
    CFire* GetNextFreeFire(bool bMayExtinguish); // bAllowDeletingOldFire - allow deleting old fire if no free slots available

    void CreateAllFxSystems();
    void DestroyAllFxSystems();

    CFire* StartFire(CVector pos, float size, uint8_t unused, CEntity* creator, uint32_t nTimeToBurn, int8_t nGenerations, uint8_t unused_);
    CFire* StartFire(CEntity* target, CEntity* creator, float size = 0.8f, uint8_t arg3 = 1, uint32_t time = 7000, int8_t numGenerations = 0);
    int32_t StartScriptFire(const CVector& point, CEntity* target, float arg2, uint8_t arg3, int8_t numGenerations, int32_t size);

    void Update();

    // NOTSA funcs
    uint32_t GetNumOfFires();
    CFire& GetRandomFire();

    // NOTSA
    CFire& Get(size_t idx)
    {
        assert(m_aFires[idx].IsActive());
        return m_aFires[idx];
    }
    auto GetIndexOf(const CFire* fire) const { return std::distance(m_aFires.data(), fire); }
private:
    CFireManager* Constructor();
    CFireManager* Destructor();
};

// Layout check: gta-reversed VALIDATE_SIZE(CFireManager, 0x964), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CFireManager) == 0x964, "CFireManager layout drift");
#endif

extern CFireManager gFireManager; // was StaticRef<CFireManager>(0xB71F80)
