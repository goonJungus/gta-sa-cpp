// CRect - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Core/Rect.h (values verified against decomp src/_types.h)
// 2D bounding rectangle, heavily used by CWorld sector queries.

#pragma once

#include "CVector.h"

class CRect {
public:
    float left{};
    float bottom{};
    float right{};
    float top{};

    constexpr CRect() = default;
    constexpr CRect(float l, float b, float r, float t)
        : left{ l }, bottom{ b }, right{ r }, top{ t } {}
    constexpr CRect(const CVector2D& center, float radius)
        : left{ center.x - radius }, bottom{ center.y - radius },
          right{ center.x + radius }, top{ center.y + radius } {}

    // TODO: port helpers (IsPointInside, StretchToPoint, Resize, ...) verified from decomp
};
