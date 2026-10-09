// CObject.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CObject (dynamic world objects: props, mission
// objects, temp objects, doors, ...).
// Adapted from gta-reversed/source/game_sa/Entity/Object/Object.cpp and
// verified against the decomp (src/CObject/*.c). Model-data/ropes/garages/
// audio dependent bodies are marked TODO(port) until those subsystems land.
//
// vtable: overrides SetIsStatic, CreateRwObject, ProcessControl, Teleport,
// SpecialEntityPreCollisionStuff, SpecialEntityCalcCollisionSteps, PreRender,
// Render, SetupLighting, RemoveLighting. All are defined here.
//
// Clean-room notes:
// - operator new/delete use the global heap (TODO: object pool).
// - The control-code list is a file-local double-link list
//   (TODO: unify with CWorld state when the world subsystem is finalized).

#include "CObject.h"
#include "CWorld.h"
#include "CTimer.h"

#include <cassert>
#include <cmath>
#include <algorithm>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;
} // namespace

// ---------------------------------------------------------------------------
// Local clean-room mirrors (TODO: replace with ported core headers)
// ---------------------------------------------------------------------------

// Minimal CPtrNodeDoubleLink (gta-reversed core).
template<typename T>
class CPtrNodeDoubleLink {
public:
    T m_item;
    CPtrNodeDoubleLink* m_prev{ nullptr };
    CPtrNodeDoubleLink* m_next{ nullptr };

    explicit CPtrNodeDoubleLink(T item) : m_item(item) {}
};

// File-local control-code list (mirrors CWorld::ms_listObjectsWithControlCode).
// TODO: unify with CWorld state when the world subsystem is finalized.
static CPtrNodeDoubleLink<CObject*>* s_pControlCodeListHead = nullptr;

// ---------------------------------------------------------------------------
// ctors / dtor
// ---------------------------------------------------------------------------

// 0x5A1D10
CObject::CObject() : CPhysical() {
    m_pDummyObject = nullptr;
    Init();
    m_nObjectType = eObjectType::OBJECT_UNKNOWN;
}

// 0x5A1D70
CObject::CObject(int32_t modelId, bool bCreate) : CPhysical() {
    m_pDummyObject = nullptr;
    if (bCreate) {
        CEntity::SetModelIndex(modelId);
    } else {
        CEntity::SetModelIndexNoCreate(modelId);
    }
    Init();
}

// 0x5A1DF0
CObject::CObject(CDummyObject* dummyObj) : CPhysical() {
    // TODO(port dummy): needs CDummyObject definition (m_nModelIndex,
    // GetRwObject, GetMatrix, GetIplIndex, GetAreaCode, m_bRenderDamaged).
    (void)dummyObj;
    m_pDummyObject = nullptr;
    Init();
}

// 0x59F660
CObject::~CObject() {
    // TODO(port scripts/streaming/radar): script-brain release, colmodel ref
    // release, blip clearing, model/txd ref releases, temp-object counter.
    if (m_pFire) {
        // TODO(port fire): m_pFire->Extinguish().
        m_pFire = nullptr;
    }
    RemoveFromControlCodeList();
    m_pDummyObject = nullptr;
}

// 0x5A1D10-ish (pool not ported: global heap for now)
void* CObject::operator new(size_t size) {
    return ::operator new(size);
}

// 0x5A1EF0
void* CObject::operator new(size_t size, int32_t poolRef) {
    (void)poolRef;
    return ::operator new(size);
}

void CObject::operator delete(void* obj) {
    ::operator delete(obj);
}

void CObject::operator delete(void* obj, int32_t poolRef) {
    (void)poolRef;
    ::operator delete(obj);
}

// ---------------------------------------------------------------------------
// Overrides
// ---------------------------------------------------------------------------

// 0x5A0760
void CObject::SetIsStatic(bool isStatic) {
    CEntity::SetIsStatic(isStatic);
    physicalFlags.bDoorHitEndStop = false;
    if (!isStatic && (physicalFlags.bDisableMoveForce && m_fDoorStartAngle < -1000.0F)) {
        m_fDoorStartAngle = GetHeading();
    }
}

// 0x59F110
void CObject::CreateRwObject() {
    CEntity::CreateRwObject();
}

