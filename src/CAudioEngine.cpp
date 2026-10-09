// CAudioEngine.cpp - GTA SA 1.0 clean-room C++ conversion
// Method bodies converted from src/CAudioEngine/*.c (Ghidra decomp).
// This file is part of a clean-room engine reimplementation for
// interoperability research. Game assets load from the user's own install
// at runtime and are never shipped.
//
// Porting notes:
// - Calls into CAEAudioHardware / CAERadioTrackManager / CFileMgr / CTimer are
//   real: those subsystems have ported headers with matching declarations.
// - Calls into the audio entities (CAEFrontendAudioEntity, CAEScriptAudioEntity,
//   CAECollisionAudioEntity, CAEPedlessSpeechAudioEntity, CAEDoorAudioEntity,
//   CAEGlobalWeaponAudioEntity, CAEWeatherAudioEntity, CAEPedSpeechAudioEntity,
//   CAEPoliceScannerAudioEntity, CAEAmbienceTrackManager, CAECutsceneTrackManager)
//   are marked TODO(port): they are size-verified stand-ins in CAudioEngine.h
//   with no methods yet. The decomp call sequence is preserved in comments.
// - Playback ultimately goes through DirectSound in CAEAudioHardware; per the
//   port plan the backend maps to bevy_kira_audio later. TODO(port) marks it.

#include "CAudioEngine.h"

#include "CAEAudioHardware.h" // AEAudioHardware
#include "CFileMgr.h"         // EVENTVOL.DAT loading
#include "CTimer.h"

#include <cstdint>
#include <utility> // std::pair (bank gate table)

// Global instance.
// Original GTA SA 1.0 address (from gta-reversed StaticRef) kept as comment.
// TODO: re-resolve for the clean-room build.
CAudioEngine AudioEngine; // 0xB6BC90

// ---------------------------------------------------------------------------
// Shims for not-yet-ported pieces. Declared here only so this TU
// syntax-checks; the real declarations belong in their subsystem headers.
// TODO(port): delete this block as the subsystems land (see BUILD_NOTES.md).
// ---------------------------------------------------------------------------

// CLoadingScreen.h not ported.
class CLoadingScreen {
public:
    static void Pause();
    static void Continue();
};

// CAESoundManager.h not ported.
class CAESoundManager {
public:
    static bool Initialise();
    static void Service();
    static void Reset();
    static void Terminate();
    static void PauseManually(bool paused);
};

// NOTE: CAERadioTrackManager.h is deliberately NOT included here: it
// re-defines `enum eRadioID : int8_t`, which CAudioEngine.h also defines -
// including both is a hard redefinition error. TODO: fix the header
// duplication (shared audio-enums header), then include the real header and
// delete this shim. Signatures below mirror CAERadioTrackManager.h.
class CAERadioTrackManager {
public:
    bool     Initialise(int32_t channelId);
    void     InitialiseRadioStationID(eRadioID id);
    void     Reset();
    static void ResetStatistics();
    bool     IsRadioOn() const;
    bool     HasRadioRetuneJustStarted() const;
    eRadioID GetCurrentRadioStationID() const;
    void     SetRadioAutoRetuneOnOff(bool enable);
    void     SetBassEnhanceOnOff(bool enable);
    void     SetBassSetting(eBassSetting bassSetting, float bassGrain);
    void     RetuneRadio(eRadioID radioId);
    void     DisplayRadioStationName();
    const GxtChar* GetRadioStationName(eRadioID id);
    void     GetRadioStationNameKey(eRadioID id, char* outStr);
    int32_t* GetRadioStationListenTimes();
    static bool IsVehicleRadioActive();
    void     StartRadio(eRadioID id, eBassSetting bassSetting, float bassGain, bool skipTrack);
    void     StartRadio(const tVehicleAudioSettings& settings);
    void     StopRadio(tVehicleAudioSettings* settings, bool bDuringPause);
    void     Service(int32_t playTime);
};
extern CAERadioTrackManager AERadioTrackManager; // 0x8CB6F8

// eSoundBank / eSoundBankSlot are opaque in the ported headers (full enums not
// ported). The values below are from gta-reversed's
// Audio/Enums/eSoundBank.h and Audio/Enums/eSoundBankSlot.h and are needed for
// the InitialisePostLoading bank gate.
// TODO(port): delete this block when the full enums are ported.
constexpr eSoundBank SND_BANK_GENRL_COLLISIONS  = static_cast<eSoundBank>(39);
constexpr eSoundBank SND_BANK_GENRL_BULLET_HITS  = static_cast<eSoundBank>(27);
constexpr eSoundBank SND_BANK_GENRL_VEHICLE_GEN = static_cast<eSoundBank>(138);
constexpr eSoundBank SND_BANK_FEET_GENERIC      = static_cast<eSoundBank>(0);
constexpr eSoundBank SND_BANK_GENRL_FRONTEND_GAME = static_cast<eSoundBank>(59);
constexpr eSoundBank SND_BANK_GENRL_EXPLOSIONS  = static_cast<eSoundBank>(52);
constexpr eSoundBank SND_BANK_GENRL_WEAPONS     = static_cast<eSoundBank>(143);
constexpr eSoundBank SND_BANK_GENRL_DOORS       = static_cast<eSoundBank>(51);
constexpr eSoundBank SND_BANK_GENRL_RAIN        = static_cast<eSoundBank>(105);
constexpr eSoundBank SND_BANK_GENRL_HORN        = static_cast<eSoundBank>(74);
constexpr eSoundBank SND_BANK_GENRL_SWIMMING    = static_cast<eSoundBank>(128);
constexpr eSoundBank SND_BANK_GENRL_APACHE_D    = static_cast<eSoundBank>(13);

