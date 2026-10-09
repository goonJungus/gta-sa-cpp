// CPlane.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CPlane/*.c
// Bodies adapted from gta-reversed (source/game_sa/Entity/Vehicle/Plane.cpp),
// verified against the decomp. Methods gta-reversed left as plugin::Call stubs
// are converted from the decompiled .c files where noted; the rest keep their
// `// TODO: decomp src/CPlane/*.c` markers.

#include "CPlane.h"

#include "CCollision.h"
#include "CDamageManager.h"
#include "CDoor.h"
#include "CModelInfo.h"
#include "CPad.h"
#include "CPed.h"
#include "CTimer.h"
#include "CVehicleModelInfo.h"
#include "CWorld.h"
#include "RenderWare.h"
#include "eVehicleHandlingModelFlags.h"
#include "tFlyingHandlingData.h"
#include "tHandlingData.h"

#include <cmath>
#include <cstdint>

// PI is used by the door math; the tree does not provide it here.
#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef TWO_PI
#define TWO_PI 6.28318530717958647692f
#endif

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

// FxSystem_c / FxManager_c (Fx/FxSystem.h, Fx/FxManager.h not ported)
class FxSystem_c {
public:
    void Kill();
    static void KillAndClear(FxSystem_c*& system);
    static void SafeKillAndClear(FxSystem_c*& system);
};
class FxManager_c {
public:
    void DestroyFxSystem(FxSystem_c* system);
};
extern FxManager_c& g_fxMan;

// CVisibilityPlugins (not ported)
class CVisibilityPlugins {
public:
    static void SetClumpForAllAtomicsFlag(RpClump* clump, uint16 flag);
};

// CCamera (CCamera.h not included; follows the CWeapon.cpp shim precedent)
class CCamera {
public:
    const CVector& GetPosition() const;
};
extern CCamera TheCamera;

// Distance helpers (not ported as free functions)
float DistanceBetweenPoints(const CVector& a, const CVector& b);
float DistanceBetweenPoints2D(const CVector& a, const CVector& b);

// rwObjectSetFlags (not in the ported RenderWare.h)
void rwObjectSetFlags(RwObject* object, uint8 flags);

// Model IDs: canonical eModelID.h (deduped 2026-10-09; the anonymous enum
// here was removed - values verified vs src_prev_export/_types.h).

// Vehicle pool accessor (Pools.h not ported; no-op range - same pattern as
// CVehicle.cpp's CVehiclePool shim, plus GetAllValid for CountPlanesAndHelis).
class CVehiclePool {
public:
    struct EmptyRange {
        struct It {
            bool operator!=(const It&) const { return false; }
            void operator++() {}
            CVehicle& operator*() const;
        };
        It begin() const { return {}; }
        It end() const { return {}; }
    };
    EmptyRange GetAllValid();
};
CVehiclePool* GetVehiclePool();

// Statics (were binary-address refs in gta-reversed; plain statics in the
// port - types match the CPlane.h declarations, 2026-10-09).
int32  CPlane::GenPlane_ModelIndex{};
uint32 CPlane::GenPlane_Status{};
uint32 CPlane::GenPlane_LastTimeGenerated{};
bool   CPlane::GenPlane_Active{};