// 0x5A2130 (reduced: buoyancy/audio/ropes need unported subsystems)
void CObject::ProcessControl() {
    // TODO(port modelinfo/ropes/buoyancy/audio): animated-clump check,
    // damage-entity carry logic, buoyancy, explosion re-arm, RC bomb.
    objectFlags.bDamaged = false;
    if (!GetIsStuck() && !GetIsStatic()) {
        if (!physicalFlags.bDisableZ && !physicalFlags.bInfiniteMass && !physicalFlags.bDisableMoveForce) {
            m_vecForce += m_vecMoveSpeed;
            m_vecForce /= 2.0F;
            m_vecTorque += m_vecTurnSpeed;
            m_vecTorque /= 2.0F;

            const float fTimeStep = CTimer::GetTimeStep() * 0.003F;
            if (fTimeStep * fTimeStep <= m_vecForce.SquaredMagnitude() || fTimeStep * fTimeStep <= m_vecTorque.SquaredMagnitude()) {
                m_nFakePhysics = 0;
            } else {
                m_nFakePhysics++;
                if (m_nFakePhysics > 10 && !physicalFlags.bAttachedToEntity) {
                    m_nFakePhysics = 10;
                    SetIsStatic(true);
                    ResetMoveSpeed();
                    ResetTurnSpeed();
                    ResetFrictionMoveSpeed();
                    ResetFrictionTurnSpeed();
                    return;
                }
            }
        }
    }

    if (!GetIsStatic()) {
        CPhysical::ProcessControl();
    }

    if (m_bIsBIGBuilding) {
        SetIsInSafePosition(true);
    }

    // Door swing logic (garage doors etc.).
    if (physicalFlags.bDisableMoveForce && m_fDoorStartAngle > -1000.0F) {
        float fHeading = GetHeading();
        if (m_fDoorStartAngle + kPi < fHeading) {
            fHeading -= kTwoPi;
        } else if (m_fDoorStartAngle - kPi > fHeading) {
            fHeading += kTwoPi;
        }

        float fDiff = m_fDoorStartAngle - fHeading;
        if (std::fabs(fDiff) > kPi / 6.0F) {
            objectFlags.bIsDoorOpen = true;
        }

        constexpr float fMaxDoorDiff = 0.3F;
        constexpr float fDoorCutoffSpeed = 0.02F;
        constexpr float fDoorSpeedMult = 0.002F;
        fDiff = std::clamp(fDiff, -fMaxDoorDiff, fMaxDoorDiff);
        if ((fDiff > 0.0F && m_vecTurnSpeed.z < +fDoorCutoffSpeed) ||
            (fDiff < 0.0F && m_vecTurnSpeed.z > -fDoorCutoffSpeed)) {
            m_vecTurnSpeed.z += CTimer::GetTimeStep() * fDoorSpeedMult * fDiff;
        }

        // TODO(port audio): AudioEngine.ReportDoorMovement(this) when moving.

        if (!m_bIsBIGBuilding
            && !GetIsStatic()
            && std::fabs(fDiff) < 0.01F
            && (objectFlags.bIsDoorMoving || std::fabs(m_vecTurnSpeed.z) < 0.01F)) {
            SetIsStatic(true);
            ResetMoveSpeed();
            ResetTurnSpeed();
            ResetFrictionMoveSpeed();
            ResetFrictionTurnSpeed();

            if (objectFlags.bIsDoorMoving && objectFlags.bIsDoorOpen) {
                LockDoor();
            }
        }
    }
}

// 0x5A17B0
void CObject::Teleport(CVector destination, bool resetRotation) {
    CWorld::Remove(this);
    SetPosn(destination);
    // TODO: resetRotation handling (decomp keeps orientation; verify).
    (void)resetRotation;
    CEntity::UpdateRwMatrix();
    CEntity::UpdateRwFrame();
    CWorld::Add(this);
}

