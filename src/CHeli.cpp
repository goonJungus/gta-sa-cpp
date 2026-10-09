// CHeli.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CHeli/*.c
// Bodies adapted from gta-reversed (source/game_sa/Entity/Vehicle/Heli.cpp),
// verified against the decomp. Methods gta-reversed left as plugin::Call stubs
// are converted from the decompiled .c files where noted; the rest keep their
// `// TODO: decomp src/CHeli/*.c` markers.

#include "CHeli.h"

#include "CCollision.h"
#include "CDamageManager.h"
#include "CDoor.h"
#include "CModelInfo.h"
#include "CPad.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CShadows.h"
#include "CTimer.h"
#include "CVehicleModelInfo.h"
#include "CWorld.h"
#include "RenderWare.h"
#include "eControllerType.h"
#include "ePedType.h"
#include "eVehicleHandlingFlags.h"
#include "eVehicleHandlingModelFlags.h"
#include "tFlyingHandlingData.h"
#include "tHandlingData.h"

#include <cmath>
#include <cstdint>

// PI is used by the rotor math; the tree does not provide it here.
#ifndef PI
#define PI 3.14159265358979323846f
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
    void CamShake(float strength, CVector from);
    static bool m_bUseMouse3rdPerson;
};
extern CCamera TheCamera;

// CStats (Stats.h not ported)
class CStats {
public:
    static void IncrementStat(int32 statId, float value);
};

// CCarCtrl (CarCtrl.h not ported)
class CCarCtrl {
public:
    static int32 NumAmbulancesOnDuty;
    static int32 NumFireTrucksOnDuty;
};

// CDarkel (Darkel.h not ported)
class CDarkel {
public:
    static void RegisterCarBlownUpByPlayer(const CVehicle& vehicle, int32 unk);
};

// CFireManager (FireManager.h not ported)
class CFireManager {
public:
    void StartFire(CEntity* entity, CEntity* creator, float strength, uint8 a4, uint32 a5, uint8 a6);
};
extern CFireManager gFireManager;

// CExplosion (Explosion.h not ported)
enum eExplosionType : uint8 {
    EXPLOSION_AIRCRAFT = 8,    // decomp: heli blow-up type
    EXPLOSION_RC_VEHICLE = 13, // decomp: RC heli blow-up type
};
class CExplosion {
public:
    static void AddExplosion(CEntity* victim, CEntity* creator, eExplosionType type,
                             CVector pos, uint32 a5, uint8 bMakeSound, float a7, uint8 bNoFx);
};

// CWanted (Wanted.h not ported)
class CWanted {
public:
    static bool UseNewsHeliInAdditionToPolice;
};

// CClock (Clock.h not ported)
class CClock {
public:
    static bool GetIsTimeInRange(uint8 hourA, uint8 hourB);
};

// CWindModifiers (WindModifiers.h not ported)
class CWindModifiers {
public:
    static void RegisterOne(CVector pos, int32 a2, float strength);
};

// Stat IDs (Enums/eStats.h not ported; CStats.h only forward-declares eStats).
// TODO(port): verify values against gta-reversed.
enum : int32 {
    STAT_COST_OF_PROPERTY_DAMAGED = 17, // TODO: verify against gta-reversed Enums/eStats.h
};

// RenderWare render-state functions (not in the ported RenderWare.h).
// RwRenderState enum is not in RenderTypes.h; define the values used here.
// TODO(port): replace with real RenderWare render-state values.
enum RwRenderState {
    rwRENDERSTATEZWRITEENABLE,
    rwRENDERSTATEZTESTENABLE,
    rwRENDERSTATESRCBLEND,
    rwRENDERSTATEDESTBLEND,
    rwRENDERSTATEVERTEXALPHAENABLE,
    rwRENDERSTATETEXTURERASTER,
    rwRENDERSTATEFOGENABLE,
    rwRENDERSTATESHADEMODE,
    rwRENDERSTATEALPHATESTFUNCTION,
    rwRENDERSTATEALPHATESTFUNCTIONREF,
    rwRENDERSTATECULLMODE,
};
enum RwAlphaTestFunction {
    rwALPHATESTFUNCTIONGREATER = 4,
    rwALPHATESTFUNCTIONGREATEREQUAL = 5,
};
#define RWRSTATE(x) (x)
void RwRenderStateSet(RwRenderState state, int32 value);
void RwRenderStateGet(RwRenderState state, void* value);

