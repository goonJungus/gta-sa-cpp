// CEntity - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Entity.h
// Base class for all world entities (buildings, vehicles, peds, objects, dummies).
//
// Replaced includes:
//   <rwplcore.h>, <rpworld.h> -> RenderWare types forward-declared below;
//       the clean-room renderer provides real definitions later.
//   "Reference.h" -> class CReference (forward-declared; port with streaming)
//   "ColModel.h"  -> class CColModel  (forward-declared; port with collision)
//   "eEntityType.h", "eEntityStatus.h", "eAreaCodes.h" -> enums defined below,
//       values verified against decomp src/_types.h
//
// Adaptations: stripped InjectHooks(), friend InjectHooksMain,
// Constructor()/Destructor() placement wrappers, NOTSA_EXPORT_VTABLE,
// VALIDATE_SIZE, StaticRef (GAME_GRAVITY moves to physics tunables later).

#pragma once

#include "CPlaceable.h"
#include "CRect.h" // CRect by value in Add()/GetBoundRect()

#include <cstdint>
#include <cassert>
#include <type_traits>

// ---- RenderWare forward declarations (clean-room renderer provides these later) ----
struct RwObject;  // RwMatrix is already defined in CMatrix.h
struct RpClump;
struct RpAtomic;
struct RpMaterial;
struct RwTexture;

// ---- Enums (values verified against decomp src/_types.h) ----
enum eEntityType : int32_t {
    ENTITY_TYPE_NOTHING = 0,
    ENTITY_TYPE_BUILDING = 1,
    ENTITY_TYPE_VEHICLE = 2,
    ENTITY_TYPE_PED = 3,
    ENTITY_TYPE_OBJECT = 4,
    ENTITY_TYPE_DUMMY = 5,
    ENTITY_TYPE_NOTINPOOLS = 6
};

enum eEntityStatus : int32_t {
    STATUS_PLAYER = 0,
    STATUS_PLAYER_PLAYBACK_FROM_BUFFER = 1,
    STATUS_SIMPLE = 2,
    STATUS_PHYSICS = 3,
    STATUS_ABANDONED = 4,
    STATUS_WRECKED = 5,
    STATUS_TRAIN_MOVING = 6,
    STATUS_TRAIN_NOT_MOVING = 7,
    STATUS_REMOTE_CONTROLLED = 8,
    STATUS_FORCED_STOP = 9,
    STATUS_IS_TOWED = 10,
    STATUS_IS_SIMPLE_TOWED = 11,
    STATUS_GHOST = 12
};

// Each area code can have one or more interiors.
// Usually stored as int8 in the game, but int32 is used for function arguments.
enum eAreaCodes : int32_t {
    AREA_CODE_NONE = -1,
    AREA_CODE_NORMAL_WORLD = 0, //!< Used when the player is outside
    AREA_CODE_1 = 1,            //!< Interiors
    AREA_CODE_2 = 2,            //!< Interiors
    AREA_CODE_3 = 3,            //!< Interiors
    AREA_CODE_4 = 4,            //!< Interiors
    AREA_CODE_5 = 5,            //!< Interiors
    AREA_CODE_6 = 6,            //!< Interiors
    AREA_CODE_7 = 7,            //!< Interiors
    AREA_CODE_8 = 8,            //!< Interiors
    AREA_CODE_9 = 9,            //!< Interiors
    AREA_CODE_10 = 10,          //!< Interiors
    AREA_CODE_11 = 11,          //!< Interiors
    AREA_CODE_12 = 12,          //!< Interiors
    AREA_CODE_13 = 13,          //!< Interiors
    AREA_CODE_14 = 14,          //!< Interiors
    AREA_CODE_15 = 15,          //!< Interiors
    AREA_CODE_16 = 16,          //!< Interiors
    AREA_CODE_17 = 17,          //!< Interiors
    AREA_CODE_18 = 18           //!< Interiors
};
// Stored as int8 in the original binary; kept narrow here to preserve layout.
using eAreaCodesS8 = int8_t;

