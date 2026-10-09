// CTimeInfo - adapted from gta-reversed for clean-room C++ build
// Method implementations. Decompiled reference: src/CTimeInfo/*.c
// (GTA SA 1.0 @ addresses noted per method.)
// NOTE: GetOtherTimeModel (0x4C4A30), GetTimeOn (0x407320), GetTimeOff (0x407330),
// SetOtherTimeModel (0x5B3440) and SetTimes (0x5B3430) are inline in CTimeInfo.h,
// matching the original.

#include "CTimeInfo.h"
#include "CBaseModelInfo.h" // GetTimeInfo() virtual
#include "CModelInfo.h"     // ms_modelInfoPtrs

#include <cstdint>
#include <cstring>

// ---- PORT(keygen): CKeyGen not ported yet. Minimal interface used here;
//      replace with #include "CKeyGen.h" when it lands.
class CKeyGen {
public:
    static uint32_t GetUppercaseKey(const char* str);
};

// ---- PORT(clock): CClock not ported yet. Minimal interface used here;
//      replace with #include "CClock.h" when the clock subsystem lands.
class CClock {
public:
    static bool GetIsTimeInRange(uint8_t timeOn, uint8_t timeOff);
};

// 0x4C47E0
CTimeInfo* CTimeInfo::FindOtherTimeModel(const char* modelName) {
    // Build the day/night-swapped model name: "foo_nt" <-> "foo_dy".
    char timeSwitchModelName[24];
    strncpy(timeSwitchModelName, modelName, sizeof(timeSwitchModelName) - 1);
    timeSwitchModelName[sizeof(timeSwitchModelName) - 1] = '\0';

    char* nightSuffix = strstr(timeSwitchModelName, "_nt");
    if (nightSuffix) {
        strncpy(nightSuffix, "_dy", 4);
    } else {
        char* daySuffix = strstr(timeSwitchModelName, "_dy");
        if (!daySuffix)
            return nullptr;
        strncpy(daySuffix, "_nt", 4);
    }

    uint32_t key = CKeyGen::GetUppercaseKey(timeSwitchModelName);

    // Decomp scans ms_modelInfoPtrs for the first entry whose key matches AND
    // which actually has time info (vtable+0x14 = GetTimeInfo()).
    for (int32_t i = 0; i < 20000; ++i) {
        CBaseModelInfo* modelInfo = CModelInfo::ms_modelInfoPtrs[i];
        if (!modelInfo)
            continue;
        CTimeInfo* timeInfo = modelInfo->GetTimeInfo();
        if (!timeInfo || modelInfo->m_nKey != key)
            continue;

        m_nOtherTimeModel = static_cast<int16_t>(i);
        return timeInfo;
    }

    return nullptr;
}

// Was inline in the original (0x4073??); moved here - needs CClock.
bool CTimeInfo::IsVisibleNow() const noexcept {
    return CClock::GetIsTimeInRange(GetTimeOn(), GetTimeOff());
}
