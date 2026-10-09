// CStreaming.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations. Decompiled reference: src/CStreaming/*.c
// Bodies cross-checked against gta-reversed/source/game_sa/Streaming.cpp.

#include "CStreaming.h"

#include <algorithm> // std::max, std::fill
#include <cctype>    // std::toupper
#include <cstdio>    // std::sscanf
#include <cstring>   // std::strchr
#include <string_view>

// Ported subsystem headers used by the filled bodies below.
#include "CEntity.h"
#include "CBaseModelInfo.h"
#include "CModelInfo.h"
#include "CTimer.h"
#include "CRenderer.h"
#include "CCamera.h"
#include "CColStore.h"
#include "CFileMgr.h"
// NOTE: CVehicleModelInfo.h is intentionally NOT included here - its eVehicleType
// clashes with CCamera.h's copy (pre-existing tree issue, see BUILD_NOTES).
// Only AssignRemapTxd is needed; declared in the shim block below.
// NOTE: CAnimManager.h is intentionally NOT included here - it pulls in
// AnimTypes.h whose CQuaternion clashes with CMatrix.h's (pre-existing tree
// issue, see BUILD_NOTES; same reason CRideAnimData.h avoids AnimTypes.h).
// The used CAnimManager members are declared in the shim block below.

// ============================================================================
// TODO(port): external subsystem shims.
// Minimal declarations for subsystems not yet ported to cpp/. Each entry is
// verified against gta-reversed/source/game_sa and the decompiled bodies in
// src/CStreaming/*.c. Delete entries as their subsystem lands; do not grow
// this list. None of these introduce link-time dependencies for -fsyntax-only
// or `ar` static-library builds.
// ============================================================================

// --- CdStream: async streaming layer (gta-reversed source/game_sa/CdStreamInfo.h)
void            CdStreamRead(int32 streamId, uint8* buffer, CdStreamPos pos, uint32 size);
void            CdStreamSync(int32 streamId);
eCdStreamStatus CdStreamGetStatus(int32 streamId);
uint32          CdStreamGetLastPosn();

// --- RenderWare: no RW layer in this build yet (see CMakeLists.txt TODO).
// Opaque declarations only; used by ConvertBufferToObject/FinishLoadingLargeFile.
struct RwStream;
struct RwChunkHeaderInfo {
    uint32 type;
    uint32 length;
    uint32 version;
};
struct RtDict;
extern RwStream gRwStream;
RwStream* _rwStreamInitialize(RwStream* stream, int32 param1, int32 param2, int32 param3, void* initData);
void      RwStreamClose(RwStream* stream, void* initData);
void      RwStreamReadChunkHeaderInfo(RwStream* stream, RwChunkHeaderInfo* out);
RtDict*   RtDictSchemaStreamReadDict(void* schema, RwStream* stream);
void      RtDictSchemaSetCurrentDict(void* schema, RtDict* dict);
void      RtDictDestroy(RtDict* dict);
void      RpClumpGtaCancelStream();
extern void* RpUVAnimDictSchema;
// UV-anim-dict chunk id, from decomp @0040c6b0 (chunkHeaderInfo.type == 0x2b)
constexpr uint32 RW_CHUNK_UVANIMDICT = 0x2B;

// --- CTxdStore: include/CTxdStore.h not yet includable (missing RenderWare.h/TxdDef.h)
class CTxdStore {
public:
    static int32 GetParentTxdSlot(int32 index);
    static void  AddRef(int32 index);
    static void  RemoveRef(int32 index);
    static void  RemoveRefWithoutDelete(int32 index);
    static int32 GetNumRefs(int32 index);
    static void  RemoveTxd(int32 index);
    static void  SetCurrentTxd(int32 index);
    static bool  LoadTxd(int32 index, RwStream* stream);
    static bool  StartLoadTxd(int32 index, RwStream* stream);
    static bool  FinishLoadTxd(int32 index, RwStream* stream);
    static int32 FindTxdSlot(const char* name);
    static int32 AddTxdSlot(const char* name);
    static void* GetTxd(int32 index); // RwTexDictionary*; null when not loaded
};

// --- CFileLoader: include/CFileLoader.h not yet includable (missing RenderWare.h/FileMgr.h)
class CFileLoader {
public:
    static bool LoadAtomicFile(RwStream* stream, uint32 modelId);
    static bool LoadClumpFile(RwStream* stream, uint32 modelId);
    static bool FinishLoadClumpFile(RwStream* stream, uint32 modelId);
};

// --- CIplStore: include/CIplStore.h not yet includable (missing IplDef.h/QuadTreeNode.h)
class CIplStore {
public:
    static void LoadIpls(CVector posn, bool bAvoidLoadInPlayerVehicleMovingDirection);
    static void EnsureIplsAreInMemory(const CVector& posn);
    static void AddIplsNeededAtPosn(const CVector& posn);
    static void RemoveIpl(int32 iplSlot);
    static bool LoadIpl(int32 iplSlotIndex, char* data, int32 dataSize);
    static int32 FindIplSlot(const char* name);
    static int32 AddIplSlot(const char* name);
};

// --- CDirectory: not yet ported. Entry layout (0x20 bytes) verified against
// decomp @005b6170: Name[24] at +0x08, Size/SizeInArchive, sector offset.
class CDirectory {
public:
    struct DirectoryInfo {
        uint32 m_nOffset; // sector offset in the IMG (low 24 bits used)
        uint16 m_nSize;   // size in sectors
        uint16 m_nSizeInArchive;
        char   m_szName[24];
    };
    void AddItem(const DirectoryInfo& entry, uint32 imgId);
};

// --- Misc single-purpose subsystems, not yet ported
struct CCutsceneMgr {
    static bool IsCutsceneProcessing();
};
struct CReplay {
    static int32 Mode;
};
constexpr int32 MODE_PLAYBACK = 1; // TODO(port): verify against CReplay port
struct CGame {
    static bool CanSeeOutSideFromCurrArea();
};
class CPathFind {
public:
    bool IsWaterNodeNearby(const CVector& pos, float radius);
    void UnLoadPathFindData(int32 id);
    static void LoadPathFindData(RwStream* stream, int32 id);
};
extern CPathFind ThePaths;
// NOTE: TheCamera is already declared as `extern CCamera& TheCamera` in CCamera.h
struct CPlayerInfo {
    // Stand-in: the real CPlayerInfo (see CWorld.h fwd decl) carries much more;
    // Update() only needs the remote vehicle. Real field type is CVehicle*.
    CEntity* m_pRemoteVehicle;
};
CPlayerInfo& FindPlayerInfo(int32 playerId);
CVector      FindPlayerCoors(int32 playerId = -1); // declared in CWorld.h; CWorld.h not included here
struct CMemoryMgr {
    static void PushMemId(int32 id);
    static void PopMemId();
};
constexpr int32 MEM_STREAMING = 1; // TODO(port): verify eMemoryID values
constexpr int32 MEM_STREAMED_TEXTURES = 2;  // TODO(port): placeholder id
constexpr int32 MEM_STREAMED_COLLISION = 3; // TODO(port): placeholder id
constexpr int32 MEM_STREAMED_ANIMATION = 4; // TODO(port): placeholder id
constexpr int32 MEM_STREAMABLE_SCM = 5;     // TODO(port): placeholder id
// TODO(port): use the real CVehicleModelInfo.h once its eVehicleType clash is fixed
struct CVehicleModelInfoPort {
    static void AssignRemapTxd(const char* name, int16 txdSlot);
};
class CStreamedScripts {
public:
    static int32 RegisterScript(const char* name);
    static void  RemoveStreamedScriptFromMemory(int32 scriptId);
    static void  LoadStreamedScript(RwStream* stream, int32 scriptId);
};

// --- CAnimManager: include/CAnimManager.h not includable here (see note above)
struct CAnimBlockStandIn {
    char Name[16];
    bool IsLoaded;
};
class CAnimManager {
public:
    // Real type: std::array<CAnimBlock, NUM_ANIM_BLOCKS> (see AnimTypes.h);
    // pointer stand-in is enough for indexed IsLoaded reads.
    static CAnimBlockStandIn* ms_aAnimBlocks;
    static void  AddAnimBlockRef(int32 index);
    static void  RemoveAnimBlockRefWithoutDelete(int32 index);
    static void  RemoveAnimBlock(int32 index);
    static int32 RegisterAnimBlock(const char* name);
    static void  LoadAnimFile(RwStream* stream, bool loadCompressed, const char (*uncompressedAnimations)[32]);
    static void  CreateAnimAssocGroups();
};
struct CVehicleRecording {
    static int32 RegisterRecordingFile(const char* name);
    static void  Load(RwStream* stream, int32 recId, int32 dataSize);
};

// eModelID is only minimally ported in CStreaming.h (sentinels only).
// Value below from gta-reversed source/game_sa/Enums/eModelID.h.
constexpr int32 MODEL_MALE01 = 7;

