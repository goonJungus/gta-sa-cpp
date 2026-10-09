// CTrain.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CTrain/*.c
// Bodies adapted from gta-reversed (source/game_sa/Entity/Vehicle/Train.cpp),
// verified against the decomp. Methods gta-reversed left as plugin::Call stubs
// keep their `// TODO: decomp src/CTrain/*.c` markers. The train-track system
// (CTrainNode, track files) is not ported yet.

#include "CTrain.h"

#include "CModelInfo.h"
#include "CTimer.h"
#include "CVehicleModelInfo.h"
#include "tHandlingData.h" // tHandlingData (full def for member access)
#include "CWorld.h"
#include "RenderWare.h"

#include <cmath>
#include <cstdint>
#include <cstring>

// ---- Deferred-subsystem shims (TODO(port)) ---------------------------------

// CGeneral: random numbers (General.h not ported)
class CGeneral {
public:
    static int32 GetRandomNumber();
    static float GetRandomNumberInRange(float min, float max);
};

// CHandlingDataMgr (HandlingDataMgr.h not ported)
class CHandlingDataMgr {
public:
    tHandlingData* GetVehiclePointer(int32 id);
};
extern CHandlingDataMgr gHandlingDataMgr;

// CClumpModelInfo: real header via CVehicleModelInfo.h (deduped 2026-10-09;
// the local shim here redefined the class).

// PI/TWO_PI (not provided by the tree here)
#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692f
#endif

// Model IDs: canonical eModelID.h (deduped 2026-10-09; local STREAKC=538
// was wrong - decomp src_prev_export/_types.h says 570).

