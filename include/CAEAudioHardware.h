// CAEAudioHardware - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Audio/Hardware/AEAudioHardware.h
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks() and the private Constructor()/Destructor()
//   hook wrappers; VALIDATE_SIZE replaced with guarded static_assert (active on
//   32-bit targets only); `extern CAEAudioHardware& AEAudioHardware`
//   (StaticRef 0xB5F8B8) replaced with a plain extern object defined in
//   CAEAudioHardware.cpp; `#include "dsound.h"` replaced with forward-declared
//   COM interfaces (IDirectSound8, IDirectSound3DListener) and a locally defined
//   DSCAPS struct (24 DWORDs = 0x60, verified against the 0x1014 class size);
//   CAEStreamThread member replaced with a size-verified opaque stand-in
//   (0x50); tVirtualChannelSettings defined locally (port of
//   Audio/Loaders/AEMPP3BankLoader.h's struct, rng::fill -> std::fill);
//   private GetChannels() std::span helper (C++20) replaced with a C++17
//   pointer accessor; "Return types aren't real" signatures kept as declared
//   with TODOs.
// Integer types converted to <cstdint>.
// TODO: verify each method against decomp src/CAEAudioHardware/*.c

#pragma once

#include "CVector.h"      // CVector (SetChannelPosition)
#include "CAudioEngine.h"
#include "CAESound.h"     // CAESound (CAEAudioHardwarePlayFlags::CopyFromAESound), eSoundID

#include <algorithm>
#include <array>
#include <cstdint>

class CAEStreamingChannel;
class CAEMP3BankLoader;
class CAEMP3TrackLoader;
class CAEAudioChannel;
class CAEBankSlot;

// COM audio interfaces from dsound.h - forward-declared so the clean-room
// build doesn't need the DirectSound SDK headers.
struct IDirectSound8;
struct IDirectSound3DListener;

// DirectSound DSCAPS (dsound.h): 24 DWORDs = 0x60 bytes.
struct DSCAPS {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwMinSecondarySampleRate;
    uint32_t dwMaxSecondarySampleRate;
    uint32_t dwPrimaryBuffers;
    uint32_t dwMaxHwMixingAllBuffers;
    uint32_t dwMaxHwMixingStaticBuffers;
    uint32_t dwMaxHwMixingStreamingBuffers;
    uint32_t dwFreeHwMixingAllBuffers;
    uint32_t dwFreeHwMixingStaticBuffers;
    uint32_t dwFreeHwMixingStreamingBuffers;
    uint32_t dwMaxHw3DAllBuffers;
    uint32_t dwMaxHw3DStaticBuffers;
    uint32_t dwMaxHw3DStreamingBuffers;
    uint32_t dwFreeHw3DAllBuffers;
    uint32_t dwFreeHw3DStaticBuffers;
    uint32_t dwFreeHw3DStreamingBuffers;
    uint32_t dwTotalHwMemBytes;
    uint32_t dwFreeHwMemBytes;
    uint32_t dwMaxContigFreeHwMemBytes;
    uint32_t dwUnlockTransferRateHwBuffers;
    uint32_t dwPlayCpuOverheadSwBuffers;
    uint32_t dwReserved1;
    uint32_t dwReserved2;
};
static_assert(sizeof(DSCAPS) == 0x60, "DSCAPS layout changed");

enum eSoundBank : int16_t;      // Audio/Enums/eSoundBank.h (full enum not ported)
enum eSoundBankSlot : int16_t;  // Audio/Enums/eSoundBankSlot.h (full enum not ported)

enum class eBassSetting : int8_t {
    NORMAL = 0,
    BOOST  = 1,
    CUT    = 2,
};

static constexpr int32_t MAX_NUM_AUDIO_CHANNELS = 64; // Audio/Managers/AESoundManager.h
static constexpr int32_t MAX_NUM_SOUNDS = 300;        // Audio/Managers/AESoundManager.h

// Port of Audio/Loaders/AEMP3BankLoader.h's tVirtualChannelSettings
// (rng::fill replaced with std::fill).
struct tVirtualChannelSettings {
    std::array<int16_t, MAX_NUM_SOUNDS> BankSlotIDs{}; // eSoundBankSlot
    std::array<int16_t, MAX_NUM_SOUNDS> SoundIDs{};    // eSoundID

    tVirtualChannelSettings() {
        // orig: rng::fill(BankSlotIDs, SND_BANK_SLOT_NONE); rng::fill(SoundIDs, -1);
        std::fill(BankSlotIDs.begin(), BankSlotIDs.end(), int16_t(-1)); // SND_BANK_SLOT_NONE
        std::fill(SoundIDs.begin(), SoundIDs.end(), int16_t(-1));
    }
};
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tVirtualChannelSettings) == 0x4B0, "tVirtualChannelSettings layout changed");
#endif

