// CVehicle.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CVehicle/*.c
// Filled method bodies adapted from gta-reversed
// (source/game_sa/Entity/Vehicle/Vehicle.cpp), verified against the decomp.
// Remaining stubs keep their `// TODO: decomp src/CVehicle/*.c` markers.

#include "CVehicle.h"
#include "CColModel.h"
#include "tHandlingData.h"
#include "CTimer.h"
#include "CWorld.h"
#include "CModelInfo.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CTrain.h"
#include "ePedType.h"
#include "eWeaponType.h"
#include "eCarWheel.h"

// TODO(port): CRadar.h not included - it pulls RenderTypes.h, whose CRGBA
// redefines CVehicleModelInfo.h's CRGBA (pre-existing tree issue).
// Values verified from gta-reversed (Radar.h).
enum class eBlipType : uint8 {
    BLIP_CAR = 1,
};
class CRadar {
public:
    static void ClearBlipForEntity(eBlipType blipType, int32 entityHandle);
};

// TODO(port): FxManager.h not included - it pulls pre-existing tree conflicts
// (FxSystem.h eBoneTag vs CPed.h, RenderTypes.h CRGBA redefinition).
// Include the real header once those are fixed.
class FxSystem_c;
class FxManager_c {
public:
    void DestroyFxSystem(FxSystem_c* system);
};
extern FxManager_c& g_fxMan;

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iterator>

// ---- Deferred-subsystem shims (TODO(port)) ---------------------------------
// Minimal declarations for subsystems referenced by the filled methods below.
// Delete each shim when the real subsystem header is ported. These exist only
// to let the filled bodies syntax-check; they carry no behavior.

// CGeneral: random numbers (General.h not ported)
class CGeneral {
public:
    static int32 GetRandomNumber();
    static float GetRandomNumberInRange(float min, float max);
};

// CCarCtrl: vehicle counts (CarCtrl.h not ported)
class CCarCtrl {
public:
    static void UpdateCarCount(CVehicle* vehicle, bool bRemove);
    static int32 NumAmbulancesOnDuty;
    static int32 NumFireTrucksOnDuty;
};

// CReplay (Replay.h not ported)
class CReplay {
public:
    static void RecordVehicleDeleted(CVehicle* vehicle);
};

// Vehicle pool accessor (Pools.h not ported; the local CPool<CVehicle> does
// not expose NewAt/GetRef yet, so a dedicated shim keeps call sites faithful)
class CVehiclePool {
public:
    void* New();
    void* NewAt(int32 poolRef);
    void Delete(CVehicle* vehicle);
    int32 GetRef(CVehicle* vehicle);
};
CVehiclePool* GetVehiclePool();

// CRopes (Ropes.h not ported)
class CRope {
public:
    void Remove();
};
class CRopes {
public:
    static int32 FindRope(uint32 id);
    static CRope& GetRope(int32 index);
};

// CDarkel (Darkel.h not ported)
class CDarkel {
public:
    static void RegisterCarBlownUpByPlayer(const CVehicle& vehicle, int32 unk);
    static void RegisterKillByPlayer(const CPed& ped, eWeaponType weapon, bool arg2, int32 arg3);
};

// RenderWare texture destroy (RenderWare layer not ported)
void RwTextureDestroy(RwTexture* texture);

// CPlayerInfo now comes from CPlayerInfo.h via CWorld.h (2026-10-09).

// CStats (Stats.h not ported; value verified from gta-reversed Enums/eStats.h)
class CStats {
public:
    static void IncrementStat(int32 statId, float value);
};
enum : int32 {
    STAT_CALORIES = 245,
};

// CStreaming (Streaming.h not ported; value verified from gta-reversed)
class CStreaming {
public:
    static void RequestModel(int32 modelId, int32 flags);
    static bool IsModelLoaded(int32 modelId);
    static void SetModelIsDeletable(int32 modelId);
};
constexpr int32 STREAMING_GAME_REQUIRED = 0x2;

// CPopulation (Population.h not ported)
class CPopulation {
public:
    static CPed* AddPedInCar(CVehicle* vehicle, bool bDriver, int32 pedType, int32 seatIdx, bool createAsMale, bool createAsCriminal);
    static void RemovePed(CPed* ped);
};

// CGameLogic (GameLogic.h not ported)
class CGameLogic {
public:
    static bool IsCoopGameGoingOn();
};

// CEventVehicleDied (Events subsystem not ported)
class CEventVehicleDied {
public:
    explicit CEventVehicleDied(CVehicle* vehicle);
};

// CVehicleSaveStructure + CGenericGameStorage (not ported)
class CVehicleSaveStructure {
public:
    void Construct(CVehicle* vehicle);
    void Extract(CVehicle* vehicle);
};
class CGenericGameStorage {
public:
    static void SaveDataToWorkBuffer(const void* data, uint32 size);
    static void LoadDataFromWorkBuffer(void* data, uint32 size);
};

// cHandlingDataMgr (not ported)
struct cHandlingDataMgrShim {
    float fWheelFriction;
};
extern cHandlingDataMgrShim gHandlingDataMgr;

// Model IDs: canonical eModelID.h (deduped 2026-10-09; the anonymous enum
// here redefined its enumerators - values verified vs src_prev_export/_types.h).

// Static data members (game addresses recorded from gta-reversed StaticRef)
float CVehicle::WHEELSPIN_TARGET_RATE{}; // game address: 0x8D3498
float CVehicle::WHEELSPIN_INAIR_TARGET_RATE{}; // game address: 0x8D349C
float CVehicle::WHEELSPIN_RISE_RATE{}; // game address: 0x8D34A0
float CVehicle::WHEELSPIN_FALL_RATE{}; // game address: 0x8D34A4
float CVehicle::m_fAirResistanceMult{}; // game address: 0x8D34A8
float CVehicle::ms_fRailTrackResistance{}; // game address: 0x8D34AC
float CVehicle::ms_fRailTrackResistanceDefault{}; // game address: 0x8D34B0
bool CVehicle::bDisableRemoteDetonation{}; // game address: 0xC1CC00
bool CVehicle::bDisableRemoteDetonationOnContact{}; // game address: 0xC1CC01
bool CVehicle::m_bEnableMouseSteering{}; // game address: 0xC1CC02
bool CVehicle::m_bEnableMouseFlying{}; // game address: 0xC1CC03
eControllerType CVehicle::m_nLastControlInput{}; // game address: 0xC1CC04
std::array<CVehicle*, 4> CVehicle::m_aSpecialColVehicle{}; // game address: 0xC1CC08
std::array<CColModel, 4> CVehicle::m_aSpecialColModel{}; // game address: 0xC1CC78
bool CVehicle::ms_forceVehicleLightsOff{}; // game address: 0xC1CC18
bool CVehicle::s_bPlaneGunsEjectShellCasings{}; // game address: 0xC1CC19
std::array<tHydraulicData, 4> CVehicle::m_aSpecialHydraulicData{}; // game address: 0xC1CB60

