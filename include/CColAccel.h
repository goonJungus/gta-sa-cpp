// CColAccel - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Collision/ColAccel.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.

#pragma once

#include "CVector.h"
#include "ColTypes.h" // CBoundingBox, CSphere, IplDef (minimal stand-ins)

#include <cstdint>

class CEntity;
class CColModel;
struct ColDef; // defined in CColStore.h (streaming subsystem); forward-declared here
               // so this header doesn't pull in CColStore.h's global using-aliases

struct CColAccelColBound {
    CRect   m_Area;
    int16_t m_wModelStart;
    int16_t m_wModelEnd;
    bool    m_bProcedural;
    bool    m_bInterior;
};
// VALIDATE_SIZE(CColAccelColBound, 0x18) stripped

struct CColAccelColEntry {
    CBoundingBox m_boundBox;
    CSphere      m_boundSphere;
    int16_t      m_wModelStart;
    int16_t      m_wModelEnd;
    uint8_t      m_nColSlot;
    bool         m_bColModelNotEmpty;
};
// VALIDATE_SIZE(CColAccelColEntry, 0x30) stripped

struct CColAccelIPLEntry {
    int32_t m_nLodIndex;
    int32_t m_nEntityIndex;
    int32_t m_nModelId;
    int32_t m_nLodModelId;
    union {
        uint32_t m_nFlags;
        struct {
            uint32_t m_bIsUnderwater : 1;
            uint32_t m_bHasColModel : 1;
            uint32_t m_bIsFarDrawDist : 1;
            uint32_t m_bOwnsColModel : 1;
        };
    };
};
// VALIDATE_SIZE(CColAccelIPLEntry, 0x14) stripped

enum eColAccelState : int32_t {
    COLACCEL_ENDED = 0,
    COLACCEL_STARTED = 1,
    COLACCEL_LOADING = 2,
};

// If you ever see a cast like: `(PackedModelStartEnd)modelId`
// That's (in theory) the same as `PackedModelStartEnd{.start = modelId, .end = 0}`, as
// no model has the MSB 16 bits set (as no model's ID is higher than 65535)
union PackedModelStartEnd {
    struct {
        int16_t wModelStart;
        int16_t wModelEnd;
    };
    int32_t modelId;
};
// VALIDATE_SIZE(PackedModelStartEnd, 0x4) stripped

class CColAccel {
public:
    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments. TODO: re-resolve for the clean-room build.
    static CColAccelColBound* m_colBounds;       // 0xBC4090
    static IplDef*           m_iplDefs;          // 0xBC4094
    static int32_t*          m_iSectionSize;     // 0xBC4098
    static int32_t           m_iCachingColSize;  // 0xBC409C
    static eColAccelState    m_iCacheState;      // 0xBC40A0
    static CColAccelColEntry* mp_caccColItems;   // 0xBC40A4
    static int32_t           m_iNumColItems;     // 0xBC40A8
    static CColAccelIPLEntry* mp_caccIPLItems;   // 0xBC40AC
    static int32_t           m_iNumIPLItems;     // 0xBC40B0
    static int32_t           m_iNumSections;     // 0xBC40B4
    static int32_t           m_iNumColBounds;    // 0xBC40B8
    static const char*       mp_cCacheName;

public:
    // InjectHooks() stripped - plugin-sdk hooking mechanism, not needed for clean-room

    static bool   isCacheLoading();
    static void   startCache();
    static void   endCache();
    static void   addCacheCol(PackedModelStartEnd startEnd, const CColModel& colModel);
    static void   cacheLoadCol();
    static void   addColDef(ColDef colDef);
    static void   getColDef(ColDef& colDef);
    static void   setIplDef(int32_t iplIndex, IplDef iplDef);
    static IplDef getIplDef(int32_t iplIndex);
    static void   cacheIPLSection(CEntity** ppEntities, int32_t entitiesCount);
    static void   addIPLEntity(CEntity** ppEntities, int32_t entitiesCount, int32_t entityIndex);
};

// Layout checks: gta-reversed VALIDATE_SIZE values, enforced only on 32-bit
// targets (the original binary is 32-bit; 64-bit dev builds skip them).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CColAccelColBound) == 0x18, "CColAccelColBound layout drift");
static_assert(sizeof(CColAccelColEntry) == 0x30, "CColAccelColEntry layout drift");
static_assert(sizeof(CColAccelIPLEntry) == 0x14, "CColAccelIPLEntry layout drift");
static_assert(sizeof(PackedModelStartEnd) == 0x4, "PackedModelStartEnd layout drift");
#endif
