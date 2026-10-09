// CTaskSimpleJetPack - minimal clean-room header for clean-room C++ build
// Source: gta-reversed/source/game_sa/Tasks/TaskTypes/TaskSimpleJetPack.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
//
// MINIMAL PORT: only RenderJetPack (called by CPed::Render) is declared.
// TODO: full port with the task batch (needs CTaskSimple base, FxSystem_c, etc.).
// Added 2026-10-09 for CPed compile errors.

#pragma once

class CPed;

class CTaskSimpleJetPack {
public:
    // gta-reversed TaskSimpleJetPack.h: void RenderJetPack(CPed* ped);
    // TODO(port): real implementation.
    void RenderJetPack(CPed* ped);
};
