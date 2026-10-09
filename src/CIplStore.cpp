// CIplStore.cpp - GTA SA 1.0 clean-room C++ conversion
// Stub implementations - fill in logic from the decompiled .c files noted.

#include "CIplStore.h"

CEntity** ppCurrIplInstance{};
int32 NumIplEntityIndexArrays{};
std::array<CEntity**, 40> IplEntityIndexArrays{};
bool gbIplsNeededAtPosn{};
CVector gvecIplsNeededAtPosn{};
uint32 gNumLoadedBuildings{};
std::array<CEntity*, 4096> gpLoadedBuildings{};

void CIplStore::Initialise() {
    // TODO: decomp src/CIplStore/Initialise_00405ec0.c
}

void CIplStore::Shutdown() {
    // TODO: decomp src/CIplStore/Shutdown_015610b0.c
}

int32 CIplStore::AddIplSlot(const char* name) {
    // TODO: decomp src/CIplStore/AddIplSlot_0156c470.c
    (void)name;
    return 0;
}

void CIplStore::AddIplsNeededAtPosn(const CVector& posn) {
    // TODO: decomp src/CIplStore/AddIplsNeededAtPosn_01567820.c
    (void)posn;
}

void CIplStore::ClearIplsNeededAtPosn() {
    // TODO: decomp src/CIplStore/ClearIplsNeededAtPosn_0156cff0.c
}

void CIplStore::EnableDynamicStreaming(int32 iplSlotIndex, bool enable) {
    // TODO: decomp src/CIplStore/EnableDynamicStreaming_00404d30.c
    (void)iplSlotIndex;
    (void)enable;
}

void CIplStore::EnsureIplsAreInMemory(const CVector& posn) {
    // TODO: decomp src/CIplStore/EnsureIplsAreInMemory_004053f0.c
    (void)posn;
}

int32 CIplStore::FindIplSlot(const char* name) {
    // TODO: decomp src/CIplStore/FindIplSlot_00404ac0.c
    // NOTE: original returns -1 when not found
    (void)name;
    return -1;
}

CRect* CIplStore::GetBoundingBox(int32 iplSlotIndex) {
    // TODO: decomp src/CIplStore/GetBoundingBox_00404c70.c
    (void)iplSlotIndex;
    return nullptr;
}

CEntity** CIplStore::GetIplEntityIndexArray(int32 arrayIndex) {
    // TODO: decomp src/CIplStore/GetIplEntityIndexArray_01569770.c
    (void)arrayIndex;
    return nullptr;
}

const char* CIplStore::GetIplName(int32 iplSlotIndex) {
    // TODO: decomp src/CIplStore/GetIplName_00404a60.c
    (void)iplSlotIndex;
    return nullptr;
}

int32 CIplStore::GetNewIplEntityIndexArray(int32 entitiesCount) {
    // TODO: decomp src/CIplStore/GetNewIplEntityIndexArray_015649e0.c
    (void)entitiesCount;
    return 0;
}

bool CIplStore::HaveIplsLoaded(const CVector& coords, int32 playerNumber) {
    // TODO: decomp src/CIplStore/HaveIplsLoaded_00405600.c
    (void)coords;
    (void)playerNumber;
    return false;
}

void CIplStore::IncludeEntity(int32 iplSlotIndex, CEntity* entity) {
    // TODO: decomp src/CIplStore/IncludeEntity_01563730.c
    (void)iplSlotIndex;
    (void)entity;
}

void CIplStore::LoadAllRemainingIpls() {
    // TODO: decomp src/CIplStore/LoadAllRemainingIpls_00405780.c
}

bool CIplStore::LoadIpl(int32 iplSlotIndex, char* data, int32 dataSize) {
    // TODO: decomp src/CIplStore/LoadIpl_00406080.c
    (void)iplSlotIndex;
    (void)data;
    (void)dataSize;
    return false;
}

bool CIplStore::LoadIplBoundingBox(int32 iplSlotIndex, char* data, int32 dataSize) {
    // TODO: decomp src/CIplStore/LoadIplBoundingBox_00405c00.c
    (void)iplSlotIndex;
    (void)data;
    (void)dataSize;
    return false;
}

void CIplStore::LoadIpls(CVector posn, bool bAvoidLoadInPlayerVehicleMovingDirection) {
    // TODO: decomp src/CIplStore/LoadIpls_00405170.c
    (void)posn;
    (void)bAvoidLoadInPlayerVehicleMovingDirection;
}

void CIplStore::RemoveAllIpls() {
    // TODO: decomp src/CIplStore/RemoveAllIpls_00405720.c
}

void CIplStore::RemoveIpl(int32 iplSlotIndex) {
    // TODO: decomp src/CIplStore/RemoveIpl_00404b20.c
    (void)iplSlotIndex;
}

void CIplStore::RemoveIplAndIgnore(int32 iplSlotIndex) {
    // TODO: decomp src/CIplStore/RemoveIplAndIgnore_0156a140.c
    (void)iplSlotIndex;
}

void CIplStore::RemoveIplSlot(int32 iplSlotIndex) {
    // TODO: decomp src/CIplStore/RemoveIplSlot_0156c5f0.c
    (void)iplSlotIndex;
}

void CIplStore::RemoveIplWhenFarAway(int32 iplSlotIndex) {
    // TODO: decomp src/CIplStore/RemoveIplWhenFarAway_0156de30.c
    (void)iplSlotIndex;
}

void CIplStore::RemoveRelatedIpls(int32 entityArraysIndex) {
    // TODO: decomp src/CIplStore/RemoveRelatedIpls_00405110.c
    (void)entityArraysIndex;
}

void CIplStore::RequestIplAndIgnore(int32 iplSlotIndex) {
    // TODO: decomp src/CIplStore/RequestIplAndIgnore_01565190.c
    (void)iplSlotIndex;
}

void CIplStore::RequestIpls(const CVector& posn, int32 playerNumber) {
    // TODO: decomp src/CIplStore/RequestIpls_00405520.c
    (void)posn;
    (void)playerNumber;
}

void CIplStore::SetIplsRequired(const CVector& posn, int32 playerNumber) {
    // TODO: decomp src/CIplStore/SetIplsRequired_00404700.c
    (void)posn;
    (void)playerNumber;
}

void CIplStore::SetIsInterior(int32 iplSlotIndex, bool isInterior) {
    // TODO: decomp src/CIplStore/SetIsInterior_00404a90.c
    (void)iplSlotIndex;
    (void)isInterior;
}

int32 CIplStore::SetupRelatedIpls(const char* iplName, int32 entityArraysIndex, CEntity** instances) {
    // TODO: decomp src/CIplStore/SetupRelatedIpls_00404de0.c
    (void)iplName;
    (void)entityArraysIndex;
    (void)instances;
    return 0;
}

bool CIplStore::Save() {
    // TODO: decomp src/CIplStore/Save_005d5420.c
    return false;
}

bool CIplStore::Load() {
    // TODO: decomp src/CIplStore/Load_005d54a0.c
    return false;
}

IplDef* CIplStore::GetInSlot(int32 slot) {
    // TODO: decomp src/CIplStore/ - no Install entry; likely get_0x8_004047d0_004047d0.c
    (void)slot;
    return nullptr;
}

CIplPool* CIplStore::GetPool() {
    // TODO: decomp src/CIplStore/ - no Install entry and no matching .c; verify address
    return nullptr;
}
