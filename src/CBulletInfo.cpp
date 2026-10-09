// CBulletInfo - adapted from gta-reversed for clean-room C++ build
// Method implementations. Decompiled reference: src/CBulletInfo/*.c
// Bodies cross-checked against gta-reversed/source/game_sa/BulletInfo.cpp
// (this class is only used by CWeapon::FireSniper).
//
// Decomp-vs-gta-reversed: no divergences found (Initialise/AddBullet/Shutdown
// match the .c files exactly; Update matches gta-reversed's structure).

#include "CBulletInfo.h"

#include "CTimer.h"
#include "CWorld.h"
#include "CWeapon.h"
#include "CWeaponInfo.h"
#include "CFireManager.h"
#include "CEntity.h"
#include "CPed.h"
#include "CObject.h"
#include "ColTypes.h"

#include <cmath> // std::asin

// NOTE on includes (pre-existing tree issues, see BUILD_NOTES):
// - CAudioEngine.h is NOT included: its `enum eSurfaceType : uint8_t;`
//   forward declaration conflicts with CPhysical.h's `enum eSurfaceType :
//   int32_t` definition (see the CONFLICT note in include/ColTypes.h).
// - CVehicle.h is NOT included: `operator new` first param must be size_t on
//   x64 (same issue BUILD_NOTES documents for CColStore.h).
// - CCamera.h is NOT included: its eVehicleType copy clashes with
//   CVehicleModelInfo.h's (pre-existing tree issue, also noted in
//   src/CStreaming.cpp).
// - CAnimManager.h / CAnimBlendAssociation.h are NOT included: they pull in
//   AnimTypes.h whose AssocGroupId/CQuaternion clash with CPedModelInfo.h's
//   minimal AssocGroupId stand-in and CMatrix.h's CQuaternion (pre-existing
//   tree issue, also noted in src/CStreaming.cpp).
// The affected subsystems are shimmed below instead.

// ============================================================================
// TODO(port): external subsystem shims.
// Minimal declarations for subsystems not yet ported to cpp/. Each entry is
// verified against gta-reversed/source/game_sa and the decompiled bodies in
// src/CBulletInfo/*.c. Delete entries as their subsystem lands; do not grow
// this list. None of these introduce link-time dependencies for /c or `ar`
// static-library builds.
// ============================================================================

// --- Crime (gta-reversed source/game_sa/Enums/eCrimeType.h, Crime.h)
enum class eCrimeType : int32_t {
    CRIME_FIRE_WEAPON    = 1,
    CRIME_DAMAGED_PED    = 2,
    CRIME_DAMAGE_CAR     = 4,
    CRIME_DAMAGE_COP_CAR = 5,
};
struct CCrime {
    static void ReportCrime(eCrimeType crimeType, CEntity* entity, CPed* ped2);
};

// --- Glass (gta-reversed source/game_sa/Glass.h)
struct CGlass {
    static void WasGlassHitByBullet(CEntity* entity, CVector hitPos);
};

// --- Localisation (gta-reversed source/game_sa/Localisation.h)
struct CLocalisation {
    static bool Blood();
};

// --- CAudioEngine (see NOTE above; signature verified against
// gta-reversed/source/game_sa/AudioEngine.h - surface is the 1-byte surface
// type, matching the binary layout)
class CAudioEngine {
public:
    void ReportBulletHit(CEntity* entity, uint8_t surface, const CVector& posn, float angleWithColPointNorm);
};
extern CAudioEngine AudioEngine;

// --- Fx (gta-reversed source/game_sa/Fx/Fx.h: class Fx_c, `extern Fx_c& g_fx`)
class Fx_c {
public:
    void AddBlood(const CVector& origin, const CVector& direction, int32_t amount, float arg3);
    void AddTyreBurst(const CVector& posn, const CVector& velocity);
    void AddBulletImpact(const CVector& posn, const CVector& direction, int32_t bulletFxType, int32_t amount, float arg4);
};
extern Fx_c g_fx;

// --- CVehicle (see NOTE above; signatures verified against
// gta-reversed/source/game_sa/Vehicle.h and include/CVehicle.h)
class CVehicle : public CPhysical {
public:
    void InflictDamage(CEntity* damager, eWeaponType weapon, float intensity, CVector coords);
    virtual bool BurstTyre(uint8_t tyreComponentId, bool bPhysicalEffect);
};
// eCarPiece wheel values verified against include/CVehicle.h
enum eCarPiece : int32_t {
    CAR_PIECE_WHEEL_LF = 13,
    CAR_PIECE_WHEEL_RF = 14,
    CAR_PIECE_WHEEL_RL = 15,
    CAR_PIECE_WHEEL_RR = 16,
};

