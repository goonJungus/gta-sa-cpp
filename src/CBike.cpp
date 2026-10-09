// CBike.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CBike/*.c
// Bodies adapted from gta-reversed (source/game_sa/Entity/Vehicle/Bike.cpp),
// verified against the decomp. Methods gta-reversed left as plugin::Call stubs
// are converted from the decompiled .c files where noted; the rest keep their
// `// TODO: decomp src/CBike/*.c` markers.

#include "CBike.h"

#include "CModelInfo.h"
#include "CTimer.h"
#include "CVehicleModelInfo.h"
#include "tHandlingData.h" // tHandlingData (full def for member access)
#include "CColLine.h" // CColLine (full def for sizeof/member access)
#include "CWorld.h"
#include "RenderWare.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#include <cmath>
#include <cstdint>

// ---- Deferred-subsystem shims (TODO(port)) ---------------------------------

// CGeneral: random numbers (General.h not ported)
class CGeneral {
public:
    static int32 GetRandomNumber();
    static float GetRandomNumberInRange(float min, float max);
};

// CAnimManager (AnimManager.h not ported)
class CAnimManager {
public:
    struct AnimBlock { int32 GroupId; };
    static AnimBlock* GetAnimBlocks();
};

// FxSystem_c / FxManager_c (Fx/FxSystem.h, Fx/FxManager.h not ported)
class FxSystem_c {
public:
    void Kill();
};
class FxManager_c {
public:
    void DestroyFxSystem(FxSystem_c* system);
};
extern FxManager_c& g_fxMan;

// CHandlingDataMgr (HandlingDataMgr.h not ported)
class CHandlingDataMgr {
public:
    tHandlingData* GetVehiclePointer(int32 id);
    tBikeHandlingData* GetBikeHandlingPointer(int32 id);
    tFlyingHandlingData* GetFlyingPointer(uint8 id);
};
extern CHandlingDataMgr gHandlingDataMgr;

// CMemoryMgr (MemoryMgr.h not ported)
class CMemoryMgr {
public:
    static void* Malloc(size_t size);
};

// AssocGroupId: canonical in AnimTypes.h (deduped 2026-10-09; the local
// enum here redefined it AND had wrong values 0,0 - real: BIKES=2/WAYFARER=6).

// 0x6B4F60
CBike::CBike(int32 modelIndex, eVehicleCreatedBy createdBy) : CVehicle(createdBy) {
    auto mi = CModelInfo::GetModelInfo(modelIndex)->AsVehicleModelInfoPtr();
    if (mi->m_nVehicleType == VEHICLE_TYPE_BIKE) {
        const auto& animationStyle = CAnimManager::GetAnimBlocks()[mi->GetAnimFileIndex()].GroupId;
        m_RideAnimData.AnimGroup = static_cast<AssocGroupId>(animationStyle); // GroupId is int32 (2026-10-09)
        if (animationStyle < ANIM_GROUP_BIKES || animationStyle > ANIM_GROUP_WAYFARER) {
            m_RideAnimData.AnimGroup = ANIM_GROUP_BIKES;
        }
    }

    m_nVehicleSubType = VEHICLE_TYPE_BIKE;
    m_nVehicleType = VEHICLE_TYPE_BIKE;

    m_BlowUpTimer = 0.0f;
    m_nBrakesOn = false;
    nBikeFlags = 0;
    SetModelIndex(modelIndex);

    m_pHandlingData = gHandlingDataMgr.GetVehiclePointer(mi->m_nHandlingId);
    m_BikeHandling = gHandlingDataMgr.GetBikeHandlingPointer(mi->m_nHandlingId);
    m_nHandlingFlagsIntValue = m_pHandlingData->m_nHandlingFlags;
    m_pFlyingHandlingData = gHandlingDataMgr.GetFlyingPointer(static_cast<uint8>(mi->m_nHandlingId));
    m_fBrakeCount = 20.0f;
    mi->ChooseVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor, 1);
    m_fSwingArmLength = 0.0f;
    m_fForkYOffset = 0.0f;
    m_fForkZOffset = 0.0f;
    m_nFixLeftHand = false;
    m_nFixRightHand = false;
    m_fSteerAngleTan = std::tan((mi->m_fBikeSteerAngle * PI) / 180.0f);
    m_fMass = m_pHandlingData->m_fMass;
    m_fTurnMass = m_pHandlingData->m_fTurnMass;
    m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    m_vecCentreOfMass.z = 0.1f;
    m_fAirResistance = GetDefaultAirResistance();
    m_fElasticity = 0.05f;
    m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;
    m_fSteerAngle = 0.0f;
    m_GasPedal = 0.0f;
    m_BrakePedal = 0.0f;
    m_Damager = nullptr;
    m_pWhoInstalledBombOnMe = nullptr;
    m_GasPedalAudioRevs = 0.0f;
    m_fTyreTemp = 1.0f;
    m_fBrakingSlide = 0.0f;
    m_PrevSpeed = 0.0f;

    for (auto i = 0; i < 2; ++i) {
        m_nWheelStatus[i] = 0;
        m_aWheelSkidmarkType[i] = eSkidmarkType::DEFAULT;
        m_bWheelBloody[i] = false;
        m_bMoreSkidMarks[i] = false;
        m_aWheelPitchAngles[i] = 0.0f;
        m_aWheelAngularVelocity[i] = 0.0f;
        m_aWheelSuspensionHeights[i] = 0.0f;
        m_aWheelOrigHeights[i] = 0.0f;
        m_WheelStates[i] = WHEEL_STATE_NORMAL;
    }

    for (auto i = 0; i < 4; ++i) {
        m_aWheelColPoints[i] = {};
        m_aWheelRatios[i] = 1.0f;
        m_aRatioHistory[i] = 0.0f;
        m_WheelCounts[i] = 0.0f;
        m_fSuspensionLength[i] = 0.0f;
        m_fLineLength[i] = 0.0f;
        m_aGroundPhysicalPtrs[i] = nullptr;
        m_aGroundOffsets[i] = CVector{};
    }

    m_nNoOfContactWheels = 0;
    m_NumDriveWheelsOnGround = 0;
    m_NumDriveWheelsOnGroundLastFrame = 0;
    m_fHeightAboveRoad = 0.0f;
    m_fExtraTractionMult = 1.0f;

    if (!mi->m_pColModel->m_pColData->m_pLines) {
        mi->m_pColModel->m_pColData->m_nNumLines = 4;
        mi->m_pColModel->m_pColData->m_pLines = static_cast<CColLine*>(CMemoryMgr::Malloc(4 * sizeof(CColLine)));
        mi->m_pColModel->m_pColData->m_pLines[1].m_vecStart.x = 99999.99f; // todo: explain this
    }
    mi->m_pColModel->m_pColData->m_pLines[0].m_vecStart.z = 99999.99f;
    CBike::SetupSuspensionLines();

    // TODO(port): CAutoPilot not ported (m_autoPilot.m_nTempAction etc.)
    // m_autoPilot.m_nTempAction = TEMPACT_NONE;
    // m_autoPilot.SetCarMission(MISSION_NONE, 0);
    // m_autoPilot.carCtrlFlags.bAvoidLevelTransitions = false;

    SetStatus(STATUS_SIMPLE);
    m_nNumPassengers = 0;
    vehicleFlags.bLowVehicle = false;
    vehicleFlags.bIsBig = false;
    vehicleFlags.bIsVan = false;

    m_bLeanMatrixCalculated = false;
    m_mLeanMatrix = *m_matrix;
    m_vecOldSpeedForPlayback = CVector{};
    // TODO(port): m_vehicleAudio.Initialise(this) - CAEVehicleAudioEntity not ported.
}