constexpr eSoundBankSlot SND_BANK_SLOT_COLLISIONS       = static_cast<eSoundBankSlot>(2);
constexpr eSoundBankSlot SND_BANK_SLOT_BULLET_HITS      = static_cast<eSoundBankSlot>(3);
constexpr eSoundBankSlot SND_BANK_SLOT_VEHICLE_GEN      = static_cast<eSoundBankSlot>(19);
constexpr eSoundBankSlot SND_BANK_SLOT_FOOTSTEPS_GENERIC = static_cast<eSoundBankSlot>(41);
constexpr eSoundBankSlot SND_BANK_SLOT_FIRST            = static_cast<eSoundBankSlot>(0);
constexpr eSoundBankSlot SND_BANK_SLOT_EXPLOSIONS       = static_cast<eSoundBankSlot>(4);
constexpr eSoundBankSlot SND_BANK_SLOT_WEAPON_GEN       = static_cast<eSoundBankSlot>(5);
constexpr eSoundBankSlot SND_BANK_SLOT_DOORS            = static_cast<eSoundBankSlot>(31);
constexpr eSoundBankSlot SND_BANK_SLOT_WEATHER          = static_cast<eSoundBankSlot>(6);
constexpr eSoundBankSlot SND_BANK_SLOT_DUMMY_END        = static_cast<eSoundBankSlot>(17);
constexpr eSoundBankSlot SND_BANK_SLOT_SWIMMING         = static_cast<eSoundBankSlot>(32);
constexpr eSoundBankSlot SND_BANK_SLOT_COP_HELI         = static_cast<eSoundBankSlot>(18);

// File-static engine state (original game addresses kept as comments).
// TODO(port): these live in CAEAudioHardware / the track managers; fold them
// in when those subsystems are ported.
namespace {
uint8_t* s_pAudioEventVolumes = nullptr; // EVENTVOL.DAT contents, 0xB159 bytes
int32_t  s_nFreeAudioChannels = 0;       // 0xB5F944: free channel count
uint16_t s_aAudioChannelUse[64] = {};    // 0xB5F948: 64-entry channel-use table
eRadioID s_nCurrentRadioStation = RADIO_INVALID; // 0x8CB7E1 (eRam008cb7e1)
} // namespace
// ---------------------------------------------------------------------------

// NOTE: CAudioEngine() = default and ~CAudioEngine() = default in the header.
// The decomp ctor (0x507670) / dtor (0x506CD0) only set the audio-entity
// vtable pointers and construct the entity members; that work comes back with
// the entity ports (TODO(port)).

// 0x5B9C60
bool CAudioEngine::Initialise() {
    CLoadingScreen::Pause();
    if (!AEAudioHardware.Initialise()) {
        return false;
    }

    // Allocate one hardware channel for background audio (radio / cutscene /
    // ambience streams). Original scans the 64-entry channel-use table at
    // 0xB5F948 for a free slot (non-zero entries are skip counts).
    int16_t channel = -1;
    if (s_nFreeAudioChannels != 0) {
        int32_t i = 0;
        do {
            if (s_aAudioChannelUse[i] == 0) {
                s_aAudioChannelUse[i] = 1;
                --s_nFreeAudioChannels;
                channel = static_cast<int16_t>(i);
                break;
            }
            i += s_aAudioChannelUse[i];
        } while (i < 0x40);
    }
    m_nBackgroundAudioChannel = channel;

    if (!AERadioTrackManager.Initialise(channel)) {
        return false;
    }

    // TODO(port): CAECutsceneTrackManager / CAEAmbienceTrackManager not ported.
    // Decomp inits: cutscene channel = m_nBackgroundAudioChannel, mode = 8,
    //   track id/status = -1; ambience channel = m_nBackgroundAudioChannel,
    //   mode = 8, volume = -100.0f, freq = 1.0f, flags = 3, handles = -1.

    if (!CAESoundManager::Initialise()) {
        return false;
    }

    // EVENTVOL.DAT: per-audio-event volume table (0xB159 bytes, raw dump).
    s_pAudioEventVolumes = new uint8_t[0xB159];
    FILESTREAM file = CFileMgr::OpenFile("AUDIO\\CONFIG\\EVENTVOL.DAT", "rb"); // orig mode: &DAT_0085a53c
    if (file != nullptr) {
        const size_t read = CFileMgr::Read(file, s_pAudioEventVolumes, 0xB159);
        CFileMgr::CloseFile(file);
        if (read != 0xB159) {
            return false;
        }
    } else {
        return false;
    }

    // TODO(port): entity static initialisers, in decomp order:
    //   CAEFrontendAudioEntity::Initialise(&m_FrontendAE);
    //   SetEffectsFaderScalingFactor(0.0f);
    //   CAEAudioUtility::StaticInitialise();
    //   CAEPedAudioEntity::StaticInitialise();
    //   CAEPedSpeechAudioEntity::StaticInitialise();
    //   CAEVehicleAudioEntity::StaticInitialise();
    //   CAEExplosionAudioEntity::StaticInitialise();
    //   CAEWeatherAudioEntity::StaticInitialise();
    //   CAEDoorAudioEntity::StaticInitialise();
    //   CAEFireAudioEntity::StaticInitialise();
    //   CAEPoliceScannerAudioEntity::StaticInitialise();
    //   CAEScriptAudioEntity::Initialise(&m_ScriptAE);
    //   CAEPedlessSpeechAudioEntity::Initialise(&m_PedlessSpeechAE);
    //   CAECollisionAudioEntity::Initialise(&m_CollisionAE);

    m_nCurrentRadioStationId = RADIO_INVALID;
    m_bPlayingMissionCompleteTrack = false;
    m_bStoppingMissionCompleteTrack = false;
    CLoadingScreen::Continue();
    return true;
}

