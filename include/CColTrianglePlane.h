// CColTrianglePlane - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Collision/ColTrianglePlane.h
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Real layout from decomp (src/CColTrianglePlane/*.c and
// src/CCollision/TestLineTriangle_00413ac0.c):
//   int16_t m_normal[3]    - plane normal, compressed x4096 (GetNormal_00411610.c)
//   int16_t m_normalOffset - plane offset, compressed x128 (TestLineTriangle_00413ac0.c)
//   Orientation m_orientation - dominant-axis enum; the decomp names its type
//                               CColTrianglePlane__Orientation (TestLineTriangle_00413ac0.c)
// Total 0xA bytes.

#pragma once

#include "ColTypes.h" // CompressedVector, CStoredCollPoly, CVector

#include <cstdint>

class CColTriangle;

class CColTrianglePlane {
public:
    // Nested orientation enum (unscoped, matching the decomp's
    // CColTrianglePlane__Orientation with bare POS_X/NEG_X/... case labels).
    enum Orientation : uint8_t {
        POS_X, NEG_X, POS_Y, NEG_Y, POS_Z, NEG_Z
    };

    CColTrianglePlane(const CStoredCollPoly& poly);
    CColTrianglePlane(const CColTriangle& tri, const CompressedVector* vertices);
    CColTrianglePlane(const CVector& a, const CVector& b, const CVector& c);

    void Set(const CompressedVector* vertices, const CColTriangle& triangle);
    void GetNormal(CVector* out) const;

    int16_t     m_normal[3];    // compressed x4096
    int16_t     m_normalOffset; // compressed x128
    Orientation m_orientation;
    uint8_t     m_pad;
};

// Layout check: 0xA bytes per the decomp (enforced on 32-bit targets only,
// same convention as CColTriangle.h).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CColTrianglePlane) == 0xA, "CColTrianglePlane layout changed");
#endif
