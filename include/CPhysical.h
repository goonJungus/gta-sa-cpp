// CPhysical - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Entity/Physical.h
// Adds physics state (velocity, forces, mass, collision records) on top of CEntity.
// Hierarchy: CPlaceable -> CEntity -> CPhysical -> CObject / CVehicle / CPed ...
//
// Adaptations: stripped InjectHooks(), friend InjectHooksMain,
// Constructor()/Destructor() placement wrappers, VALIDATE_SIZE, StaticRef
// physics tunables (DAMPING_LIMIT_IN_FRAME etc. move to physics constants later).
// C++23 deducing-this accessors rewritten as const/non-const overload pairs (C++17).

#pragma once

#include "CEntity.h"
#include "CMatrix.h" // CQuaternion

#include <cstdint>
#include <span>

enum ePhysicalFlags {
    PHYSICAL_b01                     = 0x1,
    PHYSICAL_APPLY_GRAVITY           = 0x2,
    PHYSICAL_DISABLE_COLLISION_FORCE = 0x4,
    PHYSICAL_COLLIDABLE              = 0x8,
    PHYSICAL_DISABLE_TURN_FORCE      = 0x10,
    PHYSICAL_DISABLE_MOVE_FORCE      = 0x20,
    PHYSICAL_INFINITE_MASS           = 0x40,
    PHYSICAL_DISABLE_Z               = 0x80,

    PHYSICAL_SUBMERGED_IN_WATER      = 0x100,
    PHYSICAL_ON_SOLID_SURFACE        = 0x200,
    PHYSICAL_BROKEN                  = 0x400,
    PHYSICAL_b12                     = 0x800,
    PHYSICAL_b13                     = 0x1000,
    PHYSICAL_DONT_APPLY_SPEED        = 0x2000,
    PHYSICAL_b15                     = 0x4000,
    PHYSICAL_b16                     = 0x8000,

    PHYSICAL_17                      = 0x10000,
    PHYSICAL_18                      = 0x20000,
    PHYSICAL_BULLETPROOF             = 0x40000,
    PHYSICAL_FIREPROOF               = 0x80000,
    PHYSICAL_COLLISIONPROOF          = 0x100000,
    PHYSICAL_MEELEPROOF              = 0x200000,
    PHYSICAL_INVULNERABLE            = 0x400000,
    PHYSICAL_EXPLOSIONPROOF          = 0x800000,

    PHYSICAL_DONTCOLLIDEWITHFLYERS   = 0x1000000,
    PHYSICAL_ATTACHEDTOENTITY        = 0x2000000,
    PHYSICAL_27                      = 0x4000000,
    PHYSICAL_TOUCHINGWATER           = 0x8000000,
    PHYSICAL_CANBECOLLIDEDWITH       = 0x10000000,
    PHYSICAL_DESTROYED               = 0x20000000,
    PHYSICAL_31                      = 0x40000000,
    PHYSICAL_32                      = 0x80000000,
};

enum eEntityAltCollision : uint16_t {
    ALT_ENITY_COL_DEFAULT = 0,
    ALT_ENITY_COL_OBJECT,
    ALT_ENITY_COL_VEHICLE,
    ALT_ENITY_COL_BIKE_WRECKED,
    ALT_ENITY_COL_BOAT,
};

