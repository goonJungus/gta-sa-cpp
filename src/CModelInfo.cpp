// CModelInfo - model info registry implementation.
// Adapted from gta-reversed/source/game_sa/Models/ModelInfo.cpp for the
// clean-room C++ build. Decompiled reference: src/CModelInfo/*.c
// (GTA SA 1.0 @ addresses noted per method).
//
// Adaptations: InjectHooks() stripped (plugin-sdk hooking, not needed for
//   clean-room); StaticRef<T>(addr) statics -> plain static data members
//   below (original GTA SA 1.0 addresses kept as comments); CKeyGen /
//   CTempColModels -> PORT declarations (subsystems not ported yet).

#include "CModelInfo.h"

#include <cstring>

// ---- PORT(keygen): CKeyGen not ported yet.
class CKeyGen {
public:
    static uint32_t GetUppercaseKey(const char* str);
};

// ---- PORT(collision): CTempColModels not ported yet.
class CColModel;
class CTempColModels {
public:
    static CColModel ms_colModelDoor1;
    static CColModel ms_colModelBumper1;
    static CColModel ms_colModelPanel1;
    static CColModel ms_colModelBonnet1;
    static CColModel ms_colModelBoot1;
    static CColModel ms_colModelWheel1;
    static CColModel ms_colModelBodyPart1;
    static CColModel ms_colModelBodyPart2;
};

// ---- PORT(models): temp-collision model IDs (eModelID.h not ported yet).
//      Values cross-checked against Initialise @ 0x4C6810 (0x176..0x17d).
static constexpr int32_t MODEL_TEMPCOL_DOOR1     = 0x176;
static constexpr int32_t MODEL_TEMPCOL_BUMPER1   = 0x177;
static constexpr int32_t MODEL_TEMPCOL_PANEL1   = 0x178;
static constexpr int32_t MODEL_TEMPCOL_BONNET1   = 0x179;
static constexpr int32_t MODEL_TEMPCOL_BOOT1     = 0x17A;
static constexpr int32_t MODEL_TEMPCOL_WHEEL1    = 0x17B;
static constexpr int32_t MODEL_TEMPCOL_BODYPART1 = 0x17C;
static constexpr int32_t MODEL_TEMPCOL_BODYPART2 = 0x17D;

// Static member definitions (original GTA SA 1.0 addresses as comments).
CBaseModelInfo* CModelInfo::ms_modelInfoPtrs[NUM_MODEL_INFOS]; // 0xA9B0C8
int32_t         CModelInfo::ms_lastPositionSearched;          // 0xAAE948
CStore<CAtomicModelInfo, CModelInfo::NUM_ATOMIC_MODEL_INFOS> CModelInfo::ms_atomicModelInfoStore; // 0xAAE950
CStore<CDamageAtomicModelInfo, CModelInfo::NUM_DAMAGE_ATOMIC_MODEL_INFOS> CModelInfo::ms_damageAtomicModelInfoStore; // 0xB1BF58
CStore<CLodAtomicModelInfo, CModelInfo::NUM_LOD_ATOMIC_MODEL_INFOS> CModelInfo::ms_lodAtomicModelInfoStore; // 0xB1C934
CStore<CTimeModelInfo, CModelInfo::NUM_TIME_MODEL_INFOS> CModelInfo::ms_timeModelInfoStore; // 0xB1C960
CStore<CLodTimeModelInfo, CModelInfo::NUM_LOD_TIME_MODEL_INFOS> CModelInfo::ms_lodTimeModelInfoStore; // 0xB1E128
CStore<CWeaponModelInfo, CModelInfo::NUM_WEAPON_MODEL_INFOS> CModelInfo::ms_weaponModelInfoStore; // 0xB1E158
CStore<CClumpModelInfo, CModelInfo::NUM_CLUMP_MODEL_INFOS> CModelInfo::ms_clumpModelInfoStore; // 0xB1E958
CStore<CVehicleModelInfo, CModelInfo::NUM_VEHICLE_MODEL_INFOS> CModelInfo::ms_vehicleModelInfoStore; // 0xB1F650
CStore<CPedModelInfo, CModelInfo::NUM_PED_MODEL_INFOS> CModelInfo::ms_pedModelInfoStore; // 0xB478F8
CStore<C2dEffect, CModelInfo::NUM_2DFX_INFOS> CModelInfo::ms_2dFXInfoStore; // 0xB4C2D8

