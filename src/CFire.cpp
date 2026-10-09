// CFire - adapted from gta-reversed for clean-room C++ build
// Method implementations. Decompiled reference: src/CFire/*.c
// Bodies cross-checked against gta-reversed/source/game_sa/Fire.cpp.
//
// Decomp-vs-gta-reversed: no divergences found (Constructor/Initialise/
// CreateFxSysForStrength/Extinguish/ProcessFire match the .c files;
// the Start/ExtinguishWithWater/DestroyFx/SetEntity*/HasTimeToBurn/
// IsNotInRemovalDistance/GetFireParticleNameForStrength methods have no
// named .c files and are ported from gta-reversed).

#include "CFire.h"

#include "CTimer.h"
#include "CEntity.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CObject.h"
#include "CPad.h"
#include "CFireManager.h"
#include "CWorld.h" // FindPlayerPed/FindPlayerVehicle/FindPlayerCoors

#include <algorithm> // std::min, std::max
#include <cmath>     // std::modf
#include <cstdint>
#include <new> // placement new in Constructor()

// NOTE on includes (pre-existing tree issues, see BUILD_NOTES):
// - CVehicle.h is NOT included: `operator new` first param must be size_t on
//   x64 (same issue BUILD_NOTES documents for CColStore.h).
// - CCamera.h is NOT included: its eVehicleType copy clashes with
//   CVehicleModelInfo.h's (pre-existing tree issue, also noted in
//   src/CStreaming.cpp).
// - FxManager.h / FxSystem.h are NOT included: FxSystem.h's
//   `using eBoneTag = int32;` clashes with CPed.h's opaque
//   `enum eBoneTag : int16_t;` forward declaration (pre-existing tree issue).
// The affected subsystems are shimmed below instead.

// ============================================================================
// TODO(port): external subsystem shims.
// Minimal declarations for subsystems not yet ported to cpp/. Each entry is
// verified against gta-reversed/source/game_sa and the decompiled bodies in
// src/CFire/*.c. Delete entries as their subsystem lands; do not grow
// this list. None of these introduce link-time dependencies for /c or `ar`
// static-library builds.
// ============================================================================

// --- CGeneral (gta-reversed source/game_sa/General.h)
struct CGeneral {
    static uint32_t GetRandomNumber();
    static float    GetRandomNumberInRange(float min, float max);
    static int32_t  GetRandomNumberInRange(int32_t min, int32_t max);
};

// --- Crime (gta-reversed source/game_sa/Enums/eCrimeType.h, Crime.h)
enum class eCrimeType : int32_t {
    CRIME_SET_PED_ON_FIRE     = 13,
    CRIME_SET_COP_PED_ON_FIRE = 14,
    CRIME_SET_CAR_ON_FIRE     = 15,
};
struct CCrime {
    static void ReportCrime(eCrimeType crimeType, CEntity* entity, CPed* ped2);
};

// --- CVehicle (see NOTE above; signatures verified against
// gta-reversed/source/game_sa/Vehicle.h and include/CVehicle.h)
class CVehicle : public CPhysical {
public:
    CFire*   m_pFire;
    float    m_fHealth;
    int32_t  m_nModelIndex;

    void    InflictDamage(CEntity* damager, eWeaponType weapon, float intensity, CVector coords);
    bool    IsAutomobile();
    CVector GetDummyPosition(int32_t dummy);
    bool    IsSubBMX();
    int32_t FindTyreNearestPoint(CVector point);
    virtual bool BurstTyre(uint8_t tyreComponentId, bool bPhysicalEffect);
};
enum eVehicleDummy : int32_t {
    DUMMY_LIGHT_FRONT_MAIN = 0, // verified in gta-reversed Models/VehicleModelInfo.h
};
constexpr int32_t CAR_PIECE_FIRST_WHEEL = 13; // == CAR_PIECE_WHEEL_LF (include/CVehicle.h)

// --- CCamera (see NOTE above; verified against include/CCamera.h)
class CCamera {
public:
    CVector GetPosition();
    void    CamShake(float strength, CVector pos);
};
extern CCamera& TheCamera;
void CamShakeNoPos(CCamera* cam, float strength);

// --- CPointLights (gta-reversed source/game_sa/PointLights.h)
enum class ePointLightType : uint8_t {
    PLTYPE_POINTLIGHT = 0,
};
struct CPointLights {
    static void AddLight(ePointLightType lightType, CVector point, CVector direction, float radius,
                         float red, float green, float blue, uint8_t fogType = 0,
                         bool generateExtraShadows = false, CEntity* entityAffected = nullptr);
};

