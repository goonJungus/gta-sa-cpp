// CColSphere - adapted from gta-reversed for clean-room C++ build
// Method stubs. Decompiled reference: src/CColSphere/*.c
// Each method below corresponds to a decompiled function - fill in from the .c file noted.
// (Ctors are inline in the header; the unk_0040fc8b-004100de.c files in
// src/CColSphere/ are unidentified - check them against the ctor bodies.)

#include "CColSphere.h"

void CColSphere::Set(float radius, const CVector& center, eColSurfaceType material, uint8_t pieceType, tColLighting lighting) {
    // TODO: src/CColSphere/Set_0040fd10.c
}

bool CColSphere::IntersectRay(const CVector& rayOrigin, const CVector& direction, CVector& intersectPoint1, CVector& intersectPoint2) {
    // TODO: src/CColSphere/IntersectRay_0040ff20.c
    return false;
}

bool CColSphere::IntersectEdge(const CVector& startPoint, const CVector& endPoint, CVector& intersectPoint1, CVector& intersectPoint2) {
    // TODO: src/CColSphere/IntersectEdge_004100e0.c
    return false;
}

bool CColSphere::IntersectSphere(const CColSphere& right) const {
    // TODO: src/CColSphere/IntersectSphere_00410090.c
    return false;
}

bool CColSphere::IntersectPoint(const CVector& point) {
    // TODO: src/CColSphere/IntersectPoint_00410040.c
    return false;
}

CColSphere TransformObject(const CColSphere& sp, const CMatrix& mat) {
    // TODO: friend of CColSphere declared in the header - no named .c in
    // src/CColSphere/; find the decomp reference before implementing
    return sp;
}
