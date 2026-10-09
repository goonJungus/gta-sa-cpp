#pragma once
// CTaskSimpleStandStill - minimal stub (decomp size 0x20, derives CTaskSimple).
// TODO(port): full port from decomp / plugin-sdk (added 2026-10-09 for CPed).
#include "CTask.h"

#include <cstdint>

class CTaskSimpleStandStill : public CTask {
public:
    // TODO(port): stub; real ctor is CTaskSimpleStandStill(int, bool, bool, float) per plugin-sdk
    CTaskSimpleStandStill(int32_t nTime, bool bLooped, bool bUseAnimIdleStance, float fBlendData) {
        (void)nTime; (void)bLooped; (void)bUseAnimIdleStance; (void)fBlendData;
    }
};