// 0x5078F0 - bank-loading gate: blocks until every core sound bank is loaded.
void CAudioEngine::InitialisePostLoading() {
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_LOADING_TUNE_STOP, 0.0f, 1.0f);
    // TODO(port): Service();
    // TODO(port): CAECollisionAudioEntity::InitialisePostLoading();
    // TODO(port): m_GlobalWeaponAE = new CAEGlobalWeaponAudioEntity();
    // TODO(port): CAEWeaponAudioEntity::Initialise(m_GlobalWeaponAE);

    // (bank, slot) pairs the engine waits on, in decomp order.
    static const std::pair<eSoundBank, eSoundBankSlot> kRequiredBanks[] = {
        { SND_BANK_GENRL_COLLISIONS,   SND_BANK_SLOT_COLLISIONS },
        { SND_BANK_GENRL_BULLET_HITS,  SND_BANK_SLOT_BULLET_HITS },
        { SND_BANK_GENRL_VEHICLE_GEN,  SND_BANK_SLOT_VEHICLE_GEN },
        { SND_BANK_FEET_GENERIC,       SND_BANK_SLOT_FOOTSTEPS_GENERIC },
        { SND_BANK_GENRL_FRONTEND_GAME, SND_BANK_SLOT_FIRST },
        { SND_BANK_GENRL_EXPLOSIONS,   SND_BANK_SLOT_EXPLOSIONS },
        { SND_BANK_GENRL_WEAPONS,      SND_BANK_SLOT_WEAPON_GEN },
        { SND_BANK_GENRL_DOORS,        SND_BANK_SLOT_DOORS },
        { SND_BANK_GENRL_RAIN,         SND_BANK_SLOT_WEATHER },
        { SND_BANK_GENRL_HORN,         SND_BANK_SLOT_DUMMY_END },
        { SND_BANK_GENRL_SWIMMING,     SND_BANK_SLOT_SWIMMING },
        { SND_BANK_GENRL_APACHE_D,     SND_BANK_SLOT_COP_HELI },
    };
    for (;;) {
        bool allLoaded = true;
        for (const auto& [bank, slot] : kRequiredBanks) {
            if (!AEAudioHardware.IsSoundBankLoaded(bank, slot)) {
                allLoaded = false;
                break;
            }
        }
        if (allLoaded) {
            return;
        }
        AEAudioHardware.Service();
    }
}

// 0x507CB0
void CAudioEngine::Shutdown() {
    if (AERadioTrackManager.IsRadioOn()) {
        AERadioTrackManager.StopRadio(nullptr, true);
    } else {
        // TODO(port): else if (CAEAmbienceTrackManager::IsAmbienceTrackActive()) StopAmbienceTrack(true);
        // TODO(port): else if (CAECutsceneTrackManager::IsCutsceneTrackActive()) CAECutsceneTrackManager::StopCutsceneTrack();
    }
    // TODO(port): CAECollisionAudioEntity::Reset(&m_CollisionAE);
    AERadioTrackManager.Reset();
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_RADIO_RETUNE_STOP_PAUSED, 0.0f, 1.0f);
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_RADIO_RETUNE_STOP, 0.0f, 1.0f);
    // TODO(port): CAEFrontendAudioEntity::Reset(&m_FrontendAE);
    // TODO(port): CAEScriptAudioEntity::Reset(&m_ScriptAE);
    // TODO(port): CAEWeatherAudioEntity::StaticReset();
    // TODO(port): CAEPedSpeechAudioEntity::Reset();
    // TODO(port): if (m_GlobalWeaponAE) {
    // TODO(port):     CAEGlobalWeaponAudioEntity::Destructor(m_GlobalWeaponAE); // uses_Destructor_00506dc0
    // TODO(port):     delete m_GlobalWeaponAE; // incomplete type in this TU
    // TODO(port): }
    CAESoundManager::Reset();
    CAESoundManager::Terminate();
    AEAudioHardware.Terminate();
    delete[] s_pAudioEventVolumes;
    s_pAudioEventVolumes = nullptr;
    // TODO(port): user-radio / QuickTime teardown (Win32-only):
    //   if (cRam00b6b974 && _AEUserRadioTrackManager) CMemoryMgr::Free(_AEUserRadioTrackManager);
    //   CAEWMADecoder::FreeLibrary(); QTMLClient refcount/mutex teardown
    //   (CreateMutexA / WaitForSingleObject / FreeLibrary on the QTML handle).
}