// Static data members (game addresses recorded from gta-reversed StaticRef)
size_t CStreaming::ms_memoryAvailable{}; // game address: 0x8A5A80 - 25'600'000 == 25.6 MB
uint32 CStreaming::desiredNumVehiclesLoaded{}; // game address: 0x8A5A84
bool CStreaming::ms_bLoadVehiclesInLoadScene{}; // game address: 0x8A5A88
std::array<int32, 5> CStreaming::ms_aDefaultCopCarModel{}; // game address: 0x8A5A8C - Last one is bike cop, not matching any level name
std::array<int32, 5> CStreaming::ms_aDefaultCopModel{}; // game address: 0x8A5AA0 - Last one is bike cop, not matching any level name
uint32 CStreaming::ms_nTimePassedSinceLastCopBikeStreamedIn{}; // game address: 0x9654C0
std::array<int32, 4> CStreaming::ms_aDefaultAmbulanceModel{}; // game address: 0x8A5AB4
std::array<int32, 4> CStreaming::ms_aDefaultMedicModel{}; // game address: 0x8A5AC4
std::array<int32, 4> CStreaming::ms_aDefaultFireEngineModel{}; // game address: 0x8A5AD4
std::array<int32, 4> CStreaming::ms_aDefaultFiremanModel{}; // game address: 0x8A5AE4
int32 CStreaming::ms_DefaultCopBikeModel{}; // game address: 0x8A5A9C
int32 CStreaming::ms_DefaultCopBikerModel{}; // game address: 0x8A5AB0
CDirectory* CStreaming::ms_pExtraObjectsDir{}; // game address: 0x8E48D0
tStreamingFileDesc CStreaming::ms_files[TOTAL_IMG_ARCHIVES]{}; // game address: 0x8E48D8
bool CStreaming::ms_bLoadingBigModel{}; // game address: 0x8E4A58
std::array<tStreamingChannel, 2> CStreaming::ms_channel{}; // game address: 0x8E4A60
int32 CStreaming::ms_channelError{}; // game address: 0x8E4B90
bool CStreaming::m_bHarvesterModelsRequested{}; // game address: 0x8E4B9C
bool CStreaming::m_bStreamHarvesterModelsThisFrame{}; // game address: 0x8E4B9D
uint32 CStreaming::ms_numPriorityRequests{}; // game address: 0x8E4BA0
int32 CStreaming::ms_lastCullZone{}; // game address: 0x8E4BA4
uint16 CStreaming::ms_loadedGangCars{}; // game address: 0x8E4BA8
uint16 CStreaming::ms_loadedGangs{}; // game address: 0x8E4BAC
std::array<eModelID, 8> CStreaming::ms_pedsLoaded{}; // game address: 0x8E4C00
uint32 CStreaming::ms_numPedsLoaded{}; // game address: 0x8E4BB0
std::array<int32, 18> CStreaming::ms_NextPedToLoadFromGroup{}; // game address: 0x8E4BB8
int32 CStreaming::ms_currentZoneType{}; // game address: 0x8E4C20
CLoadedCarGroup CStreaming::ms_vehiclesLoaded{}; // game address: 0x8E4C24
CStreamingInfo* CStreaming::ms_pEndRequestedList{}; // game address: 0x8E4C54
CStreamingInfo* CStreaming::ms_pStartRequestedList{}; // game address: 0x8E4C58
CStreamingInfo* CStreaming::ms_pEndLoadedList{}; // game address: 0x8E4C5C
CStreamingInfo* CStreaming::ms_startLoadedList{}; // game address: 0x8E4C60
int32 CStreaming::ms_lastImageRead{}; // game address: 0x8E4C64 - initialized but not used?
std::array<int32, 6> CStreaming::ms_imageOffsets{}; // game address: 0x8E4C8C - initialized but never used?
bool CStreaming::ms_bEnableRequestListPurge{}; // game address: 0x8E4CA4
uint32 CStreaming::ms_streamingBufferSize{}; // game address: 0x8E4CA8
uint8* CStreaming::ms_pStreamingBuffer[2]{}; // game address: 0x8E4CAC
uint32 CStreaming::ms_memoryUsedBytes{}; // game address: 0x8E4CB4
int32 CStreaming::ms_numModelsRequested{}; // game address: 0x8E4CB8
std::array<CStreamingInfo, 26316> CStreaming::ms_aInfoForModel{}; // game address: 0x8E4CC0
bool CStreaming::ms_disableStreaming{}; // game address: 0x9654B0
int32 CStreaming::ms_bIsInitialised{}; // game address: 0x9654B8
bool CStreaming::m_bBoatsNeeded{}; // game address: 0x9654BC
bool CStreaming::ms_bLoadingScene{}; // game address: 0x9654BD
bool CStreaming::m_bCopBikeLoaded{}; // game address: 0x9654BE
bool CStreaming::m_bDisableCopBikes{}; // game address: 0x9654BF
CLinkList<CEntity*> CStreaming::ms_rwObjectInstances{}; // game address: 0x9654F0
CLink<CEntity*>* CStreaming::ms_renderEntityLink{}; // game address: 0x8E48A0
bool CStreaming::m_bLoadingAllRequestedModels{}; // game address: 0x965538
bool CStreaming::m_bModelStreamNotLoaded{}; // game address: 0x9654C4
bool CStreaming::ms_bReadLayerForceFully{}; // game address: 0x9654C4
int32 CStreaming::ms_oldSectorX{}; // game address: 0x8E4B98
int32 CStreaming::ms_oldSectorY{}; // game address: 0x8E4B94

// 0x4087E0
// Request a given model to be loaded.
// Can be called on an already requested model to add the PRIORITY_REQUEST flag.
void CStreaming::RequestModel(int32 modelId, int32 flags) {
    CStreamingInfo& info = GetInfo(modelId);

    switch (info.m_LoadState) {
    case eStreamingLoadState::LOADSTATE_NOT_LOADED:
        break;

    case eStreamingLoadState::LOADSTATE_REQUESTED: {
        // Model already requested, just set priority request flag if not set already
        if ((flags & STREAMING_PRIORITY_REQUEST) && !info.IsPriorityRequest()) {
            ++ms_numPriorityRequests;
            info.SetFlags(STREAMING_PRIORITY_REQUEST);
        }
        break;
    }

    default: {
        flags &= ~STREAMING_PRIORITY_REQUEST; // Remove flag otherwise
        break;
    }
    }
    info.SetFlags(flags);

    // NOTE: if the model was requested once with PRIORITY_REQUEST set and later
    // is requested without it, ms_numPriorityRequests is not decreased (see decomp).

    switch (info.m_LoadState) {
    case eStreamingLoadState::LOADSTATE_LOADED: {
        if (info.InList()) {
            info.RemoveFromList();
            if (IsModelDFF(modelId)) {
                switch (CModelInfo::GetModelInfo(modelId)->GetModelType()) {
                case MODEL_INFO_PED:
                case MODEL_INFO_VEHICLE:
                    return;
                default:
                    break;
                }
            }

            if (!info.IsMissionOrGameRequired())
                info.AddToList(ms_startLoadedList);
        }
        break;
    }
    case eStreamingLoadState::LOADSTATE_READING:
    case eStreamingLoadState::LOADSTATE_REQUESTED:
    case eStreamingLoadState::LOADSTATE_FINISHING:
        break;

    case eStreamingLoadState::LOADSTATE_NOT_LOADED: {
        switch (GetModelType(modelId)) {
        case eModelType::TXD: { // Request parent (if any) TXD
            const int32 parentTxdSlot = CTxdStore::GetParentTxdSlot(ModelIdToTXD(modelId));
            if (parentTxdSlot != -1)
                RequestTxdModel(parentTxdSlot, flags);
            break;
        }
        case eModelType::DFF: { // Request TXD and (if any) IFP
            CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
            RequestTxdModel(modelInfo->m_nTxdIndex, flags);

            const int32 animFileIndex = modelInfo->GetAnimFileIndex();
            if (animFileIndex != -1)
                RequestModel(IFPToModelId(animFileIndex), STREAMING_KEEP_IN_MEMORY);
            break;
        }
        default:
            break;
        }

        info.AddToList(ms_pStartRequestedList);

        ++ms_numModelsRequested;
        if (flags & STREAMING_PRIORITY_REQUEST)
            ++ms_numPriorityRequests;

        info.ClearAllFlags();
        info.SetFlags(flags);
        info.m_LoadState = eStreamingLoadState::LOADSTATE_REQUESTED;
        break;
    }
    }
}

// 0x407100
void CStreaming::RequestTxdModel(int32 slot, int32 flags) {
    RequestModel(TXDToModelId(slot), flags);
}

// There are only 2 streaming channels in CStreaming::ms_channel. In this function,
// if the current channelIndex is zero then "1 - channelIndex" gives the other
// streaming channel, which is 1 (the second streaming channel).
// 0x40EA10
void CStreaming::LoadAllRequestedModels(bool bOnlyPriorityRequests) {
    if (m_bLoadingAllRequestedModels) {
        return;
    }
    m_bLoadingAllRequestedModels = true;

    FlushChannels();

    auto numModelsToLoad = std::max(10, 2 * ms_numModelsRequested);
    int32 chIdx = 0;
    while (true) {
        const tStreamingChannel& ch1 = ms_channel[0];
        const tStreamingChannel& ch2 = ms_channel[1];

        if (IsRequestListEmpty()
            && ch1.IsIdle()
            && ch2.IsIdle()
            || numModelsToLoad <= 0
        ) {
            break;
        }

        if (ms_bLoadingBigModel) {
            chIdx = 0;
        }

        auto& currCh = ms_channel[chIdx];

        if (!currCh.IsIdle()) {
            // Finish loading whatever it was loading
            CdStreamSync(chIdx);
            currCh.loadingLevel = 100;
        }

        if (currCh.IsReading()) {
            ProcessLoadingChannel(chIdx);
            if (currCh.IsStarted())
                ProcessLoadingChannel(chIdx); // Finish loading big model
        }

        if (bOnlyPriorityRequests && ms_numPriorityRequests == 0)
            break;

        if (!ms_bLoadingBigModel) {
            const auto other = 1 - chIdx;
            if (ms_channel[other].IsIdle())
                RequestModelStream(other);

            if (currCh.IsIdle() && !ms_bLoadingBigModel)
                RequestModelStream(chIdx);
        }

        if (ch1.IsIdle() && ch2.IsIdle())
            break;

        chIdx = 1 - chIdx; // Switch to other channel
        --numModelsToLoad;
    }
    FlushChannels();
    m_bLoadingAllRequestedModels = false;
}

