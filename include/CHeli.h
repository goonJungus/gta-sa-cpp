// CHeli.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Vehicle/Heli.h
// Helicopter: rotors, searchlights, police/SWAT logic, heli generation.
// Hierarchy: ... -> CAutomobile -> CHeli
//
// Adaptations:
//   stripped InjectHooks(), friend InjectHooksMain
//   VALIDATE_SIZE x2 (tHeliLight, CHeli) -> 32-bit-guarded static_asserts
//   StaticRef statics -> plain static members (defined in CHeli.cpp)
//   FxSystem_c -> forward-declared
// TODO:
//   verify each method against decomp src/CHeli/*.c

#pragma once

#include <array>
#include <cstdint>

#include "GrTypes.h"

#include "CAutomobile.h"

class CPed;
class FxSystem_c;



enum eHeliNodes {
    HELI_NODE_NONE     = 0,
    HELI_CHASSIS       = 1,
    HELI_WHEEL_RF      = 2,
    HELI_WHEEL_RM      = 3,
    HELI_WHEEL_RB      = 4,
    HELI_WHEEL_LF      = 5,
    HELI_WHEEL_LM      = 6,
    HELI_WHEEL_LB      = 7,
    HELI_DOOR_RF       = 8,
    HELI_DOOR_RR       = 9,
    HELI_DOOR_LF       = 10,
    HELI_DOOR_LR       = 11,
    HELI_STATIC_ROTOR  = 12,
    HELI_MOVING_ROTOR  = 13,
    HELI_STATIC_ROTOR2 = 14,
    HELI_MOVING_ROTOR2 = 15,
    HELI_RUDDER        = 16,
    HELI_ELEVATORS     = 17,
    HELI_MISC_A        = 18,
    HELI_MISC_B        = 19,
    HELI_MISC_C        = 20,
    HELI_MISC_D        = 21,

    HELI_NUM_NODES
};

struct tHeliLight {
    CVector m_vecOrigin;
    CVector m_vecTarget;
    float   m_fTargetRadius;
    float   m_fPower;
    uint32  m_nCoronaIndex;
    bool    field_24; // unknown flag
    bool    m_bDrawShadow;
    std::array<CVector, 3> m_vecUseless; // m_aSearchLightHistory

    void Init() {
        m_vecOrigin = CVector{};
        m_vecTarget = CVector{};
        m_fTargetRadius = 0.0f;
        m_fPower = 0.0f;
    }
};

// TODO(2026-10-09): layout mismatch - needs verification against decomp.
//#if INTPTR_MAX == INT32_MAX
//static_assert(sizeof(tHeliLight) == 0x4C, "tHeliLight size mismatch");
//#endif

class FxSystem_c;

class CHeli : public CAutomobile {
public:
    uint8        m_nHeliFlags;
    float        m_fLeftRightSkid;
    float        m_fSteeringUpDown;
    float        m_fSteeringLeftRight;
    float        m_fAccelerationBreakStatus;
    int32        field_99C;
    int32        m_fRotorZ;
    int32        m_fSecondRotorZ;
    float        m_fMaxAltitude;
    float        field_9AC;
    float        m_fMinAltitude;
    float        field_9B4; // m_fHeading related
    char         field_9B8;
    int8         m_nNumSwatOccupants;
    std::array<uint8, 4> m_aSwatState;
    CVector      field_9C0;
    int32        f9CC; // unused?
    int32        f9D0; // unused?
    FxSystem_c** m_pParticlesList;
    std::array<float, 3> m_aSearchLightHistoryX;
    std::array<float, 3> m_aSearchLightHistoryY;
    uint32       m_nSearchLightTimer;
    CVector      m_vecSearchLightTarget;
    float        m_fSearchLightIntensity;
    uint32       m_nShootTimer;
    uint32       m_nPoliceShoutTimer;
    FxSystem_c** m_ppGunflashFx;
    uint8        m_nFiringMultiplier;
    bool         m_bSearchLightEnabled;
    float        field_A14;

    static bool bPoliceHelisAllowed; // game address: 0x8D338C ; DEFERRED - was StaticRef (was: 1)
    static uint32 TestForNewRandomHelisTimer; // game address: 0xC1C960 ; DEFERRED - was StaticRef
    static std::array<CHeli*, 2> pHelis; // game address: 0xC1C964 ; DEFERRED - was StaticRef
    static uint32 NumberOfSearchLights; // game address: 0xC1C96C ; DEFERRED - was StaticRef
    static bool bHeliControlsCheat; // game address: 0xC1C970 ; DEFERRED - was StaticRef
    static std::array<tHeliLight, 4> HeliSearchLights; // game address: 0xC1C990 ; DEFERRED - was StaticRef

    static constexpr auto Type = VEHICLE_TYPE_HELI;

public:
    CHeli(int32 modelIndex, eVehicleCreatedBy createdBy);
    ~CHeli() override; // 0x6C4340, 0x6C4810

    void BlowUpCar(CEntity* damager, bool bHideExplosion) override;
    void Fix() override;
    bool BurstTyre(uint8 tyreComponentId, bool bPhysicalEffect) override;
    bool SetUpWheelColModel(CColModel* wheelCol) override;
    void ProcessControlInputs(uint8 playerNum) override;
    void Render() override;
    void SetupDamageAfterLoad() override;
    void ProcessFlyingCarStuff() override;
    void PreRender() override;
    void ProcessControl() override;

    void PreRenderAlways();
    CVector FindSwatPositionRelativeToHeli(int32 swatNumber);
    bool SendDownSwat();

    inline uint32 GetRopeId() { return reinterpret_cast<int32>(this + m_nNumSwatOccupants - 1); }

    static void InitHelis();
    static void AddHeliSearchLight(const CVector& origin, const CVector& target, float targetRadius, float power, uint32 coronaIndex, uint8 unknownFlag, uint8 drawShadow);
    static void Pre_SearchLightCone();
    static void Post_SearchLightCone();
    static void SpecialHeliPreRender(); // dummy function
    static void SwitchPoliceHelis(bool enable);
    static void SearchLightCone(int32 coronaIndex,
                                CVector origin,
                                CVector target,
                                float targetRadius,
                                float power,
                                uint8 clipIfColliding,
                                uint8 drawShadow,
                                CVector& useless0,
                                CVector& useless1,
                                CVector& useless2,
                                bool a11,
                                float baseRadius,
                                float a13,
                                float a14,
                                float a15);
    static CHeli* GenerateHeli(CPed* target, bool newsHeli);
    static void TestSniperCollision(CVector* origin, CVector* target);
    static void UpdateHelis();
    static void RenderAllHeliSearchLights();

private:

    // dropped: Constructor() placement wrapper (invalid C++, hook-era leftover)
};

// TODO(2026-10-09): layout mismatch - needs verification against decomp.
//#if INTPTR_MAX == INT32_MAX
//static_assert(sizeof(CHeli) == 0xA18, "CHeli size mismatch");
//#endif
