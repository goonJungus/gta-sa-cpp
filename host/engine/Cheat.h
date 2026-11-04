#pragma once
// CCheat: the typed-code matcher and the 92 cheat slots. CCheat::AddToCheatString keeps the last 30 typed
// letters, newest first. After every key the first 6..29 letters are hashed with CKeyGen::GetKey (CRC32 of
// the upper-cased text, no final inversion) and looked up in m_aCheatHashKeys. A hit tells the player
// (CHEAT1 on, CHEAT8 off), counts STAT_TIMES_CHEATED, runs the slot's function (or toggles m_aCheatsActive)
// and clears the buffer. The effects live in the host (main.cpp); this class owns the table and the flags.

#include <cstdint>

class Cheat {
public:
    // eCheats: the slot index is the table row, and the m_aCheatsActive flag the handlers toggle.
    enum Index {
        WeaponSet1 = 0, WeaponSet2, WeaponSet3, HealthArmorMoney, WantedUp, WantedDown,
        Sunny, ExtraSunny, Cloudy, Rainy, Foggy, FasterClock, FasterGameplay, SlowerGameplay,
        Mayhem, EverybodyAttacks, PedsArmed, Rhino, Bloodring, Rancher, HotringA, HotringB, Romero, Stretch,
        Trashmaster, Caddy, BlowUpCars, Suicide = 29, PinkCars = 32, BlackCars,
        Fat = 36, Muscle, Skinny, Elvis, Village, Beach, Gangs, GangLand, Ninja, LoveConquers,
        CheapCars, FastCars, Hydra = 50, Vortex, Midnight = 55, Dusk, Storm, Sandstorm, Predator,
        InfiniteHealth = 61, InfiniteOxygen, Parachute, Jetpack, NotWanted, Wanted, MegaPunch = 67, Riot = 69, Funhouse, Adrenaline, Driveby = 73, Country = 75,
        NeverHungry = 68, MaxRespect = 79, MaxSexAppeal, Stamina = 81, WeaponSkills, VehicleSkills, Hunter, Quad, Tanker, Dozer, StuntPlane, MonsterTruck,
        kCount = 92
    };
    // A typed letter A..Z (the WndProc calls this on every key-down that is not an auto-repeat).
    static void Push(char c);
    // The slot a typed code completed since the last call (-1 if none).
    static int Take();
    static void Trigger(int i);   // dev: a slot without typing its code
    static uint32_t Key(const char* s, int n);
    // m_aCheatsActive
    static bool Active(int i) { return i >= 0 && i < kCount && s_active[i]; }
    static void SetActive(int i, bool on) { if (i >= 0 && i < kCount) s_active[i] = on; }
    static bool Toggle(int i) { if (i < 0 || i >= kCount) return false; return s_active[i] = !s_active[i]; }
    static bool HasCheated() { return s_cheated; }   // m_bHasPlayerCheated
    static void MarkCheated() { s_cheated = true; }
    // CHEAT_MEGA_PUNCH is a toggle (no typed code yet: GTA_MEGAPUNCH): fists, brass knuckles and the parachute kill in one hit with 20x the death force.
    static bool MegaPunchOn();
    static void ToggleMegaPunch();
    // CPlayerPed::GivePlayerGoodiesWithPickUpMI(adrenaline): m_bAdrenaline for 20 s, no sprint drain, death force 5, strength 2.
    static bool AdrenalineOn();
    static void StartAdrenaline();
    static void ClearAdrenaline();
    static void Tick(float dt);   // runs the adrenaline timer
private:
    static bool s_active[kCount];
    static bool s_cheated;
};
