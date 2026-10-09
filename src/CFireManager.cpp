// CFireManager - adapted from gta-reversed for clean-room C++ build
// Method stubs. Decompiled reference: src/CFireManager/*.c
// Each method below corresponds to a decompiled function - fill in from the .c file noted.

#include "CFireManager.h"

#include <new> // placement new in Constructor()

// (was StaticRef<CFireManager>(0xB71F80))
CFireManager gFireManager{};

CFireManager::CFireManager() {
    // TODO: src/CFireManager/Constructor_00539da0.c
    // (exporter named it "Constructor"; may be the notsa Constructor() helper at 0x539DA0)
}

CFireManager* CFireManager::Constructor() {
    // TODO: src/CFireManager/Constructor_00539da0.c
    return new (this) CFireManager();
}

CFireManager* CFireManager::Destructor() {
    // TODO: src/CFireManager/Destructor_00538bb0.c
    this->~CFireManager();
    return this;
}

void CFireManager::Init() {
    // TODO: src/CFireManager/Init_00538bc0.c
}

void CFireManager::Shutdown() {
    // TODO: src/CFireManager/Shutdown_00539dd0.c
}

uint32_t CFireManager::GetNumOfNonScriptFires() {
    // TODO: src/CFireManager/GetNumOfNonScriptFires_00538f10.c
    return 0;
}

CFire* CFireManager::FindNearestFire(const CVector& point, bool bCheckWasExtinguished, bool bCheckWasCreatedByScript) {
    // TODO: src/CFireManager/FindNearestFire_00538f40.c
    (void)point;
    (void)bCheckWasExtinguished;
    (void)bCheckWasCreatedByScript;
    return nullptr;
}

bool CFireManager::PlentyFiresAvailable() {
    // TODO: src/CFireManager/PlentyFiresAvailable_00539340.c
    return false;
}

void CFireManager::ExtinguishPoint(CVector point, float fRadiusSq) {
    // TODO: src/CFireManager/ExtinguishPoint_00539450.c
    (void)point;
    (void)fRadiusSq;
}

bool CFireManager::ExtinguishPointWithWater(CVector point, float fRadiusSq, float fFireSize) {
    // TODO: src/CFireManager/ExtinguishPointWithWater_005394c0.c
    (void)point;
    (void)fRadiusSq;
    (void)fFireSize;
    return false;
}

bool CFireManager::IsScriptFireExtinguished(int16_t id) {
    // TODO: src/CFireManager/IsScriptFireExtinguished_005396e0.c
    (void)id;
    return false;
}

void CFireManager::RemoveScriptFire(uint16_t fireId) {
    // TODO: src/CFireManager/RemoveScriptFire_00539700.c
    (void)fireId;
}

void CFireManager::RemoveAllScriptFires() {
    // TODO: src/CFireManager/RemoveAllScriptFires_00539720.c
}

void CFireManager::ClearAllScriptFireFlags() {
    // TODO: src/CFireManager/ClearAllScriptFireFlags_005397a0.c
}

void CFireManager::SetScriptFireAudio(int16_t fireId, bool bFlag) {
    // TODO: src/CFireManager/SetScriptFireAudio_005397b0.c
    (void)fireId;
    (void)bFlag;
}

const CVector& CFireManager::GetScriptFireCoords(int16_t fireId) {
    // TODO: src/CFireManager/GetScriptFireCoords_005397e0.c
    // LOUD TODO: static dummy - do not call until implemented
    (void)fireId;
    static const CVector dummy{};
    return dummy;
}

uint32_t CFireManager::GetNumFiresInRange(const CVector& point, float fRadiusSq) {
    // TODO: src/CFireManager/GetNumFiresInRange_005397f0.c
    (void)point;
    (void)fRadiusSq;
    return 0;
}

uint32_t CFireManager::GetNumFiresInArea(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
    // TODO: src/CFireManager/GetNumFiresInArea_00539860.c
    (void)minX;
    (void)minY;
    (void)minZ;
    (void)maxX;
    (void)maxY;
    (void)maxZ;
    return 0;
}

CFire* CFireManager::GetNextFreeFire(bool bMayExtinguish) {
    // TODO: src/CFireManager/GetNextFreeFire_00539e50.c
    (void)bMayExtinguish;
    return nullptr;
}

void CFireManager::CreateAllFxSystems() {
    // TODO: src/CFireManager/CreateAllFxSystems_00539d50.c
}

void CFireManager::DestroyAllFxSystems() {
    // TODO: src/CFireManager/DestroyAllFxSystems_00539d10.c
}

CFire* CFireManager::StartFire(CVector pos, float size, uint8_t unused, CEntity* creator, uint32_t nTimeToBurn, int8_t nGenerations, uint8_t unused_) {
    // TODO: src/CFireManager/StartFire_00539f00.c or StartFire_0053a050.c (two overloads, two .c files - match signatures)
    (void)pos;
    (void)size;
    (void)unused;
    (void)creator;
    (void)nTimeToBurn;
    (void)nGenerations;
    (void)unused_;
    return nullptr;
}

CFire* CFireManager::StartFire(CEntity* target, CEntity* creator, float size, uint8_t arg3, uint32_t time, int8_t numGenerations) {
    // TODO: src/CFireManager/StartFire_00539f00.c or StartFire_0053a050.c (two overloads, two .c files - match signatures)
    (void)target;
    (void)creator;
    (void)size;
    (void)arg3;
    (void)time;
    (void)numGenerations;
    return nullptr;
}

int32_t CFireManager::StartScriptFire(const CVector& point, CEntity* target, float arg2, uint8_t arg3, int8_t numGenerations, int32_t size) {
    // TODO: src/CFireManager/StartScriptFire_0053a270.c
    (void)point;
    (void)target;
    (void)arg2;
    (void)arg3;
    (void)numGenerations;
    (void)size;
    return 0;
}

void CFireManager::Update() {
    // TODO: src/CFireManager/Update_0053af00.c
}

uint32_t CFireManager::GetNumOfFires() {
    // TODO: no named .c file in src/CFireManager - identify from binary
    return 0;
}

CFire& CFireManager::GetRandomFire() {
    // TODO: no named .c file in src/CFireManager - identify from binary
    // LOUD TODO: returns first slot - do not call until implemented
    return m_aFires[0];
}
