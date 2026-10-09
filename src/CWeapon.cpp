// CWeapon - ported from decompiled C (src/CWeapon/*.c) with guidance from
// gta-reversed/source/game_sa/Weapon.cpp. Fills method bodies 1:1; see
// BUILD_NOTES.md for divergences.

#include "CWeapon.h"

#include "CTimer.h"
#include "CEntity.h"
#include "CPed.h"
#include "CPlayerPed.h"
#include "CPlayerInfo.h"
#include "CVehicle.h"
#include "CObject.h"
#include "CWorld.h"
#include "CPad.h"
#include "CExplosion.h"
#include "CFireManager.h"
#include "CBulletInfo.h"
#include "CWeaponInfo.h"
#include "ColTypes.h"
#include "CMatrix.h"
#include "CColModel.h"
#include "CColSphere.h"
#include "CCollisionData.h"
// NOTE: AnimTypes.h arrives via CPed.h -> CPedModelInfo.h (the old
// AssocGroupId/CQuaternion conflicts were deduped 2026-10-09).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

// ============================================================================
// TODO(port): external subsystem shims. Minimal declarations verified against
// gta-reversed; delete entries as subsystems are ported.
// ============================================================================

// --- CWeaponInfo is in include/CWeaponInfo.h (extended 2026-10-09 for CWeapon).

// --- eCrimeType (values verified against gta-reversed/source/game_sa/Crime.h)
enum class eCrimeType : int32_t {
    CRIME_NONE = 0,
    CRIME_FIRE_WEAPON = 1,
    CRIME_DAMAGE_CAR = 2,
    CRIME_DAMAGED_PED = 4,
    CRIME_EXPLOSION = 5,
    CRIME_DAMAGE_COP_CAR = 13,
    CRIME_DAMAGE_COP_PED = 14,
    CRIME_DAMAGE_PED = 15,
    CRIME_DAMAGE_EMERGENCY_CAR = 17,
};
struct CCrime {
    static void ReportCrime(eCrimeType crimeType, CEntity* victim, CPed* criminal);
};

// (local eStats removed 2026-10-09: canonical enum lives in include/eStats.h, gta-reversed-verified)

struct CStats {
    static float GetStatValue(int32_t statId);
    static void  IncrementStat(int32_t statId, float value = 1.0f);
    static void  UpdateStatsWhenWeaponHit(eWeaponType weaponType);
    static float GetPercentageProgress();
};

// --- CCheat (gta-reversed source/game_sa/Cheat.h)
enum eCheat {
    CHEAT_INFINITE_AMMO = 25,
};
struct CCheat {
    static bool IsActive(eCheat cheat);
};

// --- events (gta-reversed source/game_sa/Events/)
class CEvent {
public:
    virtual ~CEvent() = default;
};
CEventGroup* GetEventGlobalGroup();

class CEventDamage : public CEvent {
public:
    struct DamageResponse {
        bool m_bHealthZero = false;
    };
    DamageResponse m_damageResponse;
    int32_t  m_nAnimID{};
    int32_t  m_nAnimGroup{};
    float    m_fAnimBlend{};
    float    m_fAnimSpeed{};
    bool     m_bStealthMode{};

    CEventDamage(CEntity* creator, uint32_t time, eWeaponType weaponType,
                 ePedPieceTypes pedPiece, uint8_t direction, bool bFallDown, bool bInVehicle);
    bool AffectsPed(CPed* victim);
    void ComputeAnim(CPed* victim, bool bForced);
};

class CEventGunShot : public CEvent {
public:
    CEventGunShot(CEntity* shooter, CVector startPoint, CVector endPoint, bool bSilent);
};
class CEventGunShotWhizzedBy : public CEvent {
public:
    CEventGunShotWhizzedBy(CEntity* shooter, const CVector& startPoint, const CVector& endPoint, bool bSilent);
};
class CEventVehicleDamageWeapon : public CEvent {
public:
    CEventVehicleDamageWeapon(CVehicle* vehicle, CEntity* creator, eWeaponType weaponType);
};

class CPedDamageResponseCalculator {
public:
    CPedDamageResponseCalculator(CEntity* creator, float damage, eWeaponType weaponType,
                                 ePedPieceTypes pedPiece, bool bMelee);
    void ComputeDamageResponse(CPed* victim, CEventDamage::DamageResponse& response, bool bForce);
};

// --- CAnimManager / anim blend (gta-reversed source/game_sa/Animation/)
// NOTE: AssocGroupId + AnimationId come from AnimTypes.h (via CPed.h ->
// CPedModelInfo.h). The local AnimationId definition was removed 2026-10-09:
// it redefined AnimTypes.h's enum, and its values (0-6) were wrong - the
// decomp's real values (src_prev_export/_types.h) are NO_ANIMATION_SET=191,
// DOOR_LHINGE_O=192, RELOAD=226, TURN_L=137, TURN_R=138.
constexpr int32_t BONE_HEAD_ID = 5;
constexpr int32_t BONE_SPINE1_ID = 3;
struct RpClump;
struct RpHAnimHierarchy;
struct CAnimBlendAssociation {
    float m_BlendAmount{};
    float m_BlendDelta{};
    float m_Speed{};
    float m_CurrentTime{};
    float m_TimeStep{};
    void Start();
    float GetTimeProgress() const;
    void SetFlag(uint32_t flag, bool bValue);
};
struct CAnimManager {
    static CAnimBlendAssociation* AddAnimation(RpClump* clump, AssocGroupId groupId, AnimationId animId);
    static CAnimBlendAssociation* BlendAnimation(RpClump* clump, AssocGroupId groupId, AnimationId animId, float blendDelta = 1.0f);
};
CAnimBlendAssociation* RpAnimBlendClumpGetAssociation(RpClump* clump, int32_t animId);
CAnimBlendAssociation* RpAnimBlendClumpGetFirstAssociation(RpClump* clump, uint32_t flag);

// --- CLocalisation (gta-reversed source/game_sa/Localisation.h)
struct CLocalisation {
    static bool Blood();
    static bool KickingWhenDown();
};

// --- CPedGroups (gta-reversed source/game_sa/PedGroups.h)
struct CPedGroups {
    static bool AreInSameGroup(CPed* ped1, CPed* ped2);
};

// --- CInterestingEvents (gta-reversed source/game_sa/InterestingEvents.h)
struct CInterestingEvents {
    enum class EType {
        INTERESTING_EVENT_22 = 22,
    };
    void Add(EType type, CEntity* entity);
};
extern CInterestingEvents g_InterestingEvents;

// --- CGlass (gta-reversed source/game_sa/Glass.h)
struct CGlass {
    static void WasGlassHitByBullet(CEntity* entity, const CVector& point);
};

// --- CBulletTraces (gta-reversed source/game_sa/BulletTraces.h)
struct CBulletTraces {
    static void AddTrace(const CVector& start, const CVector& end, eWeaponType weaponType, CEntity* creator);
    static void AddTrace(const CVector& start, const CVector& end, float size, uint32_t lifeTime, uint8_t brightness);
};

// --- CPointLights (gta-reversed source/game_sa/PointLights.h)
enum ePointLightType : int32_t {
    PLTYPE_POINTLIGHT = 0,
};
struct CPointLights {
    static void AddLight(ePointLightType type, const CVector& posn, const CVector& dirn,
                         float radius, float red, float green, float blue,
                         int32_t fogType = 0, bool bGenerateShadows = false, CEntity* entity = nullptr);
};

// --- CCoronas (gta-reversed source/game_sa/Coronas.h)
struct CCoronas {
    static int32_t MoonSize;
};

// --- CBirds (gta-reversed source/game_sa/Birds.h)
struct CBirds {
    static void HandleGunShot(const CVector* startPoint, const CVector* endPoint);
};

// --- CWaterLevel (gta-reversed source/game_sa/WaterLevel.h)
struct CWaterLevel {
    static bool GetWaterLevel(float x, float y, float z, float& outWaterZ, bool bCheckWaves, void* pV);
};
bool TestLineAgainstWater(float x, float y);

// --- CGeneral (gta-reversed source/game_sa/General.h)
struct CGeneral {
    static uint32_t GetRandomNumber();
    static float GetRandomNumberInRange(float min, float max);
    static int32_t GetRandomNumberInRange(int32_t min, int32_t max);
    static bool RandomBool(float probabilityPercent);
};

// --- CCollision (gta-reversed source/game_sa/Collision.h)
// Only the functions used by CWeapon are declared; types come from real headers.
struct CCollision {
    static float DistToLine(const CVector* lineStart, const CVector* lineEnd, const CVector* point);
    static bool ProcessLineSphere(const CColLine* line, const CColSphere* sphere,
                                  CColPoint* colPoint, float* maxDist);
    static void ProcessColModels(const CMatrix* matA, const CColModel* colA, const CMatrix* matB,
                                 const CColModel* colB, CColPoint* colPoints, CColPoint* colPointsB,
                                 float* outDepths, bool bCheckLines);
};

// CColLine: canonical in CColLine.h (deduped 2026-10-09; the local struct
// here redefined it with the wrong 2-member layout - real has 4 members).
#include "CColLine.h"

// --- CProjectileInfo (gta-reversed source/game_sa/ProjectileInfo.h)
struct CProjectileInfo {
    static void AddProjectile(CEntity* creator, eWeaponType weaponType, const CVector& posn,
                              float force, const CVector* dirn, CEntity* target);
    static void RemoveNotAdd(CEntity* creator, eWeaponType weaponType, const CVector& posn);
    static void Update();
};

// --- CShotInfo (gta-reversed source/game_sa/ShotInfo.h)
struct CShotInfo {
    static void AddShot(CEntity* creator, eWeaponType weaponType, const CVector& start, const CVector& end);
    static void Update();
};



// --- entity pools (gta-reversed source/game_sa/Pools.h)
// TODO(pools): real pool iteration; currently a no-op range so weapons_combat
// TUs compile. Range-for syntax matches gta-reversed.
// (Named CWeaponPool_ to avoid clashing with the real CPool<T,N> in CPool.h.)
template<typename T>
struct CWeaponPool_ {
    struct Iterable {
        struct Iterator {
            bool operator!=(const Iterator&) const { return false; }
            T& operator*();
            Iterator& operator++() { return *this; }
        };
        Iterator begin() { return {}; }
        Iterator end() { return {}; }
    };
    Iterable GetAllValid() { return {}; }
};
CWeaponPool_<CPed>* GetPedPool();
CWeaponPool_<CVehicle>* GetVehiclePool();
CWeaponPool_<CObject>* GetObjectPool();

// --- CDarkel (gta-reversed source/game_sa/Darkel.h)
struct CDarkel {
    static bool ThisPedShouldBeKilledForFrenzy(CPed& ped);
    static bool ThisVehicleShouldBeKilledForFrenzy(CVehicle& vehicle);
};

// --- CPickups (gta-reversed source/game_sa/Pickups.h)
struct CPickups {
    static void PictureTaken();
};

// --- CSprite (gta-reversed source/game_sa/Sprite.h)
struct CSprite {
    static bool CalcScreenCoors(const CVector& worldPos, CVector* outScreenPos,
                                float* outW, float* outH, bool bCheckMaxVisible, bool bCheckMinVisible);
};
extern float SCREEN_WIDTH;
extern float SCREEN_HEIGHT;

// --- camera (CCamera.h is shimmed out due to tree conflicts; minimal stand-in)
enum eCamMode : int32_t {
    MODE_M16_1STPERSON = 53,
    MODE_SNIPER = 55,
    MODE_CAMERA = 46,
    MODE_ROCKETLAUNCHER = 58,
    MODE_ROCKETLAUNCHER_HS = 59,
    MODE_M16_1STPERSON_RUNABOUT = 80,
    MODE_SNIPER_RUNABOUT = 82,
    MODE_ROCKETLAUNCHER_RUNABOUT = 83,
    MODE_ROCKETLAUNCHER_RUNABOUT_HS = 84,
    MODE_HELICANNON_1STPERSON = 51,
    MODE_TWOPLAYER_IN_CAR_AND_SHOOTING = 60,
};
struct CCam {
    int32_t m_nMode{};
    CVector m_vecSource{};
    CVector m_vecFront{};
    float   m_fHorizontalAngle{};
    float   m_fVerticalAngle{};
    bool Using3rdPersonMouseCam() const;
    void Get_TwoPlayer_AimVector(CVector* outVec);
};
struct CCamera {
    CCam m_aCams[3];
    struct {
        int32_t m_nMode{};
    } m_PlayerWeaponMode;
    static CCam& GetActiveCamera();
    static CCam& GetActiveCam();
    CVector& GetPosition();
    CMatrix& GetMatrix();
    void Find3rdPersonCamTargetVector(float range, const CVector& source, CVector* outCamPos, CVector* outTarget);
    bool IsSphereVisible(const CVector& posn, float radius);
};
extern CCamera TheCamera;
void CamShakeNoPos(CCamera* camera, float intensity);



// --- CPlayerPedData (gta-reversed source/game_sa/PlayerPedData.h)
struct CPlayerPedData {
    float   m_fAttackButtonCounter{};
    CVector m_vecTargetBoneOffset{};
    int32_t m_nTargetBone{};
    uint32_t m_nFireHSMissilePressedTime{};
    CEntity* m_LastHSMissileTarget{};
    int32_t m_nModelIndexOfLastBuildingShot{};
    float   m_fLookPitch{};
};


// --- CTaskSimpleUseGun (gta-reversed source/game_sa/Tasks/)
// (CPedIntelligence.h forward-declares CTaskSimpleUseGun and CTask; completed here.
// eTaskType is opaque in CPedIntelligence.h; enumerators defined here for this TU.)
// (CPlayerInfo was a local shim here; full definition now in include/CPlayerInfo.h)

struct CTaskSimpleUseGun {
    bool m_SkipAim{};
};
struct CTask {
    virtual int32_t GetTaskType() const;
};

// --- CWeaponEffects (gta-reversed source/game_sa/WeaponEffects.h)
enum eWeaponEffectsFlags {
    WEAPONEFFECTS_LOCK_ON = 0,
};
struct CWeaponEffects {
    static bool IsLockedOn(int32_t flag);
};

// --- crosshair (gta-reversed source/game_sa/CrossHair.h)
struct CCrossHair {
    struct { uint8_t r, g, b, a; } m_color;
    float m_fRotation{};
    uint32_t m_nTimeWhenToDeactivate{};
};
extern CCrossHair gCrossHair[1];

// --- CCreepingFire (gta-reversed source/game_sa/CreepingFire.h)
struct CCreepingFire {
    static bool TryToStartFireAtCoors(const CVector& posn, uint32_t arg1, bool bArg2, bool bArg3, float strength);
};

// --- eVehicleType and eCarPiece_WheelPieces come from CVehicle.h (included above).

// --- audio (CAudioEngine.h is shimmed out due to tree conflicts)
enum eAudioEvents : int32_t {
    AE_WEAPON_FIRE = 0,
    AE_WEAPON_RELOAD_A = 0,
    AE_WEAPON_RELOAD_B = 0,
    AE_EXPLOSION = 129,
};
struct CAudioEngine {
    void ReportBulletHit(CEntity* entity, uint32_t surfaceType, const CVector& posn, float angle);
};
extern CAudioEngine AudioEngine;

