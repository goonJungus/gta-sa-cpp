// CAudioEngine.h - GTA SA 1.0 clean-room C++ conversion.
// Adapted from gta-reversed Audio/AudioEngine.h (+ tBeatInfo, eRadioID).
// This file is part of a clean-room engine reimplementation for
// interoperability research. Game assets load from the user's own install
// at runtime and are never shipped.
//
// Porting notes:
// - eRadioID lives in the canonical eRadioID.h (deduped 2026-10-09; it was
//   previously defined here, in CAERadioTrackManager.h, CPedModelInfo.h and
//   CStats.h).
// - tBeatInfo (0xAC) is defined here: the original declares it in
//   AudioEngine.h and CAEAudioHardware.h includes this header for it.
// - tVehicleAudioSettings stays forward-declared until the audio entities
//   land (same as CAERadioTrackManager.h).
// - GxtChar comes from RenderTypes.h.
// - CAERadioTrackManager.h is deliberately NOT included: it defines its own
//   `using GxtChar = uint8_t` and a scoped eBassSetting. CAudioEngine.cpp
//   uses a local shim class mirroring it instead.
//   TODO(port): shared audio-enums header, then include the real manager
//   header and delete the shim in CAudioEngine.cpp.
// - Rewritten 2026-10-09: the minimal stub (ReportWeaponEvent/
//   ReportWaterSplash only) broke CAudioEngine.cpp with 100 errors; this
//   restores the full class declaration the .cpp was written against.

#pragma once

#include <cstdint>

#include "eRadioID.h"    // eRadioID, RADIO_OFF, RADIO_INVALID (canonical)
#include "RenderTypes.h" // GxtChar

class CEntity;
class CPhysical;
class CObject;
class CPed;
class CVehicle;
class CVector;

// Audio entities (full ports pending). Only the global weapon entity pointer
// is named by the ported method bodies, so only it is forward-declared.
class CAEGlobalWeaponAudioEntity;

// Small audio enums. Larger enums stay opaque until ported; underlying types
// match the existing forward declarations (CPhysical.h, CPed.h, CAESound.h).
// eBassSetting's full definition (scoped, NORMAL/BOOST/CUT) lives in
// CAEAudioHardware.h, which includes this header - so it is only
// forward-declared here to avoid a redefinition.
enum class eBassSetting : int8_t;
enum eSurfaceType : int32_t;         // values in CPhysical.h
enum eAudioEvents : int32_t;         // full enum not ported (see CAESound.h)
enum eWeaponType : uint32_t;         // values in eWeaponType.h / CPhysical.h
enum eGlobalSpeechContext : int16_t; // values in PedSpeechContexts.h (not ported)

// Weapon audio events (gta-reversed Audio/Enums/eWeaponAudioEvent.h - partial).
// TODO(port): verify values vs gta-reversed, full enum.
enum eWeaponAudioEvent : int32_t {
    AE_WEAPON_FIRE = 0,
};

struct tVehicleAudioSettings; // Audio/Entities/AEVehicleAudioEntity.h - port with audio entities

// tBeatInfo: beat window for the currently playing streamed track (0xAC).
// The original declares it in AudioEngine.h; CAEAudioHardware.h includes this
// header for it (it must stay the single definition - a duplicate in
// CAEAudioHardware.h breaks both TUs with C2011).
// Layout from decomp src/CAEAudioHardware/GetBeatInfo_004d8fa0.c:
// BeatWindow[20] sits at offset 0 (the 0x28-iteration zero loop starts at
// &gBeatInfo, 8 bytes per entry), followed by 3 int32s; total 0xAC.
struct tBeatInfo {
    struct tBeatWindowEntry {
        int32_t  m_nTime;
        uint32_t m_nKey;
    };
    tBeatWindowEntry BeatWindow[20];
    int32_t          IsBeatInfoPresent;
    int32_t          BeatTypeThisFrame;
    int32_t          BeatNumber;
};
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tBeatInfo) == 0xAC, "tBeatInfo layout changed");
#endif

class CAudioEngine {
public:
    CAudioEngine() = default;
    ~CAudioEngine() = default;

