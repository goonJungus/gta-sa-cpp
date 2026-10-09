// CModelInfo - model info registry (indexed by model ID).
// Adapted from gta-reversed/source/game_sa/Models/ModelInfo.h for the
// clean-room C++ build. Decompiled reference: src/CModelInfo/*.c
// (GTA SA 1.0 @ addresses noted per method in CModelInfo.cpp).
//
// Adaptations:
// - StaticRef<T>(addr) statics -> plain static data members, defined in
//   src/CModelInfo.cpp (original GTA SA 1.0 addresses kept as comments).
// - CStore<T,N> (plugin-sdk container) -> minimal local template below.
//   TODO: replace with the real CStore when the containers subsystem ports it.
// - CKeyGen/CTempColModels -> PORT declarations in CModelInfo.cpp.
#pragma once

#include <cstdint>
#include "CBaseModelInfo.h"
#include "CAtomicModelInfo.h"
#include "CClumpModelInfo.h"
#include "CVehicleModelInfo.h"
#include "CPedModelInfo.h"
#include "CWeaponModelInfo.h"
#include "CTimeModelInfo.h"

// ---- PORT(containers): minimal CStore. Replace with the real CStore when the
//      containers subsystem ports it. Only the members CModelInfo uses.
template <typename T, int32_t N>
class CStore {
public:
    T       m_aStore[N];
    int32_t m_nCount = 0;

    T& AddItem() {
        // Decomp does not bounds-check; keep the same behaviour.
        return m_aStore[m_nCount++];
    }
    T& GetItemAtIndex(int32_t index) { return m_aStore[index]; }
    void Clear() { m_nCount = 0; }
};

class CDamageAtomicModelInfo;
class CLodAtomicModelInfo;
class CLodTimeModelInfo;
class C2dEffect;

// ---- PORT(models): the damage/LOD atomic, LOD time, and 2d-effect classes
//      are not ported yet. Minimal stand-ins so CStore can instantiate;
//      replace with the real headers when they land. Sizes are NOT validated.
class CDamageAtomicModelInfo : public CAtomicModelInfo {
public:
    uint8_t _pad[0x40];
};
class CLodAtomicModelInfo : public CAtomicModelInfo {
public:
    uint8_t _pad[0x40];
};
class CLodTimeModelInfo : public CTimeModelInfo {
public:
    uint8_t _pad[0x40];
};
class C2dEffect {
public:
    uint8_t _pad[0x40]; // decomp: 2dFX store entries are 0x40 bytes
    void Shutdown() {}
};

class CModelInfo {
public:
    static constexpr int32_t NUM_MODEL_INFOS = 20000; // DFF model ids 0..19999

    static CBaseModelInfo* ms_modelInfoPtrs[NUM_MODEL_INFOS]; // 0xA9B0C8
    static int32_t         ms_lastPositionSearched;          // 0xAAE948

    static constexpr int32_t NUM_ATOMIC_MODEL_INFOS = 14000;
    static CStore<CAtomicModelInfo, NUM_ATOMIC_MODEL_INFOS> ms_atomicModelInfoStore; // 0xAAE950

    static constexpr int32_t NUM_DAMAGE_ATOMIC_MODEL_INFOS = 70;
    static CStore<CDamageAtomicModelInfo, NUM_DAMAGE_ATOMIC_MODEL_INFOS> ms_damageAtomicModelInfoStore; // 0xB1BF58

    static constexpr int32_t NUM_LOD_ATOMIC_MODEL_INFOS = 1;
    static CStore<CLodAtomicModelInfo, NUM_LOD_ATOMIC_MODEL_INFOS> ms_lodAtomicModelInfoStore; // 0xB1C934

    static constexpr int32_t NUM_TIME_MODEL_INFOS = 169;
    static CStore<CTimeModelInfo, NUM_TIME_MODEL_INFOS> ms_timeModelInfoStore; // 0xB1C960

    static constexpr int32_t NUM_LOD_TIME_MODEL_INFOS = 1;
    static CStore<CLodTimeModelInfo, NUM_LOD_TIME_MODEL_INFOS> ms_lodTimeModelInfoStore; // 0xB1E128

