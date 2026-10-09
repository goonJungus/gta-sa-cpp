// CEntity.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations for CEntity (base class for all world entities).
// Adapted from gta-reversed/source/game_sa/Entity/Entity.cpp and verified
// against the decomp (src/CEntity/*.c). RenderWare/model-info/collision
// dependent bodies are marked TODO(port) until those subsystems land.
//
// vtable (gta-reversed 0x863928, 22 entries): every virtual declared in
// CEntity.h is defined here (or inline in the header): dtor, Add x2, Remove,
// SetIsStatic, SetModelIndex, SetModelIndexNoCreate, CreateRwObject,
// DeleteRwObject, GetBoundRect, ProcessControl, ProcessCollision,
// ProcessShift, TestCollision, Teleport, SpecialEntityPreCollisionStuff,
// SpecialEntityCalcCollisionSteps, PreRender, Render, SetupLighting,
// RemoveLighting, FlagToDestroyWhenNextProcessed.
//
// Clean-room notes:
// - Local mirrors of CReference / CSector / CRepeatSector / CPtrList* are
//   defined below (TODO: delete these when the real core headers are ported;
//   definitions are kept byte-identical across the TUs that need them).
// - CGeneral::GetRandomNumber -> std::rand (TODO: central RNG).

#include "CEntity.h"
#include "CWorld.h"

#include <cstdlib> // std::rand
#include <cmath>   // TransformFromObjectSpace fallback

// ---------------------------------------------------------------------------
// Local clean-room mirrors (TODO: replace with ported core headers)
// ---------------------------------------------------------------------------

template<typename T>
class CPtrListSingleLink {
public:
    using ItemType = T;
    struct Node {
        T item;
        Node* next;
    };

    CPtrListSingleLink() = default;
    ~CPtrListSingleLink() { Clear(); }
    CPtrListSingleLink(const CPtrListSingleLink&) = delete;
    CPtrListSingleLink& operator=(const CPtrListSingleLink&) = delete;

    void AddItem(T item) { head = new Node{ item, head }; }
    void DeleteItem(T item) {
        for (Node** pp = &head; *pp; pp = &(*pp)->next) {
            if ((*pp)->item == item) {
                Node* dead = *pp;
                *pp = dead->next;
                delete dead;
                return;
            }
        }
    }
    void Clear() {
        while (head) {
            Node* n = head;
            head = head->next;
            delete n;
        }
    }
    bool IsEmpty() const { return head == nullptr; }

    struct iterator {
        Node* n;
        explicit iterator(Node* node) : n(node) {}
        T operator*() const { return n->item; }
        iterator& operator++() { n = n->next; return *this; }
        bool operator!=(const iterator& o) const { return n != o.n; }
    };
    iterator begin() { return iterator(head); }
    iterator end() { return iterator(nullptr); }

private:
    Node* head{ nullptr };
};

template<typename T>
class CPtrListDoubleLink {
public:
    using ItemType = T;
    struct Node {
        T item;
        Node* prev;
        Node* next;
    };

    CPtrListDoubleLink() = default;
    ~CPtrListDoubleLink() { Clear(); }
    CPtrListDoubleLink(const CPtrListDoubleLink&) = delete;
    CPtrListDoubleLink& operator=(const CPtrListDoubleLink&) = delete;

    Node* AddItem(T item) {
        Node* n = new Node{ item, nullptr, head };
        if (head) {
            head->prev = n;
        }
        head = n;
        return n;
    }
    void DeleteNode(Node* node) {
        if (!node) {
            return;
        }
        if (node->prev) {
            node->prev->next = node->next;
        } else {
            head = node->next;
        }
        if (node->next) {
            node->next->prev = node->prev;
        }
        delete node;
    }
    void DeleteItem(T item) {
        for (Node* n = head; n; n = n->next) {
            if (n->item == item) {
                DeleteNode(n);
                return;
            }
        }
    }
    void Clear() {
        while (head) {
            DeleteNode(head);
        }
    }
    bool IsEmpty() const { return head == nullptr; }