// 0x59FEE0 (reduced: uproot-limit branch needs CObjectData)
void CObject::SpecialEntityPreCollisionStuff(CPhysical* colPhysical, bool bIgnoreStuckCheck, bool& bCollisionDisabled, bool& bCollidedEntityCollisionIgnored, bool& bCollidedEntityUnableToMove, bool& bThisOrCollidedEntityStuck) {
    if (m_pEntityIgnoredCollision == colPhysical || colPhysical->m_pEntityIgnoredCollision == this) {
        bCollidedEntityCollisionIgnored = true;
        return;
    }

    if (m_pAttachedTo == colPhysical) {
        bCollisionDisabled = true;
    } else if (colPhysical->m_pAttachedTo == this || (m_pAttachedTo && m_pAttachedTo == colPhysical->m_pAttachedTo)) {
        bCollisionDisabled = true;
    } else if (physicalFlags.bDisableZ && !physicalFlags.bApplyGravity && !colPhysical->physicalFlags.bDisableZ) {
        bCollisionDisabled = true;
    } else {
        if (!physicalFlags.bDisableZ) {
            if (physicalFlags.bDisableMoveForce || physicalFlags.bInfiniteMass) {
                if (bIgnoreStuckCheck || GetIsStuck()) {
                    bCollisionDisabled = true;
                } else if (!colPhysical->GetIsStuck()) {
                    // Do nothing; skip further calc.
                } else if (!colPhysical->GetHasHitWall()) {
                    bThisOrCollidedEntityStuck = true;
                } else {
                    bCollidedEntityUnableToMove = true;
                }
            } else if (objectFlags.bIsLampPost && (GetUp().z < 0.66F || GetIsStuck())) {
                // TODO(port objectdata): uproot-limit check for lamp posts.
            }
        } else {
            if (bIgnoreStuckCheck) {
                bCollisionDisabled = true;
            } else if (GetIsStuck() || colPhysical->GetIsStuck()) {
                bThisOrCollidedEntityStuck = true;
            }
        }
    }

    if (!bCollidedEntityCollisionIgnored && (bIgnoreStuckCheck || GetIsStuck())) {
        bThisOrCollidedEntityStuck = true;
    }
}

// 0x5A02E0 (reduced: colmodel/objectdata branches need unported subsystems)
uint8_t CObject::SpecialEntityCalcCollisionSteps(bool& bProcessCollisionBeforeSettingTimeStep, bool& unk2) {
    (void)bProcessCollisionBeforeSettingTimeStep;
    (void)unk2;
    // TODO(port collision): full step-count logic needs colmodel bound radius /
    // bounding box and CObjectData special-response cases.
    if (!physicalFlags.bDisableMoveForce) {
        if (IsTemporary() && !objectFlags.bIsLiftable) {
            return 1;
        }
        const float fMove = m_vecMoveSpeed.SquaredMagnitude() * CTimer::GetTimeStep() * CTimer::GetTimeStep();
        if (fMove >= 0.09F) {
            return static_cast<uint8_t>(std::ceil(std::sqrt(fMove) / 0.3F));
        }
        return 1;
    }
    const float fMove = std::fabs(m_vecTurnSpeed.z); // TODO: * longest bbox edge
    if (fMove > 0.1F) {
        return static_cast<uint8_t>(std::ceil(fMove * 10.0F));
    }
    return 1;
}

// 0x59FD50
void CObject::PreRender() {
    // TODO(port lighting): bAffectedByColBrightness -> m_fDynamicLighting.
    CEntity::PreRender();
}

// 0x59F180
void CObject::Render() {
    CEntity::Render();
}

// 0x554FA0
bool CObject::SetupLighting() {
    // TODO(port lighting): object-specific lighting setup.
    return CEntity::SetupLighting();
}

// 0x553E10
void CObject::RemoveLighting(bool bRemove) {
    CEntity::RemoveLighting(bRemove);
}

// ---------------------------------------------------------------------------
// Save / load (TODO: save system)
// ---------------------------------------------------------------------------

// 0x5D2870
bool CObject::Load() {
    return false;
}

// 0x5D2830
bool CObject::Save() {
    return false;
}

// ---------------------------------------------------------------------------
// Behaviours (TODO: garages/samsite/train crossing need unported subsystems)
// ---------------------------------------------------------------------------

// 0x44A4D0
void CObject::ProcessGarageDoorBehaviour() {
}

// 0x5A07D0
void CObject::ProcessSamSiteBehaviour() {
}

// 0x5A0B50
void CObject::ProcessTrainCrossingBehaviour() {
}

// ---------------------------------------------------------------------------
// Queries / simple setters
// ---------------------------------------------------------------------------

// 0x59F120
bool CObject::CanBeDeleted() const {
    switch (m_nObjectType) {
    case eObjectType::OBJECT_MISSION:
    case eObjectType::OBJECT_TYPE_CUTSCENE:
    case eObjectType::OBJECT_TYPE_DECORATION:
    case eObjectType::OBJECT_MISSION2:
        return false;
    default:
        return true;
    }
}