// 0x507A90
void CAudioEngine::Reset() {
    CAESoundManager::Service();
    if (AERadioTrackManager.IsRadioOn()) {
        AERadioTrackManager.StopRadio(nullptr, true);
        while (AERadioTrackManager.IsRadioOn()) {
            const int32_t playTime = AEAudioHardware.GetTrackPlayTime();
            AERadioTrackManager.Service(playTime);
            AEAudioHardware.Service();
        }
    } else {
        // TODO(port): else if (CAEAmbienceTrackManager::IsAmbienceTrackActive()) {
        // TODO(port):     StopAmbienceTrack(true);
        // TODO(port):     CAEAmbienceTrackManager::StopSpecialMissionAmbienceTrack();
        // TODO(port): } else if (CAECutsceneTrackManager::IsCutsceneTrackActive()) {
        // TODO(port):     CAECutsceneTrackManager::StopCutsceneTrack();
        // TODO(port):     drain via CAECutsceneTrackManager::Service + CAEAudioHardware::Service
        // TODO(port): }
    }
    // TODO(port): CAECollisionAudioEntity::Reset(&m_CollisionAE);
    AERadioTrackManager.Reset();
    // TODO(port): CAEAmbienceTrackManager::Reset(&AEAmbienceTrackManager);
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_RADIO_RETUNE_STOP_PAUSED, 0.0f, 1.0f);
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_RADIO_RETUNE_STOP, 0.0f, 1.0f);
    // TODO(port): CAEFrontendAudioEntity::Reset(&m_FrontendAE);
    // TODO(port): CAEScriptAudioEntity::Reset(&m_ScriptAE);
    if (m_GlobalWeaponAE != nullptr) {
        // TODO(port): CAEWeaponAudioEntity::Reset(m_GlobalWeaponAE); // entity not ported
    }
    // TODO(port): CAEWeatherAudioEntity::StaticReset();
    // TODO(port): CAEPedSpeechAudioEntity::Reset();
    // TODO(port): CAEPoliceScannerAudioEntity::Reset();
    CAESoundManager::Reset();
    CAESoundManager::Service();
    CAESoundManager::PauseManually(true);
    m_nCurrentRadioStationId = RADIO_INVALID;
    m_bPlayingMissionCompleteTrack = false;
    m_bStoppingMissionCompleteTrack = false;
}

// 0x506DA0
void CAudioEngine::ResetStatistics() {
    CAERadioTrackManager::ResetStatistics();
}

// 0x507C30
void CAudioEngine::ResetSoundEffects() {
    CAESoundManager::Service();
    // TODO(port): CAECollisionAudioEntity::Reset(&m_CollisionAE);
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_RADIO_RETUNE_STOP_PAUSED, 0.0f, 1.0f);
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_RADIO_RETUNE_STOP, 0.0f, 1.0f);
    // TODO(port): CAEFrontendAudioEntity::Reset(&m_FrontendAE);
    // TODO(port): CAEScriptAudioEntity::Reset(&m_ScriptAE);
    // TODO(port): CAEWeatherAudioEntity::StaticReset();
    // TODO(port): CAEPedSpeechAudioEntity::Reset();
    CAESoundManager::Reset();
    CAESoundManager::Service();
    CAESoundManager::PauseManually(true);
}

// 0x506DB0
void CAudioEngine::Restart() {
    CAESoundManager::PauseManually(false);
}

// 0x506D90
bool CAudioEngine::IsLoadingTuneActive() {
    // TODO(port): return CAEFrontendAudioEntity::IsLoadingTuneActive(&m_FrontendAE);
    return false;
}

// 0x506FD0
bool CAudioEngine::IsRadioOn() {
    return AERadioTrackManager.IsRadioOn();
}

// 0x506FF0
bool CAudioEngine::IsRadioRetuneInProgress() {
    // TODO(port): return CAEFrontendAudioEntity::IsRadioTuneSoundActive(&m_FrontendAE);
    return false;
}

// 0x507050
bool CAudioEngine::IsVehicleRadioActive() {
    return CAERadioTrackManager::IsVehicleRadioActive();
}

// 0x507150
bool CAudioEngine::IsCutsceneTrackActive() {
    // TODO(port): return CAECutsceneTrackManager::IsCutsceneTrackActive(&AECutsceneTrackManager);
    return false;
}

// 0x5071D0
bool CAudioEngine::IsBeatInfoPresent() {
    AEAudioHardware.GetBeatInfo(&m_BeatInfo);
    return m_BeatInfo.IsBeatInfoPresent != 0;
}

// 0x507210
bool CAudioEngine::IsAmbienceTrackActive() {
    // TODO(port): return CAEAmbienceTrackManager::IsAmbienceTrackActive(&AEAmbienceTrackManager);
    return false;
}

// 0x507280
bool CAudioEngine::IsAmbienceRadioActive() {
    // TODO(port): return CAEAmbienceTrackManager::IsAmbienceRadioActive(&AEAmbienceTrackManager);
    return false;
}

// 0x507270
bool CAudioEngine::DoesAmbienceTrackOverrideRadio() {
    // TODO(port): return (bool)AEAmbienceTrackManager; // global manager state byte
    return false;
}

// 0x5072C0
bool CAudioEngine::IsMissionAudioSampleFinished(uint8_t sampleId) {
    // TODO(port): return CAEScriptAudioEntity::IsMissionAudioSampleFinished(&m_ScriptAE, sampleId);
    (void)sampleId;
    return false;
}

// 0x506DE0
void CAudioEngine::SetMusicMasterVolume(int8_t volume) {
    AEAudioHardware.SetMusicMasterScalingFactor(static_cast<float>(volume) * 0.015625f); // /64
}

// 0x506E10
void CAudioEngine::SetEffectsMasterVolume(int8_t volume) {
    AEAudioHardware.SetEffectsMasterScalingFactor(static_cast<float>(volume) * 0.015625f); // /64
}

// 0x506E40
void CAudioEngine::SetMusicFaderScalingFactor(float factor) {
    AEAudioHardware.SetMusicFaderScalingFactor(factor);
}

// 0x506E50
void CAudioEngine::SetEffectsFaderScalingFactor(float factor) {
    AEAudioHardware.SetEffectsFaderScalingFactor(factor);
}

// 0x506E60
void CAudioEngine::SetNonStreamFaderScalingFactor(float factor) {
    AEAudioHardware.SetNonStreamFaderScalingFactor(factor);
}

// 0x506E70
void CAudioEngine::SetStreamFaderScalingFactor(float factor) {
    AEAudioHardware.SetStreamFaderScalingFactor(factor);
}

// 0x506E90
void CAudioEngine::EnableEffectsLoading() {
    AEAudioHardware.EnableEffectsLoading();
}