// 0x408E20
int32 CStreaming::GetNextFileOnCd(uint32 streamLastPosn, bool bNotPriority) {
    while (true) {
        uint32 nextRequestModelOffset   = UINT32_MAX;
        uint32 firstRequestModelOffset  = UINT32_MAX;
        int32  firstRequestModelId      = MODEL_INVALID;
        int32  nextRequestModelId       = MODEL_INVALID;

        for (auto info = ms_pStartRequestedList->GetNext(); info != ms_pEndRequestedList; info = info->GetNext()) {
            const auto modelId = (int32)GetModelFromInfo(info);
            if (bNotPriority && ms_numPriorityRequests != 0 && !info->IsPriorityRequest())
                continue;

            // Additional conditions for some model types (DFF, TXD, IFP)
            switch (GetModelType(modelId)) {
            case eModelType::DFF: {
                CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);

                // Make sure TXD will be loaded for this model
                const auto txdModel = TXDToModelId(modelInfo->m_nTxdIndex);
                if (!GetInfo(txdModel).IsLoadedOrBeingRead()) {
                    RequestModel(txdModel, GetInfo(modelId).GetFlags()); // Request TXD for this DFF
                    continue;
                }

                // Check if it has an anim (IFP), if so, make sure it gets loaded
                const int32 animFileIndex = modelInfo->GetAnimFileIndex();
                if (animFileIndex != -1) {
                    const int32 animModelId = IFPToModelId(animFileIndex);
                    if (!GetInfo(animModelId).IsLoadedOrBeingRead()) {
                        RequestModel(animModelId, STREAMING_KEEP_IN_MEMORY);
                        continue;
                    }
                }
                break;
            }
            case eModelType::TXD: {
                // Make sure parent (if any) is/will be loaded
                const int32 parentIndex = CTxdStore::GetParentTxdSlot(ModelIdToTXD(modelId));
                if (parentIndex != -1) {
                    const int32 parentModelId = TXDToModelId(parentIndex);
                    if (!GetInfo(parentModelId).IsLoadedOrBeingRead()) {
                        RequestModel(parentModelId, STREAMING_KEEP_IN_MEMORY);
                        continue;
                    }
                }
                break;
            }
            case eModelType::IFP: {
                if (CCutsceneMgr::IsCutsceneProcessing() || !GetInfo(MODEL_MALE01).IsLoaded()) {
                    // Skip in this case
                    continue;
                }
                break;
            }
            default:
                break;
            }

            const auto pos = GetInfo(modelId).GetCdPosn();
            const uint32 offset = pos.ToInt();
            if (offset < firstRequestModelOffset) {
                firstRequestModelOffset = offset;
                firstRequestModelId     = modelId;
            }

            if (offset < nextRequestModelOffset && offset >= streamLastPosn) {
                nextRequestModelOffset = offset;
                nextRequestModelId     = modelId;
            }
        }

        const int32 nextModelId = nextRequestModelId == MODEL_INVALID
            ? firstRequestModelId
            : nextRequestModelId;
        if (nextModelId != MODEL_INVALID || ms_numPriorityRequests == 0) {
            return nextModelId;
        }

        ms_numPriorityRequests = 0;
        bNotPriority = false;
    }
}

// Starts reading at most 16 models at a time.
// Removes all unused (if not IsRequiredToBeKept()) IFP/TXD models as well.
// 0x40CBA0
void CStreaming::RequestModelStream(int32 chIdx) {
    int32 modelId = GetNextFileOnCd(CdStreamGetLastPosn(), true);
    if (modelId == MODEL_INVALID)
        return;

    tStreamingChannel& ch = ms_channel[chIdx];
    CdStreamPos pos{};
    size_t modelSizeSectors = 0;
    CStreamingInfo* streamingInfo = &GetInfo(modelId);

    // Find first model that has to be loaded
    while (!streamingInfo->IsRequiredToBeKept()) {
        // In case of TXD/IFP's check if they're used at all, if not remove them.
        if (IsModelTXD(modelId)) {
            if (AreTexturesUsedByRequestedModels(ModelIdToTXD(modelId)))
                break;
        } else if (IsModelIFP(modelId)) {
            if (AreAnimsUsedByRequestedModels(ModelIdToIFP(modelId)))
                break;
        } else /*model is neither TXD or IFP*/ {
            break; // No checks needed
        }

        // TXD/IFP unused, so remove it, and go on to the next file
        RemoveModel(modelId);

        streamingInfo->GetCdPosnAndSize(pos, modelSizeSectors);         // Grab pos and size of this model
        modelId = GetNextFileOnCd(pos.Offset + modelSizeSectors, true); // Find where the next file is after it
        if (modelId == MODEL_INVALID) {
            return; // No more models...
        }
        streamingInfo = &GetInfo(modelId); // Grab the *next* model's info
    }

    // 0x40CC9A
    if (modelId == MODEL_INVALID)
        return;

    // Grab cd pos and size for this model
    streamingInfo->GetCdPosnAndSize(pos, modelSizeSectors);

    // Check if it's big 0x40CCD5
    if (modelSizeSectors > ms_streamingBufferSize) {
        // A model is considered "big" if it doesn't fit into a single channel's buffer,
        // in which case it has to be loaded entirely by channel 0.
        if (chIdx == 1 || !ms_channel[1].IsIdle())
            return;
        ms_bLoadingBigModel = true;
    }

    // Find all (but at most 16) consecutive models starting at `pos` and load them in one go
    uint32 numSectorsToRead = 0; // The # of sectors to be loaded beginning at `pos`

    bool isPreviousLargeishBigOrVeh = false;
    bool isPreviousModelPed = false;

    // 0x40CD10
    uint32 i = 0;
    for (; i < std::size(ch.modelIds); i++) {
        if (modelId == MODEL_INVALID) {
            break;
        }
        streamingInfo = &GetInfo(modelId);

        if (!streamingInfo->IsRequested())
            break; // Model not requested, so no need to load it.

        if (streamingInfo->GetCdSize())
            modelSizeSectors = (uint32)streamingInfo->GetCdSize();

        const bool isThisModelLargeish = modelSizeSectors > 200;

        if (ms_numPriorityRequests && !streamingInfo->IsPriorityRequest())
            break; // There are priority requests, but this isn't one of them

        CBaseModelInfo* mi = CModelInfo::GetModelInfo(modelId);
        if (IsModelDFF(modelId)) {
            if (isPreviousModelPed && mi->GetModelType() == MODEL_INFO_PED)
                break; // Don't load two peds after each other

            if (isPreviousLargeishBigOrVeh && mi->GetModelType() == MODEL_INFO_VEHICLE)
                break; // Don't load two vehicles / big model + vehicle after each other

            // Check if TXD and/or IFP is loaded for this model.
            // If not we can't load the model yet.

            // Check TXD
            if (!GetInfo(TXDToModelId(mi->m_nTxdIndex)).IsLoadedOrBeingRead())
                break;

            // Check IFP (if any)
            const int32 animFileIndex = mi->GetAnimFileIndex();
            if (animFileIndex != -1) {
                if (!GetInfo(IFPToModelId(animFileIndex)).IsLoadedOrBeingRead())
                    break;
            }
        } else {
            if (IsModelIFP(modelId)) {
                if (CCutsceneMgr::IsCutsceneProcessing() || !GetInfo(MODEL_MALE01).IsLoaded())
                    break;
            } else {
                if (isPreviousLargeishBigOrVeh && isThisModelLargeish)
                    break; // Do not load a big model/car and a big model after each other
            }
        }

        // At this point we've made sure the model can be loaded, so let's add it to the channel.

        // Set offset where the model's data begins at
        ch.modelStreamingBufferOffsets[i] = numSectorsToRead;

        // Set the corresponding modelId
        ch.modelIds[i] = modelId;

        // `i == 0` is a special case:
        // If the 0th model doesn't fit into the buffer it's a `big` one, so
        // `ms_bLoadingBigModel` is set already (before the `for` loop). But we still
        // need to continue to set the appropriate states for the model, thus we can't
        // just `break` (which would also cause the loop below setting modelId slots
        // to -1 to override the modelId).
        if (i > 0) {
            // Check if this model + all the previous fits into one channel's buffer
            if (numSectorsToRead + modelSizeSectors > ms_streamingBufferSize) {
                // No, so stop at the previous model, and ignore this one
                break;
            }
        }
        numSectorsToRead += modelSizeSectors;

        if (IsModelDFF(modelId)) {
            switch (mi->GetModelType()) {
            case MODEL_INFO_PED:
                isPreviousModelPed = true;
                break;
            case MODEL_INFO_VEHICLE:
                isPreviousLargeishBigOrVeh = true; // I guess all vehicles are considered big?
                break;
            default:
                break;
            }
        } else {
            if (isThisModelLargeish)
                isPreviousLargeishBigOrVeh = true;
        }

        // Modify the state of models
        {
            streamingInfo->m_LoadState = eStreamingLoadState::LOADSTATE_READING; // Set as being read
            streamingInfo->RemoveFromList(); // Remove from its current list (that is, the requested list)
            ms_numModelsRequested--;
            if (streamingInfo->IsPriorityRequest()) {
                streamingInfo->ClearFlags(STREAMING_PRIORITY_REQUEST); // Remove priority request flag, as it's not a request anymore
                ms_numPriorityRequests--;
            }
        }

        modelId = streamingInfo->m_NextIndexOnCd; // Continue onto the next one in the directory
    }

    // Set remaining modelId slots to `-1`
    for (auto j = i; j < std::size(ch.modelIds); j++) {
        ch.modelIds[j] = MODEL_INVALID;
    }

    CdStreamRead(chIdx, ms_pStreamingBuffer[chIdx], pos, numSectorsToRead); // Request models to be read
    ch.LoadStatus   = eChannelState::READING;
    ch.loadingLevel = 0;
    ch.sectorCount  = numSectorsToRead; // Set how many sectors to read
    ch.pos          = pos;
    ch.totalTries   = 0;
    if (m_bModelStreamNotLoaded) {
        m_bModelStreamNotLoaded = false;
    }
}

