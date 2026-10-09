// CObject - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Object/Object.h
// Dynamic world objects (props, mission objects, temp objects, doors, ...).
// Hierarchy: CPlaceable -> CEntity -> CPhysical -> CObject
//
// Adaptations: stripped InjectHooks(), friend InjectHooksMain, VALIDATE_SIZE,
// StaticRef globals (nNoTempObjects etc. move to the object-pool module later).

#pragma once

#include "CPhysical.h"
#include "ColTypes.h" // tColLighting (1-byte day/night nibbles, canonical collision-subsystem definition)

#include <cstdint>

enum eObjectType {
    OBJECT_UNKNOWN         = 0,
    OBJECT_GAME            = 1,
    OBJECT_MISSION         = 2,
    OBJECT_TEMPORARY       = 3, // AKA OBJECT_TYPE_FLYING_COMPONENT
    OBJECT_TYPE_CUTSCENE   = 4,
    OBJECT_TYPE_DECORATION = 5, // Hand object, projectiles, escalator step, water creatures, no clue what this enum value should be called
    OBJECT_MISSION2        = 6
};

// Values verified against decomp src/_types.h
enum eObjectColDamageEffect : int32_t {
    COL_DAMAGE_EFFECT_BREAKABLE = -56,
    COL_DAMAGE_EFFECT_BREAKABLE_REMOVED = -54,
    COL_DAMAGE_EFFECT_NONE = 0,
    COL_DAMAGE_EFFECT_CHANGE_MODEL = 1,
    COL_DAMAGE_EFFECT_SMASH_COMPLETELY = 20,
    COL_DAMAGE_EFFECT_CHANGE_THEN_SMASH = 21
};

// NOTE (2026-10-08, collision subsystem): the provisional 4-byte tColLighting
// that was defined here was replaced by the canonical 1-byte definition from
// ColTypes.h (gta-reversed VALIDATE_SIZE(tColLighting, 0x1); decomp
// src/CObject/GetLightingFromCollisionBelow_0059fd00.c copies a CColPoint's
// m_nLightingB into m_nColLighting as the same type). See BUILD_NOTES.md.

class CDummyObject; // ported with the dummy/object subsystem
class CFire;        // ported with the fire subsystem
class CObjectData;  // ported with model info
template<typename T> class CPtrNodeDoubleLink;

class CObject : public CPhysical {
public:
    CPtrNodeDoubleLink<CObject*>* m_pControlCodeList;
    uint8_t                      m_nObjectType; // see enum eObjectType
    uint8_t                      m_nBonusValue;
    uint16_t                     m_wCostValue;
    union {
        struct {
            uint32_t bIsPickup : 1;               // 0x1
            uint32_t b0x02 : 1;                   // 0x2 - collision related
            uint32_t bPickupPropertyForSale : 1;  // 0x4
            uint32_t bPickupInShopOutOfStock : 1; // 0x8
            uint32_t bHasBrokenGlass : 1;         // 0x10
            uint32_t bGlassBrokenAltogether : 1;  // 0x20
            uint32_t bIsExploded : 1;             // 0x40
            uint32_t bChangesVehColor : 1;        // 0x80

            uint32_t bIsLampPost : 1;
            uint32_t bIsTargetable : 1;
            uint32_t bIsBroken : 1;
            uint32_t bTrainCrossEnabled : 1;
            uint32_t bIsPhotographed : 1;
            uint32_t bIsLiftable : 1;
            uint32_t bIsDoorMoving : 1;
            uint32_t bIsDoorOpen : 1;

            uint32_t bHasNoModel : 1;
            uint32_t bIsScaled : 1;
            uint32_t bCanBeAttachedToMagnet : 1;
            uint32_t bDamaged : 1;
            uint32_t b0x100000_0x200000 : 2; // something something scripts for brains
            uint32_t bFadingIn : 1; // works only for objects with type 2 (OBJECT_MISSION)
            uint32_t bAffectedByColBrightness : 1;

            uint32_t bEnableDisabledAttractors : 1;
            uint32_t bDoNotRender : 1;
            uint32_t bFadingIn2 : 1;
            uint32_t b0x08000000 : 1;
            uint32_t b0x10000000 : 1;
            uint32_t b0x20000000 : 1;
            uint32_t b0x40000000 : 1;
            uint32_t b0x80000000 : 1;
        } objectFlags;
        uint32_t m_nObjectFlags;
    };
    uint8_t         m_nColDamageEffect;        // see eObjectColDamageEffect
    uint8_t         m_nSpecialColResponseCase; // see eObjectSpecialColResponseCases (TODO: port enum)
    char            field_146;                 // TODO: identify from decomp
    int8_t          m_nGarageDoorGarageIndex;
    uint8_t         m_nLastWeaponDamage;
    tColLighting    m_nColLighting;
    int16_t         m_nRefModelIndex;
    uint8_t         m_nCarColor[4];  // this is used for detached car parts
    uint32_t        m_nRemovalTime;  // time when this object must be deleted
    float           m_fHealth;
    float           m_fDoorStartAngle; // this is used for door objects
    float           m_fScale;
    CObjectData*    m_pObjectInfo;
    CFire*          m_pFire;
    int16_t         m_nStreamedScriptBrainToLoad;
    int16_t         m_wRemapTxd;     // this is used for detached car parts
    RwTexture*      m_pRemapTexture; // this is used for detached car parts
    CDummyObject*   m_pDummyObject;  // used for dynamic objects like garage doors, train crossings etc.
    uint32_t        m_nBurnTime;     // time when particles must be stopped
    float           m_fBurnDamage;

