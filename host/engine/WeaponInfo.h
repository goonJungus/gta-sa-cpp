#pragma once
// CWeaponInfo: the whole of data\weapon.dat, every gun ($) and melee row, at all three skill levels
// (CWeaponInfo::LoadWeaponData). Anim loop times come out in seconds with retail's end adjustment.

#include <string>
#include <vector>

struct WeaponInfo {
    enum Fire { Melee, InstantHit, Projectile, AreaEffect, Camera, Use };
    bool valid = false;
    std::string name;
    Fire fire = InstantHit;
    float targetRange = 0, weaponRange = 0;
    int model1 = -1, model2 = -1;
    int slot = 0;
    std::string animGroup;                  // "null" rows have none
    int ammoClip = 0, damage = 0;
    float fireOffset[3] = {0, 0, 0};
    int skill = 1;                          // 0 poor, 1 standard, 2 pro
    int reqStat = 0;
    float accuracy = 1.0f, moveSpeed = 1.0f;
    float loopStart = 0, loopEnd = 0, loopFire = 0;      // seconds
    float loop2Start = 0, loop2End = 0, loop2Fire = 0;
    float breakout = 0;                     // seconds
    unsigned flags = 0;
    float speed = 0, radius = 0, lifespan = 0, spread = 0;   // projectile / area weapons
    // melee rows
    std::string baseCombo;
    int numCombos = 0;
    unsigned meleeFlags = 0;
    std::string stealthAnim;

    bool CanAim() const { return flags & 0x1; }
    bool AimWithArm() const { return flags & 0x2; }
    bool FirstPerson() const { return flags & 0x4; }
    bool OnlyFreeAim() const { return flags & 0x8; }
    // CWeaponInfo::flags bit layout (byte 0: aim bits, byte 1: handling, byte 2: area-effect shots)
    bool MoveAim() const { return flags & 0x10; }
    bool MoveFire() const { return flags & 0x20; }
    bool Throw() const { return flags & 0x100; }
    bool Heavy() const { return flags & 0x200; }
    bool ContinuousFire() const { return flags & 0x400; }
    bool TwinPistol() const { return flags & 0x800; }
    bool Reload() const { return flags & 0x1000; }
    bool CrouchFire() const { return flags & 0x2000; }
    bool Reload2Start() const { return flags & 0x4000; }
    bool LongReload() const { return flags & 0x8000; }
    bool SlowsDown() const { return flags & 0x10000; }
    bool RandSpeed() const { return flags & 0x20000; }
    bool Expands() const { return flags & 0x40000; }
    bool RangeGrows() const { return flags & 0x80000; }
};

namespace WeaponTable {
// Parse data\weapon.dat from the game folder; returns the number of rows read.
int Load(const std::string& gameDir);
// A weapon at a skill level (CWeaponInfo::GetWeaponInfo): a weapon with one row answers every level.
const WeaponInfo* Get(const std::string& name, int skill = 1);
const std::vector<WeaponInfo>& All();
// CWeaponInfo::LoadWeaponData: the loop end is snapped to the 50 Hz tick, less a hair.
float LoopEnd(float start, float end);
}  // namespace WeaponTable
