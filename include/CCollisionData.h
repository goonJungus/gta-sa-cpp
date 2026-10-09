// CCollisionData - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Collision/CollisionData.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.

#pragma once

#include "CVector.h"
#include "ColTypes.h" // tColLighting, FixedFloat/FixedVector, CompressedVector, CLink (minimal stand-ins)

#include <array>
#include <cstdint>
#include <span>

class CColSphere;
class CColBox;
class CColLine;
class CColDisk;
class CColTriangle;
class CColTrianglePlane;

template <typename T> class CLink;

namespace ColHelpers {
struct TFaceGroup;
};

//
// https://gtamods.com/wiki/Collision_File
//

class CCollisionData {
public:
    CCollisionData();

public:
    uint16_t m_nNumSpheres;
    uint16_t m_nNumBoxes;
    uint16_t m_nNumTriangles;
    uint8_t  m_nNumLines;

    struct {
        uint8_t bUsesDisks : 1;     // 0x1 - Always set to false
        uint8_t bHasFaceGroups : 1; // 0x2 - See the huge comment below
        uint8_t bHasShadowInfo : 1; // 0x4 - See wiki.
    };

    CColSphere* m_pSpheres;
    CColBox* m_pBoxes;

    union {
        CColLine* m_pLines;
        CColDisk* m_pDisks;
    };

    CompressedVector* m_pVertices;

    // If you take a look here: https://gtamods.com/wiki/Collision_File#Body
    // You may notice there's an extra section called `TFaceGroups` before `TFace` (triangles)
    // And it is used in `CCollision::ProcessColModels`.
    //
    // The following is only true if `bHasFaceGroups` flag is set (Which is the case only when loaded by `CFileLoader::LoadCollisionModelVer2/3/4`). **
    // All the data is basically stored in a big buffer, and all the data pointers - except `m_pTrianglePlanes` - point into it.
    // In case the flag `bHasFaceGroups` is set there's an array of `TFaceGroup` before the triangles, but there's no pointer to it.
    // In order to access it you have to do some black magic with `pTriangles`. Here's the memory layout of the `TFaceGroups` data:
    //
    // FaceGroup[]          - -0x8 - And growing downwards by `sizeof(FaceGroup)` (Which is 28)
    // uint32 nFaceGroups;  - -0x4
    // <Triangles>          - FaceGroup data is before the triangles!
    //
    // Whenever accessing this section make sure both `CColModel::bSingleAlloc` and `bHasFaceGroups` is set
    // (also, please, assert if `bHasFaceGroups` is set but `bSingleAlloc` isnt)
    //
    // NOTEs:
    // ** Col models may also be loaded by the Collision plugin from a clump file - In this case `CFileLoader::LoadCollisionModelVer2/3` is called, but then
    //    the col data is reallocated using `MakeMultipleAlloc` which uses `Copy` to copy the data, in this case the face groups aren't copied. (And the flag is set to false in the ctor)
    CColTriangle*      m_pTriangles;          // 0x18
    CColTrianglePlane* m_pTrianglePlanes;     // 0x1C
    uint32_t           m_nNumShadowTriangles; // 0x20
    uint32_t           m_nNumShadowVertices;  // 0x24
    CompressedVector*  m_pShadowVertices;     // 0x28
    CColTriangle*      m_pShadowTriangles;    // 0x2C

    // <size 0x30>

public:
    // InjectHooks() stripped - plugin-sdk hooking mechanism, not needed for clean-room

    void RemoveCollisionVolumes();
    void Copy(const CCollisionData& src);
    void CalculateTrianglePlanes();
    void RemoveTrianglePlanes();
    void GetTrianglePoint(CVector& outVec, int32_t vertId);
    void GetShadTrianglePoint(CVector& outVec, int32_t vertId);
    void SetLinkPtr(CLink<CCollisionData*>* link);
    CLink<CCollisionData*>* GetLinkPtr();

    // NOTSA section
    uint32_t GetNumFaceGroups() const;

    // NOTSA helpers - gta-reversed returns std::span (C++20); restored 2026-10-09
    // for CAutomobile (was pointer+count for the C++17 era).
    auto GetSpheres() const { return std::span<CColSphere>(m_pSpheres, static_cast<size_t>(m_nNumSpheres)); }
    uint16_t GetNumSpheres() const { return m_nNumSpheres; }

    const CColBox* GetBoxes() const { return m_pBoxes; }
    uint16_t GetNumBoxes() const { return m_nNumBoxes; }

    uint16_t GetNumTris() const { return m_nNumTriangles; }
    const CColTriangle* GetTris() const { return m_pTriangles; }

    const CompressedVector* GetTriVerts() const { return m_pVertices; } // Sadly there's no easy way to provide a span here - we don't know the number of vertices, and finding it is expensive
    const CColTrianglePlane* GetTriPlanes() const { return m_pTrianglePlanes; }

    const CColTriangle* GetShdwTris() const { return m_pShadowTriangles; }
    const CompressedVector* GetShdwTriVerts() const { return m_pShadowVertices; }

    const CColLine* GetLines() const { return m_pLines; }

    const ColHelpers::TFaceGroup* GetFaceGroups() const;

    [[nodiscard]] auto GetTriVertices(const CColTriangle& tri) const -> std::array<CVector, 3>;

    void AllocateLines(uint32_t num);

    void SetSpheres(const CColSphere* spheres);

private:
    // HELPERS
    template <typename T> T* GetPointerToColArray(size_t byteOffset) {
        return reinterpret_cast<T*>(&reinterpret_cast<uint8_t*>(this)[byteOffset]);
    }

    friend class CColModel;
};

// Layout check: gta-reversed VALIDATE_SIZE(CCollisionData, 0x30), enforced only
// on 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CCollisionData) == 0x30, "CCollisionData layout drift");
#endif