// Game frame helper (not in RenderWare.h)
RwObject* GetCurrentAtomicObjectCB(RwObject* object, void* data);

// Model IDs: canonical eModelID.h (deduped 2026-10-09; the anonymous enum
// here redefined its enumerators - values verified vs src_prev_export/_types.h).

// Static data members (game addresses recorded from gta-reversed StaticRef)
bool CHeli::bPoliceHelisAllowed{ true }; // game address: 0x8D338C ; DEFERRED - was StaticRef (data value: 1)
uint32 CHeli::TestForNewRandomHelisTimer{}; // game address: 0xC1C960
std::array<CHeli*, 2> CHeli::pHelis{}; // game address: 0xC1C964
uint32 CHeli::NumberOfSearchLights{}; // game address: 0xC1C96C
bool CHeli::bHeliControlsCheat{}; // game address: 0xC1C970
std::array<tHeliLight, 4> CHeli::HeliSearchLights{}; // game address: 0xC1C990

// 0x6C4190
CHeli::CHeli(int32 modelIndex, eVehicleCreatedBy createdBy) : CAutomobile(modelIndex, createdBy, true) {
    m_nVehicleSubType = VEHICLE_TYPE_HELI;

    m_fLeftRightSkid           = 0.0f;
    m_fSteeringUpDown          = 0.0f;
    m_fSteeringLeftRight       = 0.0f;
    m_fAccelerationBreakStatus = 0.0f;

    field_99C = 0;
    m_fRotorZ = 0;
    m_fSecondRotorZ = 0;

    m_fMinAltitude = 10.0f;
    m_fMaxAltitude = 10.0f;

    field_9AC = 10.0f;
    field_9B4 = 0;

    m_nHeliFlags = m_nHeliFlags & 0xFC;
    m_fSearchLightIntensity = 0.0f;
    physicalFlags.bDontCollideWithFlyers = true;

    if (modelIndex == MODEL_HUNTER) {
        m_damageManager.SetDoorStatus(DOOR_LEFT_FRONT, DAMSTATE_OK);
        m_doors[DOOR_LEFT_FRONT].Init((3.0f * PI) / 10.0f, 0.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_BASED);
    }

    m_nNumSwatOccupants = 4;
    for (auto& state : m_aSwatState)
        state = 0;

    m_nSearchLightTimer = CTimer::GetTimeInMS();

    for (auto& x : m_aSearchLightHistoryX)
        x = 0.0f;
    for (auto& y : m_aSearchLightHistoryY)
        y = 0.0f;

    m_nShootTimer = 0;
    m_nPoliceShoutTimer = CTimer::GetTimeInMS();

    vehicleFlags.bNeverUseSmallerRemovalRange = true; // 0x6C42BD
    // TODO(port): CAutoPilot not ported - m_autoPilot is an opaque stand-in.
    //     m_autoPilot.m_ucHeliTargetDist2 = 10;

    m_ppGunflashFx = nullptr;
    m_nFiringMultiplier = 16;

    field_9B8 = 0;
    m_bSearchLightEnabled = false;
    field_A14 = CGeneral::GetRandomNumberInRange(2.f, 8.f);
}

// 0x6C4340
CHeli::~CHeli() {
    if (m_ppGunflashFx) {
        for (auto i = 0; i < CVehicle::GetPlaneNumGuns(); i++) {
            if (auto& fx = m_ppGunflashFx[i]) {
                fx->Kill();
                g_fxMan.DestroyFxSystem(fx);
            }
        }
        delete[] m_ppGunflashFx;
        m_ppGunflashFx = nullptr;
    }

    // TODO(port): CAEVehicleAudioEntity not ported - m_vehicleAudio is an opaque stand-in.
    //     m_vehicleAudio.Terminate();
}

// 0x6C4560
void CHeli::InitHelis() {
    for (auto& heli : pHelis)
        heli = nullptr;
    for (auto& light : HeliSearchLights) {
        light.Init();
    }
    NumberOfSearchLights = 0;
    bPoliceHelisAllowed = true;
}

