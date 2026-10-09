// CGeneral - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/General.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations:
//   stripped InjectHooks(), thread_local <random> engine, <extensions/utility.hpp>,
//   C++20 ranges/concepts template overloads (GetRandomNumberInRange templates,
//   RandomChoice, GetPiecewiseLinear) - not ported; simple overloads provided.
//   All functions are header-inline so no TU needs a CGeneral.cpp.

#pragma once

#include <cstdint>
#include <cstdlib> // rand, RAND_MAX
#include <cmath>   // atan2f, sqrtf

namespace CGeneral {

//! Normalize `angle` to be between [-180, 180] degrees (0x53CB00)
inline float LimitAngle(float angle) {
    while (angle > 180.0f)
        angle -= 360.0f;
    while (angle < -180.0f)
        angle += 360.0f;
    return angle;
}

//! Normalize `angle` to be between [-pi, pi] radians (0x53CB50)
inline float LimitRadianAngle(float angle) {
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI;
    while (angle > PI)
        angle -= TWO_PI;
    while (angle < -PI)
        angle += TWO_PI;
    return angle;
}

//! atan2 wrapper (0x53CC70)
inline float GetATanOfXY(float x, float y) {
    return atan2f(y, x);
}

//! Get the octant the vector's heading lies in (0x53CDC0)
inline uint32_t GetNodeHeadingFromVector(float x, float y) {
    // 8 octants over 360 degrees; heading measured from +y axis like the game.
    float heading = GetATanOfXY(x, y); // radians, [-pi, pi]
    if (heading < 0.0f)
        heading += 2.0f * 3.14159265358979323846f;
    return static_cast<uint32_t>(heading / (2.0f * 3.14159265358979323846f) * 8.0f) & 7;
}

//! Try solving a quadratic equation; returns true if it had a (real) solution (0x53CE30)
inline bool SolveQuadratic(float a, float b, float c, float& x1, float& x2) {
    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f)
        return false;
    float sqrtD = sqrtf(discriminant);
    float denom = 2.0f * a;
    if (denom == 0.0f)
        return false;
    x1 = (-b + sqrtD) / denom;
    x2 = (-b - sqrtD) / denom;
    return true;
}

//! Angle in radians between 2 points (0x53CBE0)
inline float GetRadianAngleBetweenPoints(float x1, float y1, float x2, float y2) {
    return GetATanOfXY(x2 - x1, y2 - y1);
}

//! Angle in degrees between 2 points (0x53CEA0)
inline float GetAngleBetweenPoints(float x1, float y1, float x2, float y2) {
    return GetRadianAngleBetweenPoints(x1, y1, x2, y2) * (180.0f / 3.14159265358979323846f);
}

//! Random number in [0, RAND_MAX] (same as rand())
inline uint16_t GetRandomNumber() {
    return static_cast<uint16_t>(rand());
}

//! Random float in [min, max]
inline float GetRandomNumberInRange(float min, float max) {
    return min + (max - min) * (static_cast<float>(GetRandomNumber()) / static_cast<float>(RAND_MAX));
}

//! Random int in [min, max)
inline int32_t GetRandomNumberInRange(int32_t min, int32_t max) {
    if (max <= min)
        return min;
    return min + static_cast<int32_t>(GetRandomNumber() % static_cast<uint16_t>(max - min));
}

//! True `chanceOfTrue` % of the time; valid values [0, 100)
inline bool RandomBool(float chanceOfTrue) {
    return GetRandomNumberInRange(0.0f, 100.0f) < chanceOfTrue;
}

} // namespace CGeneral
