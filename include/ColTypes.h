// ColTypes - adapted from gta-reversed for clean-room C++ build
// Minimal stand-ins for small collision-adjacent types that the collision
// subsystem headers reference but which belong to other subsystems (not yet
// converted). Each is a faithful DATA LAYOUT copied from its gta-reversed
// source (sizes cross-checked against the VALIDATE_SIZE in the original).
// Methods are omitted until the owning subsystem is converted - see TODOs.
//
// SOURCES (all under gta-reversed/source/game_sa/):
//   CSphere            <- Collision/Sphere.h            (0x10)
//   CBox/CBoundingBox  <- Collision/Box.h               (0x18)
//                      <- Collision/BoundingBox.h        (0x18)
//   CColSurface        <- Collision/ColSurface.h         (0x4)
//   tColLighting       <- Collision/ColPoint.h          (0x1)
//   CColPoint          <- Collision/ColPoint.h          (0x2C)
//   CColBox            <- Collision/ColBox.h            (0x1C)
//   CStoredCollPoly    <- StoredCollPoly.h              (0x2C)
//   FixedFloat/FixedVector <- extensions/FixedFloat.hpp, extensions/FixedVector.hpp
//   CLink / CLinkList  <- Core/Link.h, Core/LinkList.h
//   IplDef             <- IplDef.h                      (0x34)
//
// Types that already have converted headers are NOT duplicated here:
//   CRect   <- include/CRect.h      (Core/Rect.h)
//   ColDef  <- include/CColStore.h  (Collision/ColStore.h)
//   CVector / CMatrix <- include/CVector.h, include/CMatrix.h

#pragma once

#include "CVector.h"
#include "CRect.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <utility>

// ---------------------------------------------------------------------------
// Surface material type.
// gta-reversed's canonical definition is `enum eSurfaceType : uint8`
// (Enums/eSurfaceType.h), and the collision structs store it as ONE byte -
// VALIDATE_SIZE(CColSurface, 0x4) requires it.
// CONFLICT: include/CPhysical.h (world_entities subsystem) currently defines an
// unscoped `enum eSurfaceType : int32_t` (4 bytes, values verified against
// decomp src/_types.h). Until the tree reconciles on one definition, the
// collision headers use this interim 1-byte type so layouts stay binary-faithful.
// TODO: reconcile with CPhysical.h - the binary layout (CColSurface 0x4,
// CColSphere 0x14, CColTriangle 0x8) proves the 1-byte width.
// ---------------------------------------------------------------------------
enum class eColSurfaceType : uint8_t {};

// ---------------------------------------------------------------------------
// Collision/ColPoint.h - tColLighting (0x1): day/night nibble-packed lighting.
// This is the CANONICAL definition (gta-reversed VALIDATE_SIZE 0x1); it is also
// what CObject::m_nColLighting stores (decomp src/CObject/
// GetLightingFromCollisionBelow_0059fd00.c copies a CColPoint's m_nLightingB
// into it as the same type). Replaces the provisional 4-byte version that was
// in include/CObject.h.
// ---------------------------------------------------------------------------
struct tColLighting {
    union {
        struct {
            uint8_t day : 4;
            uint8_t night : 4;
        };
        uint8_t value;
    };

    tColLighting() = default;
    // NOTE: gta-reversed wrote this ctor with bitfield assignments in the body;
    // that form is rejected as constexpr by GCC (union member must be initialized
    // in the mem-init list), so the value variant is initialized directly -
    // semantically identical (value == day | night << 4).
    constexpr explicit tColLighting(uint8_t ucLighting) : value(ucLighting) {}

    // Decomp: src/tColLighting/GetCurrentLighting_0059f0c0.c (needs the render
    // pipeline's day/night balance; defined in CBulletInfo.cpp until the
    // collision-lighting TU lands).
    float GetCurrentLighting(float fScale = 0.5f) const;
};