// 0x6C45B0
void CHeli::AddHeliSearchLight(const CVector& origin, const CVector& target, float targetRadius, float power, uint32 coronaIndex, uint8 unknownFlag, uint8 drawShadow) {
    auto& light = HeliSearchLights[NumberOfSearchLights];

    light.m_vecOrigin     = origin;
    light.m_vecTarget     = target;
    light.m_fTargetRadius = targetRadius;
    light.m_fPower        = power;
    light.m_nCoronaIndex  = coronaIndex;
    light.field_24        = unknownFlag;
    light.m_bDrawShadow   = drawShadow;

    NumberOfSearchLights += 1;
}

// 0x6C4640
void CHeli::PreRenderAlways() {
    // NOP
}

// 0x6C4650
void CHeli::Pre_SearchLightCone() {
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,        RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,            RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,            RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATEREQUAL));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(0));
}

// 0x6C46E0
void CHeli::Post_SearchLightCone() {
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,         RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,          RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,             RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,            RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,    RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATECULLMODE,             RWRSTATE(rwCULLMODECULLBACK));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION,    RWRSTATE(rwALPHATESTFUNCTIONGREATER));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(2u));
}

// 0x6C4750
void CHeli::SpecialHeliPreRender() {
    // NOP
}

// 0x6C4760
CVector CHeli::FindSwatPositionRelativeToHeli(int32 swatNumber) {
    switch (swatNumber) {
    case 0:
        return { -1.2f, -1.0f, -0.5f };
    case 1:
        return { 1.2f,  -1.0f, -0.5f };
    case 2:
        return { -1.2f, 1.0f,  -0.5f };
    case 3:
        return { 1.2f,  1.0f,  -0.5f };
    default:
        return { 0.0f,  0.0f,  0.0f  };
    }
}

// 0x6C4800
void CHeli::SwitchPoliceHelis(bool enable) {
    bPoliceHelisAllowed = enable;
}

// 0x6C58E0
void CHeli::SearchLightCone(int32 coronaIndex,
                            CVector origin,
                            CVector target,
                            float targetRadius,
                            float power,
                            uint8 unknownFlag,
                            uint8 drawShadow,
                            CVector& useless0,
                            CVector& useless1,
                            CVector& useless2,
                            bool a11,
                            float baseRadius,
                            float a13,
                            float a14,
                            float a15
) {
#if 0
    // TODO: decomp src/CHeli/SearchLightCone_006c58e0.c
    // Full RenderWare immediate-mode searchlight cone (coronas, realtime
    // shadows, RwIm3D). Blocked on the RenderWare render-state/immediate layer
    // (RwIm3DTransform, CCoronas, CRealTimeShadow) - none ported yet.
#endif
}

// 0x6C6520
CHeli* CHeli::GenerateHeli(CPed* target, bool newsHeli) {
    // TODO: decomp src/CHeli/GenerateHeli_006c6520.c
    (void)target;
    (void)newsHeli;
    return nullptr;
}

// 0x6C6890
void CHeli::TestSniperCollision(CVector* origin, CVector* target) {
    CVector point = *target - *origin;

    if (point.z >= point.Magnitude() / 2.0f)
        return;

    for (auto& heli : pHelis) {
        if (!heli || heli->physicalFlags.bBulletProof)
            continue;

        const auto mat = (CMatrix*)heli->m_matrix;
        if (CCollision::DistToLine(*origin, *target, mat->TransformPoint({ -0.43f, 1.49f, 1.5f })) < 0.8f) {
            heli->m_fRotationBalance = (float)(CGeneral::GetRandomNumber() < 16383) * 0.1f - 0.05f; // [-0.05, 0.05]
            heli->BlowUpCar(FindPlayerPed(), false);
            heli->m_nNumSwatOccupants = 0;
        };
    }
}

// 0x6C69C0
bool CHeli::SendDownSwat() {
#if 0
    // TODO: decomp src/CHeli/SendDownSwat_006c69c0.c
    // SWAT rope-descent task sequence. Blocked on the tasks subsystem
    // (CTaskComplexSequence, CTaskComplexUseSwatRope, CTaskComplexWanderCop),
    // CStreaming model load states, CWaterLevel, CRopes - none ported yet.
#endif
    return false;
}

// 0x6C79A0
void CHeli::UpdateHelis() {
    // TODO: decomp src/CHeli/UpdateHelis_006c79a0.c
}

