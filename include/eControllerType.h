// eControllerType.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Enums/eControllerType.h
// Controller type (keyboard/mouse/joystick); used by CVehicle::m_nLastControlInput.
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

enum class eControllerType {
    KEYBOARD,
    OPTIONAL_EXTRA_KEY,
    MOUSE,
    JOY_STICK,

    CONTROLLER_TYPES_COUNT
};

constexpr eControllerType CONTROLLER_TYPES_ALL[] = {
    eControllerType::KEYBOARD,
    eControllerType::OPTIONAL_EXTRA_KEY,
    eControllerType::MOUSE,
    eControllerType::JOY_STICK
};

constexpr eControllerType CONTROLLER_TYPES_KEYBOARD[] = {
    eControllerType::KEYBOARD,
    eControllerType::OPTIONAL_EXTRA_KEY
};