// --- fx (FxManager.h/FxSystem.h are shimmed out due to tree conflicts)
struct RwMatrix;
struct FxPrtMult_c {
    FxPrtMult_c(float r, float g, float b, float a, float size, float arg5, float arg6);
    void SetColor(float r, float g, float b);
};
struct Fx_c {
    struct CGunShell {
        void AddParticle(const CVector& posn, const CVector& velocity, float arg, const FxPrtMult_c& fxprt);
    };
    CGunShell* m_GunShell{};
    void AddBulletImpact(const CVector& posn, const CVector& normal, uint32_t surfaceType,
                         int32_t arg, uint8_t lighting);
    void AddBlood(const CVector& posn, const CVector& normal, int32_t arg, float brightness);
    void AddTyreBurst(const CVector& posn, const CVector& normal);
    void TriggerGunshot();
    void TriggerBulletSplash(const CVector& posn);
    void CreateMatFromVec(RwMatrix* mat, const CVector* origin, const CVector* dir);
};
extern Fx_c g_fx;
struct FxSystem_c {
    void Kill();
    void SetMatrix(RwMatrix* mat);
    void CopyParentMatrix();
    void Play();
    void PlayAndKill();
    void SetMustCreatePrts(bool b);
    void SetConstTime(int32_t arg, float time);
};
struct FxManager_c {
    FxSystem_c* CreateFxSystem(const char* name, const CVector& posn, RwMatrix* mat, bool bArg);
};
extern FxManager_c g_fxMan;
RwMatrix* RwMatrixCreate();
void RwMatrixDestroy(RwMatrix* mat);

// --- RenderWare anim plumbing (for drive-by bone transforms)
RpHAnimHierarchy* GetAnimHierarchyFromSkinClump(RpClump* clump);
int32_t RpHAnimIDGetIndex(RpHAnimHierarchy* hier, int32_t boneId);
// RpHAnimHierarchyGetMatrixArray: canonical decl in RenderWare.h (deduped 2026-10-09;
// the local void* decl here redefined it).
void RwV3dTransformPoints(CVector* outPts, const CVector* inPts, int32_t numPts, void* matrix);

// --- animation flags (values verified against gta-reversed/source/game_sa/Animation/AnimBlendAssociation.h)
constexpr uint32_t ANIMATION_IS_FRONT = 1u << 11;
constexpr uint32_t ANIMATION_IS_FINISH_AUTO_REMOVE = 1u << 3;
constexpr uint32_t ANIMATION_IS_PLAYING = 1u << 0; // TODO: verify value

// --- CWorld extras not in include/CWorld.h yet
namespace CWorld_Extra {
    void FindObjectsInRange(const CVector& posn, float range, bool bIncludeDead,
                            int16_t* outCount, int16_t maxCount, CEntity** outEntities,
                            bool bVehicles, bool bPeds, bool bObjects, bool bDummies1, bool bDummies2);
    bool GetIsLineOfSightClear(const CVector& start, const CVector& end,
                               bool bBuildings, bool bVehicles, bool bPeds, bool bObjects);
    bool TestSphereAgainstWorld(const CVector& posn, float radius, CEntity* ignoreEntity,
                                bool bBuildings, bool bVehicles, bool bPeds, bool bObjects,
                                bool bDummies1, bool bDummies2);
    void UseDetonator(CPed* ped);
    void ResetLineTestOptions();
}

// --- speech context
enum eSpeechContext {
    CTX_GLOBAL_SHOOT = 0,
};

// ============================================================================
// statics
// ============================================================================
float     CWeapon::ms_fExtinguisherAimAngle = -0.34907f; // -pi/8 // 0x8D610C
bool      CWeapon::bPhotographHasBeenTaken = false;      // 0xC8A7C0
bool      CWeapon::ms_bTakePhoto = false;                // 0xC8A7C1
CColModel CWeapon::ms_PelletTestCol;                     // 0xC8A7DC

// 0x73B430
CWeapon::CWeapon(eWeaponType weaponType, uint32_t ammo) {
    Constructor(weaponType, ammo);
}

// 0x73B4A0
void CWeapon::Initialise(eWeaponType weaponType, int32_t ammo, CPed* owner) {
    m_Type = weaponType;
    m_State = eWeaponState::WEAPONSTATE_READY;
    m_TimeForNextShotMs = 0;
    m_AmmoInClip = 0;
    m_TotalAmmo = 0;
    m_IsFirstPersonWeaponModeSelected = false;
    m_DontPlaceInHand = false;
    m_FxSystem = nullptr;

    switch (weaponType) {
    case eWeaponType::WEAPON_UNARMED:
    case eWeaponType::WEAPON_BRASSKNUCKLE:
    case eWeaponType::WEAPON_GOLFCLUB:
    case eWeaponType::WEAPON_NIGHTSTICK:
    case eWeaponType::WEAPON_KNIFE:
    case eWeaponType::WEAPON_BASEBALLBAT:
    case eWeaponType::WEAPON_SHOVEL:
    case eWeaponType::WEAPON_POOL_CUE:
    case eWeaponType::WEAPON_KATANA:
    case eWeaponType::WEAPON_CHAINSAW:
    case eWeaponType::WEAPON_DILDO1:
    case eWeaponType::WEAPON_DILDO2:
    case eWeaponType::WEAPON_VIBE1:
    case eWeaponType::WEAPON_VIBE2:
    case eWeaponType::WEAPON_FLOWERS:
    case eWeaponType::WEAPON_CANE:
        break;
    default:
        Reload(owner);
        break;
    }
    m_TotalAmmo = (uint32_t)ammo;
}

// 0x73A300
void CWeapon::InitialiseWeapons() {
    ms_fExtinguisherAimAngle = -0.34907f; // -pi/8
    bPhotographHasBeenTaken = false;
    ms_bTakePhoto = false;
}

// 0x73A330
void CWeapon::ShutdownWeapons() {
    // nothing
}

// 0x73A380
void CWeapon::Shutdown() {
    StopWeaponEffect();
    m_Type = eWeaponType::WEAPON_UNARMED;
    m_State = eWeaponState::WEAPONSTATE_READY;
    m_TimeForNextShotMs = 0;
    m_AmmoInClip = 0;
    m_TotalAmmo = 0;
    m_IsFirstPersonWeaponModeSelected = false;
    m_DontPlaceInHand = false;
}

// 0x73AEB0
void CWeapon::Reload(CPed* owner) {
    if (!m_TotalAmmo) {
        return;
    }

    uint32_t ammo = GetWeaponInfo(owner).m_nAmmoClip;
    m_AmmoInClip = std::min(ammo, m_TotalAmmo);
}

// 0x73B1C0
bool CWeapon::IsTypeMelee() {
    return GetWeaponInfo().m_nWeaponFire == eWeaponFire::WEAPON_FIRE_MELEE;
}

// 0x73B1E0
bool CWeapon::IsType2Handed() {
    switch (m_Type) {
    case eWeaponType::WEAPON_M4:
    case eWeaponType::WEAPON_AK47:
    case eWeaponType::WEAPON_SPAS12_SHOTGUN:
    case eWeaponType::WEAPON_SHOTGUN:
    case eWeaponType::WEAPON_SNIPERRIFLE:
    case eWeaponType::WEAPON_FLAMETHROWER:
    case eWeaponType::WEAPON_COUNTRYRIFLE:
        return true;
    }
    return false;
}

// 0x73B210
bool CWeapon::IsTypeProjectile() {
    switch (m_Type) {
    case eWeaponType::WEAPON_GRENADE:
    case eWeaponType::WEAPON_REMOTE_SATCHEL_CHARGE:
    case eWeaponType::WEAPON_TEARGAS:
    case eWeaponType::WEAPON_MOLOTOV:
    case eWeaponType::WEAPON_FREEFALL_BOMB:
        return true;
    }
    return false;
}

// 0x73B240
bool CWeapon::CanBeUsedFor2Player(eWeaponType weaponType) {
    switch (weaponType) {
    case eWeaponType::WEAPON_CHAINSAW:
    case eWeaponType::WEAPON_SNIPERRIFLE:
    case eWeaponType::WEAPON_RLAUNCHER:
    case eWeaponType::WEAPON_PARACHUTE:
        return false;
    }
    return true;
}

// 0x73DEF0
bool CWeapon::CanBeUsedFor2Player() {
    return CanBeUsedFor2Player(m_Type);
}

// 0x73B2A0
bool CWeapon::HasWeaponAmmoToBeUsed() {
    switch (m_Type) {
    case eWeaponType::WEAPON_UNARMED:
    case eWeaponType::WEAPON_BRASSKNUCKLE:
    case eWeaponType::WEAPON_GOLFCLUB:
    case eWeaponType::WEAPON_NIGHTSTICK:
    case eWeaponType::WEAPON_KNIFE:
    case eWeaponType::WEAPON_BASEBALLBAT:
    case eWeaponType::WEAPON_KATANA:
    case eWeaponType::WEAPON_CHAINSAW:
    case eWeaponType::WEAPON_DILDO1:
    case eWeaponType::WEAPON_DILDO2:
    case eWeaponType::WEAPON_VIBE1:
    case eWeaponType::WEAPON_VIBE2:
    case eWeaponType::WEAPON_FLOWERS:
    case eWeaponType::WEAPON_PARACHUTE:
        return true;
    }
    return m_TotalAmmo != 0;
}

// 0x73B300
bool CWeapon::ProcessLineOfSight(const CVector& startPoint, const CVector& endPoint, CColPoint& outColPoint, CEntity*& outEntity, eWeaponType weaponType, CEntity* arg5,
                                 bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool arg11, bool doIgnoreCameraCheck) {
    CBirds::HandleGunShot(&startPoint, &endPoint);
    // CShadows::GunShotSetsOilOnFire(startPoint, endPoint); // TODO(shadows)
    return CWorld::ProcessLineOfSight(startPoint, endPoint, outColPoint, outEntity, buildings, vehicles, peds, objects, dummies, false, doIgnoreCameraCheck, true);
}

// 0x73B360
void CWeapon::StopWeaponEffect() {
    if (m_FxSystem && m_Type != eWeaponType::WEAPON_MOLOTOV) {
        m_FxSystem->Kill();
        m_FxSystem = nullptr;
    }
}

// 0x73B380
float CWeapon::TargetWeaponRangeMultiplier(CEntity* target, CEntity* weaponOwner) {
    if (!target || !weaponOwner) {
        return 1.0f;
    }

    switch (target->GetType()) {
    case ENTITY_TYPE_VEHICLE: {
        if (!target->AsVehicle()->IsBike()) {
            return 3.0f;
        }
        break;
    }
    case ENTITY_TYPE_PED: {
        CPed* pedVictim = target->AsPed();

        if (pedVictim->m_pVehicle && !pedVictim->m_pVehicle->IsBike()) {
            return 3.0f;
        }

        if (CEntity* attachedTo = pedVictim->m_pAttachedTo) {
            if (attachedTo->GetIsTypeVehicle() && !attachedTo->AsVehicle()->IsBike()) {
                return 3.0f;
            }
        }

        break;
    }
    }

    if (!weaponOwner->GetIsTypePed() || !weaponOwner->AsPed()->IsPlayer()) {
        return 1.0f;
    }

    switch (CCamera::GetActiveCamera().m_nMode) {
    case MODE_TWOPLAYER_IN_CAR_AND_SHOOTING:
        return 2.0f;
    case MODE_HELICANNON_1STPERSON:
        return 3.0f;
    }

    return 1.0f;
}

CWeaponInfo& CWeapon::GetWeaponInfo(CPed* owner) const {
    return GetWeaponInfo(owner ? owner->GetWeaponSkill(GetType()) : eWeaponSkill::STD);
}

CWeaponInfo& CWeapon::GetWeaponInfo(eWeaponSkill skill) const {
    return *CWeaponInfo::GetWeaponInfo(GetType(), skill);
}

//! @notsa
float CWeapon::GetWeaponRange(CPed* owner, CEntity* target) const noexcept {
    const auto r = GetWeaponInfo(owner).m_fTargetRange;
    if (target) {
        return r * TargetWeaponRangeMultiplier(target, owner);
    }
    return r;
}

//! @notsa
auto CWeapon::GetProjectileType() {
    switch (GetType()) {
    case eWeaponType::WEAPON_RLAUNCHER:
        return eWeaponType::WEAPON_ROCKET;
    case eWeaponType::WEAPON_RLAUNCHER_HS:
        return eWeaponType::WEAPON_ROCKET_HS;
    case eWeaponType::WEAPON_GRENADE:
    case eWeaponType::WEAPON_TEARGAS:
    case eWeaponType::WEAPON_MOLOTOV:
    case eWeaponType::WEAPON_REMOTE_SATCHEL_CHARGE:
        return GetType();
    default:
        return eWeaponType::WEAPON_UNARMED; // NOTSA_UNREACHABLE placeholder
    }
}

// 0x73A3E0
void CWeapon::AddGunshell(CEntity* creator, CVector& position, const CVector2D& direction, float size) {
    if (!creator || !creator->GetIsOnScreen()) {
        return;
    }

    // originally squared
    if ((creator->GetPosition() - TheCamera.GetPosition()).SquaredMagnitude() > 100.0f) {
        return;
    }

    // Add gunshell fx particle
    FxPrtMult_c fxprt(0.5f, 0.5f, 0.5f, 1.0f, size, 1.0f, 1.0f);
    switch (m_Type) {
    case eWeaponType::WEAPON_SPAS12_SHOTGUN:
    case eWeaponType::WEAPON_SHOTGUN:
        fxprt.SetColor(0.6f, 0.1f, 0.1f);
    }
    g_fx.m_GunShell->AddParticle(position, { direction.x, direction.y, CGeneral::GetRandomNumberInRange(0.4f, 1.6f) }, 0.0f, fxprt);
}

// 0x73A8D0
bool CWeapon::LaserScopeDot(CVector* outCoord, float* outSize) {
    /* UNUSED */
    return false; // NOTSA_UNREACHABLE placeholder
}

// 0x73AAC0
bool CWeapon::FireSniper(CPed* shooter, CEntity* victim, CVector* target) {
    const CCam& activeCam = CCamera::GetActiveCamera();

    if (FindPlayerPed() == shooter) {
        switch (activeCam.m_nMode) {
        case MODE_M16_1STPERSON:
        case MODE_SNIPER:
        case MODE_CAMERA:
        case MODE_ROCKETLAUNCHER:
        case MODE_ROCKETLAUNCHER_HS:
        case MODE_M16_1STPERSON_RUNABOUT:
        case MODE_SNIPER_RUNABOUT:
        case MODE_ROCKETLAUNCHER_RUNABOUT:
        case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
            break;
        default:
            return false;
        }
    }

    // todo: make sense of literals.
    float vecFrontZ_Y = activeCam.m_vecFront.z * 0.145f - activeCam.m_vecFront.y * 0.98940003f;

    if (vecFrontZ_Y > 0.99699998f)
        CCoronas::MoonSize = (CCoronas::MoonSize + 1) % 8;

    CVector velocity = activeCam.m_vecFront;
    velocity.Normalise();
    velocity *= 16.0f;

    CBulletInfo::AddBullet(shooter, m_Type, activeCam.m_vecSource, velocity);

    // recoil effect for players
    if (shooter->IsPlayer()) {
        CVector creatorPos = FindPlayerCoors();
        CPad* creatorPad = CPad::GetPad(shooter->m_nPedType);

        creatorPad->StartShake_Distance(240, 128, creatorPos);
        CamShakeNoPos(&TheCamera, 0.2f);
    }

    if (shooter->GetIsTypePed()) {
        CCrime::ReportCrime(eCrimeType::CRIME_FIRE_WEAPON, shooter, shooter);
    } else if (shooter->GetIsTypeVehicle() && shooter->AsPed()->m_roadRageWith) {
        CCrime::ReportCrime(eCrimeType::CRIME_FIRE_WEAPON, shooter, shooter->AsPed()->m_roadRageWith);
    }

    CVector targetPoint = velocity * 40.0f + activeCam.m_vecSource;
    bool hasNoSound = m_Type == eWeaponType::WEAPON_PISTOL_SILENCED || m_Type == eWeaponType::WEAPON_TEARGAS;
    CEventGroup* eventGroup = GetEventGlobalGroup();

    CEventGunShot gs(shooter, activeCam.m_vecSource, targetPoint, hasNoSound);
    eventGroup->Add(static_cast<CEvent*>(&gs), false);

    CEventGunShotWhizzedBy gsw(shooter, activeCam.m_vecSource, targetPoint, hasNoSound);
    eventGroup->Add(static_cast<CEvent*>(&gsw), false);

    g_InterestingEvents.Add(CInterestingEvents::EType::INTERESTING_EVENT_22, shooter);

    return true;
}