    struct iterator {
        Node* n;
        explicit iterator(Node* node) : n(node) {}
        T operator*() const { return n->item; }
        iterator& operator++() { n = n->next; return *this; }
        bool operator!=(const iterator& o) const { return n != o.n; }
    };
    iterator begin() { return iterator(head); }
    iterator end() { return iterator(nullptr); }

private:
    Node* head{ nullptr };
};

// Mirrors gta-reversed/source/game_sa/Reference.h (8 bytes).
// m_pReferences points at the head of this entity's reference list.
class CReference {
public:
    CReference* m_pNext;
    CEntity**   m_ppEntity;
};

// Mirrors gta-reversed/source/game_sa/Sector.h.
class CSector {
public:
    CPtrListSingleLink<CBuilding*> Buildings{}; //!< Buildings in this sector [single-link]
    CPtrListDoubleLink<CDummy*>    Dummies{};   //!< Dummies in this sector

    CSector() = default;
    CSector(const CSector&) = delete;
    CSector& operator=(const CSector&) = delete;
};

// Mirrors gta-reversed CRepeatSector (Vehicles/Peds/Objects double-link lists).
class CRepeatSector {
public:
    CPtrListDoubleLink<CVehicle*> Vehicles{};
    CPtrListDoubleLink<CPed*>     Peds{};
    CPtrListDoubleLink<CObject*>  Objects{};

    CRepeatSector() = default;
    CRepeatSector(const CRepeatSector&) = delete;
    CRepeatSector& operator=(const CRepeatSector&) = delete;
};

// ---------------------------------------------------------------------------
// ctor / dtor
// ---------------------------------------------------------------------------

// 0x532A90
CEntity::CEntity() : CPlaceable() {
    SetStatus(STATUS_ABANDONED);
    SetType(ENTITY_TYPE_NOTHING);

    m_nFlags = 0;
    SetIsVisible(true);
    SetIsBackfaceCulled(true);

    SetScanCode(0);
    SetAreaCode(AREA_CODE_NORMAL_WORLD);
    m_nModelIndex = 0xFFFF; // MODEL_INVALID
    m_pRwObject = nullptr;
    SetIplIndex(0);
    m_nRandomSeed = static_cast<uint16_t>(std::rand()); // TODO(port): CGeneral::GetRandomNumber()
    m_pReferences = nullptr;
    m_pStreamingLink = nullptr;
    m_NumLodChildren = 0;
    ResetLodChildrenRendered();
    SetLod(nullptr);
}

// 0x535E90
CEntity::~CEntity() {
    if (GetLod()) {
        GetLod()->RemoveLodChildren();
    }

    CEntity::DeleteRwObject();
    CEntity::ResolveReferences();
}

// ---------------------------------------------------------------------------
// World add / remove
// ---------------------------------------------------------------------------

// 0x533020
void CEntity::Add() {
    Add(GetBoundRect());
}

// 0x5347D0
void CEntity::Add(const CRect& rect) {
    CRect usedRect = rect;
    if (usedRect.left < -3000.0F) {
        usedRect.left = -3000.0F;
    }
    if (usedRect.right >= 3000.0F) {
        usedRect.right = 2999.0F;
    }
    if (usedRect.bottom < -3000.0F) {
        usedRect.bottom = -3000.0F;
    }
    if (usedRect.top >= 3000.0F) {
        usedRect.top = 2999.0F;
    }

    if (m_bIsBIGBuilding) {
        CWorld::IterateLodSectorsOverlappedByRect(usedRect, [&](int32_t x, int32_t y) {
            CWorld::GetLodPtrList(x, y).AddItem(this);
            return true;
        });
    } else {
        CWorld::IterateSectorsOverlappedByRect(usedRect, [&](int32_t x, int32_t y) {
            auto& s = CWorld::GetSector(x, y);
            auto& rs = CWorld::GetRepeatSector(x, y);
            switch (GetType()) {
            case ENTITY_TYPE_DUMMY:    s.Dummies.AddItem(reinterpret_cast<CDummy*>(this)); break;
            case ENTITY_TYPE_VEHICLE:  rs.Vehicles.AddItem(reinterpret_cast<CVehicle*>(this)); break;
            case ENTITY_TYPE_PED:      rs.Peds.AddItem(reinterpret_cast<CPed*>(this)); break;
            case ENTITY_TYPE_OBJECT:   rs.Objects.AddItem(reinterpret_cast<CObject*>(this)); break;
            case ENTITY_TYPE_BUILDING: s.Buildings.AddItem(reinterpret_cast<CBuilding*>(this)); break;
            default: break;
            }
            return true;
        });
    }
}