// 0x59F160
void CObject::SetRelatedDummy(CDummyObject* relatedDummy) {
    assert(relatedDummy);
    m_pDummyObject = relatedDummy;
    // TODO(port dummy): m_pDummyObject->RegisterReference(reinterpret_cast<CEntity**>(&m_pDummyObject)).
}

// 0x59F2D0
bool CObject::TryToExplode() {
    // TODO(port objectdata): m_pObjectInfo->m_bCausesExplosion.
    if (objectFlags.bIsExploded) {
        return false;
    }
    objectFlags.bIsExploded = true;
    Explode();
    return true;
}

// 0x59F300
void CObject::SetObjectTargettable(bool targetable) {
    objectFlags.bIsTargetable = targetable;
}

// 0x59F320
bool CObject::CanBeTargetted() const {
    return objectFlags.bIsTargetable;
}

// 0x59F330
void CObject::RefModelInfo(int32_t modelIndex) {
    m_nRefModelIndex = static_cast<int16_t>(modelIndex);
    // TODO(port modelinfo): CModelInfo::GetModelInfo(modelIndex)->AddRef().
}

// 0x59F350
void CObject::SetRemapTexture(RwTexture* remapTexture, int16_t txdIndex) {
    m_pRemapTexture = remapTexture;
    m_wRemapTxd = txdIndex;
    // TODO(port txd): CTxdStore::AddRef(txdIndex) when txdIndex != -1.
}

// Rope helpers (TODO: CRopes not ported).
float CObject::GetRopeHeight() { return 0.0f; }
void CObject::SetRopeHeight(float height) { (void)height; }
CEntity* CObject::GetObjectCarriedWithRope() { return nullptr; }
void CObject::ReleaseObjectCarriedWithRope() {}
void CObject::GrabObjectToCarryWithRope(CPhysical* attachTo) { (void)attachTo; }

// 0x59F400
void CObject::AddToControlCodeList() {
    auto* node = new CPtrNodeDoubleLink<CObject*>(this);
    node->m_next = s_pControlCodeListHead;
    if (s_pControlCodeListHead) {
        s_pControlCodeListHead->m_prev = node;
    }
    s_pControlCodeListHead = node;
    m_pControlCodeList = node;
}

// 0x59F450
void CObject::RemoveFromControlCodeList() {
    auto* node = m_pControlCodeList;
    if (!node) {
        return;
    }
    if (node->m_prev) {
        node->m_prev->m_next = node->m_next;
    } else {
        s_pControlCodeListHead = node->m_next;
    }
    if (node->m_next) {
        node->m_next->m_prev = node->m_prev;
    }
    delete node;
    m_pControlCodeList = nullptr;
}

// 0x59F4B0
void CObject::ResetDoorAngle() {
    if (!physicalFlags.bDisableMoveForce || m_fDoorStartAngle <= -1000.0F) {
        return;
    }

    CPlaceable::SetHeading(m_fDoorStartAngle);
    SetIsStatic(true);
    ResetMoveSpeed();
    ResetTurnSpeed();
    ResetFrictionMoveSpeed();
    ResetFrictionTurnSpeed();
    CEntity::UpdateRwMatrix();
    CEntity::UpdateRwFrame();
}

// 0x59F5C0
void CObject::LockDoor() {
    objectFlags.bIsDoorOpen = false;
    physicalFlags.bCollidable = true;
    physicalFlags.bDisableCollisionForce = true;
    ResetDoorAngle();
}

// 0x59F840 (reduced: model-data branches need CObjectData/CModelInfo/CGarages)
void CObject::Init() {
    SetTypeObject();
    // TODO(port objectdata): m_pObjectInfo = &CObjectData::GetDefault();
    m_pObjectInfo = nullptr;
    m_nColDamageEffect = COL_DAMAGE_EFFECT_NONE;
    m_nSpecialColResponseCase = 0; // COL_SPECIAL_RESPONSE_NONE
    m_nObjectType = eObjectType::OBJECT_GAME;
    SetIsStatic(true);

    m_nObjectFlags &= 0xFC000000 | 0x40000;

    m_fHealth = 1000.0F;
    m_fDoorStartAngle = -1001.0F;
    m_nRemovalTime = 0;
    m_nBonusValue = 0;
    m_wCostValue = 0;
    for (auto& c : m_nCarColor) {
        c = 0;
    }

    m_nRefModelIndex = -1;
    m_nGarageDoorGarageIndex = -1;
    m_nLastWeaponDamage = static_cast<uint8_t>(-1);
    m_pFire = nullptr;

    // TODO(port modelinfo): lamp-post detection, buoy water flag, weapon-model
    // light flag, garage-door control-code registration.
    objectFlags.bIsLampPost = false;
    objectFlags.bIsTargetable = false;
    physicalFlags.bAttachedToEntity = false;

    SetAreaCode(AREA_CODE_13);
    m_wRemapTxd = -1;
    m_pRemapTexture = nullptr;
    m_pControlCodeList = nullptr;

    m_nBurnTime = 0;
    m_fScale = 1.0F;

    m_nColLighting.day = 0x8;
    m_nColLighting.night = 0x4;
    m_nStreamedScriptBrainToLoad = -1;
}