// 0x6C7C50
void CHeli::RenderAllHeliSearchLights() {
    for (auto& light : HeliSearchLights) {
        SearchLightCone(
            light.m_nCoronaIndex,
            light.m_vecOrigin,
            light.m_vecTarget,
            light.m_fTargetRadius,
            light.m_fPower,
            light.field_24,
            light.m_bDrawShadow,
            light.m_vecUseless[0],
            light.m_vecUseless[1],
            light.m_vecUseless[2],
            false,
            0.05f,
            0.0f,
            0.0f,
            1.0f
        );
    }
}

// 0x6C6D30
void CHeli::BlowUpCar(CEntity* damager, bool bHideExplosion) {
    if (!vehicleFlags.bCanBeDamaged)
        return;

    const bool isRcHeli = m_nModelIndex == MODEL_RCRAIDER || m_nModelIndex == MODEL_RCGOBLIN;

    // TODO(port): CAutoPilot not ported. Decomp src/CHeli/BlowUpCar_006c6d30.c:
    // if the heli was shot down (entity-type high bits set, autopilot mission
    // not already MISSION_HELI_CRASH_AND_BURN, not an RC heli), divert it into
    // MISSION_HELI_CRASH_AND_BURN, zero its health and return instead of
    // exploding immediately.

    CEntity* creator = damager;
    if (damager == FindPlayerPed() || damager == FindPlayerVehicle()) {
        // TODO(port): PlayerInfo.h not ported (m_nHavocCaused/m_fCurrentChaseValue).
        CStats::IncrementStat(STAT_COST_OF_PROPERTY_DAMAGED, (float)(CGeneral::GetRandomNumber() % 6000 + 4000));
    }

    if (m_nModelIndex == MODEL_VCNMAV) {
        CWanted::UseNewsHeliInAdditionToPolice = false;
    }

    m_nFlags &= 0xFFFFFF7E;
    m_vecMoveSpeed = CVector{};
    m_vecTurnSpeed = CVector{};

    SetStatus(STATUS_WRECKED);
    physicalFlags.bRenderScorched = true;
    m_nTimeWhenBlowedUp = CTimer::GetTimeInMS();
    CVisibilityPlugins::SetClumpForAllAtomicsFlag(GetRpClump(), 0x4000);
    m_damageManager.FuckCarCompletely(false);

    if (!isRcHeli) {
        SetBumperDamage(FRONT_BUMPER, false);
        SetBumperDamage(REAR_BUMPER, false);
        SetDoorDamage(DOOR_BONNET, false);
        SetDoorDamage(DOOR_BOOT, false);
        SetDoorDamage(DOOR_LEFT_FRONT, false);
        SetDoorDamage(DOOR_RIGHT_FRONT, false);
        SetDoorDamage(DOOR_LEFT_REAR, false);
        SetDoorDamage(DOOR_RIGHT_REAR, false);
        SpawnFlyingComponent(CAR_WHEEL_LF, 1);
        // Decomp hides the wheel atomic here via an unresolved RwFrameForAllObjects
        // callback (0x50) plus a vtable byte poke - intent is RpAtomicSetFlags(obj, 0).
        if (auto wheelNode = m_aCarNodes[HELI_WHEEL_LF]) {
            RpAtomic* wheelAtomic = nullptr;
            RwFrameForAllObjects(wheelNode, GetCurrentAtomicObjectCB, &wheelAtomic);
            if (wheelAtomic) {
                RpAtomicSetFlags(wheelAtomic, (RpAtomicFlag)0);
            }
        }
    }

    m_nBombOnBoard = 0;
    m_fHealth = 0.0f;
    m_wBombTimer = 0;

    TheCamera.CamShake(0.4f, GetPosition());
    KillPedsInVehicle();

    vehicleFlags.bEngineOn = false;
    vehicleFlags.bLightsOn = false;
    autoFlags.bTaxiLight = false;

    if (vehicleFlags.bIsAmbulanceOnDuty) {
        vehicleFlags.bIsAmbulanceOnDuty = false;
        CCarCtrl::NumAmbulancesOnDuty--;
    }
    if (vehicleFlags.bIsFireTruckOnDuty) {
        vehicleFlags.bIsFireTruckOnDuty = false;
        CCarCtrl::NumFireTrucksOnDuty--;
    }

    ChangeLawEnforcerState(false);
    gFireManager.StartFire(this, creator, 0.8f, 1, 7000, 0);
    CDarkel::RegisterCarBlownUpByPlayer(*this, 0);

    // NB: decomp passes 0 (not bHideExplosion) for the hide-explosion-fx flag.
    const auto explosionType = isRcHeli ? EXPLOSION_RC_VEHICLE : EXPLOSION_AIRCRAFT;
    CExplosion::AddExplosion(this, creator, explosionType, GetPosition(), 0, 1, -1.0f, 0);
    (void)bHideExplosion;
}

