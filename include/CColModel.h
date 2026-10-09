// CColModel - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Collision/ColModel.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.

#pragma once

#include "CVector.h"
#include "ColTypes.h" // CBoundingBox, CSphere (minimal stand-ins)
#include "CColSphere.h"
#include "CCollisionData.h"

#include <cstddef>
#include <cstdint>

class CColModel {
public:
    CBoundingBox m_boundBox;
    CSphere      m_boundSphere;
    uint8_t      m_nColSlot;
    union {
        struct {
            uint8_t m_bHasCollisionVolumes : 1; // AKA `m_bNotEmpty`
            uint8_t m_bIsSingleColDataAlloc : 1;
            uint8_t m_IsActive : 1;
        };
        uint8_t m_nFlags;
    };
    CCollisionData* m_pColData;

public:
    // InjectHooks() stripped - plugin-sdk hooking mechanism, not needed for clean-room

    CColModel();
    ~CColModel();

    static void* operator new(size_t size);
    static void operator delete(void* data);
    CColModel& operator=(const CColModel& colModel);

public:
    void AllocateData();
    void AllocateData(int32_t numSpheres, int32_t numBoxes, int32_t numLines, int32_t numVertices, int32_t numTriangles, bool bUsesDisks);
    void MakeMultipleAlloc();
    void RemoveCollisionVolumes();
    void CalculateTrianglePlanes();
    void RemoveTrianglePlanes();

private:
    void AllocateData(int32_t size);

public:
    // HELPERS
    [[nodiscard]] auto GetTriCount() const noexcept     { return m_pColData ? m_pColData->m_nNumTriangles : 0u; }
    [[nodiscard]] float GetBoundRadius() const noexcept { return m_boundSphere.m_fRadius; }
    auto& GetBoundCenter() { return m_boundSphere.m_vecCenter; }
    auto& GetBoundingBox() { return m_boundBox; }
    auto& GetBoundingSphere() { return m_boundSphere; }
    CCollisionData* GetData() const { return m_pColData; }
};

// Layout check: gta-reversed VALIDATE_SIZE(CColModel, 0x30), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CColModel) == 0x30, "CColModel layout drift");
#endif
