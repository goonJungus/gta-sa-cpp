// CAESound.cpp - GTA SA 1.0 clean-room C++ conversion
// Method bodies converted from src/CAESound/*.c (Ghidra decomp) and verified
// against gta-reversed/source/game_sa/Audio/AESound.cpp structure.
// This file is part of a clean-room engine reimplementation for
// interoperability research. Game assets load from the user's own install
// at runtime and are never shipped.

#include "CAESound.h"

#include "CAEAudioHardware.h" // AEAudioHardware.GetSoundHeadroom
#include "CCamera.h"          // TheCamera (SetPosition, GetSlowMoFrequencyScalingFactor)
#include "CEntity.h"          // ClearReference / ChangeEntityReference
#include "CTimer.h"

// ---------------------------------------------------------------------------
// Shims for not-yet-ported audio pieces. Each is declared here only so this
// TU syntax-checks; the real declarations belong in their subsystem headers.
// TODO(port): delete this block as CAEAudioEnvironment, CAEAudioUtility and
// CAEAudioEntity are ported (see BUILD_NOTES.md "Next steps").
// ---------------------------------------------------------------------------
class CAEAudioEnvironment {
public:
    static void  GetPositionRelativeToCamera(CVector* outPos, const CVector* pos);
    static float GetDistanceAttenuation(float distance);
    static float GetDirectionalMikeAttenuation(const CVector* relPos);
    static float GetDopplerRelativeFrequency(float prevCamDist, float currCamDist,
                                             int32_t prevTimeMs, int32_t currTimeMs,
                                             float doppler);
};

class CAEAudioUtility {
public:
    static float GetRandomNumberInRange(float min, float max);
};

// Minimal stand-in: the real CAEAudioEntity.h replaces this when ported.
class CAEAudioEntity {
public:
    void UpdateParameters(CAESound* sound, int16_t playPos);
};

namespace CGeneral {
// Unresolved import thunk @ 0x821b40 (Ghidra: CGeneral::unk_00821b40).
// Returns the initial play position (int16) used by UpdatePlayTime.
// TODO: re-resolve for the clean-room build.
int16_t unk_00821b40();
}
// ---------------------------------------------------------------------------

// 0x4EF0A0 (copy ctor in the decomp; the parameterized ctor below is the
// header-declared form and delegates the field setup to Initialise-style init)
CAESound::CAESound(
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
) :
    m_BankSlot{ bankSlot },
    m_SoundID{ sfxId },
    m_AudioEntity{ audioEntity },
    m_Volume{ volume },
    m_RollOffFactor{ rollOff },
    m_Speed{ speed },
    m_Doppler{ doppler },
    m_FrameDelay{ frameDelay },
    m_Flags{ flags },
    m_SpeedVariance{ speedVariance }
{
    SetPosition(pos);
}

// 0x4EF660
CAESound::~CAESound() {
    UnregisterWithPhysicalEntity();
}

// 0x4EFE50
void CAESound::Initialise(
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
    float           speedVariance,
    int16_t         playTime
) {
    m_SoundID       = sfxId;
    m_BankSlot      = bankSlot;
    m_AudioEntity   = audioEntity;
    CEntity::ClearReference(m_PhysicalEntity);
    m_Volume        = volume;
    m_RollOffFactor = rollOff;
    m_Speed         = speed;
    m_SpeedVariance = speedVariance;
    m_Event         = -1; // AE_UNDEFINED
    m_ClientVariable = -1.0f;
    m_LastFrameUpdatedAt = 0;
    SetPosition(pos);
    m_Doppler             = doppler;
    m_Length              = -1;
    m_IsPhysicallyPlaying = 0;
    m_HasRequestedStopped = 0;
    m_Headroom            = 0.0f;
    m_FrameDelay          = frameDelay;
    m_Flags               = flags;
    m_IsInUse             = 1;
    m_PlayTime            = playTime;
    m_ListenerVolume      = -100.0f;
    m_ListenerSpeed       = 1.0f;
}

// 0x4EF1A0
void CAESound::UnregisterWithPhysicalEntity() {
    CEntity::ClearReference(m_PhysicalEntity);
}

// 0x4EF1C0
void CAESound::StopSound() {
    m_HasRequestedStopped = 1;
    UnregisterWithPhysicalEntity();
}

// 0x4EF2B0
void CAESound::SetFlags(uint16_t envFlag, uint16_t bEnabled) {
    if (bEnabled) {
        m_Flags |= envFlag;
    } else {
        m_Flags &= ~envFlag;
    }
}

// 0x4EF2E0
void CAESound::UpdatePlayTime(int16_t soundLength, int16_t loopStartTime, int16_t playProgress) {
    m_Length = soundLength;
    if (m_IsPhysicallyPlaying) {
        return;
    }
    if (m_HasRequestedStopped) {
        m_PlayTime = -1;
        return;
    }
    // NOTE: playProgress is unused in 1.0 (the decomp never reads it).
    (void)playProgress;
    const int16_t pos = CGeneral::unk_00821b40();
    m_PlayTime = pos;
    if (soundLength <= pos) {
        if (loopStartTime == -1) {
            m_PlayTime = -1;
            return;
        }
        m_PlayTime = static_cast<int16_t>(pos % soundLength + loopStartTime);
    }
}

// 0x4EF350
CVector CAESound::GetRelativePosition() const {
    CVector outVec;
    GetRelativePosition(&outVec);
    return outVec;
}

// 0x4EF350 (original calling convention)
void CAESound::GetRelativePosition(CVector* outVec) const {
    if (IsFrontEnd()) {
        *outVec = m_CurrPos;
    } else {
        CAEAudioEnvironment::GetPositionRelativeToCamera(outVec, &m_CurrPos);
    }
}

