// CStreaming.h - GTA SA 1.0 clean-room C++ conversion
// Adapted from gta-reversed/source/game_sa/Streaming.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Restored 2026-10-09: replaces the 16-line stub (which only carried
// m_bStreamHarvesterModelsThisFrame and a SetMissionDoesntRequireModel stub)
// with the full class. All 53 static members are declared here; their
// definitions live in src/CStreaming.cpp (game addresses recorded there,
// taken from gta-reversed's StaticRef<> addresses). All 110 method
// declarations verified 1:1 against the definitions in src/CStreaming.cpp
// (signatures extracted 2026-10-09).
//
// Adaptations vs gta-reversed:
// - stripped InjectHooks(), VALIDATE_SIZE, StaticRef<T>(addr) (plugin-sdk)
// - CStreamingInfo and its enums/constants live in include/CStreamingInfo.h
// - TOTAL_* model counts + TOTAL_IMG_ARCHIVES/TOTAL_LOADED_PEDS defined here
//   (TODO: move to a shared constants header when the project grows one)
// - tStreamingFileDesc constructor uses portable strncpy (strncpy_s is MSVC-only)
// - template sector-list helpers (DeleteRwObjects*InSectorList,
//   InstanceLoadedModelsInSectorList, ProcessEntitiesInSectorList) omitted:
//   neither defined nor called by src/CStreaming.cpp
// - eLevelName / eAreaCodes are opaque declarations (full eLevelName in CWorld.h;
//   eAreaCodes is still opaque in CColStore.h as well)

#pragma once

#include "CStreamingInfo.h"   // int8..uint32, CdStreamPos, eCdStreamStatus, StreamingImgID,
                              // eStreamingFlags, eStreamingLoadState, STREAMING_SECTOR_SIZE, CStreamingInfo
#include "CVector.h"          // const CVector& parameters
#include "eModelID.h"         // eModelID
#include "CLoadedCarGroup.h"  // CLoadedCarGroup (ms_vehiclesLoaded value member)
#include "ColTypes.h"         // CLink, CLinkList

#include <array>
#include <cassert> // assert (GetModelType, GetInfo)
#include <cstddef> // size_t
#include <cstdint> // int32_t (opaque-enum underlying types)
#include <cstring> // strncpy (tStreamingFileDesc ctor)

class CEntity;             // pointer use only in this header
class CDirectory;          // full shim defined in src/CStreaming.cpp; pointer use only here
enum eLevelName : int32_t; // full enum in CWorld.h (values verified vs decomp src/_types.h)
enum eAreaCodes : int32;   // TODO: full enum (still opaque in CColStore.h as well)

// ---------------------------------------------------------------------------
// Model-count constants (values from gta-reversed source/game_sa/constants.h).
// TODO: move to a shared constants header when the project grows one.
// ---------------------------------------------------------------------------
constexpr uint32 TOTAL_DFF_MODEL_IDS = 20000;
constexpr uint32 TOTAL_TXD_MODEL_IDS = 5000;
constexpr uint32 TOTAL_COL_MODEL_IDS = 255;
constexpr uint32 TOTAL_IPL_MODEL_IDS = 256;
constexpr uint32 TOTAL_DAT_MODEL_IDS = 64;
constexpr uint32 TOTAL_IFP_MODEL_IDS = 180;
constexpr uint32 TOTAL_RRR_MODEL_IDS = 475;
constexpr uint32 TOTAL_SCM_MODEL_IDS = 82;

constexpr uint32 TOTAL_IMG_ARCHIVES = 8;
constexpr uint32 TOTAL_LOADED_PEDS = 8;

enum eResourceFirstID : int32 {
    // First ID of the resource
    RESOURCE_ID_DFF                = 0,                                     // default: 0
    RESOURCE_ID_TXD                = RESOURCE_ID_DFF + TOTAL_DFF_MODEL_IDS, // default: 20000
    RESOURCE_ID_COL                = RESOURCE_ID_TXD + TOTAL_TXD_MODEL_IDS, // default: 25000
    RESOURCE_ID_IPL                = RESOURCE_ID_COL + TOTAL_COL_MODEL_IDS, // default: 25255
    RESOURCE_ID_DAT                = RESOURCE_ID_IPL + TOTAL_IPL_MODEL_IDS, // default: 25511
    RESOURCE_ID_IFP                = RESOURCE_ID_DAT + TOTAL_DAT_MODEL_IDS, // default: 25575
    RESOURCE_ID_RRR                = RESOURCE_ID_IFP + TOTAL_IFP_MODEL_IDS, // default: 25755   (vehicle recordings)
    RESOURCE_ID_SCM                = RESOURCE_ID_RRR + TOTAL_RRR_MODEL_IDS, // default: 26230   (streamed scripts)

