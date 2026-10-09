// CColLine - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Collision/ColLine.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.

#pragma once

#include "CVector.h"

#include <cstdint>

class CColLine {
public:
    CColLine() = default;
    CColLine(const CVector& start, const CVector& end) :
        m_vecStart(start),
        m_vecEnd(end)
    {
    }

    void Set(const CVector& start, const CVector& end) {
        m_vecStart = start;
        m_vecEnd = end;
    }

public:
    CVector m_vecStart;
    float   m_fStartSize{};
    CVector m_vecEnd;
    float   m_fEndSize{};
};

// Layout check: gta-reversed VALIDATE_SIZE(CColLine, 0x20), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
// The decomp confirms it: CCollision::ProcessLineOfSight strides the sphere
// array by 0x14 and the line/disk array by 0x20/0x24
// (src/CCollision/ProcessLineOfSight_00417950.c).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CColLine) == 0x20, "CColLine layout drift");
#endif