// --- CCamera (see NOTE above; verified against include/CCamera.h)
class CCamera {
public:
    bool IsSphereVisible(const CVector& origin, float radius);
};
extern CCamera& TheCamera;

// --- CHeli (see NOTE above; verified against include/CHeli.h)
struct CHeli {
    static void TestSniperCollision(CVector* origin, CVector* target);
};

// --- CAnimManager / CAnimBlendAssociation (see NOTE above; signatures verified
// against include/CAnimManager.h and include/CAnimBlendAssociation.h)
class CAnimBlendAssociation {
public:
    void SetCurrentTime(float currentTime);
    void SetFlag(uint32_t flag, bool value = true);
};
struct CAnimManager {
    static CAnimBlendAssociation* BlendAnimation(RpClump* clump, int32_t groupId, int32_t animId, float f = 8.0f);
};
// RenderWare anim-blend helper (plugin-sdk signature)
CAnimBlendAssociation* RpAnimBlendClumpGetFirstAssociation(RpClump* clump, uint32_t flags);
// AnimationId values copied on demand from gta-reversed Enums/AnimationEnums.h
// (per the instruction in include/AnimTypes.h); eAnimationFlags values from
// include/CAnimBlendAssociation.h.
// ANIM_ID_FLOOR_HIT/_F: canonical values in AnimTypes.h (deduped 2026-10-09;
// local constexprs redefined the AnimationId enumerators).
constexpr uint32_t ANIMATION_IS_FRONT = 1u << 11;
constexpr uint32_t ANIMATION_IS_FINISH_AUTO_REMOVE = 1u << 3;

// --- CObjectData (forward-declared in include/CObject.h; full port belongs to
// the model-info subsystem. Fields verified against gta-reversed ObjectData.h)
class CObjectData {
public:
    float    m_fUprootLimit;
    float    m_fColDamageMultiplier;
    uint32_t m_nGunBreakMode; // see eObjectBreakMode
    float    m_fSmashMultiplier;
};
enum eObjectBreakMode : uint32_t {
    NOT_BY_GUN,
    BY_GUN,
    SMASHABLE,
};

// --- CPlayerPedData (forward-declared in include/CPed.h)
class CPlayerPedData {
public:
    int32_t m_nModelIndexOfLastBuildingShot;
};

