#pragma once
// Vehicle weapons: CAutomobile::TankControl (the rhino cannon) and FireTruckControl (the fire truck and swat van
// water cannon). The turret angles live on the VehicleInstance (doomV yaw, doomH pitch) and rotate the misc_a /
// misc_b frames when the car is drawn.

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

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

// ---- CVehicle::ProcessWeapons: the guns and ordnance of planes and helicopters ----
constexpr int kPredator = 430, kHunter = 425, kSeaspar = 447, kRCBaron = 464, kRustler = 476, kMaverick = 487, kPolmav = 497,
              kHydra = 520, kCargobob = 548;

// CPad::GetCarGunFired / CarGunJustDown: 1 = VEHICLE_FIREWEAPON (LMB, right Ctrl, left Alt), 2 =
// VEHICLE_FIREWEAPON_ALT (left Ctrl, numpad 0). target = GetTarget held, enterTargeting = it went down.
struct AirPad {
    int gunFired = 0, gunJustDown = 0;
    bool target = false, enterTargeting = false;
};

struct AirFire {
    // CWeapon::FireInstantHit with the auto-aim applied; damage 0: the weapon's own (MINIGUN or M4)
    struct Shot { float origin[3], end[3]; bool minigun; float damage = 0.0f; };
    struct Missile { float pos[3], vel[3]; bool homing; int target; };   // vel in m/s; target car for a lock-on
    std::vector<Shot> shots;
    std::vector<Missile> missiles;
    std::vector<std::array<float, 6>> flares;   // position, velocity (m/s)
    bool shake = false;                         // CPad::StartShake(240, ...)
    // the Hydra's crosshair: the car it marks and whether it turned red (locked)
    int markCar = -1;
    bool locked = false;
};

// What the guns can aim at and see. clear: a line of sight through the world between two points.
struct AirWorld {
    std::vector<std::array<float, 3>> aimPoints;   // peds and cars in range (DoDriveByAutoAiming candidates)
    std::function<bool(const float*, const float*)> clear;
};

// Also the Predator's CVehicle::DoFixedMachineGuns.
void ProcessAirWeapons(Vehicles& v, size_t car, const AirPad& pad, uint32_t nowMs, float dt, const AirWorld& w, AirFire& out);

}  // namespace VehicleGuns
