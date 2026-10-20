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
// ar_stats.dat as shipped; LoadReactions replaces these
float g_react[R_COUNT] = {50, 50, 10, 5, 20, 50, 50, 25, 50, 10, 50, 20, 0.8f, 5.0f, 3.0f, 0.6f, 0.45f, 0.6f, 0.4f, 1.5f, 3.0f, 2.0f, 10.0f,
                          15, 15, 10, 5, 15, 10, 15, 10, 5, 5, 300, 180, 300, 300, 300, 300, 120, 180, 300, 150, 150, 60, 600,
                          50, 20, 50, 100, 100, 100, 100, 50, 50, 50, 20, 5, 1000};

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

float FatMuscleMod(int mode) {
    const float fat = Get(STAT_FAT), muscle = Get(STAT_MUSCLE), stamina = Get(STAT_STAMINA);
    const float fatN = (fat - 200.0f) / (1000.0f - 200.0f), musN = (muscle - 50.0f) / (1000.0f - 50.0f);
    const float fatPos = std::max(0.0f, fatN), bike = std::min(1.0f, Get(STAT_BIKE_SKILL) * 0.001f);
    float v = 1.0f;
    switch (mode) {
    case 0: return fat <= 400.0f ? 0.0f : fat <= 800.0f ? 1.0f : 2.0f;
    case 1: return std::max(0.7f, musN * 0.2f + fatPos * -0.4f + 1.0f);
    case 2: return std::max(0.8f, musN * -0.1f + fatPos * -0.2f + 1.0f);
    case 3: return std::max(0.8f, fatN * -0.2f + 1.0f + musN * -0.1f);
    case 4: v = musN + fatN * 0.5f + 1.0f; return v > 2.0f ? 2.0f : v;
    case 5:
        v = Get(STAT_CYCLING_SKILL) * 0.001f + stamina * 0.001f * 0.5f + musN * 0.5f + fatN * -1.0f + 1.0f;
        return v > 2.0f ? 2.0f : std::max(0.25f, v);
    case 6:
        v = Get(STAT_CYCLING_SKILL) * 0.001f + fatN * -0.5f + 1.0f;
        return v > 2.0f ? 2.0f : std::max(0.5f, v);
    case 7: return stamina * 0.001f * 3000.0f + 150.0f;
    case 8: return (Get(STAT_LUNG_CAPACITY) + stamina) * 0.0005f * 3000.0f + 1000.0f;
    case 9: return Get(STAT_MAX_HEALTH) * 0.001f * 176.0f;
    case 10: return 176.0f;
    case 11: return bike * 0.5f + 1.0f;
    case 12: return bike * 0.3f + 1.0f;
    case 13: return bike * 0.25f + 1.0f;
    case 14: v = Get(STAT_DRIVING_SKILL) * 0.001f * 0.5f + 1.0f + fatN * 0.5f; return v > 1.0f ? 1.0f : v;
    case 15: return std::max(0.5f, musN * -0.25f + fatN * -0.5f + 1.0f);
    }
    return 1.0f;
}

float React(int index) { return index >= 0 && index < R_COUNT ? g_react[index] : 0.0f; }

int LoadReactions(const char* path) {
    FILE* f = std::fopen(path, "r");
    if (!f) return 0;
    int n = 0;
    char line[256];
    while (std::fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == ';') continue;
        int idx = 0;
        char name[96];
        float v = 0;
        if (std::sscanf(line, "%d %95s %f", &idx, name, &v) == 3 && idx >= 0 && idx < R_COUNT) { g_react[idx] = v; n++; }
    }
    std::fclose(f);
    return n;
}