// --- Math helpers (not yet in cpp/ math headers)
inline float DotProduct(const CVector& a, const CVector& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
constexpr float RadiansToDegrees(float rad) {
    return rad * 57.295776f;
}

// --- tColLighting::GetCurrentLighting (decomp src/tColLighting/
// GetCurrentLighting_0059f0c0.c; declared in include/ColTypes.h).
// Depends on the render pipeline's day/night balance; the full render port
// owns that global - shimmed here, delete when it lands.
struct CCustomBuildingDNPipeline {
    static float m_fDNBalanceParam;
};
float CCustomBuildingDNPipeline::m_fDNBalanceParam{};
float tColLighting::GetCurrentLighting(float fScale) const {
    const float day   = static_cast<float>(value >> 4);
    const float night = static_cast<float>(value & 0xF);
    return day * fScale * 0.06666667f * CCustomBuildingDNPipeline::m_fDNBalanceParam
         + (1.0f - CCustomBuildingDNPipeline::m_fDNBalanceParam) * night * fScale * 0.06666667f;
}

// Static data (were StaticRef<> at fixed game addresses; see CBulletInfo.h)
std::array<CBulletInfo, CBulletInfo::MAX_BULLET_INFOS> CBulletInfo::aBulletInfos{}; // 0xC88740
CVector CBulletInfo::PlayerSniperBulletStart{}; // 0xC888A0
CVector CBulletInfo::PlayerSniperBulletEnd{};   // 0xC888AC

// 0x735FD0
void CBulletInfo::Initialise() {
    for (auto& info : aBulletInfos) {
        info.m_bExists      = false;
        info.m_nWeaponType  = WEAPON_PISTOL;
        info.m_nDestroyTime = 0.0f;
        info.m_pCreator     = nullptr;
    }
}

// 0x736000
void CBulletInfo::Shutdown() {
    // NOP
}

// NOTSA
CBulletInfo* CBulletInfo::GetFree() {
    for (auto& info : aBulletInfos) {
        if (!info.m_bExists) {
            return &info;
        }
    }
    return nullptr;
}

// NOTSA
bool CBulletInfo::IsTimeToBeDestroyed() const noexcept {
    return static_cast<float>(CTimer::GetTimeInMS()) > m_nDestroyTime;
}

// 0x736010
void CBulletInfo::AddBullet(CEntity* creator, eWeaponType weaponType, CVector position, CVector velocity) {
    if (auto* info = GetFree()) {
        info->m_pCreator     = creator;
        info->m_nWeaponType  = weaponType;
        info->m_nDamage      = CWeaponInfo::GetWeaponInfo(weaponType, eWeaponSkill::STD)->m_nDamage;
        info->m_vecPosition  = position;
        info->m_vecVelocity  = velocity;
        info->m_nDestroyTime = static_cast<float>(CTimer::GetTimeInMS() + 1000);
        info->m_bExists      = true;
    }
}

// 0x7360D0
void CBulletInfo::Update() {
    for (auto& info : aBulletInfos) {
        if (info.IsTimeToBeDestroyed())
            info.m_bExists = false; /* next line checks */
        if (!info.m_bExists)
            continue;

        CVector newPosition = info.m_vecPosition + info.m_vecVelocity * (CTimer::GetTimeStep() / 2.0f);
        if (!CWorld::IsInWorldBounds(CVector2D{ newPosition.x, newPosition.y })) {
            info.m_bExists = false;
            continue;
        }

        CWorld::bIncludeDeadPeds = true;
        CWorld::bIncludeCarTyres = true;
        CWorld::bIncludeBikers   = true;
        CWorld::pIgnoreEntity    = info.m_pCreator;

        CColPoint colPoint;
        CEntity* hitEntity;
        if (CWorld::ProcessLineOfSight(info.m_vecPosition, newPosition, colPoint, hitEntity, true, true, true, true, true, false, false, true)) {
            CWeapon::CheckForShootingVehicleOccupant(&hitEntity, &colPoint, info.m_nWeaponType, info.m_vecPosition, newPosition);

            switch (hitEntity->GetType()) {
            case ENTITY_TYPE_PED: {
                auto* hitPed = hitEntity->AsPed();

                if (hitEntity != info.m_pCreator) {
                    if (hitPed->IsAlive()) {
                        CWeapon::GenerateDamageEvent(
                            hitPed,
                            info.m_pCreator,
                            info.m_nWeaponType,
                            info.m_nDamage,
                            static_cast<ePedPieceTypes>(colPoint.m_nPieceTypeB),
                            static_cast<uint8_t>(hitPed->GetLocalDirection(CVector2D{
                                hitPed->GetPosition().x - colPoint.m_vecPoint.x,
                                hitPed->GetPosition().y - colPoint.m_vecPoint.y
                            }))
                        );
                        CCrime::ReportCrime(
                            (hitPed->m_nPedType == PED_TYPE_COP) ? eCrimeType::CRIME_DAMAGE_COP_CAR : eCrimeType::CRIME_DAMAGE_CAR,
                            hitPed,
                            info.m_pCreator->AsPed()
                        );
                        newPosition = colPoint.m_vecPoint;
                    }
                }

                if (CLocalisation::Blood()) {
                    g_fx.AddBlood(colPoint.m_vecPoint, colPoint.m_vecNormal, 8, hitPed->m_fContactSurfaceBrightness);
                    if (hitPed->m_nPedState == PEDSTATE_DEAD) {
                        const auto anim = RpAnimBlendClumpGetFirstAssociation(hitPed->GetRpClump(), ANIMATION_IS_FRONT) ? ANIM_ID_FLOOR_HIT_F : ANIM_ID_FLOOR_HIT;

                        if (auto* assoc = CAnimManager::BlendAnimation(hitPed->GetRpClump(), ANIM_GROUP_DEFAULT, anim, 8.0f)) {
                            assoc->SetCurrentTime(0.0f);
                            assoc->SetFlag(ANIMATION_IS_FINISH_AUTO_REMOVE, false);
                        }
                    }
                    newPosition = colPoint.m_vecPoint;
                }
                break;
            }
            case ENTITY_TYPE_VEHICLE: {
                auto* hitVehicle = hitEntity->AsVehicle();

                if (info.m_pCreator && info.m_pCreator->GetIsTypePed())
                    if (info.m_pCreator->AsPed()->m_pAttachedTo == hitVehicle)
                        break;

                // Originally: if (colPoint.m_nPieceTypeB < 13u || colPoint.m_nPieceTypeB > 16u)
                switch (static_cast<eCarPiece>(colPoint.m_nPieceTypeB)) {
                default: /* originally `if` body */
                {
                    hitVehicle->InflictDamage(info.m_pCreator, info.m_nWeaponType, info.m_nDamage, colPoint.m_vecPoint);
                    if (info.m_nWeaponType == eWeaponType::WEAPON_FLAMETHROWER) {
                        gFireManager.StartFire(hitVehicle, info.m_pCreator, 0.8f, true, 7000, 100);
                    } else if (TheCamera.IsSphereVisible(colPoint.m_vecNormal, 1.0f)) {
                        g_fx.AddBulletImpact(colPoint.m_vecPoint, colPoint.m_vecNormal, static_cast<int32_t>(colPoint.m_nSurfaceTypeB), 8, colPoint.m_nLightingB.GetCurrentLighting());
                    }
                    break;
                }
                case eCarPiece::CAR_PIECE_WHEEL_LF:
                case eCarPiece::CAR_PIECE_WHEEL_RF:
                case eCarPiece::CAR_PIECE_WHEEL_RL:
                case eCarPiece::CAR_PIECE_WHEEL_RR: { /* originally `else` body */
                    hitVehicle->BurstTyre(colPoint.m_nPieceTypeB, true);
                    g_fx.AddTyreBurst(colPoint.m_vecPoint, colPoint.m_vecNormal);
                    break;
                }
                }

                break;
            }
            default: {
                if (TheCamera.IsSphereVisible(colPoint.m_vecNormal, 1.0f))
                    g_fx.AddBulletImpact(colPoint.m_vecPoint, colPoint.m_vecNormal, static_cast<int32_t>(colPoint.m_nSurfaceTypeB), 8, colPoint.m_nLightingB.GetCurrentLighting());

                if (info.m_pCreator && info.m_pCreator->GetIsTypePed())
                    if (info.m_pCreator->AsPed()->m_pAttachedTo == hitEntity)
                        break;

                switch (hitEntity->GetType()) {
                case ENTITY_TYPE_OBJECT: {
                    auto* hitObject = hitEntity->AsObject();

                    const auto DoDamageToObject = [&](float dmg) {
                        hitObject->ObjectDamage(dmg, &colPoint.m_vecPoint, &colPoint.m_vecNormal, info.m_pCreator, info.m_nWeaponType);
                    };

                    if (hitObject->m_nColDamageEffect < 200u) {
                        if (hitObject->physicalFlags.bDisableCollisionForce || hitObject->m_pObjectInfo->m_fColDamageMultiplier >= 99.9f) {
                            /* empty */
                        } else {
                            if (hitObject->GetIsStatic() && hitObject->m_pObjectInfo->m_fUprootLimit <= 0.0f) {
                                hitObject->SetIsStatic(false);
                                hitObject->AddToMovingList();
                            }
                            if (!hitObject->GetIsStatic()) {
                                hitObject->ApplyMoveForce(colPoint.m_vecNormal * -7.5f);
                            }
                        }
                    } else {
                        switch (hitObject->m_pObjectInfo->m_nGunBreakMode) {
                        case eObjectBreakMode::BY_GUN: {
                            DoDamageToObject(151.0f);
                            break;
                        }
                        case eObjectBreakMode::SMASHABLE: {
                            DoDamageToObject(hitObject->m_pObjectInfo->m_fSmashMultiplier * 151.0f);
                            break;
                        }
                        }
                    }
                    DoDamageToObject(50.0f);
                    break;
                }
                case ENTITY_TYPE_BUILDING: {
                    if (info.m_pCreator && info.m_pCreator->GetIsTypePed()) {
                        if (auto* playerData = info.m_pCreator->AsPed()->GetPlayerData()) {
                            playerData->m_nModelIndexOfLastBuildingShot = static_cast<int32_t>(hitEntity->GetModelIndex());
                        }
                    }
                    break;
                }
                }
            }
            }

            if (info.m_nWeaponType == eWeaponType::WEAPON_SNIPERRIFLE) {
                CVector dir = newPosition - info.m_vecPosition;
                dir.Normalise();
                const float dirDotColPointNorm = DotProduct(dir, colPoint.m_vecNormal);
                if (dirDotColPointNorm < 0.0f) {
                    AudioEngine.ReportBulletHit(hitEntity, static_cast<uint8_t>(colPoint.m_nSurfaceTypeB), colPoint.m_vecPoint, RadiansToDegrees(std::asin(-dirDotColPointNorm)));
                }
            }
            CGlass::WasGlassHitByBullet(hitEntity, colPoint.m_vecPoint);
        }

        CWorld::bIncludeDeadPeds = false;
        CWorld::bIncludeCarTyres = false;
        CWorld::bIncludeBikers   = false;
        CWorld::pIgnoreEntity    = nullptr;

        if (info.m_nWeaponType == eWeaponType::WEAPON_SNIPERRIFLE) {
            PlayerSniperBulletStart = info.m_vecPosition;
            PlayerSniperBulletEnd = newPosition;
            CHeli::TestSniperCollision(&PlayerSniperBulletStart, &PlayerSniperBulletEnd);
        }

        info.m_vecPosition = newPosition;
    }
}
