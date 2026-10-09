#pragma once
// CGameLogic - minimal stub.
// TODO(port): implement when game logic batch lands.

#include "CVector.h" // CVector (by value in IsPlayerAllowedToGoInThisDirection)
#include <cstdint>

class CPed;

class CGameLogic {
public:
    // TODO(port): stub
    static void Update();
    // TODO(port): decomp label for the CWeapon constructor invoked by _eh_vector_constructor_iterator_
    //   over weapon arrays (Ghidra mis-attributed it to CGameLogic). Call sites now construct CWeapons
    //   directly, so this stub is unreferenced until the pattern recurs.
    static void unk_00441e00(void* arg);

    // Game-flow state (gta-reversed: static int8 GameState @ 0xC33180,
    // static int8 SkipState @ 0xC3CE0B; decomp src/CGameLogic/*.c).
    // inline static so no new TU is needed until the game-logic batch lands.
    // Added 2026-10-09 for CAutomobile (was C2039/C2065 x10).
    inline static int8_t GameState{};
    inline static int8_t SkipState{};

    // Decomp @ 00441e10: bool __cdecl CGameLogic::IsPlayerAllowedToGoInThisDirection(CPed*, CVector, float).
    // TODO(port): real forbidden-area/roadblock checks. Stub returns true.
    // Added 2026-10-09 for CAutomobile.
    static bool IsPlayerAllowedToGoInThisDirection(CPed* ped, CVector moveDirection, float distanceLimit) {
        (void)ped; (void)moveDirection; (void)distanceLimit;
        return true;
    }
};