// 0x534AE0
void CEntity::Remove() {
    auto usedRect = GetBoundRect();
    if (usedRect.left < -3000.0F) {
        usedRect.left = -3000.0F;
    }
    if (usedRect.right >= 3000.0F) {
        usedRect.right = 2999.0F;
    }
    if (usedRect.bottom < -3000.0F) {
        usedRect.bottom = -3000.0F;
    }
    if (usedRect.top >= 3000.0F) {
        usedRect.top = 2999.0F;
    }

    if (m_bIsBIGBuilding) {
        CWorld::IterateLodSectorsOverlappedByRect(usedRect, [&](int32_t x, int32_t y) {
            CWorld::GetLodPtrList(x, y).DeleteItem(this);
            return true;
        });
    } else {
        CWorld::IterateSectorsOverlappedByRect(usedRect, [&](int32_t x, int32_t y) {
            auto& s = CWorld::GetSector(x, y);
            auto& rs = CWorld::GetRepeatSector(x, y);
            switch (GetType()) {
            case ENTITY_TYPE_DUMMY:    s.Dummies.DeleteItem(reinterpret_cast<CDummy*>(this)); break;
            case ENTITY_TYPE_VEHICLE:  rs.Vehicles.DeleteItem(reinterpret_cast<CVehicle*>(this)); break;
            case ENTITY_TYPE_PED:      rs.Peds.DeleteItem(reinterpret_cast<CPed*>(this)); break;
            case ENTITY_TYPE_OBJECT:   rs.Objects.DeleteItem(reinterpret_cast<CObject*>(this)); break;
            case ENTITY_TYPE_BUILDING: s.Buildings.DeleteItem(reinterpret_cast<CBuilding*>(this)); break;
            default: break;
            }
            return true;
        });
    }
}

// ---------------------------------------------------------------------------
// Model index / RenderWare object
// ---------------------------------------------------------------------------

// 0x532AE0
void CEntity::SetModelIndex(uint32_t index) {
    SetModelIndexNoCreate(index);
    CreateRwObject();
}

// 0x533700
void CEntity::SetModelIndexNoCreate(uint32_t index) {
    m_nModelIndex = static_cast<uint16_t>(index);
    // TODO(port modelinfo): refresh flags from CBaseModelInfo, i.e.
    //   m_bHasPreRenderEffects = HasPreRenderEffects();
    //   if (mi->GetIsDrawLast()) m_bDrawLast = true;
    //   if (!mi->IsBackfaceCulled()) SetIsBackfaceCulled(false);
    //   tag-model registration via CTagManager::AddTag
}

// 0x533D30
void CEntity::CreateRwObject() {
    // TODO(port RenderWare + CModelInfo + CStreaming):
    // instantiate the RpAtomic/RpClump for m_nModelIndex, attach 2d effects,
    // handle LOD atomics. See decomp src/CEntity/CreateRwObject_00533d30.c.
}

// 0x534030
void CEntity::DeleteRwObject() {
    // TODO(port RenderWare): destroy m_pRwObject (atomic vs clump), detach
    // effects. See decomp src/CEntity/DeleteRwObject_00534030.c.
    m_pRwObject = nullptr;
}

