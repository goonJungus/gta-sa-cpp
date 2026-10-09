// CFire - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Fire.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations: stripped InjectHooks(), VALIDATE_SIZE (now a guarded static_assert).
//   Integer types converted to <cstdint> (uint32 -> uint32_t, etc.).
//   GetId() used C++23 deducing-this (`this auto&& self`) - split into const and
//   non-const overloads for C++17.
//   GetFireParticleNameForStrength() return type pinned to const char* (was deduced
//   `auto`; gta-reversed Fire.cpp returns string literals).
// Forward declarations (owning subsystems not yet converted):
//   CEntity, FxSystem_c, RwMatrix (RenderWare layer)

#pragma once

#include "CVector.h"

#include <cstdint>

class CEntity;
class CFire;
class FxSystem_c;
struct RwMatrix;

class CFire {
public:
    CFire();
    ~CFire() = default;
    CFire* Constructor();

    void Initialise();
    void Start(CEntity* creator, CVector pos, uint32_t nTimeToBurn, uint8_t nGens);
    void Start(CEntity* creator, CEntity* target, uint32_t nTimeToBurn, uint8_t nGens);
    void Start(CVector pos, float fStrength, CEntity* target, uint8_t nGens); /* For script */
    void CreateFxSysForStrength(const CVector& point, RwMatrix* matrix);
    void Extinguish();
    void ExtinguishWithWater(float fWaterStrength);
    void ProcessFire();

    // Inlined
    bool IsScript() const { return m_IsCreatedByScript; }
    void SetIsScript(bool b) { m_IsCreatedByScript = b; }

    bool IsFirstGen() const { return m_IsFirstGeneration; }
    void SetIsFirstGen(bool b) { m_IsFirstGeneration = b; }

    bool IsActive() const { return m_IsActive; }
    bool IsBeingExtinguished() const { return m_IsBeingExtinguished; }

    bool MakesNoise() const { return m_MakesNoise; }
    void SetMakesNoise(bool b) { m_MakesNoise = b; }

    auto GetStrength() const { return m_Strength; }

    // NOTSA funcs:
    const char* GetFireParticleNameForStrength() const;
    void DestroyFx();

    auto GetEntityOnFire() const { return m_EntityOnFire; }
    void SetEntityOnFire(CEntity* target);

    auto GetEntityStartedFire() const { return m_EntityStartedFire; }
    void SetEntityStartedFire(CEntity* creator);

    bool HasTimeToBurn() const;
    bool IsNotInRemovalDistance() const;
    auto& GetPosition() const { return m_Position; }

    //! Script thing ID
    auto& GetId() { return m_ScriptReferenceIndex; }
    const auto& GetId() const { return m_ScriptReferenceIndex; }

private:
    bool        m_IsActive : 1;
    bool        m_IsCreatedByScript : 1;
    bool        m_MakesNoise : 1;
    bool        m_IsBeingExtinguished : 1;
    bool        m_IsFirstGeneration : 1;
    int16_t     m_ScriptReferenceIndex;
    CVector     m_Position;
    CEntity*    m_EntityOnFire;
    CEntity*    m_EntityStartedFire;
    uint32_t    m_TimeToBurn;
    float       m_Strength;
    uint8_t     m_NumGenerationsAllowed;
    uint8_t     m_RemovalDist;
    FxSystem_c* m_FxSystem;
};

// Layout check: gta-reversed VALIDATE_SIZE(CFire, 0x28), enforced only on
// 32-bit targets (the original binary is 32-bit; 64-bit dev builds skip it).
#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CFire) == 0x28, "CFire layout drift");
#endif