// 0x40E170
bool CStreaming::ProcessLoadingChannel(int32 chIdx) {
    tStreamingChannel& ch = ms_channel[chIdx];

    const eCdStreamStatus streamStatus = CdStreamGetStatus(chIdx);
    switch (streamStatus) {
    case eCdStreamStatus::READING_SUCCESS:
        break;

    case eCdStreamStatus::READING:
    case eCdStreamStatus::WAITING_TO_READ:
        return false; // Not ready yet.

    case eCdStreamStatus::READING_FAILURE: {
        // Retry
        ch.m_nCdStreamStatus = streamStatus;
        ch.LoadStatus = eChannelState::ERR;

        if (ms_channelError != -1)
            return false;

        ms_channelError = chIdx;
        RetryLoadFile(chIdx);

        return true;
    }
    }

    const bool isStarted = ch.IsStarted();
    ch.LoadStatus = eChannelState::IDLE;
    if (isStarted) {
        // It's a large model, so finish loading it
        const auto bufferOffset = ch.modelStreamingBufferOffsets[0];
        auto* pFileContents = &ms_pStreamingBuffer[chIdx][STREAMING_SECTOR_SIZE * bufferOffset];
        FinishLoadingLargeFile(pFileContents, ch.modelIds[0]);
        ch.modelIds[0] = MODEL_INVALID;
    } else {
        // Load models individually
        for (uint32 i = 0u; i < std::size(ch.modelIds); i++) {
            const int32 modelId = ch.modelIds[i];
            if (modelId == MODEL_INVALID)
                continue;

            CBaseModelInfo* baseModelInfo = CModelInfo::GetModelInfo(modelId);
            CStreamingInfo& info = GetInfo(modelId);

            if (!IsModelDFF(modelId)
                || baseModelInfo->GetModelType() != MODEL_INFO_VEHICLE /* It's a DFF, check if it's a vehicle */
                || ms_vehiclesLoaded.CountMembers() < desiredNumVehiclesLoaded /* It's a vehicle, so let's check if we can load more */
                || RemoveLoadedVehicle() /* no, so try to remove one, and load this in its place */
                || info.IsMissionOrGameRequired() /* failed, let's check if it's absolutely mission critical */
            ) {
                if (!IsModelIPL(modelId)) {
                    MakeSpaceFor(info.GetCdSize() * STREAMING_SECTOR_SIZE); // IPLs don't require any memory themselves
                }
                const auto bufferOffsetInSectors = ch.modelStreamingBufferOffsets[i];
                auto* fileBuffer = &ms_pStreamingBuffer[chIdx][STREAMING_SECTOR_SIZE * bufferOffsetInSectors];

                // Actually load the model into memory
                ConvertBufferToObject(fileBuffer, modelId);

                if (info.IsLoadingFinishing()) {
                    ch.LoadStatus = eChannelState::STARTED;
                    ch.modelStreamingBufferOffsets[i] = bufferOffsetInSectors;
                    ch.modelIds[i] = modelId;
                    if (i == 0)
                        continue;
                }
                ch.modelIds[i] = MODEL_INVALID;
            } else {
                // At this point it's guaranteed to be a vehicle (thus a DFF)
                // with STREAMING_MISSION_REQUIRED / STREAMING_GAME_REQUIRED unset.
                const int32 modelTxdIdx = baseModelInfo->m_nTxdIndex;
                RemoveModel(modelId);

                if (info.IsMissionOrGameRequired()) {
                    // Re-request it. (Unreachable in practice: if either flag were set
                    // the outer condition would have been true.)
                    RequestModel(modelId, info.GetFlags());
                } else if (!CTxdStore::GetNumRefs(modelTxdIdx))
                    RemoveTxdModel(modelTxdIdx); // Unload TXD, as it has no refs
            }
        }
    }

    if (ms_bLoadingBigModel) {
        if (!ch.IsStarted()) {
            ms_bLoadingBigModel = false;
            std::fill(ms_channel[1].modelIds.begin(), ms_channel[1].modelIds.end(), MODEL_INVALID);
        }
    }

    return true;
}

// 0x40E460
// Finishes loading all channels (so both channels will be IDLE after it returns).
// Blocking (calls CdStreamSync).
void CStreaming::FlushChannels() {
    // Big model: finish loading it.
    if (ms_channel[1].IsStarted())
        ProcessLoadingChannel(1);

    // Force finish loading channel 0 by using CdStreamSync.
    if (ms_channel[0].IsReading()) {
        CdStreamSync(0);
        ms_channel[0].loadingLevel = 100;
        ProcessLoadingChannel(0);
    }

    // Big model again: finish loading it.
    if (ms_channel[0].IsStarted())
        ProcessLoadingChannel(0);

    // Force finish loading channel 1 by using CdStreamSync.
    if (ms_channel[1].IsReading()) {
        CdStreamSync(1);
        ms_channel[1].loadingLevel = 100;
        ProcessLoadingChannel(1);
    }

    // Big model again: finish loading it.
    if (ms_channel[1].IsStarted())
        ProcessLoadingChannel(1);
}

// 0x40E4E0
// Removes all models found in the `requested` list.
void CStreaming::FlushRequestList() {
    // Have to do it like this, because the current iterator is invalidated when RemoveModel is called
    for (auto it = ms_pStartRequestedList->GetNext(); it != ms_pEndRequestedList;) {
        auto next = it->GetNext();
        RemoveModel((int32)GetModelFromInfo(it));
        it = next;
    }
    FlushChannels();
}

// 0x4076C0
void CStreaming::RetryLoadFile(int32 chIdx) {
    if (ms_channelError == -1)
        return; // CLoadingScreen::Continue() in the original; no loading screen in this build

    // CLoadingScreen::Pause() in the original; empty function
    if (ms_channelError == -1)
        return;

    tStreamingChannel& ch = ms_channel[chIdx];
    while (true) {
        switch (ch.LoadStatus) {
        case eChannelState::READING: {
            if (ProcessLoadingChannel(chIdx)) {
                if (ch.IsStarted())
                    ProcessLoadingChannel(chIdx);

                ms_channelError = -1; // Clear error code
                return; // CLoadingScreen::Continue()
            }
            break;
        }
        case eChannelState::ERR: {
            ch.totalTries++;

            // Keep in mind that CdStreamGetStatus changes the stream status.
            const eCdStreamStatus status = CdStreamGetStatus(chIdx);
            if ((status == eCdStreamStatus::READING || status == eCdStreamStatus::WAITING_TO_READ) &&
                (status != eCdStreamStatus::READING || CdStreamGetStatus(chIdx) != eCdStreamStatus::READING)
            ) {
                break; // Otherwise fallthrough, and do stream read
            }

            [[fallthrough]];
        }
        case eChannelState::IDLE: {
            CdStreamRead(chIdx, ms_pStreamingBuffer[chIdx], ch.pos, ch.sectorCount);
            ch.LoadStatus = eChannelState::READING;
            ch.loadingLevel = -600;
            break;
        }
        default:
            break;
        }

        if (ms_channelError == -1)
            return; // CLoadingScreen::Continue()
    }
}

// 0x40E120
void CStreaming::MakeSpaceFor(size_t memoryToCleanInBytes) {
    while (ms_memoryUsedBytes >= ms_memoryAvailable - memoryToCleanInBytes) {
        if (!RemoveLeastUsedModel(STREAMING_LOADING_SCENE)) {
            DeleteRwObjectsBehindCamera(ms_memoryAvailable - memoryToCleanInBytes);
            return;
        }
    }
}

// 0x40E3A0 (decomp @0040e3a0 via uses_*)
void CStreaming::LoadRequestedModels() {
    static int32 currentChannel = 0; // StaticRef<int32>(0x965534) in the original
    if (ms_bLoadingBigModel)
        currentChannel = 0;

    const tStreamingChannel& channel = ms_channel[currentChannel];
    if (!channel.IsIdle())
        ProcessLoadingChannel(currentChannel);

    if (!ms_bLoadingBigModel) {
        const int32 otherChannelId = 1 - currentChannel;
        if (ms_channel[otherChannelId].IsIdle())
            RequestModelStream(otherChannelId);

        if (channel.IsIdle() && !ms_bLoadingBigModel)
            RequestModelStream(currentChannel);
    }

    if (!channel.IsStarted())
        currentChannel = 1 - currentChannel;
}

// 0x409650
CLink<CEntity*>* CStreaming::AddEntity(CEntity* entity) {
    switch (entity->GetType()) {
    case ENTITY_TYPE_PED:
    case ENTITY_TYPE_VEHICLE:
        return nullptr;
    default:
        break;
    }

    auto link = ms_rwObjectInstances.Insert(entity);
    if (!link) { // No more entries left, try deleting something
        for (auto it = ms_rwObjectInstances.GetTailLink().prev; it != &ms_rwObjectInstances.GetHeadLink(); it = it->prev) {
            const auto e = it->data;
            if (!e->m_bImBeingRendered && !e->m_bStreamingDontDelete) {
                e->DeleteRwObject();
                break;
            }
        }

        // Try inserting again; should succeed now that something was deleted
        link = ms_rwObjectInstances.Insert(entity);
    }
    return link;
}

// 0x409710
void CStreaming::RemoveEntity(CLink<CEntity*>* streamingLink) {
    if (streamingLink) {
        if (streamingLink == ms_renderEntityLink) {
            ms_renderEntityLink = ms_renderEntityLink->prev;
        }
        ms_rwObjectInstances.Remove(streamingLink);
    }
}

// 0x4096D0
void CStreaming::RenderEntity(CLink<CEntity*>* streamingLink) {
    if (streamingLink && streamingLink != ms_renderEntityLink) {
        streamingLink->Remove();
        streamingLink->Insert(ms_renderEntityLink);
        ms_renderEntityLink = streamingLink;
    }
}

// 0x4096C0
void CStreaming::StartRenderEntities() {
    ms_renderEntityLink = &ms_rwObjectInstances.usedListHead;
}

