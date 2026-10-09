// CCarCtrl - minimal stub for the clean-room C++ build
// Source: gta-reversed/source/game_sa/CarCtrl.h
//
// Only the vehicle-of-interest registration used by converted TUs.
// Full port (traffic, roadblocks, etc.) belongs to a later pass.

#pragma once

#include <cstdint>

class CVehicle;

class CCarCtrl {
public:
    // Merged 2026-10-09 from CAutomobile.cpp's local stubs (were TODO(port)).
    static int32_t NumAmbulancesOnDuty;
    static int32_t NumFireTrucksOnDuty;

    static void RegisterVehicleOfInterest(CVehicle* vehicle) { (void)vehicle; } // TODO(traffic): real registration

    // TODO(port): real implementations (gta-reversed CarCtrl.h).
    // Declared here so CAutomobile links; defined in the traffic batch.
    static void ScanForPedDanger(CVehicle* vehicle);
    static void UpdateCarOnRails(CVehicle* vehicle);
    static void SteerAICarWithPhysics(CVehicle* vehicle);
    static void ReconsiderRoute(CVehicle* vehicle);
    static void SwitchVehicleToRealPhysics(CVehicle* vehicle);
};
