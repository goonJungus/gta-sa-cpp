// eAEVehicleAudioType.h - minimal stand-in (gta-reversed/source/game_sa/Audio/Enums/eAEVehicleAudioType.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Values verified 2026-10-09 vs gta-reversed.
// The full enum lands with the audio batch. Added 2026-10-09 for CAutomobile.

#pragma once

#include <cstdint>

enum class eAEVehicleAudioType : int16_t {
    CAR     = 0, // 0x0
    BIKE    = 1, // 0x1
    GENERIC = 2, // 0x2
    // TODO(audio): full eAEVehicleAudioType if more values are needed.
};