// 0x4C63B0
void CModelInfo::ReInit2dEffects()
{
    ms_2dFXInfoStore.m_nCount = 0;
    for (int32_t i = 0; i < NUM_MODEL_INFOS; ++i)
        GetModelInfo(i)->Init2dEffects();
}

// 0x4C63E0
void CModelInfo::ShutDown()
{
    for (int32_t i = 0; i < ms_atomicModelInfoStore.m_nCount; ++i)
        ms_atomicModelInfoStore.GetItemAtIndex(i).Shutdown();

    for (int32_t i = 0; i < ms_damageAtomicModelInfoStore.m_nCount; ++i)
        ms_damageAtomicModelInfoStore.GetItemAtIndex(i).Shutdown();

    for (int32_t i = 0; i < ms_lodAtomicModelInfoStore.m_nCount; ++i)
        ms_lodAtomicModelInfoStore.GetItemAtIndex(i).Shutdown();

    for (int32_t i = 0; i < ms_timeModelInfoStore.m_nCount; ++i)
        ms_timeModelInfoStore.GetItemAtIndex(i).Shutdown();

    for (int32_t i = 0; i < ms_lodTimeModelInfoStore.m_nCount; ++i)
        ms_lodTimeModelInfoStore.GetItemAtIndex(i).Shutdown();

    for (int32_t i = 0; i < ms_weaponModelInfoStore.m_nCount; ++i)
        ms_weaponModelInfoStore.GetItemAtIndex(i).Shutdown();

    for (int32_t i = 0; i < ms_clumpModelInfoStore.m_nCount; ++i)
        ms_clumpModelInfoStore.GetItemAtIndex(i).Shutdown();

    for (int32_t i = 0; i < ms_vehicleModelInfoStore.m_nCount; ++i)
        ms_vehicleModelInfoStore.GetItemAtIndex(i).Shutdown();

    for (int32_t i = 0; i < ms_pedModelInfoStore.m_nCount; ++i)
        ms_pedModelInfoStore.GetItemAtIndex(i).Shutdown();

    for (int32_t i = 0; i < ms_2dFXInfoStore.m_nCount; ++i)
        ms_2dFXInfoStore.GetItemAtIndex(i).Shutdown();

    ms_atomicModelInfoStore.Clear();
    ms_damageAtomicModelInfoStore.Clear();
    ms_lodAtomicModelInfoStore.Clear();
    ms_timeModelInfoStore.Clear();
    ms_lodTimeModelInfoStore.Clear();
    ms_weaponModelInfoStore.Clear();
    ms_clumpModelInfoStore.Clear();
    ms_vehicleModelInfoStore.Clear();
    ms_pedModelInfoStore.Clear();
    ms_2dFXInfoStore.Clear();
}

// 0x4C6620
CAtomicModelInfo* CModelInfo::AddAtomicModel(int32_t index)
{
    auto& mi = ms_atomicModelInfoStore.AddItem();
    mi.Init();
    SetModelInfo(index, &mi);
    return &mi;
}

// 0x4C6650
CDamageAtomicModelInfo* CModelInfo::AddDamageAtomicModel(int32_t index)
{
    auto& mi = ms_damageAtomicModelInfoStore.AddItem();
    mi.Init();
    SetModelInfo(index, &mi);
    return &mi;
}

// 0x4C6680
CLodAtomicModelInfo* CModelInfo::AddLodAtomicModel(int32_t index)
{
    auto& mi = ms_lodAtomicModelInfoStore.AddItem();
    mi.Init();
    SetModelInfo(index, &mi);
    return &mi;
}

// 0x4C66B0
CTimeModelInfo* CModelInfo::AddTimeModel(int32_t index)
{
    auto& mi = ms_timeModelInfoStore.AddItem();
    mi.Init();
    SetModelInfo(index, &mi);
    return &mi;
}

// 0x4C66E0
CLodTimeModelInfo* CModelInfo::AddLodTimeModel(int32_t index)
{
    auto& mi = ms_lodTimeModelInfoStore.AddItem();
    mi.Init();
    SetModelInfo(index, &mi);
    return &mi;
}