// Values verified against decomp src/_types.h
enum eSurfaceType : int32_t {
    SURFACE_P_FORESTSTUMPS = -128,
    SURFACE_P_FORESTSTICKS = -127,
    SURFACE_P_FORRESTLEAVES = -126,
    SURFACE_P_DESERTROCKS = -125,
    SURFACE_P_FORRESTDRY = -124,
    SURFACE_P_SPARSEFLOWERS = -123,
    SURFACE_P_BUILDINGSITE = -122,
    SURFACE_P_DOCKLANDS = -121,
    SURFACE_P_INDUSTRIAL = -120,
    SURFACE_P_INDUSTJETTY = -119,
    SURFACE_P_CONCRETELITTER = -118,
    SURFACE_P_ALLEYRUBISH = -117,
    SURFACE_P_JUNKYARDPILES = -116,
    SURFACE_P_JUNKYARDGRND = -115,
    SURFACE_P_DUMP = -114,
    SURFACE_P_CACTUSDENSE = -113,
    SURFACE_P_AIRPORTGRND = -112,
    SURFACE_P_CORNFIELD = -111,
    SURFACE_P_GRASSLIGHT = -110,
    SURFACE_P_GRASSLIGHTER = -109,
    SURFACE_P_GRASSLIGHTER2 = -108,
    SURFACE_P_GRASSMID1 = -107,
    SURFACE_P_GRASSMID2 = -106,
    SURFACE_P_GRASSDARK = -105,
    SURFACE_P_GRASSDARK2 = -104,
    SURFACE_P_GRASSDIRTMIX = -103,
    SURFACE_P_RIVERBEDSTONE = -102,
    SURFACE_P_RIVERBEDSHALLOW = -101,
    SURFACE_P_RIVERBEDWEEDS = -100,
    SURFACE_P_SEAWEED = -99,
    SURFACE_DOOR = -98,
    SURFACE_PLASTICBARRIER = -97,
    SURFACE_PARKGRASS = -96,
    SURFACE_STAIRSSTONE = -95,
    SURFACE_STAIRSMETAL = -94,
    SURFACE_STAIRSCARPET = -93,
    SURFACE_FLOORMETAL = -92,
    SURFACE_FLOORCONCRETE = -91,
    SURFACE_BIN_BAG = -90,
    SURFACE_THIN_METAL_SHEET = -89,
    SURFACE_METAL_BARREL = -88,
    SURFACE_PLASTIC_CONE = -87,
    SURFACE_PLASTIC_DUMPSTER = -86,
    SURFACE_METAL_DUMPSTER = -85,
    SURFACE_WOOD_PICKET_FENCE = -84,
    SURFACE_WOOD_SLATTED_FENCE = -83,
    SURFACE_WOOD_RANCH_FENCE = -82,
    SURFACE_UNBREAKABLE_GLASS = -81,
    SURFACE_HAY_BALE = -80,
    SURFACE_GORE = -79,
    SURFACE_RAILTRACK = -78,
    TOTAL_NUM_SURFACE_TYPES = -77,
    AE_SURFACE_TYPE_BMX = -68,
    AE_SURFACE_TYPE_MOLOTOV = -67,
    AE_SURFACE_TYPE_SATCHEL_CHARGE = -66,
    AE_SURFACE_TYPE_GRENADE = -65,
    AE_SURFACE_TYPE_POOL_BALL = -64,
    AE_SURFACE_TYPE_BASKETBALL = -63,
    AE_SURFACE_TYPE_PUNCHBAG = -62,
    TOTAL_NUM_COLLISION_SURFACE_TYPES = -61,
    SURFACE_NONE = -1,
    SURFACE_DEFAULT = 0,
    SURFACE_TARMAC = 1,
    SURFACE_TARMAC_FUCKED = 2,
    SURFACE_TARMAC_REALLYFUCKED = 3,
    SURFACE_PAVEMENT = 4,
    SURFACE_PAVEMENT_FUCKED = 5,
    SURFACE_GRAVEL = 6,
    SURFACE_FUCKED_CONCRETE = 7,
    SURFACE_PAINTED_GROUND = 8,
    SURFACE_GRASS_SHORT_LUSH = 9,
    SURFACE_GRASS_MEDIUM_LUSH = 10,
    SURFACE_GRASS_LONG_LUSH = 11,
    SURFACE_GRASS_SHORT_DRY = 12,
    SURFACE_GRASS_MEDIUM_DRY = 13,
    SURFACE_GRASS_LONG_DRY = 14,
    SURFACE_GOLFGRASS_ROUGH = 15,
    SURFACE_GOLFGRASS_SMOOTH = 16,
    SURFACE_STEEP_SLIDYGRASS = 17,
    SURFACE_STEEP_CLIFF = 18,
    SURFACE_FLOWERBED = 19,
    SURFACE_MEADOW = 20,
    SURFACE_WASTEGROUND = 21,
    SURFACE_WOODLANDGROUND = 22,
    SURFACE_VEGETATION = 23,
    SURFACE_MUD_WET = 24,
    SURFACE_MUD_DRY = 25,
    SURFACE_DIRT = 26,
    SURFACE_DIRTTRACK = 27,
    SURFACE_SAND_DEEP = 28,
    SURFACE_SAND_MEDIUM = 29,
    SURFACE_SAND_COMPACT = 30,
    SURFACE_SAND_ARID = 31,
    SURFACE_SAND_MORE = 32,
    SURFACE_SAND_BEACH = 33,
    SURFACE_CONCRETE_BEACH = 34,
    SURFACE_ROCK_DRY = 35,
    SURFACE_ROCK_WET = 36,
    SURFACE_ROCK_CLIFF = 37,
    SURFACE_WATER_RIVERBED = 38,
    SURFACE_WATER_SHALLOW = 39,
    SURFACE_CORNFIELD = 40,
    SURFACE_HEDGE = 41,
    SURFACE_WOOD_CRATES = 42,
    SURFACE_WOOD_SOLID = 43,
    SURFACE_WOOD_THIN = 44,
    SURFACE_GLASS = 45,
    SURFACE_GLASS_WINDOWS_LARGE = 46,
    SURFACE_GLASS_WINDOWS_SMALL = 47,
    SURFACE_EMPTY1 = 48,
    SURFACE_EMPTY2 = 49,
    SURFACE_GARAGE_DOOR = 50,
    SURFACE_THICK_METAL_PLATE = 51,
    SURFACE_SCAFFOLD_POLE = 52,
    SURFACE_LAMP_POST = 53,
    SURFACE_METAL_GATE = 54,
    SURFACE_METAL_CHAIN_FENCE = 55,
    SURFACE_GIRDER = 56,
    SURFACE_FIRE_HYDRANT = 57,
    SURFACE_CONTAINER = 58,
    SURFACE_NEWS_VENDOR = 59,
    SURFACE_WHEELBASE = 60,
    SURFACE_CARDBOARDBOX = 61,
    SURFACE_PED = 62,
    SURFACE_CAR = 63,
    SURFACE_CAR_PANEL = 64,
    SURFACE_CAR_MOVINGCOMPONENT = 65,
    SURFACE_TRANSPARENT_CLOTH = 66,
    SURFACE_RUBBER = 67,
    SURFACE_PLASTIC = 68,
    SURFACE_TRANSPARENT_STONE = 69,
    SURFACE_WOOD_BENCH = 70,
    SURFACE_CARPET = 71,
    SURFACE_FLOORBOARD = 72,
    SURFACE_STAIRSWOOD = 73,
    SURFACE_P_SAND = 74,
    SURFACE_P_SAND_DENSE = 75,
    SURFACE_P_SAND_ARID = 76,
    SURFACE_P_SAND_COMPACT = 77,
    SURFACE_P_SAND_ROCKY = 78,
    SURFACE_P_SANDBEACH = 79,
    SURFACE_P_GRASS_SHORT = 80,
    SURFACE_P_GRASS_MEADOW = 81,
    SURFACE_P_GRASS_DRY = 82,
    SURFACE_P_WOODLAND = 83,
    SURFACE_P_WOODDENSE = 84,
    SURFACE_P_ROADSIDE = 85,
    SURFACE_P_ROADSIDEDES = 86,
    SURFACE_P_FLOWERBED = 87,
    SURFACE_P_WASTEGROUND = 88,
    SURFACE_P_CONCRETE = 89,
    SURFACE_P_OFFICEDESK = 90,
    SURFACE_P_711SHELF1 = 91,
    SURFACE_P_711SHELF2 = 92,
    SURFACE_P_711SHELF3 = 93,
    SURFACE_P_RESTUARANTTABLE = 94,
    SURFACE_P_BARTABLE = 95,
    SURFACE_P_UNDERWATERLUSH = 96,
    SURFACE_P_UNDERWATERBARREN = 97,
    SURFACE_P_UNDERWATERCORAL = 98,
    SURFACE_P_UNDERWATERDEEP = 99,
    SURFACE_P_RIVERBED = 100,
    SURFACE_P_RUBBLE = 101,
    SURFACE_P_BEDROOMFLOOR = 102,
    SURFACE_P_KIRCHENFLOOR = 103,
    SURFACE_P_LIVINGRMFLOOR = 104,
    SURFACE_P_CORRIDORFLOOR = 105,
    SURFACE_P_711FLOOR = 106,
    SURFACE_P_FASTFOODFLOOR = 107,
    SURFACE_P_SKANKYFLOOR = 108,
    SURFACE_P_MOUNTAIN = 109,
    SURFACE_P_MARSH = 110,
    SURFACE_P_BUSHY = 111,
    SURFACE_P_BUSHYMIX = 112,
    SURFACE_P_BUSHYDRY = 113,
    SURFACE_P_BUSHYMID = 114,
    SURFACE_P_GRASSWEEFLOWERS = 115,
    SURFACE_P_GRASSDRYTALL = 116,
    SURFACE_P_GRASSLUSHTALL = 117,
    SURFACE_P_GRASSGRNMIX = 118,
    SURFACE_P_GRASSBRNMIX = 119,
    SURFACE_P_GRASSLOW = 120,
    SURFACE_P_GRASSROCKY = 121,
    SURFACE_P_GRASSSMALLTREES = 122,
    SURFACE_P_DIRTROCKY = 123,
    SURFACE_P_DIRTWEEDS = 124,
    SURFACE_P_GRASSWEEDS = 125,
    SURFACE_P_RIVEREDGE = 126,
    SURFACE_P_POOLSIDE = 127
};

