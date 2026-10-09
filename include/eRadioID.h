// eRadioID.h - canonical radio station IDs (deduped 2026-10-09)
// Was defined in CAERadioTrackManager.h, CAudioEngine.h, CPedModelInfo.h, CStats.h.
// Values from gta-reversed/source/game_sa/Enums/eRadioID.h
#pragma once

#include <cstdint>

enum eRadioID : int8_t {
    RADIO_INVALID = -1,

    RADIO_EMERGENCY_AA,    // AA
    RADIO_CLASSIC_HIP_HOP, // Playback FM
    RADIO_COUNTRY,         // K-Rose
    RADIO_CLASSIC_ROCK,    // K-DST
    RADIO_DISC_FUNK,       // Bounce FM
    RADIO_HOUSE_CLASSICS,  // SF-UR
    RADIO_MODERN_HIP_HOP,  // Radio Los Santos
    RADIO_MODERN_ROCK,     // Radio X
    RADIO_NEW_JACK_SWING,  // CSR 103.9
    RADIO_REGGAE,          // K-Jah West
    RADIO_RARE_GROOVE,     // Master Sounds 98.3
    RADIO_TALK,            // WCTR
    RADIO_USER_TRACKS,
    RADIO_OFF,
    RADIO_COUNT,
};
