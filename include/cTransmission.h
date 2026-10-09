// cTransmission.h - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/cTransmission.h (plugin-sdk file)
// Vehicle transmission model (gear ratios, drive type, engine params).
//
// Adaptations:
//   stripped plugin-sdk header comment + InjectHooks()
//   VALIDATE_SIZE -> 32-bit-guarded static_assert
//   <array> include added (was via Base.h)
// TODO:
//   port method bodies (InitGearRatios, CalculateDriveAcceleration, ...) from
//   decomp src/cTransmission/*.c when CAutomobile::ProcessControl is ported
//
#pragma once

#include <array>

#include "GrTypes.h"
#include "tTransmissionGear.h"

constexpr float TRANSMISSION_FREE_ACCELERATION   = 0.1f;
constexpr float TRANSMISSION_SMOOTHER_FRAC       = 0.85f;
constexpr float TRANSMISSION_AI_CHEAT_MULT       = 1.2f;
constexpr float TRANSMISSION_NITROS_MULT         = 2.0f;
constexpr float TRANSMISSION_AI_CHEAT_INERTIA_MULT = 0.75f;
constexpr float TRANSMISSION_NITROS_INERTIA_MULT   = 0.5f;

enum {
    CHEAT_HANDLING_NONE    = 0,
    CHEAT_HANDLING_PERFECT = 1,
    CHEAT_HANDLING_NITROS  = 2
};

class cTransmission {
public:
    std::array<tTransmissionGear, 6> m_aGears;      // 0 = reverse
    uint8  m_nDriveType;     // F/R/4
    uint8  m_nEngineType;    // P/D/E
    uint8  m_nNumberOfGears; // 1 to 6
    uint32 m_handlingFlags;
    float  m_EngineAcceleration; // 0.1 to 10.0
    float  m_EngineInertia;      // 0.0 to 50.0
    float  m_MaxVelocity;    // 5.0 to 150.0
    float  m_MaxFlatVelocity;
    float  m_MaxReverseVelocity;
    float  m_Velocity;

public:
    cTransmission() = default; // 0x6D0450

    void  InitGearRatios();
    void  DisplayGearRatios();
    void  CalculateGearForSimpleCar(float speed, uint8& currentGear);
    float CalculateDriveAcceleration(const float& gasPedal, uint8& currentGear, float& gearChangeCount, float& velocity, float* a6, float* a7, uint8 allWheelsOnGround, uint8 handlingType);
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(cTransmission) == 0x68, "cTransmission size mismatch");
#endif
