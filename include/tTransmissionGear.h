// tTransmissionGear.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/tTransmissionGear.h (plugin-sdk file)
// Single gear's velocity thresholds for the transmission model.
//
// Adaptations:
//   stripped plugin-sdk header comment
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
//
#pragma once

#include "GrTypes.h"

struct tTransmissionGear {
    float MaxVelocity;
    float ChangeUpVelocity;   // max velocity needed to change the current gear to higher
    float ChangeDownVelocity; // min velocity needed to change the current gear to lower
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tTransmissionGear) == 0xC, "tTransmissionGear size mismatch");
#endif