// 0x533ED0
void CEntity::AttachToRwObject(RwObject* object, bool updateMatrix) {
    // TODO(port RenderWare): attach + optional matrix sync.
    // See decomp src/CEntity/AttachToRwObject_00533ed0.c.
    (void)object;
    (void)updateMatrix;
}

// 0x533FB0
void CEntity::DetachFromRwObject() {
    // TODO(port RenderWare): detach frame, keep matrix.
    // See decomp src/CEntity/DetachFromRwObject_00533fb0.c.
    m_pRwObject = nullptr;
}

// TODO(port RenderWare frame layer)
RwMatrix* CEntity::GetRwMatrix() {
    return nullptr;
}

// 0x446F90 (header TODO: needs the RenderWare frame layer)
void CEntity::UpdateRwMatrix() {
    // TODO(port RenderWare): sync Rw frame matrix from m_matrix/m_placement.
}

// 0x46A2D0 (header TODO: needs the RenderWare frame layer)
RwMatrix* CEntity::GetModellingMatrix() {
    return nullptr;
}

// ---------------------------------------------------------------------------
// Bounds / collision queries
// ---------------------------------------------------------------------------

// 0x534120
CRect CEntity::GetBoundRect() const {
    // TODO(port collision): build from GetColModel()->GetBoundingBox().
    // Fallback: 1m box around the entity position so Add()/Remove() still
    // land in the right sector.
    const CVector& pos = GetPosition();
    return CRect{ pos.x - 1.0F, pos.y - 1.0F, pos.x + 1.0F, pos.y + 1.0F };
}

// 0x534290
void CEntity::GetBoundCentre(CVector& centre) const {
    centre = TransformFromObjectSpace(GetBoundCentre());
}

// 0x534250
CVector CEntity::GetBoundCentre() const {
    // TODO(port collision): TransformFromObjectSpace(GetColModel()->GetBoundCenter()).
    return GetPosition();
}

float CEntity::GetBoundRadius() const {
    // TODO(port collision): GetModelInfo()->GetColModel()->GetBoundingSphere().m_fRadius.
    return 0.0F;
}

// 0x535300
CColModel* CEntity::GetColModel() const {
    // TODO(port modelinfo): CModelInfo::GetModelInfo(m_nModelIndex)->GetColModel().
    return nullptr;
}

CCollisionData* CEntity::GetColData() {
    // TODO(port collision): needs CColModel definition.
    return nullptr;
}

// 0x536BE0
float CEntity::GetDistanceFromCentreOfMassToBaseOfModel() const {
    // TODO(port collision): uses colmodel bounding box vs centre of mass.
    return 0.0F;
}

// 0x5343F0
bool CEntity::GetIsTouching(CEntity* entity) const {
    CVector centreA = GetBoundCentre();
    CVector centreB = entity->GetBoundCentre();
    const float radius = GetBoundRadius() + entity->GetBoundRadius();
    return (centreA - centreB).SquaredMagnitude() < radius * radius;
}

// 0x5344B0
bool CEntity::GetIsTouching(const CVector& centre, float radius) const {
    CVector centreB = GetBoundCentre();
    const float totalRadius = GetBoundRadius() + radius;
    return (centreB - centre).SquaredMagnitude() < totalRadius * totalRadius;
}

// 0x534540
bool CEntity::GetIsOnScreen() {
    // TODO(port renderer): TheCamera.IsSphereVisible(GetBoundCentre(), GetBoundRadius()).
    return false;
}

// 0x5345D0
bool CEntity::GetIsBoundingBoxOnScreen() {
    // TODO(port renderer): oriented bounding-box frustum test.
    return false;
}

// 0x71FAE0
bool CEntity::IsEntityOccluded() {
    // TODO(port occlusion): COcclusion::IsPositionOccluded etc.
    return false;
}