// eWeaponType is only used as a parameter type in this subsystem; the full enum
// is ported with the weapons subsystem. Opaque declaration keeps this header compiling.
enum eWeaponType : uint32_t; // fixed 2026-10-08: gta-reversed uses uint32

// ---- Forward declarations (ported in later subsystems) ----
class CColPoint;
class CRealTimeShadow;
class CRepeatSector;
class CEntryInfoNode; // full definition with the collision subsystem
template<typename T> class CPtrNodeDoubleLink;

// Minimal CEntryInfoList: in the original it is a single head pointer (0x4).
// Full node management is ported with the collision subsystem.
class CEntryInfoList {
public:
    CEntryInfoNode* m_node{};

    void Flush();                           // TODO: verify from decomp (0x536E10)
    void DeleteNode(CEntryInfoNode* node);  // TODO: verify from decomp
    bool IsEmpty() const { return !m_node; }
    CEntryInfoNode* GetNodePtr() { return m_node; } // AKA GetHeadPtr
};

class CPhysical : public CEntity {
public:
    float   field_38; // TODO: identify from decomp
    uint32_t m_nLastCollisionTime;
    union {
        struct {
            uint32_t bMakeMassTwiceAsBig : 1;
            uint32_t bApplyGravity : 1;
            uint32_t bDisableCollisionForce : 1;
            uint32_t bCollidable : 1;
            uint32_t bDisableTurnForce : 1;
            uint32_t bDisableMoveForce : 1;
            uint32_t bInfiniteMass : 1;
            uint32_t bDisableZ : 1;