// 0x6D5F10
CVehicle::CVehicle(eVehicleCreatedBy createdBy) : CPhysical(), m_vehicleAudio(), m_autoPilot() {
    m_bHasPreRenderEffects = true;
    SetTypeVehicle();

    m_fRawSteerAngle = 0.0f;
    m_f2ndSteerAngle = 0.0f;
    m_nCurrentGear = 1;
    m_fGearChangeCount = 0.0f;
    m_fWheelSpinForAudio = 0.0f;
    m_nCreatedBy = createdBy;
    m_nForcedRandomRouteSeed = 0;

    m_nVehicleUpperFlags = 0;
    m_nVehicleLowerFlags = 0;
    vehicleFlags.bFreebies = true;
    vehicleFlags.bIsHandbrakeOn = true;
    vehicleFlags.bEngineOn = true;
    vehicleFlags.bCanBeDamaged = true;
    vehicleFlags.bParking = false;
    vehicleFlags.bRestingOnPhysical = false;
    vehicleFlags.bCreatedAsPoliceVehicle = false;
    vehicleFlags.bVehicleCanBeTargettedByHS = true;
    vehicleFlags.bWinchCanPickMeUp = true;
    vehicleFlags.bPetrolTankIsWeakPoint = true;
    vehicleFlags.bConsideredByPlayer = true;
    vehicleFlags.bDoesProvideCover = true;
    vehicleFlags.bUsedForReplay = false;
    vehicleFlags.bDontSetColourWhenRemapping = false;
    vehicleFlags.bUseCarCheats = false;
    vehicleFlags.bHasBeenResprayed = false;
    vehicleFlags.bNeverUseSmallerRemovalRange = false;
    vehicleFlags.bDriverLastFrame = false;

    auto fRand = static_cast<float>(CGeneral::GetRandomNumber()) / static_cast<float>(RAND_MAX);
    vehicleFlags.bCanPark = fRand < 0.0F; // BUG: seemingly never true, CGeneral::GetRandomNumber() strips the sign bit

    CCarCtrl::UpdateCarCount(this, false);
    m_nExtendedRemovalRange = 0;
    m_fHealth = 1000.0f;
    m_pDriver = nullptr;
    m_nNumPassengers = 0;
    m_nMaxPassengers = 8;
    m_nNumGettingIn = 0;
    m_nGettingInFlags = 0;
    m_nGettingOutFlags = 0;

    m_nBombOnBoard = 0;
    m_nOverrideLights = eVehicleOverrideLightsState::NO_CAR_LIGHT_OVERRIDE;
    m_ropeType = 0;
    m_nGunsCycleIndex = 0;
    physicalFlags.bCanBeCollidedWith = true;

    m_nLastWeaponDamageType = -1;
    m_vehicleSpecialColIndex = -1;

    m_pWhoInstalledBombOnMe = nullptr;
    m_wBombTimer = 0;
    m_pWhoDetonatedMe = nullptr;
    m_nTimeWhenBlowedUp = 0;

    m_nPacMansCollected = 0;
    m_pFire = nullptr;
    m_nGunFiringTime = 0;
    m_nCopsInCarTimer = 0;
    m_nUsedForCover = 0;
    m_HornCounter = 0;
    m_HornPattern = 0;
    m_nCarHornTimer = 0;
    field_4EC = 0;
    m_pTowingVehicle = nullptr;
    m_pVehicleBeingTowed = nullptr;
    m_nTimeTillWeNeedThisCar = 0;
    m_nAlarmState = 0;
    m_nDoorLock = eCarLock::CARLOCK_UNLOCKED;
    m_nProjectileWeaponFiringTime = 0;
    m_nAdditionalProjectileWeaponFiringTime = 0;
    m_nTimeForMinigunFiring = 0;
    m_pLastDamageEntity = nullptr;
    m_pEntityWeAreOn = nullptr;
    m_fVehicleRearGroundZ = 0.0f;
    m_fVehicleFrontGroundZ = 0.0f;
    field_511 = 0;
    field_512 = 0;
    m_comedyControlState = eComedyControlState::INACTIVE;
    m_FrontCollPoly.valid = false;
    m_RearCollPoly.valid = false;
    m_pHandlingData = nullptr;
    m_nHandlingFlagsIntValue = static_cast<eVehicleHandlingFlags>(0);
    // TODO(port): CAutoPilot not ported - m_autoPilot is an opaque stand-in.
    // The original also does:
    //     m_autoPilot.m_nTempAction = TEMPACT_NONE;
    //     m_autoPilot.SetCarMission(MISSION_NONE, 0);
    //     m_autoPilot.carCtrlFlags.bAvoidLevelTransitions = false;
    m_nRemapTxd = -1;
    m_nPreviousRemapTxd = -1;
    m_pRemapTexture = nullptr;
    m_pOverheatParticle = nullptr;
    m_pFireParticle = nullptr;
    m_pDustParticle = nullptr;
    m_pCustomCarPlate = nullptr;
    m_anUpgrades.fill(-1);
    m_fWheelScale = 1.0f;
    m_nWindowsOpenFlags = 0;
    m_nNitroBoosts = 0;
    m_nHasslePosId = 0;
    m_nVehicleWeaponInUse = CAR_WEAPON_NOT_USED;
    m_fDirtLevel = static_cast<float>(CGeneral::GetRandomNumber() % 15);
    m_nCreationTime = CTimer::GetTimeInMS();
    SetCollisionLighting(tColLighting(0x48));
}

// 0x6E2B40
CVehicle::~CVehicle() {
    CReplay::RecordVehicleDeleted(this);
    m_nAlarmState = 0;
    // NOTE: the original calls the virtual DeleteRwObject() here (V1053-style);
    // kept for fidelity - derived classes' overrides run before their own dtors.
    DeleteRwObject();
    CRadar::ClearBlipForEntity(eBlipType::BLIP_CAR, GetVehiclePool()->GetRef(this));

    if (m_pDriver) {
        m_pDriver->FlagToDestroyWhenNextProcessed();
    }

    for (auto passenger : m_apPassengers) {
        if (passenger) {
            passenger->FlagToDestroyWhenNextProcessed();
        }
    }

    if (m_pFire) {
        m_pFire->Extinguish();
        m_pFire = nullptr;
    }

    CCarCtrl::UpdateCarCount(this, true);
    if (vehicleFlags.bIsAmbulanceOnDuty) {
        --CCarCtrl::NumAmbulancesOnDuty;
        vehicleFlags.bIsAmbulanceOnDuty = false;
    }

    if (vehicleFlags.bIsFireTruckOnDuty) {
        --CCarCtrl::NumFireTrucksOnDuty;
        vehicleFlags.bIsFireTruckOnDuty = false;
    }

    if (m_vehicleSpecialColIndex > -1) {
        m_aSpecialColVehicle[m_vehicleSpecialColIndex] = nullptr;
        m_vehicleSpecialColIndex = -1;
    }

    // NOTE: assigning particle = nullptr only nulls the loop copy, not the member;
    // matches the original (which has the same no-op).
    for (auto particle : { m_pOverheatParticle, m_pFireParticle, m_pDustParticle }) {
        if (particle) {
            g_fxMan.DestroyFxSystem(particle);
            particle = nullptr;
        }
    }

    if (m_pCustomCarPlate) {
        RwTextureDestroy(m_pCustomCarPlate);
        m_pCustomCarPlate = nullptr;
    }

    const auto iRopeInd = CRopes::FindRope(reinterpret_cast<uint32>(this) + 1);
    if (iRopeInd >= 0) {
        CRopes::GetRope(iRopeInd).Remove();
    }

    if (!physicalFlags.bRenderScorched && m_fHealth < 250.0F) {
        CDarkel::RegisterCarBlownUpByPlayer(*this, 0);
    }
}

void* CVehicle::operator new(unsigned size) {
    return GetVehiclePool()->New();
}

void CVehicle::operator delete(void* data) {
    GetVehiclePool()->Delete(static_cast<CVehicle*>(data));
}

void* CVehicle::operator new(unsigned size, int32 poolRef) {
    return GetVehiclePool()->NewAt(poolRef);
}

void CVehicle::operator delete(void* data, int32 poolRef) {
    GetVehiclePool()->Delete(static_cast<CVehicle*>(data));
}

// 0x6D6A40
void CVehicle::SetModelIndex(uint32 index) {
    CEntity::SetModelIndex(index);
    auto mi = CModelInfo::GetModelInfo(index)->AsVehicleModelInfoPtr();
    CustomCarPlate_TextureCreate(mi);
    for (auto i = 0u; i < std::size(m_anExtras); i++) {
        m_anExtras[i] = CVehicleModelInfo::ms_compsUsed[i];
    }
    m_nMaxPassengers = CVehicleModelInfo::GetMaximumNumberOfPassengersFromNumberOfDoors(index);
    switch (m_nModelIndex) {
    case MODEL_RCBANDIT:
    case MODEL_RCBARON:
    case MODEL_RCRAIDER:
    case MODEL_RCGOBLIN:
    case MODEL_RCTIGER:
        vehicleFlags.bIsRCVehicle = true;
        break;
    default:
        vehicleFlags.bCreatedAsPoliceVehicle = false;
        vehicleFlags.bIsRCVehicle = false;
        break;
    }

    // Set up weapons
    switch (m_nModelIndex) {
    case MODEL_RUSTLER:
    case MODEL_STUNT:
        m_nVehicleWeaponInUse = CAR_WEAPON_HEAVY_GUN;
        break;
    case MODEL_BEAGLE:
        m_nVehicleWeaponInUse = CAR_WEAPON_FREEFALL_BOMB;
        break;
    case MODEL_HYDRA:
    case MODEL_TORNADO:
        m_nVehicleWeaponInUse = CAR_WEAPON_LOCK_ON_ROCKET;
        break;
    }
}

