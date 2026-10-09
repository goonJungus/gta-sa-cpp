#pragma once
// CTaskComplex - minimal stub.
// TODO(port): full port (added 2026-10-09 for CPed).
#include "CTask.h"

class CTaskComplex : public CTask {
public:
    // TODO(port): minimal
    CTask* m_pSubTask{nullptr};
    CTaskComplex* m_Parent{nullptr}; // TODO(port): decomp m_Parent
};