// 0x4EF390
void CAESound::CalculateFrequency() {
    if (m_SpeedVariance > 0.0f && m_SpeedVariance < m_Speed) {
        m_ListenerSpeed = m_Speed + CAEAudioUtility::GetRandomNumberInRange(-m_SpeedVariance, m_SpeedVariance);
    } else {
        m_ListenerSpeed = m_Speed;
    }
}

// 0x4EF3E0
void CAESound::UpdateFrequency() {
    if (m_SpeedVariance == 0.0f) {
        m_ListenerSpeed = m_Speed;
    }
}

// 0x4EF400
float CAESound::GetRelativePlaybackFrequencyWithDoppler() const {
    if (IsFrontEnd()) {
        return m_ListenerSpeed;
    }
    return m_ListenerSpeed * CAEAudioEnvironment::GetDopplerRelativeFrequency(
        m_PrevCamDist, m_CurrCamDist, m_PrevTimeUpdateMs, m_CurrTimeUpdateMs, m_Doppler);
}

// 0x4EF440
float CAESound::GetSlowMoFrequencyScalingFactor() const {
    // Decomp reads TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode (0xB6F1A8/0xB6F081)
    // and compares against 0x2E. 0x2E has no name in the ported eCamMode yet.
    // TODO: name the 0x2E camera mode when eCamMode is fully ported.
    constexpr eCamMode SLOWMO_EXEMPT_CAM_MODE = static_cast<eCamMode>(0x2E);
    if (!IsUnpausable() && CTimer::GetIsSlowMotionActive()
        && TheCamera.GetActiveCam().m_nMode != SLOWMO_EXEMPT_CAM_MODE) {
        return fSlowMoFrequencyScalingFactor;
    }
    return 1.0f;
}

// 0x4EF7A0
void CAESound::NewVPSLEntry() {
    m_IsPhysicallyPlaying  = 0;
    m_HasRequestedStopped  = 0;
    m_IsAudioHardwareAware = 0;
    m_IsInUse              = 1;
    m_Headroom = AEAudioHardware.GetSoundHeadroom(m_SoundID, m_BankSlot);
    // NOTE: the original inlines the CalculateFrequency body here.
    CalculateFrequency();
}

// 0x4EF820
void CAESound::RegisterWithPhysicalEntity(CEntity* entity) {
    CEntity::ChangeEntityReference(m_PhysicalEntity, entity);
}

// 0x4EF850
void CAESound::StopSoundAndForget() {
    m_Flags &= ~SOUND_REQUEST_UPDATES; // decomp: flags &= 0xFB
    m_HasRequestedStopped = 1;
    m_AudioEntity = nullptr;
    CEntity::ClearReference(m_PhysicalEntity);
}

// 0x4EF880
void CAESound::SetPosition(CVector vecPos) {
    // Decomp reads the game camera position via TheCamera internals
    // (0xB6F02C game-cam CVector, or the CVector at (entity+0x30) when the
    // dword at 0xB6F03C is non-null) - equivalent to GetGameCamPosition().
    const float dist = DistanceBetweenPoints(vecPos, *TheCamera.GetGameCamPosition());
    if (m_LastFrameUpdatedAt == 0) {
        m_PrevPos = vecPos;
        m_CurrPos = vecPos;
        m_CurrCamDist = dist;
        m_PrevCamDist = dist;
        m_LastFrameUpdatedAt = CTimer::m_FrameCounter;
        m_CurrTimeUpdateMs = CTimer::m_snTimeInMilliseconds;
        m_PrevTimeUpdateMs = m_CurrTimeUpdateMs;
        return;
    }
    if (CTimer::m_FrameCounter != m_LastFrameUpdatedAt) {
        m_PrevPos = m_CurrPos;
        m_PrevCamDist = m_CurrCamDist;
        m_PrevTimeUpdateMs = m_CurrTimeUpdateMs;
    }
    m_CurrPos = vecPos;
    m_CurrCamDist = dist;
    m_LastFrameUpdatedAt = CTimer::m_FrameCounter;
    m_CurrTimeUpdateMs = CTimer::m_snTimeInMilliseconds;
}

// 0x4EFA10
void CAESound::CalculateVolume() {
    if (IsFrontEnd()) {
        m_ListenerVolume = m_Volume - m_Headroom;
        return;
    }
    CVector relPos;
    CAEAudioEnvironment::GetPositionRelativeToCamera(&relPos, &m_CurrPos);
    m_ListenerVolume = m_Volume - m_Headroom
        + CAEAudioEnvironment::GetDistanceAttenuation(relPos.Magnitude() / m_RollOffFactor)
        + CAEAudioEnvironment::GetDirectionalMikeAttenuation(&relPos);
}

// 0x4EFF50
void CAESound::UpdateParameters(int16_t curPlayPos) {
    if (IsLifespanTiedToPhysicalEntity()) {
        if (m_PhysicalEntity) {
            SetPosition(m_PhysicalEntity->GetPosition());
        } else {
            m_HasRequestedStopped = 1;
        }
    }
    if (GetRequestUpdates() && m_AudioEntity) {
        // NB: the entity reference is clearable, hence the null check.
        m_AudioEntity->UpdateParameters(this, curPlayPos);
        if (m_SpeedVariance == 0.0f) {
            m_ListenerSpeed = m_Speed;
        }
    }
}

// 0x4EFFD0
void CAESound::SoundHasFinished() {
    UpdateParameters(-1);
    UnregisterWithPhysicalEntity();
    m_IsInUse = 0;
    m_IsPhysicallyPlaying = 0;
    m_PlayTime = 0;
}
