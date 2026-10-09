// CPhysical.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CPhysical (physics state on top of CEntity).
// Adapted from gta-reversed/source/game_sa/Entity/Physical.cpp and verified
// against the decomp (src/CPhysical/*.c). Collision-model/weapon/timecycle
// dependent bodies are marked TODO(port) until those subsystems land.
//
// vtable: overrides Add, Remove, GetBoundRect, ProcessControl,
// ProcessCollision, ProcessShift, TestCollision, plus virtual
// ProcessEntityCollision. All are defined here (base no-ops stay inline in
// CEntity.h).
//
// Clean-room notes:
// - CrossProduct() is a file-local helper (TODO: move to CVector core math).
// - The moving-entity list and CEntryInfoNode are file-local mirrors
//   (TODO: unify with CWorld state when the world subsystem is finalized).

#include "CPhysical.h"
#include "CWorld.h"
#include "CTimer.h"

#include <cassert>
#include <cmath>
#include <cstddef>

// ---------------------------------------------------------------------------
// Local clean-room mirrors (TODO: replace with ported core headers)
// ---------------------------------------------------------------------------

// File-local 3D cross product (TODO: CVector::Cross / free CrossProduct).
static CVector CrossProduct(const CVector& a, const CVector& b) {
    return CVector{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// Minimal CEntryInfoNode: the original carries sector/double-link bookkeeping
// for the physical-vs-sector collision lists. The clean-room build routes
// physicals through CEntity::Add()/Remove() instead, so only list hygiene
// (Flush/DeleteNode) is implemented here.
class CEntryInfoNode {
public:
    CEntryInfoNode* m_next{ nullptr };
};

// Double-link node mirror (gta-reversed CPtrNodeDoubleLink).
template<typename T>
class CPtrNodeDoubleLink {
public:
    T m_item;
    CPtrNodeDoubleLink* m_prev{ nullptr };
    CPtrNodeDoubleLink* m_next{ nullptr };

    explicit CPtrNodeDoubleLink(T item) : m_item(item) {}
};

// World moving-entity list (mirrors CWorld::ms_listMovingEntityPtrs).
// TODO: move into CWorld.cpp ownership when CWorld state is finalized.
static CPtrNodeDoubleLink<CPhysical*>* s_pMovingListHead = nullptr;

static void MovingList_Add(CPhysical* physical, CPtrNodeDoubleLink<CPhysical*>*& outLink) {
    auto* node = new CPtrNodeDoubleLink<CPhysical*>(physical);
    node->m_next = s_pMovingListHead;
    if (s_pMovingListHead) {
        s_pMovingListHead->m_prev = node;
    }
    s_pMovingListHead = node;
    outLink = node;
}

static void MovingList_Remove(CPtrNodeDoubleLink<CPhysical*>* link) {
    if (!link) {
        return;
    }
    if (link->m_prev) {
        link->m_prev->m_next = link->m_next;
    } else {
        s_pMovingListHead = link->m_next;
    }
    if (link->m_next) {
        link->m_next->m_prev = link->m_prev;
    }
    delete link;
}

// ---------------------------------------------------------------------------
// CEntryInfoList (declared in CPhysical.h)
// ---------------------------------------------------------------------------

void CEntryInfoList::Flush() {
    for (CEntryInfoNode* node = m_node; node;) {
        CEntryInfoNode* const next = node->m_next;
        delete node;
        node = next;
    }
    m_node = nullptr;
}

void CEntryInfoList::DeleteNode(CEntryInfoNode* node) {
    if (!node) {
        return;
    }
    for (CEntryInfoNode** pp = &m_node; *pp; pp = &(*pp)->m_next) {
        if (*pp == node) {
            *pp = node->m_next;
            delete node;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// ctor / dtor
// ---------------------------------------------------------------------------

// 0x542260
CPhysical::CPhysical() : CEntity() {
    m_pCollisionList.m_node = nullptr;

    // CPlaceable::AllocateStaticMatrix() is still a stub; guard the matrix.
    // TODO(port placeable): restore AllocateStaticMatrix + SetUnity.
    m_vecMoveSpeed = CVector{};
    m_vecTurnSpeed = CVector{};
    m_vecFrictionMoveSpeed = CVector{};
    m_vecFrictionTurnSpeed = CVector{};
    m_vecForce = CVector{};
    m_vecTorque = CVector{};

    m_fMass = 1.0f;
    m_fTurnMass = 1.0f;
    m_fVelocityFrequency = 1.0f;
    m_fAirResistance = 0.1f;
    m_pMovingList = nullptr;
    m_nFakePhysics = 0;
    m_nNumEntitiesCollided = 0;
    for (auto& e : m_apCollidedEntities) {
        e = nullptr;
    }

    m_nPieceType = 0;

    m_fDamageIntensity = 0.0f;
    m_pDamageEntity = nullptr;

    m_vecLastCollisionImpactVelocity = CVector{};
    m_vecLastCollisionPosn = CVector{};

    SetUsesCollision(true);

    m_vecCentreOfMass = CVector{};

    m_fMovingSpeed = 0.0f;
    m_pAttachedTo = nullptr;
    m_pEntityIgnoredCollision = nullptr;

    m_qAttachedEntityRotation = CQuaternion{};

    m_fDynamicLighting = 0.0f;
    m_pShadowData = nullptr;
    field_38 = 100.0f;

    m_nPhysicalFlags = 0;
    physicalFlags.bApplyGravity = true;

    m_nContactSurface = SURFACE_DEFAULT;
    m_fContactSurfaceBrightness = 1.0f;
}

// 0x542450
CPhysical::~CPhysical() {
    // TODO(port shadows): g_realTimeShadowMan.ReturnRealTimeShadow(m_pShadowData).
    m_pShadowData = nullptr;
    m_pCollisionList.Flush();
}

// ---------------------------------------------------------------------------
// World add / remove
// ---------------------------------------------------------------------------

// 0x544A30
void CPhysical::Add() {
    // Clean-room: physicals use the CEntity sector add (the original's
    // CEntryInfoNode sector lists are not ported yet).
    CEntity::Add();
}

// 0x5424C0
void CPhysical::Remove() {
    if (m_bIsBIGBuilding) {
        CEntity::Remove();
        return;
    }
    // Clean-room: no entry-info nodes; plain sector remove.
    CEntity::Remove();
}

// 0x5449B0
CRect CPhysical::GetBoundRect() const {
    // TODO(port collision): radius from GetColModel()->GetBoundRadius().
    const CVector boundCentre = CEntity::GetBoundCentre();
    constexpr float kFallbackRadius = 1.0F;
    return CRect{
        boundCentre.x - kFallbackRadius,
        boundCentre.y - kFallbackRadius,
        boundCentre.x + kFallbackRadius,
        boundCentre.y + kFallbackRadius
    };
}

void CPhysical::RemoveAndAdd() {
    CWorld::Remove(this);
    CWorld::Add(this);
}

// 0x542800
void CPhysical::AddToMovingList() {
    if (!m_pMovingList && !m_bIsStaticWaitingForCollision) {
        MovingList_Add(this, m_pMovingList);
    }
}

// 0x542860
void CPhysical::RemoveFromMovingList() {
    if (m_pMovingList) {
        MovingList_Remove(m_pMovingList);
        m_pMovingList = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Control / collision processing
// ---------------------------------------------------------------------------

// 0x5485E0
void CPhysical::ProcessControl() {
    if (!GetIsTypePed()) {
        physicalFlags.bSubmergedInWater = false;
    }

    SetHasHitWall(false);
    SetWasPostponed(false);
    SetIsInSafePosition(false);
    SetHasContacted(false);

    if (GetStatus() != STATUS_SIMPLE) {
        physicalFlags.bDoorHitEndStop = false;
        physicalFlags.bOnSolidSurface = false;
        m_nNumEntitiesCollided = 0;
        m_nPieceType = 0;
        m_fDamageIntensity = 0.0f;
        CEntity::SafeCleanUpRef(m_pDamageEntity);
        m_pDamageEntity = nullptr;
        ApplyFriction();
        if (!m_pAttachedTo || physicalFlags.bInfiniteMass) {
            ApplyGravity();
            ApplyAirResistance();
        }
    }
}

// 0x54DFB0
void CPhysical::ProcessCollision() {
    m_fMovingSpeed = 0.0f;
    physicalFlags.bProcessingShift = false;
    physicalFlags.bSkipLineCol = false;
    // TODO(port collision): full port needs CCollision + vehicle/bike paths.
    // See decomp src/CPhysical/ProcessCollision_0054dfb0.c and
    // gta-reversed/source/game_sa/Entity/Physical.cpp.
    if (!GetUsesCollision() || physicalFlags.bDisableSimpleCollision) {
        return;
    }
    if (GetStatus() == STATUS_SIMPLE) {
        if (CheckCollision_SimpleCar() && GetStatus() == STATUS_SIMPLE) {
            SetStatus(STATUS_PHYSICS);
        }
        SetIsStuck(false);
        SetIsInSafePosition(true);
        RemoveAndAdd();
    }
}

// 0x54DB10
void CPhysical::ProcessShift() {
    // TODO(port collision): shift processing needs ProcessShiftSectorList +
    // CCollision. See decomp src/CPhysical/ProcessShift_0054db10.c.
}

// 0x54DEC0
bool CPhysical::TestCollision(bool bApplySpeed) {
    if (!GetUsesCollision()) {
        return false;
    }
    // TODO(port collision): sector-list collision test + ApplySpeed.
    // See decomp src/CPhysical/TestCollision_0054dec0.c.
    (void)bApplySpeed;
    return false;
}

// 0x546D00
int32_t CPhysical::ProcessEntityCollision(CEntity* entity, CColPoint* colPoint) {
    // TODO(port collision): CCollision::ProcessColModels over both colmodels.
    // Original also records collisions and sets HasHitWall for static hits.
    (void)entity;
    (void)colPoint;
    return 0;
}

// ---------------------------------------------------------------------------
// Forces
// ---------------------------------------------------------------------------

// 0x5428C0
void CPhysical::SetDamagedPieceRecord(float fDamageIntensity, CEntity* entity, const CColPoint& colPoint, float fDistanceMult) {
    if (fDamageIntensity > m_fDamageIntensity) {
        m_fDamageIntensity = fDamageIntensity;
        // TODO(port collision): m_nPieceType = colPoint.m_nPieceTypeA;
        CEntity::ChangeEntityReference(m_pDamageEntity, entity);
        // TODO(port collision):
        //   m_vecLastCollisionPosn = colPoint.m_vecPoint;
        //   m_vecLastCollisionImpactVelocity = fDistanceMult * colPoint.m_vecNormal;
        //   ... bDamaged flag updates for car moving components.
        (void)colPoint;
        (void)fDistanceMult;
    }
}

// 0x4ABBA0
void CPhysical::ApplyMoveForce(float x, float y, float z) {
    ApplyMoveForce(CVector{ x, y, z });
}

// 0x5429F0
void CPhysical::ApplyMoveForce(CVector force) {
    if (!physicalFlags.bInfiniteMass && !physicalFlags.bDisableMoveForce) {
        if (physicalFlags.bDisableZ) {
            force.z = 0.0f;
        }
        m_vecMoveSpeed += force / m_fMass;
    }
}

// 0x542A50
void CPhysical::ApplyTurnForce(CVector force, CVector point) {
    if (physicalFlags.bDisableTurnForce) {
        return;
    }

    if (physicalFlags.bDisableMoveForce) {
        point.z = 0.0f;
        force.z = 0.0f;
    }

    // Adjust point to be relative to the centre-of-mass.
    if (!physicalFlags.bInfiniteMass && m_matrix) {
        point -= m_matrix->TransformVector(m_vecCentreOfMass);
    }

    m_vecTurnSpeed += CrossProduct(point, force) / m_fTurnMass;
}

// 0x542B50
void CPhysical::ApplyForce(CVector vecForce, CVector point, bool bUpdateTurnSpeed) {
    CVector vecMoveSpeedForce = vecForce;
    if (physicalFlags.bDisableZ) {
        vecMoveSpeedForce.z = 0.0f;
    }

    if (!physicalFlags.bInfiniteMass && !physicalFlags.bDisableMoveForce) {
        m_vecMoveSpeed += vecMoveSpeedForce / m_fMass;
    }

    if (!physicalFlags.bDisableTurnForce && bUpdateTurnSpeed) {
        CVector vecCentreOfMassMultiplied{};
        float fTurnMass = m_fTurnMass;
        if (physicalFlags.bInfiniteMass) {
            fTurnMass += m_vecCentreOfMass.z * m_fMass * m_vecCentreOfMass.z * 0.5f;
        } else if (m_matrix) {
            vecCentreOfMassMultiplied = m_matrix->TransformVector(m_vecCentreOfMass);
        }

        if (physicalFlags.bDisableMoveForce) {
            point.z = 0.0f;
            vecForce.z = 0.0f;
        }

        const CVector distance = point - vecCentreOfMassMultiplied;
        m_vecTurnSpeed += CrossProduct(distance, vecForce) / fTurnMass;
    }
}

// 0x542CE0
CVector CPhysical::GetSpeed(CVector point) {
    CVector vecCentreOfMassMultiplied{};
    if (!physicalFlags.bInfiniteMass && m_matrix) {
        vecCentreOfMassMultiplied = m_matrix->TransformVector(m_vecCentreOfMass);
    }

    const CVector distance = point - vecCentreOfMassMultiplied;
    const CVector vecTurnSpeed = m_vecTurnSpeed + m_vecFrictionTurnSpeed;
    CVector speed = CrossProduct(vecTurnSpeed, distance);
    speed += m_vecMoveSpeed + m_vecFrictionMoveSpeed;
    return speed;
}

// 0x542DD0
void CPhysical::ApplyMoveSpeed() {
    if (physicalFlags.bDontApplySpeed || physicalFlags.bDisableMoveForce) {
        ResetMoveSpeed();
    } else {
        GetPosition() += CTimer::GetTimeStep() * m_vecMoveSpeed;
    }
}

// 0x542E20
void CPhysical::ApplyTurnSpeed() {
    if (physicalFlags.bDontApplySpeed) {
        ResetTurnSpeed();
        return;
    }

    if (!m_matrix) {
        return; // TODO(port placeable): matrix is stubbed until AllocateMatrix lands.
    }

    const CVector vecTurnSpeedTimeStep = CTimer::GetTimeStep() * m_vecTurnSpeed;
    GetRight() += CrossProduct(vecTurnSpeedTimeStep, GetRight());
    GetForward() += CrossProduct(vecTurnSpeedTimeStep, GetForward());
    GetUp() += CrossProduct(vecTurnSpeedTimeStep, GetUp());
    if (!physicalFlags.bInfiniteMass && !physicalFlags.bDisableMoveForce) {
        const CVector vecNegativeCentreOfMass = m_vecCentreOfMass * -1.0f;
        const CVector vecCentreOfMassMultiplied = m_matrix->TransformVector(vecNegativeCentreOfMass);
        GetPosition() += CrossProduct(vecTurnSpeedTimeStep, vecCentreOfMassMultiplied);
    }
}

// 0x542FE0
void CPhysical::ApplyGravity() {
    if (physicalFlags.bApplyGravity && !physicalFlags.bDisableMoveForce) {
        if (physicalFlags.bInfiniteMass) {
            if (!m_matrix) {
                return;
            }
            const float fMassTimeStep = CTimer::GetTimeStep() * m_fMass;
            const CVector point = m_matrix->TransformVector(m_vecCentreOfMass);
            const CVector force{ 0.0f, 0.0f, fMassTimeStep * -0.008f };
            ApplyForce(force, point, true);
        } else if (GetUsesCollision()) {
            m_vecMoveSpeed.z -= CTimer::GetTimeStep() * 0.008f;
        }
    }
}

// 0x5430A0
void CPhysical::ApplyFrictionMoveForce(CVector moveForce) {
    if (!physicalFlags.bInfiniteMass && !physicalFlags.bDisableMoveForce) {
        if (physicalFlags.bDisableZ) {
            moveForce.z = 0.0f;
        }
        m_vecFrictionMoveSpeed += moveForce / m_fMass;
    }
}

// 0x543100 (unused in the original)
void CPhysical::ApplyFrictionTurnForce(CVector posn, CVector velocity) {
    (void)posn;
    (void)velocity;
}

// 0x543220
void CPhysical::ApplyFrictionForce(CVector vecMoveForce, CVector point) {
    CVector vecTheMoveForce = vecMoveForce;

    if (physicalFlags.bDisableZ) {
        vecTheMoveForce.z = 0.0f;
    }

    if (!physicalFlags.bInfiniteMass && !physicalFlags.bDisableMoveForce) {
        m_vecFrictionMoveSpeed += vecTheMoveForce / m_fMass;
    }

    if (!physicalFlags.bDisableTurnForce) {
        float fTurnMass = m_fTurnMass;
        CVector vecCentreOfMassMultiplied{};
        if (physicalFlags.bInfiniteMass) {
            fTurnMass += m_vecCentreOfMass.z * m_fMass * m_vecCentreOfMass.z * 0.5f;
        } else if (m_matrix) {
            vecCentreOfMassMultiplied = m_matrix->TransformVector(m_vecCentreOfMass);
        }

        if (physicalFlags.bDisableMoveForce) {
            point.z = 0.0f;
            vecTheMoveForce.z = 0.0f;
        }

        const CVector distance = point - vecCentreOfMassMultiplied;
        m_vecFrictionTurnSpeed += CrossProduct(distance, vecTheMoveForce) / fTurnMass;
    }
}

// 0x5433B0
void CPhysical::SkipPhysics() {
    if (!GetIsTypePed() && !GetIsTypeVehicle()) {
        physicalFlags.bSubmergedInWater = false;
    }

    SetHasHitWall(false);
    SetWasPostponed(false);
    SetIsInSafePosition(false);
    SetHasContacted(false);

    if (GetStatus() != STATUS_SIMPLE) {
        physicalFlags.bOnSolidSurface = false;
        m_nNumEntitiesCollided = 0;
        m_nPieceType = 0;
        m_fDamageIntensity = 0.0f;
        CEntity::ClearReference(m_pDamageEntity);
        ResetFrictionTurnSpeed();
        ResetFrictionMoveSpeed();
    }
}

// 0x544C40
void CPhysical::ApplyAirResistance() {
    if (m_fAirResistance <= 0.1f) {
        // TODO(port cullzones/vehicle): extra air resistance for player vehicles.
        const float fSpeedMagnitude = m_vecMoveSpeed.Magnitude() * m_fAirResistance;
        m_vecMoveSpeed *= std::pow(1.0f - fSpeedMagnitude, CTimer::GetTimeStep());
        m_vecTurnSpeed *= 0.99f;
    } else {
        const float fAirResistanceTimeStep = std::pow(m_fAirResistance, CTimer::GetTimeStep());
        m_vecMoveSpeed *= fAirResistanceTimeStep;
        m_vecTurnSpeed *= fAirResistanceTimeStep;
    }
}

// 0x5483D0
void CPhysical::ApplyFriction() {
    // TODO(port collision): the bDisableZ branch builds a ground CColPoint
    // from the colmodel bound radius and calls ApplyFriction(friction, colPoint).
    m_vecMoveSpeed += m_vecFrictionMoveSpeed;
    m_vecTurnSpeed += m_vecFrictionTurnSpeed;
    ResetFrictionMoveSpeed();
    ResetFrictionTurnSpeed();
    // TODO(port vehicle): tipped-bike damping for abandoned bikes.
}

// 0x547B80
void CPhysical::ApplySpeed() {
    // TODO(port collision): full port applies move/turn speed with collision.
    // See decomp src/CPhysical/ApplySpeed_00547b80.c.
    ApplyMoveSpeed();
    ApplyTurnSpeed();
}

// ---------------------------------------------------------------------------
// Collision records
// ---------------------------------------------------------------------------

// 0x543490
void CPhysical::AddCollisionRecord(CEntity* collidedEntity) {
    physicalFlags.bOnSolidSurface = true;
    m_nLastCollisionTime = CTimer::GetTimeInMS();
    // TODO(port vehicle): alarm-state wake-up for vehicle-vs-vehicle hits.

    if (physicalFlags.bCanBeCollidedWith) {
        for (uint8_t i = 0; i < m_nNumEntitiesCollided; i++) {
            if (m_apCollidedEntities[i] == collidedEntity) {
                return;
            }
        }

        if (m_nNumEntitiesCollided < 6) {
            m_apCollidedEntities[m_nNumEntitiesCollided] = collidedEntity;
            m_nNumEntitiesCollided++;
        }
    }
}

// 0x543540
bool CPhysical::GetHasCollidedWith(CEntity* entity) {
    if (!physicalFlags.bCanBeCollidedWith || m_nNumEntitiesCollided <= 0) {
        return false;
    }

    for (uint8_t i = 0; i < m_nNumEntitiesCollided; i++) {
        if (m_apCollidedEntities[i] == entity) {
            return true;
        }
    }
    return false;
}

// 0x543580
bool CPhysical::GetHasCollidedWithAnyObject() {
    if (!physicalFlags.bCanBeCollidedWith || m_nNumEntitiesCollided <= 0) {
        return false;
    }

    for (uint8_t i = 0; i < m_nNumEntitiesCollided; i++) {
        CEntity* const pCollidedEntity = m_apCollidedEntities[i];
        if (pCollidedEntity && pCollidedEntity->GetIsTypeObject()) {
            return true;
        }
    }
    return false;
}

// 0x544280
void CPhysical::RemoveRefsToEntity(CEntity* entity) {
    for (uint8_t i = 0; i < m_nNumEntitiesCollided;) {
        if (m_apCollidedEntities[i] == entity) {
            for (uint8_t j = i; j + 1 < m_nNumEntitiesCollided; ++j) {
                m_apCollidedEntities[j] = m_apCollidedEntities[j + 1];
            }
            --m_nNumEntitiesCollided;
        } else {
            ++i;
        }
    }
}

// ---------------------------------------------------------------------------
// Collision response (TODO: full port needs CColPoint internals + CCollision)
// ---------------------------------------------------------------------------

// 0x5435C0
bool CPhysical::ApplyCollision(CEntity* entity, const CColPoint& colPoint, float& damageIntensity) {
    (void)entity;
    (void)colPoint;
    (void)damageIntensity;
    return false;
}

// 0x543890
bool CPhysical::ApplySoftCollision(CEntity* entity, const CColPoint& colPoint, float& outDamageIntensity) {
    (void)entity;
    (void)colPoint;
    (void)outDamageIntensity;
    return false;
}

// 0x543C90
bool CPhysical::ApplySpringCollision(float fSuspensionForceLevel, CVector& direction, CVector& collisionPoint, float fSpringLength, float fSuspensionBias, float& fSpringForceDampingLimit) {
    (void)fSuspensionForceLevel;
    (void)direction;
    (void)collisionPoint;
    (void)fSpringLength;
    (void)fSuspensionBias;
    (void)fSpringForceDampingLimit;
    return false;
}

// 0x543D60
bool CPhysical::ApplySpringCollisionAlt(float fSuspensionForceLevel, CVector& direction, CVector& collisionPoint, float fSpringLength, float fSuspensionBias, CVector& normal, float& fSpringForceDampingLimit) {
    (void)fSuspensionForceLevel;
    (void)direction;
    (void)collisionPoint;
    (void)fSpringLength;
    (void)fSuspensionBias;
    (void)normal;
    (void)fSpringForceDampingLimit;
    return false;
}

// 0x543E90
bool CPhysical::ApplySpringDampening(float fDampingForce, float fSpringForceDampingLimit, CVector& direction, CVector& collisionPoint, CVector& collisionPos) {
    (void)fDampingForce;
    (void)fSpringForceDampingLimit;
    (void)direction;
    (void)collisionPoint;
    (void)collisionPos;
    return false;
}

// 0x544100
bool CPhysical::ApplySpringDampeningOld(float arg0, float arg1, CVector& arg2, CVector& arg3, CVector& arg4) {
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    (void)arg4;
    return false;
}

// 0x544D50
bool CPhysical::ApplyCollisionAlt(CPhysical* entity, CColPoint& colPoint, float& damageIntensity, CVector& outVecMoveSpeed, CVector& outVecTurnSpeed) {
    (void)entity;
    (void)colPoint;
    (void)damageIntensity;
    (void)outVecMoveSpeed;
    (void)outVecTurnSpeed;
    return false;
}

// 0x5454C0 / 0x545980
bool CPhysical::ApplyFriction(float fFriction, CColPoint& colPoint) {
    (void)fFriction;
    (void)colPoint;
    return false;
}

// 0x5483D0 (overload)
bool CPhysical::ApplyFriction(CPhysical* entity, float fFriction, CColPoint& colPoint) {
    (void)entity;
    (void)fFriction;
    (void)colPoint;
    return false;
}

// 0x548680
bool CPhysical::ApplyCollision(CEntity* theEntity, CColPoint& colPoint, float& thisDamageIntensity, float& entityDamageIntensity) {
    (void)theEntity;
    (void)colPoint;
    (void)thisDamageIntensity;
    (void)entityDamageIntensity;
    return false;
}

// 0x54A2C0
bool CPhysical::ApplySoftCollision(CPhysical* physical, CColPoint& colPoint, float& thisDamageIntensity, float& entityDamageIntensity) {
    (void)physical;
    (void)colPoint;
    (void)thisDamageIntensity;
    (void)entityDamageIntensity;
    return false;
}

// 0x546ED0 (unused in the original)
float CPhysical::ApplyScriptCollision(CVector arg0, float arg1, float arg2, CVector* arg3) {
    (void)arg0;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Attach / detach
// ---------------------------------------------------------------------------

// 0x5442F0
void CPhysical::DettachEntityFromEntity(float x, float y, float z, bool bApplyTurnForce) {
    // TODO(port collision): the vehicle-vs-vehicle branch runs
    // CCollision::ProcessColModels to decide m_pEntityIgnoredCollision.
    m_pEntityIgnoredCollision = m_pAttachedTo;

    CWorld::Remove(this);
    SetIsStatic(false);
    physicalFlags.bAttachedToEntity = false;
    CWorld::Add(this);

    if (physicalFlags.bDisableCollisionForce) {
        // TODO(port objectdata): restore mass/turnmass from CObjectData.
        physicalFlags.bCollidable = true;
        ResetTurnSpeed();
        ResetMoveSpeed();
        bApplyTurnForce = false;
    }

    if (!physicalFlags.bDisableCollisionForce && m_pAttachedTo && m_pAttachedTo->GetIsTypePhysical()) {
        m_vecMoveSpeed = m_pAttachedTo->m_vecMoveSpeed;
        // TODO: vecForce from the detach-offset matrix (needs full CMatrix ops).
        (void)x;
        (void)y;
        (void)z;
    }

    if (bApplyTurnForce) {
        ApplyTurnForce(CVector{ 0.0f, 0.0f, z }, CVector{ 0.0f, 0.0f, z * 0.5f });
    }

    m_pAttachedTo = nullptr;
    m_qAttachedEntityRotation = CQuaternion{};
    m_vecAttachOffset = CVector(0.0f, 0.0f, 0.0f);
}

// 0x5446A0
void CPhysical::DettachAutoAttachedEntity() {
    SetIsStatic(false);
    physicalFlags.bAttachedToEntity = false;
    m_nFakePhysics = 0;
    if (!physicalFlags.bDisableCollisionForce) {
        if (m_pAttachedTo && m_pAttachedTo->GetIsTypeVehicle()) {
            m_vecMoveSpeed = m_pAttachedTo->m_vecMoveSpeed;
            m_vecTurnSpeed = m_pAttachedTo->m_vecTurnSpeed;
        }
    } else {
        physicalFlags.bCollidable = true;
        ResetTurnSpeed();
        ResetMoveSpeed();
    }
    m_vecAttachOffset = CVector(0.0f, 0.0f, 0.0f);
    m_pEntityIgnoredCollision = nullptr;
    m_pAttachedTo = nullptr;
    m_qAttachedEntityRotation = CQuaternion{};
    // TODO(port objectdata): m_fElasticity = AsObject()->m_pObjectInfo->m_fElasticity for objects.
}

// 0x54D570
void CPhysical::AttachEntityToEntity(CPhysical* entityAttachTo, CVector vecAttachOffset, CVector vecAttachRotation) {
    if (!entityAttachTo) {
        return;
    }

    CEntity* const oldEntityAttachedTo = m_pAttachedTo;
    m_pAttachedTo = entityAttachTo;
    assert(m_pAttachedTo);
    m_pAttachedTo->RegisterReference(reinterpret_cast<CEntity**>(&m_pAttachedTo));
    m_vecAttachOffset = vecAttachOffset;
    if (physicalFlags.bInfiniteMass) {
        m_vecAttachedEntityRotation = GetPosition();
    } else {
        m_vecAttachedEntityRotation = vecAttachRotation;
    }
    m_qAttachedEntityRotation = CQuaternion{};
    m_pEntityIgnoredCollision = oldEntityAttachedTo;
    if (physicalFlags.bDisableCollisionForce) {
        physicalFlags.bCollidable = false;
        PositionAttachedEntity();
    } else {
        if (m_pAttachedTo->GetIsTypePhysical()
            && m_pAttachedTo->physicalFlags.bDisableCollisionForce
            && GetIsTypeObject() && !physicalFlags.bInfiniteMass) {
            physicalFlags.bDisableCollisionForce = true;
            m_fMass = 99999.9f;
            m_fTurnMass = 99999.9f;
        }
        PositionAttachedEntity();
    }
}

// 0x54D690
void CPhysical::AttachEntityToEntity(CPhysical* entityAttachTo, CVector* vecAttachOffset, CQuaternion* attachRotation) {
    // TODO(port RenderWare/animation): needs RwFrame car-node lookups and
    // RtQuatConvertFromMatrix for the quaternion branch.
    (void)entityAttachTo;
    (void)vecAttachOffset;
    (void)attachRotation;
}

// 0x546FF0 (reduced: vehicle/bike/quaternion branches need unported subsystems)
void CPhysical::PositionAttachedEntity() {
    if (!m_pAttachedTo || !m_matrix) {
        return;
    }

    CMatrix attachedEntityMatrix;
    const CMatrix attachedToEntityMatrix(m_pAttachedTo->GetMatrix());

    // TODO(port animation/quaternion): RtQuat branch; bike lean matrix;
    // forklift car-node branch.
    if (physicalFlags.bInfiniteMass) {
        attachedEntityMatrix = *m_matrix;
    } else {
        CMatrix attachedEntityRotationMatrix;
        // TODO: attachedEntityRotationMatrix.Rotate(m_vecAttachedEntityRotation)
        // once CMatrix::Rotate is ported.
        attachedEntityMatrix = attachedToEntityMatrix;
    }
    attachedEntityMatrix.GetPosition() = attachedToEntityMatrix.TransformPoint(m_vecAttachOffset);
    SetMatrix(attachedEntityMatrix);

    if (GetIsTypeObject()) {
        if (GetIsStatic()) {
            SetIsStatic(false);
        }
        physicalFlags.bAttachedToEntity = true;
        m_nFakePhysics = 0;
    }

    // TODO(port vehicle/object): detach conditions for dumper/forklift and
    // satchel; speed inheritance for vehicle/object carriers.
}

// 0x546DB0
void CPhysical::PlacePhysicalRelativeToOtherPhysical(CPhysical* relativeToPhysical, CPhysical* physicalToPlace, CVector offset) {
    if (!relativeToPhysical || !physicalToPlace || !relativeToPhysical->m_matrix || !physicalToPlace->m_matrix) {
        return; // TODO(port placeable): matrix always present once AllocateMatrix lands.
    }
    CVector vecRelativePosition = relativeToPhysical->m_matrix->TransformPoint(offset);
    vecRelativePosition += (CTimer::GetTimeStep() * 0.9f) * relativeToPhysical->m_vecMoveSpeed;
    CWorld::Remove(physicalToPlace);
    *physicalToPlace->m_matrix = *relativeToPhysical->m_matrix;
    physicalToPlace->GetPosition() = vecRelativePosition;
    physicalToPlace->m_vecMoveSpeed = relativeToPhysical->m_vecMoveSpeed;
    physicalToPlace->UpdateRwMatrix();
    physicalToPlace->UpdateRwFrame();
    CWorld::Add(physicalToPlace);
}

// ---------------------------------------------------------------------------
// Sector-list processing (TODO: full port needs CCollision + CColModel)
// ---------------------------------------------------------------------------

// 0x546670
bool CPhysical::ProcessShiftSectorList(int32_t sectorX, int32_t sectorY) {
    (void)sectorX;
    (void)sectorY;
    return false;
}

// 0x54BA60
bool CPhysical::ProcessCollisionSectorList(int32_t sectorX, int32_t sectorY) {
    (void)sectorX;
    (void)sectorY;
    return false;
}

// 0x54CFF0
bool CPhysical::ProcessCollisionSectorList_SimpleCar(CRepeatSector* repeatSector) {
    (void)repeatSector;
    return false;
}

// 0x54D920
bool CPhysical::CheckCollision() {
    SetCollisionProcessed(false);
    // TODO(port collision): ped/vehicle/object sector scans.
    return false;
}

// 0x54DAB0
bool CPhysical::CheckCollision_SimpleCar() {
    // TODO(port collision): simple-car sector scan.
    return false;
}

// ---------------------------------------------------------------------------
// Lighting / damage queries (TODO: timecycle + weapon subsystems)
// ---------------------------------------------------------------------------

// 0x5447B0
float CPhysical::GetLightingFromCol(bool bInteriorLighting) {
    (void)bInteriorLighting;
    return 1.0f;
}

// 0x544850
float CPhysical::GetLightingTotal() {
    return 1.0f;
}

// 0x5448B0
bool CPhysical::CanPhysicalBeDamaged(eWeaponType weapon, bool* bDamagedDueToFireOrExplosionOrBullet) {
    (void)weapon;
    (void)bDamagedDueToFireOrExplosionOrBullet;
    return false;
}

// 0x548320
void CPhysical::UnsetIsInSafePosition() {
    m_vecMoveSpeed *= -1.0f;
    m_vecTurnSpeed *= -1.0f;
    ApplySpeed();
    m_vecMoveSpeed *= -1.0f;
    m_vecTurnSpeed *= -1.0f;
    SetIsInSafePosition(false);
}

float CPhysical::GetMass(const CVector& pos, const CVector& dir) const {
    // TODO(port): verify formula against decomp (needs CrossProduct — now local).
    const CVector distance = m_matrix ? pos - m_matrix->TransformVector(m_vecCentreOfMass) : pos;
    const CVector cross = CrossProduct(distance, dir);
    return m_fMass + cross.SquaredMagnitude() / m_fTurnMass + dir.SquaredMagnitude() * 0.0f;
}