// Model IDs referenced by the inline helpers below.
// TODO: move into eModelID.h when the model-ID enum is ported.
// Model IDs: canonical eModelID.h (deduped 2026-10-09; the anonymous enum
// here redefined MODEL_RCTIGER from eModelID.h).
#include "eModelID.h"

// ---- Forward declarations (ported in later subsystems) ----
class C2dEffect;
class CObject;
class CVehicle;
class CTrain;
class CBike;
class CBmx;
class CBoat;
class CAutomobile;
class CPed;
class CBuilding;
class CDummy;
class CPhysical;
class CBaseModelInfo;
class CColModel;
class CCollisionData;
class CReference;
class CBox;
template<typename T> class CLink;

class CEntity : public CPlaceable {
public:
    struct CEntityInfo {
        eEntityType   m_nType : 3;   // Mask: & 0x7  = 7
        eEntityStatus m_nStatus : 5; // Mask: & 0xF8 = 248 (Remember: In the original code unless this was left shifted the value it's compared to has to be left shifted by 3!)
    };

private:
    RwObject* m_pRwObject; // Use `GetRwObject`/`GetRpClump`/`GetRpAtomic` to access

public:

    union {
        struct {
            /* https://github.com/multitheftauto/mtasa-blue/blob/master/Client/game_sa/CEntitySA.h */
            bool m_bUsesCollision : 1;               // does entity use collision
            bool m_bCollisionProcessed : 1;          // has object been processed by a ProcessEntityCollision function
            bool m_bIsStatic : 1;                    // is entity static
            bool m_bHasContacted : 1;                // has entity processed some contact forces
            bool m_bIsStuck : 1;                     // is entity stuck
            bool m_bIsInSafePosition : 1;            // is entity in a collision free safe position
            bool m_bWasPostponed : 1;                // was entity control processing postponed
            bool m_bIsVisible : 1;                   // is the entity visible

            bool m_bIsBIGBuilding : 1;               // Set if this entity is a big building
            bool m_bRenderDamaged : 1;               // use damaged LOD models for objects with applicable damage
            bool m_bStreamingDontDelete : 1;         // Don't let the streaming remove this
            bool m_bRemoveFromWorld : 1;             // remove this entity next time it should be processed
            bool m_bHasHitWall : 1;                  // has collided with a building (changes subsequent collisions)
            bool m_bImBeingRendered : 1;             // don't delete me because I'm being rendered
            bool m_bDrawLast : 1;                    // draw object last
            bool m_bDistanceFade : 1;                // Fade entity because it is far away

            bool m_bDontCastShadowsOn : 1;           // Don't cast shadows on this object
            bool m_bOffscreen : 1;                   // offscreen flag. This can only be trusted when it is set to true
            bool m_bIsStaticWaitingForCollision : 1; // this is used by script created entities - they are static until the collision is loaded below them
            bool m_bDontStream : 1;                  // tell the streaming not to stream me
            bool m_bUnderwater : 1;                  // this object is underwater change drawing order
            bool m_bHasPreRenderEffects : 1;         // Object has a prerender effects attached to it
            bool m_bIsTempBuilding : 1;              // whether the building is temporary (i.e. can be created and deleted more than once)
            bool m_bDontUpdateHierarchy : 1;         // Don't update the animation hierarchy this frame

            bool m_bHasRoadsignText : 1;             // entity is roadsign and has some 2dEffect text stuff to be rendered
            bool m_bDisplayedSuperLowLOD : 1;
            bool m_bIsProcObject : 1;                // set object has been generated by procedural object generator
            bool m_bBackfaceCulled : 1;              // has backface culling on
            bool m_bLightObject : 1;                 // light object with directional lights
            bool m_bUnimportantStream : 1;           // set that this object is unimportant, if streaming is having problems
            bool m_bTunnel : 1;                      // Is this model part of a tunnel
            bool m_bTunnelTransition : 1;            // This model should be rendered from within and outside the tunnel
        };

        uint32_t m_nFlags;
    };

    union {
        struct {
            uint16_t m_nRandomSeedUpperByte : 8;
            uint16_t m_nRandomSeedSecondByte : 8;
        };

        uint16_t m_nRandomSeed;
    };

    uint16_t m_nModelIndex;

    CReference* m_pReferences;

protected:
    CLink<CEntity*>* m_pStreamingLink;