// 0x4089A0
void CStreaming::RemoveModel(int32 modelId) {
    CStreamingInfo& streamingInfo = GetInfo(modelId);
    if (streamingInfo.m_LoadState == eStreamingLoadState::LOADSTATE_NOT_LOADED)
        return;

    if (streamingInfo.IsLoaded()) {
        switch (GetModelType(modelId)) {
        case eModelType::DFF: {
            CBaseModelInfo* modelInfo = CModelInfo::GetModelInfo(modelId);
            modelInfo->DeleteRwObject();
            switch (modelInfo->GetModelType()) {
            case MODEL_INFO_PED: {
                for (auto& mId : ms_pedsLoaded) {
                    if (mId == modelId) {
                        mId = MODEL_INVALID;
                        ms_numPedsLoaded--;
                    }
                }
                break;
            }
            case MODEL_INFO_VEHICLE: {
                RemoveCarModel((eModelID)(modelId));
                break;
            }
            default:
                break;
            }
            break;
        }
        case eModelType::TXD: {
            CTxdStore::RemoveTxd(ModelIdToTXD(modelId));
            break;
        }
        case eModelType::COL: {
            CColStore::RemoveCol(ModelIdToCOL(modelId));
            break;
        }
        case eModelType::IPL: {
            CIplStore::RemoveIpl(ModelIdToIPL(modelId));
            break;
        }
        case eModelType::DAT: {
            ThePaths.UnLoadPathFindData(ModelIdToDAT(modelId));
            break;
        }
        case eModelType::IFP: {
            CAnimManager::RemoveAnimBlock(ModelIdToIFP(modelId));
            break;
        }
        case eModelType::SCM: {
            CStreamedScripts::RemoveStreamedScriptFromMemory(ModelIdToSCM(modelId));
            break;
        }
        default:
            break;
        }
        ms_memoryUsedBytes -= STREAMING_SECTOR_SIZE * (uint32)streamingInfo.GetCdSize();
    }

    if (streamingInfo.InList()) {
        if (streamingInfo.IsRequested()) {
            ms_numModelsRequested--;
            if (streamingInfo.IsPriorityRequest()) {
                streamingInfo.ClearFlags(STREAMING_PRIORITY_REQUEST);
                ms_numPriorityRequests--;
            }
        }
        streamingInfo.RemoveFromList();
    } else if (streamingInfo.IsBeingRead()) {
        for (auto& ch : ms_channel) {
            std::replace_if(ch.modelIds.begin(), ch.modelIds.end(),
                [&](const int32 mId) { return mId == modelId; }, MODEL_INVALID);
        }
    }

    if (streamingInfo.IsLoadingFinishing()) {
        switch (GetModelType(modelId)) {
        case eModelType::DFF:
            RpClumpGtaCancelStream();
            break;
        case eModelType::TXD:
            CTxdStore::RemoveTxd(ModelIdToTXD(modelId));
            break;
        case eModelType::COL:
            CColStore::RemoveCol(ModelIdToCOL(modelId));
            break;
        case eModelType::IPL:
            CIplStore::RemoveIpl(ModelIdToIPL(modelId));
            break;
        case eModelType::IFP:
            CAnimManager::RemoveAnimBlock(ModelIdToIFP(modelId));
            break;
        case eModelType::SCM:
            CStreamedScripts::RemoveStreamedScriptFromMemory(ModelIdToSCM(modelId));
            break;
        default:
            break;
        }
    }

    streamingInfo.m_LoadState = eStreamingLoadState::LOADSTATE_NOT_LOADED;
}

// 0x40C180
void CStreaming::RemoveTxdModel(int32 modelId) {
    RemoveModel(TXDToModelId(modelId));
}

// 0x40A080
void CStreaming::RequestFile(int32 modelId, CdStreamPos posn, uint32 size, StreamingImgID imgId, int32 streamingFlags) {
    CStreamingInfo& info = GetInfo(modelId);

    // Decomp @015663b0: if the file is already registered at the same
    // position/size there is nothing to do (LOCK/UNLOCK are no-op critical
    // sections in the original). NOTE: gta-reversed re-requests the model in
    // this case; the decomp returns without doing so - decomp followed here.
    const uint32 wantedPos = (uint32(imgId) << 24) | (posn.Offset & 0xFFFFFFu);
    const uint32 curPos = ms_files[info.m_ImgID].StreamHandle + info.m_CDOffset;
    if (info.GetCdSize() != 0 && curPos == wantedPos && info.GetCdSize() == size) {
        return;
    }

    RemoveModel(modelId);
    info.SetCdPosnAndSize(posn.Offset, size);
    info.SetImg(imgId);
    RequestModel(modelId, streamingFlags);
}

// 0x409050
// Goes through the given channel and removes then re-requests each model in it.
void CStreaming::RequestFilesInChannel(int32 chIdx) {
    for (const auto& modelId : ms_channel[chIdx].modelIds) {
        if (modelId != MODEL_INVALID) {
            const int32 flags = GetInfo(modelId).GetFlags();
            RemoveModel(modelId);
            RequestModel(modelId, flags);
        }
    }
}

// 0x40C6B0
bool CStreaming::ConvertBufferToObject(uint8* fileBuffer, int32 modelId) {
    CStreamingInfo* pStartLoadedListStreamingInfo = ms_startLoadedList;
    CBaseModelInfo* mi = CModelInfo::GetModelInfo(modelId);
    CStreamingInfo& streamingInfo = GetInfo(modelId);

    const auto bufferSize = (uint32)streamingInfo.GetCdSize() * STREAMING_SECTOR_SIZE;
    tRwStreamInitializeData rwStreamInitData = { fileBuffer, bufferSize };

    // Make RW stream from memory
    // TODO(port): RenderWare layer - stream init/close below are opaque shims
    RwStream* stream = _rwStreamInitialize(&gRwStream, 0, 3 /*rwSTREAMMEMORY*/, 1 /*rwSTREAMREAD*/, &rwStreamInitData);

    switch (GetModelType(modelId)) {
    case eModelType::DFF: {
        // Check if TXD and IFP are loaded
        const int32 animFileIndex = mi->GetAnimFileIndex();
        const int32 nTXDIdx = mi->m_nTxdIndex;
        const bool txdLoaded = CTxdStore::GetTxd(nTXDIdx) != nullptr;
        const bool animLoaded = animFileIndex == -1 || CAnimManager::ms_aAnimBlocks[animFileIndex].IsLoaded;
        if (!txdLoaded || !animLoaded) {
            // TXD or IFP not loaded, re-request model (shouldn't normally happen)
            RemoveModel(modelId);
            RequestModel(modelId, streamingInfo.GetFlags());
            RwStreamClose(stream, &rwStreamInitData);
            return false;
        }

        CTxdStore::AddRef(nTXDIdx);
        if (animFileIndex != -1) {
            CAnimManager::AddAnimBlockRef(animFileIndex);
        }

        CTxdStore::SetCurrentTxd(mi->m_nTxdIndex);
        bool bFileLoaded = false;
        if (mi->GetRwModelType() == rpATOMIC) {
            RwChunkHeaderInfo chunkHeaderInfo{};
            RwStreamReadChunkHeaderInfo(stream, &chunkHeaderInfo);

            // Read UV Anim dict (if any)
            RtDict* pRtDictionary = nullptr;
            if (chunkHeaderInfo.type == RW_CHUNK_UVANIMDICT) {
                pRtDictionary = RtDictSchemaStreamReadDict(&RpUVAnimDictSchema, stream);
                RtDictSchemaSetCurrentDict(&RpUVAnimDictSchema, pRtDictionary);
            }

            RwStreamClose(stream, &rwStreamInitData);

            RwStream* stream2 = _rwStreamInitialize(&gRwStream, 0, 3 /*rwSTREAMMEMORY*/, 1 /*rwSTREAMREAD*/, &rwStreamInitData);

            bFileLoaded = CFileLoader::LoadAtomicFile(stream2, modelId);
            if (pRtDictionary) {
                RtDictDestroy(pRtDictionary);
            }
        } else {
            bFileLoaded = CFileLoader::LoadClumpFile(stream, modelId);
        }

        if (!streamingInfo.IsLoadingFinishing()) {
            CTxdStore::RemoveRefWithoutDelete(mi->m_nTxdIndex);
            if (animFileIndex != -1) {
                CAnimManager::RemoveAnimBlockRefWithoutDelete(animFileIndex);
            }

            if (bFileLoaded && mi->GetModelType() == MODEL_INFO_VEHICLE) {
                if (!AddToLoadedVehiclesList(modelId)) {
                    RemoveModel(modelId);
                    RequestModel(modelId, streamingInfo.GetFlags());
                    RwStreamClose(stream, &rwStreamInitData);
                    return false;
                }
            }
        }

        if (!bFileLoaded) {
            RemoveModel(modelId);
            RequestModel(modelId, streamingInfo.GetFlags());
            RwStreamClose(stream, &rwStreamInitData);
            return false;
        }

        break;
    }
    case eModelType::TXD: {
        const int32 modelTxdIndex = ModelIdToTXD(modelId);
        const int32 parentTXDIdx = CTxdStore::GetParentTxdSlot(modelTxdIndex);
        if (parentTXDIdx != -1 && !CTxdStore::GetTxd(parentTXDIdx)) {
            // Parent not loaded, re-request
            RemoveModel(modelId);
            RequestModel(modelId, streamingInfo.GetFlags());
            RwStreamClose(stream, &rwStreamInitData);
            return false;
        }

        if (!streamingInfo.IsRequiredToBeKept() && !AreTexturesUsedByRequestedModels(modelTxdIndex)) {
            // Model not needed anymore, unload
            RemoveModel(modelId);
            RwStreamClose(stream, &rwStreamInitData);
            return false;
        }

        CMemoryMgr::PushMemId(MEM_STREAMED_TEXTURES);
        bool bTxdLoaded = false;
        if (ms_bLoadingBigModel) {
            bTxdLoaded = CTxdStore::StartLoadTxd(modelTxdIndex, stream);
            if (bTxdLoaded)
                streamingInfo.m_LoadState = eStreamingLoadState::LOADSTATE_FINISHING;
        } else {
            bTxdLoaded = CTxdStore::LoadTxd(modelTxdIndex, stream);
        }
        CMemoryMgr::PopMemId();
        UpdateMemoryUsed();

        if (!bTxdLoaded) {
            RemoveModel(modelId);
            RequestModel(modelId, streamingInfo.GetFlags());
            RwStreamClose(stream, &rwStreamInitData);
            return false;
        }

        break;
    }
    case eModelType::COL: {
        CMemoryMgr::PushMemId(MEM_STREAMED_COLLISION);
        const auto success = CColStore::LoadCol(ModelIdToCOL(modelId), fileBuffer, bufferSize);
        CMemoryMgr::PopMemId();
        if (!success) {
            RemoveModel(modelId);
            RequestModel(modelId, streamingInfo.GetFlags());
            RwStreamClose(stream, &rwStreamInitData);
            return false;
        }
        break;
    }
    case eModelType::IPL: {
        CMemoryMgr::PushMemId(MEM_STREAMED_COLLISION); // sic: original uses the collision heap here too
        const auto success = CIplStore::LoadIpl(ModelIdToIPL(modelId), (char*)fileBuffer, bufferSize);
        CMemoryMgr::PopMemId();
        if (!success) {
            RemoveModel(modelId);
            RequestModel(modelId, streamingInfo.GetFlags());
            RwStreamClose(stream, &rwStreamInitData);
            return false;
        }
        break;
    }
    case eModelType::DAT: {
        CMemoryMgr::PushMemId(MEM_STREAMED_COLLISION); // sic: original uses the collision heap here too
        CPathFind::LoadPathFindData(stream, ModelIdToDAT(modelId));
        CMemoryMgr::PopMemId();
        break;
    }
    case eModelType::IFP: {
        if (!streamingInfo.IsRequiredToBeKept() && !AreAnimsUsedByRequestedModels(ModelIdToIFP(modelId))) {
            // Not required anymore, unload
            RemoveModel(modelId);
            RwStreamClose(stream, &rwStreamInitData);
            return false;
        }

        // Still required, load
        CMemoryMgr::PushMemId(MEM_STREAMED_ANIMATION);
        CAnimManager::LoadAnimFile(stream, true, nullptr);
        CAnimManager::CreateAnimAssocGroups();
        CMemoryMgr::PopMemId();

        break;
    }
    case eModelType::RRR: {
        CMemoryMgr::PushMemId(MEM_STREAMED_ANIMATION);
        CVehicleRecording::Load(stream, ModelIdToRRR(modelId), bufferSize);
        CMemoryMgr::PopMemId();
        break;
    }
    case eModelType::SCM: {
        CMemoryMgr::PushMemId(MEM_STREAMABLE_SCM);
        CStreamedScripts::LoadStreamedScript(stream, ModelIdToSCM(modelId));
        CMemoryMgr::PopMemId();
        break;
    }
    default:
        assert(0);
        break;
    }

    RwStreamClose(stream, &rwStreamInitData);

    switch (GetModelType(modelId)) {
    case eModelType::SCM:
    case eModelType::IFP:
    case eModelType::TXD: {
        if (!streamingInfo.IsMissionOrGameRequired())
            streamingInfo.AddToList(pStartLoadedListStreamingInfo);
        break;
    }
    case eModelType::DFF: {
        // Model is a DFF
        switch (mi->GetModelType()) {
        case MODEL_INFO_VEHICLE:
        case MODEL_INFO_PED:
            break;
        default: {
            // m_nAlpha lives on CBaseModelInfo; AsAtomicModelInfoPtr() is only
            // used as the "is atomic" test here (avoids needing the complete
            // CAtomicModelInfo type in this TU).
            if (mi->AsAtomicModelInfoPtr()) {
                // 0x40CAFA: -(flags & (LOADING_SCENE | MISSION_REQUIRED)) != 0
                mi->m_nAlpha = (uint8)-((streamingInfo.GetFlags() & (STREAMING_LOADING_SCENE | STREAMING_MISSION_REQUIRED)) != 0);
            }

            if (!streamingInfo.IsMissionOrGameRequired())
                streamingInfo.AddToList(pStartLoadedListStreamingInfo);

            break;
        }
        }
        break;
    }
    default:
        break;
    }

    if (!streamingInfo.IsLoadingFinishing()) {
        streamingInfo.m_LoadState = eStreamingLoadState::LOADSTATE_LOADED;
        ms_memoryUsedBytes += bufferSize;
    }
    return true;
}

