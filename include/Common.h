// Common.h - GTA SA 1.0 clean-room C++ conversion
// Shared math helpers + the StaticRef fixed-address global pattern.
// Adapted from gta-reversed/source/game_sa/common.h and plugin-sdk.
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// NOTE: StaticRef<T>(addr) dereferences a hard game address. It compiles into the
// clean-room static lib but is only meaningful when the address space matches
// GTA SA 1.0 - same convention the vehicle sources already used.

#pragma once

#include "CVector.h"

#include <cstdint>
#include <initializer_list>

// ---- Math constants (were #defines in the original common.h) ----
constexpr double PI      = 3.14159265358979323846;
constexpr double HALF_PI = PI / 2.0;
constexpr double TWO_PI  = PI * 2.0;

// ---- sq: square of a value (gta-reversed common.h) ----
template<typename T>
constexpr T sq(T x) {
    return x * x;
}

// ---- DotProduct (gta-reversed common.h) ----
inline float DotProduct(const CVector& v1, const CVector& v2) {
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

// ---- Angle conversion (gta-reversed common.h) ----
constexpr float DegreesToRadians(float angle) {
    return angle * static_cast<float>(PI / 180.0);
}

inline float RadiansToDegrees(float angle) {
    return angle * static_cast<float>(180.0 / PI);
}

// ---- CrossProduct (gta-reversed common.h) ----
inline CVector CrossProduct(const CVector& v1, const CVector& v2) {
    return v1.Cross(v2);
}

// ---- NOTSA_UNREACHABLE (gta-reversed Base.h: UNREACHABLE_INTRINSIC) ----
#if defined(_MSC_VER)
#define NOTSA_UNREACHABLE() __assume(false)
#else
#define NOTSA_UNREACHABLE() __builtin_unreachable()
#endif

// ---- StaticRef: reference to a fixed-address global (gta-reversed pattern) ----
template<typename T>
T& StaticRef(uint32_t address) {
    return *reinterpret_cast<T*>(address);
}

// ---- notsa utilities (gta-reversed extensions/utility.hpp) ----
namespace notsa {
// TODO(port): full version dynamic_casts when ptr is non-null (gta-reversed).
// Minimal: always null until the task class hierarchy is ported.
// Added 2026-10-09 for CAutomobile.
template<typename T, typename U>
T* dyn_cast_if_present(U* ptr) {
    (void)ptr;
    return nullptr;
}
// gta-reversed notsa::contains; minimal initializer_list version.
// Added 2026-10-09 for CAutomobile.
template<typename T, typename U>
bool contains(std::initializer_list<T> list, const U& value) {
    for (const auto& item : list) {
        if (item == value) {
            return true;
        }
    }
    return false;
}
} // namespace notsa