    // Used for CStreaming lists, just search for xrefs (VS: shift f12)
    RESOURCE_ID_LOADED_LIST_START  = RESOURCE_ID_SCM + TOTAL_SCM_MODEL_IDS, // default: 26312
    RESOURCE_ID_LOADED_LIST_END    = RESOURCE_ID_LOADED_LIST_START + 1,     // default: 26313

    RESOURCE_ID_REQUEST_LIST_START = RESOURCE_ID_LOADED_LIST_END + 1,       // default: 26314
    RESOURCE_ID_REQUEST_LIST_END   = RESOURCE_ID_REQUEST_LIST_START + 1,    // default: 26315
    RESOURCE_ID_TOTAL                                               // default: 26316
};

enum class eModelType {
    DFF,
    TXD,
    COL,
    IPL,
    DAT,
    IFP,
    RRR,
    SCM,

    INTERNAL_1,
    INTERNAL_2,
    INTERNAL_3,
    INTERNAL_4
};

// Helper functions to deal with modelID's
constexpr bool IsModelDFF(int32 model) { return RESOURCE_ID_DFF <= model && model < RESOURCE_ID_TXD; }
constexpr bool IsModelTXD(int32 model) { return RESOURCE_ID_TXD <= model && model < RESOURCE_ID_COL; }
constexpr bool IsModelCOL(int32 model) { return RESOURCE_ID_COL <= model && model < RESOURCE_ID_IPL; }
constexpr bool IsModelIPL(int32 model) { return RESOURCE_ID_IPL <= model && model < RESOURCE_ID_DAT; }
constexpr bool IsModelDAT(int32 model) { return RESOURCE_ID_DAT <= model && model < RESOURCE_ID_IFP; }
constexpr bool IsModelIFP(int32 model) { return RESOURCE_ID_IFP <= model && model < RESOURCE_ID_RRR; }
constexpr bool IsModelRRR(int32 model) { return RESOURCE_ID_RRR <= model && model < RESOURCE_ID_SCM; }
constexpr bool IsModelSCM(int32 model) { return RESOURCE_ID_SCM <= model && model < RESOURCE_ID_LOADED_LIST_START; }

constexpr eModelType GetModelType(int32 model) {
    if (IsModelDFF(model))
        return eModelType::DFF;

    else if (IsModelTXD(model))
        return eModelType::TXD;

    else if (IsModelCOL(model))
        return eModelType::COL;

    else if (IsModelIPL(model))
        return eModelType::IPL;

    else if (IsModelDAT(model))
        return eModelType::DAT;

    else if (IsModelIFP(model))
        return eModelType::IFP;

    else if (IsModelRRR(model))
        return eModelType::RRR;

    else if (IsModelSCM(model))
        return eModelType::SCM;

    else {
        assert(0); // NOTSA
        return (eModelType)-1;
    }
}

// Turn relative IDs into absolute ones.
constexpr int32 DFFToModelId(int32 relativeId) { return RESOURCE_ID_DFF + relativeId; }
constexpr int32 TXDToModelId(int32 relativeId) { return RESOURCE_ID_TXD + relativeId; }
constexpr int32 COLToModelId(int32 relativeId) { return RESOURCE_ID_COL + relativeId; }
constexpr int32 IPLToModelId(int32 relativeId) { return RESOURCE_ID_IPL + relativeId; }
constexpr int32 DATToModelId(int32 relativeId) { return RESOURCE_ID_DAT + relativeId; }
constexpr int32 IFPToModelId(int32 relativeId) { return RESOURCE_ID_IFP + relativeId; }
constexpr int32 RRRToModelId(int32 relativeId) { return RESOURCE_ID_RRR + relativeId; }
constexpr int32 SCMToModelId(int32 relativeId) { return RESOURCE_ID_SCM + relativeId; }