// 0x73A530
bool CWeapon::GenerateDamageEvent(CPed* victim, CEntity* creator, eWeaponType weaponType, int32_t damageFactor, ePedPieceTypes pedPiece, uint8_t direction) {
    CPedDamageResponseCalculator pedDmgRespCalc{
        creator,
        (float)damageFactor,
        weaponType,
        pedPiece,
        false
    };

    CEventDamage eventDmg{
        creator,
        CTimer::GetTimeInMS(),
        weaponType,
        pedPiece,
        direction,
        false,
        victim->bInVehicle
    };

    if (   victim->m_fHealth <= 0.f
        && CLocalisation::Blood()
        && CLocalisation::KickingWhenDown()
        && victim->GetTaskManager().GetSimplestActiveTask()->GetTaskType() == TASK_SIMPLE_DEAD
    ) {
        const auto floorHitAnim = CAnimManager::BlendAnimation(
            victim->GetRpClump(),
            ANIM_GROUP_DEFAULT,
            RpAnimBlendClumpGetFirstAssociation(victim->GetRpClump(), ANIMATION_IS_FRONT)
                ? ANIM_ID_FLOOR_HIT_F
                : ANIM_ID_FLOOR_HIT
        );
        if (floorHitAnim) {
            floorHitAnim->SetFlag(ANIMATION_IS_FINISH_AUTO_REMOVE, false);
            floorHitAnim->Start();
        }
        return true;
    }

    if (!victim->IsAlive()) {
        return true;
    }

    if (!eventDmg.AffectsPed(victim)) { // 0x73A687
        return false;
    }

    if (creator == FindPlayerPed()) {
        CCrime::ReportCrime(eCrimeType::CRIME_DAMAGED_PED, victim, static_cast<CPed*>(creator));
    }

    pedDmgRespCalc.ComputeDamageResponse(
        victim,
        eventDmg.m_damageResponse,
        true
    );

    bool ret = true;
    if (!victim->bInVehicle && (
           CWeaponInfo::GetWeaponInfo(weaponType)->m_nWeaponFire == eWeaponFire::WEAPON_FIRE_MELEE
        || weaponType == eWeaponType::WEAPON_FALL && creator && creator->GetIsTypeObject()
    )) { // 0x73A6F1
        eventDmg.ComputeAnim(victim, true);
        switch (eventDmg.m_nAnimID) {
        case ANIM_ID_SHOT_PARTIAL:
        case ANIM_ID_SHOT_LEFTP:
        case ANIM_ID_SHOT_PARTIAL_B:
        case ANIM_ID_SHOT_RIGHTP: { //> 0x73A769 - Inverted
            auto anim = RpAnimBlendClumpGetAssociation(victim->GetRpClump(), eventDmg.m_nAnimID);
            if (!anim) {
                anim = CAnimManager::AddAnimation(
                    victim->GetRpClump(),
                    (AssocGroupId)eventDmg.m_nAnimGroup,
                    (AnimationId)eventDmg.m_nAnimID
                );
            }
            anim->m_BlendAmount = 0.f;
            anim->m_BlendDelta = eventDmg.m_fAnimBlend;
            anim->m_Speed = eventDmg.m_fAnimSpeed;
            anim->Start();
            break;
        }
        case ANIM_ID_NO_ANIMATION_SET:
            break;
        case ANIM_ID_DOOR_LHINGE_O:
            ret = false;
            break;
        default: { //< 0x73A7B5
            const auto a = CAnimManager::BlendAnimation(
                victim->GetRpClump(),
                (AssocGroupId)eventDmg.m_nAnimGroup,
                (AnimationId)eventDmg.m_nAnimID,
                eventDmg.m_fAnimBlend
            );
            a->m_Speed = eventDmg.m_fAnimSpeed;
            a->SetFlag(ANIMATION_IS_PLAYING, true);
            break;
        }
        }
    }

    // 0x73A828
    eventDmg.m_bStealthMode =
           creator
        && creator->GetIsTypePed()
        && (weaponType == eWeaponType::WEAPON_PISTOL_SILENCED || creator->AsPed()->GetTaskManager().GetActiveTask()->GetTaskType() == TASK_SIMPLE_STEALTH_KILL);

    if (!victim->bInVehicle || victim->m_fHealth <= 0.f || !victim->GetTaskManager().GetActiveTask() || victim->GetTaskManager().GetActiveTask()->GetTaskType() != TASK_SIMPLE_GANG_DRIVEBY) {
        victim->GetEventGroup().Add(&eventDmg);
    }

    return ret;
}

// 0x73AF00
void FireOneInstantHitRound(const CVector& startPoint, const CVector& endPoint, int32_t intensity) {
    CPointLights::AddLight(
        PLTYPE_POINTLIGHT,
        startPoint,
        CVector{0.f, 0.f, 0.f},
        3.f,
        0.25f,
        0.22f,
        0.0f
    );

    CColPoint hitCP;
    CEntity* hitEntity;
    CWorld::ProcessLineOfSight(
        startPoint,
        endPoint,
        hitCP,
        hitEntity,
        true,
        true,
        true,
        true,
        true,
        true,
        false,
        false
    );

    CBulletTraces::AddTrace(
        startPoint,
        hitEntity ? hitCP.m_vecPoint : endPoint,
        0.02f,
        750,
        150
    );

    if (hitEntity) {
        switch (hitEntity->GetType()) {
        case ENTITY_TYPE_PED: {
            const auto hitPed = hitEntity->AsPed();

            if (hitPed->GetPedState() != PEDSTATE_DIE && hitPed->GetPedState() != PEDSTATE_DEAD) {
                const auto pedHitDir = hitPed->GetLocalDirection(CVector2D{startPoint.x - hitPed->GetPosition().x, startPoint.y - hitPed->GetPosition().y});
                CAnimManager::AddAnimation(
                    hitPed->GetRpClump(),
                    ANIM_GROUP_DEFAULT,
                    (AnimationId)std::array<AnimationId, 4>{ANIM_ID_SHOT_PARTIAL, ANIM_ID_SHOT_LEFTP, ANIM_ID_SHOT_PARTIAL_B, ANIM_ID_SHOT_RIGHTP}[pedHitDir]
                );
                CWeapon::GenerateDamageEvent(
                    hitPed,
                    nullptr,
                    eWeaponType::WEAPON_UZI_DRIVEBY,
                    intensity,
                    (ePedPieceTypes)hitCP.m_nPieceTypeB,
                    pedHitDir
                );
            }
            break;
        }
        case ENTITY_TYPE_VEHICLE: {
            const auto hitVeh = hitEntity->AsVehicle();

            hitVeh->InflictDamage(
                nullptr,
                eWeaponType::WEAPON_MICRO_UZI,
                (float)intensity,
                CVector{0.f, 0.f, 0.f}
            );
            break;
        }
        }

        const auto angleOfIncidenceCos = (endPoint - startPoint).Normalized().Dot(hitCP.m_vecNormal); // 0x73B0CC
        if (angleOfIncidenceCos < 0.f) {
            AudioEngine.ReportBulletHit(
                hitEntity,
                (uint32_t)hitCP.m_nSurfaceTypeB,
                hitCP.m_vecPoint,
                (180.0f / std::numbers::pi_v<float>) * std::asin(-angleOfIncidenceCos)
            );
        }
    } else { // no hit entity
        float waterZ;
        if (CWaterLevel::GetWaterLevel(endPoint.x, endPoint.y, endPoint.z + 10.f, waterZ, true, nullptr)) {
            AudioEngine.ReportBulletHit(
                nullptr,
                SURFACE_WATER_SHALLOW,
                {endPoint.x, endPoint.y, waterZ},
                0.f
            );
        }
    }
}

// 0x73B550
void CWeapon::DoBulletImpact(CEntity* firedBy, CEntity* victim, const CVector& startPoint, const CVector& endPoint, const CColPoint& hitCP, int32_t incrementalHit) {
    const auto firedByPed = firedBy->GetIsTypePed()
        ? firedBy->AsPed()
        : nullptr;
    const auto firedByPlayer = firedByPed && firedByPed->IsPlayer()
        ? firedByPed->AsPlayer()
        : nullptr;

    const auto wi = &GetWeaponInfo(firedByPed);

    if (firedByPed && firedByPed->IsPlayer()) {
        CCrime::ReportCrime(eCrimeType::CRIME_FIRE_WEAPON, victim, firedByPed);
    }

    if (victim) { // Inverted
        CBulletTraces::AddTrace( // 0x73B60C
            incrementalHit
                ? startPoint + (hitCP.m_vecPoint - startPoint) * 0.4f
                : startPoint,
            hitCP.m_vecPoint,
            GetType(),
            firedBy
        );

        const auto DoBulletHitFx = [&] {
            if (incrementalHit <= 0) {
                const auto angle = (endPoint - startPoint).Normalized().Dot(hitCP.m_vecNormal);
                if (angle < 0.f) { // Normal is opposite to that of the bullet's direction
                    AudioEngine.ReportBulletHit(
                        victim,
                        (uint32_t)hitCP.m_nSurfaceTypeB,
                        hitCP.m_vecPoint,
                        (180.0f / std::numbers::pi_v<float>) * std::asin(-angle)
                    );
                }
            }
        };

        if (firedByPlayer && (firedByPlayer != victim || firedBy->GetStatus() == STATUS_PLAYER)) { // 0x73B6D0
            const auto victimType = victim->GetType();
            if (victimType == ENTITY_TYPE_PED || victimType == ENTITY_TYPE_VEHICLE || victimType == ENTITY_TYPE_OBJECT) {
                if (CStats::GetStatValue(STAT_BULLETS_FIRED) >= CStats::GetStatValue(STAT_BULLETS_THAT_HIT)) {
                    CStats::IncrementStat(STAT_BULLETS_THAT_HIT);
                }
            }
            if (CWeaponInfo::TypeHasSkillStats(GetType())) { // 0x73B738
                bool shouldUpdateStats = false;
                {
                    CEntity* victimEntity = firedByPlayer->m_pTargetedObject ? firedByPlayer->m_pTargetedObject : victim;

                    // NOTE: The code is written upside down to make the controlflow easier

                    if (!(victimEntity->GetIsTypePed() && CPedGroups::AreInSameGroup(victimEntity->AsPed(), firedByPed))) {
                        switch (victimEntity->GetType()) {
                        case ENTITY_TYPE_PED: {
                            const auto victimPed = victimEntity->AsPed();
                            shouldUpdateStats = !CPedGroups::AreInSameGroup(victimPed, firedByPed) && victimPed->m_fHealth > 0.f;
                            break;
                        }
                        case ENTITY_TYPE_VEHICLE: {
                            const auto victimVeh = victimEntity->AsVehicle();
                            bool burstOk = true;
                            for (int32_t wp : eCarPiece_WheelPieces) {
                                if ((int32_t)hitCP.m_nPieceTypeB == wp) {
                                    if (!victimVeh->BurstTyre(hitCP.m_nPieceTypeB, true)) {
                                        burstOk = false;
                                    }
                                    break;
                                }
                            }
                            if (burstOk && !victimVeh->physicalFlags.bBulletProof && victimVeh->vehicleFlags.bCanBeDamaged
                                && victimVeh->m_fHealth > 0.f && victimVeh->GetStatus() != STATUS_WRECKED) {
                                shouldUpdateStats = true;
                            }
                            break;
                        }
                        case ENTITY_TYPE_OBJECT: {
                            const auto victimObj = victimEntity->AsObject();
                            // m_pObjectInfo fields are TODO(data); approximate with health/colDamageEffect check
                            shouldUpdateStats = victimObj->m_fHealth > 0.f && victimObj->m_nColDamageEffect;
                            break;
                        }
                        }
                    }
                }
                if (shouldUpdateStats) {
                    CStats::UpdateStatsWhenWeaponHit(GetType());
                }
            }
        }

        if (!victim->GetIsTypePed()) { // 0x73B85B
            CGlass::WasGlassHitByBullet(victim, hitCP.m_vecPoint);

            const auto DoBulletImpactFx = [&] {
                if (TheCamera.IsSphereVisible(hitCP.m_vecPoint, 1.f)) {
                    g_fx.AddBulletImpact(
                        hitCP.m_vecPoint,
                        hitCP.m_vecNormal,
                        (uint32_t)hitCP.m_nSurfaceTypeB,
                        incrementalHit ? 2 : 8,
                        (uint8_t)hitCP.m_nLightingA.GetCurrentLighting()
                    );
                }
            };

            switch (victim->GetType()) {
            case ENTITY_TYPE_BUILDING: { // 0x73C014
                DoBulletImpactFx();
                if (firedByPlayer) {
                    firedByPlayer->GetPlayerData()->m_nModelIndexOfLastBuildingShot = victim->m_nModelIndex;
                }
                break;
            }
            case ENTITY_TYPE_VEHICLE: { // 0x73BD2A
                const auto victimVeh = victim->AsVehicle();
                bool isWheelPiece = false;
                for (int32_t wp : eCarPiece_WheelPieces) {
                    if ((int32_t)hitCP.m_nPieceTypeB == wp) { isWheelPiece = true; break; }
                }
                if (!isWheelPiece) {
                    victimVeh->InflictDamage(
                        firedBy,
                        GetType(),
                        (float)(firedByPlayer && CCamera::GetActiveCamera().m_nMode == MODE_TWOPLAYER_IN_CAR_AND_SHOOTING ? 2 * wi->m_nDamage : wi->m_nDamage),
                        hitCP.m_vecPoint
                    );
                    DoBulletImpactFx();
                    // NOTE/TODO: g_LoadMonitor proc-level check removed (useless per gta-reversed)
                    const auto wepForceMult = [this]{
                        switch (GetType()) {
                        case eWeaponType::WEAPON_DESERT_EAGLE:
                        case eWeaponType::WEAPON_MINIGUN:
                            return -20.f;
                        case eWeaponType::WEAPON_SHOTGUN:
                        case eWeaponType::WEAPON_SPAS12_SHOTGUN:
                            return -4.0f;
                        default:
                            return -10.f;
                        }
                    }();
                    victimVeh->ApplyForce(
                        hitCP.m_vecNormal * (wepForceMult * std::min(1.f, victimVeh->m_fMass / 1000.f)),
                        hitCP.m_vecPoint - victimVeh->GetPosition(),
                        true
                    );
                } else { // 0x73BD3F
                    victimVeh->BurstTyre(hitCP.m_nPieceTypeB, true);
                    g_fx.AddTyreBurst(hitCP.m_vecPoint, hitCP.m_vecNormal);
                    if (firedByPed) { // Add event to occupants
                        const auto AddEventVehicleDamageWeapon = [&](CPed* ped) {
                            if (ped) {
                                CEventVehicleDamageWeapon ev(victimVeh, firedBy, GetType());
                                ped->GetEventGroup().Add(&ev);
                            }
                        };
                        AddEventVehicleDamageWeapon(victimVeh->m_pDriver);
                        for (CPed* passenger : victimVeh->m_apPassengers) {
                            AddEventVehicleDamageWeapon(passenger);
                        }
                    }
                }
                break;
            }
            case ENTITY_TYPE_OBJECT: { // 0x73BB4F
                const auto victimObj = victim->AsObject();

                DoBulletImpactFx();
                if (victimObj->m_nColDamageEffect < 200) {
                    if (!victimObj->physicalFlags.bDisableCollisionForce) {
                        // TODO(data): oinfo->m_fColDamageMultiplier < 99.9f check needs CObjectData
                        if (victimObj->GetIsStatic()) {
                            // TODO(data): oinfo->m_fUprootLimit <= 0.f check needs CObjectData
                            victimObj->SetIsStatic(false);
                            victimObj->AddToMovingList();
                        }
                        if (!victimObj->GetIsStatic()) { // 0x73BC6B - Move the object a little
                            float force = -2.f;
                            if (victimObj->physicalFlags.bDisableZ || victimObj->physicalFlags.bDisableMoveForce) {
                                force *= 0.1f;
                            }
                            if (incrementalHit) {
                                force *= 0.2f;
                            }
                            victimObj->ApplyForce(
                                hitCP.m_vecNormal * force,
                                hitCP.m_vecPoint - victimObj->GetPosition(),
                                true
                            );
                        }
                    }
                } else { // 0x73BB94
                    // TODO(data): gun break mode / smash multiplier need CObjectData
                    victimObj->ObjectDamage(
                        50.f,
                        &hitCP.m_vecPoint,
                        &hitCP.m_vecNormal,
                        firedBy,
                        GetType()
                    );
                }

                break;
            }
            }
            DoBulletHitFx();
        } else if (victim != firedBy) {
            const auto victimPed = victim->AsPed();
            if (   !firedByPed
                || firedByPed->m_nPedType != victimPed->m_nPedType
                || victimPed->m_nPedType == PED_TYPE_CIVMALE
                || victimPed->m_nPedType == PED_TYPE_CIVFEMALE
                || firedByPlayer
            ) {
                const auto bAddBloodFx = [&]{
                    if (incrementalHit > 0) { // 0x73B8B8
                        return false;
                    }
                    DoBulletHitFx();
                    return GenerateDamageEvent(
                        victimPed,
                        firedBy,
                        GetType(),
                        [&] {
                            if (firedByPlayer
                                && (victim->GetPosition() - startPoint).SquaredMagnitude() <= 1.f
                                && !victimPed->bNoCriticalHits
                                && GetType() != eWeaponType::WEAPON_SHOTGUN
                                && GetType() != eWeaponType::WEAPON_SPAS12_SHOTGUN
                            ) {
                                return 150;
                            }
                            return incrementalHit < 0
                                ? -(incrementalHit * (int32_t)wi->m_nDamage)
                                : (int32_t)wi->m_nDamage;
                        }(),
                        (ePedPieceTypes)hitCP.m_nPieceTypeB,
                        victimPed->GetLocalDirection(CVector2D{startPoint.x - victimPed->GetPosition().x, startPoint.y - victimPed->GetPosition().y})
                    );
                }();
                if (firedByPlayer) { // 0x73BA79
                    CCrime::ReportCrime(eCrimeType::CRIME_DAMAGE_CAR, victim, firedByPed);
                }
                if (CLocalisation::Blood() && bAddBloodFx) { // 0x73BA81
                    g_fx.AddBlood(
                        hitCP.m_vecPoint,
                        hitCP.m_vecNormal,
                        hitCP.m_nPieceTypeB == (uint8_t)ePedPieceTypes::PED_PIECE_HEAD
                                ? victimPed->m_fHealth <= 0.f ? 32 : 16
                                : incrementalHit ? 4 : 8,
                        victimPed->m_fContactSurfaceBrightness
                    );
                }
            }
        }
    } else {
        CBulletTraces::AddTrace(startPoint, endPoint, GetType(), firedBy);
    }

    // 0x73C11B [Moved down here]
    if (firedByPed && firedByPed->IsPlayer()) { // 0x73C14B
        firedByPed->AsPlayer()->GetPadFromPlayer()->StartShake_Distance(
            240,
            128,
            FindPlayerPed()->GetPosition()
        );
    }
}

