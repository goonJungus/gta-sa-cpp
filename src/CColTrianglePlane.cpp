// CColTrianglePlane - adapted from gta-reversed for clean-room C++ build
// Method stubs. Decompiled reference: src/CColTrianglePlane/*.c
// Each method below corresponds to a decompiled function - fill in from the .c file noted.

#include "CColTrianglePlane.h"

CColTrianglePlane::CColTrianglePlane(const CStoredCollPoly& poly) {
    // TODO: one of src/CColTrianglePlane/unk_00411580_00411580.c,
    // unk_00411590_00411590.c, unk_004115e0_004115e0.c,
    // unk_004115f0_004115f0.c - identify which maps to this ctor from the binary
}

CColTrianglePlane::CColTrianglePlane(const CColTriangle& tri, const CompressedVector* vertices) {
    // TODO: one of the unk_00411580/00411590/004115e0/004115f0.c files -
    // identify which maps to this ctor from the binary
}

CColTrianglePlane::CColTrianglePlane(const CVector& a, const CVector& b, const CVector& c) {
    // TODO: one of the unk_00411580/00411590/004115e0/004115f0.c files -
    // identify which maps to this ctor from the binary
}

void CColTrianglePlane::GetNormal(CVector* out) const {
    // src/CColTrianglePlane/GetNormal_00411610.c: normal stored compressed x4096.
    out->x = (float)m_normal[0] * 0.00024414062f;
    out->y = (float)m_normal[1] * 0.00024414062f;
    out->z = (float)m_normal[2] * 0.00024414062f;
}

void CColTrianglePlane::Set(const CompressedVector* vertices, const CColTriangle& triangle) {
    // TODO: src/CColTrianglePlane/Set_00411660.c
}
