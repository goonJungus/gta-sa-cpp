// ePedStats - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Enums/ePedStats.h
// Full enum port - values verified identical to gta-reversed.

#pragma once

#include <cstdint>

enum class ePedStats : int32_t {
    NONE = -1,
    PLAYER,
    COP,
    MEDIC,
    FIREMAN,
    GANG1,
    GANG2,
    GANG3,
    GANG4,
    GANG5,
    GANG6,
    GANG7,
    GANG8,
    GANG9,
    GANG10,
    STREET_GUY,
    SUIT_GUY,
    SENSIBLE_GUY,
    GEEK_GUY,
    OLD_GUY,
    TOUGH_GUY,
    STREET_GIRL,
    SUIT_GIRL,
    SENSIBLE_GIRL,
    GEEK_GIRL,
    OLD_GIRL,
    TOUGH_GIRL,
    TRAMP_MALE,
    TRAMP_FEMALE,
    TOURIST,
    PROSTITUTE,
    CRIMINAL,
    BUSKER,
    TAXIDRIVER,
    PSYCHO,
    STEWARD,
    SPORTSFAN,
    SHOPPER,
    OLDSHOPPER,
    BEACH_GUY,
    BEACH_GIRL,
    SKATER,
    STD_MISSION,
    COWARD,
};
