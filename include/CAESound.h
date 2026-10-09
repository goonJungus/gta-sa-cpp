// CAESound - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Audio/AESound.h
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(); VALIDATE_SIZE replaced with guarded
//   static_assert (active on 32-bit targets only); `notsa::EntityRef<>`
//   m_PhysicalEntity (auto register/unregister reference) replaced with a plain
//   CEntity* (layout-identical on Win32; ref semantics TODO with the entity
//   system); `m_Event{ AE_UNDEFINED }` replaced with the literal -1 (full
//   eAudioEvents enum not ported yet); include graph reduced to CVector.h.
// Integer types converted to <cstdint>.
// TODO: verify each method against decomp src/CAESound/*.c

#pragma once

#include "CVector.h" // CVector

#include <cstdint>

class CAEAudioEntity;
class CEntity;

// Sound IDs are plain int16 bank indices (gta-reversed Audio/Enums/SoundIDs.h:
// `using eSoundID = int16;`). Full per-bank enums not ported.
using eSoundID = int16_t;

enum eSoundBankSlot : int16_t; // Audio/Enums/eSoundBankSlot.h (full enum not ported)
enum eAudioEvents : int32_t;   // Audio/Enums/eAudioEvents.h (full enum not ported)

enum eSoundEnvironment : uint16_t {
    SOUND_DEFAULT                          = 0x0,
    SOUND_FRONT_END                        = 0x1,
    SOUND_IS_CANCELLABLE                   = 0x2,
    SOUND_REQUEST_UPDATES                  = 0x4,
    SOUND_PLAY_PHYSICALLY                  = 0x8,
    SOUND_IS_PAUSABLE                      = 0x10,
    SOUND_START_PERCENTAGE                 = 0x20,
    SOUND_MUSIC_MASTERED                   = 0x40,
    SOUND_LIFESPAN_TIED_TO_PHYSICAL_ENTITY = 0x80,
    SOUND_IS_DUCKABLE                      = 0x100,
    SOUND_IS_COMPRESSABLE                  = 0x200,
    SOUND_ROLLED_OFF                       = 0x400,
    SOUND_SMOOTH_DUCKING                   = 0x800,
    SOUND_FORCED_FRONT                     = 0x1000
};

class CAESound {
    static constexpr float fSlowMoFrequencyScalingFactor = 0.5F;
public:
    CAESound() = default;
    CAESound(
        eSoundBankSlot  bankSlot,
        eSoundID        sfxId,
        CAEAudioEntity* audioEntity,
        CVector         pos,
        float           volume,
        float           rollOff,
        float           speed,
        float           doppler,
        uint8_t         frameDelay,
        uint16_t        flags,
        float           speedVariance
    );
    ~CAESound();

    void Initialise(
        eSoundBankSlot  bankSlot,
        eSoundID        sfxId,
        CAEAudioEntity* audioEntity,
        CVector         pos,
        float           volume,
        float           rollOff       = 1.f,
        float           speed         = 1.f,
        float           doppler       = 1.f,
        uint8_t         frameDelay    = 0,
        uint16_t        flags         = 0,
        float           speedVariance = 0.f,
        int16_t         playTime      = 0
    );

    void  UnregisterWithPhysicalEntity();
    void  StopSound();
    void  SetFlags(uint16_t envFlag, uint16_t bEnabled); // pass eSoundEnvironment as envFlag
    void  UpdatePlayTime(int16_t soundLength, int16_t loopStartTime, int16_t playProgress);
    CVector GetRelativePosition() const;
    void  GetRelativePosition(CVector* outVec) const;
    void  CalculateFrequency();
    void  UpdateFrequency();
    float GetRelativePlaybackFrequencyWithDoppler() const;
    float GetSlowMoFrequencyScalingFactor() const;
    void  NewVPSLEntry();
    void  RegisterWithPhysicalEntity(CEntity* entity);
    void  StopSoundAndForget();
    void  SetPosition(CVector vecPos);
    void  CalculateVolume();
    void  UpdateParameters(int16_t curPlayPos);
    void  SoundHasFinished();

    void SetSpeed(float s) noexcept { m_Speed = s; }
    auto GetSpeed() const noexcept  { return m_Speed; }

    void SetVolume(float v) noexcept { m_Volume = v; }
    auto GetVolume() const noexcept  { return m_Volume; }

    auto GetSoundLength() const noexcept { return m_Length; }