// 0x6C4530
void CHeli::Fix() {
    m_damageManager.ResetDamageStatus();
    SetupDamageAfterLoad();
}

// 0x6C4330
bool CHeli::BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) {
    return false;
}

// 0x6C4320
bool CHeli::SetUpWheelColModel(CColModel* wheelCol) {
    return false;
}

// 0x6C4550
void CHeli::SetupDamageAfterLoad() {
    vehicleFlags.bIsDamaged = false;
}

// 0x6C4400
void CHeli::Render() {
    auto* mi = GetVehicleModelInfo();
    m_nTimeTillWeNeedThisCar = CTimer::GetTimeInMS() + 3000;
    mi->SetVehicleColour(m_nPrimaryColor, m_nSecondaryColor, m_nTertiaryColor, m_nQuaternaryColor);

    auto staticRotor = m_aCarNodes[HELI_STATIC_ROTOR];
    RpAtomic* data = nullptr;
    if (staticRotor) {
        RwFrameForAllObjects(staticRotor, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 255);
    }

    auto staticRotor2 = m_aCarNodes[HELI_STATIC_ROTOR2];
    data = nullptr;
    if (staticRotor2) {
        RwFrameForAllObjects(staticRotor2, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 255);
    }

    auto movingRotor = m_aCarNodes[HELI_MOVING_ROTOR];
    data = nullptr;
    if (movingRotor) {
        RwFrameForAllObjects(movingRotor, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 0);
    }

    auto movingRotor2 = m_aCarNodes[HELI_MOVING_ROTOR2];
    data = nullptr;
    if (movingRotor2) {
        RwFrameForAllObjects(movingRotor2, GetCurrentAtomicObjectCB, &data);
        if (data)
            CVehicle::SetComponentAtomicAlpha(data, 0);
    }

    CEntity::Render(); // exactly CEntity
}