// --- CCreepingFire (gta-reversed source/game_sa/CreepingFire.h)
struct CCreepingFire {
    static void TryToStartFireAtCoors(CVector pos, uint8_t nGens, bool arg2, bool isScript, float arg4);
};

// --- Fx particle multiplier (only forward-declared in include/FxSystem.h)
struct FxPrtMult_c {
    FxPrtMult_c(float, float, float, float, float, float, float);
};

// --- FxSystem_c / FxManager_c (see NOTE above; signatures verified against
// include/FxSystem.h and include/FxManager.h)
class FxSystem_c {
public:
    void Kill();
    void Play();
    void PlayAndKill();
    void SetOffsetPos(const CVector& pos);
    void SetConstTime(bool on, float time);
    void AddParticle(const CVector& pos, const CVector& vel, float timeSince, const FxPrtMult_c& fxMults);
};
class FxManager_c {
public:
    FxSystem_c* CreateFxSystem(const char* name, const CVector& point, RwMatrix* objectMatrix, bool ignoreBoundingChecks = false);
};
extern FxManager_c& g_fxMan;

// --- Fx (gta-reversed source/game_sa/Fx/Fx.h: class Fx_c, `extern Fx_c& g_fx`)
class Fx_c {
public:
    FxSystem_c* m_SmokeII3expand;
};
extern Fx_c g_fx;

// --- ModelIndices (gta-reversed source/game_sa/ModelIndices.h)
struct ModelIndices {
    static bool IsFireTruck(int32_t modelId);
};

// --- player helpers are in include/CWorld.h (FindPlayerPed/FindPlayerVehicle/
// FindPlayerCoors/FindPlayerInfo) - included above.

// --- entity pools (not ported; minimal range-for shim mirroring gta-reversed's
// `for (auto& x : GetXPool()->GetAllValid())` idiom)
template<typename T>
struct PoolValidView {
    struct iterator {
        T* m_ptr;
        T& operator*() const { return *m_ptr; }
        iterator& operator++() { ++m_ptr; return *this; }
        bool operator!=(const iterator& o) const { return m_ptr != o.m_ptr; }
    };
    T* m_begin;
    T* m_end;
    iterator begin() const { return { m_begin }; }
    iterator end() const { return { m_end }; }
};
struct CVehiclePool {
    PoolValidView<CVehicle> GetAllValid();
};
struct CObjectPool {
    PoolValidView<CObject> GetAllValid();
};
CVehiclePool* GetVehiclePool();
CObjectPool*  GetObjectPool();

// 0x539D90
CFire::CFire() {
    Initialise();
}

// NOTSA
CFire* CFire::Constructor() {
    return new (this) CFire();
}

// 0x538B30
void CFire::Initialise() {
    // Originally m_nFlags = (m_nFlags & 0xF4) | 0x14; - Clear 1st, 2nd, 4th and set 3rd, 5th bits (1-based numbering)
    m_IsActive            = false;
    m_IsCreatedByScript   = false;
    m_MakesNoise          = true;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration   = true;
    m_Position            = CVector{};
    m_ScriptReferenceIndex = 1;
    m_TimeToBurn          = 0;
    m_EntityOnFire        = nullptr;
    m_EntityStartedFire   = nullptr;
    m_Strength            = 1.0f;
    m_FxSystem            = nullptr;
    m_NumGenerationsAllowed = 100;
    m_RemovalDist         = 60;
}

// NOTSA
void CFire::ExtinguishWithWater(float fWaterStrength) {
    const float fOriginalStrength = m_Strength;
    m_Strength -= fWaterStrength * CTimer::GetTimeStepInSeconds();

    /* Create particles */
    CVector particlePos = m_Position + CVector{
        CGeneral::GetRandomNumberInRange(-1.28f, 1.28f),
        CGeneral::GetRandomNumberInRange(-1.28f, 1.28f),
        CGeneral::GetRandomNumberInRange(-0.64f, 0.64f)
        /* Original code:
        (float)((CGeneral::GetRandomNumber() % 256) - 128) / 100.0f,
        (float)((CGeneral::GetRandomNumber() % 256) - 128) / 100.0f,
        (float)((CGeneral::GetRandomNumber() % 256) - 128) / 200.0f
        */
    };
    FxPrtMult_c prtMult{ 1.0f, 1.0f, 1.0f, 0.6f, 0.75f, 0.0f, 0.4f };
    const auto AddParticle = [&](CVector velocity) {
        g_fx.m_SmokeII3expand->AddParticle(particlePos, velocity, 0.0f, prtMult);
    };
    /* The two particles only differ in velocity */
    AddParticle({ 0.0f, 0.0f, 0.8f });
    AddParticle({ 0.0f, 0.0f, 1.4f });

    /* Re-create fx / extinguish */
    m_IsBeingExtinguished = true;
    if (m_Strength >= 0.0f) {
        if (static_cast<int32_t>(fOriginalStrength) != static_cast<int32_t>(m_Strength)) { /* Check if integer part has changed */
            CreateFxSysForStrength(m_Position, nullptr); /* Yes, so needs a new fx */
        }
    } else {
        Extinguish();
    }
}

