// Pools.h - minimal stand-in (gta-reversed/source/game_sa/Pools.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the pool accessors used by the currently-converted TUs are defined here.
// The full pool system lands with the core batch. Added 2026-10-09 for CAutomobile.

#pragma once

#include "Common.h"

#include <cstdint>

class CObject;
class CPed;

// Minimal pool interface stand-in.
// TODO(port): real CPool<T> with GetSize/GetAt/SetDealWithNoMemory (gta-reversed Pools.h).
template<typename T>
class CPoolStandIn {
public:
    void SetDealWithNoMemory(bool b) { (void)b; }
    int32_t GetSize() const { return 0; }
    T* GetAt(int32_t index) { (void)index; return nullptr; }
    // TODO(port): real pool handle (gta-reversed CPool::GetRef). Added 2026-10-09 for CPed.
    uint32_t GetRef(T* obj) { (void)obj; return 0; }
    // TODO(port): the decomp pokes CPool internals directly (m_pObjects @ [0],
    //   m_byteMap @ [1], m_nFirstFree @ [3]). These stand in until the real
    //   CPool<T> lands with the core batch. Added 2026-10-09 for CPed.
    int32_t GetIndex(T* obj) { (void)obj; return 0; }
    void Free(T* obj) { (void)obj; }
    // TODO(port): real CPool<T>::New hands out a pool slot; the stand-in heap-allocates.
    //   Added 2026-10-09 for CPed (decomp CPedPool::New()).
    T* New() { return new T; }
};

// TODO(port): real pool globals (gta-reversed Pools.h).
// These are stubs returning empty pools.
inline CPoolStandIn<CObject>* GetObjectPool() {
    static CPoolStandIn<CObject> pool;
    return &pool;
}

inline CPoolStandIn<CPed>* GetPedPool() {
    static CPoolStandIn<CPed> pool;
    return &pool;
}
