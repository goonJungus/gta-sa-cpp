// CStats.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CStats. Adapted from gta-reversed for clean-room C++ build.
//
// All methods below are TODO stubs: verify each against the decomp
// (C:\Users\fufid\Documents\Decomps\gta-sa decomp\src\CStats\*.c)
// and gta-reversed/source/game_sa/Stats.cpp before implementing.
// Known references from gta-reversed for cross-reference: stat addresses are
// the 1.0 US binary addresses the StaticRef members were bound to.
// Priority: Init, IncrementStat/DecrementStat/SetStatValue/GetStatValue,
// UpdateStatsWhen* per-frame updaters, Save/Load.

#include "CStats.h"

// Static member definitions (original 1.0 US addresses in comments).
// TODO: re-resolve for the clean-room build.
std::array<tStatMessage, 128> CStats::StatMessage{};         // 0xB78200
uint32_t CStats::TotalNumStatMessages{};                     // 0xB794D0
char     CStats::LastMissionPassedName[8]{};                 // 0xB78A00
std::array<int32_t, 100> CStats::TimesMissionAttempted{};    // 0xB78CC8
std::array<int32_t, 14>  CStats::FavoriteRadioStationList{}; // 0xB78E58
std::array<int32_t, 32>  CStats::PedsKilledOfThisType{};     // 0xB78E90
std::array<float, 59>    CStats::StatReactionValue{};        // 0xB78F10
std::array<int32_t, 223> CStats::StatTypesInt{};             // 0xB79000
std::array<float, 82>    CStats::StatTypesFloat{};           // 0xB79380
int16_t  CStats::m_ThisStatIsABarChart{};     // addr: see gta-reversed Stats.cpp (TODO)
bool     CStats::bStatUpdateMessageDisplayed{}; // 0xB794D4
uint32_t CStats::m_SprintStaminaCounter{};      // 0xB794D8
uint32_t CStats::m_CycleStaminaCounter{};       // 0xB794DC
uint32_t CStats::m_CycleSkillCounter{};         // 0xB794E0
uint32_t CStats::m_SwimStaminaCounter{};       // 0xB794E4
uint32_t CStats::m_SwimUnderWaterCounter{};    // 0xB794E8
uint32_t CStats::m_DrivingCounter{};           // 0xB794EC
uint32_t CStats::m_FlyingCounter{};            // 0xB794F0
uint32_t CStats::m_BoatCounter{};              // 0xB794F4
uint32_t CStats::m_BikeCounter{};              // 0xB794F8
uint32_t CStats::m_FatCounter{};               // 0xB794FC
uint32_t CStats::m_RunningCounter{};           // 0xB79500
uint32_t CStats::m_WeaponCounter{};            // 0xB79504
uint32_t CStats::m_DeathCounter{};             // 0xB79508
uint32_t CStats::m_MaxHealthCounter{};         // 0xB7950C
uint32_t CStats::m_AddToHealthCounter{};       // 0xB79510
uint32_t CStats::m_LastWeaponTypeFired{};      // 0xB79514
bool     CStats::bShowUpdateStats{};           // 0x8CDE56

// TODO: implement CStats methods from decomp.