bool CEntity::IsInCurrentArea() const {
    // TODO(port CGame): needs CGame::currArea.
    return true;
}

bool CEntity::IsInArea(int32_t area) {
    // TODO(port CGame): needs CGame::currArea.
    (void)area;
    return true;
}

// 0x536BC0
bool CEntity::IsVisible() {
    // TODO(port modelinfo): full version also checks draw distance / LOD fade.
    return GetIsVisible();
}

// ---------------------------------------------------------------------------
// Render / effects (TODO: RenderWare + effects subsystems)
// ---------------------------------------------------------------------------

// 0x535FA0
void CEntity::PreRender() {
    // TODO(port): glass pre-render, LOD children accounting, wind modifiers...
    // See decomp src/CEntity/PreRender_00535fa0.c.
}

// 0x534310
void CEntity::Render() {
    // TODO(port RenderWare): RpClumpRender / RpAtomicRender dispatch.
    // See decomp src/CEntity/Render_00534310.c.
}

// 0x535F00
void CEntity::UpdateAnim() {
    // TODO(port animation): RpClumpForAllAtomics UpdateAnimCB.
}

// 0x532B00
void CEntity::UpdateRwFrame() {
    // TODO(port RenderWare): RwFrameUpdateObjects(m_pRwObject frame).
}

// 0x532B20
void CEntity::UpdateRpHAnim() {
    // TODO(port animation): RpHAnim hierarchy update.
}

// 0x532B70
bool CEntity::HasPreRenderEffects() {
    // TODO(port 2d effects): scan model 2d effects for light/particle types.
    return false;
}

// 0x532D40
bool CEntity::DoesNotCollideWithFlyers() {
    // TODO(port modelinfo): mi->SwaysInWind() || mi->bDontCollideWithFlyer.
    return false;
}

// 0x532DB0
void CEntity::BuildWindSockMatrix() {
    // TODO(port weather): orient along CWeather::WindDir.
}

// 0x534E90
void CEntity::ModifyMatrixForTreeInWind() {
    // TODO(port wind): WindModifiers tree sway.
}

// 0x535040
void CEntity::ModifyMatrixForBannerInWind() {
    // TODO(port wind): WindModifiers banner sway.
}

// 0x533170
void CEntity::ModifyMatrixForCrane() {
    // TODO(port timer): rotate Z by (CTimer::GetTimeInMS() & 0x3FF) * PI/512.26.
}

// 0x533240
void CEntity::PreRenderForGlassWindow() {
    // TODO(port glass): CGlass::AskForObjectToBeRenderedInGlass(this).
}

// 0x5332C0
void CEntity::SetRwObjectAlpha(int32_t alpha) {
    // TODO(port RenderWare): RpClumpForAllAtomics SetAtomicAlpha.
    (void)alpha;
}

// 0x533790
void CEntity::CreateEffects() {
    // TODO(port 2d effects): lights/particles/attractors/entry-exits.
}

// 0x533BF0
void CEntity::DestroyEffects() {
    // TODO(port 2d effects).
}

// 0x5342B0
void CEntity::RenderEffects() {
    // TODO(port roadsigns): render roadsign text 2d effects.
}

// 0x533380
CVector* CEntity::FindTriggerPointCoors(CVector* pOutVec, int32_t index) {
    // TODO(port 2d effects): trigger-script effect positions.
    (void)pOutVec;
    (void)index;
    return nullptr;
}

// 0x535340
void CEntity::CalculateBBProjection(CVector* point1, CVector* point2, CVector* point3, CVector* point4) {
    // TODO(port collision): project colmodel bounding box corners.
    (void)point1;
    (void)point2;
    (void)point3;
    (void)point4;
}

// 0x533410
C2dEffect* CEntity::GetRandom2dEffect(int32_t effectType, bool mustBeFree) {
    // TODO(port 2d effects).
    (void)effectType;
    (void)mustBeFree;
    return nullptr;
}