// 0x6C4830
void CHeli::ProcessControlInputs(uint8 playerNum) {
    CPad* pad = CPad::GetPad(playerNum);
    const int16 accel = pad->GetAccelerate();
    const int16 brake = pad->GetBrake();
    m_fAccelerationBreakStatus = (float)(accel - brake) * 0.003921569f;

    if (!CCamera::m_bUseMouse3rdPerson || !CVehicle::m_bEnableMouseFlying) {
        CVehicle::m_nLastControlInput = eControllerType::KEYBOARD;
        m_fSteeringUpDown = (float)pad->GetSteeringUpDown() * 0.0078125f;
        m_fSteeringLeftRight = -(float)pad->GetSteeringLeftRight() * 0.0078125f;
    } else {
        if (CPad::NewMouseControllerState.m_AmountMoved.x != 0.0f
            || CPad::NewMouseControllerState.m_AmountMoved.y != 0.0f) {
        useMouse:
            CVehicle::m_nLastControlInput = eControllerType::MOUSE;
            if (pad->NewState.m_bVehicleMouseLook == 0) {
                m_fSteeringLeftRight -= CPad::NewMouseControllerState.m_AmountMoved.x * 0.0025f;
                m_fSteeringUpDown += CPad::NewMouseControllerState.m_AmountMoved.y * 0.0025f;
            }
            if (std::abs(m_fSteeringLeftRight) < 0.5f) {
                // TODO: FxInterpInfo_c::unk_00822130() - unidentified damping factor from decomp.
                // m_fSteeringLeftRight *= <damping>;
            }
            if (std::abs(m_fSteeringUpDown) < 0.5f) {
                // TODO: FxInterpInfo_c::unk_00822130() - unidentified damping factor from decomp.
                // m_fSteeringUpDown *= <damping>;
            }
            goto clampSteering;
        }
        if ((std::abs(m_fSteeringLeftRight) > 0.0f || std::abs(m_fSteeringUpDown) > 0.0f)
            && CVehicle::m_nLastControlInput == eControllerType::MOUSE) {
            if (pad->GetSteeringLeftRight() == 0 && pad->GetSteeringUpDown() == 0)
                goto useMouse;
        }
        if (pad->GetSteeringLeftRight() == 0 && pad->GetSteeringUpDown() == 0
            && CVehicle::m_nLastControlInput == eControllerType::MOUSE) {
            goto clampSteering;
        }
        CVehicle::m_nLastControlInput = eControllerType::KEYBOARD;
        m_fSteeringUpDown = (float)pad->GetSteeringUpDown() * 0.0078125f;
        m_fSteeringLeftRight = -(float)pad->GetSteeringLeftRight() * 0.0078125f;
    }

clampSteering:
    m_fSteeringUpDown = std::clamp(m_fSteeringUpDown, -1.0f, 1.0f);
    m_fSteeringLeftRight = std::clamp(m_fSteeringLeftRight, -1.0f, 1.0f);

    m_fLeftRightSkid = pad->GetLookRight() ? 1.0f : 0.0f;
    if (pad->GetLookLeft())
        m_fLeftRightSkid = -1.0f;

    if (pad->GetHorn() && m_matrix->GetUp().z > 0.0f) {
        // Auto-level the heli while the horn is held
        m_fLeftRightSkid = 0.0f;
        const tFlyingHandlingData* handling = m_pFlyingHandlingData;

        CVector upAxis{ 0.0f, 0.0f, 1.0f };
        CVector levelRight;
        levelRight.Cross_OG(upAxis, m_matrix->GetRight());
        levelRight.Normalise();
        const float pitchInput = levelRight.Dot(m_vecMoveSpeed) * handling->m_fPitchStab;
        // TODO: _unk_00858ca0 - unidentified clamp limit from decomp (upper bound).
        // m_fSteeringUpDown = std::clamp(pitchInput, -2.0f, <upper>);
        m_fSteeringUpDown = std::max(pitchInput, -2.0f);

        CVector levelFwd;
        levelFwd.Cross_OG(m_matrix->GetForward(), upAxis);
        levelFwd.Normalise();
        const float rollInput = levelFwd.Dot(m_vecMoveSpeed) * handling->m_fRollStab;
        // TODO: _unk_00858ca0 - unidentified clamp limit from decomp (upper bound).
        // m_fSteeringLeftRight = std::clamp(rollInput, -2.0f, <upper>);
        m_fSteeringLeftRight = std::max(rollInput, -2.0f);
    }

    m_fSteerAngle = 0.0f;
    m_BrakePedal = 1.0f;
    m_GasPedal = 0.0f;
    vehicleFlags.bIsHandbrakeOn = false; // byte0 & 0xDF

    if (pad->DisablePlayerControls) {
        if (auto* playerPed = FindPlayerPed()) {
            playerPed->KeepAreaAroundPlayerClear();
        }
        const float speed = m_vecMoveSpeed.Magnitude();
        if (speed > 0.28f) {
            m_vecMoveSpeed *= 0.28f / speed;
        }
    }

    if (m_fHealth < 250.0f) {
        // Dying heli: throttle stuck, slight skid
        m_fAccelerationBreakStatus = -0.1f;
        m_fLeftRightSkid += 0.5f;
    }
}

