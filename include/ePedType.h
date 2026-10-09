// ePedType - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Enums/ePedType.h
// Full enum port - values verified identical to gta-reversed.
// Adaptation: IsPedTypeGang uses a range check (GANG1..GANG10 are contiguous)
// instead of the original std::ranges::find over s_GangPedTypes.

#pragma once

#include <cstdint>

enum ePedType : uint32_t {
    PED_TYPE_NONE = (uint32_t)(-1),
    PED_TYPE_PLAYER1 = 0,
    PED_TYPE_PLAYER2,
    PED_TYPE_PLAYER_NETWORK,
    PED_TYPE_PLAYER_UNUSED,
    PED_TYPE_CIVMALE,
    PED_TYPE_CIVFEMALE,

    PED_TYPE_COP,

    PED_TYPE_GANG1,         // Ballas
    PED_TYPE_GANG2,         // Grove Street Families
    PED_TYPE_GANG3,         // Los Santos Vagos
    PED_TYPE_GANG4,         // San Fierro Rifa
    PED_TYPE_GANG5,         // Da Nang Boys
    PED_TYPE_GANG6,         // Mafia
    PED_TYPE_GANG7,         // Mountain Cloud Triad
    PED_TYPE_GANG8,         // Varrio Los Aztecas
    PED_TYPE_GANG9,         // Russian Mafia
    PED_TYPE_GANG10,        // Bikers

    PED_TYPE_DEALER,
    PED_TYPE_MEDIC,
    PED_TYPE_FIREMAN,
    PED_TYPE_CRIMINAL,
    PED_TYPE_BUM,
    PED_TYPE_PROSTITUTE,
    PED_TYPE_SPECIAL,
    PED_TYPE_MISSION1,
    PED_TYPE_MISSION2,
    PED_TYPE_MISSION3,
    PED_TYPE_MISSION4,
    PED_TYPE_MISSION5,
    PED_TYPE_MISSION6,
    PED_TYPE_MISSION7,
    PED_TYPE_MISSION8,

    PED_TYPE_COUNT // 32
};
static_assert(PED_TYPE_COUNT <= sizeof(uint32_t) * 8); /* NOTE: See `GetPedFlag` */

inline bool IsPedTypeGang(ePedType ptype) {
    return ptype >= PED_TYPE_GANG1 && ptype <= PED_TYPE_GANG10;
}

inline bool IsPedTypePlayer(ePedType pt) {
    switch (pt) {
    case PED_TYPE_PLAYER1:
    case PED_TYPE_PLAYER2:
        return true;
    }
    return false;
}