            uint32_t bSubmergedInWater : 1;
            uint32_t bOnSolidSurface : 1;
            uint32_t bBroken : 1;
            uint32_t bProcessCollisionEvenIfStationary : 1; // ref @ 0x6F5CF0
            uint32_t bSkipLineCol : 1;                               // only used for peds
            uint32_t bDontApplySpeed : 1;
            uint32_t bDontLoadCollision : 1;
            uint32_t bProcessingShift : 1;

            uint32_t bForceHitReturnFalse : 1;
            uint32_t bDisableSimpleCollision : 1; // ref @ CPhysical::ProcessCollision
            uint32_t bBulletProof : 1;
            uint32_t bFireProof : 1;
            uint32_t bCollisionProof : 1;
            uint32_t bMeleeProof : 1;
            uint32_t bInvulnerable : 1;
            uint32_t bExplosionProof : 1;

            uint32_t bDontCollideWithFlyers : 1;
            uint32_t bAttachedToEntity : 1;
            uint32_t bAddMovingCollisionSpeed : 1;
            uint32_t bTouchingWater : 1;
            uint32_t bCanBeCollidedWith : 1;
            uint32_t bRenderScorched : 1;
            uint32_t bDoorHitEndStop : 1;
            uint32_t bCarriedByRope : 1;
        } physicalFlags;
        uint32_t m_nPhysicalFlags;
    };
    CVector             m_vecMoveSpeed;
    CVector             m_vecTurnSpeed;
    CVector             m_vecFrictionMoveSpeed;
    CVector             m_vecFrictionTurnSpeed;
    CVector             m_vecForce;
    CVector             m_vecTorque;
    float               m_fMass;
    float               m_fTurnMass;
    float               m_fVelocityFrequency;
    float               m_fAirResistance;
    float               m_fElasticity;
    float               m_fBuoyancyConstant;
    CVector             m_vecCentreOfMass;
    CEntryInfoList      m_pCollisionList;
    CPtrNodeDoubleLink<CPhysical*>* m_pMovingList;
    uint8_t             m_nFakePhysics;
    uint8_t             m_nNumEntitiesCollided;
    eSurfaceType        m_nContactSurface;
    CEntity*            m_apCollidedEntities[6];
    float               m_fMovingSpeed; // ref @ CTheScripts::IsVehicleStopped
    float               m_fDamageIntensity;
    CEntity*            m_pDamageEntity;
    CVector             m_vecLastCollisionImpactVelocity;
    CVector             m_vecLastCollisionPosn;
    uint16_t            m_nPieceType;
    CPhysical*          m_pAttachedTo;
    CVector             m_vecAttachOffset;
    CVector             m_vecAttachedEntityRotation;
    CQuaternion         m_qAttachedEntityRotation;
    CEntity*            m_pEntityIgnoredCollision;
    float               m_fContactSurfaceBrightness;
    float               m_fDynamicLighting;
    CRealTimeShadow*    m_pShadowData;