// 0x6C4E60
void CHeli::ProcessFlyingCarStuff() {
    const auto status = GetStatus();
    const bool isRcHeli = m_nModelIndex == MODEL_RCRAIDER || m_nModelIndex == MODEL_RCGOBLIN;

    if (status != STATUS_PLAYER && status != STATUS_REMOTE_CONTROLLED && status != STATUS_PHYSICS) {
        if ((m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_IS_HELI) == VEHICLE_HANDLING_MODEL_NONE) {
            return;
        }
        vehicleFlags.bEngineOn = false;
        const float spinDown = CTimer::GetTimeStep() * 0.00055f;
        if (m_fHeliRotorSpeed <= spinDown) {
            m_fHeliRotorSpeed = 0.0f;
        } else {
            m_nFakePhysics = 0;
            m_fHeliRotorSpeed -= spinDown;
        }
    } else {
        // Spin the rotor up
        if (m_fHeliRotorSpeed < 0.22f && !physicalFlags.bSubmergedInWater) {
            m_fHeliRotorSpeed += isRcHeli ? 0.003f : 0.001f;
        }

        if (m_fHeliRotorSpeed > 0.15f) {
            bool isSpecialHeli = false;
            if (physicalFlags.bTouchingWater
                && (m_nModelIndex == MODEL_SEASPAR || m_nModelIndex == MODEL_LEVIATHN)) {
                isSpecialHeli = true;
            }
            if (vehicleFlags.bSirenOrAlarm) {
                FlyingControl(FLIGHT_MODEL_RC, m_fLeftRightSkid, m_fSteeringUpDown, m_fSteeringLeftRight, m_fAccelerationBreakStatus);
            } else {
                bool canFly = true;
                if ((m_nNumContactWheels > 3 || isSpecialHeli)
                    && m_fAccelerationBreakStatus <= 0.0f
                    && std::abs(m_vecMoveSpeed.x) <= 0.02f
                    && std::abs(m_vecMoveSpeed.y) <= 0.02f
                    && std::abs(m_vecMoveSpeed.z) <= 0.02f) {
                    canFly = false;
                }
                if (canFly) {
                    FlyingControl(FLIGHT_MODEL_HELI, m_fLeftRightSkid, m_fSteeringUpDown, m_fSteeringLeftRight, m_fAccelerationBreakStatus);
                }
            }
        }

#if 0
        // TODO: decomp src/CHeli/ProcessFlyingCarStuff_006c4e60.c
        // Rotor blade collision (DoBladeCollision): needs the rotor atomic's
        // bounding-sphere radius from RpAtomic internals (not in the ported
        // RenderWare.h - RpAtomic is only forward-declared).
#endif

        // Register wind from the rotor wash
        if ((status == STATUS_PLAYER || status == STATUS_PHYSICS) && m_fHeliRotorSpeed > 0.0075f) {
            const float strength = std::min(1.0f, m_fHeliRotorSpeed * 6.6666665f);
            CWindModifiers::RegisterOne(GetPosition(), 1, strength);
        }
    }

#if 0
    // TODO: decomp src/CHeli/ProcessFlyingCarStuff_006c4e60.c
    // Heli blade audio: plays AE_HELI_BLADE when the camera is within 20 units
    // and the rotor angle changed enough. Blocked on CAEVehicleAudioEntity
    // (m_vehicleAudio is an opaque stand-in).
#endif
}