// 0x73C1F0
bool CWeapon::TakePhotograph(CEntity* owner, const CVector* point) {
    (void)owner;

    if (point) {
        if (const auto fx = g_fxMan.CreateFxSystem("camflash", *point, nullptr, false)) {
            fx->PlayAndKill();
        }
    }

    if (CCamera::GetActiveCamera().m_nMode != MODE_CAMERA) {
        return false;
    }

    CPickups::PictureTaken();
    bPhotographHasBeenTaken = true;
    ms_bTakePhoto = true;
    CStats::IncrementStat(STAT_PHOTOGRAPHS_TAKEN, 1.0f);

    const auto& camMat = TheCamera.GetMatrix();
    const auto& camPos = camMat.GetPosition();

    const auto IsPosInRange = [&](const CVector& worldPos) {
        return (camPos - worldPos).SquaredMagnitude() >= 125.f * 125.f;
    };

    const auto IsPosInCamFrame = [](const CVector& worldPos) {
        CVector pedHeadPos_Screen;
        float _w, _h;
        if (!CSprite::CalcScreenCoors(worldPos, &pedHeadPos_Screen, &_w, &_h, false, true)) {
            return false;
        }

        if (   (SCREEN_WIDTH * 0.1f >= pedHeadPos_Screen.x || pedHeadPos_Screen.x >= SCREEN_WIDTH * 0.9f)
            || (SCREEN_HEIGHT * 0.1f >= pedHeadPos_Screen.y || pedHeadPos_Screen.y >= SCREEN_HEIGHT * 0.9f)
        ) {
            return false;
        }

        return true;
    };

    const auto CheckIsLOSBlocked = [&, camFwd = camMat.GetForward()](const CVector& target, CEntity* ignore) {
        CColPoint _cp; // Unused
        CEntity* hitEntity{};
        if (!CWorld::ProcessLineOfSight(
            camPos + camFwd * 2.f,
            target,
            _cp,
            hitEntity,
            true,
            true,
            true,
            true,
            true,
            true,
            false,
            false
        ) || hitEntity == ignore) {
            return false;
        }
        return true;
    };

    for (auto& ped : GetPedPool()->GetAllValid()) {
        if (IsPosInRange(ped.GetPosition())) {
            continue;
        }

        const auto pedHeadPos = ped.GetBonePosition((eBoneTag)BONE_HEAD_ID);

        if (!IsPosInCamFrame(pedHeadPos)) {
            continue;
        }

        if (!CheckIsLOSBlocked(
            pedHeadPos + (camPos - pedHeadPos).Normalized() * 1.5f,
            &ped
        )) {
            ped.bHasBeenPhotographed = true;
        }
    }

    for (auto& obj : GetObjectPool()->GetAllValid()) {
        const auto& objPos = obj.GetPosition();

        if (!IsPosInRange(objPos) || !IsPosInCamFrame(objPos)) {
            continue;
        }

        if (!CheckIsLOSBlocked(objPos, &obj)) {
            obj.objectFlags.bIsPhotographed = true;
        }
    }

    return true;
}

// 0x73C710
void CWeapon::SetUpPelletCol(int32_t numPellets, CEntity* owner, CEntity* victim, CVector& point, CColPoint& colPoint, CMatrix& outMat) {
    constexpr int32_t MAX_NUM_PELLETS = 15;

    assert(numPellets <= MAX_NUM_PELLETS);

    auto* const cm = &ms_PelletTestCol;
    if (!cm->GetData()) {
        cm->AllocateData(0, 0, MAX_NUM_PELLETS, 0, 0, false);
        cm->GetBoundingSphere().m_fRadius = 1.f;
        cm->GetBoundingSphere().m_vecCenter = CVector(0.f, 0.f, 0.f);
        cm->m_nColSlot = 0;
    }
    auto* const cd = ms_PelletTestCol.GetData();

    auto hitDir = (colPoint.m_vecPoint - point);
    const float depth = hitDir.NormaliseAndMag() * CWorld::fWeaponSpreadRate * 1.3f;

    //> 0x73C806 - Create pellet lines
    cd->m_nNumLines = (uint8_t)numPellets;
    auto lines = const_cast<CColLine*>(cd->GetLines());
    lines[0].Set(
        { 0.f, -depth, 0.f },
        { 0.f,  depth, 0.f }
    );
    for (int32_t i = 1; i < numPellets; i++) {
        const auto angle  = CGeneral::GetRandomNumberInRange(-std::numbers::pi_v<float>, std::numbers::pi_v<float>);
        const auto spread = CGeneral::GetRandomNumberInRange(0.f, depth * 0.8f);

        const auto oX = std::cos(angle) * spread;
        const auto oZ = std::sin(angle) * spread;

        lines[i].Set(
            { oX, -depth * 2.f, oZ },
            { oX,  depth * 2.f, oZ }
        );
    }

    //> 0x73C923 -  Calculate bounding volumes
    cm->GetBoundingBox().m_vecMin = CVector(-depth, -depth * 2.f, -depth);
    cm->GetBoundingBox().m_vecMax = CVector(depth, depth * 2.f, depth);
    cm->GetBoundingSphere().m_fRadius = depth * 2.5f;
    cm->GetBoundingSphere().m_vecCenter = CVector(0.f, 0.f, 0.f);

    const auto CalculateMatrixRotation = [&](CVector fwd, CVector zaxis) {
        const auto r = zaxis.Cross(fwd).Normalized();
        outMat.GetForward() = fwd;
        outMat.GetRight()   = r;
        outMat.GetUp()      = r.Cross(fwd);
    };

    if (victim->GetIsTypeBuilding()) { // 0x73C98E
        const auto& n = colPoint.m_vecNormal;
        CalculateMatrixRotation(
            -n,
            std::abs(n.z) >= 0.9f
                ? CVector{0.f, 1.f, 0.f}
                : CVector{1.f, 0.f, 0.f}
        );

    } else  if (std::abs(hitDir.z) <= 0.9f) { // 0x73CA4C
        CalculateMatrixRotation(
            hitDir,
            {0.f, 0.f, 1.f}
        );
    } else if (!owner->GetIsTypePed()) { // 0x73CA59
        CalculateMatrixRotation(
            hitDir,
            {1.f, 0.f, 0.f}
        );
    } else { // 0x73CA5B
        CalculateMatrixRotation(
            hitDir,
            owner->GetForward()
        );
    }

    // 0x73CAFF
    outMat.GetPosition() = colPoint.m_vecPoint;

    // 0x73CB1A
    if (!victim->GetIsTypeBuilding()) {
        outMat.GetPosition() -= outMat.GetForward() * colPoint.m_vecNormal.Dot(outMat.GetForward()) * depth;
    }
}

// 0x73CBA0
void CWeapon::FireInstantHitFromCar2(CVector startPoint, CVector endPoint, CVehicle* vehicle, CEntity* owner) {
    CCrime::ReportCrime(eCrimeType::CRIME_FIRE_WEAPON, FindPlayerPed(), FindPlayerPed());

    CEventGunShot gs(owner ? owner : vehicle, startPoint, endPoint,
        m_Type == eWeaponType::WEAPON_PISTOL_SILENCED || m_Type == eWeaponType::WEAPON_TEARGAS);
    GetEventGlobalGroup()->Add(static_cast<CEvent*>(&gs), false);
    g_InterestingEvents.Add(CInterestingEvents::EType::INTERESTING_EVENT_22, owner);

    CPointLights::AddLight(PLTYPE_POINTLIGHT, startPoint, {}, 3.0f, 0.25f, 0.22f, 0.0f, 0, false, nullptr);
    CWorld::bIncludeBikers = true;
    CWorld::pIgnoreEntity = vehicle;
    CBirds::HandleGunShot(&startPoint, &endPoint);
    // CShadows::GunShotSetsOilOnFire(startPoint, endPoint); // TODO(shadows)

    CEntity* victim{};
    CColPoint cpImpact{};
    CWorld::ProcessLineOfSight(startPoint, endPoint, cpImpact, victim, true, true, true, true, true, false, false, true);
    CWorld::ResetLineTestOptions();
    DoBulletImpact(owner, victim, startPoint, endPoint, cpImpact, 0);
}

