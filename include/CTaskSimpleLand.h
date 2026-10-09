#pragma once
// CTaskSimpleLand - minimal stub.
// TODO(port): full task port (added 2026-10-09 for CPed::DoFootLanded).
// Layout from gta-reversed Tasks/TaskTypes/TaskSimpleLand.h.
#include "CTask.h"

class CTaskSimpleLand : public CTask {
public:
    // TODO(port): stubs; signatures from gta-reversed
    bool LeftFootLanded() { return false; }
    bool RightFootLanded() { return false; }
};
