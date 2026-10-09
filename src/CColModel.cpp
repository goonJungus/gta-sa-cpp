// CColModel - adapted from gta-reversed for clean-room C++ build
// Method bodies verified against src/CColModel/*.c and
// gta-reversed/source/game_sa/Collision/ColModel.cpp.

#include "CColModel.h"
#include "CCollision.h"
#include "CColLine.h"
#include "CColDisk.h"
#include "CColTriangle.h"
#include "CMemoryMgr.h"

#include <new>
#include <cassert>

// 0x40FB60 (body relocated to .HOODLUM: src/CColModel/CColModel_0156c690.c)
CColModel::CColModel() : m_boundBox() {
    m_nColSlot              = 0;
    m_pColData              = nullptr;
    m_bHasCollisionVolumes  = false;
    m_bIsSingleColDataAlloc = false;
    m_IsActive              = true;
    // NOTE: the original ctor does not initialize m_boundSphere; the member is
    // left with indeterminate values, exactly as the binary leaves 0x18-0x27.
    // It is filled by the collision loader before any read.
}

// 0x40F700 (src/CColModel/_dtor_CColModel_0040f700.c)
CColModel::~CColModel() {
    if (!m_IsActive) {
        return;
    }
    RemoveCollisionVolumes();
}

// 0x40FC30 (src/CColModel/operator_new_0040fc30.c)
void* CColModel::operator new(size_t size) {
    (void)size;
    // TODO: the original allocates from CColModelPool (CColModelPool::New);
    // the pool subsystem is not converted yet, so this falls back to the
    // global operator new. Allocation source differs, behavior is equivalent.
    return ::operator new(sizeof(CColModel));
}

void CColModel::operator delete(void* data) {
    // TODO: see operator new - original returns the slot to CColModelPool.
    ::operator delete(data);
}

// 0x40F7C0 (src/CColModel/MakeMultipleAlloc_015697f0.c - mislabeled by the
// decompiler; gta-reversed identifies 0x40F7C0 as operator=)
CColModel& CColModel::operator=(const CColModel& colModel) {
    assert(&colModel != this); // BUG(Prone): the original has no self-assignment check

    m_boundSphere = colModel.m_boundSphere;
    m_boundBox    = colModel.m_boundBox;

    // NOTE: m_nColSlot, m_nFlags and the m_pColData pointer itself are NOT
    // copied - only the pointed-to data is. Matches the decomp (it copies
    // dwords 0-9, i.e. the two bounding volumes, then Copy()s the col data).
    if (m_pColData) {
        m_pColData->Copy(*colModel.m_pColData);
    }
    // else: the original silently copies nothing either.

    return *this;
}

// 0x40F810 (body relocated to .HOODLUM: src/CColModel/AllocateData_0156deb0.c)
void CColModel::AllocateData() {
    m_bIsSingleColDataAlloc = false;
    m_pColData = new CCollisionData();
    assert(m_pColData);
}

// 0x40F870 (body relocated to .HOODLUM: src/CColModel/AllocateData_01561730.c)
//
// Memory layout of the single m_pColData allocation:
// | CCollisionData | CColSphere[] | CColLine[]/CColDisk[] | CColBox[] | Vertices[] | CColTriangle[] |
void CColModel::AllocateData(int32_t numSpheres, int32_t numBoxes, int32_t numLines, int32_t numVertices, int32_t numTriangles, bool bUsesDisks) {
    const uint32_t baseSize          = sizeof(CCollisionData);
    const uint32_t spheresSize       = numSpheres * sizeof(CColSphere);
    const uint32_t linesOrDisksSize  = bUsesDisks ? numLines * sizeof(CColDisk) : numLines * sizeof(CColLine);
    const uint32_t boxesSize         = numBoxes * sizeof(CColBox);
    const uint32_t vertsSize         = numVertices * sizeof(CompressedVector);
    const uint32_t trianglesSize     = numTriangles * sizeof(CColTriangle);

    const uint32_t spheresOffset       = baseSize;
    const uint32_t linesOrDisksOffset  = spheresOffset + spheresSize;
    const uint32_t boxesOffset         = linesOrDisksOffset + linesOrDisksSize;
    const uint32_t vertsOffset         = boxesOffset + boxesSize;
    // Decomp: the triangles start at the 4-byte-aligned address right after
    // the vertices: (vertsOffset + vertsSize + 3) & ~3.
    const uint32_t trianglesOffset     = (vertsOffset + vertsSize + 3u) & ~3u;

    AllocateData(trianglesOffset + trianglesSize);

    m_pColData->m_nNumSpheres   = static_cast<uint16_t>(numSpheres);
    m_pColData->m_nNumBoxes     = static_cast<uint16_t>(numBoxes);
    m_pColData->m_nNumLines     = static_cast<uint8_t>(numLines);
    m_pColData->m_nNumTriangles = static_cast<uint16_t>(numTriangles);
    m_pColData->bUsesDisks      = bUsesDisks;

    m_pColData->m_pSpheres = m_pColData->GetPointerToColArray<CColSphere>(spheresOffset);
    if (bUsesDisks)
        m_pColData->m_pDisks = numLines ? m_pColData->GetPointerToColArray<CColDisk>(linesOrDisksOffset) : nullptr;
    else
        m_pColData->m_pLines = numLines ? m_pColData->GetPointerToColArray<CColLine>(linesOrDisksOffset) : nullptr;
    m_pColData->m_pBoxes     = numBoxes     ? m_pColData->GetPointerToColArray<CColBox>(boxesOffset)         : nullptr;
    m_pColData->m_pVertices  = numVertices  ? m_pColData->GetPointerToColArray<CompressedVector>(vertsOffset) : nullptr;
    m_pColData->m_pTriangles = numTriangles ? m_pColData->GetPointerToColArray<CColTriangle>(trianglesOffset) : nullptr;
    m_pColData->m_pTrianglePlanes = nullptr;
}

// 0x40F9B0 (src/CColModel/AllocateData_0040f9b0.c)
void CColModel::AllocateData(int32_t size) {
    m_bIsSingleColDataAlloc = true;
    m_pColData = static_cast<CCollisionData*>(CMemoryMgr::Malloc(size));
    assert(m_pColData);
}

// 0x40F740 (body relocated to .HOODLUM: src/CColModel/MakeMultipleAlloc_01564a10.c)
void CColModel::MakeMultipleAlloc() {
    if (!m_bIsSingleColDataAlloc)
        return;

    CCollisionData* const colData = new CCollisionData();
    assert(colData);

    colData->Copy(*m_pColData);
    CMemoryMgr::Free(m_pColData);

    m_bIsSingleColDataAlloc = false;
    m_pColData = colData;
}

// 0x40F9E0 (src/CColModel/RemoveCollisionVolumes_0040f9e0.c)
void CColModel::RemoveCollisionVolumes() {
    if (!m_pColData) {
        return;
    }

    if (m_bIsSingleColDataAlloc) {
        CCollision::RemoveTrianglePlanes(m_pColData);
        CMemoryMgr::Free(m_pColData);
    } else {
        m_pColData->RemoveCollisionVolumes();
        delete m_pColData;
    }

    m_pColData = nullptr;
}

// 0x40FA30 (src/CColModel/CalculateTrianglePlanes_0040fa30.c)
void CColModel::CalculateTrianglePlanes() {
    if (m_pColData)
        m_pColData->CalculateTrianglePlanes();
}

// 0x40FA40 (src/CColModel/RemoveTrianglePlanes_0040fa40.c)
void CColModel::RemoveTrianglePlanes() {
    if (m_pColData)
        m_pColData->RemoveTrianglePlanes();
}
