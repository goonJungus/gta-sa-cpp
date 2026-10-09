// CTimeInfo - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Models/TimeInfo.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), VALIDATE_SIZE (now a guarded static_assert).
// IsVisibleNow() moved to the .cpp stub - it needs CClock::GetIsTimeInRange
// (clock subsystem not ported yet).

#pragma once

#include <cstdint>

class CTimeInfo {
public:
    uint8_t m_nTimeOn;
    uint8_t m_nTimeOff;
    int16_t m_nOtherTimeModel = -1;

public:
    CTimeInfo* FindOtherTimeModel(const char* modelName);

    int32_t GetOtherTimeModel() const { return m_nOtherTimeModel; } // 0x004C4A30

    void SetOtherTimeModel(int16_t otherTimeModel) { m_nOtherTimeModel = otherTimeModel; } // 0x005B3440

    uint8_t GetTimeOn() const { return m_nTimeOn; } // 0x00407320

    uint8_t GetTimeOff() const { return m_nTimeOff; } // 0x00407330

    void SetTimes(uint8_t timeOn, uint8_t timeOff) { m_nTimeOn = timeOn; m_nTimeOff = timeOff; } // 0x005B3430

    // Was inline in the original; needs CClock (not ported yet) - see the .cpp stub.
    bool IsVisibleNow() const noexcept;
};

// Layout check: gta-reversed VALIDATE_SIZE(CTimeInfo, 0x4), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CTimeInfo) == 0x4, "CTimeInfo layout drift");
#endif