    uint16_t     m_ScanCode;
    uint8_t      m_IplIndex;
    eAreaCodesS8 m_AreaCode;

    union {
        int32_t  m_nLodIndex; // -1 - without LOD model
        CEntity* m_pLod;
    };

    uint8_t m_NumLodChildren;

    int8_t m_NumLodChildrenRendered;

    CEntityInfo m_info;

public:
    CEntity();
    ~CEntity() override;

    virtual void Add();
    virtual void Add(const CRect& rect);
    virtual void Remove();

    void SetTypeBuilding() { SetType(ENTITY_TYPE_BUILDING); }
    void SetTypeVehicle()  { SetType(ENTITY_TYPE_VEHICLE); }
    void SetTypePed()      { SetType(ENTITY_TYPE_PED); }
    void SetTypeObject()   { SetType(ENTITY_TYPE_OBJECT); }
    void SetTypeDummy()    { SetType(ENTITY_TYPE_DUMMY); }

    [[nodiscard]] bool GetIsTypeBuilding() const { return GetType() == ENTITY_TYPE_BUILDING; }
    [[nodiscard]] bool GetIsTypeVehicle()  const { return GetType() == ENTITY_TYPE_VEHICLE; }
    [[nodiscard]] bool GetIsTypePed()      const { return GetType() == ENTITY_TYPE_PED; }
    [[nodiscard]] bool GetIsTypeObject()   const { return GetType() == ENTITY_TYPE_OBJECT; }
    [[nodiscard]] bool GetIsTypeDummy()    const { return GetType() == ENTITY_TYPE_DUMMY; }
    [[nodiscard]] bool GetIsTypePhysical() const { return GetType() > ENTITY_TYPE_BUILDING && GetType() < ENTITY_TYPE_DUMMY; } // 0x4DA030, orig GetIsPhysical

    void SetType(eEntityType type) { m_info.m_nType = type; }
    [[nodiscard]] eEntityType GetType() const noexcept { return m_info.m_nType; }

    void SetStatus(eEntityStatus status) { m_info.m_nStatus = status; }
    [[nodiscard]] eEntityStatus GetStatus() const noexcept { return m_info.m_nStatus; }

    // 0x541F70
    bool TreatAsPlayerForCollisions() {
        return GetStatus() == STATUS_PLAYER;
    }

    void SetUsesCollision(bool usesCollision) { m_bUsesCollision = usesCollision; }
    bool GetUsesCollision() const { return m_bUsesCollision; }
    void SetCollisionProcessed(bool collisionProcessed) { m_bCollisionProcessed = collisionProcessed; }
    bool GetCollisionProcessed() const { return m_bCollisionProcessed; } // unused
    virtual void SetIsStatic(bool isStatic) { m_bIsStatic = isStatic; } // 0x403E20
    bool GetIsStatic() const { return m_bIsStatic || m_bIsStaticWaitingForCollision; } // 0x4633E0
    void SetHasContacted(bool hasContacted) { m_bHasContacted = hasContacted; }
    bool GetHasContacted() const { return m_bHasContacted; }
    void SetIsStuck(bool isStuck) { m_bIsStuck = isStuck; }
    bool GetIsStuck() const { return m_bIsStuck; }
    void SetIsInSafePosition(bool isInSafePosition) { m_bIsInSafePosition = isInSafePosition; }
    bool GetIsInSafePosition() const { return m_bIsInSafePosition; }
    void SetWasPostponed(bool wasPostponed) { m_bWasPostponed = wasPostponed; }
    bool GetWasPostponed() const { return m_bWasPostponed; }
    void SetIsVisible(bool isVisible) { m_bIsVisible = isVisible; }
    bool GetIsVisible() const { return m_bIsVisible; }
    void SetHasHitWall(bool hasHitWall) { m_bHasHitWall = hasHitWall; }
    bool GetHasHitWall() const { return m_bHasHitWall; }
    void SetIsBackfaceCulled(bool backfaceCulled) { m_bBackfaceCulled = backfaceCulled; }
    bool GetIsBackfaceCulled() const { return m_bBackfaceCulled; } // unused
    void SetIsUnimportantStream(bool unimportantStream) { m_bUnimportantStream |= unimportantStream; }

