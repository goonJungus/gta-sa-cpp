// CCarAI.h - GTA SA 1.0 clean-room C++ conversion
// Minimal stand-in for gta-reversed/source/game_sa/CarAI.h
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// TODO: full port of CCarAI (class + CarAI.cpp). Bodies verified against
// decomp src/CCarAI/*.c when the port lands.

#pragma once

#include "CVector.h"

class CVehicle;

class CCarAI {
public:
    static void MakeWayForCarWithSiren(CVehicle* veh);
    static float GetCarToParkAtCoors(CVehicle* veh, const CVector& coors);
    static void UpdateCarAI(CVehicle* veh);
};