union CAEAudioHardwarePlayFlags {
    uint16_t m_nFlags{};
    struct {
        uint16_t m_bIsFrontend : 1;
        uint16_t m_bIsUncompressable : 1;
        uint16_t m_bIsUnduckable : 1;
        uint16_t m_bIsStartPercentage : 1;
        uint16_t m_bIsMusicMastered : 1;
        uint16_t : 1;
        uint16_t m_bIsRolledOff : 1;
        uint16_t m_bIsSmoothDucking : 1;

        uint16_t m_bIsForcedFront : 1;
        uint16_t m_IsPausable : 1;
    };

    void CopyFromAESound(const CAESound& sound) {
        m_bIsFrontend        = sound.IsFrontEnd();
        m_bIsUncompressable  = sound.IsIncompressible();
        m_bIsUnduckable      = sound.IsUnduckable();
        m_bIsStartPercentage = sound.GetPlayTimeIsPercentage();
        m_bIsMusicMastered   = sound.IsMusicMastered();
        m_bIsRolledOff       = sound.GetRolledOff();
        m_bIsSmoothDucking   = sound.GetSmoothDucking();
        m_bIsForcedFront     = sound.IsForcedFront();
        m_IsPausable         = m_bIsFrontend && sound.IsUnpausable();
    }
};

// Bit flags stored in `CAEAudioHardware::m_awChannelFlags` (semantic names per gta-reversed#1225)
enum eAudioChannelFlags : int16_t {
    FLAG_SECONDARY_GROUP  = 0x01, // rescale via a separate group (ambient, radio, cutscene)
    FLAG_UNDUCKABLE       = 0x02, // can't be ducked (e.g. ambience keeps playing during dialogue)
    FLAG_CLAMP_VOL_TO_NEG = 0x04, // clamps the volume to (-inf, 0]
    FLAG_IS_MUSIC         = 0x10,
    FLAG_IS_NOT_STREAM    = 0x20,
    FLAG_FADE_NEAR_END    = 0x40, // fade out based on how close to finishing
    FLAG_SLOW_FADEOUT     = 0x80,
};

// Deferred: full port belongs to Audio/AEStreamThread.h (VALIDATE_SIZE 0x50).
struct CAEStreamThread {
    uint8_t _deferred[0x50];
};
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAEStreamThread) == 0x50, "CAEStreamThread stand-in size mismatch");
#endif

// tBeatInfo (0xAC) is defined in CAudioEngine.h (the original declares it in
// AudioEngine.h); CAEAudioHardware.h includes this header for it.
struct tBeatInfo;

class CAEAudioHardware {
public:
    bool                    m_bInitialised{};
    bool                    m_bDisableEffectsLoading{};
    bool                    m_PrevMaxSecondarySlowFadeout{};
    bool                    m_PrevMaxGlobalSlowFadeout{};
    bool                    m_IsHardwareMixAvailable{};
    uint8_t                 m_nReverbEnvironment{ (uint8_t)-1};
    int16_t                 m_awChannelFlags[MAX_NUM_AUDIO_CHANNELS]{};
    uint16_t                field_86{};
    int32_t                 m_nReverbDepth{ -10000 };
    uint16_t                m_nNumAvailableChannels{};
    uint16_t                m_nNumChannels{};
    uint16_t                m_anNumChannelsInSlot[MAX_NUM_AUDIO_CHANNELS]{};
    float                   m_afChannelVolumes[MAX_NUM_AUDIO_CHANNELS]{}; // -1000.f
    float                   m_ChannelGains[MAX_NUM_AUDIO_CHANNELS]{}; // linear volume gain (10 ^ (dB / 20))
    float                   m_afChannelsFrqScalingFactor[MAX_NUM_AUDIO_CHANNELS]{};

    float                   m_fMusicMasterScalingFactor{ 1.f };
    float                   m_fEffectMasterScalingFactor{ 1.f };

    float                   m_fMusicFaderScalingFactor{ 1.f };
    float                   m_fEffectsFaderScalingFactor{ 1.f };

    float                   m_fNonStreamFaderScalingFactor{ 1.f };
    float                   m_fStreamFaderScalingFactor{ 1.f };

    float                   m_PrevMaxVolumeSecondary{};
    float                   m_PrevMaxVolumeGlobal{};
    tVirtualChannelSettings m_VirtualChannelSettings{};
    int16_t                 m_VirtualChannelLoopTimes[MAX_NUM_SOUNDS]{};
    int16_t                 m_VirtualChannelSoundLengths[MAX_NUM_SOUNDS]{};

    eBassSetting            m_BassSetting{};
    float                   m_BassGain{};
    CAEMP3BankLoader*       m_pMP3BankLoader{};
    CAEMP3TrackLoader*      m_pMP3TrackLoader{};
    IDirectSound8*          m_pDSDevice{};
    uint32_t                m_nSpeakerConfig{};
    int32_t                 m_n3dEffectsQueryResult{}; // TODO: 1 - EAX available
    DSCAPS                  m_dsCaps{};
    IDirectSound3DListener* m_pDirectSound3dListener{};
    CAEStreamingChannel*    m_pStreamingChannel{};
    CAEStreamThread         m_pStreamThread{};
    CAEAudioChannel*        m_aChannels[MAX_NUM_AUDIO_CHANNELS]{};
    tBeatInfo               gBeatInfo{};
    uint8_t                 m_PlayingTrackFlags{};

public:
    CAEAudioHardware();
    ~CAEAudioHardware() = default;