// 0x6F6030
CTrain::CTrain(int32 modelIndex, eVehicleCreatedBy createdBy) : CVehicle(createdBy) {
    // Converted from gta-reversed (the plugin::Call there wraps this body).
    m_nVehicleSubType = VEHICLE_TYPE_TRAIN;
    m_nVehicleType = VEHICLE_TYPE_TRAIN;

    const auto mi = CModelInfo::GetModelInfo(modelIndex)->AsVehicleModelInfoPtr();
    m_pHandlingData = gHandlingDataMgr.GetVehiclePointer(mi->m_nHandlingId);
    m_nHandlingFlagsIntValue = m_pHandlingData->m_nHandlingFlags;

    CVehicle::SetModelIndex(modelIndex);
    SetupModelNodes();

    std::memset(&m_aDoors, 0, sizeof(m_aDoors));
    if (m_nModelIndex == MODEL_STREAKC) {
        m_aDoors[DOOR_LEFT_FRONT].Init(1.25f, 0.25f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_aDoors[DOOR_RIGHT_FRONT].Init(1.25f, 0.25f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    } else {
        m_aDoors[DOOR_LEFT_FRONT].Init(TWO_PI / -5.0f, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        m_aDoors[DOOR_RIGHT_FRONT].Init(TWO_PI / +5.0f, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
    }

    trainFlags.bClockwiseDirection = true;
    trainFlags.bIsLastCarriage = true;
    trainFlags.bIsFrontCarriage = true;
    trainFlags.bStopsAtStations = true;

    m_nPassengersGenerationState = 0;
    m_nNumPassengersToEnter = CGeneral::GetRandomNumber() & 3;
    m_nNumPassengersToLeave = (CGeneral::GetRandomNumber() & 3) + 1;
    m_pTemporaryPassenger = nullptr;
    m_nMaxPassengers = 5;
    physicalFlags.bDisableSimpleCollision = true;
    SetUsesCollision(true);
    m_nTimeWhenCreated = CTimer::GetTimeInMS();
    field_5C8 = 0;
    m_nTrackId = 0;
    m_fCurrentRailDistance = 0.0f;
    m_fTrainSpeed = 0.0f;
    m_nTimeWhenStoppedAtStation = 0;
    mi->ChooseVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor, 1);
    m_fMass = m_pHandlingData->m_fMass;
    m_fTurnMass = m_pHandlingData->m_fTurnMass;
    m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    m_fElasticity = 0.05f;
    m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;
    m_fAirResistance = GetDefaultAirResistance();

    physicalFlags.bAddMovingCollisionSpeed = true;
    m_bTunnelTransition = true;
    m_pPrevCarriage = nullptr;
    m_pNextCarriage = nullptr;
    SetStatus(STATUS_TRAIN_MOVING);
    // TODO(port): CAutoPilot not ported (m_autoPilot.m_speed, SetCruiseSpeed).
    // TODO(port): m_vehicleAudio.Initialise(this) - CAEVehicleAudioEntity not ported.
}

// 0x6F7440
void CTrain::InitTrains() {
    // TODO: decomp src/CTrain/InitTrains_006f7440.c (needs CTrainNode, track files)
}

// 0x6F6F20
void CTrain::ReadAndInterpretTrackFile(const char* filename, CTrainNode** nodes, int32* lineCount, float* totalDist, int32 skipStations) {
    // TODO: decomp src/CTrain/ReadAndInterpretTrackFile_006f6f20.c
}

// 0x6F7A80
void CTrain::Shutdown() {
    // TODO: decomp src/CTrain/Shutdown_006f7a80.c
}

// 0x6F7B20
void CTrain::UpdateTrains() {
    // TODO: decomp src/CTrain/UpdateTrains_006f7b20.c
}

// 0x6F7E50
void CTrain::FindCoorsFromPositionOnTrack(float railDistance, int32 trackId, CVector* outCoors) {
    // TODO: decomp src/CTrain/FindCoorsFromPositionOnTrack_006f7e50.c
}

// 0x6F7EA0
bool CTrain::FindMaximumSpeedToStopAtStations(float* speed) {
    // TODO: decomp src/CTrain/FindMaximumSpeedToStopAtStations_006f7ea0.c
    return false;
}

// 0x6F7F10
uint32 CTrain::FindNumCarriagesPulled() {
    // TODO: decomp src/CTrain/FindNumCarriagesPulled_006f7f10.c
    return 0;
}

// 0x6F7F60
void CTrain::OpenTrainDoor(float state) {
    // TODO: decomp src/CTrain/OpenTrainDoor_006f7f60.c
}

// 0x6F7FA0
void CTrain::AddPassenger(CPed* ped) {
    // TODO: decomp src/CTrain/AddPassenger_006f7fa0.c
}

// 0x6F7FE0
void CTrain::RemovePassenger(CPed* ped) {
    // TODO: decomp src/CTrain/RemovePassenger_006f7fe0.c
}

// 0x6F8020
void CTrain::DisableRandomTrains(bool disable) {
    // TODO: decomp src/CTrain/DisableRandomTrains_006f8020.c
}

// 0x6F8060
void CTrain::RemoveOneMissionTrain(CTrain* train) {
    // TODO: decomp src/CTrain/RemoveOneMissionTrain_006f8060.c
}

// 0x6F80D0
void CTrain::ReleaseOneMissionTrain(CTrain* train) {
    // TODO: decomp src/CTrain/ReleaseOneMissionTrain_006f80d0.c
}

// 0x6F8120
void CTrain::SetTrainSpeed(CTrain* train, float speed) {
    // TODO: decomp src/CTrain/SetTrainSpeed_006f8120.c
}

// 0x6F8150
void CTrain::SetTrainCruiseSpeed(CTrain* train, float speed) {
    // TODO: decomp src/CTrain/SetTrainCruiseSpeed_006f8150.c
}

// 0x6F8180
CTrain* CTrain::FindCaboose(CTrain* train) {
    // TODO: decomp src/CTrain/FindCaboose_006f8180.c
    return nullptr;
}

// 0x6F81B0
CTrain* CTrain::FindEngine(CTrain* train) {
    // TODO: decomp src/CTrain/FindEngine_006f81b0.c
    return nullptr;
}

// 0x6F81E0
CTrain* CTrain::FindCarriage(CTrain* train, uint8 carriage) {
    // TODO: decomp src/CTrain/FindCarriage_006f81e0.c
    return nullptr;
}

// 0x6F8210
bool CTrain::FindSideStationIsOn() const {
    // TODO: decomp src/CTrain/FindSideStationIsOn_006f8210.c
    return false;
}

// 0x6F8240
bool CTrain::IsInTunnel() const {
    // TODO: decomp src/CTrain/IsInTunnel_006f8240.c
    return false;
}

// 0x6F82C0
void CTrain::RemoveRandomPassenger() {
    // TODO: decomp src/CTrain/RemoveRandomPassenger_006f82c0.c
}

// 0x6F82F0
void CTrain::RemoveMissionTrains() {
    // TODO: decomp src/CTrain/RemoveMissionTrains_006f82f0.c
}

// 0x6F8330
void CTrain::RemoveAllTrains() {
    // TODO: decomp src/CTrain/RemoveAllTrains_006f8330.c
}

// 0x6F8360
void CTrain::ReleaseMissionTrains() {
    // TODO: decomp src/CTrain/ReleaseMissionTrains_006f8360.c
}

// 0x6F8390
int32 CTrain::FindClosestTrackNode(CVector posn, int32* outTrackId) {
    // TODO: decomp src/CTrain/FindClosestTrackNode_006f8390.c
    return -1;
}

// 0x6F83E0
void CTrain::FindPositionOnTrackFromCoors() {
    // TODO: decomp src/CTrain/FindPositionOnTrackFromCoors_006f83e0.c
}

// 0x6F8420
CTrain* CTrain::FindNearestTrain(CVector posn, bool mustBeMainTrain) {
    // TODO: decomp src/CTrain/FindNearestTrain_006f8420.c
    return nullptr;
}

// 0x6F8470
void CTrain::SetNewTrainPosition(CTrain* train, CVector posn) {
    // TODO: decomp src/CTrain/SetNewTrainPosition_006f8470.c
}

// 0x6F84B0
bool CTrain::IsNextStationAllowed(CTrain* train) {
    // TODO: decomp src/CTrain/IsNextStationAllowed_006f84b0.c
    return false;
}

// 0x6F84E0
void CTrain::SkipToNextAllowedStation(CTrain* train) {
    // TODO: decomp src/CTrain/SkipToNextAllowedStation_006f84e0.c
}

// 0x6F8520
void CTrain::CreateMissionTrain(CVector posn, bool clockwiseDirection, uint32 trainType, CTrain** outFirstCarriage, CTrain** outLastCarriage, int32 nodeIndex, int32 trackId, bool isMissionTrain) {
    // TODO: decomp src/CTrain/CreateMissionTrain_006f8520.c
}

// 0x6F88A0
void CTrain::DoTrainGenerationAndRemoval() {
    // TODO: decomp src/CTrain/DoTrainGenerationAndRemoval_006f88a0.c
}

// 0x6F8B20
void CTrain::AddNearbyPedAsRandomPassenger() {
    // TODO: decomp src/CTrain/AddNearbyPedAsRandomPassenger_006f8b20.c
}

// 0x6F8B70
void CTrain::ProcessControl() {
    // TODO: decomp src/CTrain/ProcessControl_006f8b70.c
}

// 0x6F62A0
void CTrain::SetupModelNodes() {
    for (auto& node : m_aTrainNodes) {
        node = nullptr;
    }
    CClumpModelInfo::FillFrameArray(GetRpClump(), m_aTrainNodes.data());
}