// 0x6B57A0
CBike::~CBike() {
    // TODO(port): m_vehicleAudio.Terminate() - CAEVehicleAudioEntity not ported.
}

// 0x6B5E00
void CBike::dmgDrawCarCollidingParticles(const CVector& position, float power, eWeaponType weaponType) {
    // TODO: decomp src/CBike/dmgDrawCarCollidingParticles_006b5e00.c
}

// 0x6B5E30
bool CBike::DamageKnockOffRider(CVehicle* vehicle, float damageIntensity, uint16 pieceType, CEntity* damager, const CVector& collisionPos, const CVector& collisionImpactVelocity) {
    // TODO: decomp src/CBike/DamageKnockOffRider_006b5e30.c
    return false;
}

// 0x6B6450
CPed* CBike::KnockOffRider(eWeaponType arg0, uint8 arg1, CPed* ped, bool arg3) {
    // TODO: decomp src/CBike/KnockOffRider_006b6450.c
    return nullptr;
}

// 0x6B64A0
void CBike::SetRemoveAnimFlags(CPed* ped) {
    // TODO: decomp src/CBike/SetRemoveAnimFlags_006b64a0.c
}

// 0x6B64D0
void CBike::ReduceHornCounter() {
    // TODO: decomp src/CBike/ReduceHornCounter_006b64d0.c
}

// 0x6B65A0
void CBike::ProcessBuoyancy() {
    // TODO: decomp src/CBike/ProcessBuoyancy_006b65a0.c
}

// 0x6B6A10
bool CBike::ProcessAI(uint32& extraHandlingFlags) {
    // TODO: decomp src/CBike/ProcessAI_006b6a10.c
    return false;
}

// 0x6B6A50
void CBike::ProcessDrivingAnims(CPed* driver, bool blend) {
    // TODO: decomp src/CBike/ProcessDrivingAnims_006b6a50.c
}

// 0x6B6AC0
void CBike::ProcessRiderAnims(CPed* rider, CVehicle* vehicle, CRideAnimData* rideData, tBikeHandlingData* handling, int16 a5) {
    // TODO: decomp src/CBike/ProcessRiderAnims_006b6ac0.c
}

// 0x6B6B30
bool CBike::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    // TODO: decomp src/CBike/BurstTyre_006b6b30.c
    return false;
}

