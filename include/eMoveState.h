// eMoveState - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Enums/eMoveState.h
// Full enum port - values verified identical to gta-reversed.

#pragma once

#include <cstdint>

enum eMoveState : uint32_t {
    PEDMOVE_NONE = 0,
    PEDMOVE_STILL,
    PEDMOVE_TURN_L,
    PEDMOVE_TURN_R,
    PEDMOVE_WALK,
    PEDMOVE_JOG,
    PEDMOVE_RUN,
    PEDMOVE_SPRINT
};
