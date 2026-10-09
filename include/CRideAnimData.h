// CRideAnimData.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/RideAnimData.h
// Bike rider animation state (lean angles).
// Hierarchy: standalone (member of CBike)
//
// Adaptations:
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
//   AssocGroupId via opaque-enum decl (AnimTypes.h clashes with CSimpleTransform.h: both define CQuaternion)

#pragma once

#include <cstdint>

#include "GrTypes.h"

enum AssocGroupId : int32_t; // full definition in converted AnimTypes.h










class CRideAnimData {
public:
    AssocGroupId AnimGroup{};
    float        BarSteerAngle{};
    float        LeanAngle{};
    float        DesiredLeanAngle{};
    float        LeanFwd{};
    float        AnimLeanLeft{};
    float        AnimLeanFwd{};

public:
    CRideAnimData() = default; // 0x6D0B10
    CRideAnimData(AssocGroupId animGroup) : AnimGroup(animGroup) {} // NOTSA
};
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CRideAnimData) == 0x1C, "CRideAnimData size mismatch");
#endif
