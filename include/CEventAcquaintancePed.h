#pragma once
// CEventAcquaintancePed - minimal stub from decomp usage in CPed::CPed.
// The real event hierarchy (CEvent base) lands with the events batch.
// TODO(port): full port; verify member layout against the decomp (added 2026-10-09 for CPed).
#include "CEventSoundQuiet.h" // CEvent base (minimal; the events batch replaces it)

#include <cstdint>

class CPed;

class CEventAcquaintancePed : public CEvent {
public:
    virtual ~CEventAcquaintancePed() = default; // TODO(port): stub
    CEventAcquaintancePed() = default;
    explicit CEventAcquaintancePed(CPed* ped) : m_pPed(ped) {} // TODO(port): stub

    bool m_bValid = false;
    uint32_t _9_3_ = 0; // TODO(port): decomp field (offset 9, 3 bytes); real meaning pending
    int32_t m_nTimeActive = 0;
    int32_t m_TaskId = 0;
    CPed* m_pPed = nullptr;
};

// CEventAcquaintancePedHate - the "hate" variant. The decomp builds a
// CEventAcquaintancePed in place and then swaps its vtable to this subclass;
// the port constructs the subclass directly via placement new.
// TODO(port): full port; verify against the decomp.
class CEventAcquaintancePedHate : public CEventAcquaintancePed {
public:
    explicit CEventAcquaintancePedHate(CPed* ped) : CEventAcquaintancePed(ped) {} // TODO(port): stub
};