// 0x6FC7A0
void CEntity::ProcessLightsForEntity() {
    // TODO(port coronas/lighting): light-type 2d effects.
}

// 0x717900
void CEntity::RemoveEscalatorsForEntity() {
    // TODO(port escalators).
}

// 0x553DC0
bool CEntity::SetupLighting() {
    // TODO(port lighting): directional + point lights.
    return true;
}

// 0x553370
void CEntity::RemoveLighting(bool reset) {
    // TODO(port lighting).
    (void)reset;
}

// 0x533150
void CEntity::SetupBigBuilding() {
    SetUsesCollision(false);
    m_bIsBIGBuilding = true;
    m_bStreamingDontDelete = true;
    // TODO(port modelinfo): GetModelInfo()->SetOwnsColModel(true).
}

// ---------------------------------------------------------------------------
// Transforms
// ---------------------------------------------------------------------------

// 0x5334F0
CVector CEntity::TransformFromObjectSpace(const CVector& offset) const {
    if (m_matrix) {
        return m_matrix->TransformPoint(offset);
    }
    // CSimpleTransform fallback (heading-only rotation + position).
    // TODO: verify against decomp TransformPoint(CVector&, CSimpleTransform&, CVector&).
    const float c = std::cos(m_placement.m_fHeading);
    const float s = std::sin(m_placement.m_fHeading);
    return CVector{
        m_placement.m_vPosn.x + offset.x * c - offset.y * s,
        m_placement.m_vPosn.y + offset.x * s + offset.y * c,
        m_placement.m_vPosn.z + offset.z
    };
}

// 0x533560 (PC only)
CVector* CEntity::TransformFromObjectSpace(CVector& outPos, const CVector& offset) const {
    outPos = TransformFromObjectSpace(offset);
    return &outPos;
}

// ---------------------------------------------------------------------------
// Reference management (src/CEntity/{RegisterReference,CleanUpOldReference,
// ResolveReferences,PruneReferences}_*.c; gta-reversed references.cpp)
// ---------------------------------------------------------------------------

// Free-list of reference nodes (mirrors CReferences::pEmptyList).
static CReference* s_pEmptyReferenceList = nullptr;

// 0x571B70
void CEntity::RegisterReference(CEntity** entity) {
    if (GetIsTypeBuilding() && !m_bIsTempBuilding && !m_bIsProcObject && !GetIplIndex()) {
        return;
    }

    for (auto* ref = m_pReferences; ref; ref = ref->m_pNext) {
        if (ref->m_ppEntity == entity) {
            return;
        }
    }

    // TODO(port pools): when s_pEmptyReferenceList is null the original prunes
    // references from every ped/vehicle/object in the pools to free nodes.
    if (s_pEmptyReferenceList) {
        CReference* const ref = s_pEmptyReferenceList;
        s_pEmptyReferenceList = ref->m_pNext;
        ref->m_pNext = m_pReferences;
        m_pReferences = ref;
        ref->m_ppEntity = entity;
    }
}

// 0x571A00
void CEntity::CleanUpOldReference(CEntity** entity) {
    CReference** lastNext = &m_pReferences;
    for (CReference* ref = m_pReferences; ref; ref = ref->m_pNext) {
        if (ref->m_ppEntity == entity) {
            *lastNext = ref->m_pNext;
            ref->m_pNext = s_pEmptyReferenceList;
            s_pEmptyReferenceList = ref;
            ref->m_ppEntity = nullptr;
            break;
        }
        lastNext = &ref->m_pNext;
    }
}

// 0x571A40
void CEntity::ResolveReferences() {
    for (CReference* ref = m_pReferences; ref; ref = ref->m_pNext) {
        if (ref->m_ppEntity && *ref->m_ppEntity == this) {
            *ref->m_ppEntity = nullptr;
        }
    }

    if (m_pReferences) {
        CReference* const head = m_pReferences;
        CReference* tail = head;
        while (tail->m_pNext) {
            tail = tail->m_pNext;
        }
        for (CReference* ref = head; ref; ref = ref->m_pNext) {
            ref->m_ppEntity = nullptr;
        }
        tail->m_pNext = s_pEmptyReferenceList;
        s_pEmptyReferenceList = head;
        m_pReferences = nullptr;
    }
}

