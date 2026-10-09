// CAERadioTrackManager - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Audio/Managers/AERadioTrackManager.h
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(); VALIDATE_SIZE replaced with guarded
//   static_assert (active on 32-bit targets only); `static inline auto& x =
//   StaticRef<T>(0xADDR)` globals replaced with plain static member declarations
//   defined in CAERadioTrackManager.cpp (original game addresses kept as comments);
//   `extern CAERadioTrackManager& AERadioTrackManager` (StaticRef 0x8CB6F8)
//   replaced with a plain extern object defined in CAERadioTrackManager.cpp;
//   rng::fill replaced with std::fill; eRadioID/eBassSetting ported in full
//   (small enums); tVehicleAudioSettings forward-declared (port with audio entities).
// Integer types converted to <cstdint>.
// TODO: verify each method against decomp src/CAERadioTrackManager/*.c

#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

struct tVehicleAudioSettings; // Audio/Entities/AEVehicleAudioEntity.h - port with audio entities

// GxtChar is an 8-bit GXT character (gta-reversed GxtChar.h)
using GxtChar = uint8_t;

enum class eBassSetting : int8_t {
    NORMAL = 0,
    BOOST  = 1,
    CUT    = 2,
};
#include "eRadioID.h" // canonical (deduped 2026-10-09)

enum {
    TYPE_INDENT      = 0,
    TYPE_ADVERT      = 1,
    TYPE_DJ_BANTER   = 2,
    TYPE_INTRO       = 3,
    TYPE_TRACK       = 4,
    TYPE_OUTRO       = 5,
    TYPE_NONE        = 6,
    TYPE_USER_TRACK  = 7,
};

struct tRadioSettings {
    static constexpr size_t NUM_TRACKS = 5u;

    tRadioSettings(eRadioID currentStation = RADIO_OFF) :
        StationID(currentStation)
    {
        std::fill(TrackQueue.begin(), TrackQueue.end(), -1);
        std::fill(TrackTypes.begin(), TrackTypes.end(), int8_t(TYPE_NONE));
        std::fill(TrackIndices.begin(), TrackIndices.end(), int8_t(-1));
    }

    void Reset() {
        *this = {};
    }

    void SwitchToNextTrack() {
        PrevTrackID   = TrackQueue.front();
        PrevTrackType = TrackTypes.front();
        PrevTrackIdx  = TrackIndices.front();

        const auto Rotate = [](auto& arr, auto invalidValue) {
            std::copy(arr.begin() + 1, arr.end(), arr.begin());
            arr.back() = invalidValue;
        };
        Rotate(TrackQueue,   -1);
        Rotate(TrackTypes,   int8_t(TYPE_NONE));
        Rotate(TrackIndices, int8_t(-1));
    }

    std::array<int32_t, NUM_TRACKS> TrackQueue{ -1 };
    int32_t                         CurrTrackID{ -1 };
    int32_t                         PrevTrackID{ -1 };
    int32_t                         PlayTime{ 0 };
    int32_t                         TrackLengthMs{ 0 };
    int8_t                          TrackFlags{ 2 };        // TODO: enum
    eRadioID                        StationID{ RADIO_OFF }; // NOTSA init value.
    eBassSetting                    BassSetting{ eBassSetting::NORMAL };
    float                           BassGain{}; // unk. init
    std::array<int8_t, NUM_TRACKS>  TrackTypes{ TYPE_NONE };
    int8_t                          CurrTrackType{ TYPE_NONE };
    int8_t                          PrevTrackType{ TYPE_NONE };
    std::array<int8_t, NUM_TRACKS>  TrackIndices{ -1 };
    int8_t                          CurrTrackIdx{ -1 }; //!< Index into `TrackIndices`
    int8_t                          PrevTrackIdx{ -1 }; //!< Index into `TrackIndices`
};
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tRadioSettings) == 0x3C, "tRadioSettings layout changed");
#endif

struct tRadioState {
    std::array<int32_t, 3> m_aElapsed{0};
    int32_t m_iTimeInPauseModeInMs{-1};
    int32_t m_iTimeInMs{-1};
    int32_t m_iTrackPlayTime{-1};
    std::array<int32_t, 3> m_aTrackQueue{-1};
    std::array<int8_t, 3>  m_aTrackTypes{TYPE_NONE};
    int8_t m_nGameClockDays{-1};
    int8_t m_nGameClockHours{-1};

    void Reset(bool paused = false) {
        std::fill(m_aElapsed.begin(), m_aElapsed.end(), 0);
        std::fill(m_aTrackQueue.begin(), m_aTrackQueue.end(), -1);
        std::fill(m_aTrackTypes.begin(), m_aTrackTypes.end(), int8_t(TYPE_NONE));

        m_iTimeInMs = -1;
        if (!paused)
            m_iTimeInPauseModeInMs = -1;
        m_iTrackPlayTime = -1;
        m_nGameClockDays = -1;
        m_nGameClockHours = -1;
    }
};
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tRadioState) == 0x2C, "tRadioState layout changed");
#endif