// 0x6D6410
void CVehicle::DeleteRwObject() {
    SetRemapTexDictionary(-1);
    RemoveAllUpgrades();
    CEntity::DeleteRwObject();
}

void CVehicle::SpecialEntityPreCollisionStuff(CPhysical* colPhysical, bool bIgnoreStuckCheck, bool& bCollisionDisabled, bool& bCollidedEntityCollisionIgnored, bool& bCollidedEntityUnableToMove, bool& bThisOrCollidedEntityStuck) {    // TODO: decomp src/CVehicle/*.c
    (void)colPhysical;
    (void)bIgnoreStuckCheck;
    (void)bCollisionDisabled;
    (void)bCollidedEntityCollisionIgnored;
    (void)bCollidedEntityUnableToMove;
    (void)bThisOrCollidedEntityStuck;
}

uint8 CVehicle::SpecialEntityCalcCollisionSteps(bool& bProcessCollisionBeforeSettingTimeStep, bool& unk2) {    // TODO: decomp src/CVehicle/*.c
    (void)bProcessCollisionBeforeSettingTimeStep;
    (void)unk2;
    return 0;
}

void CVehicle::PreRender() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::Render() {    // TODO: decomp src/CVehicle/*.c
}

bool CVehicle::SetupLighting() {    // TODO: decomp src/CVehicle/*.c
    return false;
}

void CVehicle::RemoveLighting(bool bRemove) {    // TODO: decomp src/CVehicle/*.c
    (void)bRemove;
}

void CVehicle::ProcessOpenDoor(CPed* ped, uint32 doorComponentId, uint32 animGroup, uint32 animId, float fTime) {    // TODO: decomp src/CVehicle/*.c
    (void)ped;
    (void)doorComponentId;
    (void)animGroup;
    (void)animId;
    (void)fTime;
}

void CVehicle::ProcessDrivingAnims(CPed* driver, bool bBlend) {    // TODO: decomp src/CVehicle/*.c
    (void)driver;
    (void)bBlend;
}

float CVehicle::GetHeightAboveRoad() {    // TODO: decomp src/CVehicle/*.c
    return 0.0f;
}

// 0x6D2030
bool CVehicle::CanPedStepOutCar(bool bIgnoreSpeedUpright) const {
    auto const fUpZ = m_matrix->GetUp().z;
    if (std::fabs(fUpZ) <= 0.1F) {
        if (std::fabs(m_vecMoveSpeed.z) > 0.05F || m_vecMoveSpeed.Magnitude2D() > 0.01F || m_vecTurnSpeed.SquaredMagnitude() > 0.0004F) { // 0.02F / 50.0f
            return false;
        }
        return true;
    }

    if (IsBoat())
        return true;

    if (bIgnoreSpeedUpright)
        return m_vecTurnSpeed.SquaredMagnitude() > 0.0004F;

    return m_vecMoveSpeed.Magnitude2D() <= 0.01F &&
           std::fabs(m_vecMoveSpeed.z) <= 0.05F &&
           m_vecTurnSpeed.SquaredMagnitude() <= 0.0004F;
}

bool CVehicle::CanPedJumpOutCar(CPed* ped) {    // TODO: decomp src/CVehicle/*.c
    (void)ped;
    return false;
}

bool CVehicle::GetTowHitchPos(CVector& outPos, bool bCheckModelInfo, CVehicle* vehicle) {    // TODO: decomp src/CVehicle/*.c
    (void)outPos;
    (void)bCheckModelInfo;
    (void)vehicle;
    return false;
}

bool CVehicle::GetTowBarPos(CVector& outPos, bool bCheckModelInfo, CVehicle* vehicle) {    // TODO: decomp src/CVehicle/*.c
    (void)outPos;
    (void)bCheckModelInfo;
    (void)vehicle;
    return false;
}

// 0x5D4760
bool CVehicle::Save() {
    uint32 size = sizeof(CVehicleSaveStructure);
    CVehicleSaveStructure data;
    data.Construct(this);
    CGenericGameStorage::SaveDataToWorkBuffer(&size, sizeof(uint32)); // Unused, game ignores it on load and uses const value
    CGenericGameStorage::SaveDataToWorkBuffer(&data, size);
    return true;
}

// 0x5D2900
bool CVehicle::Load() {
    uint32 size;
    CVehicleSaveStructure data;
    CGenericGameStorage::LoadDataFromWorkBuffer(&size, sizeof(uint32));
    CGenericGameStorage::LoadDataFromWorkBuffer(&data, sizeof(CVehicleSaveStructure)); // BUG: should use the value read on the line above, not the constant
    data.Extract(this);
    return true;
}

int32 CVehicle::GetRemapIndex() {    // TODO: decomp src/CVehicle/*.c
    return 0;
}

void CVehicle::SetRemapTexDictionary(int32 txdId) {    // TODO: decomp src/CVehicle/*.c
    (void)txdId;
}

void CVehicle::SetRemap(int32 remapIndex) {    // TODO: decomp src/CVehicle/*.c
    (void)remapIndex;
}

void CVehicle::SetCollisionLighting(tColLighting lighting) {    // TODO: decomp src/CVehicle/*.c
    (void)lighting;
}

void CVehicle::UpdateLightingFromStoredPolys() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::CalculateLightingFromCollision() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::ResetAfterRender() {    // TODO: decomp src/CVehicle/*.c
}

eVehicleAppearance CVehicle::GetVehicleAppearance() const {    // TODO: decomp src/CVehicle/*.c
    return {};
}

bool CVehicle::CustomCarPlate_TextureCreate(CVehicleModelInfo* model) {    // TODO: decomp src/CVehicle/*.c
    (void)model;
    return false;
}

void CVehicle::CustomCarPlate_TextureDestroy() {    // TODO: decomp src/CVehicle/*.c
}

// 0x6D1180
bool CVehicle::CanBeDeleted() {
    if (m_nNumGettingIn || m_nGettingOutFlags)
        return false;

    if (m_pDriver) {
        if (m_pDriver->IsCreatedByMission())
            return false;

        if (!m_pDriver->IsStateDriving() && !m_pDriver->IsStateDead())
            return false;
    }

    for (auto passenger : m_apPassengers) {
        if (passenger) {
            if (passenger->IsCreatedByMission())
                return false;

            if (!passenger->IsStateDriving() && !passenger->IsStateDead()) // OG: checked twice
                return false;
        }
    }

    switch (GetCreatedBy()) {
    case MISSION_VEHICLE:
    case PERMANENT_VEHICLE:
        return false;
    default:
        return true;
    }
}

// 0x6D1230
float CVehicle::ProcessWheelRotation(tWheelState wheelState, const CVector& arg1, const CVector& arg2, float arg3) {
    if (wheelState == WHEEL_STATE_SPINNING)
        return -1.1f;

    if (wheelState == WHEEL_STATE_FIXED)
        return 0.0f;

    const auto angle = arg1.Dot(arg2) / arg3;
    return -angle;
}

bool CVehicle::CanVehicleBeDamaged(CEntity* damager, eWeaponType weapon, bool& bDamagedDueToFireOrExplosionOrBullet) {    // TODO: decomp src/CVehicle/*.c
    (void)damager;
    (void)weapon;
    (void)bDamagedDueToFireOrExplosionOrBullet;
    return false;
}

// 0x6D1340
void CVehicle::ProcessDelayedExplosion() {
    if (!m_wBombTimer) {
        return;
    }

    const auto period = static_cast<int16>(CTimer::GetTimeStep() * (100.0f / 6.0f));
    m_wBombTimer = std::max(m_wBombTimer - period, 0);

    if (!m_wBombTimer) {
        BlowUpCar(m_pWhoDetonatedMe, false);
    }
}

void CVehicle::ApplyTurnForceToOccupantOnEntry(CPed* passenger) {    // TODO: decomp src/CVehicle/*.c
    (void)passenger;
}