// 0x408CB0
// Finishes loading a big model by loading the second half of the file
// residing at `pFileBuffer`.
void CStreaming::FinishLoadingLargeFile(uint8* pFileBuffer, int32 modelId) {
    CBaseModelInfo* baseModelInfo = CModelInfo::GetModelInfo(modelId);
    CStreamingInfo& streamingInfo = GetInfo(modelId);
    if (streamingInfo.IsLoadingFinishing() /*first half loaded?*/) {
        const uint32 bufferSize = (uint32)streamingInfo.GetCdSize() * STREAMING_SECTOR_SIZE;
        const tRwStreamInitializeData rwStreamInitializationData = { pFileBuffer, bufferSize };

        // TODO(port): RenderWare layer - stream init/close below are opaque shims
        RwStream* pRwStream = _rwStreamInitialize(&gRwStream, 0, 3 /*rwSTREAMMEMORY*/, 1 /*rwSTREAMREAD*/,
            (void*)&rwStreamInitializationData);

        bool bLoaded = false;
        switch (GetModelType(modelId)) {
        case eModelType::DFF: {
            CTxdStore::SetCurrentTxd(baseModelInfo->m_nTxdIndex);

            bLoaded = CFileLoader::FinishLoadClumpFile(pRwStream, modelId);
            if (bLoaded)
                bLoaded = AddToLoadedVehiclesList(modelId);
            baseModelInfo->RemoveRef();

            CTxdStore::RemoveRefWithoutDelete(baseModelInfo->m_nTxdIndex);

            const int32 animFileIndex = baseModelInfo->GetAnimFileIndex();
            if (animFileIndex != -1) {
                CAnimManager::RemoveAnimBlockRefWithoutDelete(animFileIndex);
            }
            break;
        }
        case eModelType::TXD: {
            CTxdStore::AddRef(ModelIdToTXD(modelId));
            bLoaded = CTxdStore::FinishLoadTxd(ModelIdToTXD(modelId), pRwStream);
            CTxdStore::RemoveRefWithoutDelete(ModelIdToTXD(modelId));
            break;
        }
        default: {
            assert(modelId < RESOURCE_ID_COL && "FinishLoadingLargeFile: model id is out of range");
            break;
        }
        }
        RwStreamClose(pRwStream, pFileBuffer);

        streamingInfo.m_LoadState = eStreamingLoadState::LOADSTATE_LOADED;
        ms_memoryUsedBytes += bufferSize;
        if (!bLoaded) {
            RemoveModel(modelId);
            RequestModel(modelId, streamingInfo.GetFlags());
        }
    } else {
        if (IsModelDFF(modelId)) {
            baseModelInfo->RemoveRef();
        }
    }
}

// 0x40E670
void CStreaming::Update() {
    if (CTimer::GetIsPaused())
        return;

    const auto& camPos = TheCamera.GetPosition();
    const float fCamDistanceToGroundZ = camPos.z - TheCamera.CalculateGroundHeight(eGroundHeightType::ENTITY_BB_BOTTOM);
    if (!ms_disableStreaming && !CRenderer::m_loadingPriority) {
        if (fCamDistanceToGroundZ >= 50.0f) {
            if (CGame::CanSeeOutSideFromCurrArea()) {
                AddLodsToRequestList(camPos, 0);
            }
        } else if (CRenderer::ms_bRenderOutsideTunnels) {
            AddModelsToRequestList(camPos, 0);
        }
    }

    if (CTimer::GetFrameCounter() % 128 == 106) {
        m_bBoatsNeeded = false;
        if (camPos.z < 500.0f) {
            m_bBoatsNeeded = ThePaths.IsWaterNodeNearby(camPos, 80.0f);
        }
    }

    const CVector playerPos = FindPlayerCoors();
    if (!ms_disableStreaming
        && !CCutsceneMgr::IsCutsceneProcessing()
        && CGame::CanSeeOutSideFromCurrArea()
        && CReplay::Mode != MODE_PLAYBACK
        && fCamDistanceToGroundZ < 50.0f
    ) {
        StreamVehiclesAndPeds_Always(playerPos);
        if (!IsVeryBusy()) {
            StreamVehiclesAndPeds();
            StreamZoneModels(playerPos);
        }
    }
    LoadRequestedModels();

    if (CEntity* remoteVehicle = FindPlayerInfo(0).m_pRemoteVehicle) {
        CColStore::AddCollisionNeededAtPosn(playerPos);
        CIplStore::AddIplsNeededAtPosn(playerPos);

        const CVector& remoteVehiclePos = remoteVehicle->GetPosition();
        CColStore::LoadCollision(remoteVehiclePos, false);
        CColStore::EnsureCollisionIsInMemory(remoteVehiclePos);
        CIplStore::LoadIpls(remoteVehiclePos, false);
        CIplStore::EnsureIplsAreInMemory(remoteVehiclePos);
    } else {
        CColStore::LoadCollision(playerPos, false);
        CColStore::EnsureCollisionIsInMemory(playerPos);
        CIplStore::LoadIpls(playerPos, false);
        CIplStore::EnsureIplsAreInMemory(playerPos);
    }

    if (ms_bEnableRequestListPurge) {
        PurgeRequestList();
    }
}

// 0x4083C0
void CStreaming::InitImageList() {
    std::fill(std::begin(ms_files), std::end(ms_files), tStreamingFileDesc{});
    AddImageToList("MODELS\\GTA3.IMG", true);
    AddImageToList("MODELS\\GTA_INT.IMG", true);
}

namespace {
// Case-insensitive 3-letter extension match (decomp @005b6170 uses _strnicmp)
bool ExtIs(const char* ext, const char* expected) {
    for (int i = 0; i < 3; i++) {
        if (std::toupper((unsigned char)ext[i]) != (unsigned char)expected[i])
            return false;
    }
    return true;
}
} // namespace