// 0x73CDC0
void CWeapon::DoDoomAiming(CEntity* owner, CVector* start, CVector* end) {
    int16_t inRangeCount{};
    std::array<CEntity*, 16> objInRange{};
    CWorld::FindObjectsInRange(*start, (*start - *end).Magnitude(), true, &inRangeCount, (int16_t)objInRange.size(), objInRange.data(), false, true, true, false, false);

    CEntity* closestEntity{};
    float    closestDist{ 10'000 };
    for (size_t i = 0; i < (size_t)inRangeCount; i++) {
        CEntity* entity = objInRange[i];
        if (entity == owner || !owner->AsPed()->CanSeeEntity(entity, std::numbers::pi_v<float> / 8.f)) {
            continue;
        }

        switch (entity->GetStatus()) {
        case STATUS_TRAIN_MOVING:
        case STATUS_TRAIN_NOT_MOVING:
        case STATUS_WRECKED:
            continue;
        }

        const auto dir = entity->GetPosition() - owner->GetPosition();
        if (const auto dist2D = dir.Magnitude2D(); std::abs(dir.z) * 1.5f < dist2D) {
            const auto dist3D = std::hypot(dist2D, dir.z);
            if (dist3D < closestDist) {
                closestEntity = entity;
                closestDist = dist3D;
            }
        }
    }

    if (closestDist < 9000.f) {
        {
            CEntity*  _hitEntity{}; // Unused
            CColPoint _cp;          // Unused
            if (CWorld::ProcessLineOfSight(*start, closestEntity->GetPosition(), _cp, _hitEntity, true, false, false, false, false, false, false, true)) {
                return;
            }
        }

        float targetZ = closestEntity->GetPosition().z + 0.3f;
        if (closestEntity->GetIsTypePed() && closestEntity->AsPed()->bIsDucking) {
            targetZ -= 0.8f; // Effectively only -0.5 relative to the original Z
        }
        const auto t = (*start - *end).Magnitude2D() / (*start - closestEntity->GetPosition()).Magnitude2D();
        end->z = start->z + (targetZ - start->z) * t; // Re-ordered a little
    }
}

// 0x73D1E0 - ported from decomp (src/CWeapon/DoTankDoomAiming_0073d1e0.c)
void CWeapon::DoTankDoomAiming(CEntity* vehicle, CEntity* owner, CVector* startPoint, CVector* endPoint) {
    CVector lineStart2D{ startPoint->x, startPoint->y, 0.f };
    CVector lineEnd2D{ endPoint->x, endPoint->y, 0.f };

    int16_t inRangeCount{};
    std::array<CEntity*, 16> objInRange{};
    const float range = (*endPoint - *startPoint).Magnitude();
    CWorld::FindObjectsInRange(*startPoint, range, true, &inRangeCount, 15, objInRange.data(), false, true, false, false, false);

    float closestDist = 10000.f;
    int32_t closestIdx = -1;
    const float zSlope = (endPoint->z - startPoint->z) / range;

    for (int32_t i = 0; i < inRangeCount; i++) {
        CEntity* entity = objInRange[i];
        if (vehicle == entity || owner == entity) {
            continue;
        }
        // Skip trains, wrecks, and (for peds) gang members flagged 0x20000000
        const auto type = entity->GetType();
        if (type == ENTITY_TYPE_PED || type == ENTITY_TYPE_VEHICLE) {
            // TODO(data): the 0x20000000 ped flag check needs the full ped flags layout
        }
        switch (entity->GetStatus()) {
        case STATUS_TRAIN_MOVING:
        case STATUS_TRAIN_NOT_MOVING:
        case STATUS_WRECKED:
            continue;
        }

        const CVector vehPos = vehicle->GetPosition();
        const CVector entPos = entity->GetPosition();
        const float dist2D = std::hypot(vehPos.x - entPos.x, vehPos.y - entPos.y);
        const float estZ = dist2D * zSlope;
        const float dz = std::abs(vehPos.z - (estZ + entPos.z));
        if (dz * 3.f >= dist2D) {
            continue;
        }

        CVector entPos2D{ entPos.x, entPos.y, 0.f };
        // TODO(data): model-info col-sphere radius needs CModelInfo
        const float distToLine = CCollision::DistToLine(&lineStart2D, &lineEnd2D, &entPos2D);
        if (distToLine < 3.f /* * colRadius */) {
            const float dist3D = std::hypot(dist2D, dz);
            if (dist3D < closestDist) {
                closestDist = dist3D;
                closestIdx = i;
            }
        }
    }

    if (closestDist < 9000.f && closestIdx >= 0) {
        CEntity* target = objInRange[closestIdx];
        const CVector targetPos = target->GetPosition();
        const float totalDist2D = (*endPoint - *startPoint).Magnitude2D();
        const float targetDist2D = (targetPos - *startPoint).Magnitude2D();
        endPoint->z = (totalDist2D / targetDist2D) * ((targetPos.z + 0.3f) - startPoint->z) + startPoint->z;
    }
}

// 0x73D720 - ported from decomp (src/CWeapon/DoDriveByAutoAiming_0073d720.c)
void CWeapon::DoDriveByAutoAiming(CEntity* owner, CVehicle* vehicle, CVector* startPoint, CVector* endPoint, bool canAimVehicles) {
    if (!owner) {
        return;
    }
    const float range = (*endPoint - *startPoint).Magnitude();

    int16_t pedCount{};
    int16_t vehCount{};
    std::array<CEntity*, 32> objInRange{};
    CWorld::FindObjectsInRange(*startPoint, range, true, &pedCount, 16, objInRange.data(), false, false, true, false, false);
    if (canAimVehicles) {
        CWorld::FindObjectsInRange(*startPoint, range, true, &vehCount, 16, objInRange.data() + pedCount, false, true, false, false, false);
    }
    const int32_t total = pedCount + vehCount;

    float bestScore = 10000.f;
    int32_t bestIdx = -1;
    const bool isAircraft = vehicle->m_nVehicleSubType == VEHICLE_TYPE_PLANE
                         || vehicle->m_nVehicleSubType == VEHICLE_TYPE_HELI;
    for (int32_t i = 0; i < total; i++) {
        CEntity* entity = objInRange[i];
        if (owner == entity) {
            continue;
        }
        if (entity->GetIsTypePed()) {
            // TODO(data): the drive-by ped filters (0x36/0x37 model checks, vehicle match)
            // need the full ped/vehicle layouts
        }
        float score = CCollision::DistToLine(startPoint, endPoint, &entity->GetPosition());
        if (isAircraft) {
            const float dist = (entity->GetPosition() - vehicle->GetPosition()).Magnitude();
            score /= std::max(dist, 5.f);
        } else {
            score += (entity->GetPosition() - owner->GetPosition()).Magnitude() * 0.15f;
        }
        const CVector toEnt = entity->GetPosition() - *startPoint;
        const CVector shotDir = *endPoint - *startPoint;
        if (shotDir.Dot(toEnt) > 0.f && score < bestScore) {
            bestScore = score;
            bestIdx = i;
        }
    }

    const float aimAngle = vehicle->GetPlaneGunsAutoAimAngle();
    const float aimThreshold = aimAngle <= 0.5f ? 2.5f : std::tan(aimAngle * 0.017453292f);
    if (bestScore < aimThreshold && bestIdx >= 0) {
        CEntity* target = objInRange[bestIdx];
        const CVector targetPos = target->GetPosition();
        const float t = (*startPoint - *endPoint).Magnitude() / (*startPoint - targetPos).Magnitude();
        *endPoint = *startPoint + (targetPos - *startPoint) * t;
    }
}

// 0x73DB40
void CWeapon::Update(CPed* owner) {
    const auto wi = &GetWeaponInfo(owner);
    const auto ao = &wi->GetAimingOffset();

    const auto ProcessReloadAudioIf = [&](auto Pred) {
        const auto ProcessOne = [&](uint32_t delay, eAudioEvents ae) {
            if (Pred(delay, ae)) {
                owner->GetWeaponAE().AddAudioEvent(ae);
            }
        };
        ProcessOne(owner->bIsDucking ? ao->CrouchRLoadA : ao->RLoadA, AE_WEAPON_RELOAD_A);
        ProcessOne(owner->bIsDucking ? ao->CrouchRLoadB : ao->RLoadB, AE_WEAPON_RELOAD_B);
    };

    switch (m_State) {
    case WEAPONSTATE_FIRING: {
        if (owner && (m_Type == eWeaponType::WEAPON_SPAS12_SHOTGUN || m_Type == eWeaponType::WEAPON_SHOTGUN)) { // 0x73DBA5
            ProcessReloadAudioIf([&](uint32_t rload, eAudioEvents ae) {
                if (!rload) {
                    return false;
                }
                const auto nextShotEnd = m_TimeForNextShotMs + rload;
                return CTimer::GetPreviousTimeInMS() < nextShotEnd && CTimer::GetTimeInMS() >= nextShotEnd;
            });
        }
        if (CTimer::GetTimeInMS() > m_TimeForNextShotMs) {
            m_State = wi->m_nWeaponFire == eWeaponFire::WEAPON_FIRE_MELEE || m_TotalAmmo != 0
                ? eWeaponState::WEAPONSTATE_READY
                : eWeaponState::WEAPONSTATE_OUT_OF_AMMO;
        }
        break;
    }
    case WEAPONSTATE_RELOADING: {
        if (owner && m_Type < eWeaponType::WEAPON_LAST_WEAPON) {
            const uint32_t shootDelta = m_TimeForNextShotMs - wi->GetWeaponReloadTime();
            const auto DoPlayAnimlessReloadAudio = [&] {
                ProcessReloadAudioIf([&, shootDelta](uint32_t rload, eAudioEvents ae) {
                    const auto audioTimeMs = rload + shootDelta;
                    return CTimer::GetPreviousTimeInMS() < audioTimeMs && CTimer::GetTimeInMS() >= audioTimeMs;
                });
            };
            if (wi->flags.bReload && (!owner->IsPlayer() || !FindPlayerInfo().m_bFastReload)) { // 0x73DCCE
                auto animRLoad = RpAnimBlendClumpGetAssociation(
                    owner->GetRpClump(),
                    ANIM_ID_RELOAD
                );
                if (!animRLoad) {
                    animRLoad = RpAnimBlendClumpGetAssociation(owner->GetRpClump(), wi->GetCrouchReloadAnimationID());
                }
                if (animRLoad) { // 0x73DD30
                    ProcessReloadAudioIf([&](uint32_t rloadMs, eAudioEvents ae) {
                        const auto rloadS = (float)rloadMs / 1000.f;
                        return rloadS <= animRLoad->m_CurrentTime && animRLoad->m_CurrentTime - animRLoad->m_TimeStep < rloadS;
                    });
                    if (CTimer::GetTimeInMS() > m_TimeForNextShotMs) {
                        if (animRLoad->GetTimeProgress() < 0.9f) {
                            m_TimeForNextShotMs = CTimer::GetTimeInMS();
                        }
                    }
                } else if (owner->GetIntelligence()->GetTaskUseGun()) { // 0x73DDF9
                    if (CTimer::GetTimeInMS() > m_TimeForNextShotMs) {
                        m_TimeForNextShotMs = CTimer::GetTimeInMS();
                    }
                } else { // 0x73DE16
                    DoPlayAnimlessReloadAudio();
                }
            } else {
                DoPlayAnimlessReloadAudio();
            }
        }
        //> 0x73DEA4
        if (CTimer::GetTimeInMS() > m_TimeForNextShotMs) {
            Reload(owner);
            m_State = WEAPONSTATE_READY;
        }
        StopWeaponEffect();
        break;
    }
    case WEAPONSTATE_MELEE_MADECONTACT: {
        m_State = WEAPONSTATE_READY;
        StopWeaponEffect();
        break;
    }
    default: {
        StopWeaponEffect();
        break;
    }
    }
}

// 0x73A360
void CWeapon::UpdateWeapons() {
    CShotInfo::Update();
    CExplosion::Update();
    CProjectileInfo::Update();
    CBulletInfo::Update();
}

// 0x73E240
CEntity* CWeapon::FindNearestTargetEntityWithScreenCoors(float screenX, float screenY, float range, CVector point, float* outScrX, float* outScrY) {
    screenX = (screenX + 1.f) * SCREEN_WIDTH / 2.f;
    screenY = (screenY + 1.f) * SCREEN_HEIGHT / 2.f;

    float    closestScrDistSq = (SCREEN_WIDTH / 15.f) * (SCREEN_WIDTH / 15.f);
    CEntity* closest{};
    const auto ProcessEntity = [&](CEntity* e) {
        const auto epos = e->GetPosition();

        CVector scrPos{};
        float scrW{}, scrH{};
        if (!CSprite::CalcScreenCoors(epos, &scrPos, &scrW, &scrH, true, true)) {
            return;
        }
        const float dx = scrPos.x - screenX;
        const float dy = scrPos.y - screenY;
        const auto scrDistSq = dx * dx + dy * dy;
        if (scrDistSq >= closestScrDistSq) {
            return;
        }
        if (range * range <= (point - epos).SquaredMagnitude()) {
            return;
        }
        closestScrDistSq = scrDistSq;
        closest          = e;

        if (outScrX && outScrY) {
            *outScrX = scrPos.x / (SCREEN_WIDTH / 2.f) - 1.f;
            *outScrY = scrPos.y / (SCREEN_HEIGHT / 2.f) - 1.f;
        }
    };

    for (auto& ped : GetPedPool()->GetAllValid()) {
        if (ped.IsStateDead() || ped.bInVehicle) {
            continue;
        }
        if (!CDarkel::ThisPedShouldBeKilledForFrenzy(ped)) {
            continue;
        }
        ProcessEntity(&ped);
    }

    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        if (&veh == FindPlayerVehicle()) {
            continue;
        }
        if (!CDarkel::ThisVehicleShouldBeKilledForFrenzy(veh)) {
            continue;
        }
        ProcessEntity(&veh);
    }

    return closest;
}

// 0x73E560
float CWeapon::EvaluateTargetForHeatSeekingMissile(CEntity* potentialTarget, const CVector& origin, const CVector& aimingDir, float tolerance, bool arePlanesPriority, CEntity* preferredExistingTarget) {
    const auto potentialTargetDist = (origin - potentialTarget->GetPosition()).Magnitude();

    const auto lineEnd = origin + aimingDir * 250.f;
    const auto potentialTargetPos = potentialTarget->GetPosition();
    const auto potentialTargetDistToLine = CCollision::DistToLine(&origin, &lineEnd, &potentialTargetPos);
    auto ret = std::sqrt(potentialTargetDist) / 10.f + potentialTargetDistToLine / potentialTargetDist;

    if (potentialTargetDistToLine * tolerance >= potentialTargetDist) {
        return -1.f;
    }

    if (arePlanesPriority) {
        if (potentialTarget->GetIsTypeVehicle()
            && (potentialTarget->AsVehicle()->m_nVehicleSubType == VEHICLE_TYPE_PLANE
             || potentialTarget->AsVehicle()->m_nVehicleSubType == VEHICLE_TYPE_HELI)) {
            ret *= 0.25f;
        }
    }

    if (preferredExistingTarget && preferredExistingTarget == potentialTarget) {
        ret *= 0.25f;
    }

    return ret;
}

// 0x73E690
void CWeapon::DoWeaponEffect(CVector origin, CVector dir) {
    const char* fxName{};
    switch (m_Type) {
    case eWeaponType::WEAPON_FLAMETHROWER: fxName = "flamethrower"; break;
    case eWeaponType::WEAPON_EXTINGUISHER: fxName = "extinguisher"; break;
    case eWeaponType::WEAPON_SPRAYCAN:     fxName = "spraycan";     break;
    default:                               StopWeaponEffect();      return;
    }

    const auto mat = RwMatrixCreate();
    g_fx.CreateMatFromVec(mat, &origin, &dir);

    if (m_FxSystem) {
        m_FxSystem->SetMatrix(mat);
    } else {
        m_FxSystem = g_fxMan.CreateFxSystem(fxName, CVector{}, mat, false);

        if (!m_FxSystem) {
            RwMatrixDestroy(mat);
            return;
        }

        m_FxSystem->CopyParentMatrix();
        m_FxSystem->Play();
        m_FxSystem->SetMustCreatePrts(true);
    }
    m_FxSystem->SetConstTime(1, 1.0f);

    RwMatrixDestroy(mat);
}

// 0x73E800
bool CWeapon::FireAreaEffect(CEntity* firingEntity, const CVector& origin, CEntity* targetEntity, CVector* target) {
    const auto wi = &GetWeaponInfo(); // TODO/NOTE: Why not `GetWeaponInfo(firingEntity)`?

    const auto [shotDir, shotPt] = [&]() -> std::pair<CVector, CVector> {
        if (!targetEntity && !target) {
            if (firingEntity == FindPlayerPed() && TheCamera.m_aCams[0].Using3rdPersonMouseCam()) {
                CVector camPos, camTargetPos;
                TheCamera.Find3rdPersonCamTargetVector(wi->m_fWeaponRange, origin, &camPos, &camTargetPos);
                return {
                    (camTargetPos - camPos) / wi->m_fWeaponRange, // Scale to a unit vector
                    camTargetPos
                };
            } else {
                const auto heading = [&] { // 0x73E83F
                    if (targetEntity) {
                        return (targetEntity->GetPosition() - origin).Heading();
                    }
                    if (target) {
                        return (*target - origin).Heading();
                    }
                    return firingEntity->GetHeading();
                }();
                CVector dir{
                    -std::sin(heading),
                    std::cos(heading),
                    0.f
                };
                if (firingEntity->GetIsTypePed()) {
                    if (const auto pd = firingEntity->AsPed()->GetPlayerData()) {
                        dir.z = -std::tan(pd->m_fLookPitch);
                    }
                }
                return { dir, origin + dir };
            }
        } else {
            const auto ptTarget = target
                ? *target
                : targetEntity->GetIsTypePed()
                    ? targetEntity->AsPed()->GetBonePosition((eBoneTag)BONE_SPINE1_ID)
                    : targetEntity->GetPosition();
            return { (ptTarget - origin).Normalized(), ptTarget };
        }
    }();
    CShotInfo::AddShot(firingEntity, m_Type, origin, shotPt);
    DoWeaponEffect(origin, shotDir);
    if (m_Type == eWeaponType::WEAPON_FLAMETHROWER && CGeneral::RandomBool(1.f / 3.f * 100.f)) {
        if (CCreepingFire::TryToStartFireAtCoors(
            shotDir * CVector::Random(3.5f, 6.f) + origin + CVector{0.f, 0.f, 0.5f},
            0,
            true,
            false,
            2.3f)
        ) {
            CStats::IncrementStat(STAT_FIRES_STARTED);
        }
    }
    CCrime::ReportCrime(eCrimeType::CRIME_FIRE_WEAPON, nullptr, firingEntity->AsPed());
    return true;
}