// 0x506E80
void CAudioEngine::DisableEffectsLoading() {
    AEAudioHardware.DisableEffectsLoading();
}

// 0x507430
void CAudioEngine::PauseAllSounds() {
    AEAudioHardware.PauseAllSounds();
}

// 0x507440
void CAudioEngine::ResumeAllSounds() {
    AEAudioHardware.ResumeAllSounds();
}

// 0x5078A0
void CAudioEngine::ServiceLoadingTune(float target) {
    const float current = AEAudioHardware.GetEffectsFaderScalingFactor();
    if (current < target && current + 0.005f < target) {
        target = current + 0.005f; // fade up at most 0.005 per service
    }
    AEAudioHardware.SetEffectsFaderScalingFactor(target);
    CAESoundManager::Service();
}

// 0x507410
void CAudioEngine::StartLoadingTune() {
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_LOADING_TUNE_START, 0.0f, 1.0f);
    CAESoundManager::Service();
}

// 0x506EB0
void CAudioEngine::ReportCollision(CEntity* entity1, CEntity* entity2, eSurfaceType surf1,
    eSurfaceType surf2, const CVector& pos, const CVector* normal, float fCollisionImpact1,
    float fCollisionImpact2, bool playOnlyOneShotCollisionSound, bool unknown) {
    // TODO(port): CAECollisionAudioEntity::ReportCollision(&m_CollisionAE, entity1, entity2,
    //   surf1, surf2, pos, normal, fCollisionImpact1, fCollisionImpact2,
    //   playOnlyOneShotCollisionSound, unknown);
    (void)entity1; (void)entity2; (void)surf1; (void)surf2; (void)pos; (void)normal;
    (void)fCollisionImpact1; (void)fCollisionImpact2;
    (void)playOnlyOneShotCollisionSound; (void)unknown;
}

// 0x506EC0
void CAudioEngine::ReportBulletHit(CEntity* entity, eSurfaceType surface, const CVector& posn,
    float angleWithColPointNorm) {
    // TODO(port): CAECollisionAudioEntity::ReportBulletHit(&m_CollisionAE, entity, surface, posn, angleWithColPointNorm);
    (void)entity; (void)surface; (void)posn; (void)angleWithColPointNorm;
}

// 0x506ED0
void CAudioEngine::ReportObjectDestruction(CEntity* entity) {
    // TODO(port): CAECollisionAudioEntity::ReportObjectDestruction(&m_CollisionAE, entity);
    (void)entity;
}

// 0x506EE0
void CAudioEngine::ReportGlassCollisionEvent(eAudioEvents glassSoundType, const CVector& posn) {
    // TODO(port): CAECollisionAudioEntity::ReportGlassCollisionEvent(&m_CollisionAE, glassSoundType, posn, 0);
    (void)glassSoundType; (void)posn;
}

// 0x506EF0
void CAudioEngine::ReportWaterSplash(CVector posn, float volume) {
    // TODO(port): CAECollisionAudioEntity::ReportWaterSplash(&m_CollisionAE, posn, volume);
    (void)posn; (void)volume;
}

// 0x506F00
void CAudioEngine::ReportWaterSplash(CPhysical* physical, float volume, bool forcePlaySplashSound) {
    // TODO(port): CAECollisionAudioEntity::ReportWaterSplash(&m_CollisionAE, physical, volume, forcePlaySplashSound);
    (void)physical; (void)volume; (void)forcePlaySplashSound;
}

// 0x506F40
void CAudioEngine::ReportWeaponEvent(int32_t audioEvent, eWeaponType weaponType, CPhysical* physical) {
    // TODO(port): CAEGlobalWeaponAudioEntity::AddAudioEvent(m_GlobalWeaponAE, audioEvent, weaponType, physical);
    (void)audioEvent; (void)weaponType; (void)physical;
}

// 0x506F50
void CAudioEngine::ReportDoorMovement(CPhysical* physical) {
    // TODO(port): CAEDoorAudioEntity::AddAudioEvent(&m_DoorAE, AE_ENTRY_EXIT_DOOR_MOVING, physical);
    (void)physical;
}

// 0x507340
void CAudioEngine::ReportMissionAudioEvent(uint16_t eventId, CVector& posn) {
    // TODO(port): CAEScriptAudioEntity::ReportMissionAudioEvent(&m_ScriptAE, eventId, posn);
    (void)eventId; (void)posn;
}

// 0x507350
void CAudioEngine::ReportMissionAudioEvent(uint16_t eventId, CObject* object) {
    // TODO(port): CAEScriptAudioEntity::ReportMissionAudioEvent(&m_ScriptAE, eventId, (CPhysical*)object, 0.0f, 1.0f);
    (void)eventId; (void)object;
}

// 0x507370
void CAudioEngine::ReportMissionAudioEvent(uint16_t eventId, CPed* ped) {
    // TODO(port): CAEScriptAudioEntity::ReportMissionAudioEvent(&m_ScriptAE, eventId, (CPhysical*)ped, 0.0f, 1.0f);
    (void)eventId; (void)ped;
}

// 0x507390
void CAudioEngine::ReportMissionAudioEvent(uint16_t eventId, CVehicle* vehicle) {
    // TODO(port): CAEScriptAudioEntity::ReportMissionAudioEvent(&m_ScriptAE, eventId, (CPhysical*)vehicle, 0.0f, 1.0f);
    (void)eventId; (void)vehicle;
}

// 0x5073A0 (5th overload: eventId, physical, volume, speed)
void CAudioEngine::ReportMissionAudioEvent(uint16_t eventId, CPhysical* physical, float volume, float speed) {
    // TODO(port): CAEScriptAudioEntity::ReportMissionAudioEvent(&m_ScriptAE, eventId, physical, volume, speed);
    (void)eventId; (void)physical; (void)volume; (void)speed;
}

