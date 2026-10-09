// CBoat.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CBoat/*.c
// Bodies adapted from gta-reversed (source/game_sa/Entity/Vehicle/Boat.cpp),
// verified against the decomp. Methods gta-reversed left as plugin::Call stubs
// are converted from the decompiled .c files where noted; the rest keep their
// `// TODO: decomp src/CBoat/*.c` markers.

#include "CBoat.h"

#include "CModelInfo.h"
#include "CTimer.h"
#include "CVehicleModelInfo.h"
#include "tHandlingData.h" // tHandlingData (full def for member access)
#include "CWorld.h"
#include "RenderWare.h"

#include <cmath>
#include <cstdint>

// ---- Deferred-subsystem shims (TODO(port)) ---------------------------------

// CGeneral: random numbers (General.h not ported)
class CGeneral {
public:
    static int32 GetRandomNumber();
    static float GetRandomNumberInRange(float min, float max);
};

// FxSystem_c (Fx/FxSystem.h not ported)
class FxSystem_c {
public:
    void Kill();
    static void SafeKillAndClear(FxSystem_c*& system);
};

// CHandlingDataMgr (HandlingDataMgr.h not ported)
class CHandlingDataMgr {
public:
    tHandlingData* GetVehiclePointer(int32 id);
    tFlyingHandlingData* GetFlyingPointer(uint8 id);
    tBoatHandlingData* GetBoatPointer(uint8 id);
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

// Model IDs: canonical eModelID.h (deduped 2026-10-09; MARQUIS=484 verified).

// 0x6F0070
CBoat::CBoat(int32 modelIndex, eVehicleCreatedBy createdBy) : CVehicle(createdBy) {
    CVehicleModelInfo* mi = CModelInfo::GetModelInfo(modelIndex)->AsVehicleModelInfoPtr();

    m_BoatDoor = {};

    m_nVehicleType = VEHICLE_TYPE_BOAT;
    m_nVehicleSubType = VEHICLE_TYPE_BOAT;

    m_PadNum = 0;
    m_Scan = 0.0f;

    m_PropellerAngle = 0.0f;

    m_OldMoveSpeed.Reset();
    m_OldTurnSpeed.Reset();

    m_NextTalkTimer = CTimer::GetTimeInMS();

    m_EngineSpeed   = 0.0f;
    CVehicle::SetModelIndex(modelIndex);
    SetupModelNodes();

    m_pHandlingData = gHandlingDataMgr.GetVehiclePointer(mi->m_nHandlingId);
    m_nHandlingFlagsIntValue = m_pHandlingData->m_nHandlingFlags;
    m_pFlyingHandlingData = gHandlingDataMgr.GetFlyingPointer(static_cast<uint8>(mi->m_nHandlingId));
    m_BoatHandling = gHandlingDataMgr.GetBoatPointer(static_cast<uint8>(mi->m_nHandlingId));

    mi->ChooseVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor, 1);

    m_fMass = m_pHandlingData->m_fMass;
    m_fTurnMass = m_pHandlingData->m_fTurnMass * 0.5f;
    m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
    m_fElasticity = 0.1f;
    m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;

    m_fAirResistance = GetDefaultAirResistance();

    physicalFlags.bTouchingWater = true;
    physicalFlags.bSubmergedInWater = true;
    m_nBoatFlags.bLockedToXY = true;
    m_nBoatFlags.bBoatEngineInWater = true;
    m_nBoatFlags.bBoatInWater = true;

    m_fSteerAngle = 0.0f;
    m_GasPedal = 0.0f;
    m_BrakePedal = 0.0f;
    m_fRawSteerAngle = 0.0f;
    m_CurrentField = 0;
    m_PrevVolume = 7.0f;
    m_TimeOfLastParticle = 0;

    m_LockedHeading = -10000.0f;
    m_BlowUpTimer = 0.0f;
    m_EntityThatSetUsOnFire = nullptr;
    m_NumWakeCoords = 0;
    for (auto& counter : m_WakePtCounters) {
        counter = 0.0f;
    }

    m_nAmmoInClip = 20;
    if (m_nModelIndex == MODEL_MARQUIS) {
        m_BoatDoor.Init(PI / 10.0f, -PI / 10.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Y, DOOR_EXTRA_NONE);
    } else {
        m_BoatDoor.Init(TWO_PI / 10.0f, -TWO_PI / 10.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_NONE);
    }