// see 0x539F00 CFireManager::StartFire
void CFire::Start(CEntity* creator, CVector pos, uint32_t nTimeToBurn, uint8_t nGens) {
    m_IsActive            = true;
    m_IsCreatedByScript   = false;
    m_MakesNoise          = true;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration   = true;

    m_TimeToBurn = CTimer::GetTimeInMS() + static_cast<uint32_t>(CGeneral::GetRandomNumberInRange(1.0f, 1.3f) * static_cast<float>(nTimeToBurn));

    SetEntityOnFire(nullptr);
    SetEntityStartedFire(creator);

    m_NumGenerationsAllowed = nGens;
    m_Strength              = 1.0f;
    m_Position              = pos;

    CreateFxSysForStrength(m_Position, nullptr);
}

// see 0x53A050 CFireManager::StartFire
void CFire::Start(CEntity* creator, CEntity* target, uint32_t nTimeToBurn, uint8_t nGens) {
    switch (target->GetType()) {
    case ENTITY_TYPE_PED: {
        auto* targetPed = target->AsPed();
        targetPed->m_pFire = this;
        CCrime::ReportCrime(
            targetPed->m_nPedType == PED_TYPE_COP ? eCrimeType::CRIME_SET_COP_PED_ON_FIRE : eCrimeType::CRIME_SET_PED_ON_FIRE,
            targetPed,
            creator->AsPed()
        );
        break;
    }
    case ENTITY_TYPE_VEHICLE: {
        auto* targetVehicle = target->AsVehicle();
        targetVehicle->m_pFire = this;
        CCrime::ReportCrime(
            eCrimeType::CRIME_SET_CAR_ON_FIRE,
            targetVehicle,
            creator->AsPed()
        );
        break;
    }
    case ENTITY_TYPE_OBJECT: {
        target->AsObject()->m_pFire = this;
        break;
    }
    }

    m_IsActive            = true;
    m_IsCreatedByScript   = false;
    m_MakesNoise          = true;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration   = true;

    m_NumGenerationsAllowed = nGens;
    m_Strength              = 1.0f;
    m_Position              = target->GetPosition();

    if (target->GetIsTypePed() && target->AsPed()->IsPlayer())
        m_TimeToBurn = CTimer::GetTimeInMS() + 2333;
    else if (target->GetIsTypeVehicle())
        m_TimeToBurn = CTimer::GetTimeInMS() + static_cast<uint32_t>(CGeneral::GetRandomNumberInRange(0, 1000)) + 3000;
    else
        m_TimeToBurn = CTimer::GetTimeInMS() + static_cast<uint32_t>(CGeneral::GetRandomNumberInRange(0, 1000)) + nTimeToBurn;

    SetEntityOnFire(target);
    SetEntityStartedFire(creator);

    CreateFxSysForStrength(m_Position, nullptr);
}

// see 0x53A270 CFireManager::StartScriptFire
void CFire::Start(CVector pos, float fStrength, CEntity* target, uint8_t nGens) {
    SetEntityOnFire(target);
    SetEntityStartedFire(nullptr);

    m_IsActive            = true;
    m_IsCreatedByScript   = true;
    m_MakesNoise          = true;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration   = true;

    m_NumGenerationsAllowed = nGens;
    m_Strength              = fStrength;
    m_Position              = pos;

    if (target) {
        switch (target->GetType()) { /* Set target's `m_pFire` to `this` */
        case ENTITY_TYPE_PED:
            target->AsPed()->m_pFire = this;
            break;
        case ENTITY_TYPE_VEHICLE:
            target->AsVehicle()->m_pFire = this;
            break;
        }
    }

    CreateFxSysForStrength(target ? target->GetPosition() : pos, nullptr);
}

// NOTSA
void CFire::SetEntityOnFire(CEntity* target) {
    CEntity::SafeCleanUpRef(m_EntityOnFire); /* Assume old target's m_pFire is not pointing to `*this` */

    m_EntityOnFire = target; /* assign, even if its null, to clear it */
    CEntity::SafeRegisterRef(m_EntityOnFire); /* Assume caller set target->m_pFire */
}

