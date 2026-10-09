// CWeaponEffects - adapted from gta-reversed for clean-room C++ build
// Method stubs. Decompiled reference: src/CWeaponEffects/*.c
// Each method below corresponds to a decompiled function - fill in from the .c file noted.

#include "CWeaponEffects.h"

// Static data (were StaticRef<> at fixed game addresses; see CWeaponEffects.h)
std::array<CWeaponEffects, MAX_NUM_WEAPON_CROSSHAIRS> gCrossHair{}; // 0xC8A838
RwTexture* gpCrossHairTex{};           // 0xC8A818
RwTexture* gpCrossHairTexFlight[2]{};  // 0xC8A810

void CWeaponEffects::Init() {
    // TODO: src/CWeaponEffects/Init_00742ab0.c
}

void CWeaponEffects::Shutdown() {
    // TODO: src/CWeaponEffects/Shutdown_00742b80.c
}

bool CWeaponEffects::IsLockedOn(CrossHairId id) {
    // TODO: src/CWeaponEffects/IsLockedOn_00742bd0.c
    (void)id;
    return false;
}

void CWeaponEffects::MarkTarget(CrossHairId id, CVector posn, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float size, bool bClearImmediately) {
    // TODO: src/CWeaponEffects/MarkTarget_00742bf0.c
    (void)id;
    (void)posn;
    (void)red;
    (void)green;
    (void)blue;
    (void)alpha;
    (void)size;
    (void)bClearImmediately;
}

void CWeaponEffects::ClearCrossHair(CrossHairId id) {
    // TODO: src/CWeaponEffects/ClearCrossHair_00742c60.c
    (void)id;
}

void CWeaponEffects::ClearCrossHairs() {
    // TODO: src/CWeaponEffects/ClearCrossHairs_00742c80.c
}

void CWeaponEffects::ClearCrossHairImmediately(CrossHairId id) {
    // TODO: src/CWeaponEffects/ClearCrossHairImmediately_00742ca0.c
    (void)id;
}

void CWeaponEffects::ClearCrossHairsImmediately() {
    // TODO: src/CWeaponEffects/ClearCrossHairsImmediately_00742cc0.c
}

void CWeaponEffects::Render() {
    // TODO: src/CWeaponEffects/Render_00742cf0.c
}
