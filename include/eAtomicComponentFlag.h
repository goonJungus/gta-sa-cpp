// eAtomicComponentFlag.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/VisibilityPlugins.h
// Atomic component flags (damage/visibility/culling). Enum extracted; rest of VisibilityPlugins.h (RW callbacks) not ported.
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

enum eAtomicComponentFlag {
    ATOMIC_NONE = 0x0,
    ATOMIC_OK = 0x1,
    ATOMIC_DAMAGED = 0x2,

    // Mask for checking whether the value is OK or DAMAGED ^^^
    ATOMIC_MASK = 0x3,

    ATOMIC_LEFT = 0x4,
    ATOMIC_RIGHT = 0x8,
    ATOMIC_FRONT = 0x10,
    ATOMIC_REAR = 0x20,
    ATOMIC_ALPHA = 0x40,
    ATOMIC_FLAT = 0x80,
    ATOMIC_REAR_DOOR = 0x100,
    ATOMIC_FRONT_DOOR = 0x200,
    ATOMIC_DONT_CULL = 0x400, // enabled for opened components (doors)
    ATOMIC_UPGRADE = 0x800,
    ATOMIC_DONT_RENDER_ALPHA = 0x1000,
    ATOMIC_UNIQUE_MATERIALS = 0x2000,
    ATOMIC_PIPE_NO_EXTRA_PASSES_LOD = 0x2000,
    ATOMIC_PIPE_NO_EXTRA_PASSES = 0x4000,
    ATOMIC_TOP = 0x8000
};