// 0x6C8E90
CPlane::CPlane(int32 modelIndex, eVehicleCreatedBy createdBy) : CAutomobile(modelIndex, createdBy, true) {
    m_nVehicleSubType = VEHICLE_TYPE_PLANE;

    m_fLeftRightSkid               = 0.0f;
    m_fSteeringUpDown              = 0.0f;
    m_fSteeringLeftRight           = 0.0f;
    m_fAccelerationBreakStatus     = 0.0f;
    m_fAccelerationBreakStatusPrev = 1.0f;
    m_fPropSpeed                   = 0.0f;
    field_9C8                      = 0.0f;
    m_fLandingGearStatus           = 0.0f;
    field_9A0                      = 0;
    m_planeCreationHeading         = 0.0f;
    m_planeHeading                 = 0.0f;
    m_planeHeadingPrev             = 0.0f;
    m_maxAltitude                  = 15.0f;
    m_altitude                     = 25.0f;
    m_minAltitude                  = 20.0f;
    m_forwardZ                     = 0;
    m_nStartedFlyingTime           = 0;
    m_fSteeringFactor              = 0.0f;

    if (m_nModelIndex != MODEL_VORTEX)
        physicalFlags.bDontCollideWithFlyers = true;

    m_nExtendedRemovalRange = 255;
    vehicleFlags.bNeverUseSmallerRemovalRange = true;
    vehicleFlags.bIsBig = true;

    auto& leftDoor = m_doors[DOOR_LEFT_FRONT];
    switch (modelIndex) {
    case MODEL_HYDRA:
    case MODEL_RUSTLER:
    case MODEL_CROPDUST:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init((3.0f * PI) / 5.0f, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
        break;
    case MODEL_SHAMAL:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init(-((3.0f * PI) / 4.0f), 0.0f, DOOR_AXIS_Z, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
        rwObjectSetFlags(GetFirstObject(m_aCarNodes[PLANE_WHEEL_LF]), 0);
        break;
    case MODEL_NEVADA:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init(-TWO_PI / 5.0f, 0.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Z, DOOR_EXTRA_BASED);
        break;
    case MODEL_VORTEX:
        if (m_panels[FRONT_LEFT_PANEL].m_nFrameId == (uint16)-1)
            m_panels[FRONT_LEFT_PANEL].SetPanel(PLANE_GEAR_L, 1, -0.25f);
        break;
    case MODEL_STUNT:
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        leftDoor.Init((3.0f * PI) / 5.0f, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
        rwObjectSetFlags(GetFirstObject(m_aCarNodes[PLANE_WHEEL_LB]), 0);
        rwObjectSetFlags(GetFirstObject(m_aCarNodes[PLANE_WHEEL_RB]), 0);
        break;
    }

    CVector modelPos, localPos;
    for (auto wheelId = 0; wheelId < 4; wheelId++) {
        GetVehicleModelInfo()->GetWheelPosn(wheelId, modelPos, false);
        GetVehicleModelInfo()->GetWheelPosn(wheelId, localPos, true);
        m_wheelPosition[wheelId] = m_wheelPosition[wheelId] - modelPos.z + localPos.z;
    }

    m_planeDamageWave = 0;
    m_pGunParticles = nullptr;
    m_nFiringMultiplier = 16;
    field_9DC = 0;
    field_9E0 = 0;
    m_apJettrusParticles.fill(nullptr);

    m_pSmokeParticle = nullptr;

    if (m_nModelIndex == MODEL_HYDRA)
        m_wMiscComponentAngle = HARRIER_NOZZLE_ROTATE_LIMIT;

    m_bSmokeEjectorEnabled = false;
}

// 0x6C9160
CPlane::~CPlane() {
    if (m_pGunParticles) {
        for (auto i = 0; i < CVehicle::GetPlaneNumGuns(); i++) {
            if (auto& particle = m_pGunParticles[i]) {
                particle->Kill();
                g_fxMan.DestroyFxSystem(particle);
            }
        }
        delete[] m_pGunParticles;
        m_pGunParticles = nullptr;
    }

    for (auto particle : m_apJettrusParticles) {
        if (particle) {
            FxSystem_c::KillAndClear(particle);
        }
    }

    FxSystem_c::SafeKillAndClear(m_pSmokeParticle);

    // TODO(port): m_vehicleAudio.Terminate() - CAEVehicleAudioEntity::Terminate not ported.
}

// 0x6CAD90
void CPlane::InitPlaneGenerationAndRemoval() {
    GenPlane_Status = 0;
    GenPlane_LastTimeGenerated = 0;
    GenPlane_Active = true;
}

// 0x6CCCF0
void CPlane::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    // TODO: decomp src/CPlane/BlowUpCar_006cccf0.c
}

// 0x6CACC0
void CPlane::Fix() {
    m_damageManager.ResetDamageStatus();
    if (m_pHandlingData->m_bNoDoors) {
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_NOTPRESENT);
        m_damageManager.SetDoorStatus(DOOR_RIGHT_FRONT, DAMSTATE_NOTPRESENT);
        m_damageManager.SetDoorStatus(DOOR_LEFT_REAR, DAMSTATE_NOTPRESENT);
        m_damageManager.SetDoorStatus(DOOR_RIGHT_REAR, DAMSTATE_NOTPRESENT);
    }
    SetupDamageAfterLoad();
}

// 0x6CACB0
void CPlane::OpenDoor(CPed* ped, int32 componentId, eDoors door, float doorOpenRatio, bool playSound) {
    CAutomobile::OpenDoor(ped, componentId, door, doorOpenRatio, playSound);

    if (m_nModelIndex != MODEL_STUNT)
        return;

    // Unfinished code R*, which removed in Android
    if (false) // byte_C1CAFC
    {
        CMatrix matrix(RwFrameGetMatrix(m_aCarNodes[componentId]), false);
        const auto y = m_doors[door].m_angle - m_doors[door].m_prevAngle + matrix.GetPosition().y;
        matrix.SetTranslate({matrix.GetPosition().x, y, matrix.GetPosition().z});
        matrix.UpdateRW();
    }
}

// 0x6CAC10
void CPlane::SetupDamageAfterLoad() {
    vehicleFlags.bIsDamaged = false;
}

// 0x6CC4B0
void CPlane::VehicleDamage(float damageIntensity, eVehicleCollisionComponent component, CEntity* damager, CVector* vecCollisionCoors, CVector* vecCollisionDirection, eWeaponType weapon) {
    // TODO: decomp src/CPlane/VehicleDamage_006cc4b0.c
}

// 0x6CAB90
void CPlane::IsAlreadyFlying() {
    m_nStartedFlyingTime = CTimer::GetTimeInMS() - 20000;
}

// 0x6CAC20
void CPlane::SetGearUp() {
    m_fLandingGearStatus = 1.0f;
    m_fAirResistance = m_pHandlingData->m_fDragMult / 1000.0f / 2.0f * m_pFlyingHandlingData->m_fGearUpR;
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_LEFT,  WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_LEFT,   WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_RIGHT, WHEEL_STATUS_MISSING);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_RIGHT,  WHEEL_STATUS_MISSING);
}

