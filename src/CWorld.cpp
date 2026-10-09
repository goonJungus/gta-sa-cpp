// CWorld.cpp - GTA SA 1.0 clean-room C++ conversion
// Static world query API: 120x120 sector grid, repeat sectors, LOD lists,
// line-of-sight, ground height, explosions, area clearing.
// Adapted from gta-reversed/source/game_sa/World.cpp and verified against the
// decomp (src/CWorld/*.c). Collision/weapon/population dependent bodies are
// marked TODO(port) until those subsystems land.
//
// The original kept all state in StaticRef globals bound to fixed binary
// addresses; here the state is file-local (see below) and owned by this TU.
// CWorld itself stays a static-only class per CWorld.h.
//
// Clean-room notes:
// - Local mirrors of CSector / CRepeatSector / CPtrList* are defined below
//   (TODO: delete these when the real core headers are ported; definitions
//   are kept byte-identical across the TUs that need them).

#include "CWorld.h"
#include "CObject.h" // m_nObjectType for the mission-object filter
#include "CTimer.h"

#include <cmath>

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
    struct const_iterator {
        const Node* n;
        explicit const_iterator(const Node* node) : n(node) {}
        T operator*() const { return n->item; }
        const_iterator& operator++() { n = n->next; return *this; }
        bool operator!=(const const_iterator& o) const { return n != o.n; }
    };
    const_iterator begin() const { return const_iterator(head); }
    const_iterator end() const { return const_iterator(nullptr); }

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
    struct const_iterator {
        const Node* n;
        explicit const_iterator(const Node* node) : n(node) {}
        T operator*() const { return n->item; }
        const_iterator& operator++() { n = n->next; return *this; }
        bool operator!=(const const_iterator& o) const { return n != o.n; }
    };
    const_iterator begin() const { return const_iterator(head); }
    const_iterator end() const { return const_iterator(nullptr); }

private:
    Node* head{ nullptr };
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
// World state (file-local; the original used StaticRef fixed addresses)
// ---------------------------------------------------------------------------

static CSector ms_aSectors[MAX_SECTORS_Y][MAX_SECTORS_X];
static CRepeatSector ms_aRepeatSectors[MAX_REPEAT_SECTORS_Y][MAX_REPEAT_SECTORS_X];
static CPtrListSingleLink<CEntity*> ms_aLodPtrLists[MAX_LOD_PTR_LISTS_Y][MAX_LOD_PTR_LISTS_X];
static uint16_t ms_nCurrentScanCode = 0;

// Line-test / process options (were CWorld StaticRefs; the line-test trio plus
// pIgnoreEntity are CWorld static members so other subsystems can set them,
// e.g. CBulletInfo::Update).
bool      CWorld::bIncludeDeadPeds = false; // 0xB7CD71
bool      CWorld::bIncludeCarTyres = false; // 0xB7CD70
bool      CWorld::bIncludeBikers   = false; // 0xB7CD6F
CEntity*  CWorld::pIgnoreEntity    = nullptr; // 0xB7CD68
float     CWorld::fWeaponSpreadRate = 0.0f; // 0xC8A7D4
CColPoint CWorld::m_aTempColPts[32];
bool CWorld::bForceProcessControl = false; // 0xB7CD6E
static bool bProcessCutsceneOnly = false;

// ---------------------------------------------------------------------------
// Setup / teardown
// ---------------------------------------------------------------------------

// 0x563120
void CWorld::ResetLineTestOptions() {
    bIncludeCarTyres = false;
    bIncludeDeadPeds = false;
    bIncludeBikers = false;
}

// 0x5631E0
void CWorld::Initialise() {
    bForceProcessControl = false;
    bIncludeDeadPeds = false;
    bIncludeCarTyres = false;
    bIncludeBikers = false;
    bProcessCutsceneOnly = false;
    // TODO(port ipl): CIplStore::Initialise().
}

// 0x564050
void CWorld::ShutDown() {
    for (int32_t y = 0; y < MAX_SECTORS_Y; y++) {
        for (int32_t x = 0; x < MAX_SECTORS_X; x++) {
            ms_aSectors[y][x].Buildings.Clear();
            ms_aSectors[y][x].Dummies.Clear();
        }
    }
    for (size_t y = 0; y < MAX_REPEAT_SECTORS_Y; y++) {
        for (size_t x = 0; x < MAX_REPEAT_SECTORS_X; x++) {
            ms_aRepeatSectors[y][x].Vehicles.Clear();
            ms_aRepeatSectors[y][x].Peds.Clear();
            ms_aRepeatSectors[y][x].Objects.Clear();
        }
    }
    for (size_t y = 0; y < MAX_LOD_PTR_LISTS_Y; y++) {
        for (size_t x = 0; x < MAX_LOD_PTR_LISTS_X; x++) {
            ms_aLodPtrLists[y][x].Clear();
        }
    }
    ms_nCurrentScanCode = 0;
}

// 0x564360
void CWorld::ClearForRestart() {
    // TODO(port streaming/population): the original also tears down streamed
    // entities and restarts the population; sector lists are cleared here.
    ShutDown();
    Initialise();
}

// ---------------------------------------------------------------------------
// Sector access
// ---------------------------------------------------------------------------

// 0x566820 (clamps to [0, 119])
CSector& CWorld::GetSector(int32_t x, int32_t y) {
    if (x < 0) {
        x = 0;
    } else if (x > MAX_SECTORS_X - 1) {
        x = MAX_SECTORS_X - 1;
    }
    if (y < 0) {
        y = 0;
    } else if (y > MAX_SECTORS_Y - 1) {
        y = MAX_SECTORS_Y - 1;
    }
    return ms_aSectors[y][x];
}

