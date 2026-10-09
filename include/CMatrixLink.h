// CMatrixLink - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Core/MatrixLink.h
// CMatrix with an owner back-pointer, pooled in CPlaceable's static matrix array.

#pragma once

#include "CMatrix.h"

class CPlaceable;

class CMatrixLink : public CMatrix {
public:
    CPlaceable*  m_pOwner{};
    CMatrixLink* m_pPrev{};
    CMatrixLink* m_pNext{};

    CMatrixLink() = default;
    CMatrixLink(float fScale) { SetScale(fScale); } // TODO: verify from decomp

    void Insert(CMatrixLink* pWhere); // TODO: verify from decomp
    void Remove();                    // TODO: verify from decomp
};

#if INTPTR_MAX == INT32_MAX
// CMatrix (0x48) + 3 pointers (0xC) = 0x54, matches original VALIDATE_SIZE
static_assert(sizeof(CMatrixLink) == 0x54, "CMatrixLink layout changed");
#endif