// 0x6CAC70
void CPlane::SetGearDown() {
    m_fLandingGearStatus = 0.0f;
    m_fAirResistance = m_pHandlingData->m_fDragMult / 1000.0f / 2.0f;
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_LEFT,  WHEEL_STATUS_OK);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_LEFT,   WHEEL_STATUS_OK);
    m_damageManager.SetWheelStatus(CAR_WHEEL_FRONT_RIGHT, WHEEL_STATUS_OK);
    m_damageManager.SetWheelStatus(CAR_WHEEL_REAR_RIGHT,  WHEEL_STATUS_OK);
}

// 0x6CCA50
uint32 CPlane::CountPlanesAndHelis() {
    uint32 counter = 0;
    for (auto& vehicle : GetVehiclePool()->GetAllValid()) {
        if (vehicle.IsSubHeli() || vehicle.IsSubPlane()) {
            counter++;
        }
    }
    return counter;
}

// 0x6CCAA0
bool CPlane::AreWeInNoPlaneZone() {
    const auto& camPos = TheCamera.GetPosition();
    constexpr CVector vec1 = { -1073.0f, -675.0f, 50.0f };

    return DistanceBetweenPoints(vec1, camPos) < 200.0f ||
           camPos.x > -2743.0f && camPos.x < -2626.0f && camPos.y > 1300.0f && camPos.y < 2200.0f ||
           camPos.x > -1668.0f && camPos.x < -1122.0f && camPos.y > 541.0f && camPos.y < 1118.0f;
}