// 0x56C6B0 (clamps to [0, 15])
CRepeatSector& CWorld::GetRepeatSector(int32_t x, int32_t y) {
    constexpr int32_t maxX = static_cast<int32_t>(MAX_REPEAT_SECTORS_X) - 1;
    constexpr int32_t maxY = static_cast<int32_t>(MAX_REPEAT_SECTORS_Y) - 1;
    if (x < 0) {
        x = 0;
    } else if (x > maxX) {
        x = maxX;
    }
    if (y < 0) {
        y = 0;
    } else if (y > maxY) {
        y = maxY;
    }
    return ms_aRepeatSectors[y][x];
}

// 0x4072C0 (clamps to [0, 29])
CPtrListSingleLink<CEntity*>& CWorld::GetLodPtrList(int32_t x, int32_t y) {
    constexpr int32_t maxX = static_cast<int32_t>(MAX_LOD_PTR_LISTS_X) - 1;
    constexpr int32_t maxY = static_cast<int32_t>(MAX_LOD_PTR_LISTS_Y) - 1;
    if (x < 0) {
        x = 0;
    } else if (x > maxX) {
        x = maxX;
    }
    if (y < 0) {
        y = 0;
    } else if (y > maxY) {
        y = maxY;
    }
    return ms_aLodPtrLists[y][x];
}

// NOTE: GetMovingEntityPtrList / GetObjectsWithControlCodePtrList are declared
// in CWorld.h but intentionally left undefined here: they must bind to the
// file-local moving-entity / control-code lists (currently owned by
// CPhysical.cpp / CObject.cpp) once world state is finalized. TODO.

// ---------------------------------------------------------------------------
// Scan codes
// ---------------------------------------------------------------------------

// 0x570050
void CWorld::AdvanceCurrentScanCode() {
    if (ms_nCurrentScanCode != 0xFFFF) {
        ms_nCurrentScanCode++;
        return;
    }
    ClearScanCodes();
    ms_nCurrentScanCode = 1;
}

// 0x407250
uint16_t CWorld::GetCurrentScanCode() {
    return ms_nCurrentScanCode;
}