// 0x506EA0
void CAudioEngine::ReportFrontendAudioEvent(eAudioEvents eventId, float volumeChange, float speed) {
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, eventId, volumeChange, speed);
    (void)eventId; (void)volumeChange; (void)speed;
}

// 0x506FC0
void CAudioEngine::InitialiseRadioStationID(eRadioID id) {
    AERadioTrackManager.InitialiseRadioStationID(id);
}

// 0x507DF0
void CAudioEngine::StartRadio(const tVehicleAudioSettings& settings) {
    // TODO(port): gate: only when CAECutsceneTrackManager::GetCutsceneTrackStatus() == 0
    AERadioTrackManager.StartRadio(settings);
}

// 0x507DC0
void CAudioEngine::StartRadio(eRadioID id, eBassSetting bassSetting) {
    // TODO(port): gate: only when CAECutsceneTrackManager::GetCutsceneTrackStatus() == 0
    AERadioTrackManager.StartRadio(id, bassSetting, 0.0f, false);
}

// 0x506F70
void CAudioEngine::StopRadio(tVehicleAudioSettings* settings, bool bDuringPause) {
    AERadioTrackManager.StopRadio(settings, bDuringPause);
}

// 0x506F80
void CAudioEngine::SetRadioAutoRetuneOnOff(bool enable) {
    AERadioTrackManager.SetRadioAutoRetuneOnOff(enable);
}

// 0x506F90
void CAudioEngine::SetBassEnhanceOnOff(bool enable) {
    AERadioTrackManager.SetBassEnhanceOnOff(enable);
}

// 0x506FA0
void CAudioEngine::SetRadioBassSetting(eBassSetting bassSetting) {
    AERadioTrackManager.SetBassSetting(bassSetting, 1.0f);
}

// 0x506FE0
bool CAudioEngine::HasRadioRetuneJustStarted() {
    return AERadioTrackManager.HasRadioRetuneJustStarted();
}

// 0x507000
const GxtChar* CAudioEngine::GetRadioStationName(eRadioID id) {
    return AERadioTrackManager.GetRadioStationName(id);
}

// 0x507010
void CAudioEngine::GetRadioStationNameKey(eRadioID id, char* outStr) {
    AERadioTrackManager.GetRadioStationNameKey(id, outStr);
}

// 0x507020
int32_t* CAudioEngine::GetRadioStationListenTimes() {
    return AERadioTrackManager.GetRadioStationListenTimes();
}

// 0x507030
void CAudioEngine::DisplayRadioStationName() {
    AERadioTrackManager.DisplayRadioStationName();
}

// 0x507040
eRadioID CAudioEngine::GetCurrentRadioStationID() {
    // Decomp reads the radio manager's current-station global (0x8CB7E1).
    if (s_nCurrentRadioStation == RADIO_INVALID) {
        return RADIO_OFF;
    }
    return s_nCurrentRadioStation;
}

// 0x507060 - empty in the original
void CAudioEngine::PlayRadioAnnouncement(uint32_t trackId) {
    (void)trackId;
}

// 0x507E10
void CAudioEngine::RetuneRadio(eRadioID id) {
    // TODO(port): gate: only when CAECutsceneTrackManager::GetCutsceneTrackStatus() == 0
    AERadioTrackManager.RetuneRadio(id);
}

// 0x507E30
void CAudioEngine::PreloadCutsceneTrack(int16_t trackId, bool wait) {
    if (AERadioTrackManager.IsRadioOn()) {
        m_nCurrentRadioStationId = AERadioTrackManager.GetCurrentRadioStationID();
        // TODO(port): settings = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio();
        // TODO(port): AERadioTrackManager.StopRadio(settings, true);
        // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_RADIO_RETUNE_STOP, 0.0f, 1.0f);
        while (AERadioTrackManager.IsRadioOn()) {
            const int32_t playTime = AEAudioHardware.GetTrackPlayTime();
            AERadioTrackManager.Service(playTime);
            AEAudioHardware.Service();
        }
    } else {
        // TODO(port): else if (CAEAmbienceTrackManager::IsAmbienceTrackActive()) StopAmbienceTrack(true);
        // TODO(port): else {
        // TODO(port):     if (CAECutsceneTrackManager::IsCutsceneTrackActive())
        // TODO(port):         CAECutsceneTrackManager::StopCutsceneTrack();
        // TODO(port):     drain via CAECutsceneTrackManager::Service + CAEAudioHardware::Service
        // TODO(port): }
    }
    // TODO(port): CAECutsceneTrackManager preload: track id = trackId, state 0, status 0
    //   (decomp writes _DAT_008ae560 = trackId, _DAT_008ae568 = 0, _DAT_008ae55c = 0).
    // TODO(port): if (wait) drain CAECutsceneTrackManager::Service until status == 2.
    (void)trackId; (void)wait;
}

// 0x507070
void CAudioEngine::PlayPreloadedCutsceneTrack() {
    // TODO(port): CAECutsceneTrackManager::PlayPreloadedCutsceneTrack(&AECutsceneTrackManager);
}