    void SetScanCode(uint16_t scanCode) { m_ScanCode = scanCode; }
    uint16_t GetScanCode() const { return m_ScanCode; }
    void SetAreaCode(eAreaCodes areaCode) { m_AreaCode = static_cast<int8_t>(areaCode); }
    eAreaCodes GetAreaCode() const { return static_cast<eAreaCodes>(m_AreaCode); }
    void SetIplIndex(uint8_t iplIndex) { m_IplIndex = iplIndex; }
    uint8_t GetIplIndex() { return m_IplIndex; }

    virtual void SetModelIndex(uint32_t index);
    virtual void SetModelIndexNoCreate(uint32_t index);
    uint32_t GetModelIndex() const { return m_nModelIndex; }
    int32_t GetModelId() const { return m_nModelIndex; } // TODO: return eModelID once ported
    virtual void CreateRwObject();
    void AttachToRwObject(RwObject* object, bool updateMatrix);
    void DetachFromRwObject();
    virtual void DeleteRwObject();

    // TODO: needs the RenderWare frame layer (RwFrameGetMatrix etc.)
    RwMatrix* GetRwMatrix();
    void UpdateRwMatrix();

    CVector GetBoundCentre() const;
    void GetBoundCentre(CVector& outCentre) const;

    float GetBoundRadius() const;
    virtual CRect GetBoundRect() const;

    CColModel* GetColModel() const;

    // is entity touching entity
    bool GetIsTouching(CEntity* entity) const;
    // is entity touching sphere
    bool GetIsTouching(const CVector& centre, float radius) const;

    bool GetIsOnScreen();
    bool GetIsBoundingBoxOnScreen();
    bool IsEntityOccluded();

    // TODO: need CGame::currArea (port CGame first)
    bool IsInCurrentArea() const;
    bool IsInArea(int32_t area);
    bool IsVisible();
    bool IsVisibleComplex() { return IsVisible(); } // unused

    virtual void ProcessControl() { /* Do nothing */ } // 0x403E40
    virtual void ProcessCollision() { /* Do nothing */ } // 0x403E50
    virtual void ProcessShift() { /* Do nothing */ } // 0x403E60
    virtual bool TestCollision(bool applySpeed) { return false; } // 0x403E70
    virtual void Teleport(CVector newCoors, bool clearOrientation) { /* Do nothing */ } // 0x403E80
    virtual void SpecialEntityPreCollisionStuff(CPhysical* colPhysical, bool doingShift, bool& skipTestEntirely, bool& skipCol, bool& forceBuildingCol, bool& forceSoftCol) { /* Do nothing */ } // 0x403E90
    virtual uint8_t SpecialEntityCalcCollisionSteps(bool& doPreCheckAtFullSpeed, bool& doPreCheckAtHalfSpeed) { return 1; } // 0x403EA0

    void UpdateRwFrame();
    void UpdateRpHAnim();

    bool HasPreRenderEffects();
    bool DoesNotCollideWithFlyers();
    virtual void PreRender();
    virtual void Render();
    void UpdateAnim();

    void BuildWindSockMatrix();
    bool LivesInThisNonOverlapSector(int32_t x, int32_t y);
    float GetDistanceFromCentreOfMassToBaseOfModel() const;

    void ProcessLightsForEntity();

    void RemoveEscalatorsForEntity();

    virtual bool SetupLighting();
    virtual void RemoveLighting(bool reset);

    void SetupBigBuilding();

    void RegisterReference(CEntity** entity);

    void CleanUpOldReference(CEntity** entity);
    void ResolveReferences();
    void PruneReferences();
    void ModifyMatrixForTreeInWind();
    void ModifyMatrixForBannerInWind();
    void ModifyMatrixForCrane();
    void PreRenderForGlassWindow();

    virtual void FlagToDestroyWhenNextProcessed() { /* Do nothing */ } // 0x403EB0

    void SetRwObjectAlpha(int32_t alpha);

    void CreateEffects();
    void DestroyEffects();
    void RenderEffects();