// Turn absolute IDs into relative ones
constexpr int32 ModelIdToDFF(int32 absId) { return absId - RESOURCE_ID_DFF; }
constexpr int32 ModelIdToTXD(int32 absId) { return absId - RESOURCE_ID_TXD; }
constexpr int32 ModelIdToCOL(int32 absId) { return absId - RESOURCE_ID_COL; }
constexpr int32 ModelIdToIPL(int32 absId) { return absId - RESOURCE_ID_IPL; }
constexpr int32 ModelIdToDAT(int32 absId) { return absId - RESOURCE_ID_DAT; }
constexpr int32 ModelIdToIFP(int32 absId) { return absId - RESOURCE_ID_IFP; }
constexpr int32 ModelIdToRRR(int32 absId) { return absId - RESOURCE_ID_RRR; }
constexpr int32 ModelIdToSCM(int32 absId) { return absId - RESOURCE_ID_SCM; }

enum class eChannelState
{
    // Doing nothing
    IDLE = 0,

    // Currently reading model(s)
    READING = 1,

    // A big model (also called a large file) is loaded in steps; see
    // gta-reversed's comment on this enumerator for the full state machine.
    STARTED = 2,

    // Also called ERROR in the original, but that's a windgi.h macro
    ERR = 3,
};

struct tStreamingFileDesc {
    tStreamingFileDesc() = default;

    tStreamingFileDesc(const char* name, bool bNotPlayerImg) :
        IsNotPlayerImg(bNotPlayerImg),
        StreamHandle(CdStreamOpen(name))
    {
        std::strncpy(Name, name, sizeof(Name) - 1);
        Name[sizeof(Name) - 1] = '\0';
    }

    [[nodiscard]] bool IsInUse() const noexcept { return Name[0] != '\0'; }

    char   Name[40]{}; // If this string is empty the entry isn't in use
    bool   IsNotPlayerImg{};
    uint32 StreamHandle{static_cast<uint32>(-1)};
};

struct tStreamingChannel {
    std::array<int32, 16> modelIds;
    std::array<int32, 16> modelStreamingBufferOffsets;
    eChannelState       LoadStatus;
    int32               loadingLevel; // the value gets modified, but it's not used
    CdStreamPos         pos;
    int32               sectorCount;
    int32               totalTries;
    eCdStreamStatus     m_nCdStreamStatus;

    [[nodiscard]] bool IsIdle() const noexcept    { return LoadStatus == eChannelState::IDLE; }
    [[nodiscard]] bool IsReading() const noexcept { return LoadStatus == eChannelState::READING; }
    [[nodiscard]] bool IsStarted() const noexcept { return LoadStatus == eChannelState::STARTED; }
};

struct tRwStreamInitializeData {
    uint8* m_pBuffer;
    uint32 m_uiBufferSize;
};

class CStreaming {
public:
    // Static data members. Definitions live in src/CStreaming.cpp
    // (game addresses recorded there, from gta-reversed's StaticRef<>).
    static size_t ms_memoryAvailable; // 25'600'000 == 25.6 MB
    static uint32 desiredNumVehiclesLoaded;
    static bool ms_bLoadVehiclesInLoadScene;

    // Default models for each level (see eLevelName)
    static std::array<int32, 5> ms_aDefaultCopCarModel; // Last one is bike cop, not matching any level name
    static std::array<int32, 5> ms_aDefaultCopModel;    // Last one is bike cop, not matching any level name

    static uint32 ms_nTimePassedSinceLastCopBikeStreamedIn;

    static std::array<int32, 4> ms_aDefaultAmbulanceModel;
    static std::array<int32, 4> ms_aDefaultMedicModel;
    static std::array<int32, 4> ms_aDefaultFireEngineModel;
    static std::array<int32, 4> ms_aDefaultFiremanModel;

    // Default models for current level
    static int32 ms_DefaultCopBikeModel;
    static int32 ms_DefaultCopBikerModel;

    static CDirectory* ms_pExtraObjectsDir;
    static tStreamingFileDesc ms_files[TOTAL_IMG_ARCHIVES];
    static bool ms_bLoadingBigModel;
    // There are only two channels within CStreaming::ms_channel
    static std::array<tStreamingChannel, 2> ms_channel;
    static int32 ms_channelError;
    static bool m_bHarvesterModelsRequested;
    static bool m_bStreamHarvesterModelsThisFrame;
    static uint32 ms_numPriorityRequests;
    //! Initialized to -1 and never used
    static int32 ms_lastCullZone;
    static uint16 ms_loadedGangCars;
    // Bitfield of gangs loaded. Each gang is a bit. (0th bit being BALLAS, following the ordering in POPCYCLE_GROUP_BALLAS)
    static uint16 ms_loadedGangs;

