// CShadows - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Shadows.h
// Decompiled bodies: src/CShadows/*.c
// TODO: verify each method against decomp.

#pragma once

#include "CVector.h"     // CVector, CVector2D, RwV3d
#include "CMatrix.h"     // RwMatrix
#include "RenderTypes.h" // RwTexture, RwImVertexIndex, RwUInt8, RwUInt32, RwBool

#include <array>
#include <cstdint>

class CEntity;
class CPhysical;
class CVehicle;
class CPed;
class CRealTimeShadow; // port with the shadow-map subsystem

/*
 Shadow rectangle:
         Front
 +---------+---------+
 |         |         |
 |         |         |
 |         | Posn    |
 +---------+---------+ Side
 |         |         |
 |         |         |
 |         |         |
 +---------+---------+

 Posn - world coordinates (x,y,z)
 Front - local coordinates (x,y) relatively to center
 Side - local coordinates (x,y) relatively to center
*/

enum eShadowType : uint8_t {
    SHADOW_NONE     = 0,
    SHADOW_DEFAULT  = 1,
    SHADOW_ADDITIVE = 2,
    SHADOW_INVCOLOR = 3,
    SHADOW_OIL_1    = 4,
    SHADOW_OIL_2    = 5, // Oil on fire
    SHADOW_OIL_3    = 6,
    SHADOW_OIL_4    = 7,
    SHADOW_OIL_5    = 8
};

enum eShadowTextureType {
    SHADOW_TEX_CAR = 1,
    SHADOW_TEX_PED = 2,
    SHADOW_TEX_EXPLOSION = 3,
    SHADOW_TEX_HELI = 4,
    SHADOW_TEX_HEADLIGHTS = 5,
    SHADOW_TEX_BLOOD = 6
};

// bruh, OG name
enum VEH_SHD_TYPE {
    VEH_SHD_CAR = 0,
    VEH_SHD_BIKE = 1,
    VEH_SHD_HELI = 2,
    VEH_SHD_PLANE = 3,
    VEH_SHD_RC = 4,
    VEH_SHD_BIG_PLANE = 5 // AT400; ANDROM
};

// CPolyBunch - adapted from gta-reversed/source/game_sa/PolyBunch.h
// (shadow-owned stand-in; full conversion with the shadows subsystem helpers.
// NOTSA GetVerts() dropped: it needs rng::views, which is not in this build.)
class CPolyBunch {
public:
    std::array<CVector, 7> m_avecPosn;
    CPolyBunch*            m_pNext;
    int16_t                m_wNumVerts; // 100% it's signed
    std::array<uint8_t, 7> m_aU; /// Divide by 200 to get the actual coords
    std::array<uint8_t, 7> m_aV; /// Divide by 200 to get the actual coords
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CPolyBunch) == 0x68, "CPolyBunch layout changed");
#endif

class CRegisteredShadow {
public:
    CVector          m_vecPosn;
    CVector2D        m_Front;
    CVector2D        m_Side;
    float            m_fZDistance;
    float            m_fScale;
    RwTexture*       m_pTexture;
    CRealTimeShadow* m_pRTShadow;
    uint16_t         m_nIntensity;
    eShadowType      m_nType; // TODO: Check if this is the correct type...
    uint8_t          m_nRed;
    uint8_t          m_nGreen;
    uint8_t          m_nBlue;

    uint8_t m_bDrawOnWater : 1;
    uint8_t m_bAlreadyRenderedInBatch : 1;          /// Whenever it has been rendered. Reset each frame, used for batching
    uint8_t m_bDrawOnBuildings : 1;
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CRegisteredShadow) == 0x34, "CRegisteredShadow layout changed");
#endif

class CPermanentShadow {
public:
    CVector    m_vecPosn;
    float      m_fFrontX;
    float      m_fFrontY;
    float      m_fSideX;
    float      m_fSideY;
    float      m_fZDistance;
    float      m_fScale;
    uint32_t   m_nTimeCreated;
    uint32_t   m_nTimeDuration;
    RwTexture* m_pTexture;
    uint16_t   m_nIntensity;
    eShadowType m_nType;
    uint8_t    m_nRed;
    uint8_t    m_nGreen;
    uint8_t    m_nBlue;

    uint8_t m_bDrawOnWater : 1;
    uint8_t m_bIgnoreMapObjects : 1;
    uint8_t m_bDrawOnBuildings : 1;

    void Init() {
        m_nType = SHADOW_NONE;
    }
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CPermanentShadow) == 0x38, "CPermanentShadow layout changed");
#endif