// 0x507080
void CAudioEngine::StopCutsceneTrack(bool waitForStop) {
    // TODO(port): CAECutsceneTrackManager::StopCutsceneTrack(&AECutsceneTrackManager);
    // TODO(port): if (waitForStop) drain via CAECutsceneTrackManager::Service + CAEAudioHardware::Service
    // TODO(port):   while CAECutsceneTrackManager::IsCutsceneTrackActive().
    if (waitForStop) {
        if (RADIO_INVALID < m_nCurrentRadioStationId) {
            if (CAERadioTrackManager::IsVehicleRadioActive()) {
                // eBassSetting::NORMAL == 0 (full enum in CAERadioTrackManager.h,
                // not included here due to the eRadioID duplication - see top).
                AERadioTrackManager.StartRadio(m_nCurrentRadioStationId,
                    static_cast<eBassSetting>(0), 0.0f, false);
            }
            m_nCurrentRadioStationId = RADIO_INVALID;
            m_bPlayingMissionCompleteTrack = false;
            return;
        }
        if (CAERadioTrackManager::IsVehicleRadioActive()) {
            // TODO(port): settings = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio();
            // TODO(port): AERadioTrackManager.StartRadio(*settings);
            m_bPlayingMissionCompleteTrack = false;
            return;
        }
    }
}

// 0x507160
int8_t CAudioEngine::GetCutsceneTrackStatus() {
    // TODO(port): return CAECutsceneTrackManager::GetCutsceneTrackStatus(&AECutsceneTrackManager);
    return 0;
}

// 0x507170
int8_t CAudioEngine::GetBeatTrackStatus() {
    // TODO(port): return CAECutsceneTrackManager::GetCutsceneTrackStatus(&AECutsceneTrackManager);
    return 0;
}

// 0x507180
void CAudioEngine::PlayPreloadedBeatTrack(bool playMissionComplete) {
    // TODO(port): CAECutsceneTrackManager::PlayPreloadedCutsceneTrack(&AECutsceneTrackManager);
    m_bPlayingMissionCompleteTrack = playMissionComplete;
}

// 0x5071A0
void CAudioEngine::StopBeatTrack() {
    StopCutsceneTrack(true);
}

// 0x5071B0
tBeatInfo* CAudioEngine::GetBeatInfo() {
    AEAudioHardware.GetBeatInfo(&m_BeatInfo);
    return &m_BeatInfo;
}

// 0x507200
void CAudioEngine::PauseBeatTrack(bool pause) {
    // TODO(port): CAECutsceneTrackManager::PauseTrack(&AECutsceneTrackManager, pause);
    (void)pause;
}

// 0x507F40
void CAudioEngine::PreloadBeatTrack(int16_t trackId) {
    // Same radio/ambience/cutscene teardown as PreloadCutsceneTrack (see above).
    if (AERadioTrackManager.IsRadioOn()) {
        m_nCurrentRadioStationId = AERadioTrackManager.GetCurrentRadioStationID();
        // TODO(port): settings = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio();
        // TODO(port): AERadioTrackManager.StopRadio(settings, true);
        // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_RADIO_RETUNE_STOP, 0.0f, 1.0f);
        while (AERadioTrackManager.IsRadioOn()) {
            const int32_t playTime = AEAudioHardware.GetTrackPlayTime();
            AERadioTrackManager.Service(playTime);
            AEAudioHardware.Service();
        }
    } else {
        // TODO(port): ambience/cutscene stop-drain (as in PreloadCutsceneTrack).
    }
    // TODO(port): CAECutsceneTrackManager::PreloadBeatTrack(&AECutsceneTrackManager, trackId, false);
    (void)trackId;
}

// 0x507220
void CAudioEngine::StopAmbienceTrack(bool waitForStop) {
    // TODO(port): CAEAmbienceTrackManager::StopAmbienceTrack();
    if (waitForStop) {
        // TODO(port): while (CAEAmbienceTrackManager::IsAmbienceTrackActive()) {
        // TODO(port):     const int32_t playTime = AEAudioHardware.GetTrackPlayTime();
        // TODO(port):     CAEAmbienceTrackManager::Service(&AEAmbienceTrackManager, playTime);
        // TODO(port):     AEAudioHardware.Service();
        // TODO(port): }
    }
}

// 0x507290
void CAudioEngine::PreloadMissionAudio(uint8_t slotId, int32_t scriptSlotAudioEvent) {
    // TODO(port): CAEScriptAudioEntity::PreloadMissionAudio(&m_ScriptAE, slotId, scriptSlotAudioEvent);
    (void)slotId; (void)scriptSlotAudioEvent;
}

// 0x5072A0
int8_t CAudioEngine::GetMissionAudioLoadingStatus(uint8_t slotId) {
    // TODO(port): return CAEScriptAudioEntity::GetMissionAudioLoadingStatus(&m_ScriptAE, slotId);
    (void)slotId;
    return 0;
}

// 0x5072B0
void CAudioEngine::PlayLoadedMissionAudio(uint8_t slotId) {
    // TODO(port): CAEScriptAudioEntity::PlayLoadedMissionAudio(&m_ScriptAE, slotId);
    (void)slotId;
}

// 0x5072D0
int32_t CAudioEngine::GetMissionAudioEvent(uint8_t slotId) {
    // TODO(port): return CAEScriptAudioEntity::GetMissionAudioEvent(&m_ScriptAE, slotId);
    (void)slotId;
    return 0;
}

// 0x5072E0
CVector* CAudioEngine::GetMissionAudioPosition(uint8_t slotId) {
    // TODO(port): return CAEScriptAudioEntity::GetMissionAudioPosition(&m_ScriptAE, slotId);
    (void)slotId;
    return nullptr;
}

// 0x5072F0
void CAudioEngine::ClearMissionAudio(uint8_t slotId) {
    // TODO(port): CAEScriptAudioEntity::ClearMissionAudio(&m_ScriptAE, slotId);
    (void)slotId;
}

// 0x507300
void CAudioEngine::SetMissionAudioPosition(uint8_t slotId, CVector& posn) {
    // TODO(port): CAEScriptAudioEntity::SetMissionAudioPosition(&m_ScriptAE, slotId, posn);
    (void)slotId; (void)posn;
}

