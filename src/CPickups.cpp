// CPickups.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CPickups. Adapted from gta-reversed for clean-room C++ build.
//
// All methods below are TODO stubs: verify each against the decomp
// (C:\Users\fufid\Documents\Decomps\gta-sa decomp\src\CPickups\*.c)
// and gta-reversed/source/game_sa/Pickups.cpp before implementing.
// Priority: Init, Update, GenerateNewOne / GenerateNewOne_WeaponType,
// RemovePickUp, Save/Load, TestForPickupsInBubble.

#include "CPickups.h"

// Static member definitions (original 1.0 US addresses in comments).
// TODO: re-resolve for the clean-room build.
uint8_t  CPickups::DisplayHelpMessage{};   // 0x8A5F48
int32_t  CPickups::PlayerOnWeaponPickup{}; // 0x97D640
int32_t  CPickups::StaticCamStartTime{};   // 0x978618
CVector* CPickups::StaticCamCoors{};        // was (CVector*)0x97D660
CVehicle* CPickups::pPlayerVehicle{};      // 0x97861C
bool     CPickups::bPickUpcamActivated{};  // 0x978620
uint16_t CPickups::CollectedPickUpIndex{}; // 0x978624
std::array<int32_t, MAX_COLLECTED_PICKUPS> CPickups::aPickUpsCollected{}; // 0x978628
uint16_t CPickups::NumMessages{};          // 0x978678
std::array<tPickupMessage, MAX_PICKUP_MESSAGES> CPickups::aMessages{}; // 0x978680
std::array<CPickup, MAX_NUM_PICKUPS> CPickups::aPickUps{};             // 0x9788C0

// TODO: implement CPickups methods from decomp.