// 0x5B6170
// Load a directory (aka img file).
// Sets the CdSize, CdPosn, m_ImgID, m_NextIndexOnCd of each model present in
// the file (according to file name).
void CStreaming::LoadCdDirectory(const char* filename, StreamingImgID img) {
    auto* imgFile = CFileMgr::OpenFile(filename, "rb");
    if (!imgFile)
        return;

    // Read and check IMG file version ("VER2" for SA)
    {
        char version[4];
        CFileMgr::Read(imgFile, &version, 4u);
        assert((std::string_view{ version, 4u } == "VER2"));
    }

    int32 previousModelId = MODEL_INVALID;
    int32 entryCount;
    CFileMgr::Read(imgFile, &entryCount, sizeof(int32));
    for (int32 i = 0; i < entryCount; i++) {
        CDirectory::DirectoryInfo entry{};
        CFileMgr::Read(imgFile, &entry, sizeof(CDirectory::DirectoryInfo));

        // Maybe increase buffer size
        ms_streamingBufferSize = std::max(ms_streamingBufferSize, (uint32)entry.m_nSize);

        // Find extension from name
        constexpr auto nameSize = sizeof(CDirectory::DirectoryInfo::m_szName);
        entry.m_szName[nameSize - 1] = 0;
        char* extension = std::strchr(entry.m_szName, '.');
        if (!extension || (size_t)(extension - entry.m_szName) > nameSize - 4u) {
            entry.m_szName[nameSize - 1] = 0;
            previousModelId = MODEL_INVALID;
            continue;
        }

        *extension = 0; // Cut the extension off; Name now holds the bare file name

        int32 modelId = MODEL_INVALID;
        if (ExtIs(extension + 1, "DFF")) {
            if (!CModelInfo::GetModelInfo(entry.m_szName, &modelId)) {
                entry.m_nOffset |= (uint32(img) << 24);
                ms_pExtraObjectsDir->AddItem(entry, img);
                previousModelId = MODEL_INVALID;
                continue;
            }
        } else if (ExtIs(extension + 1, "TXD")) {
            int32 txdSlot = CTxdStore::FindTxdSlot(entry.m_szName);
            if (txdSlot == -1) {
                txdSlot = CTxdStore::AddTxdSlot(entry.m_szName);
                CVehicleModelInfoPort::AssignRemapTxd(entry.m_szName, (int16)txdSlot);
            }
            modelId = TXDToModelId(txdSlot);
        } else if (ExtIs(extension + 1, "COL")) {
            int32 colSlot = CColStore::FindColSlot(entry.m_szName);
            if (colSlot == -1)
                colSlot = CColStore::AddColSlot(entry.m_szName);
            modelId = COLToModelId(colSlot);
        } else if (ExtIs(extension + 1, "IPL")) {
            int32 iplSlot = CIplStore::FindIplSlot(entry.m_szName);
            if (iplSlot == -1)
                iplSlot = CIplStore::AddIplSlot(entry.m_szName);
            modelId = IPLToModelId(iplSlot);
        } else if (ExtIs(extension + 1, "DAT")) {
            // Extract nodes file sector from name ("nodesXX.dat", XX is the id)
            if (std::sscanf(&entry.m_szName[sizeof("nodes") - 1], "%d", &modelId) != 1) {
                previousModelId = MODEL_INVALID;
                continue;
            }
            modelId += RESOURCE_ID_DAT;
        } else if (ExtIs(extension + 1, "IFP")) {
            modelId = IFPToModelId(CAnimManager::RegisterAnimBlock(entry.m_szName));
        } else if (ExtIs(extension + 1, "RRR")) {
            modelId = RRRToModelId(CVehicleRecording::RegisterRecordingFile(entry.m_szName));
        } else if (ExtIs(extension + 1, "SCM")) {
            modelId = SCMToModelId(CStreamedScripts::RegisterScript(entry.m_szName));
        } else {
            *extension = '.'; // Put the '.' back
            previousModelId = MODEL_INVALID;
            continue;
        }

        CStreamingInfo& info = GetInfo(modelId);
        if (!info.HasCdPosnAndSize()) {
            if (entry.m_nSizeInArchive)
                entry.m_nSize = entry.m_nSizeInArchive;

            info.SetCdPosnAndSize(entry.m_nOffset, entry.m_nSize);
            info.SetImg(img);
            info.ClearAllFlags();

            if (previousModelId != MODEL_INVALID) {
                assert(modelId <= INT16_MAX);
                GetInfo(previousModelId).m_NextIndexOnCd = (int16)modelId;
            }

            previousModelId = modelId;
        } else {
            previousModelId = MODEL_INVALID;
        }
    }
    CFileMgr::CloseFile(imgFile);
}

// 0x5B82C0
void CStreaming::LoadCdDirectory() {
    // ms_imageOffsets / ms_lastImageRead are initialized but never used
    ms_imageOffsets[0] = 0;
    ms_imageOffsets[1] = -1;
    ms_imageOffsets[2] = -1;
    ms_imageOffsets[3] = -1;
    ms_imageOffsets[4] = -1;
    ms_imageOffsets[5] = -1;

    // Load the directory of every in-use archive
    int32 archiveId = 0;
    for (auto& file : ms_files) {
        if (file.IsInUse() && file.IsNotPlayerImg) {
            LoadCdDirectory(file.Name, (StreamingImgID)archiveId);
        }
        archiveId++;
    }

    ms_lastImageRead = 0;
}

void CStreaming::UpdateMemoryUsed() {
#ifdef MEMORY_MGR_USE_MEMORY_HEAP
    // TODO(port): CMemoryMgr heaps not yet ported; ms_memoryUsedBytes is
    // maintained manually by the streaming code in this build.
#endif
}

// 0x407BF0
void CStreaming::IHaveUsedStreamingMemory() {
    CMemoryMgr::PopMemId();
    UpdateMemoryUsed();
}

// 0x407BE0
void CStreaming::ImGonnaUseStreamingMemory() {
    CMemoryMgr::PushMemId(MEM_STREAMING);
}