// NOTSA
template<typename T, size_t Count>
struct tRadioIndexHistory {
    std::array<T, Count> indices{-1};

    void Reset() {
        std::fill(indices.begin(), indices.end(), T(-1));
    }

    void PutAtFirst(int32_t index) {
        if constexpr (Count > 1) {
            // rotate all elements to right.
            std::rotate(indices.rbegin(), indices.rbegin() + 1, indices.rend());
        }
        indices[0] = static_cast<T>(index);
    }
};
static_assert(sizeof(tRadioIndexHistory<int32_t, 1>) == sizeof(int32_t), "tRadioIndexHistory has unexpected padding");

enum class eRadioTrackMode {
    RADIO_STARTING,
    RADIO_WAITING_TO_PLAY,
    RADIO_PLAYING,
    RADIO_STOPPING, // ?
    RADIO_STOPPING_SILENCED,
    RADIO_STOPPING_CHANNELS_STOPPED,
    RADIO_WAITING_TO_STOP,
    RADIO_STOPPED
};

class CAERadioTrackManager {
public:
    bool            m_bInitialised{false};
    bool            m_bDisplayStationName{false};
    char            m_prev{0}; // TODO: make sense of this.
    bool            m_bEnabledInPauseMode{false};
    bool            m_bBassEnhance{true};
    bool            m_bPauseMode{false};
    bool            m_bRetuneJustStarted{false};
    bool            m_bRadioAutoSelect{true};
    std::array<uint8_t, RADIO_COUNT>       m_nTracksInARow{};
    uint8_t           m_nSavedGameClockDays{0xff};
    uint8_t           m_nSavedGameClockHours{0xff};
    std::array<int32_t, RADIO_COUNT>       m_aListenTimes{}; // Filled from `CStats::FavoriteRadioStationList`
    uint32_t          m_nTimeRadioStationRetuned{0};
    uint32_t          m_nTimeToDisplayRadioName{0};
    uint32_t          m_nSavedTimeMs{0};
    uint32_t          m_nRetuneStartedTime;
    uint32_t          field_60{0};
    int32_t           m_HwClientHandle;
    eRadioTrackMode m_nMode{eRadioTrackMode::RADIO_STOPPED};
    int32_t           m_nStationsListed{0};
    int32_t           m_nStationsListDown{0};
    int32_t           m_nSavedRadioStationId{-1};         // TODO: convert to eRadioID after finished reversing
    int32_t           m_iRadioStationMenuRequest{ -1 };   // <-
    int32_t           m_iRadioStationScriptRequest{ -1 }; // <-
    float           m_f80{0.0f}; // 80 and 84 volume related fields. See ::UpdateRadioVolumes
    float           m_f84{0.0f};
    tRadioSettings  m_RequestedSettings{}; // settings1
    tRadioSettings  m_ActiveSettings{}; // settings2
    std::array<tRadioState, RADIO_COUNT> m_aRadioState{};
    uint32_t          field_368{0};
    uint8_t           m_nUserTrackPlayMode{};

public:
    static constexpr auto DJBANTER_INDEX_HISTORY_COUNT = 15;
    static constexpr auto ADVERT_INDEX_HISTORY_COUNT   = 40;
    static constexpr auto IDENT_INDEX_HISTORY_COUNT    = 8;
    static constexpr auto MUSIC_TRACK_HISTORY_COUNT    = 20;
    using DJBanterIndexHistory = tRadioIndexHistory<int32_t, DJBANTER_INDEX_HISTORY_COUNT>;
    using AdvertIndexHistory   = tRadioIndexHistory<int32_t, ADVERT_INDEX_HISTORY_COUNT>;
    using IdentIndexHistory    = tRadioIndexHistory<int32_t, IDENT_INDEX_HISTORY_COUNT>;
    using MusicTrackHistory    = tRadioIndexHistory<int8_t, MUSIC_TRACK_HISTORY_COUNT>;

    // Static history tables. Original was `static inline auto& x = StaticRef<T[RADIO_COUNT]>(0xADDR)`;
    // clean-room uses plain static members, defined in CAERadioTrackManager.cpp.
    static DJBanterIndexHistory m_nDJBanterIndexHistory[RADIO_COUNT]; // orig 0xB61D78
    static AdvertIndexHistory   m_nAdvertIndexHistory[RADIO_COUNT];   // orig 0xB620C0
    static IdentIndexHistory    m_nIdentIndexHistory[RADIO_COUNT];    // orig 0xB62980
    static MusicTrackHistory    m_nMusicTrackIndexHistory[RADIO_COUNT]; // orig 0xB62B40

