// CTempColModels - minimal clean-room header for clean-room C++ build
// Source: gta-reversed/source/game_sa/Collision/TempColModels.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
//
// MINIMAL PORT: only ms_colModelPed2 (used by CPed::ProcessEntityCollision).
// TODO: full port with the collision batch.
// Added 2026-10-09 for CPed compile errors.

#pragma once

#include "CColModel.h" // CColModel (full type for the static member)

class CTempColModels {
public:
    // gta-reversed: static inline auto& ms_colModelPed2 = StaticRef<CColModel>(0x968E20);
    // Clean-room: plain static member; the original game address is kept as a comment.
    // TODO(port): re-resolve for the clean-room build. Defined in the collision batch.
    static CColModel ms_colModelPed2; // 0x968E20
};
