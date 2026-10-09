// CTxdStore.cpp - GTA SA 1.0 clean-room C++ conversion
// Stub implementations - fill in logic from the decompiled .c files noted.

#include "CTxdStore.h"

int32 ms_txdPluginOffset{};

CTxdPool* CTxdStore::ms_pTxdPool{};
RwTexDictionary* CTxdStore::ms_pStoredTxd{};
int32 CTxdStore::ms_lastSlotFound{};
std::array<int16, 4> CTxdStore::defaultTxds{};

bool CTxdStore::PluginAttach() {
    // TODO: decomp src/CTxdStore/PluginAttach_00731650.c
    return false;
}

void CTxdStore::Initialise() {
    // TODO: decomp src/CTxdStore/Initialise_00731f20.c
}

void CTxdStore::Shutdown() {
    // TODO: decomp src/CTxdStore/Shutdown_00732000.c
}

void CTxdStore::GameShutdown() {
    // TODO: decomp src/CTxdStore/GameShutdown_00732060.c
}

bool CTxdStore::StartLoadTxd(int32 index, RwStream* stream) {
    // TODO: decomp src/CTxdStore/StartLoadTxd_00731930.c
    (void)index;
    (void)stream;
    return false;
}

bool CTxdStore::FinishLoadTxd(int32 index, RwStream* stream) {
    // TODO: decomp src/CTxdStore/FinishLoadTxd_00731e40.c
    (void)index;
    (void)stream;
    return false;
}

bool CTxdStore::LoadTxd(int32 index, RwStream* stream) {
    // TODO: decomp src/CTxdStore/LoadTxd_00731dd0.c
    (void)index;
    (void)stream;
    return false;
}

bool CTxdStore::LoadTxd(int32 index, const char* filename) {
    // TODO: decomp src/CTxdStore/LoadTxd_007320b0.c
    (void)index;
    (void)filename;
    return false;
}

void CTxdStore::PushCurrentTxd() {
    // TODO: decomp src/CTxdStore/PushCurrentTxd_007316a0.c
}

void CTxdStore::PopCurrentTxd() {
    // TODO: decomp src/CTxdStore/PopCurrentTxd_007316b0.c
}

void CTxdStore::SetCurrentTxd(int32 index) {
    // TODO: decomp src/CTxdStore/SetCurrentTxd_007319c0.c
    (void)index;
}

int32 CTxdStore::FindTxdSlot(const char* name) {
    // TODO: decomp src/CTxdStore/FindTxdSlot_00731850.c
    // NOTE: original returns -1 when not found (see FindOrAddTxdSlot)
    (void)name;
    return -1;
}

int32 CTxdStore::FindTxdSlot(uint32 hash) {
    // TODO: decomp src/CTxdStore/FindTxdSlot_007318e0.c
    // NOTE: original returns -1 when not found
    (void)hash;
    return -1;
}

RwTexDictionary* CTxdStore::GetTxd(int32 index) {
    // TODO: decomp src/CTxdStore/GetTxd_0156c9b0.c
    (void)index;
    return nullptr;
}

int32 CTxdStore::GetParentTxdSlot(int32 index) {
    // TODO: decomp src/CTxdStore/GetParentTxdSlot_015700a0.c
    // NOTE: original returns -1 when there is no parent
    (void)index;
    return -1;
}

void CTxdStore::Create(int32 index) {
    // TODO: decomp src/CTxdStore/Create_00731990.c
    (void)index;
}

int32 CTxdStore::AddTxdSlot(const char* name) {
    // TODO: decomp src/CTxdStore/AddTxdSlot_00731c80.c
    (void)name;
    return 0;
}

void CTxdStore::RemoveTxdSlot(int32 index) {
    // TODO: decomp src/CTxdStore/RemoveTxdSlot_00731cd0.c
    (void)index;
}

void CTxdStore::RemoveTxd(int32 index) {
    // TODO: decomp src/CTxdStore/RemoveTxd_00731e90.c
    (void)index;
}

void CTxdStore::AddRef(int32 index) {
    // TODO: decomp src/CTxdStore/AddRef_00731a00.c
    (void)index;
}

void CTxdStore::RemoveRef(int32 index) {
    // TODO: decomp src/CTxdStore/RemoveRef_00731a30.c
    (void)index;
}

void CTxdStore::RemoveRefWithoutDelete(int32 index) {
    // TODO: decomp src/CTxdStore/RemoveRefWithoutDelete_00731a70.c
    (void)index;
}

int32 CTxdStore::GetNumRefs(int32 index) {
    // TODO: decomp src/CTxdStore/GetNumRefs_00731aa0.c
    (void)index;
    return 0;
}

RwTexDictionary* CTxdStore::GetTxdParent(RwTexDictionary* txd) {
    // TODO: decomp src/CTxdStore/ - no Install entry; likely get_0x8_00731750_00731750.c
    (void)txd;
    return nullptr;
}

void CTxdStore::SetTxdParent(RwTexDictionary* txd, RwTexDictionary* parent) {
    // TODO: decomp src/CTxdStore/ - no Install entry and no matching .c; verify address
    (void)txd;
    (void)parent;
}

void CTxdStore::SetupTxdParent(int32 index) {
    // TODO: decomp src/CTxdStore/SetupTxdParent_00731d50.c
    (void)index;
}

RwTexture* CTxdStore::TxdStoreFindCB(const char* name) {
    // TODO: decomp src/CTxdStore/TxdStoreFindCB_00731720.c
    (void)name;
    return nullptr;
}

RwTexture* CTxdStore::TxdStoreLoadCB(const char* name, const char* mask) {
    // TODO: decomp src/CTxdStore/TxdStoreLoadCB_00731710.c
    (void)name;
    (void)mask;
    return nullptr;
}

RwTexture* RemoveIfRefCountIsGreaterThanOne(RwTexture* texture, void* data) {
    // TODO: decomp src/CTxdStore/ - no matching .c found; verify address
    (void)texture;
    (void)data;
    return nullptr;
}
