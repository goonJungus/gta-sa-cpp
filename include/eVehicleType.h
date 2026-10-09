// eVehicleType.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Enums/eVehicleType.h
// Vehicle type ids. NOTE: CCamera.h and CVehicleModelInfo.h carry their own copies (values verified identical) - future dedup.
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

enum eVehicleType : int32 {
    VEHICLE_TYPE_IGNORE      = -1,
    VEHICLE_TYPE_AUTOMOBILE  = 0,
    VEHICLE_TYPE_MTRUCK      = 1,  // MONSTER TRUCK
    VEHICLE_TYPE_QUAD        = 2,
    VEHICLE_TYPE_HELI        = 3,
    VEHICLE_TYPE_PLANE       = 4,
    VEHICLE_TYPE_BOAT        = 5,
    VEHICLE_TYPE_TRAIN       = 6,
    VEHICLE_TYPE_FHELI       = 7,
    VEHICLE_TYPE_FPLANE      = 8,
    VEHICLE_TYPE_BIKE        = 9,
    VEHICLE_TYPE_BMX         = 10,
    VEHICLE_TYPE_TRAILER     = 11
};