// 0x507310
CVector* CAudioEngine::AttachMissionAudioToPed(uint8_t slotId, CPed* ped) {
    // TODO(port): return CAEScriptAudioEntity::AttachMissionAudioToPed(&m_ScriptAE, slotId, ped);
    (void)slotId; (void)ped;
    return nullptr;
}

// 0x507320
CVector* CAudioEngine::AttachMissionAudioToObject(uint8_t slotId, CObject* object) {
    // TODO(port): return CAEScriptAudioEntity::AttachMissionAudioToPhysical(&m_ScriptAE, slotId, object);
    (void)slotId; (void)object;
    return nullptr;
}

// 0x507330
CVector* CAudioEngine::AttachMissionAudioToPhysical(uint8_t slotId, CPhysical* physical) {
    // TODO(port): return CAEScriptAudioEntity::AttachMissionAudioToPhysical(&m_ScriptAE, slotId, physical);
    (void)slotId; (void)physical;
    return nullptr;
}

// 0x5073C0
void CAudioEngine::SayPedless(eAudioEvents audioEvent, eGlobalSpeechContext gCtx, CEntity* attachTo,
    uint32_t startTimeDelayMs, float probability, bool overrideSilence, bool isForceAudible,
    bool isFrontEnd) {
    // TODO(port): CAEPedlessSpeechAudioEntity::AddSayEvent(&m_PedlessSpeechAE, audioEvent, gCtx,
    //   attachTo, startTimeDelayMs, probability, overrideSilence, isForceAudible, isFrontEnd);
    (void)audioEvent; (void)gCtx; (void)attachTo; (void)startTimeDelayMs; (void)probability;
    (void)overrideSilence; (void)isForceAudible; (void)isFrontEnd;
}

// NOTE: the header declares EnablePoliceScanner(), but no decomp source exists
// for it in src/CAudioEngine/ (only Disable/Stop). Left unimplemented.
// TODO: locate the original (possibly inlined or in another TU).
void CAudioEngine::EnablePoliceScanner() {
}

// 0x5073D0
void CAudioEngine::DisablePoliceScanner(uint8_t arg1, uint8_t arg2) {
    // TODO(port): CAEPoliceScannerAudioEntity::DisableScanner(arg1, arg2);
    (void)arg1; (void)arg2;
}

// 0x507400
void CAudioEngine::StopPoliceScanner(uint8_t arg1) {
    // TODO(port): CAEPoliceScannerAudioEntity::StopScanner(arg1);
    (void)arg1;
}

// 0x507750 - per-frame orchestration
void CAudioEngine::Service() {
    // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_WAKEUP_AMPLIFIER, 0.0f, 1.0f);
    if (!CTimer::m_UserPause && !CTimer::m_CodePause) {
        // TODO(port): CAEFrontendAudioEntity::AddAudioEvent(&m_FrontendAE, AE_FRONTEND_RADIO_RETUNE_STOP_PAUSED, 0.0f, 1.0f);
    }

    const int32_t trackPlayTime = AEAudioHardware.GetTrackPlayTime();
    // TODO(port): AEAudioHardware.GetChannelPlayTimes(m_nBackgroundAudioChannel, nullptr);

    // Mission-complete track state machine (trackPlayTime -4/-6 are
    // CAEAudioHardware end-of-track sentinel values).
    if (!m_bPlayingMissionCompleteTrack || trackPlayTime != -4) {
        if (m_bStoppingMissionCompleteTrack && trackPlayTime == -6) {
            m_bStoppingMissionCompleteTrack = false;
            if (CAERadioTrackManager::IsVehicleRadioActive()) {
                // TODO(port): settings = CAEVehicleAudioEntity::StaticGetPlayerVehicleAudioSettingsForRadio();
                // TODO(port): AERadioTrackManager.StartRadio(*settings);
            }
        }
    } else {
        m_bStoppingMissionCompleteTrack = true;
    }

    // TODO(port): CAECutsceneTrackManager::Service(&AECutsceneTrackManager, trackPlayTime);
    // TODO(port): CAEAmbienceTrackManager::Service(&AEAmbienceTrackManager, trackPlayTime);
    AERadioTrackManager.Service(trackPlayTime);
    // TODO(port): if (FindPlayerPed(0)) CAEGlobalWeaponAudioEntity::ServiceAmbientGunFire(m_GlobalWeaponAE);
    // TODO(port): CAECollisionAudioEntity::Service(&m_CollisionAE);

    float streamFactor;
    if (!CTimer::m_UserPause && !CTimer::m_CodePause) {
        // TODO(port): streamFactor = CCutsceneMgr::ms_running ? 0.0f : 1.0f;
        streamFactor = 1.0f;
    } else {
        AEAudioHardware.SetMusicFaderScalingFactor(1.0f);
        AEAudioHardware.SetEffectsFaderScalingFactor(1.0f);
        streamFactor = 1.0f;
    }
    AEAudioHardware.SetStreamFaderScalingFactor(streamFactor);

    // TODO(port): CAEScriptAudioEntity::Service(&m_ScriptAE);
    // TODO(port): CAEPedSpeechAudioEntity::Service();
    CAESoundManager::Service();

    // TODO(port): police-scanner state machine
    //   (CAEPoliceScannerAudioEntity statics: s_bStoppingScanner,
    //   s_nAbortPlaybackTime, s_nPlaybackStartTime, s_pPSControlling,
    //   s_fVolumeOffset; states in _s_nScannerPlaybackState 2/4/5/7 with the
    //   DAT_00b6f065 enable flag and DAT_008c8154/58/5c timing constants).
    //   Decomp: src/CAudioEngine/Service_00507750.c tail.
}