    // NOTE: the original header declared StaticRef globals here
    // (nNoTempObjects, fDistToNearestTree, bAircraftCarrierSamSiteDisabled,
    //  bArea51SamSiteDisabled). They move to the object-pool module when ported.
    // Added 2026-10-09 for CAutomobile. TODO(port): real StaticRef addresses.
    inline static int32_t nNoTempObjects = 0;

public:
    CObject();
    CObject(int32_t modelId, bool bCreate);
    explicit CObject(CDummyObject* dummyObj);
    ~CObject() override;

    static void* operator new(size_t size);
    static void* operator new(size_t size, int32_t poolRef);
    static void operator delete(void* obj);
    static void operator delete(void* obj, int32_t poolRef);

    void  SetIsStatic(bool isStatic) override;
    void  CreateRwObject() override;
    void  ProcessControl() override;
    void  Teleport(CVector destination, bool resetRotation) override;
    void  SpecialEntityPreCollisionStuff(CPhysical* colPhysical, bool bIgnoreStuckCheck, bool& bCollisionDisabled, bool& bCollidedEntityCollisionIgnored, bool& bCollidedEntityUnableToMove, bool& bThisOrCollidedEntityStuck) override;
    uint8_t SpecialEntityCalcCollisionSteps(bool& bProcessCollisionBeforeSettingTimeStep, bool& unk2) override;
    void  PreRender() override;
    void  Render() override;
    bool  SetupLighting() override;
    void  RemoveLighting(bool bRemove) override;

    bool Load();
    bool Save();

    void     ProcessGarageDoorBehaviour();
    [[nodiscard]] bool CanBeDeleted() const;
    void     SetRelatedDummy(CDummyObject* relatedDummy);
    bool     TryToExplode();
    void     SetObjectTargettable(bool targetable);
    [[nodiscard]] bool CanBeTargetted() const;
    void     RefModelInfo(int32_t modelIndex);
    void     SetRemapTexture(RwTexture* remapTexture, int16_t txdIndex);
    float    GetRopeHeight();
    void     SetRopeHeight(float height);
    CEntity* GetObjectCarriedWithRope();
    void     ReleaseObjectCarriedWithRope();
    void     AddToControlCodeList();
    void     RemoveFromControlCodeList();
    void     ResetDoorAngle();
    void     LockDoor();
    void     Init();
    void     DoBurnEffect() const;
    void     GetLightingFromCollisionBelow();
    void     ProcessSamSiteBehaviour();
    void     ProcessTrainCrossingBehaviour();
    void     ObjectDamage(float damage, const CVector* fxOrigin, const CVector* fxDirection, CEntity* damager, eWeaponType weaponType);
    void     Explode();
    void     ObjectFireDamage(float damage, CEntity* damager);

    void GrabObjectToCarryWithRope(CPhysical* attachTo);
    bool CanBeUsedToTakeCoverBehind();
    void ProcessControlLogic();

    static CObject* Create(int32_t modelIndex, bool bUnused);
    static CObject* Create(CDummyObject* dummyObject);

    static void SetMatrixForTrainCrossing(CMatrix* matrix, float fAngle);
    static void TryToFreeUpTempObjects(int32_t numObjects);
    static void DeleteAllTempObjects();
    static void DeleteAllMissionObjects();
    static void DeleteAllTempObjectsInArea(CVector point, float radius);

    // Helpers
    [[nodiscard]] bool IsTemporary() const     { return m_nObjectType == OBJECT_TEMPORARY; }
    [[nodiscard]] bool IsMissionObject() const { return m_nObjectType == OBJECT_MISSION || m_nObjectType == OBJECT_MISSION2; }
    [[nodiscard]] bool IsCraneMovingPart() const;
    [[nodiscard]] bool IsFallenLampPost() const { return objectFlags.bIsLampPost && m_matrix->GetUp().z < 0.66F; }
    [[nodiscard]] bool IsExploded() const       { return objectFlags.bIsExploded; }
    [[nodiscard]] bool CanBeSmashed() const     { return m_nColDamageEffect >= COL_DAMAGE_EFFECT_SMASH_COMPLETELY; }
};

// TODO: static_assert(sizeof(CObject) == 0x17C) once the CEntity/CPhysical layout chain is verified.

bool IsObjectPointerValid_NotInWorld(CObject* object);
bool IsObjectPointerValid(CObject* object);