// 0x4C6710
CWeaponModelInfo* CModelInfo::AddWeaponModel(int32_t index)
{
    auto& mi = ms_weaponModelInfoStore.AddItem();
    mi.Init();
    SetModelInfo(index, &mi);
    return &mi;
}

// 0x4C6740
CClumpModelInfo* CModelInfo::AddClumpModel(int32_t index)
{
    auto& mi = ms_clumpModelInfoStore.AddItem();
    mi.Init();
    SetModelInfo(index, &mi);
    return &mi;
}

// 0x4C6770
CVehicleModelInfo* CModelInfo::AddVehicleModel(int32_t index)
{
    auto& mi = ms_vehicleModelInfoStore.AddItem();
    mi.Init();
    SetModelInfo(index, &mi);
    return &mi;
}

// 0x4C67A0
CPedModelInfo* CModelInfo::AddPedModel(int32_t index)
{
    auto& mi = ms_pedModelInfoStore.AddItem();
    mi.Init();
    SetModelInfo(index, &mi);
    return &mi;
}

// 0x4C6810
void CModelInfo::Initialise()
{
    memset(ms_modelInfoPtrs, 0, sizeof(ms_modelInfoPtrs));
    ms_damageAtomicModelInfoStore.Clear();
    ms_lodAtomicModelInfoStore.Clear();
    ms_timeModelInfoStore.Clear();
    ms_lodTimeModelInfoStore.Clear();
    ms_weaponModelInfoStore.Clear();
    ms_clumpModelInfoStore.Clear();
    ms_vehicleModelInfoStore.Clear();
    ms_pedModelInfoStore.Clear();
    ms_2dFXInfoStore.Clear();
    ms_atomicModelInfoStore.Clear();

    auto door1 = AddAtomicModel(MODEL_TEMPCOL_DOOR1);
    door1->SetColModel(&CTempColModels::ms_colModelDoor1, false);
    door1->SetTexDictionary("generic");
    door1->m_fDrawDistance = 80.0F;

    auto bumper1 = AddAtomicModel(MODEL_TEMPCOL_BUMPER1);
    bumper1->SetColModel(&CTempColModels::ms_colModelBumper1, false);
    bumper1->SetTexDictionary("generic");
    bumper1->m_fDrawDistance = 80.0F;

    auto modelPanel1 = AddAtomicModel(MODEL_TEMPCOL_PANEL1);
    modelPanel1->SetColModel(&CTempColModels::ms_colModelPanel1, false);
    modelPanel1->SetTexDictionary("generic");
    modelPanel1->m_fDrawDistance = 80.0F;

    auto bonnet1 = AddAtomicModel(MODEL_TEMPCOL_BONNET1);
    bonnet1->SetColModel(&CTempColModels::ms_colModelBonnet1, false);
    bonnet1->SetTexDictionary("generic");
    bonnet1->m_fDrawDistance = 80.0F;

    auto boot1 = AddAtomicModel(MODEL_TEMPCOL_BOOT1);
    boot1->SetColModel(&CTempColModels::ms_colModelBoot1, false);
    boot1->SetTexDictionary("generic");
    boot1->m_fDrawDistance = 80.0F;

    auto wheel1 = AddAtomicModel(MODEL_TEMPCOL_WHEEL1);
    wheel1->SetColModel(&CTempColModels::ms_colModelWheel1, false);
    wheel1->SetTexDictionary("generic");
    wheel1->m_fDrawDistance = 80.0F;

    auto bodyPart1 = AddAtomicModel(MODEL_TEMPCOL_BODYPART1);
    bodyPart1->SetColModel(&CTempColModels::ms_colModelBodyPart1, false);
    bodyPart1->SetTexDictionary("generic");
    bodyPart1->m_fDrawDistance = 80.0F;

    auto bodyPart2 = AddAtomicModel(MODEL_TEMPCOL_BODYPART2);
    bodyPart2->SetColModel(&CTempColModels::ms_colModelBodyPart2, false);
    bodyPart2->SetTexDictionary("generic");
    bodyPart2->m_fDrawDistance = 80.0F;
}

