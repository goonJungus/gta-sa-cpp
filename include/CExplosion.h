// CExplosion - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Explosion.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), VALIDATE_SIZE (now a guarded static_assert),
//   StaticRef<> (m_ExplosionAudioEntity/aExplosions are now static members defined
//   in CExplosion.cpp; original game addresses kept in comments).
//   Integer types converted to <cstdint> (int32 -> int32_t, etc.).
//   CAEExplosionAudioEntity: interim 0x80-byte stand-in - the audio subsystem is not
//   yet converted (size per gta-reversed VALIDATE_SIZE(CAEExplosionAudioEntity, 0x80)).
// Forward declarations (owning subsystems not yet converted): CEntity

#pragma once

#include "CVector.h"

#include <array>
#include <cstdint>

class CEntity;

// ---- interim CAEExplosionAudioEntity (full conversion belongs to the audio subsystem) ----
struct CAEExplosionAudioEntity {
    uint8_t data[0x80]{};

    // TODO(audio): full CAEExplosionAudioEntity port; interim AddAudioEvent
    // declaration so weapons_combat TUs compile (signature verified against
    // gta-reversed/source/game_sa/Audio/CAEExplosionAudioEntity.h).
    void AddAudioEvent(int32_t audioEvent, CVector pos, float volume);
};
static_assert(sizeof(CAEExplosionAudioEntity) == 0x80, "CAEExplosionAudioEntity layout changed");

enum eExplosionType : int32_t {
    EXPLOSION_UNDEFINED = -1,
    EXPLOSION_GRENADE = 0,
    EXPLOSION_MOLOTOV,
    EXPLOSION_ROCKET,
    EXPLOSION_WEAK_ROCKET,
    EXPLOSION_CAR,
    EXPLOSION_QUICK_CAR,
    EXPLOSION_BOAT,
    EXPLOSION_AIRCRAFT,
    EXPLOSION_MINE,
    EXPLOSION_OBJECT,
    EXPLOSION_TANK_FIRE,
    EXPLOSION_SMALL,
    EXPLOSION_RC_VEHICLE
};

class CExplosion {
public:
    static constexpr auto NUM_FUEL = 3;

    eExplosionType m_nType;
    CVector        m_vecPosition;
    float          m_fRadius{1.0f};
    float          m_fPropagationRate;
    CEntity*       m_pCreator;
    CEntity*       m_pVictim;
    float          m_nExpireTime;
    float          m_fDamagePercentage;
    uint8_t        m_nActiveCounter;
    bool           m_bMakeSound;
    float          m_nCreatedTime;
    uint32_t       m_nParticlesExpireTime;
    float          m_fVisibleDistance;
    float          m_fGroundZ;
    int32_t        m_nFuelTimer;
    CVector        m_vecFuelDirection[NUM_FUEL];
    float          m_fFuelOffsetDistance[NUM_FUEL];
    float          m_fFuelSpeed[NUM_FUEL];

    // (were StaticRef<> globals at the noted game addresses)
    static CAEExplosionAudioEntity    m_ExplosionAudioEntity; // 0xC888D0
    static std::array<CExplosion, 16> aExplosions;            // 0xC88950

public:
    static void Initialise();
    static void Shutdown();
    static void ClearAllExplosions();

    static uint8_t GetExplosionActiveCounter(uint8_t id);
    static void ResetExplosionActiveCounter(uint8_t id);
    static bool DoesExplosionMakeSound(uint8_t id);
    static int32_t GetExplosionType(uint8_t id);
    static const CVector& GetExplosionPosition(uint8_t id);

    static bool TestForExplosionInArea(eExplosionType type, float minX, float maxX, float minY, float maxY, float minZ, float maxZ);
    static void RemoveAllExplosionsInArea(CVector pos, float radius);
    static void AddExplosion(CEntity* victim, CEntity* creator, eExplosionType type, CVector pos, uint32_t lifetime, uint8_t usesSound, float cameraShake, uint8_t isVisible);
    static void Update();

private:
    // NOTSA functions:
    static CExplosion* GetFree();
    void SetCreator(CEntity* newCreator) noexcept;
    void SetVictim(CEntity* newVictim) noexcept;
};

// Layout check: gta-reversed VALIDATE_SIZE(CExplosion, 0x7C), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CExplosion) == 0x7C, "CExplosion layout drift");
#endif