    static uint8_t m_nStatsLastHitTimeOutHours;    // orig 0xB62C58
    static uint8_t m_nStatsLastHitGameClockHours;  // orig 0xB62C59
    static uint8_t m_nStatsLastHitGameClockDays;   // orig 0xB62C5A
    static uint8_t m_nStatsStartedCrash1;          // orig 0xB62C5B
    static uint8_t m_nStatsStartedCat2;            // orig 0xB62C5C
    static uint8_t m_nStatsStartedBadlands;        // orig 0xB62C5D
    static uint8_t m_nStatsPassedVCrash2;          // orig 0xB62C5E
    static uint8_t m_nStatsPassedTruth2;           // orig 0xB62C5F
    static uint8_t m_nStatsPassedSweet2;           // orig 0xB62C60
    static uint8_t m_nStatsPassedStrap4;           // orig 0xB62C61
    static uint8_t m_nStatsPassedSCrash1;          // orig 0xB62C62
    static uint8_t m_nStatsPassedRiot1;            // orig 0xB62C63
    static uint8_t m_nStatsPassedRyder2;           // orig 0xB62C64
    static uint8_t m_nStatsPassedMansion2;         // orig 0xB62C65
    static uint8_t m_nStatsPassedLAFin2;           // orig 0xB62C66
    static uint8_t m_nStatsPassedFarlie3;          // orig 0xB62C67
    static uint8_t m_nStatsPassedDesert10;         // orig 0xB62C68
    static uint8_t m_nStatsPassedDesert8;          // orig 0xB62C69
    static uint8_t m_nStatsPassedDesert5;          // orig 0xB62C6A
    static uint8_t m_nStatsPassedDesert3;          // orig 0xB62C6B
    static uint8_t m_nStatsPassedDesert1;          // orig 0xB62C6C
    static uint8_t m_nStatsPassedCat1;             // orig 0xB62C6D
    static uint8_t m_nStatsPassedCasino10;         // orig 0xB62C6E
    static uint8_t m_nStatsPassedCasino6;          // orig 0xB62C6F
    static uint8_t m_nStatsPassedCasino3;          // orig 0xB62C70
    static uint8_t m_nStatsCitiesPassed;           // orig 0xB62C71
    static uint8_t m_nSpecialDJBanterIndex;        // orig 0xB62C72
    static uint8_t m_nSpecialDJBanterPending;      // orig 0xB62C73

public:
    CAERadioTrackManager(int32_t hwClientHandle);

    CAERadioTrackManager() = default; // NOTSA
    ~CAERadioTrackManager() = default;

    bool Initialise(int32_t channelId);
    void InitialiseRadioStationID(eRadioID id);

    void Reset();
    static void ResetStatistics();

    bool   IsRadioOn() const;
    bool   HasRadioRetuneJustStarted() const;
    eRadioID GetCurrentRadioStationID() const;
    int32_t* GetRadioStationListenTimes();
    void   SetRadioAutoRetuneOnOff(bool enable);
    void   SetBassEnhanceOnOff(bool enable);
    void   SetBassSetting(eBassSetting bassSetting, float bassGrain);
    void   RetuneRadio(eRadioID radioId);

    void  DisplayRadioStationName();
    const GxtChar* GetRadioStationName(eRadioID id);
    void  GetRadioStationNameKey(eRadioID id, char* outStr);
    static bool IsVehicleRadioActive();

    void StartTrackPlayback();
    void UpdateRadioVolumes();
    void PlayRadioAnnouncement(uint32_t);
    void StartRadio(eRadioID id, eBassSetting bassSetting, float bassGain, bool skipTrack);
    void StartRadio(const tVehicleAudioSettings& settings);
    void StopRadio(tVehicleAudioSettings* settings, bool bDuringPause);

    void Service(int32_t playTime);

    static void Load();
    static void Save();

protected:
    void AddMusicTrackIndexToHistory(eRadioID id, int8_t trackIndex);
    void AddIdentIndexToHistory(eRadioID id, int8_t trackIndex);
    void AddAdvertIndexToHistory(eRadioID id, int8_t trackIndex);
    void AddDJBanterIndexToHistory(eRadioID id, int8_t trackIndex);

    void  ChooseTracksForStation(eRadioID id);
    int32_t ChooseIdentIndex(eRadioID id);
    int32_t ChooseAdvertIndex(eRadioID id);
    int32_t ChooseDJBanterIndex(eRadioID id);
    int32_t ChooseDJBanterIndexFromList(eRadioID id, int32_t** list);
    int8_t  ChooseMusicTrackIndex(eRadioID id);
    static int8_t  ChooseTalkRadioShow();

    void CheckForTrackConcatenation();
    static void CheckForMissionStatsChanges();
    void CheckForStationRetune();
    void CheckForStationRetuneDuringPause();
    void CheckForPause();

    bool QueueUpTracksForStation(eRadioID id, int8_t* iTrackCount, int8_t radioState, tRadioSettings& settings);
    bool TrackRadioStation(eRadioID id, bool skipTrack);
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAERadioTrackManager) == 0x370, "CAERadioTrackManager layout changed");
#endif

// Global instance, defined in CAERadioTrackManager.cpp.
// Original GTA SA 1.0 address (from gta-reversed StaticRef) kept in the .cpp.
extern CAERadioTrackManager AERadioTrackManager;