// 0x6CCBB0
bool CPlane::AreWeInNoBigPlaneZone() {
    const auto& camPos = TheCamera.GetPosition();
    const CVector zone1{ +1522.0f, -1237.0f, 0.0f };
    const CVector zone2{ -1836.0f, +659.0f, 0.0f };
    return DistanceBetweenPoints2D(zone1, camPos) < 800.0f ||
           DistanceBetweenPoints2D(zone2, camPos) < 800.0f;
}

// 0x6CCC50
void CPlane::SwitchAmbientPlanes(bool enable) {
    // TODO: decomp src/CPlane/SwitchAmbientPlanes_006ccc50.c
}

// 0x6CD090
void CPlane::FindPlaneCreationCoors(CVector* outCoors, CVector* outTargetCoors, float* outPlaneOrientation, float* outFlightHeight, bool isBigPlane) {
    // TODO: decomp src/CPlane/FindPlaneCreationCoors_006cd090.c
}

// 0x6CD3B0
void CPlane::DoPlaneGenerationAndRemoval() {
    // TODO: decomp src/CPlane/DoPlaneGenerationAndRemoval_006cd3b0.c
}

// 0x6C9140
bool CPlane::SetUpWheelColModel(CColModel* wheelCol) {
    return false;
}

// 0x6C9150
bool CPlane::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    return false;
}

// 0x6C94A0
void CPlane::PreRender() {
    // TODO: decomp src/CPlane/PreRender_006c94a0.c
}

// 0x6CAB70
void CPlane::Render() {
    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;
    CVehicle::Render();
}

// 0x6C9260
void CPlane::ProcessControl() {
    // Converted from gta-reversed (marked "untested" there; verified structure
    // against decomp src/CPlane/ProcessControl_006c9260.c).
    if (GetStatus() == STATUS_PLAYER) {
        if (m_nModelIndex == MODEL_CROPDUST || m_nModelIndex == MODEL_STUNT) {
            auto pad = CPad::GetPad(m_pDriver->GetPadNumber());
            if (pad->IsRightShockPressed()) {
                m_bSmokeEjectorEnabled = !m_bSmokeEjectorEnabled;
            }
        }
    }

    if (m_bSmokeEjectorEnabled) {
        if (!vehicleFlags.bEngineOn || vehicleFlags.bIsDrowning || !m_pDriver) {
            m_bSmokeEjectorEnabled = false;
        }
    }

    if (m_nModelIndex == MODEL_SKIMMER) {
        m_damageManager.SetAllWheelsState(WHEEL_STATUS_MISSING);
    }

    CAutomobile::ProcessControl();

    // TODO(port): m_vehicleAudio.m_DoCountStalls - CAEVehicleAudioEntity not ported.
    // m_vehicleAudio.m_DoCountStalls = static_cast<int16>(field_9A0);
    if (field_9A0) {
        field_9A0 = 0;
    }

    CVehicle::ProcessWeapons();
    if (m_nModelIndex == MODEL_VORTEX) {
        m_WheelStates[0] = WHEEL_STATE_NORMAL;
        m_WheelStates[1] = WHEEL_STATE_NORMAL;
        m_WheelStates[2] = WHEEL_STATE_NORMAL;
        m_WheelStates[3] = WHEEL_STATE_NORMAL;
    }

#if 0
    // TODO: decomp src/CPlane/ProcessControl_006c9260.c
    // Smoke-ejector particle emission: needs FxPrtMult_c, g_fx.m_SmokeHuge,
    // FxSystem_c::GetCompositeMatrix - Fx subsystem not ported.
#endif
}

// 0x6CADD0
void CPlane::ProcessControlInputs(uint8 playerNum) {
    // TODO: decomp src/CPlane/ProcessControlInputs_006cadd0.c
}

// 0x6CB7C0
void CPlane::ProcessFlyingCarStuff() {
    // TODO: decomp src/CPlane/ProcessFlyingCarStuff_006cb7c0.c
}