class CStaticShadow {
public:
    uint32_t      m_nId;
    CPolyBunch*   m_pPolyBunch;
    uint32_t      m_nTimeCreated;
    CVector       m_vecPosn;
    float         m_fFrontX;
    float         m_fFrontY;
    float         m_fSideX;
    float         m_fSideY;
    float         m_fZDistance;
    float         m_fScale;
    RwTexture*    m_pTexture;
    uint16_t      m_nIntensity;
    eShadowType   m_nType;
    uint8_t       m_nRed;
    uint8_t       m_nGreen;
    uint8_t       m_nBlue;
    bool          m_bJustCreated;
    bool          m_bRendered;
    bool          m_bTemporaryShadow; // delete after 5000ms
    union {
        struct {
            uint8_t m_nDayIntensity : 4;
            uint8_t m_nNightIntensity : 4;
        };
        uint8_t m_nDayNightIntensity;
    };

public:
    void Init() {
        m_nId = 0;
        m_pPolyBunch = nullptr;
    }
    void Free();
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CStaticShadow) == 0x40, "CStaticShadow layout changed");
#endif

struct _ProjectionParam {
    RwV3d    at;           // Camera at vector
    RwMatrix invMatrix;    // Transforms to shadow camera space
    RwUInt8  shadowValue;  // Shadow opacity value
    RwBool   fade;         // Shadow fades with distance
    RwUInt32 numIm3DBatch; // Number of buffer flushes. Unused
    RwMatrix entityMatrix;
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(_ProjectionParam) == 0x98, "_ProjectionParam layout changed");
#endif

constexpr uint32_t MAX_STORED_SHADOWS       = 48;
constexpr uint32_t MAX_PERMANENT_SHADOWS    = 48;
constexpr uint32_t MAX_STATIC_SHADOWS       = 48;
constexpr uint32_t MAX_SHADOW_POLY_BUNCHES  = 360;

class CShadows {
public:
    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments. Definitions in CShadows.cpp.
    // TODO: re-resolve for the clean-room build.
    static uint16_t ShadowsStoredToBeRendered;                            // 0xC403DC
    static CPolyBunch* pEmptyBunchList;                                  // 0xC403D8
    static std::array<CPolyBunch, MAX_SHADOW_POLY_BUNCHES> aPolyBunches;  // 0xC40DF0
    static std::array<CStaticShadow, MAX_STATIC_SHADOWS> aStaticShadows; // 0xC4A030
    static std::array<CPermanentShadow, MAX_PERMANENT_SHADOWS> aPermanentShadows; // 0xC4AC30
    static std::array<CRegisteredShadow, MAX_STORED_SHADOWS> asShadowsStored;    // 0xC40430

public:
    static void Init();
    static void Shutdown();
    static void TidyUpShadows();

    static void AddPermanentShadow(uint8_t type, RwTexture* texture, CVector* posn, float topX, float topY, float rightX, float rightY, int16_t intensity, uint8_t red, uint8_t greeb, uint8_t blue, float drawDistance, uint32_t time, float upDistance);
    static void UpdatePermanentShadows();

    static void StoreShadowToBeRendered(uint8_t type, const CVector& posn, float frontX, float frontY, float sideX, float sideY, int16_t intensity, uint8_t red, uint8_t green, uint8_t blue);
    static void StoreShadowToBeRendered(uint8_t type, RwTexture* texture, const CVector& posn, float topX, float topY, float rightX, float rightY, int16_t intensity, uint8_t red, uint8_t green, uint8_t blue, float zDistance, bool drawOnWater, float scale, CRealTimeShadow* realTimeShadow, bool drawOnBuildings);
    // NOTSA
    static void StoreShadowToBeRendered(eShadowType type, RwTexture* tex, const CVector& posn, CVector2D top, CVector2D right, int16_t intensity, uint8_t red, uint8_t green, uint8_t blue, float zDistance, bool drawOnWater, float scale, CRealTimeShadow* realTimeShadow, bool drawOnBuildings);
    static void SetRenderModeForShadowType(eShadowType type);
    static void RemoveOilInArea(float x1, float x2, float y1, float y2);
    static void GunShotSetsOilOnFire(const CVector& shotOrigin, const CVector& shotTarget);
    static void PrintDebugPoly(CVector* a, CVector* b, CVector* c);
    static void CalcPedShadowValues(
        CVector sunPosn,
        float& frontX,        float& frontY,
        float& sideX,         float& sideY,
        float& displacementX, float& displacementY
    );
    static void AffectColourWithLighting(eShadowType shadowType,
        uint8_t dayNightIntensity,
        uint8_t red, uint8_t green, uint8_t blue,
        uint8_t& outRed, uint8_t& outGreen, uint8_t& outBlue
    );
    static void StoreShadowForPedObject(CPed* ped, float displacementX, float displacementY, float frontX, float frontY, float sideX, float sideY);
    static void StoreRealTimeShadow(CPhysical* physical, float displacementX, float displacementY, float frontX, float frontY, float sideX, float sideY);
    /*!
    * @addr 0x707F40
    */
    static void UpdateStaticShadows();
    static void RenderExtraPlayerShadows();

