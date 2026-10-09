// eGameState.h - minimal stand-in (gta-reversed/source/game_sa/Enums/eGameState.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the values used by the currently-converted TUs are defined here.
// Values verified 2026-10-09 vs gta-reversed.
// The full enum lands with the game-flow batch. Added 2026-10-09 for CAutomobile.

#pragma once

#include <cstdint>

enum eGameState : uint8_t {
    GAME_STATE_INITIAL = 0,
    // TODO(port): full eGameState enum (10 values in gta-reversed).
};
