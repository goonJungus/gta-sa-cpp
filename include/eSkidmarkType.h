// eSkidmarkType.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Skidmark.h (enum extracted; CSkidmark not ported)
// Skid mark surface types (wheels).
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

enum class eSkidmarkType : uint32 {
    DEFAULT = 0,
    SANDY,
    MUDDY,
    BLOODY,
};