    bool Initialise();
    void InitialisePostLoading();
    void Shutdown();
    void Reset();
    void ResetStatistics();
    void ResetSoundEffects();
    void Restart();
    bool IsLoadingTuneActive();
    bool IsRadioOn();
    bool IsRadioRetuneInProgress();
    bool IsVehicleRadioActive();
    bool IsCutsceneTrackActive();
    bool IsBeatInfoPresent();
    bool IsAmbienceTrackActive();
    bool IsAmbienceRadioActive();
    bool DoesAmbienceTrackOverrideRadio();
    bool IsMissionAudioSampleFinished(uint8_t sampleId);
    void SetMusicMasterVolume(int8_t volume);
    void SetEffectsMasterVolume(int8_t volume);
    void SetMusicFaderScalingFactor(float factor);
    void SetEffectsFaderScalingFactor(float factor);
    void SetNonStreamFaderScalingFactor(float factor);
    void SetStreamFaderScalingFactor(float factor);
    void EnableEffectsLoading();
    void DisableEffectsLoading();
    void PauseAllSounds();
    void ResumeAllSounds();
    void ServiceLoadingTune(float target);
    void StartLoadingTune();
    void ReportCollision(CEntity* entity1, CEntity* entity2, eSurfaceType surf1,
        eSurfaceType surf2, const CVector& pos, const CVector* normal,
        float fCollisionImpact1, float fCollisionImpact2,
        bool playOnlyOneShotCollisionSound, bool unknown);
    void ReportBulletHit(CEntity* entity, eSurfaceType surface, const CVector& posn,
        float angleWithColPointNorm);
    void ReportObjectDestruction(CEntity* entity);
    void ReportGlassCollisionEvent(eAudioEvents glassSoundType, const CVector& posn);
    void ReportWaterSplash(CVector posn, float volume);
    void ReportWaterSplash(CPhysical* physical, float volume, bool forcePlaySplashSound);
    void ReportWeaponEvent(int32_t audioEvent, eWeaponType weaponType, CPhysical* physical);
    void ReportDoorMovement(CPhysical* physical);
    void ReportMissionAudioEvent(uint16_t eventId, CVector& posn);
    void ReportMissionAudioEvent(uint16_t eventId, CObject* object);
    void ReportMissionAudioEvent(uint16_t eventId, CPed* ped);
    void ReportMissionAudioEvent(uint16_t eventId, CVehicle* vehicle);
    void ReportMissionAudioEvent(uint16_t eventId, CPhysical* physical, float volume, float speed);
    void ReportFrontendAudioEvent(eAudioEvents eventId, float volumeChange, float speed);
    void InitialiseRadioStationID(eRadioID id);
    void StartRadio(const tVehicleAudioSettings& settings);
    void StartRadio(eRadioID id, eBassSetting bassSetting);
    void StopRadio(tVehicleAudioSettings* settings, bool bDuringPause);
    void SetRadioAutoRetuneOnOff(bool enable);
    void SetBassEnhanceOnOff(bool enable);
    void SetRadioBassSetting(eBassSetting bassSetting);
    bool HasRadioRetuneJustStarted();
    const GxtChar* GetRadioStationName(eRadioID id);
    void GetRadioStationNameKey(eRadioID id, char* outStr);
    int32_t* GetRadioStationListenTimes();
    void DisplayRadioStationName();
    eRadioID GetCurrentRadioStationID();
    void PlayRadioAnnouncement(uint32_t trackId);
    void RetuneRadio(eRadioID id);
    void PreloadCutsceneTrack(int16_t trackId, bool wait);
    void PlayPreloadedCutsceneTrack();
    void StopCutsceneTrack(bool waitForStop);
    int8_t GetCutsceneTrackStatus();
    int8_t GetBeatTrackStatus();
    void PlayPreloadedBeatTrack(bool playMissionComplete);
    void StopBeatTrack();
    tBeatInfo* GetBeatInfo();
    void PauseBeatTrack(bool pause);
    void PreloadBeatTrack(int16_t trackId);
    void StopAmbienceTrack(bool waitForStop);
    void PreloadMissionAudio(uint8_t slotId, int32_t scriptSlotAudioEvent);
    int8_t GetMissionAudioLoadingStatus(uint8_t slotId);
    void PlayLoadedMissionAudio(uint8_t slotId);
    int32_t GetMissionAudioEvent(uint8_t slotId);
    CVector* GetMissionAudioPosition(uint8_t slotId);
    void ClearMissionAudio(uint8_t slotId);
    void SetMissionAudioPosition(uint8_t slotId, CVector& posn);
    CVector* AttachMissionAudioToPed(uint8_t slotId, CPed* ped);
    CVector* AttachMissionAudioToObject(uint8_t slotId, CObject* object);
    CVector* AttachMissionAudioToPhysical(uint8_t slotId, CPhysical* physical);
    void SayPedless(eAudioEvents audioEvent, eGlobalSpeechContext gCtx, CEntity* attachTo,
        uint32_t startTimeDelayMs, float probability, bool overrideSilence,
        bool isForceAudible, bool isFrontEnd);
    void EnablePoliceScanner();
    void DisablePoliceScanner(uint8_t arg1, uint8_t arg2);
    void StopPoliceScanner(uint8_t arg1);
    void Service();

private:
    // Members referenced by the ported method bodies. The audio-entity
    // members (m_FrontendAE @0xB4, m_ScriptAE @0x2A0, m_CollisionAE @0x4BC,
    // m_PedlessSpeechAE @0x1E38, m_DoorAE @0x1F50) come back with the entity
    // ports; until then the class is NOT size-verified (was 0x1FD8).
    int16_t  m_nBackgroundAudioChannel;
    eRadioID m_nCurrentRadioStationId;
    bool     m_bPlayingMissionCompleteTrack;
    bool     m_bStoppingMissionCompleteTrack;
    tBeatInfo m_BeatInfo;
    CAEGlobalWeaponAudioEntity* m_GlobalWeaponAE;
};

// Global audio engine (gta-reversed).
// TODO(port): verify address.
extern CAudioEngine AudioEngine;
