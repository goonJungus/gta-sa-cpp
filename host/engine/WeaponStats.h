#pragma once
// Weapon skill stats (port of CStats weapon skills): a 0..1000 stat per firearm that rises as the
// player hits peds (CStats::UpdateStatsWhenWeaponHit, rates from ar_stats.dat) and selects the
// poor / standard / pro weapon.dat row (CPed::GetWeaponSkill).

#include <cstring>

namespace WeaponStats {

struct Entry {
    const char* weapon;   // weapon.dat name
    float inc;            // ar_stats.dat increment per qualifying hit
    int timeLimit;        // hits before the "+" help box shows
    float stdReq;         // weapon.dat reqStat of the standard row
};

// Micro Uzi and Tec-9 share the machine pistol stat.
inline constexpr Entry kEntries[] = {
    {"PISTOL", 0.8f, 50, 40.0f},       {"PISTOL_SILENCED", 5.0f, 20, 500.0f}, {"DESERT_EAGLE", 3.0f, 50, 200.0f},
    {"SHOTGUN", 0.6f, 100, 200.0f},    {"SAWNOFF", 0.45f, 100, 200.0f},       {"SPAS12", 0.6f, 100, 200.0f},
    {"MICRO_UZI", 0.4f, 100, 50.0f},   {"TEC9", 0.4f, 100, 50.0f},            {"MP5", 1.5f, 50, 250.0f},
    {"AK47", 3.0f, 50, 200.0f},        {"M4", 2.0f, 50, 200.0f},              {"SNIPERRIFLE", 10.0f, 20, 300.0f},
    {"COUNTRYRIFLE", 10.0f, 20, 300.0f},
};
inline constexpr int kCount = (int)(sizeof(kEntries) / sizeof(kEntries[0]));
inline constexpr float kProReq = 999.0f;

inline float g_stat[kCount] = {};
inline int g_counter[kCount] = {};
inline int g_forced = -1;   // [Player] WeaponSkill / GTA_SKILL: every weapon at this skill, stats frozen

inline int Index(const char* weapon) {
    // the machine pistols and the two rifles share one stat
    for (int i = 0; i < kCount; i++) if (!std::strcmp(kEntries[i].weapon, weapon)) return i;
    return -1;
}
inline int StatSlot(int i) {   // TEC9 -> MICRO_UZI, COUNTRYRIFLE -> SNIPERRIFLE
    if (i < 0) return i;
    if (!std::strcmp(kEntries[i].weapon, "TEC9")) return Index("MICRO_UZI");
    if (!std::strcmp(kEntries[i].weapon, "COUNTRYRIFLE")) return Index("SNIPERRIFLE");
    return i;
}
// CPed::GetWeaponSkill: 2 pro, 1 standard, 0 poor.
inline int Skill(const char* weapon) {
    if (g_forced >= 0) return g_forced;
    const int i = Index(weapon);
    if (i < 0) return 1;
    const float s = g_stat[StatSlot(i)];
    return s >= kProReq ? 2 : s >= kEntries[i].stdReq ? 1 : 0;
}
inline float Stat(const char* weapon) { const int i = StatSlot(Index(weapon)); return i < 0 ? 0.0f : g_stat[i]; }
// CStats::UpdateStatsWhenWeaponHit: true when the stat changed; showBox set when the help box is due.
inline bool OnHit(const char* weapon, bool* showBox) {
    *showBox = false;
    const int i = Index(weapon);
    if (i < 0 || g_forced >= 0) return false;
    const int s = StatSlot(i);
    if (g_stat[s] >= 1000.0f) return false;
    g_stat[s] += kEntries[i].inc;
    if (g_stat[s] > 1000.0f) g_stat[s] = 1000.0f;
    if (++g_counter[s] > kEntries[i].timeLimit) { g_counter[s] = 0; *showBox = true; }
    return true;
}

}  // namespace WeaponStats