// 0x6C5420
void CHeli::PreRender() {
    CVehicle::PreRender();

    auto* mi = GetVehicleModelInfo();

    if (m_bSearchLightEnabled && m_fSearchLightIntensity > 0.0f && CClock::GetIsTimeInRange(0x13, 0x06)) {
        const CVector target = m_vecSearchLightTarget;
        // Searchlight origin: heli-space (0, 3.5, -0.3) transformed to world.
        const CVector origin = m_matrix->GetPosition()
            + m_matrix->GetForward() * 3.5f
            - m_matrix->GetUp() * 0.3f;
        // NB: decomp passes garbage for coronaIndex (pointer arithmetic on
        // m_placement); the real value is unknown - passing 0.
        // TODO: identify the coronaIndex source.
        AddHeliSearchLight(origin, target, 20.0f, m_fSearchLightIntensity, 0, 1, 1);
    }

    if (vehicleFlags.bVehicleColProcessed) {
        // TODO: decomp src/CHeli/PreRender_006c5420.c - unidentified no-arg
        // virtual call (vtable slot 0xD0) before the wheel visual update.
        // Not converted; the target could not be identified with confidence.
        for (int32 wheel = 0; wheel < 4; wheel++) {
            float ratio = 1.0f - m_aSuspensionSpringLength[wheel] / m_aSuspensionLineLength[wheel];
            ratio = (m_fWheelsSuspensionCompression[wheel] - ratio) / (1.0f - ratio);
            CVector wheelPos;
            mi->GetWheelPosn(wheel, wheelPos, true);
            float height = wheelPos.z + m_pHandlingData->m_fSuspensionUpperLimit;
            if (ratio > 0.0f)
                height -= ratio * m_aSuspensionSpringLength[wheel];
            if (height <= m_wheelPosition[wheel]
                && (physicalFlags.bDisableCollisionForce
                    || (m_nHandlingFlagsIntValue & VEHICLE_HANDLING_HYDRAULIC_INST) == 0)) {
                height = (height - m_wheelPosition[wheel]) * 0.75f + m_wheelPosition[wheel];
            }
            m_wheelPosition[wheel] = height;
        }
    }

    CAutomobile::UpdateWheelMatrix(4, 1);
    CAutomobile::UpdateWheelMatrix(7, 1);
    CAutomobile::UpdateWheelMatrix(2, 1);
    CAutomobile::UpdateWheelMatrix(5, 1);

    const bool isRcHeli = m_nModelIndex == MODEL_RCRAIDER || m_nModelIndex == MODEL_RCGOBLIN;
    if (!isRcHeli) {
        CAutomobile::DoHeliDustEffect(1.0f, 1.0f);
    }

    // Spin the rotor visuals
    float rotorSpin = CTimer::GetTimeStep() * m_fHeliRotorSpeed;
    // TODO: _DAT_008d33a0 - unidentified rotor-speed multiplier from decomp,
    // applied for SPARROW/SEASPAR/MAVERICK/VCNMAV/POLMAV. Value unknown.
    m_fRotorZ = (int32)((float)m_fRotorZ - rotorSpin);
    while ((float)m_fRotorZ < -2.0f * PI)
        m_fRotorZ = (int32)((float)m_fRotorZ + 2.0f * PI);

    float secondRotorSpin = CTimer::GetTimeStep() * m_fHeliRotorSpeed;
    secondRotorSpin *= (m_nModelIndex == MODEL_LEVIATHN) ? 2.0f : 2.3f;
    m_fSecondRotorZ = (int32)((float)m_fSecondRotorZ - secondRotorSpin);
    while ((float)m_fSecondRotorZ > 2.0f * PI)
        m_fSecondRotorZ = (int32)((float)m_fSecondRotorZ - 2.0f * PI);

    // Apply rotor rotation to the rotor nodes
    auto applyRotorRotation = [](RwFrame* node, float angle, bool rotateX) {
        if (!node)
            return;
        CMatrix mat;
        mat.Attach(RwFrameGetMatrix(node), false);
        const CVector pos = mat.GetPosition();
        if (rotateX)
            mat.SetRotateX(angle);
        else
            mat.SetRotateZ(angle);
        mat.GetPosition() = pos;
        mat.UpdateRW();
        mat.Detach();
    };
    applyRotorRotation(m_aCarNodes[HELI_STATIC_ROTOR], (float)m_fRotorZ, false);
    applyRotorRotation(m_aCarNodes[HELI_STATIC_ROTOR2], (float)m_fRotorZ, false);
    applyRotorRotation(m_aCarNodes[HELI_MOVING_ROTOR], (float)m_fSecondRotorZ, true);
    applyRotorRotation(m_aCarNodes[HELI_MOVING_ROTOR2], (float)m_fSecondRotorZ, true);

    CShadows::StoreShadowForVehicle(this, VEH_SHD_HELI);
}

// 0x6C7050
void CHeli::ProcessControl() {
    CAutomobile::ProcessControl();

    if (!vehicleFlags.bEngineOn && m_pDustParticle) {
        m_pDustParticle->Kill();
        m_pDustParticle = nullptr;
        m_heliDustFxTimeConst = 0.0f;
    }

    CPad* pad = CPad::GetPad(0);
    if (m_pDriver && m_pDriver->m_nPedType == PED_TYPE_PLAYER2) {
        pad = CPad::GetPad(1);
    }
    if (pad->HornJustDown()) {
        m_bSearchLightEnabled = !m_bSearchLightEnabled;
    }

#if 0
    // TODO: decomp src/CHeli/ProcessControl_006c7050.c
    // Police searchlight target tracking (m_vecSearchLightTarget history
    // interpolation, 40-60 unit intensity falloff), minigun firing at the
    // player, SWAT rope descent. Blocked on: CAutoPilot (m_nCarMission,
    // m_TargetEntity), CWanted/CPlayerPedData (m_pWanted->m_WantedLevel),
    // CAudioEngine speech events, CCullZones, CRopes, CInterestingEvents,
    // CWorld::GetIsLineOfSightClear - none ported yet.
#endif

    // SWAT rope-descent state countdown (rope registration needs CRopes).
    for (int32 i = 0; i < 4; i++) {
        if (m_aSwatState[i]) {
            m_aSwatState[i]--;
        }
    }

    CVehicle::UpdateWinch();
    CVehicle::ProcessWeapons();
}
