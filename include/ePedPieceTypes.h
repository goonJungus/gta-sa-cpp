// ePedPieceTypes - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Entity/Ped/Ped.h (enum defined there;
// full CPed conversion belongs to the ped subsystem)

#pragma once

enum ePedPieceTypes {
    PED_COL_SPHERE_LEG  = 0,
    PED_COL_SPHERE_MID  = 1,
    PED_COL_SPHERE_HEAD = 2,
    PED_PIECE_TORSO     = 3, // AKA CHEST
    PED_PIECE_ASS       = 4, // AKA MIDSECTION
    PED_PIECE_LEFT_ARM  = 5, // AKA UPPERARM_L
    PED_PIECE_RIGHT_ARM = 6, // AKA UPPERARM_R
    PED_PIECE_LEFT_LEG  = 7, // AKA LEG_L
    PED_PIECE_RIGHT_LEG = 8, // AKA LEG_R
    PED_PIECE_HEAD      = 9
};