    // NOTE: the original header also declared StaticRef physics tunables here
    // (DAMPING_LIMIT_IN_FRAME, SOFTCOL_* etc.). They move to a central physics
    // constants header when the physics subsystem is ported.

public:
    CPhysical();
    ~CPhysical() override;

    // originally virtual functions
    void Add() override;
    void Remove() override;
    CRect GetBoundRect() const override;
    void ProcessControl() override;
    void ProcessCollision() override;
    void ProcessShift() override;
    bool TestCollision(bool bApplySpeed) override;
    virtual int32_t ProcessEntityCollision(CEntity* entity, CColPoint* colPoint);

public:
    void RemoveAndAdd();
    void AddToMovingList();

    void RemoveFromMovingList();
    void SetDamagedPieceRecord(float fDamageIntensity, CEntity* entity, const CColPoint& colPoint, float fDistanceMult);
    void ApplyMoveForce(float x, float y, float z);
    void ApplyMoveForce(CVector force);
    void ApplyTurnForce(CVector force, CVector point);
    void ApplyForce(CVector vecMoveSpeed, CVector point, bool bUpdateTurnSpeed);

    CVector GetSpeed(CVector point);
    void ApplyMoveSpeed();
    void ApplyTurnSpeed();
    void ApplyGravity();
    void ApplyFrictionMoveForce(CVector moveForce);
    void ApplyFrictionTurnForce(CVector posn, CVector velocity);
    void ApplyFrictionForce(CVector vecMoveForce, CVector point);

    void SkipPhysics();
    void AddCollisionRecord(CEntity* collidedEntity);
    bool GetHasCollidedWith(CEntity* entity);
    bool GetHasCollidedWithAnyObject();

    bool ApplyCollision(CEntity* entity, const CColPoint& colPoint, float& outDamageIntensity);
    bool ApplySoftCollision(CEntity* entity, const CColPoint& colPoint, float& outDamageIntensity);
    bool ApplySpringCollision(float fSuspensionForceLevel, CVector& direction, CVector& collisionPoint, float fSpringLength, float fSuspensionBias, float& fSpringForceDampingLimit);
    bool ApplySpringCollisionAlt(float fSuspensionForceLevel, CVector& direction, CVector& collisionPoint, float fSpringLength, float fSuspensionBias, CVector& normal, float& fSpringForceDampingLimit);
    bool ApplySpringDampening(float fDampingForce, float fSpringForceDampingLimit, CVector& direction, CVector& collisionPoint, CVector& collisionPos);
    bool ApplySpringDampeningOld(float arg0, float arg1, CVector& arg2, CVector& arg3, CVector& arg4);