// 0x6D13A0
bool CVehicle::AddPassenger(CPed* passenger) {
    ApplyTurnForceToOccupantOnEntry(passenger);

    // Now, find a seat and place them into it
    for (int32 i = 0; i < m_nMaxPassengers; i++) {
        if (!m_apPassengers[i]) {
            m_apPassengers[i] = passenger;
            CEntity::RegisterReference(m_apPassengers[i]);
            m_nNumPassengers++;
            return true;
        }
    }

    // No empty seats
    return false;
}

// 0x6D14D0
bool CVehicle::AddPassenger(CPed* passenger, uint8 seatNumber) {
    if (vehicleFlags.bIsBus) {
        return AddPassenger(passenger);
    }

    // Check if seat is valid
    if (seatNumber >= m_nMaxPassengers) {
        return false;
    }

    // Check if anyone is already in that seat
    if (HasPassengerAtSeat(seatNumber)) {
        return false;
    }

    // Place passenger into seat, and add ref
    m_apPassengers[seatNumber] = passenger;
    CEntity::RegisterReference(m_apPassengers[seatNumber]);
    m_nNumPassengers++;

    return true;
}

// 0x6D1610
void CVehicle::RemovePassenger(CPed* passenger) {
    if (!passenger) {
        return;
    }

    // Trains scan the whole array, everything else only the used seats
    const auto numSeats = IsTrain() ? static_cast<int32>(m_apPassengers.size()) : m_nMaxPassengers;
    for (int32 i = 0; i < numSeats; i++) {
        if (m_apPassengers[i] == passenger) {
            CEntity::SafeCleanUpRef(m_apPassengers[i]);
            m_apPassengers[i] = nullptr;

            assert(m_nNumPassengers > 0); // NOTSA: sanity check
            m_nNumPassengers--;
            return;
        }
    }
}

// 0x6D16A0
void CVehicle::SetDriver(CPed* driver) {
    CEntity::ChangeEntityReference(m_pDriver, driver);

    if (vehicleFlags.bFreebies && driver == FindPlayerPed()) {
        vehicleFlags.bFreebies = false;

        switch (m_nModelIndex)
        {
        case MODEL_AMBULAN: {
            FindPlayerInfo(0).AddHealth(20);
            break;
        }
        case MODEL_TAXI:
        case MODEL_CABBIE: {
            FindPlayerInfo().m_nMoney += 12;
            break;
        }
        case MODEL_ENFORCER: {
            driver->m_fArmour = std::max(static_cast<float>(FindPlayerInfo(0).m_nMaxArmour), driver->m_fArmour);
            break;
        }
        case MODEL_CADDY: {
            if (!driver->IsPlayer() || driver->AsPlayer()->DoesPlayerWantNewWeapon(eWeaponType::WEAPON_GOLFCLUB, true)) {
                CStreaming::RequestModel(MODEL_GOLFCLUB, STREAMING_GAME_REQUIRED);
            }
            break;
        }
        case MODEL_HOTDOG: {
            CStats::IncrementStat(STAT_CALORIES, 40.0f);
            break;
        }
        case MODEL_COPCARLA:
        case MODEL_COPCARSF:
        case MODEL_COPCARVG:
        case MODEL_COPCARRU: {
            CStreaming::RequestModel(MODEL_CHROMEGUN, STREAMING_GAME_REQUIRED);
            vehicleFlags.bFreebies = true;
            break;
        }
        default:
            break;
        }
    }

    ApplyTurnForceToOccupantOnEntry(driver);
}

// 0x6D1950
void CVehicle::RemoveDriver(bool arg0) {
    const bool dontTurnEngineOff = arg0;
    SetStatus(STATUS_ABANDONED);

    if (!dontTurnEngineOff) {
        if (!m_pDriver || !m_pDriver->IsPlayer()) {
            vehicleFlags.bEngineOn = false;
        }
    }

    if (const auto playerPed = FindPlayerPed(); m_pDriver == playerPed) {
        switch (m_nModelIndex) {
        case MODEL_CADDY: {
            if (CStreaming::IsModelLoaded(MODEL_GOLFCLUB)) {
                if (playerPed->DoesPlayerWantNewWeapon(eWeaponType::WEAPON_GOLFCLUB, true)) {
                    playerPed->GiveWeapon(WEAPON_GOLFCLUB, 1, true);
                }
                CStreaming::SetModelIsDeletable(MODEL_GOLFCLUB);
            }
            break;
        }
        case MODEL_COPCARLA:
        case MODEL_COPCARSF:
        case MODEL_COPCARVG:
        case MODEL_COPCARRU: {
            if (CStreaming::IsModelLoaded(MODEL_CHROMEGUN) && vehicleFlags.bFreebies) {
                if (playerPed->DoesPlayerWantNewWeapon(eWeaponType::WEAPON_SHOTGUN, true)) {
                    playerPed->GiveWeapon(eWeaponType::WEAPON_SHOTGUN, 5, true);
                } else {
                    playerPed->GrantAmmo(eWeaponType::WEAPON_SHOTGUN, 5);
                }
                vehicleFlags.bFreebies = false;
                CStreaming::SetModelIsDeletable(MODEL_CHROMEGUN);
            }
            break;
        }
        }
    }

    CEntity::ClearReference(m_pDriver);
}

// 0x6D1A50
CPed* CVehicle::SetUpDriver(int32 pedType, bool arg1, bool arg2) {
    if (m_pDriver) {
        return m_pDriver;
    }

    if (IsCreatedBy(eVehicleCreatedBy::RANDOM_VEHICLE)) {
        CPopulation::AddPedInCar(this, true, pedType, 0, arg1, arg2);
        return m_pDriver;
    }

    return nullptr;
}

// 0x6D1AA0
CPed* CVehicle::SetupPassenger(int32 seatNumber, int32 pedType, bool arg2, bool arg3) {
    if (const auto passenger = m_apPassengers[seatNumber]) {
        return passenger;
    }

    switch (m_nModelIndex) {
    case MODEL_TAXI:
    case MODEL_CABBIE:
    case MODEL_STRETCH: {
        if (!seatNumber) {
            // RemovePassenger(m_apPassengers[0]); // does nothing: we already ensured nobody sits here
            return nullptr;
        }
    }
    }

    const auto psgrAdded = CPopulation::AddPedInCar(this, false, pedType, seatNumber, arg2, arg3);

    const auto ShouldCheckModels = [&] {
        switch (psgrAdded->m_nPedType) {
        case PED_TYPE_MEDIC:
        case PED_TYPE_FIREMAN:
        case PED_TYPE_COP: {
            return false;
        }
        case PED_TYPE_CRIMINAL: {
            switch (pedType) {
            case PED_TYPE_GANG8:
            case PED_TYPE_GANG9:
            case PED_TYPE_GANG10:
            case PED_TYPE_DEALER:
            case PED_TYPE_MEDIC:
            case PED_TYPE_FIREMAN:
            case PED_TYPE_CRIMINAL:
            case PED_TYPE_BUM:
            case PED_TYPE_PROSTITUTE:
            case PED_TYPE_SPECIAL:
                return false;
            }
            break;
        }
        default:
            return !IsPedTypeGang(psgrAdded->m_nPedType);
        }
        return true;
    };

    // For some ped types, make sure no occupant in the seats before this one
    // has the same model id; if one does, the just-added passenger is removed
    // and nullptr is returned.
    if (ShouldCheckModels()) {
        const auto ProcessOccupant = [&](CPed* occupant) {
            if (occupant && occupant->m_nModelIndex == psgrAdded->m_nModelIndex) {
                RemovePassenger(psgrAdded);
                CPopulation::RemovePed(psgrAdded);
                return false;
            }
            return true;
        };

        if (!ProcessOccupant(m_pDriver)) {
            return nullptr;
        }
        for (int32 i = 0; i < seatNumber; i++) {
            if (!ProcessOccupant(m_apPassengers[i])) {
                return nullptr;
            }
        }
    }

    return psgrAdded;
}

// 0x6D1BD0
bool CVehicle::IsPassenger(CPed* ped) const {
    if (!ped)
        return false;

    for (const auto& passenger : m_apPassengers) {
        if (passenger == ped) {
            return true;
        }
    }
    return false;
}

