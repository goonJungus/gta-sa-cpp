#pragma once
// Vehicle weapons: CAutomobile::TankControl (the rhino cannon) and FireTruckControl (the fire truck and swat van
// water cannon). The turret angles live on the VehicleInstance (doomV yaw, doomH pitch) and rotate the misc_a /
// misc_b frames when the car is drawn.

#include <cstddef>
#include <functional>

#include "Vehicle.h"
#include "WaterCannon.h"

namespace VehicleGuns {

constexpr int kRhino = 432, kFireTruck = 407, kSwatVan = 601;

enum class Kind { None, Tank, WaterCannon };
Kind KindOf(int model);

// The turret follows the camera: camFront is the camera's forward vector in the world (the "on a string" camera
// mode of retail). dt is in seconds.
void AimTank(VehicleInstance& c, const float camFront[3], float dt);
void AimWater(VehicleInstance& c, const float camFront[3], float dt);

struct TankShot {
    float origin[3];
    float dir[3];
    float end[3];      // origin + dir * 60
};
// TankControl's fire branch: 800 ms between shots. Applies the recoil to the car. Returns false while reloading.
bool FireTank(Vehicles& v, size_t car, float nowMs, TankShot* shot);

// FireTruckControl's fire branch: feeds one section to the car's water cannon (owner id = car + 1).
void FireWater(Vehicles& v, size_t car, WaterCannons& wc, float nowMs, float dt);

void ResetClock();   // forget the last tank shot (dev tests)

}  // namespace VehicleGuns