// NOTSA
void CFire::SetEntityStartedFire(CEntity* creator) {
    CEntity::SafeCleanUpRef(m_EntityStartedFire);

    m_EntityStartedFire = creator; /* assign, even if its null, to clear it */
    CEntity::SafeRegisterRef(m_EntityStartedFire); /* Assume caller set target->m_pFire */
}

// NOTSA
void CFire::DestroyFx() {
    if (m_FxSystem) {
        m_FxSystem->Kill();
        m_FxSystem = nullptr;
    }
}

// NOTSA
const char* CFire::GetFireParticleNameForStrength() const {
    if (m_Strength > 1.0f)
        return (m_Strength > 2.0f) ? "fire_large" : "fire_med";
    else
        return "fire";
}

// 0x539360
void CFire::CreateFxSysForStrength(const CVector& point, RwMatrix* matrix) {
    DestroyFx();
    m_FxSystem = g_fxMan.CreateFxSystem(GetFireParticleNameForStrength(), point, matrix, true);
    if (m_FxSystem)
        m_FxSystem->Play();
}

// 0x5393F0
void CFire::Extinguish() {
    if (!m_IsActive)
        return;

    m_TimeToBurn = 0;

    // Originally m_nFlags = (m_nFlags & 0xF6) | 0x10; - Clear 1st and 4th, set 5th
    m_IsActive            = false;
    m_IsBeingExtinguished = false;
    m_IsFirstGeneration   = true;

    DestroyFx();

    if (m_EntityOnFire) {
        switch (m_EntityOnFire->GetType()) {
        case ENTITY_TYPE_PED: {
            m_EntityOnFire->AsPed()->m_pFire = nullptr;
            break;
        }
        case ENTITY_TYPE_VEHICLE: {
            m_EntityOnFire->AsVehicle()->m_pFire = nullptr;
            break;
        }
        }
        CEntity::ClearReference(m_EntityOnFire);
    }
}