    bool IsFrontEnd() const { return m_IsFrontEnd; }
    bool GetRequestUpdates() const { return m_RequestUpdates; }
    bool IsUnpausable() const { return m_IsUnpausable; }
    bool GetPlayPhysically() const { return m_PlayPhysically; };
    bool GetPlayTimeIsPercentage() const { return m_PlayTimeIsPercentage; }
    bool IsMusicMastered() const { return m_IsMusicMastered; }
    bool IsLifespanTiedToPhysicalEntity() const { return m_IsLifespanTiedToPhysicalEntity; }
    bool IsUnduckable() const { return m_IsUnduckable; }
    bool IsIncompressible() const { return m_IsIncompressible; }
    bool IsUnancellable() const { return m_IsUnancellable; }
    bool GetRolledOff() const { return m_IsRolledOff; }
    bool GetSmoothDucking() const { return m_HasSmoothDucking; }
    bool IsForcedFront() const { return m_IsForcedFront; }
    bool IsActive() const { return m_IsInUse; }
    bool IsAudioHardwareAware() const { return m_IsAudioHardwareAware; }
    bool IsPhysicallyPlaying() const { return m_IsPhysicallyPlaying; }

public:
    eSoundBankSlot     m_BankSlot{};             //!< Slot to use for the sound
    eSoundID           m_SoundID{};              //!< Sound ID in the bank that's loaded into the slot
    CAEAudioEntity*    m_AudioEntity{};          //!< The entity that's playing this sound
    CEntity*           m_PhysicalEntity{};       //!< If set, the sound is tied to this entity (orig notsa::EntityRef<> - ref semantics TODO)
    int32_t            m_Event{ -1 };            //!< AE_UNDEFINED; not necessarily `eAudioEvents`, for ex. see `CAEWeaponAudioEntity`
    float              m_ClientVariable{ -1.f }; //!< Custom variable set when playing the sound
    float              m_Volume{};               //!< Volume of the sound (Used to calculate the final volume, `ListenerVolume`)
    float              m_RollOffFactor{};        //!< Roll-off factor
    float              m_Speed{};                //!< Speed of the sound (Used to calculate the final frequency, `ListenerSpeed`)
    float              m_SpeedVariance{};        //!< Speed variability
    CVector            m_CurrPos{};              //!< Current position of the sound
    CVector            m_PrevPos{};              //!< Previous position of the sound the last time it was updated
    int32_t            m_LastFrameUpdatedAt{};   //!< Frame count when the sound was last updated (`CTimer::GetFrameCounter()`)
    int32_t            m_CurrTimeUpdateMs{};     //!< Time in milliseconds when the sound was updated (`CTimer::GetTimeInMS()`)
    int32_t            m_PrevTimeUpdateMs{};     //!< Time in milliseconds when the sound was last updated (`CTimer::GetTimeInMS()`)
    float              m_CurrCamDist{};          //!< Distance to the camera
    float              m_PrevCamDist{};          //!< Distance to the camera the last time it was updated
    float              m_Doppler{};              //!< Doppler effect
    uint8_t            m_FrameDelay{};           //!< How many frames to delay the sound (0 = play immediately)
    char               __pad;
    union {
        uint16_t m_Flags{};
        struct {
            uint16_t m_IsFrontEnd : 1;
            uint16_t m_IsUnancellable : 1;
            uint16_t m_RequestUpdates : 1;
            uint16_t m_PlayPhysically : 1;
            uint16_t m_IsUnpausable : 1;
            uint16_t m_PlayTimeIsPercentage : 1;
            uint16_t m_IsMusicMastered : 1;
            uint16_t m_IsLifespanTiedToPhysicalEntity : 1;

            uint16_t m_IsUnduckable : 1;
            uint16_t m_IsIncompressible : 1;
            uint16_t m_IsRolledOff : 1;
            uint16_t m_HasSmoothDucking : 1;
            uint16_t m_IsForcedFront : 1;
        };
    };
    uint16_t m_IsInUse{true};
    int16_t  m_IsAudioHardwareAware{};
    int16_t  m_PlayTime{}; //!< Current play time in milliseconds
    int16_t  m_IsPhysicallyPlaying{};
    float  m_ListenerVolume{-100.f};
    float  m_ListenerSpeed{1.f};
    int16_t  m_HasRequestedStopped{};
    float  m_Headroom{};
    int16_t  m_Length{ -1 }; //!< Length of the sound in milliseconds
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CAESound) == 0x74, "CAESound layout changed");
#endif