// CStats::UpdateStatsWhenSprinting / Running / Swimming / Driving and the max-health part of UpdateFatAndMuscleStats,
// with the ar_stats.dat values (STAT_INC_* amounts, STAT_TIMELIMIT_* seconds). Counters run in milliseconds.
int UpdateActivity(float dt, int rate, bool swimming, bool underwater, int vehicle, float carSpeed, bool hasDriver) {
    static float stamina = 0, swim = 0, breath = 0, drive = 0, health = 0, bike = 0, fly = 0;
    const float ms = dt * 1000.0f;
    int raised = -1;
    auto fire = [&](float& counter, float limitSec, int stat, float inc) {
        if (counter > limitSec * 1000.0f) {
            counter = 0.0f;
            Increment(stat, inc);
            raised = stat;
        }
    };
    if (rate > 0) {
        health += ms;   // UpdateFatAndMuscleStats: the max-health counter runs on every call
        fire(health, React(R_TIMELIMIT_MAX_HEALTH), STAT_MAX_HEALTH, React(R_INC_MAX_HEALTH));
        if (swimming) {
            swim += ms;
            fire(swim, React(R_TIMELIMIT_SWIM_STAMINA), STAT_STAMINA, React(R_INC_SWIM_STAMINA));
        } else {
            stamina += ms;
            fire(stamina, React(rate >= 10 ? R_TIMELIMIT_SPRINT_STAMINA : R_TIMELIMIT_RUNNING), STAT_STAMINA, React(rate >= 10 ? R_INC_SPRINT_STAMINA : R_INC_RUNNING));
        }
    }
    if (swimming && underwater) {
        breath += ms;
        fire(breath, React(R_TIMELIMIT_BREATH_UNDERWATER), STAT_LUNG_CAPACITY, React(R_INC_BREATH_UNDERWATER));
    }
    if (vehicle == 1) {   // UpdateStatsWhenDriving
        if (!hasDriver || carSpeed > 0.8f) drive += ms;
        else if (carSpeed > 0.2f) drive += ms * 0.5f;
        fire(drive, React(R_TIMELIMIT_DRIVING_SKILL), STAT_DRIVING_SKILL, React(R_INC_DRIVING_SKILL));
    } else if (vehicle == 2) {   // UpdateStatsWhenOnMotorBike
        if (carSpeed > 0.6f) bike += ms;
        else if (carSpeed > 0.2f) bike += ms * 0.5f;
        fire(bike, React(R_TIMELIMIT_MOTORBIKE_SKILL), STAT_BIKE_SKILL, React(R_INC_MOTORBIKE_SKILL));
    } else if (vehicle == 3) {   // UpdateStatsWhenFlying
        if (carSpeed > 1.3f) fly += ms;
        else if (carSpeed > 0.5f) fly += ms * 0.5f;
        fire(fly, React(R_TIMELIMIT_FLYING_SKILL), STAT_FLYING_SKILL, React(R_INC_FLYING_SKILL));
    }
    return raised;
}

static float s_respectShown = -99.0f;
void ResetRespectMessage() { s_respectShown = -99.0f; }

int UpdateRespect(int money, float territoryFrac, int tagPct, bool cheat) {
    const float cash = std::min(1.0f, (float)money * 1e-7f);
    const float terr = std::max(0.0f, territoryFrac - 0.2f);
    const float missionTotal = std::max(1.0f, Get(STAT_RESPECT_MISSION_TOTAL));
    float r = Get(STAT_CLOTHES_RESPECT) * 0.03f + Get(STAT_MUSCLE) * 0.03f + cash * 1000.0f * 0.05f + terr * 1250.0f * 0.05f +
              Get(STAT_GIRLFRIEND_RESPECT) * 0.03f + (Get(STAT_RESPECT_MISSION) * 1000.0f / missionTotal) * 0.36f +
              Get(STAT_RESPECT) * 0.4f + (float)(tagPct * 10) * 0.05f;
    r = std::max(0.0f, r);
    if (cheat) r = 1000.0f;
    Set(STAT_TOTAL_RESPECT, r);
    if (s_respectShown == -99.0f) { s_respectShown = r; return 0; }
    if (r <= s_respectShown + 40.0f && r >= s_respectShown - 40.0f) return 0;   // CStats::CheckForThreshold
    const int dir = r > s_respectShown ? 1 : -1;
    s_respectShown = r;
    return dir;
}

void UpdateSexAppeal(bool haveCar, float carDist, float carHealth, int carClass, bool cheat) {
    float car = 0.0f;
    if (haveCar && carDist < 35.0f) {
        car = std::clamp(carHealth * 2.0f - 1000.0f, 0.0f, 1000.0f);
        switch (carClass) {   // how much a car of this class impresses
        case 1: case 5: case 7: case 11: car *= 0.1f; break;
        case 2: case 3: case 9: break;
        case 4: case 6: case 10: car *= 0.3f; break;
        default: car *= 0.5f;
        }
    }
    Set(STAT_SEX_APPEAL, std::clamp(car * 0.5f + Get(STAT_APPEARANCE) * 0.5f, 0.0f, 1000.0f));
    if (cheat) Set(STAT_SEX_APPEAL, 1000.0f);
}

}  // namespace Stats
