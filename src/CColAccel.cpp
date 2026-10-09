// CColAccel - adapted from gta-reversed for clean-room C++ build
// Method stubs. Decompiled reference: src/CColAccel/*.c
// Each method below corresponds to a decompiled function - fill in from the .c file noted.

#include "CColAccel.h"
#include "CColStore.h" // ColDef (complete type needed for by-value params)

// Static member definitions.
// Original GTA SA 1.0 addresses (from gta-reversed StaticRef) kept as comments.
// TODO: re-resolve these for the clean-room build.
CColAccelColBound* CColAccel::m_colBounds      = nullptr; // 0xBC4090
IplDef*            CColAccel::m_iplDefs         = nullptr; // 0xBC4094
int32_t*           CColAccel::m_iSectionSize   = nullptr; // 0xBC4098
int32_t            CColAccel::m_iCachingColSize = 0;      // 0xBC409C
eColAccelState     CColAccel::m_iCacheState    = COLACCEL_ENDED; // 0xBC40A0
CColAccelColEntry* CColAccel::mp_caccColItems  = nullptr; // 0xBC40A4
int32_t            CColAccel::m_iNumColItems   = 0;       // 0xBC40A8
CColAccelIPLEntry* CColAccel::mp_caccIPLItems  = nullptr; // 0xBC40AC
int32_t            CColAccel::m_iNumIPLItems   = 0;       // 0xBC40B0
int32_t            CColAccel::m_iNumSections   = 0;       // 0xBC40B4
int32_t            CColAccel::m_iNumColBounds  = 0;       // 0xBC40B8
const char*        CColAccel::mp_cCacheName    = nullptr;

bool CColAccel::isCacheLoading() {
    // TODO: src/CColAccel/isCacheLoading_005b2ac0.c
    return false;
}

void CColAccel::startCache() {
    // TODO: src/CColAccel/startCache_005b31a0.c
}

void CColAccel::endCache() {
    // TODO: src/CColAccel/endCache_005b2ad0.c
}

void CColAccel::addCacheCol(PackedModelStartEnd startEnd, const CColModel& colModel) {
    // TODO: src/CColAccel/addCacheCol_005b2c20.c
}

void CColAccel::cacheLoadCol() {
    // TODO: src/CColAccel/cacheLoadCol_005b2cc0.c
}

void CColAccel::addColDef(ColDef colDef) {
    // TODO: src/CColAccel/addColDef_005b2dd0.c
}

void CColAccel::getColDef(ColDef& colDef) {
    // TODO: src/CColAccel/getColDef_005b2e60.c
}

void CColAccel::setIplDef(int32_t iplIndex, IplDef iplDef) {
    // TODO: src/CColAccel/setIplDef_005b2ed0.c
}

IplDef CColAccel::getIplDef(int32_t iplIndex) {
    // TODO: src/CColAccel/getIplDef_005b2ef0.c
    return IplDef{};
}

void CColAccel::cacheIPLSection(CEntity** ppEntities, int32_t entitiesCount) {
    // TODO: src/CColAccel/cacheIPLSection_005b2f10.c
}

void CColAccel::addIPLEntity(CEntity** ppEntities, int32_t entitiesCount, int32_t entityIndex) {
    // TODO: src/CColAccel/addIPLEntity_005b3040.c
}
