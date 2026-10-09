// CStats - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Stats.h
// Player statistics: criminal rating, skill/fat/muscle stats, mission attempts,
// favorite radio stations, reaction stats, stat update messages.
//
// Adaptations: stripped plugin-sdk header block, InjectHooks(), StaticRef globals
// (now plain static members; original 1.0 US addresses kept as comments and
// re-resolved in CStats.cpp for the clean-room build), VALIDATE_SIZE ->
// guarded static_assert. eStats / eStatModAbilities / eStatsReactions /
// eRadioID enums not yet ported: opaque declarations here (underlying types TODO).
// GxtChar comes from RenderTypes.h. The NOTSA GetStatValue<T> template is kept
// but adapted to C++17 (no concepts/requires; std::in_range is C++20, the range
// check is simplified — restore it when the project moves past C++17).

#pragma once

#include "RenderTypes.h" // GxtChar

#include <array>
#include <cassert>
#include <cstdint>

enum eStatUpdateState : uint8_t {
    STAT_UPDATE_DECREASE = 0,
    STAT_UPDATE_INCREASE = 1
};

enum eStatMessageCondition {
    STATMESSAGE_LESSTHAN = 0,
    STATMESSAGE_MORETHAN = 1
};

struct tStatMessage {
    int16_t stat_num; // unique stat id
    bool    displayed;
    uint8_t condition; // this can be lessthan/morethan, see eStatMessageCondition
    float   value;             // value stat must reach to display message
    char    text_id[8];        // text id from american.gxt text file to display
};

// ---- Stat enums: eStats / eStatModAbilities ported 2026-10-09; eStatsReactions still opaque ----
#include "eStats.h"            // canonical (gta-reversed Enums/eStats.h)
#include "eStatModAbilities.h" // canonical (gta-reversed Enums/eStatModAbilities.h)
enum eStatsReactions : int32_t; // opaque; verify underlying type when ported
#include "eRadioID.h" // canonical (deduped 2026-10-09)

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tStatMessage) == 0x10, "tStatMessage layout changed");
#endif

// class CStats - minimal declaration for the clean-room C++ build.
// Only the static methods used by converted TUs are declared here; bodies are
// inline stubs until Stats.cpp is ported. (CStats.cpp is not in the build yet.)
class CStats {
public:
    static float GetFatAndMuscleModifier(eStatModAbilities) { return 1.0f; } // TODO(stats): real stat lookup
    static void UpdateStatsWhenSprinting() {}                                // TODO(stats)
    static void UpdateStatsWhenRunning() {}                                  // TODO(stats)
    static int32_t FindMaxNumberOfGroupMembers() { return 0; }               // TODO(stats)
    static float GetStatValue(eStats) { return 0.0f; }                       // TODO(stats)
    static void IncrementStat(eStats, float = 1.0f) {}                       // TODO(stats)
    static void DisplayScriptStatUpdateMessage(eStatUpdateState, eStats, float) {} // TODO(stats)
    static float GetPercentageProgress() { return 0.0f; }                    // TODO(stats): real stat lookup. Added 2026-10-09 for CAutomobile.
};
