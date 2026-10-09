// cHandlingDataMgr.h - GTA SA 1.0 clean-room C++ conversion
// Minimal stand-in for gta-reversed/source/game_sa/cHandlingDataMgr.h
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// TODO: full port of cHandlingDataMgr (class + cHandlingDataMgr.cpp:
// Convert*DataToGameUnits, GetFlyingPointer, GetBikeHandlingPointer,
// LoadHandlingData, ...). Bodies verified against decomp
// src/cHandlingDataMgr/*.c when the port lands.

#pragma once

#include "Common.h" // StaticRef
#include "tHandlingData.h"
#include "tBoatHandlingData.h"

#include <cstdint>

class cHandlingDataMgr {
public:
    tBoatHandlingData* GetBoatPointer(uint8_t handlingId);
    tHandlingData*     GetVehiclePointer(uint32_t handlingId);

    // gta-reversed cHandlingDataMgr.cpp (added 2026-10-09 for CAutomobile).
    // TODO(port): verify against decomp when the handling batch lands.
    bool HasFrontWheelDrive(uint8_t handlingId) {
        return GetVehiclePointer(handlingId)->GetTransmission().m_nDriveType != 'R';
    }
    bool HasRearWheelDrive(uint8_t handlingId) {
        return GetVehiclePointer(handlingId)->GetTransmission().m_nDriveType != 'F';
    }
};

// 0xC2B9C8
static inline auto& gHandlingDataMgr = StaticRef<cHandlingDataMgr>(0xC2B9C8);