// 0x6B6B70
void CBike::ProcessControlInputs(uint8 playerNum) {
    // TODO: decomp src/CBike/ProcessControlInputs_006b6b70.c
}

// 0x6B6C20
int32 CBike::ProcessEntityCollision(CEntity* entity, CColPoint* outColPoints) {
    // TODO: decomp src/CBike/ProcessEntityCollision_006b6c20.c
    return 0;
}

// 0x6B7280
void CBike::ProcessControl() {
    // TODO: decomp src/CBike/ProcessControl_006b7280.c
}

// 0x6B7E10
void CBike::ResetSuspension() {
    // TODO: decomp src/CBike/ResetSuspension_006b7e10.c
}

// 0x6B7E40
bool CBike::GetAllWheelsOffGround() const {
    // TODO: decomp src/CBike/GetAllWheelsOffGround_006b7e40.c
    return false;
}

// 0x6B7E70
void CBike::DebugCode() {
    // TODO: decomp src/CBike/DebugCode_006b7e70.c
}

// 0x6B7EA0
void CBike::DoSoftGroundResistance(uint32& arg0) {
    // TODO: decomp src/CBike/DoSoftGroundResistance_006b7ea0.c
}

// 0x6B7F10
void CBike::PlayHornIfNecessary() {
    // TODO: decomp src/CBike/PlayHornIfNecessary_006b7f10.c
}

// 0x6B7F60
void CBike::CalculateLeanMatrix() {
    // TODO: decomp src/CBike/CalculateLeanMatrix_006b7f60.c
}

// 0x6B80A0
void CBike::FixHandsToBars(CPed* rider) {
    // TODO: decomp src/CBike/FixHandsToBars_006b80a0.c
}

// 0x6B80D0
void CBike::PlaceOnRoadProperly() {
    // TODO: decomp src/CBike/PlaceOnRoadProperly_006b80d0.c
}

// 0x6B8120
void CBike::GetCorrectedWorldDoorPosition(CVector& out, CVector arg1, CVector arg2) {
    // TODO: decomp src/CBike/GetCorrectedWorldDoorPosition_006b8120.c
}

// 0x6B8180
void CBike::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    // TODO: decomp src/CBike/BlowUpCar_006b8180.c
}

// 0x6B84D0
void CBike::Fix() {
    // TODO: decomp src/CBike/Fix_006b84d0.c
}

// 0x6B8520
void CBike::PreRender() {
    // TODO: decomp src/CBike/PreRender_006b8520.c
}

// 0x6B85A0
void CBike::Render() {
    // TODO: decomp src/CBike/Render_006b85a0.c
}

// 0x6B86F0
void CBike::Teleport(CVector destination, bool resetRotation) {
    // TODO: decomp src/CBike/Teleport_006b86f0.c
}

// 0x6B87A0
void CBike::VehicleDamage(float damageIntensity, eVehicleCollisionComponent component, CEntity* damager, CVector* vecCollisionCoors, CVector* vecCollisionDirection, eWeaponType weapon) {
    // TODO: decomp src/CBike/VehicleDamage_006b87a0.c
}

// 0x6B87F0
void CBike::SetupSuspensionLines() {
    // TODO: decomp src/CBike/SetupSuspensionLines_006b87f0.c
}

// 0x6B88A0
void CBike::SetModelIndex(uint32 index) {
    // TODO: decomp src/CBike/SetModelIndex_006b88a0.c
}

// 0x6B8920
void CBike::SetupModelNodes() {
    // TODO: decomp src/CBike/SetupModelNodes_006b8920.c
}

// 0x6B8980
void CBike::PlayCarHorn() {
    // TODO: decomp src/CBike/PlayCarHorn_006b8980.c
}

// 0x6B89C0
void CBike::SetupDamageAfterLoad() {
    // TODO: decomp src/CBike/SetupDamageAfterLoad_006b89c0.c
}

// 0x6B89F0
void CBike::DoBurstAndSoftGroundRatios() {
    // TODO: decomp src/CBike/DoBurstAndSoftGroundRatios_006b89f0.c
}

// 0x6B8A40
bool CBike::SetUpWheelColModel(CColModel* wheelCol) {
    // TODO: decomp src/CBike/SetUpWheelColModel_006b8a40.c
    return false;
}

// 0x6B8A90
void CBike::RemoveRefsToVehicle(CEntity* entityToRemove) {
    // TODO: decomp src/CBike/RemoveRefsToVehicle_006b8a90.c
}

// 0x6B8B20
void CBike::ProcessControlCollisionCheck(bool applySpeed) {
    // TODO: decomp src/CBike/ProcessControlCollisionCheck_006b8b20.c
}

// 0x6B8E50
void CBike::GetComponentWorldPosition(int32 componentId, CVector& outPos) {
    // TODO: decomp src/CBike/GetComponentWorldPosition_006b8e50.c
}

// 0x6B8EA0
void CBike::ProcessOpenDoor(CPed* ped, uint32 doorComponentId, uint32 animGroup, uint32 animId, float fTime) {
    // TODO: decomp src/CBike/ProcessOpenDoor_006b8ea0.c
}
