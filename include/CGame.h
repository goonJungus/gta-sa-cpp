// CGame - minimal clean-room header for clean-room C++ build
// Adapted from gta-reversed/source/game_sa/Game.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
//
// MINIMAL PORT: only what converted TUs need so far (CGame::currArea).
// TODO: port the full CGame surface (Init, Process, ShutDown, currArea as
// eAreaCodes, etc.) when the game-flow batch lands.

#pragma once

#include <cstdint>

class CGame {
public:
    // Current area code (was StaticRef<eAreaCodes>(0xB72914) in the original).
    // inline: owned here so no CGame.cpp is needed yet.
    static inline int32_t currArea = 0;

    // TODO(port): real implementation (gta-reversed Game.h).
    // Added 2026-10-09 for CAutomobile.
    // Declared here so CAutomobile links; defined in the game-flow batch.
    static bool CanSeeOutSideFromCurrArea();
};