    void SetLodIndex(uint32_t lodIndex) { m_nLodIndex = lodIndex; }
    uint32_t GetLodIndex() { return m_nLodIndex; }
    void SetLod(CEntity* lod) { m_pLod = lod; }
    CEntity* GetLod() { return m_pLod; }

    void AddLodChildren() { m_NumLodChildren++; } // orig AddLodChild
    void RemoveLodChildren() { m_NumLodChildren--; } // orig RemoveLodChild
    int32_t GetNumLodChildren() { return m_NumLodChildren; }

    void AddLodChildrenRendered() { m_NumLodChildrenRendered++; } // orig AddLodChildRendered
    void ResetLodChildrenRendered() { m_NumLodChildrenRendered = 0; } // orig ResetLodRenderedCounter
    bool HasLodChildrenRendered() { return m_NumLodChildrenRendered > 0; } // orig HasLodChildBeenRendered
    int32_t GetNumLodChildrenRendered() { return m_NumLodChildrenRendered; }
    void SetCannotLodChildrenRender() { m_NumLodChildrenRendered = -0x80; } // orig SetLodChildCannotRender
    bool CanLodChildrenRender() { return m_NumLodChildrenRendered != -0x80; } // orig CanLodChildRender

    // 128 = displaySuperLowLodFlag, yes, this is very hacky. Blame R*

    CVector* FindTriggerPointCoors(CVector* pOutVec, int32_t index);
    void CalculateBBProjection(CVector* point1, CVector* point2, CVector* point3, CVector* point4);

    C2dEffect* GetRandom2dEffect(int32_t effectType, bool mustBeFree);

    CVector TransformFromObjectSpace(const CVector& offset) const;
    CVector* TransformFromObjectSpace(CVector& outPos, const CVector& offset) const;
    RwMatrix* GetModellingMatrix();

    // NOTSA section

    /*!
    * @notsa
    * @param nodeID Node ID of the bone. For peds, see eBoneTag
    * @returns Bone transformation matrix into object space. To transform to world space ped's matrix must be used as well.
    */
    RwMatrix* GetBoneMatrix(uint32_t nodeID) const;

    RwObject* GetRwObject() const noexcept { return m_pRwObject; }
    // TODO(port): added 2026-10-09 for CPed (decomp reads m_nFlags on another entity).
    uint32_t GetFlags() const noexcept { return m_nFlags; }
    // TODO: need RenderWare type queries (RwObjectGetType, rpCLUMP/rpATOMIC)
    RpClump*  GetRpClump()  const noexcept;
    RpAtomic* GetRpAtomic() const noexcept;
    RpAtomic* GetRpAtomicOrFirstAtomicOfClump() const noexcept;

    // Always returns a non-null value. In case there's no LOD object `this` is returned
    CEntity* FindLastLOD() noexcept;

    CBaseModelInfo* GetModelInfo() const;

    // TODO: needs CColModel definition (collision subsystem)
    CCollisionData* GetColData();

    // Wrapper around the mess called `CleanUpOldReference`
    // Takes in `ref` (which is usually a member variable),
    // calls `CleanUpOldReference` on it, then sets it to `nullptr`
    // Used often in the code.
    // NOTE: original used C++23 `requires`; spelled with enable_if for C++17.
    template<typename T, typename = std::enable_if_t<std::is_base_of_v<CEntity, T>>>
    static void ClearReference(T*& ref) {
        if (ref) {
            ref->CleanUpOldReference(reinterpret_cast<CEntity**>(&ref));
            ref = nullptr;
        }
    }

    // Wrapper around the mess called "entity references"
    // This one sets the given `inOutRef` member variable to `entity`
    // + clears the old entity (if any)
    // + set the new entity (if any)
    template<typename T, typename Y,
        typename = std::enable_if_t<std::is_base_of_v<CEntity, T> && std::is_base_of_v<CEntity, Y>>>
    static void ChangeEntityReference(T*& inOutRef, Y* entity) {
        ClearReference(inOutRef); // Clear old
        if (entity) { // Set new (if any)
            inOutRef = entity;
            inOutRef->RegisterReference(reinterpret_cast<CEntity**>(&inOutRef));
        }
    }