    // TODO(port): m_vehicleAudio.Initialise(this) - CAEVehicleAudioEntity not ported.
    for (auto& fx : m_fxSysProp) {
        fx = nullptr;
    }
}

// 0x6F00F0
CBoat::~CBoat() {
    FxSystem_c::SafeKillAndClear(m_pFireParticle);
    for (auto& fx : m_fxSysProp) {
        FxSystem_c::SafeKillAndClear(fx);
    }
    // TODO(port): m_vehicleAudio.Terminate() - CAEVehicleAudioEntity not ported.
}

// 0x6F01A0
void CBoat::SetupModelNodes() {
    for (auto& node : m_BoatNodes) {
        node = nullptr;
    }
    CClumpModelInfo::FillFrameArray(GetRpClump(), m_BoatNodes.data());
}

// 0x6F01D0
void CBoat::DebugCode() {
    // TODO: decomp src/CBoat/DebugCode_006f01d0.c (debug/unused)
}

// 0x6F0230
void CBoat::DisplayHandlingData() {
    // TODO: decomp src/CBoat/DisplayHandlingData_006f0230.c (debug/unused)
}

// 0x6F0280
void CBoat::ModifyHandlingValue(const bool& plus) {
    // TODO: decomp src/CBoat/ModifyHandlingValue_006f0280.c (debug/unused)
}

// 0x6F02D0
void CBoat::PruneWakeTrail() {
    // TODO: decomp src/CBoat/PruneWakeTrail_006f02d0.c
}

// 0x6F0340
void CBoat::AddWakePoint(CVector pos) {
    // TODO: decomp src/CBoat/AddWakePoint_006f0340.c
}

// 0x6F0450
void CBoat::RenderWakePoints() {
    // TODO: decomp src/CBoat/RenderWakePoints_006f0450.c
}

// 0x6F05B0
void CBoat::RenderAllWakePointBoats() {
    // TODO: decomp src/CBoat/RenderAllWakePointBoats_006f05b0.c
}

// 0x6F0620
bool CBoat::IsSectorAffectedByWake(CVector2D centreCoords, float semiSize, CBoat** ppBoats) {
    // TODO: decomp src/CBoat/IsSectorAffectedByWake_006f0620.c
    return false;
}

// 0x6F06D0
float CBoat::IsVertexAffectedByWake(CVector coords, CBoat* boat, int16 wakeQuadrant, bool forceCheck) {
    // TODO: decomp src/CBoat/IsVertexAffectedByWake_006f06d0.c
    return 0.0f;
}

// 0x6F0790
void CBoat::CheckForSkippingCalculations() {
    // TODO: decomp src/CBoat/CheckForSkippingCalculations_006f0790.c
}

// 0x6F07E0
void CBoat::FillBoatList() {
    // TODO: decomp src/CBoat/FillBoatList_006f07e0.c
}

// 0x6F0990
void CBoat::SetModelIndex(uint32 index) {
    // TODO: decomp src/CBoat/SetModelIndex_006f0990.c
}

// 0x6F09E0
void CBoat::ProcessControl() {
    // TODO: decomp src/CBoat/ProcessControl_006f09e0.c
}

// 0x6F0D30
void CBoat::Teleport(CVector newCoors, bool clearOrientation) {
    // TODO: decomp src/CBoat/Teleport_006f0d30.c
}

// 0x6F0D90
void CBoat::PreRender() {
    // TODO: decomp src/CBoat/PreRender_006f0d90.c
}

// 0x6F0F40
void CBoat::Render() {
    // TODO: decomp src/CBoat/Render_006f0f40.c
}

// 0x6F12A0
void CBoat::ProcessControlInputs(uint8 padNum) {
    // TODO: decomp src/CBoat/ProcessControlInputs_006f12a0.c
}

// 0x6F1340
void CBoat::GetComponentWorldPosition(int32 componentId, CVector& posn) {
    // TODO: decomp src/CBoat/GetComponentWorldPosition_006f1340.c
}

// 0x6F1370
bool CBoat::IsComponentPresent(int32 component) const {
    // TODO: decomp src/CBoat/IsComponentPresent_006f1370.c
    return false;
}

// 0x6F13A0
void CBoat::BlowUpCar(CEntity* culprit, bool inACutscene) {
    // TODO: decomp src/CBoat/BlowUpCar_006f13a0.c
}
