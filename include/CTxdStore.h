// CTxdStore - adapted from gta-reversed for clean-room C++ build
// Decompiled bodies: src/CTxdStore/*.c
// TODO: verify each method against decomp.

#pragma once

#include "RenderWare.h"
#include "TxdDef.h"
#include "CPool.h"

#include <array>
#include <cassert>
#include <cstdint>

struct _TxdParent {
    RwTexDictionary* parent;
};

/**
 * Txd Store plugin unique rwID
 */
#define rwID_TXDPARENTPLUGIN  MAKECHUNKID(rwVENDORID_DEVELOPER, 0xF5)

// Clean-room: original was StaticRef-bound to a fixed game address (0xC88018);
// converted to a plain extern. Definition in CTxdStore.cpp.
extern int32 ms_txdPluginOffset;

typedef CPool<TxdDef> CTxdPool;

class CTxdStore {
public:
    struct ScopedTXDSlot {
        ScopedTXDSlot(int32 id) {
            assert(id >= 0);
            CTxdStore::PushCurrentTxd();
            CTxdStore::SetCurrentTxd(static_cast<uint32>(id));
        }

        ScopedTXDSlot(const char* txd) :
            ScopedTXDSlot{ CTxdStore::FindTxdSlot(txd) }
        {
        }

        ~ScopedTXDSlot() {
            CTxdStore::PopCurrentTxd();
        }
    };

    // Clean-room: the originals were StaticRef-bound references to fixed game
    // addresses; converted to plain static data members. Original addresses
    // noted per member. Definitions in CTxdStore.cpp.
    static CTxdPool* ms_pTxdPool;             // was StaticRef<CTxdPool*>(0xC8800C)
    static RwTexDictionary* ms_pStoredTxd;   // was StaticRef<RwTexDictionary*>(0xC88010)
    static int32 ms_lastSlotFound;           // was StaticRef<int32>(0xC88014)
    static std::array<int16, 4> defaultTxds; // was StaticRef<std::array<int16, 4>>(0xC88004)

public:
    static bool PluginAttach();
    static void Initialise();
    static void Shutdown();
    static void GameShutdown();

    static bool StartLoadTxd(int32 index, RwStream* stream);
    static bool FinishLoadTxd(int32 index, RwStream* stream);
    static bool LoadTxd(int32 index, RwStream* stream);
    static bool LoadTxd(int32 index, const char* filename);

    static void PushCurrentTxd();
    static void PopCurrentTxd();
    static void SetCurrentTxd(int32 index);

    static int32 FindTxdSlot(const char* name);
    static int32 FindTxdSlot(uint32 hash);

    static RwTexDictionary* GetTxd(int32 index);
    static int32 GetParentTxdSlot(int32 index);

    static void Create(int32 index);

    static int32 AddTxdSlot(const char* name);
    static void RemoveTxdSlot(int32 index);
    static void RemoveTxd(int32 index);

    static void AddRef(int32 index);
    static void RemoveRef(int32 index);
    static void RemoveRefWithoutDelete(int32 index);
    static int32 GetNumRefs(int32 index);

    static RwTexDictionary* GetTxdParent(RwTexDictionary* txd);
    static void SetTxdParent(RwTexDictionary* txd, RwTexDictionary* parent);
    static void SetupTxdParent(int32 index);

    static RwTexture* TxdStoreFindCB(const char* name);
    static RwTexture* TxdStoreLoadCB(const char* name, const char* mask);

    static auto FindOrAddTxdSlot(const char* name) {
        auto slot = CTxdStore::FindTxdSlot(name);
        if (slot == -1) slot = CTxdStore::AddTxdSlot(name);
        return slot;
    }
    static void SafeRemoveTxdSlot(const char* name) {
        auto slot = CTxdStore::FindTxdSlot(name);
        if (slot != -1) CTxdStore::RemoveTxdSlot(slot);
    }
};

RwTexture* RemoveIfRefCountIsGreaterThanOne(RwTexture* texture, void* data);
