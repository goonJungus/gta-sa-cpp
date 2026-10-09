// CColTriangle - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Collision/ColTriangle.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.

#pragma once

#include "ColTypes.h" // eColSurfaceType, tColLighting

#include <cstdint>

class CColTriangle {
public:
    CColTriangle() = default;
    CColTriangle(uint16_t a, uint16_t b, uint16_t c, eColSurfaceType material, tColLighting light) :
        vA(a),
        vB(b),
        vC(c),
        m_nMaterial(material),
        m_nLight(light)
    {
    }

    auto GetSurfaceType() const { return m_nMaterial; }
public:
    union {
        struct {
            uint16_t vA; // vertex index in vertices array
            uint16_t vB; // vertex index in vertices array
            uint16_t vC; // vertex index in vertices array
        };
        uint16_t m_vertIndices[3];
    };
    eColSurfaceType m_nMaterial;
    tColLighting    m_nLight;
};

// Layout check: gta-reversed VALIDATE_SIZE(CColTriangle, 0x8), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
// The decomp confirms it: CColModel::AllocateData reserves numTriangles * 8
// bytes (src/CColModel/AllocateData_01561730.c: `param_6 * 8`).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CColTriangle) == 0x8, "CColTriangle layout drift");
#endif