// ---------------------------------------------------------------------------
// Collision/ColSurface.h - CColSurface (0x4)
// ---------------------------------------------------------------------------
struct CColSurface {
    eColSurfaceType m_nMaterial{};
    uint8_t         m_nPiece{}; // ePedPieceTypes, eCarPiece, etc
    tColLighting    m_nLighting{};
    uint8_t         m_nLight{};
};

// ---------------------------------------------------------------------------
// Collision/Sphere.h - CSphere (0x10)
// TODO: Set(), IsPointWithin(), DrawWireFrame(), GetTransformed(),
//       TransformObject() not ported yet
// ---------------------------------------------------------------------------
class CSphere {
public:
    CVector m_vecCenter{};
    float   m_fRadius{};

    constexpr CSphere() = default;
    constexpr CSphere(CVector center, float radius) : m_vecCenter(center), m_fRadius(radius) {}
};

// ---------------------------------------------------------------------------
// Collision/Box.h - CBox (0x18)
// TODO: Set(), Recalc(), GetSize()/GetWidth()/GetLength()/GetHeight(),
//       GetCenter(), IsPointInside(), StretchToPoint(), DrawWireFrame(),
//       GetShortestVectorDistToPt() not ported yet
// ---------------------------------------------------------------------------
class CBox {
public:
    CVector m_vecMin{}, m_vecMax{};

    constexpr CBox() = default;
    constexpr CBox(CVector min, CVector max) : m_vecMin(min), m_vecMax(max) {}
};

// ---------------------------------------------------------------------------
// Collision/BoundingBox.h - CBoundingBox (0x18)
// TODO: SetMinMax() not ported yet
// ---------------------------------------------------------------------------
class CBoundingBox : public CBox {
public:
    constexpr CBoundingBox() : CBox(CVector{ 1.0f }, CVector{ -1.0f }) {}
    constexpr CBoundingBox(CVector min, CVector max) : CBox(min, max) {}
    constexpr explicit CBoundingBox(const CBox& box) : CBox(box) {}

    // Needed by CExplosion::TestForExplosionInArea (decomp 0x736950).
    bool IsPointWithin(const CVector& point) const {
        return point.x >= m_vecMin.x && point.x <= m_vecMax.x
            && point.y >= m_vecMin.y && point.y <= m_vecMax.y
            && point.z >= m_vecMin.z && point.z <= m_vecMax.z;
    }
};

// ---------------------------------------------------------------------------
// Collision/ColBox.h - CColBox (0x1C)
// TODO: Set(), operator= not ported yet
// ---------------------------------------------------------------------------
class CColBox : public CBox {
public:
    CColSurface m_Surface;

    constexpr CColBox() = default;
    constexpr CColBox(const CVector& min, const CVector& max) : CBox(min, max) {}
    constexpr CColBox(const CBox& box) : CBox(box) {}
    constexpr CColBox(const CBox& box, eColSurfaceType material, uint8_t pieceType, tColLighting lighting) : CBox(box) {
        m_Surface.m_nMaterial = material;
        m_Surface.m_nPiece = pieceType;
        m_Surface.m_nLighting = lighting;
    }

    auto GetSurfaceType() const { return m_Surface.m_nMaterial; }
};

// ---------------------------------------------------------------------------
// Collision/ColPoint.h - CColPoint (0x2C)
// ---------------------------------------------------------------------------
class CColPoint {
public:
    /* https://github.com/multitheftauto/mtasa-blue/blob/master/Client/game_sa/CColPointSA.h */
    CVector      m_vecPoint;        // 0x00
    float        field_C;           // 0x0C
    CVector      m_vecNormal;       // 0x10
    float        field_1C;          // 0x1C

    // col shape 1 info
    eColSurfaceType m_nSurfaceTypeA; // 0x20
    uint8_t         m_nPieceTypeA;    // 0x21
    tColLighting    m_nLightingA;     // 0x22

