// CCoverPoint - minimal clean-room header for clean-room C++ build
// Source: gta-reversed/source/game_sa/CoverPoint.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
//
// MINIMAL PORT: only ReleaseCoverPointForPed (called by CPed) is declared.
// TODO: full port with the tasks/cover batch.
// Added 2026-10-09 for CPed compile errors.

#pragma once

class CPed;

class CCoverPoint {
public:
    // gta-reversed CoverPoint.h: void ReleaseCoverPointForPed(CPed* ped);
    // TODO(port): real implementation.
    void ReleaseCoverPointForPed(CPed* ped);
};
