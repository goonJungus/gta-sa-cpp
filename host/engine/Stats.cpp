// CStats: the statistics store. Float stats 0..81 and integer stats 120..342 share one array; the weapon
// skills (69..79) live in WeaponStats, which the combat code already advances, so they are read through it.

#include "Stats.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "WeaponStats.h"

namespace Stats {
namespace {
float g_v[kCount] = {};

// STAT_PISTOL_SKILL .. STAT_RIFLE_SKILL -> WeaponStats entry (Micro Uzi stat is the machine pistol one)
const char* const kSkillWeapon[11] = {"PISTOL", "PISTOL_SILENCED", "DESERT_EAGLE", "SHOTGUN", "SAWNOFF", "SPAS12",
                                      "MICRO_UZI", "MP5", "AK47", "M4", "SNIPERRIFLE"};
int SkillSlot(int stat) {
    if (stat < STAT_PISTOL_SKILL || stat > STAT_RIFLE_SKILL) return -1;
    return WeaponStats::Index(kSkillWeapon[stat - STAT_PISTOL_SKILL]);
}
// fat and muscle are kept in the ini [Player] section by the player code (0..100), not under S<id>
bool Kept(int stat) { return stat == STAT_FAT || stat == STAT_MUSCLE || (stat >= STAT_PISTOL_SKILL && stat <= STAT_RIFLE_SKILL); }
}  // namespace

void Init() {
    std::fill(g_v, g_v + kCount, 0.0f);
    g_v[STAT_MAX_HEALTH] = 569.0f;
    g_v[STAT_STAMINA] = 100.0f;
}

bool IsFloat(int stat) { return stat < 82; }

bool IsCapped(int stat) {
    switch (stat) {
    case STAT_GIRLFRIEND_RESPECT: case STAT_CLOTHES_RESPECT: case STAT_FITNESS_RESPECT: case STAT_FAT: case STAT_STAMINA:
    case STAT_MUSCLE: case STAT_MAX_HEALTH: case STAT_SEX_APPEAL: case STAT_PISTOL_SKILL: case STAT_SILENCED_PISTOL_SKILL:
    case STAT_DESERT_EAGLE_SKILL: case STAT_SHOTGUN_SKILL: case STAT_SAWN_OFF_SHOTGUN_SKILL: case STAT_COMBAT_SHOTGUN_SKILL:
    case STAT_MACHINE_PISTOL_SKILL: case STAT_SMG_SKILL: case STAT_AK_47_SKILL: case STAT_M4_SKILL: case STAT_RIFLE_SKILL:
    case STAT_APPEARANCE: case STAT_ARMOR: case STAT_ENERGY: case STAT_DRIVING_SKILL: case STAT_FLYING_SKILL:
    case STAT_LUNG_CAPACITY: case STAT_BIKE_SKILL: case STAT_LUCK: case STAT_HORSESHOES_COLLECTED: case STAT_TOTAL_HORSESHOES:
    case STAT_OYSTERS_COLLECTED: case STAT_TOTAL_OYSTERS: case STAT_CYCLING_SKILL:
        return true;
    }
    return false;
}

float Get(int stat) {
    if (stat < 0 || stat >= kCount) return 0.0f;
    if (const int s = SkillSlot(stat); s >= 0) return WeaponStats::g_stat[s];
    return g_v[stat];
}

void Set(int stat, float value) {
    if (stat < 0 || stat >= kCount) return;
    if (const int s = SkillSlot(stat); s >= 0) { WeaponStats::g_stat[s] = value; return; }
    g_v[stat] = IsFloat(stat) ? value : (float)(int)value;
}

// CStats::ProcessReactionStatsOnIncrement: stamina and lung capacity strip fat while energy is negative,
// energy above 1000 turns into fat.
static void ReactIncrement(int stat) {
    if (stat != STAT_STAMINA && stat != STAT_ENERGY && stat != STAT_LUNG_CAPACITY) return;
    const float energy = Get(STAT_ENERGY);
    if (stat == STAT_STAMINA || stat == STAT_LUNG_CAPACITY) {
        if (energy < 0.0f) Set(STAT_FAT, std::max(Get(STAT_FAT) - 23.0f, 0.0f));
        return;
    }
    if (energy > 1000.0f) Increment(STAT_FAT, energy - 1000.0f);
}

void Increment(int stat, float value) {
    if (value <= 0.0f || stat < 0 || stat >= kCount) return;
    if (IsFloat(stat)) {
        Set(stat, Get(stat) + value);
        if (IsCapped(stat)) Set(stat, std::min(Get(stat), 1000.0f));
        ReactIncrement(stat);
        return;
    }
    if (stat == STAT_CALORIES) {   // eating: the food heals first, what is left over fattens (half of it)
        const float missing = g_hooks.maxHealth && g_hooks.health ? g_hooks.maxHealth() - g_hooks.health() : 0.0f;
        Increment(STAT_RIOT_MISSION_ACCOMPLISHED, value);
        if (value > missing) Increment(STAT_FAT, (value - missing) / 2.0f);
        return;
    }
    if (stat == STAT_RIOT_MISSION_ACCOMPLISHED) {   // the health the food gave
        const float missing = g_hooks.maxHealth && g_hooks.health ? g_hooks.maxHealth() - g_hooks.health() : 0.0f;
        if (g_hooks.addHealth) g_hooks.addHealth(value, value >= missing);
        return;
    }
    g_v[stat] += (float)(int)value;
    if (IsCapped(stat)) g_v[stat] = std::min(g_v[stat], 1000.0f);
}

void Decrement(int stat, float value) {
    if (value <= 0.0f) return;
    Set(stat, std::max(Get(stat) - value, 0.0f));
    if (stat == STAT_ENERGY && Get(STAT_ENERGY) < 0.0f) Decrement(STAT_FAT, 23.0f);
}

void Modify(int stat, float value) {
    if (value < 0.0f) Decrement(stat, -value);
    else Increment(stat, value);
}

void SetRecord(int stat, float value) {
    if (Get(stat) < value) Set(stat, value);
}

int MaxGroupMembers() {
    const float r = g_v[STAT_TOTAL_RESPECT];
    return r < 10.0f ? 0 : r < 60.0f ? 2 : r < 160.0f ? 3 : r < 330.0f ? 4 : r < 540.0f ? 5 : r < 800.0f ? 6 : 7;
}

void Load(const char* ini) {
    for (int i = 0; i < kCount; i++) {
        if (Kept(i)) continue;
        char key[16], buf[32];
        std::snprintf(key, sizeof key, "S%d", i);
        if (GetPrivateProfileStringA("Stats", key, "", buf, sizeof buf, ini) && buf[0]) Set(i, (float)std::atof(buf));
    }
}

void Save(const char* ini) {
    static float s_saved[kCount] = {};   // what the ini holds, so only changes are written
    for (int i = 0; i < kCount; i++) {
        if (Kept(i)) continue;
        if (s_saved[i] == g_v[i]) continue;
        char key[16], buf[32];
        std::snprintf(key, sizeof key, "S%d", i);
        std::snprintf(buf, sizeof buf, "%g", g_v[i]);
        WritePrivateProfileStringA("Stats", key, buf, ini);
        s_saved[i] = g_v[i];
    }
}

}  // namespace Stats