// 0x571A90
void CEntity::PruneReferences() {
    if (!m_pReferences) {
        return;
    }

    CReference* refs = m_pReferences;
    CReference** ppPrev = &m_pReferences;
    while (refs) {
        if (*refs->m_ppEntity == this) {
            ppPrev = &refs->m_pNext;
            refs = refs->m_pNext;
        } else {
            CReference* const next = refs->m_pNext;
            *ppPrev = refs->m_pNext;
            refs->m_pNext = s_pEmptyReferenceList;
            s_pEmptyReferenceList = refs;
            refs->m_ppEntity = nullptr;
            refs = next;
        }
    }
}

// ---------------------------------------------------------------------------
// Misc
// ---------------------------------------------------------------------------

// NOTSA
CEntity* CEntity::FindLastLOD() noexcept {
    CEntity* it = this;
    for (; it->m_pLod; it = it->m_pLod) {
    }
    return it;
}

// NOTSA
CBaseModelInfo* CEntity::GetModelInfo() const {
    // TODO(port modelinfo): CModelInfo::GetModelInfo(GetModelIndex()).
    return nullptr;
}

// NOTSA
bool CEntity::ProcessScan() {
    if (IsScanCodeCurrent()) {
        return false;
    }
    SetCurrentScanCode();
    return true;
}

// NOTSA
bool CEntity::IsScanCodeCurrent() const {
    return GetScanCode() == CWorld::GetCurrentScanCode();
}

// NOTSA
void CEntity::SetCurrentScanCode() {
    SetScanCode(CWorld::GetCurrentScanCode());
}

// NOTSA
RwMatrix* CEntity::GetBoneMatrix(uint32_t nodeID) const {
    // TODO(port animation): RpHAnimHierarchyGetNodeMatrix.
    (void)nodeID;
    return nullptr;
}

RpClump* CEntity::GetRpClump() const noexcept {
    // TODO(port RenderWare): RwObjectGetType dispatch.
    return nullptr;
}

RpAtomic* CEntity::GetRpAtomic() const noexcept {
    // TODO(port RenderWare): RwObjectGetType dispatch.
    return nullptr;
}

RpAtomic* CEntity::GetRpAtomicOrFirstAtomicOfClump() const noexcept {
    // TODO(port RenderWare): atomic-or-first-of-clump dispatch.
    return nullptr;
}

// 0x533290
RpAtomic* SetAtomicAlpha(RpAtomic* atomic, void* data) {
    // TODO(port RenderWare): RpAtomicGetGeometry + SetCompAlphaCB over materials.
    (void)data;
    return atomic;
}

// 0x533280
RpMaterial* SetCompAlphaCB(RpMaterial* material, void* data) {
    // TODO(port RenderWare): RpMaterialGetColor(material)->alpha = (uint8)data.
    (void)data;
    return material;
}

// 0x532D70
RpMaterial* MaterialUpdateUVAnimCB(RpMaterial* material, void* data) {
    // TODO(port RenderWare): RpMaterialUVAnimApplyUpdate.
    (void)data;
    return material;
}

// 0x533310
bool IsEntityPointerValid(CEntity* entity) {
    // TODO(port pools): validate against the entity pools.
    return entity != nullptr;
}

// 0x533050
bool CEntity::LivesInThisNonOverlapSector(int32_t x, int32_t y) {
    const CRect rect = GetBoundRect();
    const float centerX = (rect.left + rect.right) * 0.5F;
    const float centerY = (rect.bottom + rect.top) * 0.5F;
    return x == CWorld::GetSectorX(centerX) && y == CWorld::GetSectorY(centerY);
}