bool CVehicle::IsPassenger(int32 modelIndex) const {    // TODO: decomp src/CVehicle/*.c
    (void)modelIndex;
    return false;
}

bool CVehicle::IsPedOfModelInside(eModelID model) const {    // TODO: decomp src/CVehicle/*.c
    (void)model;
    return false;
}

// 0x6D1C40
bool CVehicle::IsDriver(const CPed* ped) const {
    return ped && ped == m_pDriver;
}

bool CVehicle::IsDriver(int32 modelIndex) const {    // TODO: decomp src/CVehicle/*.c
    (void)modelIndex;
    return false;
}

// 0x6D1C80
void CVehicle::KillPedsInVehicle() {
    const auto ProcessOccupant = [this](CPed* occupant) {
        if (occupant) {
            if (!CGameLogic::IsCoopGameGoingOn()) {
                CDarkel::RegisterKillByPlayer(*occupant, WEAPON_EXPLOSION, false, 0);
            }
            // TODO(port): events subsystem - the original dispatches CEventVehicleDied:
            //     CEventVehicleDied event{ this };
            //     occupant->GetIntelligence()->m_eventGroup.Add(&event);
        }
    };

    ProcessOccupant(m_pDriver);
    for (int32 i = 0; i < m_nMaxPassengers; i++) {
        ProcessOccupant(m_apPassengers[i]);
    }
}

// 0x6D1D90
bool CVehicle::IsUpsideDown() const {
    return m_matrix->GetUp().z <= -0.9f;
}

// 0x6D1DD0
bool CVehicle::IsOnItsSide() const {
    return m_matrix->GetRight().z >= 0.8f || m_matrix->GetRight().z <= -0.8f;
}

bool CVehicle::CanPedOpenLocks(const CPed* ped) const {    // TODO: decomp src/CVehicle/*.c
    (void)ped;
    return false;
}

bool CVehicle::CanDoorsBeDamaged() const {    // TODO: decomp src/CVehicle/*.c
    return false;
}

// 0x6D1E80
bool CVehicle::CanPedEnterCar() {
    const auto upZ = GetUp().z;
    if (IsBike() || upZ > 0.1f || upZ < -0.1f) {
        return true;
    }

    return m_vecTurnSpeed.SquaredMagnitude() <= 0.2f * 0.2f &&
           m_vecMoveSpeed.SquaredMagnitude() <= 0.2f * 0.2f;
}

void CVehicle::ProcessCarAlarm() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::DestroyVehicleAndDriverAndPassengers(CVehicle* vehicle) {    // TODO: decomp src/CVehicle/*.c
    (void)vehicle;
}

// 0x6D22F0
bool CVehicle::IsVehicleNormal() {
    if (m_pDriver
        && !m_nNumPassengers
        && GetStatus() != STATUS_WRECKED
        && GetVehicleModelInfo()->m_nVehicleClass != VEHICLE_CLASS_IGNORE
    ) {
        return true;
    }
    return false;
}

void CVehicle::ChangeLawEnforcerState(bool bIsEnforcer) {    // TODO: decomp src/CVehicle/*.c
    (void)bIsEnforcer;
}

bool CVehicle::IsLawEnforcementVehicle() const {    // TODO: decomp src/CVehicle/*.c
    return false;
}

bool CVehicle::ShufflePassengersToMakeSpace() {    // TODO: decomp src/CVehicle/*.c
    return false;
}

void CVehicle::ExtinguishCarFire() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::ActivateBomb() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::ActivateBombWhenEntered() {    // TODO: decomp src/CVehicle/*.c
}

bool CVehicle::CarHasRoof() {    // TODO: decomp src/CVehicle/*.c
    return false;
}

float CVehicle::HeightAboveCeiling(float arg0, eFlightModel arg1) {    // TODO: decomp src/CVehicle/*.c
    (void)arg0;
    (void)arg1;
    return 0.0f;
}

void CVehicle::SetComponentVisibility(RwFrame* component, uint32 visibilityState) {    // TODO: decomp src/CVehicle/*.c
    (void)component;
    (void)visibilityState;
}

void CVehicle::ApplyBoatWaterResistance(tBoatHandlingData* boatHandling, float fImmersionDepth) {    // TODO: decomp src/CVehicle/*.c
    (void)boatHandling;
    (void)fImmersionDepth;
}

void CVehicle::UpdateClumpAlpha() {    // TODO: decomp src/CVehicle/*.c
}

// 0x6D29E0
void CVehicle::UpdatePassengerList() {
    // Checks if there should be any passengers; if none are found,
    // the count is reset to 0.
    if (m_nNumPassengers) {
        bool anyPassenger = false;
        for (auto passenger : m_apPassengers) {
            if (passenger) {
                anyPassenger = true;
                break;
            }
        }
        if (!anyPassenger) {
            m_nNumPassengers = 0;
        }
    }
}

CPed* CVehicle::PickRandomPassenger() {    // TODO: decomp src/CVehicle/*.c
    return nullptr;
}

void CVehicle::AddDamagedVehicleParticles() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::MakeDirty(CColPoint& colPoint) {    // TODO: decomp src/CVehicle/*.c
    (void)colPoint;
}

bool CVehicle::AddWheelDirtAndWater(CColPoint& colPoint, bool isProduceWheelDrops, bool isWheelsSpinning, bool isWheelInWater) {    // TODO: decomp src/CVehicle/*.c
    (void)colPoint;
    (void)isProduceWheelDrops;
    (void)isWheelsSpinning;
    (void)isWheelInWater;
    return false;
}

void CVehicle::SetGettingInFlags(uint8 doorId) {    // TODO: decomp src/CVehicle/*.c
    (void)doorId;
}

void CVehicle::SetGettingOutFlags(uint8 doorId) {    // TODO: decomp src/CVehicle/*.c
    (void)doorId;
}

void CVehicle::ClearGettingInFlags(uint8 doorId) {    // TODO: decomp src/CVehicle/*.c
    (void)doorId;
}

void CVehicle::ClearGettingOutFlags(uint8 doorId) {    // TODO: decomp src/CVehicle/*.c
    (void)doorId;
}

void CVehicle::SetWindowOpenFlag(uint8 doorId) {    // TODO: decomp src/CVehicle/*.c
    (void)doorId;
}

void CVehicle::ClearWindowOpenFlag(uint8 doorId) {    // TODO: decomp src/CVehicle/*.c
    (void)doorId;
}

bool CVehicle::SetVehicleUpgradeFlags(int32 upgradeModelIndex, int32 mod, int32& resultModelIndex) {    // TODO: decomp src/CVehicle/*.c
    (void)upgradeModelIndex;
    (void)mod;
    (void)resultModelIndex;
    return false;
}

bool CVehicle::ClearVehicleUpgradeFlags(int32 arg0, int32 componentIndex) {    // TODO: decomp src/CVehicle/*.c
    (void)arg0;
    (void)componentIndex;
    return false;
}

RpAtomic* CVehicle::CreateUpgradeAtomic(CBaseModelInfo* model, const UpgradePosnDesc* upgradePosn, RwFrame* parentComponent, bool isDamaged) {    // TODO: decomp src/CVehicle/*.c
    (void)model;
    (void)upgradePosn;
    (void)parentComponent;
    (void)isDamaged;
    return nullptr;
}

void CVehicle::RemoveUpgrade(int32 upgradeId) {    // TODO: decomp src/CVehicle/*.c
    (void)upgradeId;
}

int32 CVehicle::GetUpgrade(int32 upgradeId) {    // TODO: decomp src/CVehicle/*.c
    (void)upgradeId;
    return 0;
}

RpAtomic* CVehicle::CreateReplacementAtomic(CBaseModelInfo* model, RwFrame* component, eAtomicComponentFlag flags, bool bDamaged, bool bIsWheel) {    // TODO: decomp src/CVehicle/*.c
    (void)model;
    (void)component;
    (void)flags;
    (void)bDamaged;
    (void)bIsWheel;
    return nullptr;
}

void CVehicle::AddReplacementUpgrade(int32 modelIndex, int32 nodeId) {    // TODO: decomp src/CVehicle/*.c
    (void)modelIndex;
    (void)nodeId;
}