// 0x53A570
void CFire::ProcessFire() {
    {
        const float newStrength = std::min(3.0f, m_Strength + CTimer::GetTimeStep() / 500.0f);
        if (static_cast<uint32_t>(m_Strength) != static_cast<uint32_t>(newStrength)) {
            m_Strength = newStrength; // Not sure why they do this, probably just some hack
        }
    }

    if (m_EntityOnFire) {
        m_Position = m_EntityOnFire->GetPosition();

        switch (m_EntityOnFire->GetType()) {
        case ENTITY_TYPE_PED: {
            auto* targetPed = m_EntityOnFire->AsPed();

            if (targetPed->m_pFire != this) {
                Extinguish();
                return;
            }

            switch (targetPed->m_nPedState) {
            case PEDSTATE_DIE:
            case PEDSTATE_DEAD: {
                m_Position.z -= 1.0f; /* probably because ped is laying on the ground */
                break;
            }
            }

            if (auto* vehicle = targetPed->GetVehicleIfInOne()) {
                if (!ModelIndices::IsFireTruck(vehicle->m_nModelIndex) && vehicle->IsAutomobile()) {
                    vehicle->m_fHealth = 75.0f;
                }
            } else if (!targetPed->IsPlayer() && !targetPed->IsAlive()) {
                targetPed->physicalFlags.bRenderScorched = true;
            }

            break;
        }
        case ENTITY_TYPE_VEHICLE: {
            auto* targetVehicle = m_EntityOnFire->AsVehicle();

            if (targetVehicle->m_pFire != this) {
                Extinguish();
                return;
            }

            if (!m_IsCreatedByScript) {
                targetVehicle->InflictDamage(m_EntityStartedFire, eWeaponType::WEAPON_FLAMETHROWER, CTimer::GetTimeStep() * 1.2f, CVector{});
            }

            if (targetVehicle->IsAutomobile()) {
                m_Position = targetVehicle->GetDummyPosition(eVehicleDummy::DUMMY_LIGHT_FRONT_MAIN) + CVector{ 0.0f, 0.0f, 0.15f };
            }
            break;
        }
        }

        if (m_FxSystem) {
            auto* targetPhysical = m_EntityOnFire->AsPhysical();
            m_FxSystem->SetOffsetPos(m_Position + CTimer::GetTimeStep() * 2.0f * targetPhysical->m_vecMoveSpeed);
        }
    }

    CPlayerPed* player = FindPlayerPed();
    if (!m_EntityOnFire || !m_EntityOnFire->GetIsTypeVehicle()) {
        // Check if we can set player's ped on fire
        if (!FindPlayerVehicle()
         && !player->m_pFire /* not already on fire */
         && !player->physicalFlags.bFireProof
         && !player->m_pAttachedTo
         ) {
            if ((player->GetPosition() - m_Position).SquaredMagnitude() < 1.2f) { /* Note: Squared distance */
                player->DoStuffToGoOnFire();
                gFireManager.StartFire(player, m_EntityStartedFire, 0.8f, true, 7000, 100);
            }
        }
    }

    if (CGeneral::GetRandomNumber() % 32 == 0) {
        for (auto& veh : GetVehiclePool()->GetAllValid()) { // NOTSA: Original loop was backwards [not that it matters]
            if (DistanceBetweenPoints(m_Position, veh.GetPosition()) >= 2.0f)
                continue;

            if (veh.IsSubBMX()) {
                player->DoStuffToGoOnFire();
                gFireManager.StartFire(player, m_EntityStartedFire, 0.8f, true, 7000, 100);
                veh.BurstTyre(static_cast<uint8_t>(CAR_PIECE_FIRST_WHEEL + veh.FindTyreNearestPoint(m_Position)), false);
            } else {
                gFireManager.StartFire(&veh, m_EntityStartedFire, 0.8f, true, 7000, 100);
            }
        }
    }

    if (CGeneral::GetRandomNumber() % 4 == 0) {
        for (auto& obj : GetObjectPool()->GetAllValid()) { // NOTSA: Original loop was backwards [not that it matters]
            if (DistanceBetweenPoints(m_Position, obj.GetPosition()) >= 3.0f)
                continue;

            obj.ObjectFireDamage(CTimer::GetTimeStep() * 8.0f, m_EntityStartedFire);
        }
    }

    if (m_NumGenerationsAllowed > 0 && CGeneral::GetRandomNumber() % 128 == 0) {
        if (gFireManager.GetNumOfFires() < 25) {
            const CVector dir{ CGeneral::GetRandomNumberInRange(-1.0f, 1.0f), CGeneral::GetRandomNumberInRange(-1.0f, 1.0f), 0.0f };
            CCreepingFire::TryToStartFireAtCoors(m_Position + dir * CGeneral::GetRandomNumberInRange(2.0f, 3.0f), m_NumGenerationsAllowed, false, IsScript(), 10.0f);
        }
    }

    if (m_Strength <= 2.0f && m_NumGenerationsAllowed && CGeneral::GetRandomNumber() % 16 == 0) {
        CFire& nearby = gFireManager.GetRandomFire();
        if (&nearby != this && nearby.m_IsActive && !nearby.m_IsCreatedByScript && nearby.m_Strength <= 1.0f) {
            if (DistanceBetweenPoints(nearby.m_Position, m_Position) < 3.5f) {
                nearby.m_Position = nearby.m_Position * 0.3f + m_Position * 0.7f;
                m_Strength += 1.0f;
                m_TimeToBurn = std::max(m_TimeToBurn, CTimer::GetTimeInMS() + 7000);
                CreateFxSysForStrength(m_Position, nullptr);
                m_NumGenerationsAllowed = std::max(m_NumGenerationsAllowed, nearby.m_NumGenerationsAllowed);
                nearby.Extinguish();
            }
        }
    }

    if (m_FxSystem) {
        float unused;
        const float fFractPart = std::modf(m_Strength, &unused); // R* way: m_fStrength - (float)(int)m_fStrength
        m_FxSystem->SetConstTime(true, std::min(static_cast<float>(CTimer::GetTimeInMS()) / 3500.0f, fFractPart));
    }

    if (m_IsCreatedByScript || (HasTimeToBurn() && IsNotInRemovalDistance())) {
        const float fColorRG = static_cast<float>(CGeneral::GetRandomNumber() % 128) / 512.0f; // todo: GetRandomNumberInRange
        CPointLights::AddLight(ePointLightType::PLTYPE_POINTLIGHT, m_Position, CVector{}, 8.0f, fColorRG, fColorRG, 0.0f, 0, false, nullptr);
    } else {
        if (m_Strength <= 1.0f) {
            Extinguish();
        } else {
            m_Strength -= 1.0f;
            m_TimeToBurn = CTimer::GetTimeInMS() + 7000;
            CreateFxSysForStrength(m_Position, nullptr);
        }
    }
}

// NOTSA
bool CFire::HasTimeToBurn() const {
    return CTimer::GetTimeInMS() < m_TimeToBurn;
}

// NOTSA
bool CFire::IsNotInRemovalDistance() const {
    return m_RemovalDist > (TheCamera.GetPosition() - m_Position).Magnitude();
}
