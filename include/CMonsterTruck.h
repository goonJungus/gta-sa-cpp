// CMonsterTruck.h - GTA SA 1.0 clean-room C++ conversion
// Minimal stand-in for gta-reversed/source/game_sa/Entity/Vehicle/MonsterTruck.h
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// TODO: full port of CMonsterTruck (class + MonsterTruck.cpp). Bodies verified
// against decomp src/CMonsterTruck/*.c when the port lands.

#pragma once

#include "Common.h"      // StaticRef
#include "CAutomobile.h" // base class

class CMonsterTruck : public CAutomobile {
public:
    // 0x8D33A8 (0.0002f)
    static inline auto& DUMPER_COL_ANGLEMULT = StaticRef<float>(0x8D33A8);
};