void CVehicle::RemoveReplacementUpgrade(int32 nodeId) {    // TODO: decomp src/CVehicle/*.c
    (void)nodeId;
}

int32 CVehicle::GetReplacementUpgrade(int32 nodeId) {    // TODO: decomp src/CVehicle/*.c
    (void)nodeId;
    return 0;
}

void CVehicle::RemoveAllUpgrades() {    // TODO: decomp src/CVehicle/*.c
}

int32 CVehicle::GetSpareHasslePosId() const {    // TODO: decomp src/CVehicle/*.c
    return 0;
}

void CVehicle::SetHasslePosId(int32 hasslePos, bool enable) {    // TODO: decomp src/CVehicle/*.c
    (void)hasslePos;
    (void)enable;
}

void CVehicle::InitWinch(int32 arg0) {    // TODO: decomp src/CVehicle/*.c
    (void)arg0;
}

void CVehicle::UpdateWinch() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::RemoveWinch() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::ReleasePickedUpEntityWithWinch() const {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::PickUpEntityWithWinch(CEntity* entity) const {    // TODO: decomp src/CVehicle/*.c
    (void)entity;
}

CEntity* CVehicle::QueryPickedUpEntityWithWinch() const {    // TODO: decomp src/CVehicle/*.c
    return nullptr;
}

float CVehicle::GetRopeHeightForHeli() const {    // TODO: decomp src/CVehicle/*.c
    return 0.0f;
}

void CVehicle::SetRopeHeightForHeli(float height) const {    // TODO: decomp src/CVehicle/*.c
    (void)height;
}

void CVehicle::RenderDriverAndPassengers() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::PreRenderDriverAndPassengers() {    // TODO: decomp src/CVehicle/*.c
}

float CVehicle::GetPlaneGunsAutoAimAngle() {    // TODO: decomp src/CVehicle/*.c
    return 0.0f;
}

int32 CVehicle::GetPlaneNumGuns() {    // TODO: decomp src/CVehicle/*.c
    return 0;
}

void CVehicle::SetFiringRateMultiplier(float multiplier) {    // TODO: decomp src/CVehicle/*.c
    (void)multiplier;
}

float CVehicle::GetFiringRateMultiplier() {    // TODO: decomp src/CVehicle/*.c
    return 0.0f;
}

uint32 CVehicle::GetPlaneGunsRateOfFire() {    // TODO: decomp src/CVehicle/*.c
    return 0;
}

CVector CVehicle::GetPlaneGunsPosition(int32 gunId) {    // TODO: decomp src/CVehicle/*.c
    (void)gunId;
    return {};
}

uint32 CVehicle::GetPlaneOrdnanceRateOfFire(eOrdnanceType type) {    // TODO: decomp src/CVehicle/*.c
    (void)type;
    return 0;
}

CVector CVehicle::GetPlaneOrdnancePosition(eOrdnanceType type) {    // TODO: decomp src/CVehicle/*.c
    (void)type;
    return {};
}

void CVehicle::SelectPlaneWeapon(bool bChange, eOrdnanceType type) {    // TODO: decomp src/CVehicle/*.c
    (void)bChange;
    (void)type;
}

void CVehicle::DoPlaneGunFireFX(CWeapon* weapon, CVector& particlePos, CVector& gunshellPos, int32 particleIndex) {    // TODO: decomp src/CVehicle/*.c
    (void)weapon;
    (void)particlePos;
    (void)gunshellPos;
    (void)particleIndex;
}

void CVehicle::FirePlaneGuns() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::FireUnguidedMissile(eOrdnanceType type, bool bCheckTime) {    // TODO: decomp src/CVehicle/*.c
    (void)type;
    (void)bCheckTime;
}

// 0x6D5400
bool CVehicle::CanBeDriven() const {
    if (IsSubTrailer() || IsSubTrain() && AsTrain()->m_nTrackId || vehicleFlags.bIsRCVehicle) {
        return false;
    }
    return GetDriverSeatDummyPositionOS().SquaredMagnitude() > 0.0f;
}

void CVehicle::ReactToVehicleDamage(CPed* ped) {    // TODO: decomp src/CVehicle/*.c
    (void)ped;
}

bool CVehicle::GetVehicleLightsStatus() {    // TODO: decomp src/CVehicle/*.c
    return false;
}

bool CVehicle::CanPedLeanOut(CPed* ped) {    // TODO: decomp src/CVehicle/*.c
    (void)ped;
    return false;
}

void CVehicle::SetVehicleCreatedBy(eVehicleCreatedBy createdBy) {    // TODO: decomp src/CVehicle/*.c
    (void)createdBy;
}

void CVehicle::SetupRender() {    // TODO: decomp src/CVehicle/*.c
}

// 0x6D6C00
void CVehicle::ProcessWheel(CVector& wheelFwd, CVector& wheelRight,
                            CVector& wheelContactSpeed, CVector& wheelContactPoint,
                            int32 wheelsOnGround,
                            float thrust, float brake, float adhesion,
                            int8 wheelId, float* wheelSpeed,
                            tWheelState* wheelState, uint16 wheelStatus
) {
    // Converted from StaticRef globals (game addresses kept as comments)
    static bool bBraking = false;         // 0xC1CDAE
    static bool bDriving = false;         // 0xC1CDAD
    static bool bAlreadySkidding = false; // 0xC1CDAC
    static float fBurstTyreMod = 0.13f;         // 0x8D34B4
    static float fBurstSpeedMax = 0.3f;         // 0x8D34B8
    static float WS_TRAC_FRAC_LIMIT = 0.3f;     // 0x8D34C0
    static float WS_ALREADY_SPINNING_LOSS = 0.2f; // 0x8D34C4

    float right = 0.0f;
    float fwd = 0.0f;
    float contactSpeedFwd = wheelFwd.Dot(wheelContactSpeed);
    float contactSpeedRight = wheelRight.Dot(wheelContactSpeed);

    bBraking = brake != 0.0f;
    bDriving = !bBraking;
    if (bDriving && thrust == 0.0f)
        bDriving = false;

    adhesion *= CTimer::GetTimeStep();
    if (*wheelState != WHEEL_STATE_NORMAL) {
        bAlreadySkidding = true;
        adhesion *= m_pHandlingData->m_fTractionLoss;
        if (*wheelState == WHEEL_STATE_SPINNING) {
            if (GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_REMOTE_CONTROLLED)
                adhesion *= (1.0f - fabs(m_GasPedal) * WS_ALREADY_SPINNING_LOSS);
        }
    }

    *wheelState = WHEEL_STATE_NORMAL;

    if (contactSpeedRight != 0.0f) {
        right = -(contactSpeedRight / wheelsOnGround);
        if (wheelStatus == WHEEL_STATUS_BURST) {
            float fwdspeed = std::min(contactSpeedFwd, fBurstSpeedMax);
            right += fwdspeed * CGeneral::GetRandomNumberInRange(-fBurstTyreMod, fBurstTyreMod);
        }
    }

    if (bDriving) {
        fwd = thrust;
        right = std::clamp(right, -adhesion, adhesion);
    }
    else if (contactSpeedFwd != 0.0f) {
        fwd = -contactSpeedFwd / wheelsOnGround;
        if (!bBraking && std::fabs(m_GasPedal) < 0.01f) {
            if (IsBike())
                brake = gHandlingDataMgr.fWheelFriction * 0.6f / (m_pHandlingData->m_fMass + 200.0f);
            else if (IsSubPlane())
                brake = 0.0f;
            else {
                brake = gHandlingDataMgr.fWheelFriction / m_pHandlingData->m_fMass;

                if (brake > 500.0f)
                    brake *= 0.1f;
                else if (m_nModelIndex == MODEL_RCBANDIT)
                    brake *= 0.2f;
            }
        }
        if (brake > adhesion) {
            if (std::fabs(contactSpeedFwd) > 0.005f) {
                *wheelState = WHEEL_STATE_FIXED;
            }
        } else {
            fwd = std::clamp(fwd, -brake, brake);
        }
    }

    float speedSq = right * right + fwd * fwd;
    if (speedSq > adhesion * adhesion) {
        if (*wheelState != WHEEL_STATE_FIXED) {
            float tractionLimit = WS_TRAC_FRAC_LIMIT;
            if (contactSpeedFwd > 0.15f && (!wheelId || wheelId == CAR_WHEEL_FRONT_RIGHT)) {
                tractionLimit += tractionLimit;
            }
            if (bDriving && tractionLimit * adhesion < std::fabs(fwd))
                *wheelState = WHEEL_STATE_SPINNING;
            else
                *wheelState = WHEEL_STATE_SKIDDING;
        }
        float tractionLoss = m_pHandlingData->m_fTractionLoss;
        if (bAlreadySkidding) {
            tractionLoss = 1.0f;
        } else if (*wheelState == WHEEL_STATE_SPINNING) {
            if (GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_REMOTE_CONTROLLED) {
                tractionLoss = tractionLoss * (1.0f - std::fabs(m_GasPedal) * WS_ALREADY_SPINNING_LOSS);
            }
        }
        float l = sqrt(speedSq);
        fwd *= adhesion * tractionLoss / l;
        right *= adhesion * tractionLoss / l;
    }

    if (fwd != 0.0f || right != 0.0f) {
        bool separateTurnForce = false;
        CVector totalSpeed = fwd * wheelFwd + right * wheelRight;
        CVector turnDirection  = totalSpeed;

        if (m_pHandlingData->m_fSuspensionAntiDiveMultiplier > 0.0f) {
            if (bBraking) {
                separateTurnForce = true;
                turnDirection -= (m_pHandlingData->m_fSuspensionAntiDiveMultiplier * wheelFwd * fwd);
            }
            else if (bDriving) {
                separateTurnForce = true;
                turnDirection -= (0.5f * m_pHandlingData->m_fSuspensionAntiDiveMultiplier * wheelFwd * fwd);
            }
        }

        CVector direction = totalSpeed;
        float speed = totalSpeed.Magnitude();
        float turnSpeed = speed;
        if (separateTurnForce)
            turnSpeed = turnDirection.Magnitude();
        direction.Normalise();
        if (separateTurnForce)
            turnDirection.Normalise();
        else
            turnDirection = direction;

        float force = speed * m_fMass;
        float turnForce = turnSpeed * GetMass(wheelContactPoint, turnDirection);
        ApplyMoveForce(force * direction);
        ApplyTurnForce(turnForce * turnDirection, wheelContactPoint);
    }
}

void CVehicle::ProcessBikeWheel(CVector& wheelFwd, CVector& wheelRight, CVector& wheelContactSpeed, CVector& wheelContactPoint, int32 wheelsOnGround, float thrust, float brake, float adhesion, float destabTraction, int8 wheelId, float* wheelSpeed, tWheelState* wheelState, eBikeWheelSpecial special, uint16 wheelStatus) {    // TODO: decomp src/CVehicle/*.c
    (void)wheelFwd;
    (void)wheelRight;
    (void)wheelContactSpeed;
    (void)wheelContactPoint;
    (void)wheelsOnGround;
    (void)thrust;
    (void)brake;
    (void)adhesion;
    (void)destabTraction;
    (void)wheelId;
    (void)wheelSpeed;
    (void)wheelState;
    (void)special;
    (void)wheelStatus;
}

CVehicle::eNearestCarWheel CVehicle::FindTyreNearestPoint(CVector2D point) {    // TODO: decomp src/CVehicle/*.c
    (void)point;
    return {};
}

void CVehicle::InflictDamage(CEntity* damager, eWeaponType weapon, float intensity, CVector coords) {    // TODO: decomp src/CVehicle/*.c
    (void)damager;
    (void)weapon;
    (void)intensity;
    (void)coords;
}

void CVehicle::KillPedsGettingInVehicle() {    // TODO: decomp src/CVehicle/*.c
}

bool CVehicle::UsesSiren() {    // TODO: decomp src/CVehicle/*.c
    return false;
}

bool CVehicle::IsSphereTouchingVehicle(CVector posn, float radius) {    // TODO: decomp src/CVehicle/*.c
    (void)posn;
    (void)radius;
    return false;
}

void CVehicle::FlyingControl(eFlightModel flightModel, float leftRightSkid, float steeringUpDown, float steeringLeftRight, float accelerationBreakStatus) {    // TODO: decomp src/CVehicle/*.c
    (void)flightModel;
    (void)leftRightSkid;
    (void)steeringUpDown;
    (void)steeringLeftRight;
    (void)accelerationBreakStatus;
}

void CVehicle::SetComponentRotation(RwFrame* component, eRotationAxis axis, float angle, bool bResetPosition) {    // TODO: decomp src/CVehicle/*.c
    (void)component;
    (void)axis;
    (void)angle;
    (void)bResetPosition;
}

void CVehicle::SetTransmissionRotation(RwFrame* component, float angleL, float angleR, CVector wheelPos, bool isFront) {    // TODO: decomp src/CVehicle/*.c
    (void)component;
    (void)angleL;
    (void)angleR;
    (void)wheelPos;
    (void)isFront;
}

void CVehicle::ProcessBoatControl(tBoatHandlingData* boatHandling, float* fWaterResistance, bool bCollidedWithWorld, bool bPostCollision) {    // TODO: decomp src/CVehicle/*.c
    (void)boatHandling;
    (void)fWaterResistance;
    (void)bCollidedWithWorld;
    (void)bPostCollision;
}

void CVehicle::DoBoatSplashes(float fWaterDamping) {    // TODO: decomp src/CVehicle/*.c
    (void)fWaterDamping;
}

void CVehicle::DoSunGlare() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::AddWaterSplashParticles() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::AddExhaustParticles() {    // TODO: decomp src/CVehicle/*.c
}

bool CVehicle::AddSingleWheelParticles(tWheelState wheelState, uint32 arg1, float arg2, float arg3, CColPoint* arg4, CVector* arg5, float arg6, int32 arg7, uint32 surfaceType, bool* bloodState, uint32 arg10) {    // TODO: decomp src/CVehicle/*.c
    (void)wheelState;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    (void)arg5;
    (void)arg6;
    (void)arg7;
    (void)surfaceType;
    (void)bloodState;
    (void)arg10;
    return false;
}

bool CVehicle::GetSpecialColModel() {    // TODO: decomp src/CVehicle/*.c
    return false;
}

void CVehicle::RemoveVehicleUpgrade(int32 upgradeModelIndex) {    // TODO: decomp src/CVehicle/*.c
    (void)upgradeModelIndex;
}

void CVehicle::AddUpgrade(int32 modelIndex, int32 upgradeIndex) {    // TODO: decomp src/CVehicle/*.c
    (void)modelIndex;
    (void)upgradeIndex;
}

void CVehicle::UpdateTrailerLink(bool arg0, bool arg1) {    // TODO: decomp src/CVehicle/*.c
    (void)arg0;
    (void)arg1;
}

void CVehicle::UpdateTractorLink(bool arg0, bool arg1) {    // TODO: decomp src/CVehicle/*.c
    (void)arg0;
    (void)arg1;
}

CEntity* CVehicle::ScanAndMarkTargetForHeatSeekingMissile(CEntity* entity) {    // TODO: decomp src/CVehicle/*.c
    (void)entity;
    return nullptr;
}

void CVehicle::FireHeatSeakingMissile(CEntity* targetEntity, eOrdnanceType ordnanceType, bool arg2) {    // TODO: decomp src/CVehicle/*.c
    (void)targetEntity;
    (void)ordnanceType;
    (void)arg2;
}

void CVehicle::PossiblyDropFreeFallBombForPlayer(eOrdnanceType ordnanceType, bool arg1) {    // TODO: decomp src/CVehicle/*.c
    (void)ordnanceType;
    (void)arg1;
}

void CVehicle::ProcessSirenAndHorn(bool arg0) {    // TODO: decomp src/CVehicle/*.c
    (void)arg0;
}

bool CVehicle::DoHeadLightEffect(eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight, bool disabledOrAlarm) {    // TODO: decomp src/CVehicle/*.c
    (void)lightId;
    (void)vehicleMatrix;
    (void)isRight;
    (void)disabledOrAlarm;
    return false;
}

void CVehicle::DoHeadLightBeam(eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight) {    // TODO: decomp src/CVehicle/*.c
    (void)lightId;
    (void)vehicleMatrix;
    (void)isRight;
}

void CVehicle::DoHeadLightReflectionSingle(CMatrix& vehicleMatrix, bool isRight) {    // TODO: decomp src/CVehicle/*.c
    (void)vehicleMatrix;
    (void)isRight;
}

void CVehicle::DoHeadLightReflectionTwin(CMatrix& vehicleMatrix) {    // TODO: decomp src/CVehicle/*.c
    (void)vehicleMatrix;
}

void CVehicle::DoHeadLightReflectionImpl(CMatrix& vehicleMatrix, eVehicleLightsFlags flags, bool includeLeft, bool includeRight) {    // TODO: decomp src/CVehicle/*.c
    (void)vehicleMatrix;
    (void)flags;
    (void)includeLeft;
    (void)includeRight;
}

void CVehicle::DoHeadLightReflection(CMatrix& vehicleMatrix, eVehicleLightsFlags flags, bool includeLeft, bool includeRight) {    // TODO: decomp src/CVehicle/*.c
    (void)vehicleMatrix;
    (void)flags;
    (void)includeLeft;
    (void)includeRight;
}

bool CVehicle::DoTailLightEffect(eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight, bool disabledOrAlarm, eVehicleLightsFlags flags_unused, bool staticEmission) {    // TODO: decomp src/CVehicle/*.c
    (void)lightId;
    (void)vehicleMatrix;
    (void)isRight;
    (void)disabledOrAlarm;
    (void)flags_unused;
    (void)staticEmission;
    return false;
}

bool CVehicle::DoLightEffectImpl(bool isFront, eVehicleLightId lightId, CMatrix& vehicleMatrix, bool isRight, bool disabledOrAlarm, bool staticEmission) {    // TODO: decomp src/CVehicle/*.c
    (void)isFront;
    (void)lightId;
    (void)vehicleMatrix;
    (void)isRight;
    (void)disabledOrAlarm;
    (void)staticEmission;
    return false;
}

void CVehicle::DoVehicleLights(CMatrix& vehicleMatrix, eVehicleLightsFlags flags) {    // TODO: decomp src/CVehicle/*.c
    (void)vehicleMatrix;
    (void)flags;
}

void CVehicle::FillVehicleWithPeds(bool bSetClothesToAfro) {    // TODO: decomp src/CVehicle/*.c
    (void)bSetClothesToAfro;
}

bool CVehicle::DoBladeCollision(CVector pos, CMatrix& matrix, int16 rotorType, float radius, float damageMult) {    // TODO: decomp src/CVehicle/*.c
    (void)pos;
    (void)matrix;
    (void)rotorType;
    (void)radius;
    (void)damageMult;
    return false;
}

void CVehicle::AddVehicleUpgrade(int32 modelId) {    // TODO: decomp src/CVehicle/*.c
    (void)modelId;
}

void CVehicle::SetupUpgradesAfterLoad() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::GetPlaneWeaponFiringStatus(bool& status, eOrdnanceType& ordnanceType) {    // TODO: decomp src/CVehicle/*.c
    (void)status;
    (void)ordnanceType;
}

void CVehicle::ProcessWeapons() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::DoFixedMachineGuns() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::FireFixedMachineGuns() {    // TODO: decomp src/CVehicle/*.c
}

void CVehicle::DoDriveByShootings() {    // TODO: decomp src/CVehicle/*.c
}

bool CVehicle::AreAnyOfPassengersFollowerOfGroup(const CPedGroup& group) {    // TODO: decomp src/CVehicle/*.c
    (void)group;
    return false;
}

std::optional<size_t> CVehicle::GetPassengerIndex(const CPed* ped) const {    // TODO: decomp src/CVehicle/*.c
    (void)ped;
    return {};
}

// 0x6D0B40
void CVehicle::Shutdown() {
    for (auto& specialColModel : m_aSpecialColModel) {
        if (specialColModel.m_pColData) {
            specialColModel.RemoveCollisionVolumes();
        }
    }
}

void CVehicle::SetComponentAtomicAlpha(RpAtomic* atomic, int32 alpha) {    // TODO: decomp src/CVehicle/*.c
    (void)atomic;
    (void)alpha;
}

bool CVehicle::IsRealBike() const { return m_pHandlingData->m_bIsBike; }

bool CVehicle::IsRealHeli() const { return m_pHandlingData->m_bIsHeli; }

bool CVehicle::IsRealPlane() const { return m_pHandlingData->m_bIsPlane; }

bool CVehicle::IsRealBoat() const { return m_pHandlingData->m_bIsBoat; }

CVehicleModelInfo* CVehicle::GetVehicleModelInfo() const {
    return CModelInfo::GetModelInfo(m_nModelIndex)->AsVehicleModelInfoPtr();
}

CVector CVehicle::GetDummyPositionObjSpace(eVehicleDummy dummy) const {
    return GetVehicleModelInfo()->GetModelDummyPosition(dummy);
}

CVector CVehicle::GetDummyPosition(eVehicleDummy dummy, bool bWorldSpace) {    // TODO: decomp src/CVehicle/*.c
    (void)dummy;
    (void)bWorldSpace;
    return {};
}

CVector CVehicle::GetDriverSeatDummyPositionOS() const {
    return GetDummyPositionObjSpace(
        IsBoat() ? DUMMY_LIGHT_FRONT_MAIN : DUMMY_SEAT_FRONT
    );
}

CVector CVehicle::GetDriverSeatDummyPositionWS() {    // TODO: decomp src/CVehicle/*.c
    return {};
}

float CVehicle::GetNewSteeringAmt() {    // TODO: decomp src/CVehicle/*.c
    return 0.0f;
}

AssocGroupId CVehicle::GetAnimGroupId() const {    // TODO: decomp src/CVehicle/*.c
    return {};
}

float CVehicle::GetDefaultAirResistance() const {
    if (m_pHandlingData->m_fDragMult <= 0.01f) {
        return m_pHandlingData->m_fDragMult;
    } else {
        return m_pHandlingData->m_fDragMult / 1000.0f / 2.0f;
    }
}

bool CVehicle::IsDriverAPlayer() const {    // TODO: decomp src/CVehicle/*.c
    return false;
}

bool IsValidModForVehicle(uint32 modelId, CVehicle* vehicle) {    // TODO: decomp src/CVehicle/*.c
    (void)modelId;
    (void)vehicle;
    return false;
}

bool IsVehiclePointerValid(CVehicle* vehicle) {    // TODO: decomp src/CVehicle/*.c
    (void)vehicle;
    return false;
}

RpAtomic* RemoveUpgradeCB(RpAtomic* atomic, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)atomic;
    (void)data;
    return nullptr;
}