    static constexpr int32_t NUM_WEAPON_MODEL_INFOS = 51;
    static CStore<CWeaponModelInfo, NUM_WEAPON_MODEL_INFOS> ms_weaponModelInfoStore; // 0xB1E158

    static constexpr int32_t NUM_CLUMP_MODEL_INFOS = 92;
    static CStore<CClumpModelInfo, NUM_CLUMP_MODEL_INFOS> ms_clumpModelInfoStore; // 0xB1E958

    static constexpr int32_t NUM_VEHICLE_MODEL_INFOS = 212;
    static CStore<CVehicleModelInfo, NUM_VEHICLE_MODEL_INFOS> ms_vehicleModelInfoStore; // 0xB1F650

    static constexpr int32_t NUM_PED_MODEL_INFOS = 278;
    static CStore<CPedModelInfo, NUM_PED_MODEL_INFOS> ms_pedModelInfoStore; // 0xB478F8

    static constexpr int32_t NUM_2DFX_INFOS = 100;
    static CStore<C2dEffect, NUM_2DFX_INFOS> ms_2dFXInfoStore; // 0xB4C2D8

public:
    static void Initialise();   // 0x4C6810
    static void ShutDown();     // 0x4C63E0
    static void ReInit2dEffects(); // 0x4C63B0

    static CAtomicModelInfo*       AddAtomicModel(int32_t index);       // 0x4C6620
    static CDamageAtomicModelInfo* AddDamageAtomicModel(int32_t index); // 0x4C6650
    static CLodAtomicModelInfo*    AddLodAtomicModel(int32_t index);    // 0x4C6680
    static CTimeModelInfo*         AddTimeModel(int32_t index);         // 0x4C66B0
    static CLodTimeModelInfo*      AddLodTimeModel(int32_t index);      // 0x4C66E0
    static CWeaponModelInfo*       AddWeaponModel(int32_t index);       // 0x4C6710
    static CClumpModelInfo*        AddClumpModel(int32_t index);        // 0x4C6740
    static CVehicleModelInfo*      AddVehicleModel(int32_t index);      // 0x4C6770
    static CPedModelInfo*          AddPedModel(int32_t index);          // 0x4C67A0

    static CBaseModelInfo* GetModelInfo(const char* name, int32_t* index = nullptr); // 0x4C5940
    static CBaseModelInfo* GetModelInfo(const char* name, int32_t minIndex, int32_t maxIndex); // 0x4C5A20
    static CBaseModelInfo* GetModelInfoFromHashKey(uint32_t uiHash, int32_t* index = nullptr); // 0x4C59B0
    static CBaseModelInfo* GetModelInfoUInt16(const char* name, uint16_t* pOutIndex = nullptr); // 0x4C59F0
    static CStore<C2dEffect, NUM_2DFX_INFOS>* Get2dEffectStore(); // 0x4C5A60

    static bool IsBoatModel(int32_t index);         // 0x4C5A70
    static bool IsCarModel(int32_t index);          // 0x4C5AA0
    static bool IsTrainModel(int32_t index);        // 0x4C5AD0
    static bool IsHeliModel(int32_t index);         // 0x4C5B00
    static bool IsPlaneModel(int32_t index);        // 0x4C5B30
    static bool IsBikeModel(int32_t index);         // 0x4C5B60
    static bool IsFakePlaneModel(int32_t index);    // 0x4C5B90
    static bool IsMonsterTruckModel(int32_t index); // 0x4C5BC0
    static bool IsQuadBikeModel(int32_t index);     // 0x4C5BF0
    static bool IsBmxModel(int32_t index);          // 0x4C5C20
    static bool IsTrailerModel(int32_t index);      // 0x4C5C50
    static int32_t IsVehicleModelType(int32_t index); // 0x4C5C80 (-1 if not a vehicle)

    static CBaseModelInfo* GetModelInfo(int32_t index) { return ms_modelInfoPtrs[index]; }
    static CPedModelInfo* GetPedModelInfo(int32_t index) { return GetModelInfo(index)->AsPedModelInfoPtr(); }
    static CVehicleModelInfo* GetVehicleModelInfo(int32_t index) { return GetModelInfo(index)->AsVehicleModelInfoPtr(); }
    static void SetModelInfo(int32_t index, CBaseModelInfo* pInfo) { ms_modelInfoPtrs[index] = pInfo; }
};
