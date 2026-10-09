// CPopulation - minimal clean-room header for clean-room C++ build
// Adapted from gta-reversed/source/game_sa/Population.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
//
// MINIMAL PORT: only UpdatePedCount() used by CPed::ProcessControl.
// TODO: port the full population system (ped spawning, groups, zones)
// when that batch lands.

#pragma once

class CPed;

class CPopulation {
public:
    // Updates the ped counter (pedAddedOrRemoved: false = added, true = removed).
    static void UpdatePedCount(CPed* ped, bool pedAddedOrRemoved);
    // TODO(port): stub; decomp CPopulation::NumMiamiViceCops (added 2026-10-09 for CPed)
    inline static int32_t NumMiamiViceCops{};
};