RpAtomic* FindUpgradeCB(RpAtomic* atomic, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)atomic;
    (void)data;
    return nullptr;
}

RwObject* RemoveObjectsCB(RwObject* object, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)object;
    (void)data;
    return nullptr;
}

RwFrame* RemoveObjectsCB(RwFrame* component, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)component;
    (void)data;
    return nullptr;
}

RwObject* CopyObjectsCB(RwObject* object, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)object;
    (void)data;
    return nullptr;
}

RwObject* FindReplacementUpgradeCB(RwObject* object, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)object;
    (void)data;
    return nullptr;
}

RpAtomic* RemoveAllUpgradesCB(RpAtomic* atomic, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)atomic;
    (void)data;
    return nullptr;
}

RpMaterial* SetCompAlphaCB(RpMaterial* material, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)material;
    (void)data;
    return nullptr;
}

RwObject* SetVehicleAtomicVisibilityCB(RwObject* object, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)object;
    (void)data;
    return nullptr;
}

RwFrame* SetVehicleAtomicVisibilityCB(RwFrame* component, void* data) {    // TODO: decomp src/CVehicle/*.c
    (void)component;
    (void)data;
    return nullptr;
}

void DestroyVehicleAndDriverAndPassengers(CVehicle* vehicle) {    // TODO: decomp src/CVehicle/*.c
    (void)vehicle;
}

void SetVehicleAtomicVisibility(RpAtomic* atomic, int16 state) {    // TODO: decomp src/CVehicle/*.c
    (void)atomic;
    (void)state;
}

// No stub: reference-returning declarations over incomplete types
// (defined when the subsystem is ported):
//   CVehicleAnimGroup& CVehicle::GetAnimGroup();
