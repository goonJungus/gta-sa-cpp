// CPlane.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Vehicle/Plane.h
// Fixed-wing aircraft: landing gear, propellers, plane generation.
// Hierarchy: ... -> CAutomobile -> CPlane
//
// Adaptations:
//   stripped InjectHooks(), friend InjectHooksMain
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
//   StaticRef statics -> plain static members (defined in CPlane.cpp)
//   FxSystem_c -> forward-declared
// TODO:
//   verify each method against decomp src/CPlane/*.c

#pragma once

#include <array>
#include <cstdint>

#include "GrTypes.h"

#include "CAutomobile.h"

class CPed;
class FxSystem_c;



enum ePlaneNodes {
    PLANE_NODE_NONE = 0,
    PLANE_CHASSIS = 1,
    PLANE_WHEEL_RF = 2,
    PLANE_WHEEL_RM = 3,
    PLANE_WHEEL_RB = 4,
    PLANE_WHEEL_LF = 5,
    PLANE_WHEEL_LM = 6,
    PLANE_WHEEL_LB = 7,
    PLANE_DOOR_RF = 8,
    PLANE_DOOR_RR = 9,
    PLANE_DOOR_LF = 10,
    PLANE_DOOR_LR = 11,
    PLANE_STATIC_PROP = 12,
    PLANE_MOVING_PROP = 13,
    PLANE_STATIC_PROP2 = 14,
    PLANE_MOVING_PROP2 = 15,
    PLANE_RUDDER = 16,
    PLANE_ELEVATOR_L = 17,
    PLANE_ELEVATOR_R = 18,
    PLANE_AILERON_L = 19,
    PLANE_AILERON_R = 20,
    PLANE_GEAR_L = 21,
    PLANE_GEAR_R = 22,
    PLANE_MISC_A = 23,
    PLANE_MISC_B = 24,
    PLANE_NUM_NODES
};

class CPlane : public CAutomobile {
public:
    float        m_fLeftRightSkid;
    float        m_fSteeringUpDown;
    float        m_fSteeringLeftRight;
    float        m_fAccelerationBreakStatus;
    float        m_fAccelerationBreakStatusPrev;
    float        m_fSteeringFactor;
    float        field_9A0;
    float        m_planeCreationHeading; // The heading when plane is created or placed on road properly
    float        m_maxAltitude;
    float        m_altitude;
    float        m_minAltitude;
    float        m_planeHeading;
    float        m_planeHeadingPrev;
    float        m_forwardZ;
    uint32       m_nStartedFlyingTime;
    float        m_fPropSpeed;
    float        field_9C8;
    float        m_fLandingGearStatus;
    int32        m_planeDamageWave;
    FxSystem_c** m_pGunParticles;
    uint8        m_nFiringMultiplier;
    int32        field_9DC;
    int32        field_9E0;
    int32        field_9E4;
    std::array<FxSystem_c*, 4> m_apJettrusParticles;
    FxSystem_c*  m_pSmokeParticle;
    uint32       m_nSmokeTimer;
    bool         m_bSmokeEjectorEnabled;

    static constexpr auto Type = VEHICLE_TYPE_PLANE;

public:
    static int32 GenPlane_ModelIndex; // game address: 0xC1CAD8 ; DEFERRED - was StaticRef
    static uint32 GenPlane_Status; // game address: 0xC1CADC ; DEFERRED - was StaticRef
    static uint32 GenPlane_LastTimeGenerated; // game address: 0xC1CAE0 ; DEFERRED - was StaticRef

    static bool GenPlane_Active; // game address: 0x8D33BC ; DEFERRED - was StaticRef
    static float ANDROM_COL_ANGLE_MULT; // game address: 0x8D33C0 ; DEFERRED - was StaticRef
    static uint16 HARRIER_NOZZLE_ROTATE_LIMIT; // game address: 0x8D33C4 ; DEFERRED - was StaticRef
    static uint16 HARRIER_NOZZLE_SWITCH_LIMIT; // game address: 0x8D33C8 ; DEFERRED - was StaticRef
    static float PLANE_MIN_PROP_SPEED; // game address: 0x8D33CC ; DEFERRED - was StaticRef
    static float PLANE_STD_PROP_SPEED; // game address: 0x8D33D0 ; DEFERRED - was StaticRef
    static float PLANE_MAX_PROP_SPEED; // game address: 0x8D33D4 ; DEFERRED - was StaticRef
    static float PLANE_ROC_PROP_SPEED; // game address: 0x8D33D8 ; DEFERRED - was StaticRef

public:
    CPlane(int32 modelIndex, eVehicleCreatedBy createdBy);
    ~CPlane() override;

    bool SetUpWheelColModel(CColModel* wheelCol) override;
    bool BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) override;
    void ProcessControl() override;
    void ProcessControlInputs(uint8 playerNum) override;
    void ProcessFlyingCarStuff() override;
    void PreRender() override;
    void Render() override;
    void BlowUpCar(CEntity* damager, bool bHideExplosion) override;
    void Fix() override;
    void OpenDoor(CPed* ped, int32 componentId, eDoors door, float doorOpenRatio, bool playSound) override;
    void SetupDamageAfterLoad() override;
    void VehicleDamage(float damageIntensity, eVehicleCollisionComponent component, CEntity* damager, CVector* vecCollisionCoors, CVector* vecCollisionDirection, eWeaponType weapon) override;

    static void InitPlaneGenerationAndRemoval();

    void IsAlreadyFlying();
    void SetGearUp();
    void SetGearDown();

    static uint32 CountPlanesAndHelis();
    static bool AreWeInNoPlaneZone();
    static bool AreWeInNoBigPlaneZone();
    static void SwitchAmbientPlanes(bool enable);
    static void FindPlaneCreationCoors(CVector* outCoors, CVector* outTargetCoors, float* outPlaneOrientation, float* outFlightHeight, bool isBigPlane);
    static void DoPlaneGenerationAndRemoval();

private:

    // dropped: Constructor() placement wrapper (invalid C++, hook-era leftover)
    CPlane* Destroy() { this->CPlane::~CPlane(); return this; }

};

// TODO(2026-10-09): layout mismatch - needs verification against decomp.
//#if INTPTR_MAX == INT32_MAX
//static_assert(sizeof(CPlane) == 0xA04, "CPlane size mismatch");
//#endif