    // col shape 2 info
    eColSurfaceType m_nSurfaceTypeB; // 0x23
    uint8_t         m_nPieceTypeB;    // 0x24
    tColLighting    m_nLightingB;     // 0x25

    char _align0x26[2];              // 0x26

    float m_fDepth;                  // 0x28
};

// ---------------------------------------------------------------------------
// StoredCollPoly.h - CStoredCollPoly (0x2C)
// ---------------------------------------------------------------------------
struct CStoredCollPoly {
    CVector      verts[3]{}; // triangle vertices
    bool         valid{};
    char         _pad[3];
    tColLighting ligthing{}; // (sic - original field name)
};

// ---------------------------------------------------------------------------
// extensions/FixedFloat.hpp / extensions/FixedVector.hpp, adapted to C++17:
//  - the `std::integral` concept constraint is dropped (<concepts> is C++20)
//  - the float non-type template parameter CompressValue becomes int
//    (every use is a whole number: 128, 4096, 8, 32767 - arithmetic identical)
//  - cross-scale converting ctors and arithmetic operators omitted for now
// TODO: replace with converted extensions/ headers if the project moves to C++20
// ---------------------------------------------------------------------------
template<typename T, int CompressValue>
struct FixedFloat {
    constexpr FixedFloat() = default;

    //! Construct from an uncompressed value
    constexpr FixedFloat(float v) : value(static_cast<T>(v * CompressValue)) {}

    //! Construct from a pre-compressed value
    template<typename Y>
    explicit constexpr FixedFloat(Y x) : value(static_cast<T>(x)) {}

    constexpr operator float() const { return static_cast<float>(value) / CompressValue; }

    constexpr void Set(float v) { value = static_cast<T>(v * CompressValue); }

    T value{};
};

template<typename T, int CompressValue>
struct FixedVector {
    constexpr FixedVector() = default;
    constexpr FixedVector(const CVector& v) : x(v.x), y(v.y), z(v.z) {}
    constexpr FixedVector(T X, T Y, T Z) : x(X), y(Y), z(Z) {}

    constexpr operator CVector() const {
        return CVector{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) };
    }

    FixedFloat<T, CompressValue> x{}, y{}, z{};
};

using CompressedVector     = FixedVector<int16_t, 128>;
using CompressedUnitVector = FixedVector<int16_t, 4096>;

// ---------------------------------------------------------------------------
// Core/Link.h - CLink<T> (faithful; VALIDATE_SIZE stripped, see assert below)
// ---------------------------------------------------------------------------
template <typename T>
class CLink {
public:
    T         data;
    CLink<T>* prev;
    CLink<T>* next;

    void Remove() {
        next->prev = prev;
        prev->next = next;
    }

    /*!
     * @brief Insert `this` into a list.
     * @brief If `this` is already in another list, `Remove()` must first be called!
     * @param after The link to insert `this` after.
     */
    void Insert(CLink<T>* after) {
        assert(after);

        next = after->next;
        next->prev = this;

        prev = after;
        prev->next = this;
    }
};

