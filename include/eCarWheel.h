// eCarWheel.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Enums/eCarWheel.h
// Car wheel ids. NOTE: CVehicleModelInfo.h carries its own copy (values verified identical) - future dedup. CarWheelToCarPiece() decl dropped (eCarPiece lives in CVehicle.h; re-add with its port).
//
#pragma once

#include <cstdint>

// Base.h (plugin-sdk) replacements - gta-reversed integer typedefs
using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;

enum eCarWheel {
    CAR_WHEEL_FRONT_LEFT  = 0,
    CAR_WHEEL_REAR_LEFT   = 1,
    CAR_WHEEL_FRONT_RIGHT = 2,
    CAR_WHEEL_REAR_RIGHT  = 3,

    MAX_CARWHEELS
};