// ---------------------------------------------------------------------------
// Remaining methods: TODO stubs (not yet filled - see decomp src/CStreaming/*.c)
// ---------------------------------------------------------------------------
uint32 CStreaming::AddImageToList(const char* fileName, bool bNotPlayerImg) {
    // TODO: decomp src/CStreaming/AddImageToList_*.c
    (void)fileName;
    (void)bNotPlayerImg;
    return 0;
}
void CStreaming::AddLodsToRequestList(const CVector& point, int32 flags) {
    // TODO: decomp src/CStreaming/AddLodsToRequestList_*.c
    (void)point;
    (void)flags;
}
void CStreaming::AddModelsToRequestList(const CVector& point, int32 flags) {
    // TODO: decomp src/CStreaming/AddModelsToRequestList_*.c
    (void)point;
    (void)flags;
}
bool CStreaming::AddToLoadedVehiclesList(int32 modelId) {
    // TODO: decomp src/CStreaming/AddToLoadedVehiclesList_*.c
    (void)modelId;
    return false;
}
bool CStreaming::AreAnimsUsedByRequestedModels(int32 animModelId) {
    // TODO: decomp src/CStreaming/AreAnimsUsedByRequestedModels_*.c
    (void)animModelId;
    return false;
}
bool CStreaming::AreTexturesUsedByRequestedModels(int32 txdModelId) {
    // TODO: decomp src/CStreaming/AreTexturesUsedByRequestedModels_*.c
    (void)txdModelId;
    return false;
}
void CStreaming::ClearFlagForAll(uint32 streamingFlag) {
    // TODO: decomp src/CStreaming/ClearFlagForAll_*.c
    (void)streamingFlag;
}
void CStreaming::ClearSlots(uint32 totalSlots) {
    // TODO: decomp src/CStreaming/ClearSlots_*.c
    (void)totalSlots;
}
void CStreaming::DeleteAllRwObjects() {
    // TODO: decomp src/CStreaming/DeleteAllRwObjects_*.c
}
bool CStreaming::DeleteLeastUsedEntityRwObject(bool bNotOnScreen, int32 flags) {
    // TODO: decomp src/CStreaming/DeleteLeastUsedEntityRwObject_*.c
    (void)bNotOnScreen;
    (void)flags;
    return false;
}
void CStreaming::DeleteRwObjectsAfterDeath(const CVector& point) {
    // TODO: decomp src/CStreaming/DeleteRwObjectsAfterDeath_*.c
    (void)point;
}
void CStreaming::DeleteRwObjectsBehindCamera(size_t memoryToCleanInBytes) {
    // TODO: decomp src/CStreaming/DeleteRwObjectsBehindCamera_*.c
    (void)memoryToCleanInBytes;
}
bool CStreaming::RemoveReferencedTxds(size_t goalMemoryUsageBytes) {
    // TODO: decomp src/CStreaming/RemoveReferencedTxds_*.c
    (void)goalMemoryUsageBytes;
    return false;
}
void CStreaming::DisableCopBikes(bool bDisable) {
    // TODO: decomp src/CStreaming/DisableCopBikes_*.c
    (void)bDisable;
}
int32 CStreaming::FindMIPedSlotForInterior(int32 randFactor) {
    // TODO: decomp src/CStreaming/FindMIPedSlotForInterior_*.c
    (void)randFactor;
    return 0;
}
void CStreaming::ForceLayerToRead(int32 arg1) {
    // TODO: decomp src/CStreaming/ForceLayerToRead_*.c
    (void)arg1;
}
int32 CStreaming::GetDefaultCabDriverModel() {
    // TODO: decomp src/CStreaming/GetDefaultCabDriverModel_*.c
    return 0;
}
eModelID CStreaming::GetDefaultCopCarModel(bool ignoreLvpd1Model) {
    // TODO: decomp src/CStreaming/GetDefaultCopCarModel_*.c
    (void)ignoreLvpd1Model;
    return static_cast<eModelID>(-1) /* TODO: MODEL_INVALID */;
}
eModelID CStreaming::GetDefaultCopModel() {
    // TODO: decomp src/CStreaming/GetDefaultCopModel_*.c
    return static_cast<eModelID>(-1) /* TODO: MODEL_INVALID */;
}
eModelID CStreaming::GetDefaultFiremanModel() {
    // TODO: decomp src/CStreaming/GetDefaultFiremanModel_*.c
    return static_cast<eModelID>(-1) /* TODO: MODEL_INVALID */;
}
eModelID CStreaming::GetDefaultMedicModel() {
    // TODO: decomp src/CStreaming/GetDefaultMedicModel_*.c
    return static_cast<eModelID>(-1) /* TODO: MODEL_INVALID */;
}
int32 CStreaming::GetDiscInDrive() {
    // TODO: decomp src/CStreaming/GetDiscInDrive_*.c
    return 0;
}
bool CStreaming::HasSpecialCharLoaded(int32 slot) {
    // TODO: decomp src/CStreaming/HasSpecialCharLoaded_*.c
    (void)slot;
    return false;
}
bool CStreaming::HasVehicleUpgradeLoaded(int32 modelId) {
    // TODO: decomp src/CStreaming/HasVehicleUpgradeLoaded_*.c
    (void)modelId;
    return false;
}
void CStreaming::Init() {
    // TODO: decomp src/CStreaming/Init_*.c
}
void CStreaming::Init2() {
    // TODO: decomp src/CStreaming/Init2_*.c
}
void CStreaming::InstanceLoadedModels(const CVector& point) {
    // TODO: decomp src/CStreaming/InstanceLoadedModels_*.c
    (void)point;
}
bool CStreaming::IsCarModelNeededInCurrentZone(int32 modelId) {
    // TODO: decomp src/CStreaming/IsCarModelNeededInCurrentZone_*.c
    (void)modelId;
    return false;
}
bool CStreaming::IsInitialised() {
    // TODO: decomp src/CStreaming/IsInitialised_*.c
    return false;
}
bool CStreaming::IsObjectInCdImage(int32 modelId) {
    // TODO: decomp src/CStreaming/IsObjectInCdImage_*.c
    (void)modelId;
    return false;
}
bool CStreaming::IsVeryBusy() {
    // TODO: decomp src/CStreaming/IsVeryBusy_*.c
    return false;
}
void CStreaming::LoadInitialPeds() {
    // TODO: decomp src/CStreaming/LoadInitialPeds_*.c
}
void CStreaming::LoadInitialVehicles() {
    // TODO: decomp src/CStreaming/LoadInitialVehicles_*.c
}
void CStreaming::LoadInitialWeapons() {
    // TODO: decomp src/CStreaming/LoadInitialWeapons_*.c
}
void CStreaming::LoadScene(const CVector& point) {
    // TODO: decomp src/CStreaming/LoadScene_*.c
    (void)point;
}
void CStreaming::LoadSceneCollision(const CVector& point) {
    // TODO: decomp src/CStreaming/LoadSceneCollision_*.c
    (void)point;
}
void CStreaming::LoadZoneVehicle(const CVector& point) {
    // TODO: decomp src/CStreaming/LoadZoneVehicle_*.c
    (void)point;
}
void CStreaming::PossiblyStreamCarOutAfterCreation(int32 modelId) {
    // TODO: decomp src/CStreaming/PossiblyStreamCarOutAfterCreation_*.c
    (void)modelId;
}
void CStreaming::PurgeRequestList() {
    // TODO: decomp src/CStreaming/PurgeRequestList_*.c
}
void CStreaming::ReInit() {
    // TODO: decomp src/CStreaming/ReInit_*.c
}
void CStreaming::ReadIniFile() {
    // TODO: decomp src/CStreaming/ReadIniFile_*.c
}
void CStreaming::ReclassifyLoadedCars() {
    // TODO: decomp src/CStreaming/ReclassifyLoadedCars_*.c
}
void CStreaming::RemoveAllUnusedModels() {
    // TODO: decomp src/CStreaming/RemoveAllUnusedModels_*.c
}
void CStreaming::RemoveBigBuildings() {
    // TODO: decomp src/CStreaming/RemoveBigBuildings_*.c
}
void CStreaming::RemoveBuildingsNotInArea(eAreaCodes areaCode) {
    // TODO: decomp src/CStreaming/RemoveBuildingsNotInArea_*.c
    (void)areaCode;
}
void CStreaming::RemoveCarModel(eModelID modelId) {
    // TODO: decomp src/CStreaming/RemoveCarModel_*.c
    (void)modelId;
}
void CStreaming::RemoveCurrentZonesModels() {
    // TODO: decomp src/CStreaming/RemoveCurrentZonesModels_*.c
}
void CStreaming::RemoveDodgyPedsFromRandomSlots() {
    // TODO: decomp src/CStreaming/RemoveDodgyPedsFromRandomSlots_*.c
}
void CStreaming::RemoveInappropriatePedModels() {
    // TODO: decomp src/CStreaming/RemoveInappropriatePedModels_*.c
}
bool CStreaming::RemoveLeastUsedModel(int32 flags) {
    // TODO: decomp src/CStreaming/RemoveLeastUsedModel_*.c
    (void)flags;
    return false;
}
bool CStreaming::CarIsCandidateForRemoval(int32 modelId) {
    // TODO: decomp src/CStreaming/CarIsCandidateForRemoval_*.c
    (void)modelId;
    return false;
}
bool CStreaming::RemoveLoadedVehicle() {
    // TODO: decomp src/CStreaming/RemoveLoadedVehicle_*.c
    return false;
}
bool CStreaming::RemoveLoadedZoneModel() {
    // TODO: decomp src/CStreaming/RemoveLoadedZoneModel_*.c
    return false;
}
void CStreaming::RemoveUnusedModelsInLoadedList() {
    // TODO: decomp src/CStreaming/RemoveUnusedModelsInLoadedList_*.c
}
void CStreaming::RequestBigBuildings(const CVector& point) {
    // TODO: decomp src/CStreaming/RequestBigBuildings_*.c
    (void)point;
}
void CStreaming::RequestPlayerSection(int32 modelId, const char* string, int32 flags) {
    // TODO: decomp src/CStreaming/RequestPlayerSection_*.c
    (void)modelId;
    (void)string;
    (void)flags;
}
void CStreaming::RequestSpecialChar(int32 modelId, const char* name, int32 flags) {
    // TODO: decomp src/CStreaming/RequestSpecialChar_*.c
    (void)modelId;
    (void)name;
    (void)flags;
}
void CStreaming::RequestSpecialModel(int32 modelId, const char* name, int32 flags) {
    // TODO: decomp src/CStreaming/RequestSpecialModel_*.c
    (void)modelId;
    (void)name;
    (void)flags;
}
void CStreaming::RequestVehicleUpgrade(int32 modelId, int32 flags) {
    // TODO: decomp src/CStreaming/RequestVehicleUpgrade_*.c
    (void)modelId;
    (void)flags;
}
void CStreaming::SetLoadVehiclesInLoadScene(bool bEnable) {
    // TODO: decomp src/CStreaming/SetLoadVehiclesInLoadScene_*.c
    (void)bEnable;
}
void CStreaming::SetMissionDoesntRequireAnim(int32 slot) {
    // TODO: decomp src/CStreaming/SetMissionDoesntRequireAnim_*.c
    (void)slot;
}
void CStreaming::SetMissionDoesntRequireModel(int32 modelId) {
    // TODO: decomp src/CStreaming/SetMissionDoesntRequireModel_*.c
    (void)modelId;
}
void CStreaming::SetMissionDoesntRequireSpecialChar(int32 slot) {
    // TODO: decomp src/CStreaming/SetMissionDoesntRequireSpecialChar_*.c
    (void)slot;
}
void CStreaming::SetModelIsDeletable(int32 modelId, bool mission) {
    // TODO: decomp src/CStreaming/SetModelIsDeletable_*.c
    (void)modelId;
    (void)mission;
}
void CStreaming::SetModelTxdIsDeletable(int32 modelId) {
    // TODO: decomp src/CStreaming/SetModelTxdIsDeletable_*.c
    (void)modelId;
}
void CStreaming::SetModelAndItsTxdDeletable(int32 modelId) {
    // TODO: decomp src/CStreaming/SetModelAndItsTxdDeletable_*.c
    (void)modelId;
}
void CStreaming::SetSpecialCharIsDeletable(int32 slot) {
    // TODO: decomp src/CStreaming/SetSpecialCharIsDeletable_*.c
    (void)slot;
}
void CStreaming::Shutdown() {
    // TODO: decomp src/CStreaming/Shutdown_*.c
}
bool CStreaming::StreamAmbulanceAndMedic(bool bStreamForAccident) {
    // TODO: decomp src/CStreaming/StreamAmbulanceAndMedic_*.c
    (void)bStreamForAccident;
    return false;
}
void CStreaming::StreamCopModels(eLevelName level) {
    // TODO: decomp src/CStreaming/StreamCopModels_*.c
    (void)level;
}
bool CStreaming::StreamFireEngineAndFireman(bool bStreamForFire) {
    // TODO: decomp src/CStreaming/StreamFireEngineAndFireman_*.c
    (void)bStreamForFire;
    return false;
}
void CStreaming::StreamOneNewCar() {
    // TODO: decomp src/CStreaming/StreamOneNewCar_*.c
}
void CStreaming::StreamPedsForInterior(int32 interiorType) {
    // TODO: decomp src/CStreaming/StreamPedsForInterior_*.c
    (void)interiorType;
}
void CStreaming::StreamPedsIntoRandomSlots(const int32 (&modelArray)[TOTAL_LOADED_PEDS]) {
    // TODO: decomp src/CStreaming/StreamPedsIntoRandomSlots_*.c
    (void)modelArray;
}
void CStreaming::StreamVehiclesAndPeds() {
    // TODO: decomp src/CStreaming/StreamVehiclesAndPeds_*.c
}
void CStreaming::StreamVehiclesAndPeds_Always(const CVector& unused) {
    // TODO: decomp src/CStreaming/StreamVehiclesAndPeds_Always_*.c
    (void)unused;
}
void CStreaming::StreamZoneModels(const CVector& unused) {
    // TODO: decomp src/CStreaming/StreamZoneModels_*.c
    (void)unused;
}
void CStreaming::StreamZoneModels_Gangs(const CVector& unused) {
    // TODO: decomp src/CStreaming/StreamZoneModels_Gangs_*.c
    (void)unused;
}
void CStreaming::UpdateForAnimViewer() {
    // TODO: decomp src/CStreaming/UpdateForAnimViewer_*.c
}
bool CStreaming::WeAreTryingToPhaseVehicleOut(int32 modelId) {
    // TODO: decomp src/CStreaming/WeAreTryingToPhaseVehicleOut_*.c
    (void)modelId;
    return false;
}
bool CStreaming::Load() {
    // TODO: decomp src/CStreaming/Load_*.c
    return false;
}
bool CStreaming::Save() {
    // TODO: decomp src/CStreaming/Save_*.c
    return false;
}
// NOTE: template member functions declared in the header