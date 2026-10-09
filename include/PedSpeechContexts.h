// PedSpeechContexts.h - minimal stand-in (gta-reversed/source/game_sa/Audio/Enums/PedSpeechContexts.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the values used by the currently-converted TUs are defined here.
// Values verified 2026-10-09 vs gta-reversed (940-line full enum).
// The full enum lands with the audio batch. Added 2026-10-09 for CAutomobile.

#pragma once

#include <cstdint>

enum eGlobalSpeechContext : int16_t {
    CTX_GLOBAL_UNK         = -1,
    CTX_GLOBAL_NO_SPEECH   = 0,
    // TODO(audio): full eGlobalSpeechContext (940 lines in gta-reversed).
    CTX_GLOBAL_BLOCKED     = 22, // 0x16
    CTX_GLOBAL_CAR_CRASH   = 29, // 0x1D
    CTX_GLOBAL_CAR_FIRE    = 33, // 0x21
    CTX_GLOBAL_CAR_HIT_PED = 36, // 0x24
    CTX_GLOBAL_CRASH_BIKE  = 66, // 0x42
    CTX_GLOBAL_CRASH_CAR   = 67, // 0x43
    CTX_GLOBAL_CRASH_GENERIC = 68, // 0x44
};
