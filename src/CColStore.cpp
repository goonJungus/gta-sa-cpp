// CColStore.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CColStore/*.c
// Stub implementations - fill in logic from the decompiled .c files noted.

#include "CColStore.h"

// Static data members (game addresses recorded from gta-reversed StaticRef)
CVector CColStore::ms_vecCollisionNeeded{}; // game address: 0x965580
bool CColStore::ms_bCollisionNeeded{}; // game address: 0x965558
eAreaCodesS32 CColStore::ms_EntityAreaCode{}; // game address: 0x965554

void* operator CColStore::new(unsigned size) {
    // TODO: decomp src/CColStore/new_*.c
    (void)size;
    return nullptr;
}

void operator CColStore::delete(void* data) {
    // TODO: decomp src/CColStore/delete_*.c
    (void)data;
    // TODO: choose return value for void operator
    return {}; // TODO: verify
}

void CColStore::Initialise() {
    // TODO: decomp src/CColStore/Initialise_*.c
}

void CColStore::Shutdown() {
    // TODO: decomp src/CColStore/Shutdown_*.c
}

int32 CColStore::AddColSlot(const char* name) {
    // TODO: decomp src/CColStore/AddColSlot_*.c
    (void)name;
    return 0;
}

void CColStore::AddCollisionNeededAtPosn(const CVector& pos) {
    // TODO: decomp src/CColStore/AddCollisionNeededAtPosn_*.c
    (void)pos;
}

void CColStore::AddRef(int32 colNum) {
    // TODO: decomp src/CColStore/AddRef_*.c
    (void)colNum;
}

int32 CColStore::FindColSlot(const char*) {
    // TODO: decomp src/CColStore/FindColSlot_*.c
    (void)char;
    return 0;
}

void CColStore::BoundingBoxesPostProcess() {
    // TODO: decomp src/CColStore/BoundingBoxesPostProcess_*.c
}

void CColStore::EnsureCollisionIsInMemory(const CVector& pos) {
    // TODO: decomp src/CColStore/EnsureCollisionIsInMemory_*.c
    (void)pos;
}

CRect* CColStore::GetBoundingBox(int32 colSlot) {
    // TODO: decomp src/CColStore/GetBoundingBox_*.c
    (void)colSlot;
    return nullptr;
}

void CColStore::IncludeModelIndex(int32 colSlot, int32 modelId) {
    // TODO: decomp src/CColStore/IncludeModelIndex_*.c
    (void)colSlot;
    (void)modelId;
}

bool CColStore::HasCollisionLoaded(const CVector& pos, eAreaCodes areaCode) {
    // TODO: decomp src/CColStore/HasCollisionLoaded_*.c
    (void)pos;
    (void)areaCode;
    return false;
}

void CColStore::LoadAllBoundingBoxes() {
    // TODO: decomp src/CColStore/LoadAllBoundingBoxes_*.c
}

void CColStore::LoadAllCollision() {
    // TODO: decomp src/CColStore/LoadAllCollision_*.c
}

void CColStore::LoadCol(int32 colSlot, const char* filename) {
    // TODO: decomp src/CColStore/LoadCol_*.c
    (void)colSlot;
    (void)filename;
}

bool CColStore::LoadCol(int32 colSlot, uint8* data, int32 dataSize) {
    // TODO: decomp src/CColStore/LoadCol_*.c
    (void)colSlot;
    (void)data;
    (void)dataSize;
    return false;
}

void CColStore::LoadCollision(CVector pos, bool bIgnorePlayerVeh) {
    // TODO: decomp src/CColStore/LoadCollision_*.c
    (void)pos;
    (void)bIgnorePlayerVeh;
}

void CColStore::RemoveAllCollision() {
    // TODO: decomp src/CColStore/RemoveAllCollision_*.c
}

void CColStore::RemoveCol(int32 colSlot) {
    // TODO: decomp src/CColStore/RemoveCol_*.c
    (void)colSlot;
}

void CColStore::RemoveColSlot(int32 colSlot) {
    // TODO: decomp src/CColStore/RemoveColSlot_*.c
    (void)colSlot;
}

void CColStore::RemoveRef(int32 colNum) {
    // TODO: decomp src/CColStore/RemoveRef_*.c
    (void)colNum;
}

void CColStore::RequestCollision(const CVector& pos, eAreaCodes areaCode) {
    // TODO: decomp src/CColStore/RequestCollision_*.c
    (void)pos;
    (void)areaCode;
}

void CColStore::SetCollisionRequired(const CVector& pos, eAreaCodes areaCode) {
    // TODO: decomp src/CColStore/SetCollisionRequired_*.c
    (void)pos;
    (void)areaCode;
}

ColDef* CColStore::GetInSlot(int32 slot) {
    // TODO: decomp src/CColStore/GetInSlot_*.c
    (void)slot;
    return nullptr;
}

CColPool* CColStore::GetPool() {
    // TODO: decomp src/CColStore/GetPool_*.c
    return nullptr;
}