// 0x59FB50
void CObject::DoBurnEffect() const {
    // TODO(port effects): needs colmodel bounding box + particle system.
}

// 0x59FD00
void CObject::GetLightingFromCollisionBelow() {
    // TODO(port collision): needs CColPoint lighting.
}

// ---------------------------------------------------------------------------
// Damage (TODO: full port needs weapons/explosion/particle subsystems)
// ---------------------------------------------------------------------------

// 0x5A0D90
void CObject::ObjectDamage(float damage, const CVector* fxOrigin, const CVector* fxDirection, CEntity* damager, eWeaponType weaponType) {
    (void)damage;
    (void)fxOrigin;
    (void)fxDirection;
    (void)damager;
    (void)weaponType;
}

// 0x5A1340
void CObject::Explode() {
    // TODO(port explosion): CExplosion::AddExplosion with object position.
}

// 0x5A1580
void CObject::ObjectFireDamage(float damage, CEntity* damager) {
    (void)damage;
    (void)damager;
}

// ---------------------------------------------------------------------------
// Statics
// ---------------------------------------------------------------------------

// 0x5A1F60
CObject* CObject::Create(int32_t modelIndex, bool bUnused) {
    // TODO(port pool): GetObjectPool()->SetDealWithNoMemory(true) + temp-object
    // eviction (TryToFreeUpTempObjects) when allocation fails.
    (void)bUnused;
    return new CObject(modelIndex, false);
}

// 0x5A2070
CObject* CObject::Create(CDummyObject* dummyObject) {
    return new CObject(dummyObject);
}

// 0x59F200 (reduced: Rotate() needs full CMatrix port)
void CObject::SetMatrixForTrainCrossing(CMatrix* matrix, float fAngle) {
    if (!matrix) {
        return;
    }
    // TODO(port cmatrix): cross-product based forward/up rebuild.
    (void)fAngle;
}

// 0x5A1840
void CObject::TryToFreeUpTempObjects(int32_t numObjects) {
    // TODO(port pool): evict temp objects from the object pool.
    (void)numObjects;
}

// 0x5A18B0
void CObject::DeleteAllTempObjects() {
    // TODO(port pool).
}

// 0x5A1910
void CObject::DeleteAllMissionObjects() {
    // TODO(port pool).
}

// 0x5A1980
void CObject::DeleteAllTempObjectsInArea(CVector point, float radius) {
    // TODO(port pool): CWorld::FindObjectsInRange + delete temps.
    (void)point;
    (void)radius;
}

// 0x5A1B60 (reduced: height check needs colmodel)
bool CObject::CanBeUsedToTakeCoverBehind() {
    if (m_nObjectType == eObjectType::OBJECT_MISSION) {
        return false;
    }
    // TODO(port modelinfo): fire-hydrant / breakable-statue / bbox-height checks.
    return false;
}

// 0x5A29A0 (reduced: ropes/garages need unported subsystems)
void CObject::ProcessControlLogic() {
    // TODO(port): SAM site / train crossing / crane rope / garage-door dispatch
    // by model index (needs eModelID + CRopes + CGarages).
}

// NOTSA (reduced: needs eModelID crane model ids)
bool CObject::IsCraneMovingPart() const {
    return false;
}

// 0x5A2B90
bool IsObjectPointerValid_NotInWorld(CObject* object) {
    // TODO(port pool): GetObjectPool()->IsObjectValid(object).
    return object != nullptr;
}

// 0x5A2C20
bool IsObjectPointerValid(CObject* object) {
    // TODO(port pool): pool validity + in-world check via m_pCollisionList.
    return object != nullptr;
}