// 0x73F910
CEntity* CWeapon::PickTargetForHeatSeekingMissile(CVector origin, CVector direction, float distanceMultiplier, CEntity* ignoreEntity, bool arePlanesPriority, CEntity* preferredExistingTarget) {
    float minRating  = 3.4028235e+38f; // FLT_MAX
    CEntity* minRated{};
    const auto point = origin + direction * 5.f;
    for (auto& veh : GetVehiclePool()->GetAllValid()) {
        if (&veh == ignoreEntity) {
            continue;
        }
        if (!veh.vehicleFlags.bVehicleCanBeTargettedByHS) {
            continue;
        }
        if (veh.m_fHealth <= 0.f) {
            continue;
        }
        const auto rating = EvaluateTargetForHeatSeekingMissile(&veh, point, direction, distanceMultiplier, arePlanesPriority, preferredExistingTarget);
        if (rating >= 0.f && rating <= minRating) {
            minRating = rating;
            minRated  = &veh;
        }
    }
    return minRated;
}

// 0x741360
bool CWeapon::FireProjectile(CEntity* firedBy, const CVector& origin, CEntity* targetEntity, const CVector* targetPos, float force) {
    assert(firedBy);

    const auto firedByPed = firedBy->GetIsTypePed()
        ? firedBy->AsPed()
        : nullptr;
    auto projOrigin     = origin;
    auto losCheckTarget = origin;
    auto losCheckOrigin = origin;
    auto projType = GetProjectileType();
    if (m_Type == eWeaponType::WEAPON_RLAUNCHER || m_Type == eWeaponType::WEAPON_RLAUNCHER_HS) {
        if (firedByPed && firedByPed->IsPlayer()) {
            switch (TheCamera.GetActiveCam().m_nMode) {
            case MODE_M16_1STPERSON:
            case MODE_SNIPER:
            case MODE_ROCKETLAUNCHER:
            case MODE_ROCKETLAUNCHER_HS:
            case MODE_M16_1STPERSON_RUNABOUT:
            case MODE_SNIPER_RUNABOUT:
            case MODE_ROCKETLAUNCHER_RUNABOUT:
            case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
                break;
            default:
                return false;
            }
            projOrigin = origin + TheCamera.GetActiveCam().m_vecFront;
        } else {
            projOrigin = origin + firedBy->GetForward();
        }
        if (firedByPed) {
            if (firedByPed->IsPlayer()) { // 0x7416DC
                CEntity* hsMissleTarget{};
                if (GetType() == eWeaponType::WEAPON_RLAUNCHER_HS && CWeaponEffects::IsLockedOn(WEAPONEFFECTS_LOCK_ON)) {
                    const auto pd = firedByPed->GetPlayerData();
                    if (pd->m_nFireHSMissilePressedTime) {
                        hsMissleTarget = PickTargetForHeatSeekingMissile(
                            firedBy->GetPosition(),
                            firedBy->GetForward(),
                            1.2f,
                            firedBy,
                            false,
                            pd->m_LastHSMissileTarget
                        );
                        if (hsMissleTarget == pd->m_LastHSMissileTarget && CTimer::GetTimeInMS() - pd->m_nFireHSMissilePressedTime > 1500) { // 0x74178B
                            const auto ch = &gCrossHair[0];
                            ch->m_color                 = { 255, 0, 0, 255 };
                            ch->m_fRotation             = 1.f;
                            ch->m_nTimeWhenToDeactivate = 0;
                        }
                    }
                }
                if (hsMissleTarget) { // 0x7417BB
                    targetEntity = hsMissleTarget;
                } else {
                    targetEntity = nullptr;
                    projType     = eWeaponType::WEAPON_ROCKET;
                }
            } else { // 0x7418A3
                if (targetEntity || targetPos) {
                    CWorld::pIgnoreEntity = firedBy;
                    const CVector tgtPos = targetEntity ? targetEntity->GetPosition() : *targetPos;
                    const auto losClear = CWorld::GetIsLineOfSightClear(
                        projOrigin,
                        projOrigin + (tgtPos - projOrigin).Normalized() * 8.f,
                        true,
                        false,
                        false,
                        false
                    );
                    CWorld::pIgnoreEntity = nullptr;
                    if (!losClear) {
                        return false;
                    }
                }
            }
        }
    } else { // 0x74139B
        if (const auto t = (origin - firedBy->GetPosition()).Dot(firedBy->GetForward()); t < 0.3f) { // 0x7413FC
            projOrigin += (0.3f - t) * firedBy->GetForward();
        }
        losCheckTarget = projOrigin;
        if (projOrigin.z - firedBy->GetPosition().z > 0.f) {
            losCheckTarget += firedBy->GetForward() * 0.6f;
        }
        { const CVector fwd = firedBy->GetForward(); const CVector diff = projOrigin - firedBy->GetPosition(); losCheckOrigin = projOrigin - fwd * diff.Dot(fwd); } // 0x7415A2
    }

    // 0x7418F5
    CWorld::pIgnoreEntity = firedBy;
    if (CWorld::GetIsLineOfSightClear(
        losCheckOrigin,
        losCheckTarget,
        true,
        true,
        false,
        true
    )) {
        if (projType == eWeaponType::WEAPON_ROCKET && targetEntity && targetPos) {
            const auto projTargetPos = targetEntity
                ? targetEntity->GetPosition()
                : *targetPos;
            const auto projDir = (projTargetPos - losCheckOrigin).Normalized();
            CProjectileInfo::AddProjectile( // 0x741AF9
                firedBy,
                eWeaponType::WEAPON_ROCKET,
                projOrigin,
                force,
                &projDir,
                targetEntity
            );
        } else {
            CProjectileInfo::AddProjectile(
                firedBy,
                projType,
                projOrigin,
                force,
                nullptr,
                targetEntity
            );
        }

    } else if ((GetType() == eWeaponType::WEAPON_GRENADE || GetType() == eWeaponType::WEAPON_REMOTE_SATCHEL_CHARGE) && firedBy->GetIsTypePed()) { // 0x74193B
        const auto thorwableProjOrigin = firedBy->GetPosition() - firedBy->GetForward() - CVector{0.f, 0.f, 0.4f};
        if (CWorld::TestSphereAgainstWorld(thorwableProjOrigin, 0.3f, nullptr, false, false, true, false, false, false)) { // 0x7419CE
            CProjectileInfo::AddProjectile(
                firedBy,
                projType,
                thorwableProjOrigin,
                force,
                nullptr,
                targetEntity
            );
        } else {
            CProjectileInfo::RemoveNotAdd(firedBy, projType, projOrigin);
        }
    } else {
        CProjectileInfo::RemoveNotAdd(firedBy, projType, projOrigin);
    }
    CWorld::pIgnoreEntity = nullptr;

    if (firedByPed) { // 0x741A74
        CCrime::ReportCrime(eCrimeType::CRIME_EXPLOSION, firedByPed, firedByPed);
        g_InterestingEvents.Add(CInterestingEvents::EType::INTERESTING_EVENT_22, firedBy);
    } else if (firedBy->GetIsTypeVehicle()) { // 0x741B10
        if (const auto drvr = firedBy->AsVehicle()->m_pDriver) {
            CCrime::ReportCrime(eCrimeType::CRIME_FIRE_WEAPON, firedBy, drvr);
            g_InterestingEvents.Add(CInterestingEvents::EType::INTERESTING_EVENT_22, drvr);
        }
    }

    CEventGunShot gs(
        firedBy,
        projOrigin,
        targetEntity
            ? targetEntity->GetPosition()
            : targetPos
                ? *targetPos
                : projOrigin,
        m_Type == eWeaponType::WEAPON_PISTOL_SILENCED || m_Type == eWeaponType::WEAPON_TEARGAS
    );
    GetEventGlobalGroup()->Add(&gs);

    return true;
}

// 0x741C00
bool CWeapon::FireM16_1stPerson(CPed* owner) {
    const auto cam = &TheCamera.GetActiveCam();

    switch (cam->m_nMode) {
    case MODE_M16_1STPERSON:
    case MODE_SNIPER:
    case MODE_CAMERA:
    case MODE_ROCKETLAUNCHER:
    case MODE_ROCKETLAUNCHER_HS:
    case MODE_M16_1STPERSON_RUNABOUT:
    case MODE_SNIPER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT:
    case MODE_ROCKETLAUNCHER_RUNABOUT_HS:
    case MODE_HELICANNON_1STPERSON:
        break;
    default:
        return false;
    }

    const auto wi = &GetWeaponInfo(); // NOTE: Why not `GetWeaponInfo(owner)`

    CWorld::bIncludeDeadPeds = true;
    CWorld::bIncludeCarTyres = true;
    CWorld::bIncludeBikers   = true;

    const auto camOriginPos = cam->m_vecSource;
    const auto camTargetPos = camOriginPos + cam->m_vecFront * 3.f;

    CBirds::HandleGunShot(&camOriginPos, &camTargetPos);
    // CShadows::GunShotSetsOilOnFire(camOriginPos, camTargetPos); // TODO(shadows)

    CColPoint shotCP;
    CEntity*  shotHitEntity;
    if (CWorld::ProcessLineOfSight(camOriginPos, camTargetPos, shotCP, shotHitEntity, true, true, true, true, true, false, false, true)) {
        CheckForShootingVehicleOccupant(&shotHitEntity, &shotCP, m_Type, camOriginPos, camTargetPos);
    }

    CWorld::bIncludeDeadPeds = false;
    CWorld::bIncludeCarTyres = false;
    CWorld::bIncludeBikers   = false;
    CWorld::pIgnoreEntity    = nullptr;

    //> 0x741DC4 - Check if hit entity is within range
    if (shotHitEntity) {
        if (TargetWeaponRangeMultiplier(shotHitEntity, owner) * wi->m_fWeaponRange >= (camOriginPos - shotCP.m_vecPoint).SquaredMagnitude2D()) {
            shotHitEntity = nullptr;
        }
    }

    DoBulletImpact(owner, shotHitEntity, camOriginPos, camTargetPos, shotCP, false);

    //> 0x741E48 - Visual/physical feedback for the player(s)
    if (owner->IsPlayer()) {
        auto intensity = [&]{
            switch (m_Type) {
            case eWeaponType::WEAPON_AK47:
                return 0.00015f;
            case eWeaponType::WEAPON_M4:
                return 0.0003f;
            default:
                return 0.0002f;
            }
        }();
        if (FindPlayerPed()->bIsDucking || FindPlayerPed()->m_pAttachedTo) {
            intensity *= 0.3f;
        }

        // Move the camera around a little
        cam->m_fHorizontalAngle += (float)CGeneral::GetRandomNumberInRange(-64, 64) * intensity;
        cam->m_fVerticalAngle += (float)CGeneral::GetRandomNumberInRange(-64, 64) * intensity;

        // Do pad shaking
        const auto shakeFreq = (uint8_t)(130.f + (210.f - 130.f) * std::clamp((20.f - (wi->m_fAnimLoopEnd - wi->m_fAnimLoopStart) * 900.f) / 80.f, 0.f, 1.f));
        CPad::GetPad(owner->GetPadNumber())->StartShake(
            (int16_t)(CTimer::GetTimeStep() * 20000.f / (float)shakeFreq),
            shakeFreq,
            0
        );
    }

    return true;
}

// 0x73EC40 - ported from decomp (src/CWeapon/FireInstantHitFromCar_0073ec40.c)
// NOTE: RenderWare bone transforms (driver hand bone) are shimmed - see below.
namespace {
    // TODO(rw): real RpHAnimHierarchy bone transform; currently offsets by ped position.
    CVector TransformDriverHandBone(CPed* driver, const CVector& point) {
        return point + driver->GetPosition();
    }
}

// 0x73EC40
bool CWeapon::FireInstantHitFromCar(CVehicle* vehicle, bool leftSide, bool rightSide) {
    const auto wi = CWeaponInfo::GetWeaponInfo(m_Type, eWeaponSkill::STD);

    CVector startPoint{};
    CVector endPoint{};

    if (vehicle->m_nVehicleType != VEHICLE_TYPE_BIKE) {
        if (rightSide) {
            startPoint.x = wi->m_vecFireOffset.x * 1.8f;
            startPoint.y = wi->m_vecFireOffset.y * 1.8f;
            startPoint.z = wi->m_vecFireOffset.z * 1.8f - 0.1f;
        } else {
            startPoint = wi->m_vecFireOffset;
        }
        startPoint = TransformDriverHandBone(vehicle->m_pDriver, startPoint);
        startPoint += vehicle->m_vecMoveSpeed * CTimer::GetTimeStep();

        const float range = wi->m_fWeaponRange;
        const CVector& right = vehicle->GetMatrix().GetRight();
        const CVector& fwd = vehicle->GetMatrix().GetForward();
        if (leftSide) {
            endPoint = startPoint - right * range;
        } else if (rightSide) {
            endPoint = startPoint + right * range;
        } else {
            endPoint = startPoint + fwd * range;
        }
    } else if (vehicle->m_pDriver) {
        startPoint = wi->m_vecFireOffset;
        startPoint = TransformDriverHandBone(vehicle->m_pDriver, startPoint);
        startPoint += vehicle->m_vecMoveSpeed * CTimer::GetTimeStep();

        const float range = wi->m_fWeaponRange;
        const CVector& right = vehicle->GetMatrix().GetRight();
        const CVector& fwd = vehicle->GetMatrix().GetForward();
        if (leftSide) {
            endPoint = startPoint - right * range;
        } else if (rightSide) {
            endPoint = startPoint + right * range;
        } else {
            endPoint = startPoint + fwd * range;
        }
        // TODO(data): bike model-specific spread (0x1bf/0x1d5/0x234) needs model IDs
    } else {
        // TODO(rw/data): unoccupied bike dummy positions need CModelInfo + CMatrix::Scale
        return false;
    }

    // Add random spread
    endPoint.x += (float)(CGeneral::GetRandomNumber() & 0xff) * 0.01f - 1.28f;
    endPoint.y += (float)(CGeneral::GetRandomNumber() & 0xff) * 0.01f - 1.28f;
    endPoint.z += (float)(CGeneral::GetRandomNumber() & 0xff) * 0.01f - 1.28f;

    CEntity* owner = FindPlayerPed();
    DoDriveByAutoAiming(owner, vehicle, &startPoint, &endPoint, false);
    FireInstantHitFromCar2(startPoint, endPoint, vehicle, vehicle->m_pDriver);
    return true;
}

// 0x73F480 - ported from decomp (src/CWeapon/CheckForShootingVehicleOccupant_0073f480.c)
// NOTE: RenderWare head-bone transforms are shimmed via GetBonePosition.
bool CWeapon::CheckForShootingVehicleOccupant(CEntity** pCarEntity, CColPoint* colPoint, eWeaponType weaponType, const CVector& origin, const CVector& target) {
    CVehicle* vehicle = (CVehicle*)*pCarEntity;
    if (vehicle->GetType() != ENTITY_TYPE_VEHICLE) {
        return false;
    }

    CColLine shotLine(origin, target);
    float maxDist = 1.0f;
    bool hitOccupant = false;

    const auto TestOccupantHead = [&](CPed* occupant) {
        if (!occupant) {
            return;
        }
        // TODO(data): the 0x4000000 "shootable" ped flag check needs the full flags layout
        CVector headPos = occupant->GetBonePosition((eBoneTag)BONE_HEAD_ID);
        headPos.z += 0.1f;
        CColSphere headSphere{};
        headSphere.Set(0.2f, headPos, (eColSurfaceType)SURFACE_DEFAULT, 9);
        if (CCollision::ProcessLineSphere(&shotLine, &headSphere, colPoint, &maxDist)) {
            *pCarEntity = occupant;
            hitOccupant = true;
        }
    };

    TestOccupantHead(vehicle->m_pDriver);
    for (CPed* passenger : vehicle->m_apPassengers) {
        TestOccupantHead(passenger);
    }

    if (vehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
        // TODO(data): windscreen glass damage (CDamageManager::ProgressPanelDamage)
        // needs the damage-manager and collision-triangle plumbing.
    }

    if (!hitOccupant) {
        *pCarEntity = vehicle;
    }
    return hitOccupant;
}