    bool Initialise();
    bool InitDirectSoundListener(uint32_t numChannels, uint32_t samplesPerSec, uint32_t bitsPerSample);
    void Terminate();

    int16_t AllocateChannels(uint16_t numChannels);

    void PlaySound(int16_t channel, uint16_t channelSlot, uint16_t soundIdInSlot, uint16_t bankSlot, int16_t playPosition, int16_t flags, float speed);
    uint16_t GetNumAvailableChannels() const;
    void GetChannelPlayTimes(int16_t channel, int16_t* playTimes);
    void SetChannelVolume(int16_t channel, uint16_t channelId, float volume, uint8_t unused);

    void LoadSoundBank(eSoundBank bank, eSoundBankSlot slot);
    bool IsSoundBankLoaded(eSoundBank bank, eSoundBankSlot slot);
    int8_t GetSoundBankLoadingStatus(eSoundBank bank, eSoundBankSlot slot);
    bool EnsureSoundBankIsLoaded(eSoundBank bank, eSoundBankSlot slot, bool checkLoadingTune = false, bool cancelSoundsInSlot = false);

    void LoadSound(eSoundBank bank, eSoundID sfx, eSoundBankSlot slot);
    bool IsSoundLoaded(eSoundBank bank, eSoundID sfx, eSoundBankSlot slot);
    bool GetSoundLoadingStatus(eSoundBank bank, eSoundID sfx, eSoundBankSlot slot);

    void StopSound(int16_t channel, uint16_t channelSlot) const;
    void SetChannelPosition(int16_t slotId, uint16_t channelSlot, const CVector& vecPos, uint8_t unused) const;
    void SetChannelFrequencyScalingFactor(int16_t channel, uint16_t channelSlot, float freqFactor);
    void RescaleChannelVolumes();
    void UpdateReverbEnvironment();
    float GetSoundHeadroom(eSoundID sfx, eSoundBankSlot slot);

    void EnableEffectsLoading();
    void DisableEffectsLoading();

    void RequestVirtualChannelSoundInfo(uint16_t vch, eSoundID sfx, eSoundBankSlot slot);
    void GetVirtualChannelSoundLengths(int16_t* outArr) const;
    void GetVirtualChannelSoundLoopStartTimes(int16_t* outArr) const;

    void PlayTrack(uint32_t trackID, int nextTrackID, uint32_t startOffsetMs, uint8_t trackFlags, bool bUserTrack, bool bUserNextTrack);
    void StartTrackPlayback() const;
    void StopTrack();

    int32_t GetTrackPlayTime() const;
    int32_t GetTrackLengthMs() const;
    int32_t GetActiveTrackID() const;
    int32_t GetPlayingTrackID() const;
    void GetBeatInfo(tBeatInfo* beatInfo);
    void GetActualNumberOfHardwareChannels(); // TODO: return type not verified in gta-reversed

    void SetBassSetting(eBassSetting bassSetting, float bassGain);
    void EnableBassEq();
    void DisableBassEq();

    void SetChannelFlags(int16_t channel, uint16_t channelId, int16_t flags);

    void SetMusicMasterScalingFactor(float factor);
    float GetMusicMasterScalingFactor() const;

    void SetEffectsMasterScalingFactor(float factor);
    float GetEffectsMasterScalingFactor() const;

    void SetMusicFaderScalingFactor(float factor);

    void SetEffectsFaderScalingFactor(float factor);
    float GetEffectsFaderScalingFactor() const;

    void SetStreamFaderScalingFactor(float factor);
    void SetNonStreamFaderScalingFactor(float factor);

    bool IsStreamingFromDVD();
    char GetDVDDriveLetter();
    bool CheckDVD();

    void PauseAllSounds();
    void ResumeAllSounds();

    void Query3DSoundEffects();

    void Service();

    // notsa
    const CAEBankSlot& GetBankSlot(eSoundBankSlot slot) const;

private:
    // orig returned std::span (C++20); C++17 equivalent
    CAEAudioChannel* const* GetChannels() const { return m_aChannels; }
};

#if INTPTR_MAX == INT32_MAX
// Layout: 0x430 fixed fields + tVirtualChannelSettings(0x4B0) + 2x int16[300](0x4B0)
//   + bass(8) + 3 ptrs(12) + speaker/result(8) + DSCAPS(0x60) + 2 ptrs(8)
//   + CAEStreamThread(0x50) + 64 ptrs(0x100) + tBeatInfo(0xAC) + flags(1) = 0x1014.
// gta-reversed notes the size might be bigger; nothing is accessed beyond 0x1014.
static_assert(sizeof(CAEAudioHardware) == 0x1014, "CAEAudioHardware layout changed");
#endif

// Global instance, defined in CAEAudioHardware.cpp.
// Original GTA SA 1.0 address (from gta-reversed StaticRef) kept in the .cpp.
extern CAEAudioHardware AEAudioHardware;