// 0x4C5940
CBaseModelInfo* CModelInfo::GetModelInfo(const char* name, int32_t* index)
{
    auto iKey = CKeyGen::GetUppercaseKey(name);
    auto iCurInd = ms_lastPositionSearched;

    while (iCurInd < NUM_MODEL_INFOS) {
        auto mi = GetModelInfo(iCurInd);
        if (mi && mi->m_nKey == iKey) {
            ms_lastPositionSearched = iCurInd;
            if (index)
                *index = iCurInd;

            return mi;
        }

        ++iCurInd;
    }

    iCurInd = ms_lastPositionSearched;
    if (iCurInd < 0)
        return nullptr;

    while (iCurInd >= 0) {
        auto mi = GetModelInfo(iCurInd);
        if (mi && mi->m_nKey == iKey) {
            ms_lastPositionSearched = iCurInd;
            if (index)
                *index = iCurInd;

            return mi;
        }

        --iCurInd;
    }

    return nullptr;
}

// 0x4C59B0
CBaseModelInfo* CModelInfo::GetModelInfoFromHashKey(uint32_t uiHash, int32_t* index)
{
    for (int32_t i = 0; i < NUM_MODEL_INFOS; ++i) {
        auto mi = GetModelInfo(i);
        if (mi && mi->m_nKey == uiHash) {
            if (index)
                *index = i;

            return mi;
        }
    }

    return nullptr;
}

// 0x4C59F0
CBaseModelInfo* CModelInfo::GetModelInfoUInt16(const char* name, uint16_t* pOutIndex)
{
    int32_t modelId = 0;
    auto result = GetModelInfo(name, &modelId);
    if (pOutIndex)
        *pOutIndex = static_cast<uint16_t>(modelId);

    return result;
}

// 0x4C5A20
CBaseModelInfo* CModelInfo::GetModelInfo(const char* name, int32_t minIndex, int32_t maxIndex)
{
    auto iKey = CKeyGen::GetUppercaseKey(name);
    if (minIndex > maxIndex)
        return nullptr;

    for (int32_t i = minIndex; i <= maxIndex; ++i) {
        auto mi = GetModelInfo(i);
        if (mi && mi->m_nKey == iKey)
            return mi;
    }

    return nullptr;
}

// 0x4C5A60
CStore<C2dEffect, CModelInfo::NUM_2DFX_INFOS>* CModelInfo::Get2dEffectStore()
{
    return &ms_2dFXInfoStore;
}

// 0x4C5A70
bool CModelInfo::IsBoatModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsBoat();
}

// 0x4C5AA0
bool CModelInfo::IsCarModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsAutomobile();
}

// 0x4C5AD0
bool CModelInfo::IsTrainModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsTrain();
}

// 0x4C5B00
bool CModelInfo::IsHeliModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsHeli();
}

// 0x4C5B30
bool CModelInfo::IsPlaneModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsPlane();
}

// 0x4C5B60
bool CModelInfo::IsBikeModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsBike();
}

// 0x4C5B90
bool CModelInfo::IsFakePlaneModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->m_nVehicleType == eVehicleType::VEHICLE_TYPE_FPLANE;
}

// 0x4C5BC0
bool CModelInfo::IsMonsterTruckModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsMonsterTruck();
}

// 0x4C5BF0
bool CModelInfo::IsQuadBikeModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsQuad();
}

// 0x4C5C20
bool CModelInfo::IsBmxModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsBMX();
}

// 0x4C5C50
bool CModelInfo::IsTrailerModel(int32_t index)
{
    auto mi = GetModelInfo(index);
    if (!mi)
        return false;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return false;

    return mi->AsVehicleModelInfoPtr()->IsTrailer();
}

// 0x4C5C80
int32_t CModelInfo::IsVehicleModelType(int32_t index)
{
    if (index >= NUM_MODEL_INFOS)
        return -1;

    auto mi = GetModelInfo(index);
    if (!mi)
        return -1;

    if (mi->GetModelType() != ModelInfoType::MODEL_INFO_VEHICLE)
        return -1;

    return static_cast<int32_t>(mi->AsVehicleModelInfoPtr()->m_nVehicleType);
}
