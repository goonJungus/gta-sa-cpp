#pragma once
// CAETwinLoopSoundEntity - minimal stub (plugin-sdk: 0xA8, derives CAEAudioEntity).
// Only the members touched by the CPed TU are modeled.
// TODO(port): full audio port (added 2026-10-09 for CPed).
#include "CAESound.h"

struct CAETwinLoopSoundEntity {
    CAESound m_tempSound; // TODO(port): stub
    bool m_IsInUse = false; // TODO(port): stub
    CAESound* m_Sounds[2] = { nullptr, nullptr }; // TODO(port): stub; decomp m_Sounds
};