// 0x73FA20
bool CWeapon::FireFromCar(CVehicle* vehicle, bool leftSide, bool rightSide) {
    if (m_State != WEAPONSTATE_READY || m_AmmoInClip <= 0) {
        return false;
    }
    if (!CWeapon::FireInstantHitFromCar(vehicle, leftSide, rightSide)) {
        return false;
    }
    if (const auto d = vehicle->m_pDriver) {
        d->GetWeaponAE().AddAudioEvent(AE_WEAPON_FIRE);
    }
    if (!CCheat::IsActive(CHEAT_INFINITE_AMMO)) {
        if (m_AmmoInClip) {
            m_AmmoInClip--;
        }
        if (  m_TotalAmmo < 25'000 && m_TotalAmmo > 0
            && (vehicle->GetStatus() != STATUS_PLAYER || CStats::GetPercentageProgress() < 100.f)
        ) {
            m_TotalAmmo--;
        }
    }
    m_State = WEAPONSTATE_FIRING;
    if (m_AmmoInClip) {
        m_TimeForNextShotMs = CTimer::GetTimeInMS() + 1000; // NOTE: Shoot delay can be adjusted here
    } else if (m_TotalAmmo) {
        m_State = WEAPONSTATE_RELOADING;
        m_TimeForNextShotMs = CTimer::GetTimeInMS() + GetWeaponInfo().GetWeaponReloadTime();
    }
    return true;
}

// 0x73FB10 - ported from decomp (src/CWeapon/FireInstantHit_0073fb10.c)
// NOTE: RenderWare bone-transform branches are shimmed; ped skill-byte at
// +0x302 and several flag bits are TODO(data).
bool CWeapon::FireInstantHit(CEntity* firingEntity, CVector* origin, CVector* muzzlePosn, CEntity* targetEntity, CVector* target, CVector* originForDriveBy, bool arg6, bool muzzle) {
    constexpr auto PLAYER_AIM_SCALE      = 0.75f;
    constexpr auto PLAYER_AIM_SCALE_DIST = 5.00f;
    constexpr auto PLAYER_ANIM_ROT_RATE  = 0.0062832f;
    constexpr auto SHOTGUN_SPREAD_RATE   = 0.05f;
    constexpr auto SHOTGUN_NUM_PELLETS   = 15;
    constexpr auto SPAS_NUM_PELLETS      = 4;

    const eWeaponSkill skill = firingEntity->GetIsTypePed()
        ? firingEntity->AsPed()->GetWeaponSkill(m_Type)
        : eWeaponSkill::STD;
    const auto wi = CWeaponInfo::GetWeaponInfo(m_Type, skill);

    CVector vecGunMuzzle = *muzzlePosn;
    CVector vecStart = *origin;
    CVector vecEnd{ 0.f, 0.f, 0.f };
    CColPoint colPoint{};
    CEntity* hitEntity{};

    if (originForDriveBy) {
        vecStart = *originForDriveBy;
    }

    CPed* pedFiring{};
    CTaskSimpleUseGun* useGunTask{};
    float accuracySpread = 0.f;
    if (firingEntity->GetIsTypePed()) {
        pedFiring = firingEntity->AsPed();
        // TODO(data): (100 - pedSkillByte) / accuracy; pedSkillByte is at CPed+0x302
        accuracySpread = 100.f / wi->m_fAccuracy;
        // TODO(data): the 0x4000000 halving flag needs the full ped flags layout
        useGunTask = pedFiring->GetIntelligence()->GetTaskUseGun();
    }

    const auto type = m_Type;
    if (type == eWeaponType::WEAPON_SHOTGUN || type == eWeaponType::WEAPON_SAWNOFF_SHOTGUN
        || type == eWeaponType::WEAPON_SPAS12_SHOTGUN) {
        accuracySpread = 0.f;
        CWorld::fWeaponSpreadRate = SHOTGUN_SPREAD_RATE / wi->m_fAccuracy;
    }

    CVector vecShotDir{};
    if (!useGunTask || !useGunTask->m_SkipAim) {
        if (!pedFiring) {
        defaultAim:
            // Non-ped firing entity (or player not in aim mode): shoot along forward
            if (firingEntity->GetType() != ENTITY_TYPE_VEHICLE) {
                const float range = wi->m_fWeaponRange;
                const CVector& fwd = firingEntity->GetForward();
                vecEnd = vecGunMuzzle + fwd * range;
                vecShotDir = fwd;
                if (pedFiring && pedFiring->bIsDucking) {
                    DoDoomAiming(firingEntity, &vecStart, &vecEnd);
                }
                CWorld::pIgnoreEntity = pedFiring && pedFiring->m_pVehicle ? pedFiring->m_pVehicle
                    : pedFiring && pedFiring->m_pAttachedTo && pedFiring->m_pAttachedTo->GetIsTypeVehicle() ? pedFiring->m_pAttachedTo
                    : firingEntity;
            } else {
                // Vehicle firing (drive-by): apply drive-by auto-aim + spread
                accuracySpread = 0.6f;
                vecShotDir = firingEntity->GetForward();
                vecEnd = vecStart + vecShotDir * wi->m_fWeaponRange;
                // TODO(data): model-specific drive-by spread (0x1bf/0x1d5/0x234) needs model IDs
                DoDriveByAutoAiming(firingEntity->AsPed()->m_pVehicle ? (CEntity*)FindPlayerPed() : firingEntity,
                    (CVehicle*)firingEntity, &vecStart, &vecEnd, true);
                vecShotDir = (vecEnd - vecStart).Normalized();
                const float rnd1 = (float)CGeneral::GetRandomNumber() * 3.051851e-05f;
                const float rnd2 = (float)CGeneral::GetRandomNumber() * 3.051851e-05f;
                const float rnd3 = (float)CGeneral::GetRandomNumber() * 3.051851e-05f;
                vecShotDir.x += (rnd3 * 0.4f - 0.2f) * accuracySpread;
                vecShotDir.y += (rnd2 * 0.4f - 0.2f) * accuracySpread;
                vecShotDir.z += (rnd1 * 0.2f - 0.1f) * accuracySpread;
                vecShotDir.Normalise();
                vecEnd = vecStart + vecShotDir * wi->m_fWeaponRange;
                CWorld::pIgnoreEntity = firingEntity;
            }
            CWorld::bIncludeBikers = true;
        } else {
            // Ped firing entity
            if (!targetEntity && !target) {
                const bool isPlayer = pedFiring->IsPlayer();
                const int32_t camMode = TheCamera.m_aCams[0].m_nMode; // TODO: active cam index
                const bool isAimMode = camMode == 0x35 || camMode == 0x37 || camMode == 0x41 || camMode == 0x31;
                if (!isPlayer || !isAimMode) {
                    goto defaultAim;
                }
                // Player 3rd-person aim: target from camera
                CVector camPos, camTargetPos;
                TheCamera.Find3rdPersonCamTargetVector(wi->m_fWeaponRange * 3.f, vecStart, &camPos, &camTargetPos);
                vecEnd = camTargetPos;
                vecShotDir = (vecEnd - vecStart).Normalized();
                if (accuracySpread != 0.f) {
                    // Apply aim wobble
                    const float aimScale = std::min(PLAYER_AIM_SCALE_DIST / wi->m_fWeaponRange * 3.f, 1.f)
                        * accuracySpread * pedFiring->GetPlayerData()->m_fAttackButtonCounter * PLAYER_AIM_SCALE;
                    const float timeS = (float)CTimer::GetTimeInMS() / 1000.f;
                    const float s = std::sin(timeS * PLAYER_ANIM_ROT_RATE);
                    const float c = std::cos(timeS * PLAYER_ANIM_ROT_RATE);
                    // TODO(rw): the perpendicular basis uses camera vectors; approximated
                    CVector right = vecShotDir.Cross(CVector{ 0.f, 0.f, 1.f }).Normalized();
                    CVector up = right.Cross(vecShotDir);
                    vecEnd += (right * s + up * c) * aimScale;
                    pedFiring->GetPlayerData()->m_fAttackButtonCounter += (float)wi->m_nDamage * 0.04f;
                }
                CWorld::pIgnoreEntity = pedFiring->m_pVehicle ? (CEntity*)pedFiring->m_pVehicle
                    : pedFiring->m_pAttachedTo && !pedFiring->m_pAttachedTo->GetIsTypeVehicle() ? firingEntity
                    : firingEntity;
                CWorld::bIncludeDeadPeds = true;
                CWorld::bIncludeCarTyres = true;
                CWorld::bIncludeBikers = true;
            } else {
                // Ped has a target: aim at it
                // TODO(rw): pedIK.bGunReachedTarget check needs the IK layout
                if (!arg6) {
                    goto afterLos;
                }
                CVector targetPt;
                if (!target) {
                    if (!targetEntity->GetIsTypePed()) {
                        targetPt = targetEntity->GetPosition();
                    } else if (pedFiring->GetPlayerData()) {
                        // TODO(rw): GetTransformedBonePosition needs the bone layout
                        targetPt = targetEntity->GetPosition();
                    } else {
                        targetPt = targetEntity->AsPed()->GetBonePosition((eBoneTag)BONE_SPINE1_ID);
                    }
                } else {
                    targetPt = *target;
                }
                vecShotDir = (targetPt - vecStart).Normalized();
                const float range = TargetWeaponRangeMultiplier(targetEntity, pedFiring) * wi->m_fWeaponRange;
                vecEnd = vecStart + vecShotDir * range;
                if (accuracySpread > 0.f) {
                    if (!pedFiring->GetPlayerData()) {
                        const float rnd1 = (float)CGeneral::GetRandomNumber() * 3.051851e-05f;
                        const float rnd2 = (float)CGeneral::GetRandomNumber() * 3.051851e-05f;
                        const float rnd3 = (float)CGeneral::GetRandomNumber() * 3.051851e-05f;
                        vecShotDir.x += (rnd3 * 0.4f - 0.2f) * accuracySpread;
                        vecShotDir.y += (rnd2 * 0.4f - 0.2f) * accuracySpread;
                        vecShotDir.z += (rnd1 * 0.2f - 0.1f) * accuracySpread;
                        vecEnd = vecStart + vecShotDir.Normalized() * range;
                    } else {
                        // TODO: player targeted spread (aim wobble) - see 3rd-person branch above
                    }
                }
                CWorld::pIgnoreEntity = pedFiring->m_pVehicle ? (CEntity*)pedFiring->m_pVehicle
                    : pedFiring->m_pAttachedTo && pedFiring->m_pAttachedTo->GetIsTypeVehicle() ? (CEntity*)pedFiring->m_pAttachedTo
                    : firingEntity;
                if (pedFiring->IsPlayer()) {
                    CWorld::bIncludeDeadPeds = true;
                }
                CWorld::bIncludeBikers = true;
            }
        }
        // Line of sight
        {
            CBirds::HandleGunShot(&vecStart, &vecEnd);
            // CShadows::GunShotSetsOilOnFire(vecStart, vecEnd); // TODO(shadows)
            CWorld::ProcessLineOfSight(vecStart, vecEnd, colPoint, hitEntity,
                true, true, true, true, true, false, false, true);
            if (hitEntity && pedFiring && pedFiring->IsPlayer()) {
                const float dist2D = (colPoint.m_vecPoint - vecStart).Magnitude2D();
                if (dist2D <= TargetWeaponRangeMultiplier(hitEntity, pedFiring) * wi->m_fWeaponRange) {
                    CheckForShootingVehicleOccupant(&hitEntity, &colPoint, m_Type, vecStart, vecEnd);
                } else {
                    hitEntity = nullptr;
                }
            }
        }
    } else {
        // m_SkipAim: shoot straight forward from muzzle
        // TODO(rw): the original transforms by an (elided) matrix; approximated
        vecShotDir = firingEntity->GetForward();
        vecEnd = vecStart + vecShotDir * wi->m_fWeaponRange;
        CWorld::pIgnoreEntity = pedFiring && pedFiring->m_pVehicle ? (CEntity*)pedFiring->m_pVehicle
            : pedFiring && pedFiring->m_pAttachedTo ? (CEntity*)pedFiring->m_pAttachedTo
            : firingEntity;
        CWorld::bIncludeBikers = true;
        CBirds::HandleGunShot(&vecStart, &vecEnd);
        CWorld::ProcessLineOfSight(vecStart, vecEnd, colPoint, hitEntity,
            true, true, true, true, true, false, false, true);
    }
afterLos:;

    // 0x73B0CC-ish: fire events (LAB_00740b71)
    const bool hasNoSound = type == eWeaponType::WEAPON_PISTOL_SILENCED || type == eWeaponType::WEAPON_TEARGAS;
    vecStart = *origin;

    {
        CEventGunShot gs(firingEntity, vecStart, vecEnd, hasNoSound);
        GetEventGlobalGroup()->Add(&gs, false);

        CEventGunShotWhizzedBy gsw(firingEntity, vecStart, vecEnd, type == eWeaponType::WEAPON_PISTOL_SILENCED);
        GetEventGlobalGroup()->Add(&gsw, false);

        g_InterestingEvents.Add(CInterestingEvents::EType::INTERESTING_EVENT_22, firingEntity);
    }

    if (muzzle) {
        float shellOffset = 0.f;
        float shellSize = 0.f;
        bool doMuzzleFlash = true;
        switch (type) {
        case eWeaponType::WEAPON_PISTOL:
        case eWeaponType::WEAPON_PISTOL_SILENCED:
        case eWeaponType::WEAPON_DESERT_EAGLE:
        case eWeaponType::WEAPON_SNIPERRIFLE:
            shellOffset = 0.2f; shellSize = 0.25f;
            break;
        case eWeaponType::WEAPON_SHOTGUN:
        case eWeaponType::WEAPON_SAWNOFF_SHOTGUN:
        case eWeaponType::WEAPON_SPAS12_SHOTGUN:
            shellOffset = 0.3f; shellSize = 0.35f;
            break;
        case eWeaponType::WEAPON_MICRO_UZI:
        case eWeaponType::WEAPON_MP5:
        case eWeaponType::WEAPON_TEC9:
            shellOffset = 0.2f; shellSize = 0.3f;
            break;
        case eWeaponType::WEAPON_AK47:
        case eWeaponType::WEAPON_M4:
        case eWeaponType::WEAPON_MINIGUN: {
            // Alternate the muzzle flash for high-rate weapons
            static uint32_t s_muzzleFlashCounter{};
            if (!((CGeneral::GetRandomNumber() > 0x31) || ((++s_muzzleFlashCounter & 1) == 0))) {
                doMuzzleFlash = false;
            } else {
                shellOffset = 0.65f; shellSize = 0.25f;
            }
            break;
        }
        default:
            doMuzzleFlash = false;
            break;
        }
        if (doMuzzleFlash) {
            CPointLights::AddLight(PLTYPE_POINTLIGHT, vecGunMuzzle, CVector{0.f, 0.f, 0.f},
                3.0f, 0.25f, 0.22f, 0.0f, 0, false, nullptr);
            g_fx.TriggerGunshot();
            const CVector shellPos = vecGunMuzzle - vecShotDir * shellOffset;
            const CVector2D shellDir{ firingEntity->GetMatrix().GetRight().x, firingEntity->GetMatrix().GetRight().y };
            AddGunshell(firingEntity, const_cast<CVector&>(shellPos), shellDir, shellSize);
        }
    }

    // Water splash check
    {
        bool checkWater = false;
        if (!targetEntity) {
            const bool pedFiringIsPlayer = pedFiring && pedFiring->IsPlayer();
            const int32_t typeCat = (int32_t)firingEntity->GetType() >> 3;
            if ((!pedFiring || !pedFiringIsPlayer || vecStart.z > vecEnd.z)
                && (typeCat != 0 && typeCat != 8 || vecStart.z > vecEnd.z)) {
                checkWater = true;
            }
        } else {
            const auto tgtType = (int32_t)targetEntity->GetType() & 7;
            // TODO(data): the 0x100 flag check needs the full entity flags layout
            if (tgtType >= 2 && tgtType <= 4) {
                checkWater = true;
            }
        }
        if (checkWater && TestLineAgainstWater(vecStart.x, vecStart.y)) {
            const CVector splashPos{ 0.f, 0.f, 0.f };
            g_fx.TriggerBulletSplash(splashPos);
            AudioEngine.ReportBulletHit(nullptr, SURFACE_WATER_SHALLOW, splashPos, 0.f);
        }
    }

    // Bullet impact (or shotgun pellet spread)
    const bool isWheelShot = hitEntity && hitEntity->GetIsTypeVehicle()
        && colPoint.m_nPieceTypeB > 12 && colPoint.m_nPieceTypeB < 17;
    if (CWorld::fWeaponSpreadRate <= 0.f || !hitEntity || isWheelShot) {
        DoBulletImpact(firingEntity, hitEntity, vecGunMuzzle, vecEnd, colPoint, 0);
    } else {
        // Shotgun pellet spread
        int32_t iterations = 0;
        while (hitEntity) {
            iterations++;
            const int32_t numPellets = type == eWeaponType::WEAPON_SPAS12_SHOTGUN ? SPAS_NUM_PELLETS : SHOTGUN_NUM_PELLETS;
            CMatrix pelletMat{};
            SetUpPelletCol(numPellets, firingEntity, hitEntity, vecStart, colPoint, pelletMat);

            float pelletDepths[15];
            for (int32_t i = 0; i < 15; i++) {
                pelletDepths[i] = 1.f;
            }
            CColModel* cmB;
            if (hitEntity->GetIsTypePed()) {
                // TODO(rw): AnimatePedColModelSkinned needs the ped model-info layout
                cmB = hitEntity->GetColModel();
            } else {
                cmB = hitEntity->GetColModel();
            }
            CCollision::ProcessColModels(&pelletMat, &ms_PelletTestCol, &hitEntity->GetMatrix(), cmB,
                CWorld::m_aTempColPts, CWorld::m_aTempColPts, pelletDepths, false);

            int32_t hitCount = 0;
            int32_t lastHitIdx = 0;
            for (int32_t i = 0; i < numPellets; i++) {
                if (pelletDepths[i] < 1.f) {
                    hitCount++;
                    lastHitIdx = i;
                }
            }
            for (int32_t i = 0; i < numPellets; i++) {
                if (pelletDepths[i] < 1.f) {
                    const int32_t incrementalHit = (i == lastHitIdx) ? -hitCount : 1;
                    const CColPoint& pelletCP = CWorld::m_aTempColPts[i];
                    DoBulletImpact(firingEntity, hitEntity, vecGunMuzzle, pelletCP.m_vecPoint, pelletCP, incrementalHit);
                }
            }

            const auto hitType = hitEntity->GetType();
            if (hitType == ENTITY_TYPE_PED || hitType == ENTITY_TYPE_VEHICLE) {
                if ((pelletDepths[0] != 1.f && (float)hitCount / (float)numPellets >= 0.5f) || iterations > 1) {
                    break;
                }
                // Re-shoot from the hit point, ignoring the entity just hit
                CWorld::pIgnoreEntity = hitEntity;
                vecStart = colPoint.m_vecPoint;
                hitEntity = nullptr;
                CBirds::HandleGunShot(&vecStart, &vecEnd);
                CWorld::ProcessLineOfSight(vecStart, vecEnd, colPoint, hitEntity,
                    true, true, true, true, true, false, false, true);
            } else {
                DoBulletImpact(firingEntity, hitEntity, vecGunMuzzle, vecEnd, colPoint, 0);
                break;
            }
        }
    }

    CWorld::ResetLineTestOptions();
    return true;
}