    void RemoveRefsToEntity(CEntity* entity);
    void DettachEntityFromEntity(float x, float y, float z, bool bApplyTurnForce);
    void DettachAutoAttachedEntity();
    float GetLightingFromCol(bool bInteriorLighting);
    float GetLightingTotal();
    bool CanPhysicalBeDamaged(eWeaponType weapon, bool* bDamagedDueToFireOrExplosionOrBullet);

    void ApplyAirResistance();
    bool ApplyCollisionAlt(CPhysical* entity, CColPoint& colPoint, float& damageIntensity, CVector& outVecMoveSpeed, CVector& outVecTurnSpeed);
    bool ApplyFriction(float fFriction, CColPoint& colPoint);
    bool ApplyFriction(CPhysical* entity, float fFriction, CColPoint& colPoint);

    bool ProcessShiftSectorList(int32_t sectorX, int32_t sectorY);
    static void PlacePhysicalRelativeToOtherPhysical(CPhysical* relativeToPhysical, CPhysical* physicalToPlace, CVector offset);

    float ApplyScriptCollision(CVector arg0, float arg1, float arg2, CVector* arg3);
    void PositionAttachedEntity();
    void ApplySpeed();
    void UnsetIsInSafePosition();
    void ApplyFriction();
    bool ApplyCollision(CEntity* theEntity, CColPoint& colPoint, float& thisDamageIntensity, float& entityDamageIntensity);
    bool ApplySoftCollision(CPhysical* physical, CColPoint& colPoint, float& thisDamageIntensity, float& entityDamageIntensity);

    bool ProcessCollisionSectorList(int32_t sectorX, int32_t sectorY);
    bool ProcessCollisionSectorList_SimpleCar(CRepeatSector* repeatSector);
    void AttachEntityToEntity(CPhysical* entity, CVector offset, CVector rotation);
    void AttachEntityToEntity(CPhysical* pEntityAttachTo, CVector* vecAttachOffset, CQuaternion* attachRotation);
    bool CheckCollision();
    bool CheckCollision_SimpleCar();

    void  SetMoveSpeedXY(CVector2D v)    { m_vecMoveSpeed = CVector{ v.x, v.y, m_vecMoveSpeed.z }; }
    // Original used C++23 deducing-this; spelled out as overload pairs for C++17.
    CVector& GetMoveSpeed() { return m_vecMoveSpeed; }
    const CVector& GetMoveSpeed() const { return m_vecMoveSpeed; }
    void  SetVelocity(CVector velocity)  { m_vecMoveSpeed = velocity; } // 0x441130
    void  ResetMoveSpeed()               { SetVelocity(CVector{}); }

    CVector& GetTurnSpeed() { return m_vecTurnSpeed; }
    const CVector& GetTurnSpeed() const { return m_vecTurnSpeed; }
    void ResetTurnSpeed() { m_vecTurnSpeed = CVector(); }

    void ResetFrictionMoveSpeed() { m_vecFrictionMoveSpeed = CVector(); }
    void ResetFrictionTurnSpeed() { m_vecFrictionTurnSpeed = CVector(); }

    float GetMass() const { return m_fMass; }
    // TODO: needs CrossProduct() from core math (verify against decomp)
    [[nodiscard]] float GetMass(const CVector& pos, const CVector& dir) const;

// HELPERS
    [[nodiscard]] bool IsImmovable() const { return physicalFlags.bDisableZ || physicalFlags.bInfiniteMass || physicalFlags.bDisableMoveForce; }

    // TODO: std::span is C++20; restore this helper when the project moves past C++17
    // (or return a pointer + count pair).
    auto GetCollidingEntities() const { return std::span{ m_apCollidedEntities, m_nNumEntitiesCollided }; } // restored 2026-10-09 for CAutomobile (project is C++20)

    // TODO: needs CColModel definition (collision subsystem)
    // const auto& GetBoundingBox() { return GetColModel()->m_boundBox; }
};

// TODO: static_assert(sizeof(CPhysical) == 0x138) once CEntity layout is verified.
