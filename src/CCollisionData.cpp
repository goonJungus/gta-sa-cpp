// CCollisionData - adapted from gta-reversed for clean-room C++ build
// Method stubs. Decompiled reference: src/CCollisionData/*.c
// Each method below corresponds to a decompiled function - fill in from the .c file noted.

#include "CCollisionData.h"

CCollisionData::CCollisionData() {
    // TODO: src/CCollisionData/CCollisionData_0040f030.c
}

void CCollisionData::RemoveCollisionVolumes() {
    // TODO: src/CCollisionData/RemoveCollisionVolumes_0156fae0.c
}

void CCollisionData::Copy(const CCollisionData& src) {
    // TODO: src/CCollisionData/Copy_01562c90.c
}

void CCollisionData::CalculateTrianglePlanes() {
    // TODO: src/CCollisionData/CalculateTrianglePlanes_0040f590.c
}

void CCollisionData::RemoveTrianglePlanes() {
    // TODO: src/CCollisionData/RemoveTrianglePlanes_0040f6a0.c
}

void CCollisionData::GetTrianglePoint(CVector& outVec, int32_t vertId) {
    // TODO: src/CCollisionData/GetTrianglePoint_0040f5e0.c
}

void CCollisionData::GetShadTrianglePoint(CVector& outVec, int32_t vertId) {
    // TODO: src/CCollisionData/GetShadTrianglePoint_0040f640.c
}

void CCollisionData::SetLinkPtr(CLink<CCollisionData*>* link) {
    // TODO: src/CCollisionData/SetLinkPtr_01568990.c
}

CLink<CCollisionData*>* CCollisionData::GetLinkPtr() {
    // TODO: src/CCollisionData/GetLinkPtr_0156d850.c
    return nullptr;
}

uint32_t CCollisionData::GetNumFaceGroups() const {
    // TODO: NOTSA helper - no named .c in src/CCollisionData/; derive from the
    // FaceGroups memory layout documented in the header
    return 0;
}

const ColHelpers::TFaceGroup* CCollisionData::GetFaceGroups() const {
    // TODO: NOTSA helper - no named .c in src/CCollisionData/; derive from the
    // FaceGroups memory layout documented in the header
    return nullptr;
}

std::array<CVector, 3> CCollisionData::GetTriVertices(const CColTriangle& tri) const {
    // TODO: NOTSA helper - no named .c in src/CCollisionData/
    return std::array<CVector, 3>{};
}

void CCollisionData::AllocateLines(uint32_t num) {
    // TODO: no named .c in src/CCollisionData/ - identify from unk_*.c / binary
}

void CCollisionData::SetSpheres(const CColSphere* spheres) {
    // TODO: no named .c in src/CCollisionData/ - identify from unk_*.c / binary
}