    static std::array<eModelID, 8> ms_pedsLoaded; //!< Currently loaded peds (for/from ped groups)
    static uint32 ms_numPedsLoaded;               //!< Number of active values in ms_pedsLoaded
    //! Contains the next slot, that is, index at which the next model to load of a group is.
    static std::array<int32, 18> ms_NextPedToLoadFromGroup;

    static int32 ms_currentZoneType;
    static CLoadedCarGroup ms_vehiclesLoaded;
    static CStreamingInfo* ms_pEndRequestedList;
    static CStreamingInfo* ms_pStartRequestedList;
    static CStreamingInfo* ms_pEndLoadedList;
    static CStreamingInfo* ms_startLoadedList;

    static int32 ms_lastImageRead; // initialized but not used?
    static std::array<int32, 6> ms_imageOffsets; // initialized but never used?

    static bool ms_bEnableRequestListPurge;
    static uint32 ms_streamingBufferSize;
    static uint8* ms_pStreamingBuffer[2];
    static uint32 ms_memoryUsedBytes;
    static int32 ms_numModelsRequested;
    static std::array<CStreamingInfo, 26316> ms_aInfoForModel;
    static bool ms_disableStreaming;
    static int32 ms_bIsInitialised;
    static bool m_bBoatsNeeded;
    static bool ms_bLoadingScene;
    static bool m_bCopBikeLoaded;
    static bool m_bDisableCopBikes;
    static CLinkList<CEntity*> ms_rwObjectInstances;
    static CLink<CEntity*>* ms_renderEntityLink;
    static bool m_bLoadingAllRequestedModels;
    static bool m_bModelStreamNotLoaded;
    static bool ms_bReadLayerForceFully;
    static int32 ms_oldSectorX;
    static int32 ms_oldSectorY;

public:
    static CLink<CEntity*>* AddEntity(CEntity* entity);
    static uint32 AddImageToList(const char* fileName, bool bNotPlayerImg);
    static void AddLodsToRequestList(const CVector& point, int32 flags);
    static void AddModelsToRequestList(const CVector& point, int32 flags);
    static bool AddToLoadedVehiclesList(int32 modelId);
    static bool AreAnimsUsedByRequestedModels(int32 animModelId);
    static bool AreTexturesUsedByRequestedModels(int32 txdModelId);
    static bool CarIsCandidateForRemoval(int32 modelId);
    static void ClearFlagForAll(uint32 streamingFlag);
    static void ClearSlots(uint32 totalSlots);
    static bool ConvertBufferToObject(uint8* fileBuffer, int32 modelId);
    static void DeleteAllRwObjects();
    static bool DeleteLeastUsedEntityRwObject(bool bNotOnScreen, int32 flags);
    static void DeleteRwObjectsAfterDeath(const CVector& point);
    static void DeleteRwObjectsBehindCamera(size_t memoryToCleanInBytes);
    static void DisableCopBikes(bool bDisable);
    static int32 FindMIPedSlotForInterior(int32 randFactor);
    static void FinishLoadingLargeFile(uint8* pFileBuffer, int32 modelId);
    static void FlushChannels();
    static void FlushRequestList();
    static void ForceLayerToRead(int32 arg1);
    static int32 GetDefaultCabDriverModel();
    static eModelID GetDefaultCopCarModel(bool ignoreLvpd1Model = true);
    static eModelID GetDefaultCopModel();
    static eModelID GetDefaultFiremanModel();
    static eModelID GetDefaultMedicModel();
    static int32 GetDiscInDrive();
    static int32 GetNextFileOnCd(uint32 streamLastPosn, bool bNotPriority);
    static bool HasSpecialCharLoaded(int32 slot);
    static bool HasVehicleUpgradeLoaded(int32 modelId);
    static void IHaveUsedStreamingMemory();
    static void ImGonnaUseStreamingMemory();
    static void Init();
    static void Init2();
    static void InitImageList();
    static void InstanceLoadedModels(const CVector& point);
    static bool IsCarModelNeededInCurrentZone(int32 modelId);
    static bool IsInitialised();
    static bool IsObjectInCdImage(int32 modelId);
    static bool IsVeryBusy();
    static bool Load();
    static void LoadAllRequestedModels(bool bOnlyPriorityRequests);
    static void LoadCdDirectory(const char* filename, StreamingImgID img);
    static void LoadCdDirectory();
    static void LoadInitialPeds();
    static void LoadInitialVehicles();
    static void LoadInitialWeapons();
    static void LoadRequestedModels();
    static void LoadScene(const CVector& point);
    static void LoadSceneCollision(const CVector& point);
    static void LoadZoneVehicle(const CVector& point);
    static void MakeSpaceFor(size_t memoryToCleanInBytes);
    static void PossiblyStreamCarOutAfterCreation(int32 modelId);
    static bool ProcessLoadingChannel(int32 chIdx);
    static void PurgeRequestList();
    static void ReadIniFile();
    static void ReclassifyLoadedCars();
    static void ReInit();
    static void RemoveAllUnusedModels();
    static void RemoveBigBuildings();
    static void RemoveBuildingsNotInArea(eAreaCodes areaCode);
    static void RemoveCarModel(eModelID modelId);
    static void RemoveCurrentZonesModels();
    static void RemoveDodgyPedsFromRandomSlots();
    static void RemoveEntity(CLink<CEntity*>* streamingLink);
    static void RemoveInappropriatePedModels();
    static bool RemoveLeastUsedModel(int32 flags);
    static bool RemoveLoadedVehicle();
    static bool RemoveLoadedZoneModel();
    static void RemoveModel(int32 modelId);
    static bool RemoveReferencedTxds(size_t goalMemoryUsageBytes);
    static void RemoveTxdModel(int32 modelId);
    static void RemoveUnusedModelsInLoadedList();
    static void RenderEntity(CLink<CEntity*>* streamingLink);
    static void RequestBigBuildings(const CVector& point);
    static void RequestFile(int32 modelId, CdStreamPos posn, uint32 size, StreamingImgID imgId, int32 streamingFlags);
    static void RequestFilesInChannel(int32 chIdx);
    static void RequestModel(int32 modelId, int32 flags);
    static void RequestModelStream(int32 chIdx);
    static void RequestPlayerSection(int32 modelId, const char* string, int32 flags);
    static void RequestSpecialChar(int32 modelId, const char* name, int32 flags);
    static void RequestSpecialModel(int32 modelId, const char* name, int32 flags);
    static void RequestTxdModel(int32 slot, int32 flags);
    static void RequestVehicleUpgrade(int32 modelId, int32 flags);
    static void RetryLoadFile(int32 chIdx);
    static bool Save();
    static void SetLoadVehiclesInLoadScene(bool bEnable);
    static void SetMissionDoesntRequireAnim(int32 slot);
    static void SetMissionDoesntRequireModel(int32 modelId);
    static void SetMissionDoesntRequireSpecialChar(int32 slot);
    static void SetModelAndItsTxdDeletable(int32 modelId);
    static void SetModelIsDeletable(int32 modelId, bool mission = false);
    static void SetModelTxdIsDeletable(int32 modelId);
    static void SetSpecialCharIsDeletable(int32 slot);
    static void Shutdown();
    static void StartRenderEntities();
    static bool StreamAmbulanceAndMedic(bool bStreamForAccident);
    static void StreamCopModels(eLevelName level);
    static bool StreamFireEngineAndFireman(bool bStreamForFire);
    static void StreamOneNewCar();
    static void StreamPedsForInterior(int32 interiorType);
    static void StreamPedsIntoRandomSlots(const int32 (&modelArray)[TOTAL_LOADED_PEDS]);
    static void StreamVehiclesAndPeds();
    static void StreamVehiclesAndPeds_Always(const CVector& unused);
    static void StreamZoneModels(const CVector& unused);
    static void StreamZoneModels_Gangs(const CVector& unused);
    static void Update();
    static void UpdateForAnimViewer();
    static void UpdateMemoryUsed();
    static bool WeAreTryingToPhaseVehicleOut(int32 modelId);

    // Inlined or NOTSA
    static bool IsModelLoaded(int32 model) { return ms_aInfoForModel[model].m_LoadState == eStreamingLoadState::LOADSTATE_LOADED; }
    static CStreamingInfo& GetInfo(int32 modelId) { assert(modelId >= 0); return ms_aInfoForModel[modelId]; }
    static bool IsRequestListEmpty() { return ms_pEndRequestedList->GetPrev() == ms_pStartRequestedList; }
    static ptrdiff_t GetModelFromInfo(const CStreamingInfo* info) { return info - ms_aInfoForModel.data(); }
};