    /*!
    * @addr 0x708300
    * @brief Render all active static shadows
    */
    static void RenderStaticShadows();

    static void CastShadowEntityXY(CEntity* entity, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16_t intensity, uint8_t red, uint8_t green, uint8_t blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, uint8_t* pDayNightIntensity, int32_t shadowType);
    static void CastShadowEntityXYZ(CEntity* entity, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16_t intensity, uint8_t red, uint8_t green, uint8_t blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, CRealTimeShadow* realTimeShadow);
    template<typename PtrListType>
    static void CastPlayerShadowSectorList(PtrListType& ptrList, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16_t intensity, uint8_t red, uint8_t green, uint8_t blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, uint8_t* pDayNightIntensity, int32_t shadowType);
    template<typename PtrListType>
    static void CastShadowSectorList(PtrListType& ptrList, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16_t intensity, uint8_t red, uint8_t green, uint8_t blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, uint8_t* pDayNightIntensity, int32_t shadowType);
    template<typename PtrListType>
    static void CastRealTimeShadowSectorList(PtrListType& ptrList, float conrerAX, float cornerAY, float cornerBX, float cornerBY, CVector* posn, float frontX, float frontY, float sideX, float sideY, int16_t intensity, uint8_t red, uint8_t green, uint8_t blue, float zDistance, float scale, CPolyBunch** ppPolyBunch, CRealTimeShadow* realTimeShadow, uint8_t* pDayNightIntensity);
    static void RenderStoredShadows();
    static void GeneratePolysForStaticShadow(int16_t staticShadowIndex);
    static bool StoreStaticShadow(uint32_t id, eShadowType type, RwTexture* texture, const CVector& posn, float frontX, float frontY, float sideX, float sideY, int16_t intensity, uint8_t red, uint8_t green, uint8_t blue, float zDistane, float scale, float drawDistance, bool temporaryShadow, float upDistance);
    static void StoreShadowForVehicle(CVehicle* vehicle, VEH_SHD_TYPE vehShadowType);
    static void StoreCarLightShadow(CVehicle* vehicle, int32_t id, RwTexture* texture, const CVector& posn, float frontX, float frontY, float sideX, float sideY, uint8_t red, uint8_t green, uint8_t blue, float maxViewAngle);
    static void StoreShadowForPole(CEntity* entity, float offsetX, float offsetY, float offsetZ, float poleHeight, float poleWidth, uint32_t localId);
    static void RenderIndicatorShadow(uint32_t id, eShadowType, RwTexture* texture, const CVector& posn, float frontX, float frontY, float sideX, float sideY, int16_t intensity);
};

#ifdef _MSC_VER
CVector* ShadowRenderTriangleCB(CVector* normal, CVector* trianglePos, _ProjectionParam* param);
#endif

constexpr float MAX_DISTANCE_PED_SHADOWS = 15.0f; // 0x8D5240 - TODO: Rename to `MAX_DISTANCE_SHADOWS`
constexpr float MAX_DISTANCE_PED_SHADOWS_SQR = MAX_DISTANCE_PED_SHADOWS * MAX_DISTANCE_PED_SHADOWS; // 0xC4B6B0 - TODO: Rename to `MAX_DISTANCE_PED_SHADOWS_SQ`

// Clean-room: these were plugin-sdk address-bound file-scope globals in
// Shadows.cpp; converted to plain externs. Definitions in CShadows.cpp.
// TODO: re-resolve for the clean-room build.
extern RwTexture* gpShadowCarTex;        // 0xC403E0
extern RwTexture* gpShadowPedTex;        // 0xC403E4
extern RwTexture* gpShadowHeliTex;       // 0xC403E8
extern RwTexture* gpShadowBikeTex;       // 0xC403EC
extern RwTexture* gpShadowBaronTex;      // 0xC403F0
extern RwTexture* gpShadowExplosionTex;  // 0xC403F4
extern RwTexture* gpShadowHeadLightsTex; // 0xC403F8
extern RwTexture* gpShadowHeadLightsTex2; // 0xC403FC
extern RwTexture* gpBloodPoolTex;        // 0xC40400
extern RwTexture* gpHandManTex;          // 0xC40404
extern RwTexture* gpCrackedGlassTex;     // 0xC40408
extern RwTexture* gpPostShadowTex;       // 0xC4040C

extern std::array<RwImVertexIndex, 24> g_ShadowVertices; // 0xC403A8
