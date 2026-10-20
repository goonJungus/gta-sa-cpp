#pragma once
// CCheat::AddToCheatString: the last 30 typed letters, newest first. After every key the buffer's
// first 6..29 letters are hashed with CKeyGen::GetKey (CRC32 of the upper-cased text, no final
// inversion) and looked up in m_aCheatHashKeys. A hit applies the cheat and clears the buffer.

#include <cstdint>

class Cheat {
public:
    enum Id { None = 0, Jetpack, HealthArmorMoney, Adrenaline, Fat, Muscle, Skinny, WeaponSkills, VehicleSkills, Stamina };
    // A typed letter A..Z (the WndProc calls this on every key-down that is not an auto-repeat).
    static void Push(char c);
    // The cheat a key completed since the last call (None if no cheat).
    static Id Take();
    static uint32_t Key(const char* s, int n);
    // CHEAT_MEGA_PUNCH is a toggle (no typed code yet: GTA_MEGAPUNCH): fists, brass knuckles and the parachute kill in one hit with 20x the death force.
    static bool MegaPunchOn();
    static void ToggleMegaPunch();
    // CPlayerPed::GivePlayerGoodiesWithPickUpMI(adrenaline): m_bAdrenaline for 20 s, no sprint drain, death force 5, strength 2.
    static bool AdrenalineOn();
    static void StartAdrenaline();
    static void ClearAdrenaline();
    static void Tick(float dt);   // runs the adrenaline timer
};
