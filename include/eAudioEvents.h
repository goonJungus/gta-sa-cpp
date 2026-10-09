// eAudioEvents.h - minimal stand-in (gta-reversed/source/game_sa/Audio/Enums/eAudioEvents.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the values used by the currently-converted TUs are defined here.
// Values verified 2026-10-09 vs gta-reversed.
// The full enum lands with the audio batch. Added 2026-10-09 for CAutomobile.

#pragma once

#include <cstdint>

enum eAudioEvents : int32_t {
    AE_CAR_BONNET_OPEN  = 78,  // 0x4E
    AE_CAR_BONNET_CLOSE = 84,  // 0x54
    // TODO(audio): full eAudioEvents enum.
};