// 0x742300
bool CWeapon::Fire(CEntity* firedBy, CVector* startPosn, CVector* barrelPosn, CEntity* targetEnt, CVector* targetPosn, CVector* altPosn) {
    const auto firedByPed = firedBy && firedBy->GetIsTypePed()
        ? firedBy->AsPed()
        : nullptr;
    const auto wi = &GetWeaponInfo(firedByPed);

    CVector point{ 0.f, 0.f, 0.6f };

    const auto fxPos = startPosn
        ? startPosn
        : &point;
    const auto shotOrigin = startPosn
        ? barrelPosn
        : &point;
    if (!startPosn) {
        point     = firedBy->GetMatrix().TransformPoint(point);
        startPosn = &point;
    }

    if (m_IsFirstPersonWeaponModeSelected) {
        const auto r = 0.15f;

        const auto h = firedBy->GetHeading();
        fxPos->x -= std::sin(h) * r;
        fxPos->y += std::cos(h) * r;
    }

    switch (m_State) {
    case WEAPONSTATE_READY:
    case WEAPONSTATE_FIRING:
        break;
    default:
        return false;
    }

    if (!m_AmmoInClip) {
        if (!m_TotalAmmo) {
            return false;
        }
        m_AmmoInClip = std::min<uint32_t>(m_TotalAmmo, wi->m_nAmmoClip);
    }

    const auto [hasFired, delayNextShot] = [&]() -> std::pair<bool, bool> {
        switch (m_Type) {
        case eWeaponType::WEAPON_GRENADE:
        case eWeaponType::WEAPON_TEARGAS:
        case eWeaponType::WEAPON_MOLOTOV:
        case eWeaponType::WEAPON_REMOTE_SATCHEL_CHARGE: { // 0x74268B
            if (targetPosn) {
                return {
                    FireProjectile( // 0x742705
                        firedBy,
                        *shotOrigin,
                        targetEnt,
                        targetPosn,
                        std::clamp(((firedBy->GetPosition() - *targetPosn).Magnitude() - 10.f) / 10.f, 0.2f, 1.f)
                    ),
                    true
                };
            } else if (firedBy == FindPlayerPed()) { // 0x74271F
                return {
                    FireProjectile(
                        firedBy,
                        *shotOrigin,
                        targetEnt,
                        nullptr,
                        firedBy->AsPed()->GetPlayerData()->m_fAttackButtonCounter * 0.0375f
                    ),
                    true
                };
            }
            return {
                FireProjectile( // 0x74274E
                    firedBy,
                    *shotOrigin,
                    targetEnt,
                    nullptr,
                    0.3f
                ),
                true
            };
        }
        case eWeaponType::WEAPON_PISTOL:
        case eWeaponType::WEAPON_PISTOL_SILENCED:
        case eWeaponType::WEAPON_DESERT_EAGLE:
        case eWeaponType::WEAPON_MICRO_UZI:
        case eWeaponType::WEAPON_MP5:
        case eWeaponType::WEAPON_AK47:
        case eWeaponType::WEAPON_M4:
        case eWeaponType::WEAPON_TEC9:
        case eWeaponType::WEAPON_COUNTRYRIFLE:
        case eWeaponType::WEAPON_MINIGUN: { // 0x7424FE
            if (   firedByPed
                && firedByPed->m_nPedType == PED_TYPE_PLAYER1
                && (TheCamera.m_PlayerWeaponMode.m_nMode == MODE_M16_1STPERSON
                 || TheCamera.m_PlayerWeaponMode.m_nMode == MODE_HELICANNON_1STPERSON)
            ) {
                return { FireM16_1stPerson(firedByPed), true };
            }
            const auto fired = FireInstantHit(firedBy, startPosn, shotOrigin, targetEnt, targetPosn, altPosn, false, true);
            if (firedByPed) { // 0x74255B
                if (!firedByPed->bInVehicle) {
                    return { fired, false };
                }
                if (const auto t = firedByPed->GetTaskManager().GetActiveTask()) {
                    return { fired, t->GetTaskType() == TASK_SIMPLE_GANG_DRIVEBY };
                }
            }
            return { fired, true };
        }
        case eWeaponType::WEAPON_SHOTGUN:
        case eWeaponType::WEAPON_SAWNOFF_SHOTGUN:
        case eWeaponType::WEAPON_SPAS12_SHOTGUN:
            return {
                FireInstantHit( // 0x742495
                    firedBy,
                    startPosn,
                    shotOrigin,
                    targetEnt,
                    targetPosn,
                    altPosn,
                    false,
                    true
                ),
                true
            };
        case eWeaponType::WEAPON_SNIPERRIFLE: { // 0x7424AC
            if (firedByPed && firedByPed->m_nPedType == PED_TYPE_PLAYER1 && TheCamera.m_PlayerWeaponMode.m_nMode == MODE_SNIPER) {
                return {
                    FireSniper(firedByPed, targetEnt, targetPosn),
                    true
                };
            }
            return {
                FireInstantHit(
                    firedBy,
                    startPosn,
                    shotOrigin,
                    targetEnt,
                    targetPosn,
                    nullptr,
                    false,
                    true
                ),
                true
            };
        }
        case eWeaponType::WEAPON_RLAUNCHER:
        case eWeaponType::WEAPON_RLAUNCHER_HS: { // 0x7425B3
            if (firedByPed) {
                const auto CanFire = [&](CVector origin, CVector end) {
                    return (origin - end).SquaredMagnitude() <= 8.f * 8.f && !firedBy->GetIsTypePed();
                };
                if (   targetEnt  && !CanFire(firedBy->GetPosition(), targetEnt->GetPosition())
                    || targetPosn && !CanFire(firedBy->GetPosition(), *targetPosn)
                ) {
                    return std::pair<bool, bool>{ false, true };
                }
            }
            return {
                FireProjectile(
                    firedBy,
                    *shotOrigin,
                    targetEnt,
                    targetPosn
                ),
                true
            };
        }
        case eWeaponType::WEAPON_FLAMETHROWER:
        case eWeaponType::WEAPON_SPRAYCAN:
        case eWeaponType::WEAPON_EXTINGUISHER:
            return {
                FireAreaEffect(
                    firedBy,
                    *shotOrigin,
                    targetEnt,
                    targetPosn
                ),
                true
            };
        case eWeaponType::WEAPON_DETONATOR: {
            assert(firedByPed);
            CWorld_Extra::UseDetonator(firedByPed);
            m_AmmoInClip = m_TotalAmmo  = 1;
            return std::pair<bool, bool>{ true, true };
        }
        case eWeaponType::WEAPON_CAMERA:
            return {
                TakePhotograph(firedBy, shotOrigin),
                true
            };
        default:
            return std::pair<bool, bool>{ false, true }; // NOTSA_UNREACHABLE placeholder
        }
    }();

    // 0x74279A
    if (hasFired) {
        // 0x7427B3
        const bool isPlayerFiring = firedByPed && m_Type != eWeaponType::WEAPON_CAMERA && firedByPed->IsPlayer();
        if (firedByPed) {
            if (m_Type != eWeaponType::WEAPON_CAMERA) {
                firedByPed->bFiringWeapon = true;
            }
            firedByPed->GetWeaponAE().AddAudioEvent(AE_WEAPON_FIRE);
            if (isPlayerFiring && targetEnt && targetEnt->GetIsTypePed() && m_Type != eWeaponType::WEAPON_PISTOL_SILENCED) {
                firedByPed->Say((eGlobalSpeechContext)CTX_GLOBAL_SHOOT, 200); // 0x74280E
            }
        }

        // 0x74282C
        if (m_Type == eWeaponType::WEAPON_REMOTE_SATCHEL_CHARGE) {
            firedByPed->GiveWeapon(eWeaponType::WEAPON_DETONATOR, true, true);
            if (firedByPed->GetWeapon(eWeaponType::WEAPON_REMOTE_SATCHEL_CHARGE).m_TotalAmmo <= 1) {
                firedByPed->GetWeapon(eWeaponType::WEAPON_DETONATOR).m_State = eWeaponState::WEAPONSTATE_READY;
                firedByPed->SetCurrentWeapon(eWeaponType::WEAPON_DETONATOR);
            }
        }

        //> 0x74286D - Increase stats
        if (isPlayerFiring) {
            switch (m_Type)
            {
            case eWeaponType::WEAPON_GRENADE:
            case eWeaponType::WEAPON_MOLOTOV:
            case eWeaponType::WEAPON_ROCKET:
            case eWeaponType::WEAPON_RLAUNCHER:
            case eWeaponType::WEAPON_RLAUNCHER_HS:
            case eWeaponType::WEAPON_REMOTE_SATCHEL_CHARGE:
            case eWeaponType::WEAPON_DETONATOR:
                CStats::IncrementStat(STAT_KGS_OF_EXPLOSIVES_USED);
                break;
            case eWeaponType::WEAPON_PISTOL:
            case eWeaponType::WEAPON_PISTOL_SILENCED:
            case eWeaponType::WEAPON_DESERT_EAGLE:
            case eWeaponType::WEAPON_SHOTGUN:
            case eWeaponType::WEAPON_SAWNOFF_SHOTGUN:
            case eWeaponType::WEAPON_SPAS12_SHOTGUN:
            case eWeaponType::WEAPON_MICRO_UZI:
            case eWeaponType::WEAPON_MP5:
            case eWeaponType::WEAPON_AK47:
            case eWeaponType::WEAPON_M4:
            case eWeaponType::WEAPON_TEC9:
            case eWeaponType::WEAPON_COUNTRYRIFLE:
            case eWeaponType::WEAPON_SNIPERRIFLE:
            case eWeaponType::WEAPON_MINIGUN:
                CStats::IncrementStat(STAT_BULLETS_FIRED);
                break;
            default:
                break;
            }
        }

        // 0x7428A6
        if (!CCheat::IsActive(CHEAT_INFINITE_AMMO)) {
            if (m_AmmoInClip) {
                m_AmmoInClip--;
            }
            if (m_TotalAmmo > 0) {
                if (isPlayerFiring
                        ? m_Type == eWeaponType::WEAPON_DETONATOR || CStats::GetPercentageProgress() < 100.f
                        : m_TotalAmmo < 25'000
                ) {
                    m_TotalAmmo--;
                }
            }
        }

        m_State = WEAPONSTATE_FIRING;

        if (!m_AmmoInClip) { // 0x7428FB
            if (m_TotalAmmo) {
                m_State = WEAPONSTATE_RELOADING;
                m_TimeForNextShotMs = s_DebugSettings.NoShotDelay
                    ? 0
                    : firedBy == FindPlayerPed() && FindPlayerInfo().m_bFastReload
                        ? wi->GetWeaponReloadTime() / 4
                        : wi->GetWeaponReloadTime();
                m_TimeForNextShotMs += CTimer::GetTimeInMS();
            } else if (TheCamera.GetActiveCam().m_nMode == MODE_CAMERA) {
                CPad::GetPad()->Clear(false, true);
            }
            return true;
        }

        m_TimeForNextShotMs = s_DebugSettings.NoShotDelay
            ? 0
            : delayNextShot
                ? m_Type == eWeaponType::WEAPON_CAMERA
                    ? 1100
                    : (uint32_t)((wi->m_fAnimLoopEnd - wi->m_fAnimLoopStart) * 900.f)
                : 0;
        m_TimeForNextShotMs += CTimer::GetTimeInMS();
    }
    // 0x7429F2
    if (m_Type == eWeaponType::WEAPON_UNARMED || m_Type == eWeaponType::WEAPON_BASEBALLBAT) {
        return true;
    }
    return hasFired;
}
