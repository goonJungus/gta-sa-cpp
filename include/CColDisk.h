// CColDisk - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Collision/ColDisk.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.

#pragma once

#include "CColSphere.h"

#include <cstdint>

class CColDisk : public CColSphere {
public:
    CVector m_vThickness{};
    float   m_fThickness{};
};

// Layout check: gta-reversed VALIDATE_SIZE(CColDisk, 0x24), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
// The decomp confirms it: CColModel::AllocateData uses numLines * 0x24 when
// bUsesDisks (src/CColModel/AllocateData_01561730.c).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CColDisk) == 0x24, "CColDisk layout drift");
#endif
