// TxdDef.h - minimal texture-dictionary entry (created 2026-10-09)
// CTxdStore.h does `typedef CPool<TxdDef> CTxdPool`; CPool needs sizeof(T),
// so the struct must be complete here. Members per gta-reversed TxdStore.h;
// verify against the decomp when the streaming subsystem lands.

#pragma once

#include "RenderWare.h" // RwTexDictionary (fwd)

struct TxdDef {
    RwTexDictionary* m_pTxd{};
};