    // Similar to `ChangeEntityReference`, but doesn't clear the old reference
    template<typename T, typename Y,
        typename = std::enable_if_t<std::is_base_of_v<CEntity, T> && std::is_base_of_v<CEntity, Y>>>
    static void SetEntityReference(T*& inOutRef, Y* entity) {
        inOutRef = entity;
        inOutRef->RegisterReference(reinterpret_cast<CEntity**>(&inOutRef));
    }

    // Register a reference to the entity that is stored in that given reference
    template<typename T, typename = std::enable_if_t<std::is_base_of_v<CEntity, T>>>
    static void RegisterReference(T*& ref) {
        ref->RegisterReference(reinterpret_cast<CEntity**>(&ref));
    }

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<CEntity, T>>>
    static void CleanUpOldReference(T*& ref) {
        ref->CleanUpOldReference(reinterpret_cast<CEntity**>(&ref));
    }

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<CEntity, T>>>
    static void SafeRegisterRef(T*& e) {
        if (e) {
            e->RegisterReference(reinterpret_cast<CEntity**>(&e));
        }
    }

    template<typename T, typename = std::enable_if_t<std::is_base_of_v<CEntity, T>>>
    static void SafeCleanUpRef(T*& e) {
        if (e) {
            e->CleanUpOldReference(reinterpret_cast<CEntity**>(&e));
        }
    }

public:
    // NOTSA section

    [[nodiscard]] bool IsModelTempCollision() const { return GetModelIndex() >= MODEL_TEMPCOL_DOOR1 && GetModelIndex() <= MODEL_TEMPCOL_BODYPART2; }
    [[nodiscard]] bool IsRCCar() const { return GetModelIndex() == MODEL_RCBANDIT || GetModelIndex() == MODEL_RCTIGER || GetModelIndex() == MODEL_RCCAM; }

    auto AsPhysical()         { return reinterpret_cast<CPhysical*>(this); }
    auto AsVehicle()          { return reinterpret_cast<CVehicle*>(this); }
    auto AsVehicle()    const { return reinterpret_cast<const CVehicle*>(this); }
    auto AsAutomobile()       { return reinterpret_cast<CAutomobile*>(this); }
    auto AsAutomobile() const { return reinterpret_cast<const CAutomobile*>(this); }
    auto AsBike()             { return reinterpret_cast<CBike*>(this); }
    auto AsBike()       const { return reinterpret_cast<const CBike*>(this); }
    auto AsBmx()              { return reinterpret_cast<CBmx*>(this); }
    auto AsBmx()        const { return reinterpret_cast<const CBmx*>(this); }
    auto AsBoat()             { return reinterpret_cast<CBoat*>(this); }
    auto AsBoat()       const { return reinterpret_cast<const CBoat*>(this); }
    auto AsTrain()            { return reinterpret_cast<CTrain*>(this); }
    auto AsTrain()      const { return reinterpret_cast<const CTrain*>(this); }
    auto AsPed()              { return reinterpret_cast<CPed*>(this); }
    auto AsPed()        const { return reinterpret_cast<const CPed*>(this); }
    auto AsObject()           { return reinterpret_cast<CObject*>(this); }
    auto AsBuilding()         { return reinterpret_cast<CBuilding*>(this); }
    auto AsDummy()            { return reinterpret_cast<CDummy*>(this); }

    bool ProcessScan();
    bool IsScanCodeCurrent() const;
    void SetCurrentScanCode();
};

// TODO: static_assert(sizeof(CEntity) == 0x38) once CEntityInfo bitfield packing is verified on MSVC.

// Rw callbacks (defined when the RenderWare layer is ported):
// TODO: SetAtomicAlpha / SetCompAlphaCB / MaterialUpdateUVAnimCB
RpAtomic* SetAtomicAlpha(RpAtomic* atomic, void* data);
RpMaterial* SetCompAlphaCB(RpMaterial* material, void* data);
RpMaterial* MaterialUpdateUVAnimCB(RpMaterial* material, void* data);

bool IsEntityPointerValid(CEntity* entity);

// TODO: GAME_GRAVITY (default 0.008f) and other physics tunables move to a central
// constants header once the physics subsystem is ported.
