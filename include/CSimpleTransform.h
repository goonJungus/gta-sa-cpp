// CSimpleTransform - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/SimpleTransform.h
// Minimal position + heading transform. CPlaceable embeds one of these and only
// allocates a full CMatrix when it needs rotation/scale.

#pragma once

#include "CMatrix.h" // CVector, RwMatrix

class CSimpleTransform {
public:
    CVector m_vPosn;
    float   m_fHeading{ 0.0f };

    CSimpleTransform() = default;

    void UpdateRwMatrix(RwMatrix* out);        // TODO: verify from decomp
    void Invert(const CSimpleTransform& base); // TODO: verify from decomp
    void UpdateMatrix(CMatrix* out);           // TODO: verify from decomp
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CSimpleTransform) == 0x10, "CSimpleTransform layout changed"); // CVector(0xC) + float(4)
#endif
