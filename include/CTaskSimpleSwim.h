#pragma once
// CTaskSimpleSwim - minimal stub for CPed.cpp (full port lands with the task batch).
// Source layout: gta-reversed/source/game_sa/Tasks/TaskTypes/TaskSimpleSwim.h
// (VALIDATE_SIZE 0x64).
//
// Only CTaskSimpleSwim::m_fSwimStopTime is exposed (used by CPed::ProcessBuoyancy);
// it sits at offset 0x54 in the real class. Standalone struct (not derived from the
// project's 4-byte CTask stub) with explicit padding so the offset stays exact
// regardless of what the CTask stub looks like.
// Added 2026-10-09 for CPed compile errors.

#include <cstdint>

class CTaskSimpleSwim {
public:
    uint8_t _pad[0x54];
    float m_fSwimStopTime; // 0x54 (gta-reversed TaskSimpleSwim.h)
};
static_assert(sizeof(CTaskSimpleSwim) == 0x58, "CTaskSimpleSwim stub size");