// 0x563470
void CWorld::ClearScanCodes() {
    for (int32_t y = 0; y < MAX_SECTORS_Y; y++) {
        for (int32_t x = 0; x < MAX_SECTORS_X; x++) {
            auto& sector = ms_aSectors[y][x];
            for (CBuilding* item : sector.Buildings) {
                reinterpret_cast<CEntity*>(item)->SetScanCode(0);
            }
            for (CDummy* item : sector.Dummies) {
                reinterpret_cast<CEntity*>(item)->SetScanCode(0);
            }
        }
    }
    for (size_t y = 0; y < MAX_REPEAT_SECTORS_Y; y++) {
        for (size_t x = 0; x < MAX_REPEAT_SECTORS_X; x++) {
            auto& rs = ms_aRepeatSectors[y][x];
            for (CVehicle* item : rs.Vehicles) {
                reinterpret_cast<CEntity*>(item)->SetScanCode(0);
            }
            for (CPed* item : rs.Peds) {
                reinterpret_cast<CEntity*>(item)->SetScanCode(0);
            }
            for (CObject* item : rs.Objects) {
                reinterpret_cast<CEntity*>(item)->SetScanCode(0);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Add / remove
// ---------------------------------------------------------------------------

// 0x563220
void CWorld::Add(CEntity* entity) {
    entity->UpdateRwMatrix();
    entity->UpdateRwFrame();
    entity->Add();
    if (!entity->GetIsTypeBuilding() && !entity->GetIsTypeDummy()) {
        if (!entity->GetIsStatic()) {
            entity->AsPhysical()->AddToMovingList();
        }
    }
}

// 0x563280
void CWorld::Remove(CEntity* entity) {
    entity->Remove();
    if (entity->GetIsTypePhysical()) {
        entity->AsPhysical()->RemoveFromMovingList();
    }
}

// ---------------------------------------------------------------------------
// Range / volume queries
// ---------------------------------------------------------------------------

// 0x563500
template<typename PtrListType>
void CWorld::FindObjectsInRangeSectorList(PtrListType& ptrList, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities) {
    const float radiusSq = radius * radius;
    for (auto* const item : ptrList) {
        CEntity* const entity = reinterpret_cast<CEntity*>(item);
        if (entity->IsScanCodeCurrent()) {
            continue;
        }

        entity->SetCurrentScanCode();

        if (b2D) {
            const float dx = entity->GetPosition().x - point.x;
            const float dy = entity->GetPosition().y - point.y;
            if (dx * dx + dy * dy > radiusSq) {
                continue;
            }
        } else {
            if (DistanceBetweenPointsSquared(point, entity->GetPosition()) > radiusSq) {
                continue;
            }
        }

        // Don't stop at max count: scan codes must still be updated.
        if (*outCount < maxCount) {
            if (outEntities) {
                outEntities[*outCount] = entity;
            }
            ++*outCount;
        }
    }
}

// 0x564A20
void CWorld::FindObjectsInRange(const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies) {
    AdvanceCurrentScanCode();
    *outCount = 0;
    IterateSectorsOverlappedByRect(
        CRect{ CVector2D{ point.x, point.y }, radius },
        [&](int32_t x, int32_t y) {
            auto& sector = GetSector(x, y);
            auto& repeatSector = GetRepeatSector(x, y);

            if (buildings) {
                FindObjectsInRangeSectorList(sector.Buildings, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (vehicles) {
                FindObjectsInRangeSectorList(repeatSector.Vehicles, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (peds) {
                FindObjectsInRangeSectorList(repeatSector.Peds, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (objects) {
                FindObjectsInRangeSectorList(repeatSector.Objects, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (dummies) {
                FindObjectsInRangeSectorList(sector.Dummies, point, radius, b2D, outCount, maxCount, outEntities);
            }
            return true;
        }
    );
}

// 0x5635C0
template<typename PtrListType>
void CWorld::FindObjectsOfTypeInRangeSectorList(uint32_t modelId, PtrListType& ptrList, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities) {
    const float radiusSq = radius * radius;
    for (auto* const item : ptrList) {
        CEntity* const entity = reinterpret_cast<CEntity*>(item);
        if (entity->IsScanCodeCurrent()) {
            continue;
        }

        entity->SetCurrentScanCode();

        if (entity->GetModelIndex() != modelId) {
            continue;
        }

        bool inRange;
        if (b2D) {
            const float dx = entity->GetPosition().x - point.x;
            const float dy = entity->GetPosition().y - point.y;
            inRange = dx * dx + dy * dy <= radiusSq;
        } else {
            inRange = DistanceBetweenPointsSquared(point, entity->GetPosition()) <= radiusSq;
        }
        if (!inRange) {
            continue;
        }

        if (*outCount < maxCount) {
            if (outEntities) {
                outEntities[*outCount] = entity;
            }
            ++*outCount;
        }
    }
}

// 0x564C70
void CWorld::FindObjectsOfTypeInRange(uint32_t modelId, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies) {
    AdvanceCurrentScanCode();
    *outCount = 0;
    IterateSectorsOverlappedByRect(
        CRect{ CVector2D{ point.x, point.y }, radius },
        [&](int32_t x, int32_t y) {
            auto& sector = GetSector(x, y);
            auto& repeatSector = GetRepeatSector(x, y);

            if (buildings) {
                FindObjectsOfTypeInRangeSectorList(modelId, sector.Buildings, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (vehicles) {
                FindObjectsOfTypeInRangeSectorList(modelId, repeatSector.Vehicles, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (peds) {
                FindObjectsOfTypeInRangeSectorList(modelId, repeatSector.Peds, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (objects) {
                FindObjectsOfTypeInRangeSectorList(modelId, repeatSector.Objects, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (dummies) {
                FindObjectsOfTypeInRangeSectorList(modelId, sector.Dummies, point, radius, b2D, outCount, maxCount, outEntities);
            }
            return true;
        }
    );
}

// 0x564ED0
void CWorld::FindLodOfTypeInRange(uint32_t modelId, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities) {
    AdvanceCurrentScanCode();
    *outCount = 0;
    IterateLodSectorsOverlappedByRect(
        CRect{ CVector2D{ point.x, point.y }, radius },
        [&](int32_t x, int32_t y) {
            FindObjectsOfTypeInRangeSectorList(modelId, GetLodPtrList(x, y), point, radius, b2D, outCount, maxCount, outEntities);
            return true;
        }
    );
}

// 0x565000 (reduced: colmodel bound radius not ported; uses GetBoundRadius())
template<typename PtrListType>
void CWorld::FindObjectsKindaCollidingSectorList(PtrListType& ptrList, const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities) {
    for (auto* const item : ptrList) {
        if (*outCount >= maxCount) {
            return;
        }

        CEntity* const entity = reinterpret_cast<CEntity*>(item);
        if (entity->IsScanCodeCurrent()) {
            continue;
        }

        entity->SetCurrentScanCode();

        const float fRadiusToCheck = entity->GetBoundRadius() + radius; // TODO(port): colmodel bound radius
        float dist;
        if (b2D) {
            const float dx = entity->GetBoundCentre().x - point.x;
            const float dy = entity->GetBoundCentre().y - point.y;
            dist = std::sqrt(dx * dx + dy * dy);
        } else {
            dist = DistanceBetweenPoints(entity->GetBoundCentre(), point);
        }
        if (dist >= fRadiusToCheck) {
            continue;
        }

        if (outEntities) {
            outEntities[*outCount] = entity;
        }
        ++*outCount;
    }
}

// 0x568B80
void CWorld::FindObjectsKindaColliding(const CVector& point, float radius, bool b2D, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies) {
    AdvanceCurrentScanCode();
    *outCount = 0;
    IterateSectorsOverlappedByRect(
        CRect{ CVector2D{ point.x, point.y }, radius },
        [&](int32_t x, int32_t y) {
            auto& sector = GetSector(x, y);
            auto& repeatSector = GetRepeatSector(x, y);

            if (buildings) {
                FindObjectsKindaCollidingSectorList(sector.Buildings, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (vehicles) {
                FindObjectsKindaCollidingSectorList(repeatSector.Vehicles, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (peds) {
                FindObjectsKindaCollidingSectorList(repeatSector.Peds, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (objects) {
                FindObjectsKindaCollidingSectorList(repeatSector.Objects, point, radius, b2D, outCount, maxCount, outEntities);
            }
            if (dummies) {
                FindObjectsKindaCollidingSectorList(sector.Dummies, point, radius, b2D, outCount, maxCount, outEntities);
            }
            return true;
        }
    );
}

// 0x5650E0 (reduced: uses GetBoundRadius() until colmodel lands)
template<typename PtrListType>
void CWorld::FindObjectsIntersectingCubeSectorList(PtrListType& ptrList, const CVector& cornerA, const CVector& cornerB, int16_t* outCount, int16_t maxCount, CEntity** outEntities) {
    const CVector mn{ std::fmin(cornerA.x, cornerB.x), std::fmin(cornerA.y, cornerB.y), std::fmin(cornerA.z, cornerB.z) };
    const CVector mx{ std::fmax(cornerA.x, cornerB.x), std::fmax(cornerA.y, cornerB.y), std::fmax(cornerA.z, cornerB.z) };
    for (auto* const item : ptrList) {
        CEntity* const entity = reinterpret_cast<CEntity*>(item);
        if (entity->IsScanCodeCurrent()) {
            continue;
        }

        entity->SetCurrentScanCode();

        const float fBoundRadius = entity->GetBoundRadius(); // TODO(port): colmodel bound radius
        const CVector pos = entity->GetPosition();

        if (pos.x + fBoundRadius >= mn.x && pos.x - fBoundRadius <= mx.x
            && pos.y + fBoundRadius >= mn.y && pos.y - fBoundRadius <= mx.y
            && pos.z + fBoundRadius >= mn.z && pos.z - fBoundRadius <= mx.z) {
            if (*outCount < maxCount) {
                if (outEntities) {
                    outEntities[*outCount] = entity;
                }
                ++*outCount;
            }
        }
    }
}

// 0x568DD0
void CWorld::FindObjectsIntersectingCube(const CVector& cornerA, const CVector& cornerB, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies) {
    AdvanceCurrentScanCode();
    *outCount = 0;
    const CRect rect{
        std::fmin(cornerA.x, cornerB.x), std::fmin(cornerA.y, cornerB.y),
        std::fmax(cornerA.x, cornerB.x), std::fmax(cornerA.y, cornerB.y)
    };
    IterateSectorsOverlappedByRect(
        rect,
        [&](int32_t x, int32_t y) {
            auto& sector = GetSector(x, y);
            auto& repeatSector = GetRepeatSector(x, y);

            if (buildings) {
                FindObjectsIntersectingCubeSectorList(sector.Buildings, cornerA, cornerB, outCount, maxCount, outEntities);
            }
            if (vehicles) {
                FindObjectsIntersectingCubeSectorList(repeatSector.Vehicles, cornerA, cornerB, outCount, maxCount, outEntities);
            }
            if (peds) {
                FindObjectsIntersectingCubeSectorList(repeatSector.Peds, cornerA, cornerB, outCount, maxCount, outEntities);
            }
            if (objects) {
                FindObjectsIntersectingCubeSectorList(repeatSector.Objects, cornerA, cornerB, outCount, maxCount, outEntities);
            }
            if (dummies) {
                FindObjectsIntersectingCubeSectorList(sector.Dummies, cornerA, cornerB, outCount, maxCount, outEntities);
            }
            return true;
        }
    );
}

// 0x565200 (TODO: needs CBox + CCollision::TestSphereBox)
template<typename PtrListType>
void CWorld::FindObjectsIntersectingAngledCollisionBoxSectorList(PtrListType& ptrList, const CBox& box, const CMatrix& transform, const CVector& point, int16_t* outCount, int16_t maxCount, CEntity** outEntities) {
    (void)ptrList;
    (void)box;
    (void)transform;
    (void)point;
    (void)outCount;
    (void)maxCount;
    (void)outEntities;
}

// 0x568FF0 (TODO: needs CBox + CCollision::TestSphereBox)
void CWorld::FindObjectsIntersectingAngledCollisionBox(const CBox& box, const CMatrix& transform, const CVector& point, float x1, float y1, float x2, float y2, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool buildings, bool vehicles, bool peds, bool objects, bool dummies) {
    (void)box;
    (void)transform;
    (void)point;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)outCount;
    (void)maxCount;
    (void)outEntities;
    (void)buildings;
    (void)vehicles;
    (void)peds;
    (void)objects;
    (void)dummies;
}

// 0x565300 (reduced: mission-vehicle/ped filters need unported type info)
template<typename PtrListType>
void CWorld::FindMissionEntitiesIntersectingCubeSectorList(PtrListType& ptrList, const CVector& cornerA, const CVector& cornerB, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool vehiclesList, bool pedsList, bool objectsList) {
    const CVector mn{ std::fmin(cornerA.x, cornerB.x), std::fmin(cornerA.y, cornerB.y), std::fmin(cornerA.z, cornerB.z) };
    const CVector mx{ std::fmax(cornerA.x, cornerB.x), std::fmax(cornerA.y, cornerB.y), std::fmax(cornerA.z, cornerB.z) };
    for (auto* const item : ptrList) {
        CEntity* const entity = reinterpret_cast<CEntity*>(item);
        if (entity->IsScanCodeCurrent()) {
            continue;
        }

        entity->SetCurrentScanCode();

        if (vehiclesList || pedsList) {
            // TODO(port): mission-vehicle / mission-ped creation flags.
            continue;
        } else if (objectsList) {
            const auto type = entity->AsObject()->m_nObjectType;
            if (type != OBJECT_MISSION && type != OBJECT_MISSION2) {
                continue;
            }
        }

        const CVector pos = entity->GetPosition();
        if (pos.x >= mn.x && pos.x <= mx.x && pos.y >= mn.y && pos.y <= mx.y && pos.z >= mn.z && pos.z <= mx.z) {
            if (*outCount < maxCount) {
                if (outEntities) {
                    outEntities[(*outCount)++] = entity;
                }
            } else {
                break;
            }
        }
    }
}

// 0x569240
void CWorld::FindMissionEntitiesIntersectingCube(const CVector& cornerA, const CVector& cornerB, int16_t* outCount, int16_t maxCount, CEntity** outEntities, bool vehicles, bool peds, bool objects) {
    AdvanceCurrentScanCode();
    *outCount = 0;
    const CRect rect{
        std::fmin(cornerA.x, cornerB.x), std::fmin(cornerA.y, cornerB.y),
        std::fmax(cornerA.x, cornerB.x), std::fmax(cornerA.y, cornerB.y)
    };
    IterateSectorsOverlappedByRect(
        rect,
        [&](int32_t x, int32_t y) {
            auto& repeatSector = GetRepeatSector(x, y);
            if (vehicles) {
                FindMissionEntitiesIntersectingCubeSectorList(repeatSector.Vehicles, cornerA, cornerB, outCount, maxCount, outEntities, true, false, false);
            }
            if (peds) {
                FindMissionEntitiesIntersectingCubeSectorList(repeatSector.Peds, cornerA, cornerB, outCount, maxCount, outEntities, false, true, false);
            }
            if (objects) {
                FindMissionEntitiesIntersectingCubeSectorList(repeatSector.Objects, cornerA, cornerB, outCount, maxCount, outEntities, false, false, true);
            }
            return true;
        }
    );
}

// 0x565450
template<typename PtrListType>
void CWorld::FindNearestObjectOfTypeSectorList(int32_t modelId, PtrListType& ptrList, const CVector& point, float radius, bool b2D, CEntity*& outEntity, float& outDistance) {
    for (auto* const item : ptrList) {
        CEntity* const entity = reinterpret_cast<CEntity*>(item);
        if (entity->IsScanCodeCurrent()) {
            continue;
        }

        entity->SetCurrentScanCode();

        if (modelId >= 0 && entity->GetModelIndex() != static_cast<uint32_t>(modelId)) {
            continue;
        }

        float dist;
        if (b2D) {
            const float dx = entity->GetPosition().x - point.x;
            const float dy = entity->GetPosition().y - point.y;
            dist = std::sqrt(dx * dx + dy * dy);
        } else {
            dist = DistanceBetweenPoints(entity->GetPosition(), point);
        }

        if (dist <= radius) {
            outDistance = dist;
            outEntity = entity;
        }
    }
}

// 0x5693F0
CEntity* CWorld::FindNearestObjectOfType(int32_t modelId, const CVector& point, float radius, bool b2D, bool buildings, bool vehicles, bool peds, bool objects, bool dummies) {
    AdvanceCurrentScanCode();
    CEntity* nearest = nullptr;
    float nearestDist = radius;
    IterateSectorsOverlappedByRect(
        CRect{ CVector2D{ point.x, point.y }, radius },
        [&](int32_t x, int32_t y) {
            auto& sector = GetSector(x, y);
            auto& repeatSector = GetRepeatSector(x, y);

            if (buildings) {
                FindNearestObjectOfTypeSectorList(modelId, sector.Buildings, point, radius, b2D, nearest, nearestDist);
            }
            if (vehicles) {
                FindNearestObjectOfTypeSectorList(modelId, repeatSector.Vehicles, point, radius, b2D, nearest, nearestDist);
            }
            if (peds) {
                FindNearestObjectOfTypeSectorList(modelId, repeatSector.Peds, point, radius, b2D, nearest, nearestDist);
            }
            if (objects) {
                FindNearestObjectOfTypeSectorList(modelId, repeatSector.Objects, point, radius, b2D, nearest, nearestDist);
            }
            if (dummies) {
                FindNearestObjectOfTypeSectorList(modelId, sector.Dummies, point, radius, b2D, nearest, nearestDist);
            }
            return true;
        }
    );
    return nearest;
}

// ---------------------------------------------------------------------------
// Sphere test (reduced: CCollision not ported; bound-sphere pre-test only)
// ---------------------------------------------------------------------------

// 0x566140
template<typename PtrListType>
CEntity* CWorld::TestSphereAgainstSectorList(PtrListType& ptrList, CVector sphereCenter, float sphereRadius, CEntity* ignoreEntity, bool doCameraIgnoreCheck) {
    if (ptrList.IsEmpty()) {
        return nullptr;
    }

    for (auto* const item : ptrList) {
        CEntity* const entity = reinterpret_cast<CEntity*>(item);
        if (entity->IsScanCodeCurrent()) {
            continue;
        }

        if (!entity->GetUsesCollision() || ignoreEntity == entity) {
            continue;
        }

        if (doCameraIgnoreCheck && CameraToIgnoreThisObject(entity)) {
            continue;
        }

        entity->SetCurrentScanCode();

        // TODO(port collision): full CCollision::ProcessColModels test.
        if ((entity->GetBoundCentre() - sphereCenter).Magnitude() < sphereRadius + entity->GetBoundRadius()) {
            return entity;
        }
    }

    return nullptr;
}

// 0x569E20
CEntity* CWorld::TestSphereAgainstWorld(CVector sphereCenter, float sphereRadius, CEntity* ignoreEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doCameraIgnoreCheck) {
    AdvanceCurrentScanCode();
    CEntity* found = nullptr;
    IterateSectorsOverlappedByRect(
        CRect{ CVector2D{ sphereCenter.x, sphereCenter.y }, sphereRadius },
        [&](int32_t x, int32_t y) {
            auto& sector = GetSector(x, y);
            auto& repeatSector = GetRepeatSector(x, y);

            if (!found && buildings) {
                found = TestSphereAgainstSectorList(sector.Buildings, sphereCenter, sphereRadius, ignoreEntity, doCameraIgnoreCheck);
            }
            if (!found && vehicles) {
                found = TestSphereAgainstSectorList(repeatSector.Vehicles, sphereCenter, sphereRadius, ignoreEntity, doCameraIgnoreCheck);
            }
            if (!found && peds) {
                found = TestSphereAgainstSectorList(repeatSector.Peds, sphereCenter, sphereRadius, ignoreEntity, doCameraIgnoreCheck);
            }
            if (!found && objects) {
                found = TestSphereAgainstSectorList(repeatSector.Objects, sphereCenter, sphereRadius, ignoreEntity, doCameraIgnoreCheck);
            }
            if (!found && dummies) {
                found = TestSphereAgainstSectorList(sector.Dummies, sphereCenter, sphereRadius, ignoreEntity, doCameraIgnoreCheck);
            }
            return !found; // stop iterating once something is found
        }
    );
    return found;
}

// 0x563F40 (reduced: needs CGarages + CObjectData camera-avoid flags)
bool CWorld::CameraToIgnoreThisObject(CEntity* entity) {
    (void)entity;
    return false;
}

// ---------------------------------------------------------------------------
// Ground / roof queries (TODO: collision subsystem)
// ---------------------------------------------------------------------------

// 0x569660
float CWorld::FindGroundZForCoord(float x, float y) {
    (void)x;
    (void)y;
    return 0.0f;
}

// 0x5696C0
float CWorld::FindGroundZFor3DCoord(CVector coord, bool* outResult, CEntity** outEntity) {
    (void)coord;
    if (outResult) {
        *outResult = false;
    }
    if (outEntity) {
        *outEntity = nullptr;
    }
    return 0.0f;
}

// 0x569750
float CWorld::FindRoofZFor3DCoord(float x, float y, float z, bool* outResult) {
    (void)x;
    (void)y;
    (void)z;
    if (outResult) {
        *outResult = false;
    }
    return 0.0f;
}

// 0x5697F0
float CWorld::FindLowestZForCoord(float x, float y) {
    (void)x;
    (void)y;
    return 0.0f;
}

// ---------------------------------------------------------------------------
// Line of sight (TODO: collision subsystem)
// ---------------------------------------------------------------------------

// 0x56A490
bool CWorld::GetIsLineOfSightClear(const CVector& origin, const CVector& target, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doCameraIgnoreCheck) {
    (void)origin;
    (void)target;
    (void)buildings;
    (void)vehicles;
    (void)peds;
    (void)objects;
    (void)dummies;
    (void)doSeeThroughCheck;
    (void)doCameraIgnoreCheck;
    return true;
}

// 0x56BA00
bool CWorld::ProcessLineOfSight(const CVector& origin, const CVector& target, CColPoint& outColPoint, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doCameraIgnoreCheck, bool doShootThroughCheck) {
    (void)origin;
    (void)target;
    (void)outColPoint;
    (void)buildings;
    (void)vehicles;
    (void)peds;
    (void)objects;
    (void)dummies;
    (void)doSeeThroughCheck;
    (void)doCameraIgnoreCheck;
    (void)doShootThroughCheck;
    outEntity = nullptr;
    return false;
}

// 0x5674E0
bool CWorld::ProcessVerticalLine(const CVector& origin, float distance, CColPoint& outColPoint, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly) {
    (void)origin;
    (void)distance;
    (void)outColPoint;
    (void)buildings;
    (void)vehicles;
    (void)peds;
    (void)objects;
    (void)dummies;
    (void)doSeeThroughCheck;
    (void)outCollPoly;
    outEntity = nullptr;
    return false;
}

// 0x567620
bool CWorld::ProcessVerticalLine_FillGlobeColPoints(const CVector& origin, float distance, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly) {
    (void)origin;
    (void)distance;
    (void)buildings;
    (void)vehicles;
    (void)peds;
    (void)objects;
    (void)dummies;
    (void)doSeeThroughCheck;
    (void)outCollPoly;
    outEntity = nullptr;
    return false;
}

bool CWorld::GetIsLineOfSightSectorClear(CSector& sector, CRepeatSector& repeatSector, const CColLine& colLine, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doIgnoreCameraCheck) {
    (void)sector;
    (void)repeatSector;
    (void)colLine;
    (void)buildings;
    (void)vehicles;
    (void)peds;
    (void)objects;
    (void)dummies;
    (void)doSeeThroughCheck;
    (void)doIgnoreCameraCheck;
    return true;
}

bool CWorld::ProcessLineOfSightSector(CSector& sector, CRepeatSector& repeatSector, const CColLine& colLine, CColPoint& outColPoint, float& maxTouchDistance, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, bool doCameraIgnoreCheck, bool doShootThroughCheck) {
    (void)sector;
    (void)repeatSector;
    (void)colLine;
    (void)outColPoint;
    (void)maxTouchDistance;
    (void)buildings;
    (void)vehicles;
    (void)peds;
    (void)objects;
    (void)dummies;
    (void)doSeeThroughCheck;
    (void)doCameraIgnoreCheck;
    (void)doShootThroughCheck;
    outEntity = nullptr;
    return false;
}

bool CWorld::ProcessVerticalLineSector(CSector& sector, CRepeatSector& repeatSector, const CColLine& colLine, CColPoint& outColPoint, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly) {
    (void)sector;
    (void)repeatSector;
    (void)colLine;
    (void)outColPoint;
    (void)buildings;
    (void)vehicles;
    (void)peds;
    (void)objects;
    (void)dummies;
    (void)doSeeThroughCheck;
    (void)outCollPoly;
    outEntity = nullptr;
    return false;
}

bool CWorld::ProcessVerticalLineSector_FillGlobeColPoints(CSector& sector, CRepeatSector& repeatSector, const CColLine& colLine, CEntity*& outEntity, bool buildings, bool vehicles, bool peds, bool objects, bool dummies, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly) {
    (void)sector;
    (void)repeatSector;
    (void)colLine;
    (void)buildings;
    (void)vehicles;
    (void)peds;
    (void)objects;
    (void)dummies;
    (void)doSeeThroughCheck;
    (void)outCollPoly;
    outEntity = nullptr;
    return false;
}

template<typename PtrListType>
bool CWorld::GetIsLineOfSightSectorListClear(PtrListType& ptrList, const CColLine& colLine, bool doSeeThroughCheck, bool doCameraIgnoreCheck) {
    (void)ptrList;
    (void)colLine;
    (void)doSeeThroughCheck;
    (void)doCameraIgnoreCheck;
    return true;
}

template<typename PtrListType>
bool CWorld::ProcessLineOfSightSectorList(PtrListType& ptrList, const CColLine& colLine, CColPoint& outColPoint, float& minTouchDistance, CEntity*& outEntity, bool doSeeThroughCheck, bool doIgnoreCameraCheck, bool doShootThroughCheck) {
    (void)ptrList;
    (void)colLine;
    (void)outColPoint;
    (void)minTouchDistance;
    (void)doSeeThroughCheck;
    (void)doIgnoreCameraCheck;
    (void)doShootThroughCheck;
    outEntity = nullptr;
    return false;
}

template<typename PtrListType>
bool CWorld::ProcessVerticalLineSectorList(PtrListType& ptrList, const CColLine& colLine, CColPoint& colPoint, float& maxTouchDistance, CEntity*& outEntity, bool doSeeThroughCheck, CStoredCollPoly* collPoly) {
    (void)ptrList;
    (void)colLine;
    (void)colPoint;
    (void)maxTouchDistance;
    (void)doSeeThroughCheck;
    (void)collPoly;
    outEntity = nullptr;
    return false;
}

template<typename PtrListType>
bool CWorld::ProcessVerticalLineSectorList_FillGlobeColPoints(PtrListType& ptrList, const CColLine& colLine, CEntity*& outEntity, bool doSeeThroughCheck, CStoredCollPoly* outCollPoly) {
    (void)ptrList;
    (void)colLine;
    (void)doSeeThroughCheck;
    (void)outCollPoly;
    outEntity = nullptr;
    return false;
}

// ---------------------------------------------------------------------------
// Explosions / shadows (TODO: explosion + shadow subsystems)
// ---------------------------------------------------------------------------

// 0x56B790
void CWorld::TriggerExplosion(const CVector& point, float radius, float visibleDistance, CEntity* victim, CEntity* creator, bool processVehicleBombTimer, float damage) {
    (void)point;
    (void)radius;
    (void)visibleDistance;
    (void)victim;
    (void)creator;
    (void)processVehicleBombTimer;
    (void)damage;
}

template<typename PtrListType>
void CWorld::TriggerExplosionSectorList(PtrListType& ptrList, const CVector& point, float radius, float visibleDistance, CEntity* victim, CEntity* creator, bool processVehicleBombTimer, float damage) {
    (void)ptrList;
    (void)point;
    (void)radius;
    (void)visibleDistance;
    (void)victim;
    (void)creator;
    (void)processVehicleBombTimer;
    (void)damage;
}

// 0x564600 (debug NOP in the original)
void CWorld::CastShadow(float x1, float y1, float x2, float y2) {
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
}

template<typename PtrListType>
void CWorld::CastShadowSectorList(PtrListType& ptrList, float xmin, float ymin, float xmax, float ymax) {
    (void)ptrList;
    (void)xmin;
    (void)ymin;
    (void)xmax;
    (void)ymax;
}

// ---------------------------------------------------------------------------
// Frame processing (TODO: streaming/population/ped-vehicle systems)
// ---------------------------------------------------------------------------

// 0x5684A0
void CWorld::Process() {
}

// 0x5633D0 (unused; debug)
void CWorld::ProcessForAnimViewer() {
}

// 0x5647F0
void CWorld::ProcessAttachedEntities() {
    // TODO(port): iterate moving list, PositionAttachedEntity for attached.
}

// 0x563430
void CWorld::ProcessPedsAfterPreRender() {
}

// ---------------------------------------------------------------------------
// Area management (TODO: population/vehicle/ped systems)
// ---------------------------------------------------------------------------

// 0x563840
void CWorld::RemoveStaticObjects() {
}

// 0x565510 (reduced: ped/vehicle/object pools not ported; walks sector lists)
void CWorld::RemoveReferencesToDeletedObject(CEntity* entity) {
    // All repeat-sector occupants derive from CPhysical (see hierarchy in
    // CPlaceable.h); buildings are static and hold no collision records.
    for (size_t y = 0; y < MAX_REPEAT_SECTORS_Y; y++) {
        for (size_t x = 0; x < MAX_REPEAT_SECTORS_X; x++) {
            auto& rs = ms_aRepeatSectors[y][x];
            for (CVehicle* e : rs.Vehicles) {
                reinterpret_cast<CPhysical*>(e)->RemoveRefsToEntity(entity);
            }
            for (CPed* e : rs.Peds) {
                reinterpret_cast<CPhysical*>(e)->RemoveRefsToEntity(entity);
            }
            for (CObject* e : rs.Objects) {
                e->RemoveRefsToEntity(entity);
            }
        }
    }
}

// 0x563A10 (reduced: CPopulation not ported)
void CWorld::RemoveEntityInsteadOfProcessingIt(CEntity* entity) {
    Remove(entity);
    // TODO(port population): peds go through CPopulation::RemovePed; the
    // player ped is only removed from the world. Non-peds are deleted.
}

// 0x565610
void CWorld::ClearCarsFromArea(float x1, float y1, float z1, float x2, float y2, float z2) {
    (void)x1;
    (void)y1;
    (void)z1;
    (void)x2;
    (void)y2;
    (void)z2;
}

// 0x5667F0
void CWorld::ClearPedsFromArea(float x1, float y1, float z1, float x2, float y2, float z2) {
    (void)x1;
    (void)y1;
    (void)z1;
    (void)x2;
    (void)y2;
    (void)z2;
}

// 0x56A0D0
void CWorld::ClearExcitingStuffFromArea(const CVector& point, float radius, uint8_t bRemoveProjectilesAndShadows) {
    (void)point;
    (void)radius;
    (void)bRemoveProjectilesAndShadows;
}

// 0x566810
void CWorld::SetAllCarsCanBeDamaged(bool enable) {
    (void)enable;
}

// 0x566950
void CWorld::ExtinguishAllCarFiresInArea(CVector point, float radius) {
    (void)point;
    (void)radius;
}

// 0x565800
void CWorld::SetPedsOnFire(float x, float y, float z, float radius, CEntity* fireCreator) {
    (void)x;
    (void)y;
    (void)z;
    (void)radius;
    (void)fireCreator;
}

// 0x565900
void CWorld::SetPedsChoking(float x, float y, float z, float radius, CEntity* gasCreator) {
    (void)x;
    (void)y;
    (void)z;
    (void)radius;
    (void)gasCreator;
}

// 0x5659F0
void CWorld::SetCarsOnFire(CVector pos, float radius, CEntity* fireCreator) {
    (void)pos;
    (void)radius;
    (void)fireCreator;
}

// 0x56B910
void CWorld::SetWorldOnFire(CVector pos, float radius, CEntity* fireCreator) {
    (void)pos;
    (void)radius;
    (void)fireCreator;
}

// 0x565B70
eSprayPaintState CWorld::SprayPaintWorld(CVector& posn, CVector& outDir, float radius, bool processTagAlphaState) {
    (void)posn;
    (void)outDir;
    (void)radius;
    (void)processTagAlphaState;
    return eSprayPaintState::NOT_FOUND;
}

void CWorld::CheckBuildingOrientations() {
}

// 0x565CB0
void CWorld::RemoveFallenPeds() {
}

// 0x565E80
void CWorld::RemoveFallenCars() {
}

// 0x56B9C0
void CWorld::RepositionCertainDynamicObjects() {
}

// 0x569850
void CWorld::RepositionOneObject(CEntity* object) {
    (void)object;
}

// 0x5660B0
void CWorld::UseDetonator(CPed* creator) {
    (void)creator;
}

// 0x566420 (unused; debug)
void CWorld::PrintCarChanges() {
}

// 0x563690 / 0x5664A0 (unused; debug)
void CWorld::TestForBuildingsOnTopOfEachOther() {
}

template<typename PtrListType>
void CWorld::TestForBuildingsOnTopOfEachOther(PtrListType& ptrList) {
    (void)ptrList;
}

// 0x5639D0 / 0x566510 (unused; debug)
void CWorld::TestForUnusedModels() {
}

template<typename PtrListType>
void CWorld::TestForUnusedModels(PtrListType& ptrList, int32_t* models) {
    (void)ptrList;
    (void)models;
}

// 0x563690-ish
void CWorld::CheckBlockListIntegrity() {
}

// ---------------------------------------------------------------------------
// Chase / zone / wanted (TODO: wanted + ped systems)
// ---------------------------------------------------------------------------

// 0x566A60
void CWorld::CallOffChaseForArea(float x1, float y1, float x2, float y2) {
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
}

void CWorld::CallOffChaseForAreaSectorListVehicles(CPtrListDoubleLink<CVehicle*>& ptrList, float x1, float y1, float x2, float y2, float minX, float minY, float maxX, float maxY) {
    (void)ptrList;
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)minX;
    (void)minY;
    (void)maxX;
    (void)maxY;
}

void CWorld::CallOffChaseForAreaSectorListPeds(CPtrListDoubleLink<CPed*>& list, float minX, float minY, float maxX, float maxY, float biggerMinX, float biggerMinY, float biggerMaxX, float biggerMaxY) {
    (void)list;
    (void)minX;
    (void)minY;
    (void)maxX;
    (void)maxY;
    (void)biggerMinX;
    (void)biggerMinY;
    (void)biggerMaxX;
    (void)biggerMaxY;
}

// 0x563F20
void CWorld::HandleCollisionZoneChange(eLevelName oldZone, eLevelName newZone) {
    (void)oldZone;
    (void)newZone;
}

// 0x563F30
void CWorld::DoZoneTestForChaser(CPhysical* physical) {
    (void)physical;
}

// 0x566C10
void CWorld::StopAllLawEnforcersInTheirTracks() {
}

// ---------------------------------------------------------------------------
// Player slots (TODO: player info)
// ---------------------------------------------------------------------------

// 0x563FA0
int32_t CWorld::FindPlayerSlotWithPedPointer(void* ptr) {
    (void)ptr;
    return -1;
}

// 0x563FD0
int32_t CWorld::FindPlayerSlotWithRemoteVehiclePointer(void* ptr) {
    (void)ptr;
    return -1;
}

// 0x564000
int32_t CWorld::FindPlayerSlotWithVehiclePointer(CEntity* vehiclePtr) {
    (void)vehiclePtr;
    return -1;
}

// 0x56B9C0-ish helper
void CWorld::RemoveVehicleAndItsOccupants(CVehicle* veh) {
    (void)veh;
}

// 0x566C90
CVehicle* CWorld::FindUnsuspectingTargetCar(CVector point, CVector playerPosn) {
    (void)point;
    (void)playerPosn;
    return nullptr;
}

// 0x566DA0
CPed* CWorld::FindUnsuspectingTargetPed(CVector point, CVector playerPosn) {
    (void)point;
    (void)playerPosn;
    return nullptr;
}
