// CColSphere - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Collision/ColSphere.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.

#pragma once

#include "CVector.h"
#include "CMatrix.h"
#include "ColTypes.h" // CSphere, CColSurface, eColSurfaceType, tColLighting (minimal stand-ins)

#include <cstdint>

class CColSphere : public CSphere {
public:
    CColSurface m_Surface;

public:
    // InjectHooks() stripped - plugin-sdk hooking mechanism, not needed for clean-room

    CColSphere() = default;

    CColSphere(const CSphere& sp, const CColSurface& surface = {}) : // TODO: Make this explicit
        CSphere{ sp }, m_Surface{ surface }
    { }

    constexpr CColSphere(CSphere sp, eColSurfaceType material, uint8_t pieceType, tColLighting lighting = tColLighting(0xFF)) : CSphere(sp) {
        m_Surface.m_nMaterial = material;
        m_Surface.m_nPiece = pieceType;
        m_Surface.m_nLighting = lighting;
    }

    [[deprecated]]
    CColSphere(float radius, const CVector& center) : CSphere(center, radius){};

    CColSphere(const CVector& center, float radius) : CSphere(center, radius){};

    void Set(float radius, const CVector& center, eColSurfaceType material, uint8_t pieceType, tColLighting lighting = tColLighting{0xFF});
    bool IntersectRay(const CVector& rayOrigin, const CVector& direction, CVector& intersectPoint1, CVector& intersectPoint2);
    bool IntersectEdge(const CVector& startPoint, const CVector& endPoint, CVector& intersectPoint1, CVector& intersectPoint2);
    bool IntersectSphere(const CColSphere& right) const;
    bool IntersectPoint(const CVector& point);

    auto GetSurfaceType() const { return m_Surface.m_nMaterial; }

    friend auto TransformObject(const CColSphere& sp, const CMatrix& mat) -> CColSphere;
};

// Layout check: gta-reversed VALIDATE_SIZE(CColSphere, 0x14), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CColSphere) == 0x14, "CColSphere layout drift");
#endif