// ---------------------------------------------------------------------------
// Core/LinkList.h - CLinkList<T> (0x34 for void*)
// Adapted: game-memory operator new/delete (0x821195/0x8213AE) replaced with
// standard new/delete. TODO: verify allocation behavior from decomp.
// ---------------------------------------------------------------------------
template <typename T> class CLinkList {
public:
    CLink<T>  usedListHead{};
    CLink<T>  usedListTail{};
    CLink<T>  freeListHead{};
    CLink<T>  freeListTail{};
    CLink<T>* links{};

    void Init(int32_t count) {
        usedListHead.next = &usedListTail;
        usedListTail.prev = &usedListHead;
        freeListHead.next = &freeListTail;
        freeListTail.prev = &freeListHead;

        links = new CLink<T>[count];
        for (int32_t i = count - 1; i >= 0; i--) {
            links[i].Insert(&freeListHead);
        }
    }

    void Shutdown() {
        delete[] std::exchange(links, nullptr);
    }

    //! Insert `link` at head
    void Insert(CLink<T>& link) {
        link.Remove();
        link.Insert(&usedListHead);
    }

    CLink<T>* Insert(T const& data) {
        CLink<T>* link = freeListHead.next;
        if (link == &freeListTail)
            return nullptr;
        link->data = data;
        Insert(*link);
        return link;
    }

    CLink<T>* InsertSorted(T const& data) {
        CLink<T>* i = nullptr;
        for (i = usedListHead.next; i != &usedListTail; i = i->next) {
            if (i->data.m_distance >= data.m_distance)
                break;
        }
        CLink<T>* link = freeListHead.next;
        if (link == &freeListTail)
            return nullptr;
        link->data = data;
        link->Remove();
        link->Insert(i->prev);
        return link;
    }

    void Clear() {
        for (CLink<T>* link = usedListHead.next; link != &usedListTail; link = usedListHead.next) {
            Remove(link);
        }
    }

    auto Remove(CLink<T>* l) {
        l->Remove();
        l->Insert(&freeListHead);
        return l;
    }

    auto GetTail() { return usedListTail.prev; }
    auto& GetTailLink() { return usedListTail; }

    auto GetHead() { return usedListHead.next; }
    auto& GetHeadLink() { return usedListHead; }
};

// ---------------------------------------------------------------------------
// IplDef.h - IplDef (0x34)
// NOTE: gta-reversed used SHRT_MAX/SHRT_MIN (<climits>) and strcpy_s (MSVC-only)
// in the named ctor; replaced with INT16_MAX/MIN and strncpy for portability.
// ---------------------------------------------------------------------------
struct IplDef {
    CRect   bb{};
    char    name[18]{};

    int16_t firstBuilding{ INT16_MAX };
    int16_t lastBuilding{ INT16_MIN };

    int16_t firstDummy{ INT16_MAX };
    int16_t lastDummy{ INT16_MIN };

    int16_t staticIdx{ -1 }; // entity arrays index
    bool    isInterior{};
    char    loaded{};
    bool    loadRequested{};
    bool    disableDynamicStreaming{ true };
    char    ignoreWhenDeleted{};
    char    isLarge{}; // Makes bounding box bigger. (+350 vs +200 units). See `CIplStore::LoadIpl`

    constexpr IplDef() = default;

    IplDef(const char* n) {
        std::strncpy(name, n, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }
};

// ---------------------------------------------------------------------------
// Layout checks: gta-reversed VALIDATE_SIZE values, enforced only on 32-bit
// targets (the original binary is 32-bit; 64-bit dev builds skip them).
// ---------------------------------------------------------------------------
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tColLighting) == 0x1, "tColLighting layout drift");
static_assert(sizeof(CColSurface) == 0x4, "CColSurface layout drift");
static_assert(sizeof(CSphere) == 0x10, "CSphere layout drift");
static_assert(sizeof(CBox) == 0x18, "CBox layout drift");
static_assert(sizeof(CBoundingBox) == 0x18, "CBoundingBox layout drift");
static_assert(sizeof(CColBox) == 0x1C, "CColBox layout drift");
static_assert(sizeof(CColPoint) == 0x2C, "CColPoint layout drift");
static_assert(sizeof(CStoredCollPoly) == 0x2C, "CStoredCollPoly layout drift");
static_assert(sizeof(CompressedVector) == 0x6, "CompressedVector layout drift");
static_assert(sizeof(CompressedUnitVector) == 0x6, "CompressedUnitVector layout drift");
static_assert(sizeof(CLink<void*>) == 0xC, "CLink layout drift");
static_assert(sizeof(CLinkList<void*>) == 0x34, "CLinkList layout drift");
static_assert(sizeof(IplDef) == 0x34, "IplDef layout drift");
#endif
