// CWanted - PARTIAL port for the clean-room C++ build
// Source: gta-reversed/source/game_sa/Wanted.h
//
// This is a PARTIAL port: scalar state + the methods used by converted TUs.
// NOT yet ported (need their types first):
//   m_CrimesBeingQd[16]      (needs CCrimeBeingQd)
//   m_CopsInPursuit[10]      (needs CCopPed)
//   m_PoliceScannerAudioEntity (needs CAEPoliceScannerAudioEntity)
// eWantedLevel is forward-declared here (defined inline in CPlayerPed.h);
// eCrimeType not yet ported.

#pragma once

#include "Common.h" // StaticRef

#include <cstdint>

class CPed;
class CCopPed;

enum class eWantedLevel : uint32_t; // defined inline in CPlayerPed.h

class CWanted {
public:
    // gta-reversed Wanted.h: static inline auto& UseNewsHeliInAdditionToPolice = StaticRef<bool>(0xB7CB8C);
    // Added 2026-10-09 for CAutomobile.
    static inline auto& UseNewsHeliInAdditionToPolice = StaticRef<bool>(0xB7CB8C);

    static constexpr uint32_t MAX_COPS_IN_PURSUIT = 10;

    uint32_t m_ChaosLevel;
    uint32_t m_ChaosLevelBeforeParole;
    uint32_t m_nLastTimeWantedDecreased;
    uint32_t m_nLastTimeWantedLevelChanged;
    uint32_t m_nTimeOfParole;
    float    m_fMultiplier; // New crimes have their wanted level contribution multiplied by this
    uint8_t  m_nCopsInPursuit;
    uint8_t  m_nMaxCopsInPursuit;
    uint8_t  m_nMaxCopCarsInPursuit;
    uint8_t  m_nCopsBeatingSuspect;
    uint16_t m_nChanceOnRoadBlock;

    union {
        struct {
            uint8_t m_bPoliceBackOff : 1;       // If this is set the police will leave player alone (for cut-scenes)
            uint8_t m_bPoliceBackOffGarage : 1; // If this is set the police will leave player alone (for garages)
            uint8_t m_bEverybodyBackOff : 1;    // If this is set then everybody (including police) will leave the player alone (for cut-scenes)
            uint8_t m_bSwatRequired : 1;        // These three booleans are needed so that the
            uint8_t m_bFbiRequired : 1;         // streaming required vehicle stuff can be overrided
            uint8_t m_bArmyRequired : 1;
        };
        uint8_t m_nFlags;
    };
    uint32_t m_nCurrentChaseTime;
    uint32_t m_nCurrentChaseTimeCounter;
    bool     m_bTimeCounting;

    eWantedLevel m_WantedLevel;
    eWantedLevel m_WantedLevelBeforeParole;

public:
    void Update();
    void SetWantedLevel(eWantedLevel newLev);
    void CheatWantedLevel(eWantedLevel newLev);
    void SetWantedLevelNoDrop(eWantedLevel newLev);
    void ClearWantedLevelAndGoOnParole();

    eWantedLevel GetWantedLevel() const { return m_WantedLevel; }
    bool PoliceBackOff() const { return m_bPoliceBackOff || m_bPoliceBackOffGarage || m_bEverybodyBackOff; }
};
