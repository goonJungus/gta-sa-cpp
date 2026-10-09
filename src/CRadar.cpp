// CRadar.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations.
// Decompiled reference: src/CRadar/*.c
// Logic cross-checked against gta-reversed/source/game_sa/Radar.cpp (public source,
// itself annotated from the same binary). Where gta-reversed only stubs a method
// via plugin::Call (ClipRadarPoly, DrawEntityBlip), the body below is converted
// directly from the decompiled .c.

#include "CRadar.h"

#include <algorithm> // std::min, std::max, std::clamp
#include <cmath>     // std::sin, std::cos, std::sqrt, std::floor, std::ceil, std::round, std::abs
#include <cstdint>
#include <cstdio>    // std::snprintf
#include <utility>   // std::pair, std::make_pair

// Ported subsystem headers used by the filled bodies below.
#include "CFont.h"
#include "CTimer.h"
#include "CTxdStore.h"
#include "CStreaming.h"   // RequestTxdModel/RemoveTxdModel/RequestModel/RemoveModel, TXDToModelId
#include "CMenuManager.h" // FrontEndMenuManager
#include "CCamera.h"      // TheCamera, GetActiveCamera, LOOKING_DIRECTION_*
#include "CPad.h"         // CPad::IsMouseLButton

// Static member definitions.
// Original GTA SA 1.0 addresses (from gta-reversed StaticRef) kept as comments.
// TODO: re-resolve these for the clean-room build.
float CRadar::m_fRadarOrientation = 0.0f; // 0xBA8310
float CRadar::cachedCos = 1.0f;           // 0xBA8308
float CRadar::cachedSin = 0.0f;           // 0xBA830C

// Blip sprite texture/mask file names, indexed by eRadarSprite (0..63).
// Names from gta-reversed Radar.cpp; RADAR_SPRITE_TORENO (64) has no entry.
SpriteFileName CRadar::RadarBlipFileNames[CRadar::MAX_RADAR_SPRITES] = {
    // name,                 maskName
    { nullptr,               nullptr }, // 0  RADAR_SPRITE_NONE
    { nullptr,               nullptr }, // 1  RADAR_SPRITE_WHITE
    { "radar_centre",        nullptr }, // 2  RADAR_SPRITE_CENTRE
    { "arrow",               nullptr }, // 3  RADAR_SPRITE_MAP_HERE
    { "radar_north",         nullptr }, // 4  RADAR_SPRITE_NORTH
    { "radar_airYard",       nullptr }, // 5  RADAR_SPRITE_AIRYARD
    { "radar_ammugun",       nullptr }, // 6  RADAR_SPRITE_AMMUGUN
    { "radar_barbers",       nullptr }, // 7  RADAR_SPRITE_BARBERS
    { "radar_BIGSMOKE",      nullptr }, // 8  RADAR_SPRITE_BIGSMOKE
    { "radar_boatyard",      nullptr }, // 9  RADAR_SPRITE_BOATYARD
    { "radar_burgerShot",    nullptr }, // 10 RADAR_SPRITE_BURGERSHOT
    { "radar_bulldozer",     nullptr }, // 11 RADAR_SPRITE_BULLDOZER
    { "radar_CATALINAPINK",  nullptr }, // 12 RADAR_SPRITE_CATALINAPINK
    { "radar_CESARVIAPANDO", nullptr }, // 13 RADAR_SPRITE_CESARVIAPANDO
    { "radar_chicken",       nullptr }, // 14 RADAR_SPRITE_CHICKEN
    { "radar_CJ",            nullptr }, // 15 RADAR_SPRITE_CJ
    { "radar_CRASH1",        nullptr }, // 16 RADAR_SPRITE_CRASH1
    { "radar_diner",         nullptr }, // 17 RADAR_SPRITE_DINER
    { "radar_emmetGun",      nullptr }, // 18 RADAR_SPRITE_EMMETGUN
    { "radar_enemyAttack",   nullptr }, // 19 RADAR_SPRITE_ENEMYATTACK
    { "radar_fire",          nullptr }, // 20 RADAR_SPRITE_FIRE
    { "radar_girlfriend",    nullptr }, // 21 RADAR_SPRITE_GIRLFRIEND
    { "radar_hostpitaL",     nullptr }, // 22 RADAR_SPRITE_HOSTPITAL
    { "radar_LocoSyndicate", nullptr }, // 23 RADAR_SPRITE_LOGOSYNDICATE
    { "radar_MADDOG",        nullptr }, // 24 RADAR_SPRITE_MADDOG
    { "radar_mafiaCasino",   nullptr }, // 25 RADAR_SPRITE_MAFIACASINO
    { "radar_MCSTRAP",       nullptr }, // 26 RADAR_SPRITE_MCSTRAP
    { "radar_modGarage",     nullptr }, // 27 RADAR_SPRITE_MODGARAGE
    { "radar_OGLOC",         nullptr }, // 28 RADAR_SPRITE_OGLOC
    { "radar_pizza",         nullptr }, // 29 RADAR_SPRITE_PIZZA
    { "radar_police",        nullptr }, // 30 RADAR_SPRITE_POLICE
    { "radar_propertyG",     nullptr }, // 31 RADAR_SPRITE_PROPERTYG
    { "radar_propertyR",     nullptr }, // 32 RADAR_SPRITE_PROPERTYR
    { "radar_race",          nullptr }, // 33 RADAR_SPRITE_RACE
    { "radar_RYDER",         nullptr }, // 34 RADAR_SPRITE_RYDER
    { "radar_saveGame",      nullptr }, // 35 RADAR_SPRITE_SAVEGAME
    { "radar_school",        nullptr }, // 36 RADAR_SPRITE_SCHOOL
    { "radar_qmark",         nullptr }, // 37 RADAR_SPRITE_QMARK
    { "radar_SWEET",         nullptr }, // 38 RADAR_SPRITE_SWEET
    { "radar_tattoo",        nullptr }, // 39 RADAR_SPRITE_TATTOO
    { "radar_THETRUTH",      nullptr }, // 40 RADAR_SPRITE_THETRUTH
    { "radar_waypoint",      nullptr }, // 41 RADAR_SPRITE_WAYPOINT
    { "radar_TorenoRanch",   nullptr }, // 42 RADAR_SPRITE_TORENORANCH
    { "radar_triads",        nullptr }, // 43 RADAR_SPRITE_TRIADS
    { "radar_triadsCasino",  nullptr }, // 44 RADAR_SPRITE_TRIADSCASINO
    { "radar_tshirt",        nullptr }, // 45 RADAR_SPRITE_TSHIRT
    { "radar_WOOZIE",        nullptr }, // 46 RADAR_SPRITE_WOOZIE
    { "radar_ZERO",          nullptr }, // 47 RADAR_SPRITE_ZERO
    { "radar_dateDisco",     nullptr }, // 48 RADAR_SPRITE_DATEDISCO
    { "radar_dateDrink",     nullptr }, // 49 RADAR_SPRITE_DATEDRINK
    { "radar_dateFood",      nullptr }, // 50 RADAR_SPRITE_DATEFOOD
    { "radar_truck",         nullptr }, // 51 RADAR_SPRITE_TRUCK
    { "radar_cash",          nullptr }, // 52 RADAR_SPRITE_CASH
    { "radar_flag",          nullptr }, // 53 RADAR_SPRITE_FLAG
    { "radar_gym",           nullptr }, // 54 RADAR_SPRITE_GYM
    { "radar_impound",       nullptr }, // 55 RADAR_SPRITE_IMPOUND
    { "radar_light",         nullptr }, // 56 RADAR_SPRITE_LIGHT
    { "radar_runway",        nullptr }, // 57 RADAR_SPRITE_RUNWAY
    { "radar_gangB",         nullptr }, // 58 RADAR_SPRITE_GANGB
    { "radar_gangP",         nullptr }, // 59 RADAR_SPRITE_GANGP
    { "radar_gangY",         nullptr }, // 60 RADAR_SPRITE_GANGY
    { "radar_gangN",         nullptr }, // 61 RADAR_SPRITE_GANGN
    { "radar_gangG",         nullptr }, // 62 RADAR_SPRITE_GANGG
    { "radar_spray",         nullptr }, // 63 RADAR_SPRITE_SPRAY
};

float CRadar::m_radarRange = 2990.0f; // 0xBA8314
std::array<int16_t, CRadar::MAX_RADAR_TRACES> CRadar::MapLegendList{}; // 0xBA8318
uint16_t CRadar::MapLegendCounter = 0;                 // 0xBA86B8
std::array<CRGBA, 6> CRadar::ArrowBlipColour{};       // 0xBA86D4
std::array<tRadarTrace, CRadar::MAX_RADAR_TRACES> CRadar::ms_RadarTrace{}; // 0xBA86F0
CVector2D CRadar::vec2DRadarOrigin{};                 // 0xBAA248
std::array<CSprite2d, CRadar::MAX_RADAR_SPRITES> CRadar::RadarBlipSprites{}; // 0xBAA250
CRect CRadar::m_radarRect{};                          // 0x8D0920

eAirstripLocation CRadar::airstrip_location = AIRSTRIP_LS_AIRPORT; // 0xBA8300
int32_t CRadar::airstrip_blip = 0;                                // 0xBA8304

// ============================================================================
// TODO(port): external subsystem shims.
// Minimal declarations for subsystems not yet ported to cpp/. Each entry is
// verified against gta-reversed/source/game_sa and the decompiled bodies in
// src/CRadar/*.c. Delete entries as their subsystem lands; do not grow
// this list. None of these introduce link-time dependencies for -fsyntax-only
// or `ar` static-library builds.
// ============================================================================

// --- Screen scaling (decomp evidence: X factors use 1/640 = 0.0015625,
// Y factors use 1/448 ~= 0.002232143, e.g. DrawLegend_005828a0.c).
// TODO(port): verify exact SCALE-vs-STRETCH semantics against the render layer.
struct RsGlobalType {
    int32_t maximumWidth{};
    int32_t maximumHeight{};
};
extern RsGlobalType RsGlobal;
#define SCREEN_WIDTH  ((float)RsGlobal.maximumWidth)
#define SCREEN_HEIGHT ((float)RsGlobal.maximumHeight)
#define SCREEN_STRETCH_X(x) ((float)(x) * SCREEN_WIDTH / 640.0f)
#define SCREEN_STRETCH_Y(y) ((float)(y) * SCREEN_HEIGHT / 448.0f)
#define SCREEN_STRETCH_FROM_RIGHT(x)  (SCREEN_WIDTH - SCREEN_STRETCH_X(x))
#define SCREEN_STRETCH_FROM_BOTTOM(y) (SCREEN_HEIGHT - SCREEN_STRETCH_Y(y))
constexpr float DEFAULT_SCREEN_WIDTH  = 640.0f;
constexpr float DEFAULT_SCREEN_HEIGHT = 448.0f;

// --- RenderWare render-state shims (no RW layer in this build yet).
// The bodies only ever set states / issue 2D primitives; values are passed
// through opaquely.
using RwRenderState = int32_t;
inline void RwRenderStateSet(RwRenderState, uint32_t) {}
#define RWRSTATE(x) (x)
enum {
    rwRENDERSTATETEXTUREFILTER = 0,
    rwRENDERSTATEVERTEXALPHAENABLE,
    rwRENDERSTATEFOGENABLE,
    rwRENDERSTATESRCBLEND,
    rwRENDERSTATEDESTBLEND,
    rwRENDERSTATETEXTUREADDRESS,
    rwRENDERSTATETEXTURERASTER,
    rwRENDERSTATESHADEMODE,
    rwRENDERSTATEALPHATESTFUNCTION,
    rwRENDERSTATEALPHATESTFUNCTIONREF,
    rwRENDERSTATEZTESTENABLE,
    rwRENDERSTATEZWRITEENABLE,
    rwRENDERSTATETEXTUREPERSPECTIVE,
};
enum {
    rwBLENDSRCALPHA = 2,
    rwBLENDINVSRCALPHA,
    rwFILTERLINEAR,
    rwSHADEMODEFLAT,
    rwTEXTUREADDRESSCLAMP,
    rwALPHATESTFUNCTIONALWAYS,
    rwALPHATESTFUNCTIONGREATER,
};
#define D3DSHADE_FLAT rwSHADEMODEFLAT
#define D3DTADDRESS_CLAMP rwTEXTUREADDRESSCLAMP
inline void RwIm2DRenderPrimitive(int32_t /*primType*/, void* /*vertices*/, int32_t /*numVerts*/) {}
enum { rwPRIMTYPETRIFAN = 4 };

// --- Math constants (gta-reversed common.h).
#ifndef M_PI
constexpr float M_PI = 3.14159265358979323846f;
#endif
constexpr float PI       = M_PI;
constexpr float TWO_PI   = 2.0f * M_PI;
constexpr float HALF_PI  = 0.5f * M_PI;
constexpr float FRAC_PI_2 = HALF_PI;
constexpr float FRAC_PI_4 = 0.25f * M_PI;
constexpr float SQRT_2   = 1.4142135623730950488f;
inline float DegreesToRadians(float deg) { return deg * (PI / 180.0f); }
inline float RadiansToDegrees(float rad) { return rad * (180.0f / PI); }

// --- CVector2D arithmetic (cpp/CVector.h keeps CVector2D minimal).
// TODO(port): move to CVector.h when converted.
inline CVector2D operator-(const CVector2D& a, const CVector2D& b) { return CVector2D{a.x - b.x, a.y - b.y}; }
inline CVector2D operator+(const CVector2D& a, const CVector2D& b) { return CVector2D{a.x + b.x, a.y + b.y}; }
inline CVector2D operator*(const CVector2D& v, float s) { return CVector2D{v.x * s, v.y * s}; }
inline CVector2D operator/(const CVector2D& v, float s) { return CVector2D{v.x / s, v.y / s}; }
inline CVector2D& operator*=(CVector2D& v, const CVector2D& o) { v.x *= o.x; v.y *= o.y; return v; }
inline float CVector2DMagnitude(const CVector2D& v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline void CVector2DNormalise(CVector2D& v) {
    const float m = CVector2DMagnitude(v);
    if (m > 0.0f) { v.x /= m; v.y /= m; }
}

// --- CRGBA from packed 0xRRGGBBAA (decomp evidence: DrawEntityBlip_00587000.c
// passes uVar14>>24, uVar14>>16, uVar14>>8, uVar14 as R,G,B,A to
// ShowRadarTraceWithHeight).
// TODO(port): move to RenderTypes.h when converted.
inline CRGBA CRGBAFromInt(uint32_t c) {
    return CRGBA{
        static_cast<uint8_t>(c >> 24),
        static_cast<uint8_t>(c >> 16),
        static_cast<uint8_t>(c >> 8),
        static_cast<uint8_t>(c),
    };
}

// --- 1.0 float->int casts compile to FPU fistp (round-to-nearest).
// (Decomp evidence: CGeneral::unk_00821b40 in DrawLegend_005828a0.c etc.)
inline int32_t FloatToInt(float v) { return static_cast<int32_t>(std::nearbyint(v)); }

// --- Text / GXT shims.
class CText {
public:
    const GxtChar* Get(const char* key);
};
extern CText TheText;

// --- HUD colours (not yet ported). Values from gta-reversed HudColours.h.
enum eHudColour : uint8_t {
    HUD_COLOUR_RED        = 0,
    HUD_COLOUR_GREEN      = 1,
    HUD_COLOUR_DARK_BLUE  = 2,
    HUD_COLOUR_LIGHT_BLUE = 3,
    HUD_COLOUR_LIGHT_GRAY = 4,
    HUD_COLOUR_BLACK      = 5,
    HUD_COLOUR_GOLD       = 6,
    HUD_COLOUR_PURPLE     = 7,
    HUD_COLOUR_DARK_GRAY  = 8,
    HUD_COLOUR_DARK_RED   = 9,
    HUD_COLOUR_DARK_GREEN = 10,
    HUD_COLOUR_CREAM      = 11,
    HUD_COLOUR_NIGHT_BLUE = 12,
    HUD_COLOUR_BLUE       = 13,
    HUD_COLOUR_YELLOW     = 14,
};
struct CHudColours {
    uint32_t GetIntColour(eHudColour c);
    CRGBA GetRGBA(eHudColour c, uint8_t alpha);
    CRGBA GetRGB(eHudColour c);
};
extern CHudColours HudColour;

// --- Entry/exit (CRadar.h only forward-declares CEntryExit).
struct CEntryExit {
    void GetPositionRelativeToOutsideWorld(CVector& pos) const;
};
struct CEntryExitManager {
    static CEntryExit* GetInSlot(int32_t slot);
    struct Pool { int32_t GetIndex(CEntryExit* e); bool IsIndexInBounds(int32_t i); bool IsFreeSlotAtIndex(int32_t i); };
    static Pool* GetPool();
};

// --- Save-game storage (not yet ported).
struct CGenericGameStorage {
    template<typename T> static void LoadDataFromWorkBuffer(T& data);
    static void SaveDataToWorkBuffer(const void* data, uint32_t size);
};

// --- Minimal entity interfaces used by radar logic (full ports not yet in cpp/).
// TODO(port): replace with CEntity/CPed/CVehicle/CObject ports.
class CEntityShim {
public:
    virtual ~CEntityShim() = default;
};
class CPed {
public:
    int32_t GetAreaCode() const;
    bool IsHidden() const;
    CVector GetRealPosition() const;
    CVector GetPosition() const;
    float GetHeading() const;
};
class CVehicle {
public:
    bool IsSubPlane() const;
    int32_t m_nModelIndex{};
    CVector GetPosition() const;
    float GetHeading() const;
    struct Matrix { CVector GetForward() const; CVector GetUp() const; };
    Matrix* m_matrix{};
};
class CObject {
public:
    CVector GetPosition() const;
    float GetHeading() const;
    struct BBox { CVector m_vecMax{}; };
    struct ColModel { BBox GetBoundingBox() const; };
    const ColModel* GetColModel() const;
};
class CCamShim {
public:
    int32_t m_nMode{};
    CEntityShim* m_pCamTargetEntity{};
    CVector m_vecSourceBeforeLookBehind{};
};
template<typename T> struct CPoolShim {
    T* GetAtRef(int32_t handle);
    int32_t GetIndex(T* entity);
};
CPoolShim<CVehicle>* GetVehiclePool();
CPoolShim<CPed>* GetPedPool();
CPoolShim<CObject>* GetObjectPool();
struct ModelIndices { static bool IsNevada(int32_t model); static bool IsVortex(int32_t model); };

// --- Player accessors (not yet ported).
CPed* FindPlayerPed(int32_t playerIndex = 0);
CVehicle* FindPlayerVehicle(int32_t playerIndex = 0);
CVector FindPlayerCentreOfWorldForMap(int32_t playerIndex);
float FindPlayerHeading(int32_t playerIndex);
CVector FindPlayerSpeed(int32_t playerIndex = 0);
constexpr int32_t PED_TYPE_PLAYER1 = 0;
constexpr int32_t AREA_CODE_NORMAL_WORLD = 0;

// TODO(port): CPlayerInfo/CWorld::Players need the player port.
struct CPlayerInfo {
    CPed* m_pPed{};
};
struct CWorld {
    static CPlayerInfo Players[2];
};
struct CPlayerPed {
    static bool IsHidden(const CPed* ped);
};

// --- Camera helpers not in cpp/CCamera.h yet.
inline float CCamera_GetHeading() { return 0.0f; } // TODO(port): TheCamera.GetHeading()

// --- Script state (not yet ported). Field names verified against
// gta-reversed/source/game_sa/Scripts.h and src/CRadar/*.c usage.
struct tScriptSearchlight {
    CVector m_Target{};
    float m_fTargetRadius{};
};
constexpr int32_t SCRIPT_THING_SEARCH_LIGHT = 6; // TODO(port): verify eScriptThing value
struct CTheScripts {
    static bool HideAllFrontEndMapBlips;
    static bool bPlayerIsOffTheMap;
    static uint32_t RadarZoomValue;
    static bool RadarShowBlipOnAllLevels;
    static bool IsPlayerOnAMission();
    static void ScriptDebugLine3D(const CVector& p1, const CVector& p2, uint32_t c1, uint32_t c2);
    static int32_t GetActualScriptThingIndex(int32_t handle, int32_t type);
    static tScriptSearchlight* ScriptSearchLightArray;
};

// --- Zones (not yet ported).
struct CTheZones {
    static int32_t ZonesRevealed;
    static bool GetCurrentZoneLockedOrUnlocked(const CVector& pos);
    struct Zone { CRect GetRect() const; };
    struct ZoneInfo { int32_t RadarMode{}; CRGBA ZoneColor{}; };
    static Zone* GetNavigationZones();
    static ZoneInfo* GetZoneInfo(Zone* zone);
};

// --- Game state (not yet ported).
struct CGame {
    static int32_t currArea;
    static bool CanSeeOutSideFromCurrArea();
};
struct CGameLogic {
    static bool IsCoopGameGoingOn();
    static int32_t n2PlayerPedInFocus;
};

// --- Pickups (not yet ported).
struct CPickupShim { CVector GetPosn() const; };
struct CPickups {
    static int32_t GetActualPickupIndex(int32_t handle);
    static CPickupShim* aPickUps;
};

// --- 3D markers (not yet ported).
struct C3dMarkers {
    static void PlaceMarkerCone(uint32_t id, const CVector& pos, float size,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                uint32_t p6, float p7, int32_t p8, bool p9);
    static void PlaceMarkerSet(uint32_t id, int32_t type, const CVector& pos, float size,
                               uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                               uint32_t p9, float p10, int32_t p11);
};
constexpr int32_t MARKER3D_CYLINDER = 1;

// --- Gang wars (not yet ported).
struct CGangWars {
    static bool bGangWarsActive;
    static bool CanPlayerStartAGangWarHere(const CTheZones::ZoneInfo* info);
};

// --- Texture helper (not yet ported).
RwTexture* GetFirstTexture(RwTexDictionary* txd);

// --- notsa helpers.
namespace notsa {
template<typename C, typename T> bool contains(const C& c, const T& v) {
    for (const auto& e : c) if (e == v) return true;
    return false;
}
inline bool IsFixBugs() { return false; } // TODO(port): NOTSA fix-bugs toggle
}

// --- Streaming flags (gta-reversed Streaming.h).
constexpr int32_t STREAMING_GAME_REQUIRED = 1;
constexpr int32_t STREAMING_KEEP_IN_MEMORY = 2;

// --- Airstrip table (gta-reversed Radar.cpp: 0x8D06E0).
constexpr std::array<airstrip_info, NUM_AIRSTRIPS> airstrip_table{{
    airstrip_info{ { +1750.0f, -2494.0f }, 180.0f, 1000.0f }, // AIRSTRIP_LS_AIRPORT
    airstrip_info{ { -1373.0f, +120.00f }, 315.0f, 1500.0f }, // AIRSTRIP_SF_AIRPORT
    airstrip_info{ { +1478.0f, +1461.0f },  90.0f, 1200.0f }, // AIRSTRIP_LV_AIRPORT
    airstrip_info{ {  +175.0f, +2502.0f }, 180.0f, 1000.0f }, // AIRSTRIP_VERDANT_MEADOWS
}};

// --- Radar texture TXD slots, indexed [y][x] (gta-reversed: 0xBA8478).
static std::array<std::array<int32_t, MAX_RADAR_WIDTH_TILES>, MAX_RADAR_HEIGHT_TILES> gRadarTextures{};

// --- DrawLegend statics (gta-reversed: 0xBAA350/0xBAA354).
static eRadarTraceHeight legendTraceHeight = RADAR_TRACE_LOW; // 0xBAA350
static uint32_t legendTraceTimer = 0;                         // 0xBAA354

// --- DrawYouAreHereSprite statics (gta-reversed: 0xBAA358/0x8D0930).
static uint32_t mapYouAreHereTimer = 0;   // 0xBAA358
static bool mapYouAreHereDisplay = false; // 0x8D0930

// --- DrawRadarGangOverlay statics (gta-reversed: 0xBAA36C/0xBAA35C).
static uint32_t g_RadarGangResetOverlay = 0; // 0xBAA36C
static CRect g_RadarGangOverlay{};           // 0xBAA35C

// --- DrawEntityBlip airstrip statics (decomp: DrawEntityBlip_00587000.c,
// DAT_00baa370/74/78/7c/80).
static uint8_t  s_airstripBlipOnScreen = 0; // 0xBAA370
static uint32_t s_airstripLastTime = 0;     // 0xBAA374
static int32_t  s_airstripDist1 = 0;        // 0xBAA378
static int32_t  s_airstripDist2 = 0;        // 0xBAA37C
static uint32_t s_airstripFlags = 0;        // 0xBAA380

// --- SetupAirstripBlips timer (gta-reversed NOTSA: CTimer::GetTimeInMS()+200).
static uint32_t airstripBlipCounter = 0;

// --- File-scope helpers (gta-reversed: file-static in Radar.cpp).
static void Limit(float& x, float& y) {
    // src/CRadar/ShowRadarTrace_00583f40.c (inlined `Limit` in gta-reversed)
    if (FrontEndMenuManager.m_bDrawingMap) {
        x = SCREEN_STRETCH_X(x);
        y = SCREEN_STRETCH_Y(y);
        CRadar::LimitToMap(x, y);
    }
}

static CVector GetAirStripLocation(eAirstripLocation location) {
    return CVector(airstrip_table[location].position);
}

static float DistanceBetweenPoints2D(const CVector2D& a, const CVector& b) {
    const float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ============================================================
// tRadarTrace methods
// ============================================================

CRGBA tRadarTrace::GetStaticColour() const {
    // NOTSA helper (no direct decomp; logic from gta-reversed Radar.cpp,
    // consistent with DrawCoordBlip_00586d60.c / DrawEntityBlip_00587000.c usage).
    switch (m_nAppearance) {
    case eBlipAppearance::BLIP_FLAG_FRIEND:
        return CRGBAFromInt(CRadar::GetRadarTraceColour(m_nColour, m_bBright, false));
    case eBlipAppearance::BLIP_FLAG_THREAT:
        return HudColour.GetRGB(HUD_COLOUR_BLUE);
    case eBlipAppearance::BLIP_FLAG_UNK:
        return HudColour.GetRGB(HUD_COLOUR_RED);
    default:
        return CRGBA{};
    }
}

CVector tRadarTrace::GetWorldPos() const {
    // NOTSA helper (gta-reversed Radar.cpp; decomp evidence:
    // DrawEntityBlip_00587000.c adjusts local_c via
    // CEntryExit::GetPositionRelativeToOutsideWorld).
    CVector pos = m_vPosition;
    if (m_pEntryExit) {
        m_pEntryExit->GetPositionRelativeToOutsideWorld(pos);
    }
    return pos;
}

std::pair<CVector2D, CVector2D> tRadarTrace::GetRadarAndScreenPos(float* radarPointDist) const {
    // NOTSA helper (gta-reversed Radar.cpp).
    const CVector world = GetWorldPos();

    CVector2D radar = CRadar::TransformRealWorldPointToRadarSpace({world.x, world.y});

    const float dist = CRadar::LimitRadarPoint(radar); // normalises `radar` in place
    if (radarPointDist) {
        *radarPointDist = dist;
    }

    const CVector2D screen = CRadar::TransformRadarPointToScreenSpace(radar);

    return std::make_pair(radar, screen);
}

// ============================================================
// CRadar methods
// ============================================================

void CRadar::Initialise() {
    // src/CRadar/Initialise_00587fb0.c
    airstrip_blip = 0;
    airstrip_location = AIRSTRIP_LS_AIRPORT;

    for (auto& trace : ms_RadarTrace) {
        ClearActualBlip(trace);
    }
    // Decomp also sets m_nCounter = 1 per trace (puVar1[-4] = 1);
    // ClearActualBlip leaves the counter alone, so set it explicitly.
    for (auto& trace : ms_RadarTrace) {
        trace.m_nCounter = 1;
    }

    m_radarRange = 350.0f;

    // Decomp fills gRadarTextures linearly: "radar%02d" for i in 0..143,
    // i.e. gRadarTextures[y][x] with i = y * MAX_RADAR_WIDTH_TILES + x.
    char name[16]{};
    for (int32_t i = 0; i < MAX_RADAR_WIDTH_TILES * MAX_RADAR_HEIGHT_TILES; i++) {
        std::snprintf(name, sizeof(name), "radar%02d", i);
        gRadarTextures[i / MAX_RADAR_WIDTH_TILES][i % MAX_RADAR_WIDTH_TILES] = CTxdStore::FindTxdSlot(name);
    }
}
void CRadar::Shutdown() {
    // src/CRadar/Shutdown_00585940.c
    for (auto& sprite : RadarBlipSprites) {
        sprite.Delete();
    }
    RemoveRadarSections();
}

void CRadar::LoadTextures() {
    // src/CRadar/LoadTextures_005827d0.c
    // Decomp: PushCurrentTxd, SetCurrentTxd("hud" slot), SetTexture(name) for
    // all 64 sprites (mask unused), PopCurrentTxd.
    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("hud"));

    for (auto i = 0u; i < MAX_RADAR_SPRITES; i++) {
        RadarBlipSprites[i].SetTexture(RadarBlipFileNames[i].name); // mask unused
    }

    CTxdStore::PopCurrentTxd();
}

tBlipHandle CRadar::GetNewUniqueBlipIndex(int32_t blipIndex) {
    // src/CRadar/GetNewUniqueBlipIndex_00582820.c (inlined in 1.0)
    auto& trace = ms_RadarTrace[blipIndex];
    if (trace.m_nCounter < 0xFFFE) {
        trace.m_nCounter++;
    } else {
        trace.m_nCounter = 1; // wrap back to 1
    }
    return static_cast<uint32_t>(trace.m_nCounter) << 16 | static_cast<uint32_t>(blipIndex);
}

int32_t CRadar::GetActualBlipArrayIndex(tBlipHandle blip) {
    // src/CRadar/GetActualBlipArrayIndex_00582870.c
    if (blip == 0xFFFFFFFFu) {
        return -1;
    }
    const uint32_t traceIndex = blip & 0xFFFFu;
    const auto& trace = ms_RadarTrace[traceIndex];
    const uint32_t counter = blip >> 16;
    if (counter != trace.m_nCounter || !trace.m_bTrackingBlip) {
        return -1;
    }
    return static_cast<int32_t>(traceIndex);
}

void CRadar::DrawLegend(int32_t x, int32_t y, eRadarSprite blipType) {
    // src/CRadar/DrawLegend_005828a0.c
    // (The 1.0 binary inlines the GetBlipName LG_xx switch here; the mapping
    // below is implemented in GetBlipName and verified against the decomp.)
    CFont::PrintString(
        static_cast<float>(x) + SCREEN_STRETCH_X(20.0f),
        static_cast<float>(y) + SCREEN_STRETCH_Y(3.0f),
        GetBlipName(blipType)
    );

    if (blipType > -1) { // sprite blip: draw the sprite itself
        RadarBlipSprites[blipType].Draw(
            CRect{
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(x) + SCREEN_STRETCH_X(16.0f),
                static_cast<float>(y) + SCREEN_STRETCH_X(16.0f) // NOTE: X stretch, keeps sprites square
            },
            CRGBA{255, 255, 255, 255}
        );
        return;
    }

    // Arrow blip: cycle the arrow shape every 600 ms (decomp: legendTraceHeight
    // 0->1->2->0, legendTraceTimer at 0xBAA350/0xBAA354).
    if (CTimer::GetTimeInMSPauseMode() - legendTraceTimer > 600) {
        legendTraceTimer = CTimer::GetTimeInMSPauseMode();
        legendTraceHeight = (legendTraceHeight == RADAR_TRACE_NORMAL)
            ? RADAR_TRACE_LOW
            : static_cast<eRadarTraceHeight>(legendTraceHeight + 1);
    }

    const float posX = std::round(SCREEN_STRETCH_X(8.0f) + static_cast<float>(x));
    const float posY = std::round(SCREEN_STRETCH_Y(8.0f) + static_cast<float>(y));

    const float x4px = SCREEN_STRETCH_X(4.0f);
    const float x5px = SCREEN_STRETCH_X(5.0f), y5px = SCREEN_STRETCH_Y(5.0f);
    const float x7px = SCREEN_STRETCH_X(7.0f), y7px = SCREEN_STRETCH_Y(7.0f);

    const CRGBA arrowColor = ArrowBlipColour[-blipType];
    const CRGBA outlineColor{0, 0, 0, 255};

    switch (legendTraceHeight) {
    case RADAR_TRACE_LOW: // up-pointing triangle
        CSprite2d::Draw2DPolygon(
            posX - x7px, posY + y7px,
            posX + x7px, posY + y7px,
            posX,        posY - x7px,
            posX,        posY - x7px,
            outlineColor
        );
        CSprite2d::Draw2DPolygon(
            posX + x5px, posY + y5px,
            posX - x5px, posY + y5px,
            posX,        posY - x5px,
            posX,        posY - x5px,
            arrowColor
        );
        break;
    case RADAR_TRACE_HIGH: // down-pointing triangle
        CSprite2d::Draw2DPolygon(
            posX + x7px, posY - y7px,
            posX - x7px, posY - y7px,
            posX,        posY + y7px,
            posX,        posY + y7px,
            outlineColor
        );
        CSprite2d::Draw2DPolygon(
            posX + x5px, posY - y5px,
            posX - x5px, posY - y5px,
            posX,        posY + y5px,
            posX,        posY + y5px,
            arrowColor
        );
        break;
    case RADAR_TRACE_NORMAL: // box
    default:
        CSprite2d::DrawRect(
            CRect{posX - x7px, posY + y7px, posX + x7px, posY - x7px},
            outlineColor
        );
        CSprite2d::DrawRect(
            CRect{posX - x4px, posY + x4px, posX + x4px, posY - x4px},
            arrowColor
        );
        break;
    }
}

float CRadar::LimitRadarPoint(CVector2D& point) {
    // src/CRadar/LimitRadarPoint_005832f0.c
    // Limits the point to the unit circle; returns pre-limit magnitude.
    // Does nothing while the map is being drawn.
    const float mag = CVector2DMagnitude(point);

    if (FrontEndMenuManager.m_bDrawingMap) {
        return mag;
    }

    if (mag > 1.0f) {
        CVector2DNormalise(point);
    }

    return mag;
}

void CRadar::LimitToMap(float& x, float& y) {
    // src/CRadar/LimitToMap_00583350.c
    const float zoom = FrontEndMenuManager.m_bMapLoaded ? FrontEndMenuManager.m_fMapZoom : 140.0f;

    x = std::clamp(x,
        SCREEN_STRETCH_X(FrontEndMenuManager.m_vMapOrigin.x - zoom),
        SCREEN_STRETCH_X(FrontEndMenuManager.m_vMapOrigin.x + zoom));
    y = std::clamp(y,
        SCREEN_STRETCH_Y(FrontEndMenuManager.m_vMapOrigin.y - zoom),
        SCREEN_STRETCH_Y(FrontEndMenuManager.m_vMapOrigin.y + zoom));
}

uint8_t CRadar::CalculateBlipAlpha(float distance) {
    // src/CRadar/CalculateBlipAlpha_00583420.c
    // (Blip transparency is a VC leftover; SA always draws opaque blips on the
    // radar, but the function is still called for map drawing.)
    if (FrontEndMenuManager.m_bDrawingMap) {
        return 255;
    }
    const uint32_t alpha = 255u - static_cast<uint32_t>(distance / 6.0f * 255.0f);
    return static_cast<uint8_t>(std::max(static_cast<float>(alpha), 70.0f));
}

CVector2D CRadar::TransformRadarPointToScreenSpace(const CVector2D& in) {
    // src/CRadar/TransformRadarPointToScreenSpace_00583480.c
    if (FrontEndMenuManager.m_bDrawingMap) {
        return CVector2D{
            FrontEndMenuManager.m_vMapOrigin.x + FrontEndMenuManager.m_fMapZoom * in.x,
            FrontEndMenuManager.m_vMapOrigin.y - FrontEndMenuManager.m_fMapZoom * in.y
        };
    }
    return CVector2D{
        SCREEN_STRETCH_X(94.0f) / 2.0f + SCREEN_STRETCH_X(40.0f) + SCREEN_STRETCH_X(94.0f * in.x) / 2.0f,
        SCREEN_STRETCH_FROM_BOTTOM(104.0f) + SCREEN_STRETCH_Y(76.0f) / 2.0f - SCREEN_STRETCH_Y(76.0f * in.y) / 2.0f
    };
}

CVector2D CRadar::TransformRealWorldPointToRadarSpace(const CVector2D& in) {
    // src/CRadar/TransformRealWorldPointToRadarSpace_00583530.c (inlined in 1.0)
    return CachedRotateClockwise((in - vec2DRadarOrigin) / m_radarRange);
}

CVector2D CRadar::TransformRadarPointToRealWorldSpace(const CVector2D& in) {
    // src/CRadar/TransformRadarPointToRealWorldSpace_005835a0.c (unused in 1.0)
    return CachedRotateCounterclockwise(in) * m_radarRange + vec2DRadarOrigin;
}

CVector2D CRadar::TransformRealWorldToTexCoordSpace(const CVector2D& in, int32_t x, int32_t y) {
    // src/CRadar/TransformRealWorldToTexCoordSpace_00583600.c (inlined, see DrawRadarSection)
    return CVector2D{
        +(in.x - (500.0f * static_cast<float>(x) - 3000.0f)),
        -(in.y - ((500.0f * static_cast<float>(MAX_RADAR_HEIGHT_TILES - y)) - 3000.0f))
    } / 500.0f;
}

void CRadar::CalculateCachedSinCos() {
    // src/CRadar/CalculateCachedSinCos_00583670.c
    if (FrontEndMenuManager.m_bDrawingMap) {
        cachedSin = std::sin(0.0f);
        cachedCos = std::cos(0.0f);
        return;
    }

    const auto SaveAngle = [](float angle) {
        m_fRadarOrientation = angle;
        cachedSin = std::sin(angle);
        cachedCos = std::cos(angle);
    };

    if (TheCamera.GetLookDirection() == LOOKING_DIRECTION_FORWARD) {
        SaveAngle(CCamera_GetHeading()); // TODO(port): TheCamera.GetHeading()
        return;
    }

    // TODO(port): full camera-target branch needs CCam/CEntity ports.
    // Decomp: uses activeCam.m_pCamTargetEntity, m_nMode, MODE_1STPERSON,
    // m_vecSourceBeforeLookBehind to derive the heading.
    SaveAngle(0.0f);
}

tBlipHandle CRadar::SetCoordBlip(eBlipType type, CVector posn, eBlipColour color, eBlipDisplay blipDisplay, const char* scriptName) {
    // src/CRadar/SetCoordBlip_00583820.c
    // (color/scriptName unused in 1.0; the FindTraceNotTrackingBlipIndex loop
    // and counter bump are inlined in the original.)
    const int32_t index = FindTraceNotTrackingBlipIndex();
    if (index == -1) {
        return 0xFFFFFFFFu;
    }

    auto& t = ms_RadarTrace[index];
    t.m_vPosition        = posn;
    // Decomp packs: (type & 0xF) << 2 | (old & 0xC0) | (blipDisplay & 3)
    t.m_nBlipDisplayFlag = blipDisplay;
    t.m_nBlipType        = type;
    t.m_nColour          = BLIP_COLOUR_DESTINATION;
    t.m_fSphereRadius    = 1.0f;
    t.m_nEntityHandle    = 0;
    t.m_nBlipSize        = 1;
    t.m_nBlipSprite      = RADAR_SPRITE_NONE;
    t.m_bBright          = true;
    t.m_bTrackingBlip    = true;
    t.m_pEntryExit       = nullptr;

    (void)color; (void)scriptName;
    return GetNewUniqueBlipIndex(index);
}

tBlipHandle CRadar::SetShortRangeCoordBlip(eBlipType type, CVector posn, eBlipColour color, eBlipDisplay blipDisplay, const char* scriptName) {
    // src/CRadar/SetShortRangeCoordBlip_00583920.c
    const tBlipHandle blip = SetCoordBlip(type, posn, color, blipDisplay, scriptName);
    if (blip != 0xFFFFFFFFu) {
        ms_RadarTrace[GetActualBlipArrayIndex(blip)].m_bShortRange = true;
        return blip;
    }
    return 0xFFFFFFFFu;
}

tBlipHandle CRadar::SetEntityBlip(eBlipType type, int32_t entityHandle, uint32_t arg2, eBlipDisplay blipDisplay) {
    // src/CRadar/SetEntityBlip_005839a0.c (arg2 unused in 1.0)
    const int32_t index = FindTraceNotTrackingBlipIndex();
    if (index == -1) {
        return 0xFFFFFFFFu;
    }

    auto& t = ms_RadarTrace[index];
    t.m_nBlipDisplayFlag = blipDisplay;
    t.m_nColour = (type == BLIP_CHAR || type == BLIP_CAR) ? BLIP_COLOUR_THREAT : BLIP_COLOUR_GREEN;
    t.m_nEntityHandle = static_cast<uint32_t>(entityHandle);
    t.m_fSphereRadius = 1.0f;
    t.m_nBlipSize = 1;
    t.m_nBlipType = type;
    t.m_nBlipSprite = RADAR_SPRITE_NONE;
    t.m_bBright = true;
    t.m_bTrackingBlip = true;
    t.m_pEntryExit = nullptr;

    (void)arg2;
    return GetNewUniqueBlipIndex(index);
}

void CRadar::ChangeBlipColour(tBlipHandle blip, eBlipColour color) {
    // src/CRadar/ChangeBlipColour_00583ab0.c
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1) {
        ms_RadarTrace[index].m_nColour = color;
    }
}

bool CRadar::HasThisBlipBeenRevealed(int32_t blipIndex) {
    // src/CRadar/HasThisBlipBeenRevealed_00583af0.c
    if (!FrontEndMenuManager.m_bDrawingMap
        || !ms_RadarTrace[blipIndex].m_bShortRange
        || CTheZones::ZonesRevealed > 80
        || CTheZones::GetCurrentZoneLockedOrUnlocked(ms_RadarTrace[blipIndex].m_vPosition)) {
        return true;
    }
    return false;
}

bool CRadar::DisplayThisBlip(eRadarSprite spriteId, int8_t priority) {
    // src/CRadar/DisplayThisBlip_00583b40.c
    if (CGame::CanSeeOutSideFromCurrArea() && FindPlayerPed()->GetAreaCode() == AREA_CODE_NORMAL_WORLD) {
        switch (spriteId) {
        case RADAR_SPRITE_NONE:
        case RADAR_SPRITE_WHITE:
        case RADAR_SPRITE_CENTRE:
        case RADAR_SPRITE_MAP_HERE:
        case RADAR_SPRITE_NORTH:
            return true;

        case RADAR_SPRITE_AIRYARD:
        case RADAR_SPRITE_AMMUGUN:
        case RADAR_SPRITE_BARBERS:
        case RADAR_SPRITE_BOATYARD:
        case RADAR_SPRITE_BURGERSHOT:
        case RADAR_SPRITE_BULLDOZER:
        case RADAR_SPRITE_CHICKEN:
        case RADAR_SPRITE_DINER:
        case RADAR_SPRITE_HOSTPITAL:
        case RADAR_SPRITE_MODGARAGE:
        case RADAR_SPRITE_PIZZA:
        case RADAR_SPRITE_POLICE:
        case RADAR_SPRITE_RACE:
        case RADAR_SPRITE_SAVEGAME:
        case RADAR_SPRITE_SCHOOL:
        case RADAR_SPRITE_TATTOO:
        case RADAR_SPRITE_TSHIRT:
        case RADAR_SPRITE_DATEDISCO:
        case RADAR_SPRITE_DATEDRINK:
        case RADAR_SPRITE_DATEFOOD:
        case RADAR_SPRITE_TRUCK:
        case RADAR_SPRITE_CASH:
        case RADAR_SPRITE_FLAG:
        case RADAR_SPRITE_GYM:
        case RADAR_SPRITE_IMPOUND:
        case RADAR_SPRITE_SPRAY:
            return (FrontEndMenuManager.m_ShowLocationsBlips && priority < 0) || priority == 1;

        case RADAR_SPRITE_BIGSMOKE:
        case RADAR_SPRITE_CATALINAPINK:
        case RADAR_SPRITE_CESARVIAPANDO:
        case RADAR_SPRITE_CJ:
        case RADAR_SPRITE_CRASH1:
        case RADAR_SPRITE_EMMETGUN:
        case RADAR_SPRITE_GIRLFRIEND:
        case RADAR_SPRITE_LOGOSYNDICATE:
        case RADAR_SPRITE_MADDOG:
        case RADAR_SPRITE_MAFIACASINO:
        case RADAR_SPRITE_MCSTRAP:
        case RADAR_SPRITE_OGLOC:
        case RADAR_SPRITE_RYDER:
        case RADAR_SPRITE_QMARK:
        case RADAR_SPRITE_SWEET:
        case RADAR_SPRITE_THETRUTH:
        case RADAR_SPRITE_TORENORANCH:
        case RADAR_SPRITE_TRIADS:
        case RADAR_SPRITE_TRIADSCASINO:
        case RADAR_SPRITE_WOOZIE:
        case RADAR_SPRITE_ZERO:
        case RADAR_SPRITE_GANGB:
        case RADAR_SPRITE_GANGP:
        case RADAR_SPRITE_GANGY:
        case RADAR_SPRITE_GANGN:
        case RADAR_SPRITE_GANGG:
            return (FrontEndMenuManager.m_ShowContactsBlips && priority < 0) || priority == 3;

        default:
            return (FrontEndMenuManager.m_ShowOtherBlips && priority < 0) || priority == 2;
        }
    }

    switch (spriteId) {
    case RADAR_SPRITE_NONE:
    case RADAR_SPRITE_WHITE:
    case RADAR_SPRITE_CENTRE:
    case RADAR_SPRITE_MAP_HERE:
    case RADAR_SPRITE_NORTH:
    case RADAR_SPRITE_MAFIACASINO:
    case RADAR_SPRITE_SCHOOL:
    case RADAR_SPRITE_WAYPOINT:
    case RADAR_SPRITE_TRIADSCASINO:
    case RADAR_SPRITE_CASH:
        return true;

    default:
        return false;
    }
}

void CRadar::ChangeBlipBrightness(tBlipHandle blip, int32_t brightness) {
    // src/CRadar/ChangeBlipBrightness_00583c70.c (unused in 1.0)
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1) {
        ms_RadarTrace[index].m_bBright = (brightness == 1);
    }
}

void CRadar::ChangeBlipScale(tBlipHandle blip, int32_t size) {
    // src/CRadar/ChangeBlipScale_00583cc0.c
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1) {
        ms_RadarTrace[index].m_nBlipSize = FrontEndMenuManager.m_bDrawingMap
            ? 1
            : static_cast<uint16_t>(size);
    }
}

void CRadar::ChangeBlipDisplay(tBlipHandle blip, eBlipDisplay blipDisplay) {
    // src/CRadar/ChangeBlipDisplay_00583d20.c
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1) {
        ms_RadarTrace[index].m_nBlipDisplayFlag = blipDisplay;
    }
}

void CRadar::SetBlipSprite(tBlipHandle blip, eRadarSprite spriteId) {
    // src/CRadar/SetBlipSprite_00583d70.c
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1 && ms_RadarTrace[index].m_bTrackingBlip) {
        ms_RadarTrace[index].m_nBlipSprite = spriteId;
    }
}

void CRadar::SetBlipAlwaysDisplayInZoom(tBlipHandle blip, bool display) {
    // src/CRadar/SetBlipAlwaysDisplayInZoom_00583db0.c
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1 && ms_RadarTrace[index].m_bTrackingBlip) {
        ms_RadarTrace[index].m_bBlipRemain = display;
    }
}

void CRadar::SetBlipFade(tBlipHandle blip, bool fade) {
    // src/CRadar/SetBlipFade_00583e00.c (unused in 1.0)
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1) {
        ms_RadarTrace[index].m_bBlipFade = fade;
    }
}

void CRadar::SetCoordBlipAppearance(tBlipHandle blip, eBlipAppearance appearance) {
    // src/CRadar/SetCoordBlipAppearance_00583e50.c
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1) {
        auto& trace = ms_RadarTrace[index];
        if (trace.m_nBlipType != eBlipType::BLIP_CAR) {
            return;
        }
        switch (appearance) {
        case BLIP_FLAG_FRIEND:
        case BLIP_FLAG_THREAT:
        case BLIP_FLAG_UNK:
            trace.m_nAppearance = appearance;
            break;
        }
    }
}

void CRadar::SetBlipFriendly(tBlipHandle blip, bool friendly) {
    // src/CRadar/SetBlipFriendly_00583eb0.c
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1) {
        ms_RadarTrace[index].m_bFriendly = friendly;
    }
}

void CRadar::SetBlipEntryExit(tBlipHandle blip, CEntryExit* enex) {
    // src/CRadar/SetBlipEntryExit_00583f00.c
    // (CEntryExit* here is the file-scope shim; the header forward-declares the class.)
    const int32_t index = GetActualBlipArrayIndex(blip);
    if (index != -1) {
        auto& trace = ms_RadarTrace[index];
        if (trace.m_bTrackingBlip) {
            trace.m_pEntryExit = enex;
        }
    }
}
void CRadar::ShowRadarTrace(float x, float y, uint32_t size, CRGBA color) {
    // src/CRadar/ShowRadarTrace_00583f40.c
    Limit(x, y);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));

    // Black border rect (1px larger on each side), then the actual rect on top.
    CSprite2d::DrawRect(
        CRect{
            x - static_cast<float>(size - 1),
            y - static_cast<float>(size - 1),
            x + static_cast<float>(size + 1),
            y + static_cast<float>(size + 1)
        },
        CRGBA{0, 0, 0, color.a}
    );
    CSprite2d::DrawRect(
        CRect{
            x - static_cast<float>(size),
            y - static_cast<float>(size),
            x + static_cast<float>(size),
            y + static_cast<float>(size)
        },
        color
    );
}

void CRadar::ShowRadarTraceWithHeight(float x, float y, uint32_t size, uint32_t r, uint32_t g, uint32_t b, uint32_t a, eRadarTraceHeight height) {
    // src/CRadar/ShowRadarTraceWithHeight_00584070.c
    // NOTE: the RGBA parameters are 4 bytes per channel in 1.0; truncate to bytes.
    Limit(x, y);
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));

    const uint8_t cr = static_cast<uint8_t>(r), cg = static_cast<uint8_t>(g);
    const uint8_t cb = static_cast<uint8_t>(b), ca = static_cast<uint8_t>(a);

    const float size0 = static_cast<float>(size + 0), size1 = static_cast<float>(size + 1);
    const float size2 = static_cast<float>(size + 2), size3 = static_cast<float>(size + 3);

    switch (height) {
    case RADAR_TRACE_HIGH: // down-pointing triangle
        CSprite2d::Draw2DPolygon( // black border
            x,                           y + SCREEN_STRETCH_Y(size3),
            x,                           y + SCREEN_STRETCH_Y(size3),
            x + SCREEN_STRETCH_X(size3), y - SCREEN_STRETCH_Y(size2),
            x - SCREEN_STRETCH_X(size3), y - SCREEN_STRETCH_Y(size2),
            CRGBA{0, 0, 0, ca}
        );
        CSprite2d::Draw2DPolygon( // triangle
            x,                           y + SCREEN_STRETCH_Y(size1),
            x,                           y + SCREEN_STRETCH_Y(size1),
            x + SCREEN_STRETCH_X(size1), y - SCREEN_STRETCH_Y(size1),
            x - SCREEN_STRETCH_X(size1), y - SCREEN_STRETCH_Y(size1),
            CRGBA{cr, cg, cb, ca}
        );
        break;
    case RADAR_TRACE_NORMAL: // box
        CSprite2d::DrawRect( // black border
            CRect{
                x - SCREEN_STRETCH_X(size1), y - SCREEN_STRETCH_Y(size1),
                x + SCREEN_STRETCH_X(size1), y + SCREEN_STRETCH_Y(size1)
            },
            CRGBA{0, 0, 0, ca}
        );
        CSprite2d::DrawRect( // box
            CRect{
                x - SCREEN_STRETCH_X(size0), y - SCREEN_STRETCH_Y(size0),
                x + SCREEN_STRETCH_X(size0), y + SCREEN_STRETCH_Y(size0)
            },
            CRGBA{cr, cg, cb, ca}
        );
        break;
    case RADAR_TRACE_LOW: // up-pointing triangle
    default:
        CSprite2d::Draw2DPolygon( // black border
            x + SCREEN_STRETCH_X(size3), y + SCREEN_STRETCH_Y(size2),
            x - SCREEN_STRETCH_X(size3), y + SCREEN_STRETCH_Y(size2),
            x,                           y - SCREEN_STRETCH_Y(size3),
            x,                           y - SCREEN_STRETCH_Y(size3),
            CRGBA{0, 0, 0, ca}
        );
        CSprite2d::Draw2DPolygon( // triangle
            x + SCREEN_STRETCH_X(size1), y + SCREEN_STRETCH_Y(size1),
            x - SCREEN_STRETCH_X(size1), y + SCREEN_STRETCH_Y(size1),
            x,                           y - SCREEN_STRETCH_Y(size1),
            x,                           y - SCREEN_STRETCH_Y(size1),
            CRGBA{cr, cg, cb, ca}
        );
        break;
    }
}

void CRadar::ShowRadarMarker(CVector posn, uint32_t color, float radius) {
    // src/CRadar/ShowRadarMarker_00584480.c (debug cross marker)
    // TODO(port): needs CMatrix::GetUp/GetRight from the RenderWare layer.
    const CVector up{0.0f, 0.0f, 1.0f};
    const CVector right{1.0f, 0.0f, 0.0f};

    const float r0 = radius * 0.5f;
    const float r1 = radius * 1.4f;

    CTheScripts::ScriptDebugLine3D(posn + r1 * up,    posn + r0 * up,    color, color);
    CTheScripts::ScriptDebugLine3D(posn - r1 * up,    posn - r0 * up,    color, color);
    CTheScripts::ScriptDebugLine3D(posn + r1 * right, posn + r0 * right, color, color);
    CTheScripts::ScriptDebugLine3D(posn - r1 * right, posn - r0 * right, color, color);
}

uint32_t CRadar::GetRadarTraceColour(eBlipColour color, bool bright, bool friendly) {
    // src/CRadar/GetRadarTraceColour_00584770.c
    switch (color) {
    case BLIP_COLOUR_RED:
    case BLIP_COLOUR_REDCOPY:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_RED : HUD_COLOUR_DARK_RED);
    case BLIP_COLOUR_GREEN:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_GREEN : HUD_COLOUR_DARK_GREEN);
    case BLIP_COLOUR_BLUE:
    case BLIP_COLOUR_BLUECOPY:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_LIGHT_BLUE : HUD_COLOUR_BLUE);
    case BLIP_COLOUR_WHITE:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_LIGHT_GRAY : HUD_COLOUR_DARK_GRAY);
    case BLIP_COLOUR_YELLOW:
        return HudColour.GetIntColour(bright ? HUD_COLOUR_CREAM : HUD_COLOUR_GOLD);
    case BLIP_COLOUR_THREAT:
        return HudColour.GetIntColour(friendly ? HUD_COLOUR_BLUE : HUD_COLOUR_RED);
    case BLIP_COLOUR_DESTINATION:
        return HudColour.GetIntColour(HUD_COLOUR_CREAM);
    default:
        return static_cast<uint32_t>(color);
    }
}

void CRadar::DrawRotatingRadarSprite(CSprite2d& sprite, float x, float y, float angle, uint32_t width, uint32_t height, CRGBA color) {
    // src/CRadar/DrawRotatingRadarSprite_00584850.c
    Limit(x, y);

    CVector2D verts[4]{};
    for (auto i = 0u; i < 4u; i++) {
        const float theta = static_cast<float>(i) * HALF_PI + (angle - FRAC_PI_4);
        verts[i].x = std::sin(theta) * static_cast<float>(width) + x;
        verts[i].y = std::cos(theta) * static_cast<float>(height) + y;
    }

    sprite.Draw(verts[3].x, verts[3].y, verts[2].x, verts[2].y,
                verts[0].x, verts[0].y, verts[1].x, verts[1].y, color);
}

void CRadar::DrawYouAreHereSprite(float x, float y) {
    // src/CRadar/DrawYouAreHereSprite_00584960.c
    if (CTimer::GetTimeInMSPauseMode() - mapYouAreHereTimer > 700) {
        mapYouAreHereTimer = CTimer::GetTimeInMSPauseMode();
        mapYouAreHereDisplay = !mapYouAreHereDisplay;
    }

    if (mapYouAreHereDisplay) {
        const float angle = FindPlayerHeading(0) + DegreesToRadians(180.0f);
        const float circleAngle = angle + DegreesToRadians(90.0f);

        DrawRotatingRadarSprite(
            RadarBlipSprites[RADAR_SPRITE_MAP_HERE],
            x + 17.0f * std::cos(circleAngle),
            y - 17.0f * std::sin(circleAngle),
            angle,
            static_cast<uint32_t>(SCREEN_STRETCH_X(25.0f)),
            static_cast<uint32_t>(SCREEN_STRETCH_Y(25.0f)),
            CRGBA{255, 255, 255, 255}
        );
    }

    MapLegendList[MapLegendCounter++] = RADAR_SPRITE_MAP_HERE;
}

void CRadar::SetupRadarRect(int32_t x, int32_t y) {
    // src/CRadar/SetupRadarRect_00584a80.c
    m_radarRect.left   = static_cast<float>(500 * (x - 7));
    m_radarRect.top    = static_cast<float>(500 * (7 - y));
    m_radarRect.right  = static_cast<float>(500 * (x - 4));
    m_radarRect.bottom = static_cast<float>(500 * (4 - y));
}

void CRadar::RequestMapSection(int32_t x, int32_t y) {
    // src/CRadar/RequestMapSection_00584b50.c
    if (!IsMapSectionInBounds(x, y)) {
        return;
    }
    const int32_t texture = gRadarTextures[y][x];
    if (texture != -1) {
        CStreaming::RequestTxdModel(texture, STREAMING_GAME_REQUIRED | STREAMING_KEEP_IN_MEMORY);
    }
}

void CRadar::RemoveMapSection(int32_t x, int32_t y) {
    // src/CRadar/RemoveMapSection_00584bb0.c
    if (!IsMapSectionInBounds(x, y)) {
        return;
    }
    const int32_t texture = gRadarTextures[y][x];
    if (texture != -1) {
        CStreaming::RemoveTxdModel(texture);
    }
}

void CRadar::RemoveRadarSections() {
    // src/CRadar/RemoveRadarSections_00584bf0.c
    for (auto y = 0u; y < MAX_RADAR_HEIGHT_TILES; y++) {
        for (auto x = 0u; x < MAX_RADAR_WIDTH_TILES; x++) {
            CStreaming::RemoveTxdModel(gRadarTextures[y][x]);
        }
    }
}

void CRadar::StreamRadarSections(const CVector& worldPosn) {
    // src/CRadar/StreamRadarSections_005858d0.c
    if (!CStreaming::ms_disableStreaming) {
        StreamRadarSections(
            static_cast<int32_t>(std::floor((worldPosn.x + 3000.0f) / 500.0f)),
            static_cast<int32_t>(std::ceil(11.0f - (worldPosn.y + 3000.0f) / 500.0f))
        );
    }
}

void CRadar::StreamRadarSections(int32_t x, int32_t y) {
    // src/CRadar/StreamRadarSections_00584c50.c
    for (auto curY = 0u; curY < MAX_RADAR_HEIGHT_TILES; curY++) {
        for (auto curX = 0u; curX < MAX_RADAR_WIDTH_TILES; curX++) {
            const int32_t texture = gRadarTextures[curY][curX];
            if (texture != -1) {
                if (curY >= static_cast<uint32_t>(y - 1) && curY <= static_cast<uint32_t>(y + 1)
                    && curX >= static_cast<uint32_t>(x - 1) && curX <= static_cast<uint32_t>(x + 1)) {
                    CStreaming::RequestModel(TXDToModelId(texture), STREAMING_KEEP_IN_MEMORY | STREAMING_GAME_REQUIRED);
                } else {
                    CStreaming::RemoveModel(TXDToModelId(texture));
                }
            }
        }
    }
}

int32_t CRadar::ClipRadarPoly(CVector2D* out, const CVector2D* in) {
    // src/CRadar/ClipRadarPoly_00585040.c
    // Clips a 4-vertex polygon against the [-1,1]x[-1,1] radar box.
    // Returns the number of output vertices (0..8).
    // (gta-reversed stubs this via plugin::Call; converted directly from decomp.)
    static const float boxCorners[8] = {
        +1.0f, -1.0f, // side 0 (y = -1)
        +1.0f, +1.0f, // side 1 (x = +1)
        -1.0f, +1.0f, // side 2 (y = +1)
        -1.0f, -1.0f, // side 3 (x = -1)
    };

    const auto IsInside = [](const CVector2D& p) {
        return p.x >= -1.0f && p.x <= 1.0f && p.y >= -1.0f && p.y <= 1.0f;
    };

    bool inside[4];
    for (int32_t i = 0; i < 4; i++) {
        inside[i] = IsInside(in[i]);
    }

    int32_t numVerts = 0;
    int32_t lastSide = -1;

    for (int32_t i = 0; i < 4; i++) {
        if (!inside[i]) {
            const int32_t prev = (i - 1) & 3;
            const int32_t next = (i + 1) & 3;

            CVector2D prevX{};
            const int32_t prevSide = LineRadarBoxCollision(prevX, in[i], in[prev]);
            if (prevSide != -1) {
                out[numVerts++] = prevX;
                lastSide = prevSide;
            }

            CVector2D nextX{};
            const int32_t nextSide = LineRadarBoxCollision(nextX, in[i], in[next]);
            if (nextSide != -1) {
                if (prevSide == -1) {
                    // No intersection with the previous edge: find the first
                    // intersected edge to know which box corners to emit.
                    if (lastSide == -1) {
                        for (int32_t e = 3; e >= i; e--) {
                            CVector2D tmp{};
                            const int32_t s = (e == 0)
                                ? LineRadarBoxCollision(tmp, in[0], in[3])
                                : LineRadarBoxCollision(tmp, in[e], in[e - 1]);
                            if (s != -1) {
                                lastSide = s;
                                break;
                            }
                        }
                    }
                    // Emit box corners from lastSide up to (not including) nextSide.
                    for (int32_t s = lastSide; s != nextSide; s = (s + 1) & 3) {
                        out[numVerts].x = boxCorners[s * 2];
                        out[numVerts].y = boxCorners[s * 2 + 1];
                        numVerts++;
                    }
                    out[numVerts++] = nextX;
                } else {
                    out[numVerts++] = nextX;
                }
            }
        } else {
            out[numVerts++] = in[i];
        }
    }

    if (numVerts == 0) {
        // Degenerate case: the polygon fully surrounds the box without any
        // vertex inside. Emit the whole box if the box centre is straddled.
        const float slope01 = (in[0].y - in[1].y) / (in[0].x - in[1].x);
        const float slope30 = (in[0].y - in[3].y) / (in[0].x - in[3].x);
        const bool straddles01 = (slope01 * in[0].x - in[0].y) * (slope01 * in[3].x - in[3].y) < 0.0f;
        const bool straddles30 = (slope30 * in[0].x - in[0].y) * (slope30 * in[1].x - in[1].y) < 0.0f;
        if (straddles01 && straddles30) {
            for (int32_t s = 0; s < 4; s++) {
                out[s].x = boxCorners[s * 2];
                out[s].y = boxCorners[s * 2 + 1];
            }
            return 4;
        }
    }

    return numVerts;
}

void CRadar::DrawAreaOnRadar(const CRect& rect, const CRGBA& color, bool inMenu) {
    // src/CRadar/DrawAreaOnRadar_005853d0.c
    // Decomp condition to proceed: inMenu OR rects overlap
    // (rect->left <= m_radarRect.right && m_radarRect.left <= rect->right &&
    //  rect->bottom <= m_radarRect.top && m_radarRect.bottom <= rect->top).
    const bool overlaps =
        rect.left <= m_radarRect.right && m_radarRect.left <= rect.right &&
        rect.bottom <= m_radarRect.top && m_radarRect.bottom <= rect.top;
    if (!inMenu && !overlaps) {
        return;
    }

    // Corner positions, transformed to radar space (inlined
    // TransformRealWorldPointToRadarSpace in the original).
    const CVector2D rectCorners[4]{
        {rect.left,  rect.bottom},
        {rect.right, rect.bottom},
        {rect.right, rect.top},
        {rect.left,  rect.top},
    };
    CVector2D polyUnclipped[4]{};
    for (int32_t i = 0; i < 4; i++) {
        polyUnclipped[i] = TransformRealWorldPointToRadarSpace(rectCorners[i]);
    }

    CVector2D polyVerts[8]{};
    const int32_t numVerts = ClipRadarPoly(polyVerts, polyUnclipped);
    if (numVerts == 0) {
        return;
    }

    CVector2D texCoords[8]{};
    const float scaleX = SCREEN_STRETCH_X(1.0f);
    const float scaleY = SCREEN_STRETCH_Y(1.0f);
    for (int32_t i = 0; i < numVerts; i++) {
        texCoords[i] = TransformRadarPointToScreenSpace(polyVerts[i]);
        if (FrontEndMenuManager.m_bDrawingMap) {
            texCoords[i] *= CVector2D{scaleX, scaleY};
            polyVerts[i] *= CVector2D{scaleX, scaleY};
        }
    }

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));
    CSprite2d::SetVertices(numVerts, texCoords, polyVerts, color);
    if (numVerts > 2) {
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, CSprite2d::GetVertices(), numVerts);
    }
}

void CRadar::DrawRadarMask() {
    // src/CRadar/DrawRadarMask_00585700.c
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(NULL));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,         RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(TRUE));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, RWRSTATE(rwALPHATESTFUNCTIONALWAYS));

    // Draw the shape to mask out from the radar in four quarter-circle segments.
    const CVector2D corners[4]{
        {+1.0f, -1.0f},
        {+1.0f, +1.0f},
        {-1.0f, +1.0f},
        {-1.0f, -1.0f},
    };
    CVector2D out[8]{};
    for (const auto& corner : corners) {
        out[0] = TransformRadarPointToScreenSpace(corner);
        for (auto j = 0; j < 7; j++) {
            const CVector2D in{
                corner.x * std::cos(static_cast<float>(j) * (FRAC_PI_2 / 6.0f)),
                corner.y * std::sin(static_cast<float>(j) * (FRAC_PI_2 / 6.0f)),
            };
            out[j + 1] = TransformRadarPointToScreenSpace(in);
        }
        CSprite2d::SetMaskVertices(8, out, CSprite2d::GetNearScreenZ());
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, CSprite2d::GetVertices(), 8);
    }

    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, RWRSTATE(rwALPHATESTFUNCTIONGREATER));
}
void CRadar::InitFrontEndMap() {
    // src/CRadar/InitFrontEndMap_00585960.c
    CalculateCachedSinCos();
    for (auto& entry : MapLegendList) {
        entry = RADAR_SPRITE_NONE;
    }

    vec2DRadarOrigin = CVector2D{0.0f, 0.0f};
    m_radarRange = 2990.0f; // WORLD_BOUND_RANGE - 10.0f
    MapLegendCounter = 0;

    for (auto& c : ArrowBlipColour) {
        c = CRGBA{0, 0, 0, 0};
    }
}

void CRadar::AddBlipToLegendList(bool noSprite, int32_t blipIndex) {
    // src/CRadar/AddBlipToLegendList_005859f0.c
    if (!FrontEndMenuManager.m_bDrawingMap) {
        return;
    }

    auto& trace = ms_RadarTrace[blipIndex];

    eRadarSprite sprite;
    if (!noSprite) {
        sprite = static_cast<eRadarSprite>(blipIndex);
    } else {
        switch (trace.m_nBlipType) {
        case BLIP_CAR:
        case BLIP_CHAR:
            sprite = trace.m_bFriendly ? RADAR_SPRITE_FRIEND : RADAR_SPRITE_THREAT;
            break;
        case BLIP_OBJECT:
            sprite = RADAR_SPRITE_OBJECT;
            break;
        case BLIP_COORD:
            sprite = RADAR_SPRITE_DESTINATION;
            break;
        default:
            sprite = RADAR_SPRITE_PLAYER_INTEREST;
            break;
        }
    }

    // Don't add duplicates.
    if (!notsa::contains(MapLegendList, static_cast<int16_t>(sprite))) {
        MapLegendList[MapLegendCounter++] = static_cast<int16_t>(sprite);
        if (noSprite) {
            const CRGBA color = CRGBAFromInt(GetRadarTraceColour(trace.m_nColour, trace.m_bBright, trace.m_bFriendly));
            ArrowBlipColour[-sprite] = CRGBA{color.r, color.g, color.b, 255};
        }
    }
}

void CRadar::SetMapCentreToPlayerCoords() {
    // src/CRadar/SetMapCentreToPlayerCoords_00585b20.c
    if (FindPlayerPed() == nullptr) {
        return;
    }

    FrontEndMenuManager.m_bDrawingMap = true;

    InitFrontEndMap();

    CVector2D posReal = [&]() {
        const CVector p = FindPlayerCentreOfWorldForMap(0);
        return CVector2D{p.x, p.y};
    }();

    if (CTheScripts::HideAllFrontEndMapBlips || CTheScripts::bPlayerIsOffTheMap) {
        posReal = CVector2D{0.0f, 0.0f};
    }

    CVector2D posRadar = TransformRealWorldPointToRadarSpace(posReal);
    LimitRadarPoint(posRadar);

    FrontEndMenuManager.m_vMousePos = posReal;
    FrontEndMenuManager.m_vMapOrigin.x = DEFAULT_SCREEN_WIDTH  / 2.0f - FrontEndMenuManager.m_fMapZoom * posRadar.x;
    FrontEndMenuManager.m_vMapOrigin.y = DEFAULT_SCREEN_HEIGHT / 2.0f + FrontEndMenuManager.m_fMapZoom * posRadar.y;
    FrontEndMenuManager.m_bDrawingMap = false;
}

void CRadar::Draw3dMarkers() {
    // src/CRadar/Draw3dMarkers_00585bf0.c
    const auto PutMarkerCone = [](uint32_t id, const CVector& pos, float size, const CRGBA& color) {
        C3dMarkers::PlaceMarkerCone(id, pos, size, color.r, color.g, color.b, 255, 1024u, 0.2f, 5, true);
    };

    for (auto i = 0u; i < MAX_RADAR_TRACES; i++) {
        auto& trace = ms_RadarTrace[i];
        if (!trace.m_bTrackingBlip) {
            continue;
        }

        if (trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BOTH && trace.m_nBlipDisplayFlag != BLIP_DISPLAY_MARKERONLY) {
            continue;
        }

        const CRGBA color = CRGBAFromInt(GetRadarTraceColour(trace.m_nColour, trace.m_bBright, trace.m_bFriendly));
        const uint32_t coneHandle = i | (static_cast<uint32_t>(trace.m_nCounter) << 16);

        switch (trace.m_nBlipType) {
        case BLIP_CAR: {
            const auto* vehicle = GetVehiclePool()->GetAtRef(static_cast<int32_t>(trace.m_nEntityHandle));
            if (!vehicle) {
                break; // TODO(port): 1.0 asserts here (NOTSA: assert(vehicle))
            }
            const float bbMaxZ = vehicle->GetColModel()->GetBoundingBox().m_vecMax.z
                * (ModelIndices::IsNevada(vehicle->m_nModelIndex) ? (5.0f / 3.0f) : (6.0f / 5.0f));
            const CVector posn = vehicle->GetPosition() + CVector{0.0f, 0.0f, bbMaxZ + 2.0f};
            PutMarkerCone(coneHandle, posn, 2.0f, color);
            break;
        }
        case BLIP_CHAR: {
            const auto* ped = GetPedPool()->GetAtRef(static_cast<int32_t>(trace.m_nEntityHandle));
            if (!ped) {
                break; // TODO(port): 1.0 asserts here
            }
            PutMarkerCone(coneHandle, ped->GetRealPosition() + CVector{0.0f, 0.0f, 2.7f}, 1.2f, color);
            break;
        }
        case BLIP_OBJECT:
        case BLIP_PICKUP: {
            CVector posn{};
            if (trace.m_nBlipType == BLIP_OBJECT) {
                const auto* obj = GetObjectPool()->GetAtRef(static_cast<int32_t>(trace.m_nEntityHandle));
                if (!obj) {
                    break; // TODO(port): 1.0 raises NOTSA_UNREACHABLE here
                }
                posn = obj->GetPosition() + CVector{0.0f, 0.0f, obj->GetColModel()->GetBoundingBox().m_vecMax.z};
            } else {
                const int32_t idx = CPickups::GetActualPickupIndex(static_cast<int32_t>(trace.m_nEntityHandle));
                if (idx < 0) {
                    break; // TODO(port): 1.0 raises NOTSA_UNREACHABLE here
                }
                posn = CPickups::aPickUps[idx].GetPosn() + CVector{0.0f, 0.0f, 2.0f};
            }
            posn.z += (CGame::currArea != 0 || FindPlayerPed()->GetAreaCode() != AREA_CODE_NORMAL_WORLD) ? 1.6f : 1.8f;
            PutMarkerCone(coneHandle, posn, 0.8f, color);
            break;
        }
        case BLIP_CONTACT_POINT: {
            if (CTheScripts::IsPlayerOnAMission() || !FindPlayerPed()) {
                break;
            }
            if (!trace.m_bTrackingBlip && FindPlayerPed()->GetAreaCode() != AREA_CODE_NORMAL_WORLD) {
                break;
            }
            C3dMarkers::PlaceMarkerSet(coneHandle, MARKER3D_CYLINDER, trace.m_vPosition,
                                       2.0f, 255, 0, 0, 228, 2048u, 0.2f, 0);
            break;
        }
        default:
            break;
        }
    }
}

void CRadar::SetRadarMarkerState(int32_t counter, bool flag) {
    // src/CRadar/SetRadarMarkerState_00585fe0.c
    // NOP in SA (III/VC leftover).
    (void)counter; (void)flag;
}

void CRadar::DrawRadarSprite(eRadarSprite spriteId, float x, float y, uint8_t alpha) {
    // src/CRadar/DrawRadarSprite_00585ff0.c
    Limit(x, y);

    const float width  = std::floor(SCREEN_STRETCH_X(8.0f));
    const float height = std::floor(SCREEN_STRETCH_Y(8.0f));

    if (DisplayThisBlip(spriteId, -99)) {
        RadarBlipSprites[static_cast<size_t>(spriteId)].Draw(
            CRect{x - width, y - height, x + width, y + height},
            CRGBA{255, 255, 255, alpha}
        );
        AddBlipToLegendList(false, spriteId);
    }
}

void CRadar::DrawRadarSection(int32_t x, int32_t y) {
    // src/CRadar/DrawRadarSection_00586110.c
    CVector2D clipped[8]{};
    int32_t numVerts = 0;
    {
        CVector2D corners[4]{};
        GetTextureCorners(x, y, corners);

        CVector2D rotated[4]{};
        for (auto i = 0; i < 4; i++) {
            rotated[i] = CachedRotateClockwise((corners[i] - vec2DRadarOrigin) / m_radarRange);
        }
        numVerts = ClipRadarPoly(clipped, rotated);
    }

    CVector2D texCoords[8]{};
    CVector2D verts[8]{};
    for (auto i = 0; i < numVerts; i++) {
        texCoords[i] = TransformRealWorldToTexCoordSpace(
            vec2DRadarOrigin + CachedRotateCounterclockwise(clipped[i]) * m_radarRange, x, y);
        verts[i] = TransformRadarPointToScreenSpace(clipped[i]);
    }

    if (!IsMapSectionInBounds(x, y)) {
        // No land here: draw sea.
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));
        CSprite2d::SetVertices(numVerts, verts, texCoords, CRGBA{111, 137, 170, 255});
    } else if (CTheScripts::bPlayerIsOffTheMap) {
        // Draw blank.
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL));
        CSprite2d::SetVertices(numVerts, verts, texCoords, CRGBA{204, 204, 204, 255});
    } else if (const int32_t txdIndex = gRadarTextures[y][x]) {
        if (const auto* txd = CTxdStore::GetTxd(txdIndex)) {
            if (RwTexture* const texture = GetFirstTexture(const_cast<RwTexDictionary*>(txd))) {
                RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL)); // TODO(port): pass texture raster
                CSprite2d::SetVertices(numVerts, verts, texCoords, CRGBA{255, 255, 255, 255});
                (void)texture;
            }
        }
    }

    if (numVerts > 2) {
        RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, CSprite2d::GetVertices(), numVerts);
    }
}

void CRadar::DrawRadarSectionMap(int32_t x, int32_t y, CRect rect) {
    // src/CRadar/DrawRadarSectionMap_00586520.c
    if (!IsMapSectionInBounds(x, y)) {
        return;
    }
    const int32_t txdIndex = gRadarTextures[y][x];
    if (txdIndex == -1) {
        return;
    }
    if (const auto* txd = CTxdStore::GetTxd(txdIndex)) {
        if (GetFirstTexture(const_cast<RwTexDictionary*>(txd))) {
            const CRGBA bg{255, 255, 255, 255};
            RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(NULL)); // TODO(port): texture->raster
            CSprite2d::SetVertices(rect, bg, bg, bg, bg);
            RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, CSprite2d::GetVertices(), 4);
        }
    }
}

void CRadar::DrawRadarGangOverlay(bool inMenu) {
    // src/CRadar/DrawRadarGangOverlay_00586650.c
    if ((g_RadarGangResetOverlay & 1u) == 0) {
        g_RadarGangResetOverlay |= 1u;
        g_RadarGangOverlay = CRect{};
    }

    if (!CGangWars::bGangWarsActive || !FrontEndMenuManager.m_abPrefsMapBlips[4]) {
        return;
    }

    // TODO(port): 1.0 iterates CTheZones::GetNavigationZones(); the zones port
    // will provide the real range. Single-zone shim for now.
    CTheZones::Zone* zone = CTheZones::GetNavigationZones();
    if (!zone) {
        return;
    }

    const auto* info = CTheZones::GetZoneInfo(zone);
    if (!info || !info->RadarMode || !CGangWars::CanPlayerStartAGangWarHere(info)) {
        return;
    }

    g_RadarGangOverlay = zone->GetRect();

    switch (info->RadarMode) {
    case 1:
        DrawAreaOnRadar(g_RadarGangOverlay, info->ZoneColor, inMenu);
        break;
    case 2: {
        const uint32_t timeInMS = FrontEndMenuManager.m_bDrawingMap
            ? CTimer::GetTimeInMSPauseMode()
            : CTimer::GetTimeInMS();
        CRGBA zoneColor = info->ZoneColor;
        zoneColor.a = static_cast<uint8_t>(
            (std::sin(static_cast<float>(timeInMS % 1024) * (1024.0f / TWO_PI)) + 1.0f) / 2.0f
            * static_cast<float>(zoneColor.a));
        DrawAreaOnRadar(g_RadarGangOverlay, zoneColor, inMenu);
        break;
    }
    default:
        break;
    }
}
void CRadar::DrawRadarMap() {
    // src/CRadar/DrawRadarMap_00586880.c
    DrawRadarMask();

    auto x = static_cast<int32_t>(std::floor((vec2DRadarOrigin.x + 3000.0f) / 500.0f));
    auto y = static_cast<int32_t>(std::ceil(11.0f - (vec2DRadarOrigin.y + 3000.0f) / 500.0f));

    SetupRadarRect(x, y);
    StreamRadarSections(x, y);

    RwRenderStateSet(rwRENDERSTATEFOGENABLE,          RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,           RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,          RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,      RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,          RWRSTATE(D3DSHADE_FLAT));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,        RWRSTATE(rwRENDERSTATETEXTURERASTER));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,       RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,  RWRSTATE(FALSE));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,     RWRSTATE(D3DTADDRESS_CLAMP));
    RwRenderStateSet(rwRENDERSTATETEXTUREPERSPECTIVE, RWRSTATE(FALSE));

    DrawRadarSection(x - 1, y - 1);
    DrawRadarSection(x,     y - 1);
    DrawRadarSection(x + 1, y - 1);
    DrawRadarSection(x - 1, y);
    DrawRadarSection(x,     y);
    DrawRadarSection(x + 1, y);
    DrawRadarSection(x - 1, y + 1);
    DrawRadarSection(x,     y + 1);
    DrawRadarSection(x + 1, y + 1);

    DrawRadarGangOverlay(false);

    // Green rectangle when flying a plane: pitch/roll indicator.
    // TODO(port): needs CVehicle::IsSubPlane/m_nModelIndex/m_matrix from the vehicle port.
    const CVehicle* vehicle = FindPlayerVehicle();
    if (vehicle && vehicle->IsSubPlane() && !ModelIndices::IsVortex(vehicle->m_nModelIndex)) {
        const CVector playerPos = FindPlayerCentreOfWorldForMap(0);

        const float savedSin = cachedSin, savedCos = cachedCos;
        cachedSin = std::sin(PI);
        cachedCos = std::cos(PI);

        const float angle = std::atan2(-vehicle->m_matrix->GetForward().z, vehicle->m_matrix->GetUp().z);

        DrawAreaOnRadar(
            CRect{
                playerPos.x - 1000.0f,
                playerPos.y - (2.0f * RadiansToDegrees(angle)),
                playerPos.x + 1000.0f,
                playerPos.y + 2000.0f,
            },
            CRGBA{20, 175, 20, 200},
            false
        );

        cachedSin = savedSin;
        cachedCos = savedCos;
    }

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(FALSE));
}

void CRadar::DrawMap() {
    // src/CRadar/DrawMap_00586b00.c
    constexpr float RADAR_MIN_RANGE = 180.0f;
    constexpr float RADAR_MAX_RANGE = 350.0f;
    constexpr float RADAR_MIN_SPEED = 0.3f;
    constexpr float RADAR_MAX_SPEED = 0.9f;

    const CPed* player = FindPlayerPed();
    const bool mapShouldDrawn =
        !CGame::currArea && player->GetAreaCode() == AREA_CODE_NORMAL_WORLD
        && FrontEndMenuManager.m_nRadarMode != 1; // RADAR_MODE_BLIPS_ONLY

    CalculateCachedSinCos();

    const CVehicle* vehicle = FindPlayerVehicle();

    if (!vehicle) {
        // On foot (or remote mode): zoom range from script override or minimum.
        // Decomp: m_radarRange = 180 - RadarZoomValue (0xB4 - zoom).
        if (CTheScripts::RadarZoomValue) {
            m_radarRange = RADAR_MIN_RANGE - static_cast<float>(CTheScripts::RadarZoomValue);
        } else {
            m_radarRange = RADAR_MIN_RANGE;
        }
    } else if (vehicle->IsSubPlane() && !ModelIndices::IsVortex(vehicle->m_nModelIndex)) {
        // Flying: range grows with altitude (decomp: z * 0.005).
        const float speedZ = vehicle->GetPosition().z * 0.005f;
        if (speedZ < RADAR_MIN_SPEED) {
            m_radarRange = RADAR_MAX_RANGE - 10.0f;
        } else if (speedZ < RADAR_MAX_SPEED) {
            m_radarRange = (speedZ - RADAR_MIN_SPEED) * 16.666668f + (RADAR_MAX_RANGE - 10.0f);
        } else {
            m_radarRange = RADAR_MAX_RANGE;
        }
    } else {
        // Driving: range grows with speed (decomp: 3D magnitude).
        const float speed = FindPlayerSpeed().Magnitude();
        if (speed < RADAR_MIN_SPEED) {
            m_radarRange = RADAR_MIN_RANGE;
        } else if (speed >= RADAR_MAX_SPEED) {
            m_radarRange = RADAR_MAX_RANGE;
        } else {
            m_radarRange = (speed - RADAR_MIN_SPEED) * 283.33334f + RADAR_MIN_RANGE;
        }
    }
    // TODO(port): 1.0 also takes the on-foot branch when
    // CPlayerInfo::IsPlayerInRemoteMode() (DrawMap_00586b00.c).

    if (!CGameLogic::IsCoopGameGoingOn()) {
        const CVector p = FindPlayerCentreOfWorldForMap(0);
        vec2DRadarOrigin = CVector2D{p.x, p.y};
    } else if (CGameLogic::n2PlayerPedInFocus == 1) { // PLAYER2
        const CVector p = FindPlayerCentreOfWorldForMap(1);
        vec2DRadarOrigin = CVector2D{p.x, p.y};
    } else {
        // Halfway between the two players' positions.
        const CVector p0 = FindPlayerCentreOfWorldForMap(0);
        const CVector p1 = FindPlayerCentreOfWorldForMap(1);
        vec2DRadarOrigin = CVector2D{(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
    }

    if (mapShouldDrawn) {
        DrawRadarMap();
    }
}

void CRadar::DrawCoordBlip(int32_t blipIndex, bool isSprite) {
    // src/CRadar/DrawCoordBlip_00586d60.c
    const auto& trace = ms_RadarTrace[blipIndex];
    if (trace.m_nBlipType == BLIP_CONTACT_POINT && CTheScripts::IsPlayerOnAMission()) {
        return;
    }

    if (isSprite == !trace.HasSprite()) {
        return;
    }

    if (trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BOTH && trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BLIPONLY) {
        return;
    }

    float realDist = 0.0f;
    const auto [radarPos, screenPos] = trace.GetRadarAndScreenPos(&realDist);
    (void)radarPos;
    const float zoomedDist = CTheScripts::RadarZoomValue != 0u ? 255.0f : realDist;

    if (isSprite) {
        const bool canBeDrawn =
            !trace.m_bShortRange || zoomedDist <= 1.0f || FrontEndMenuManager.m_bDrawingMap;
        if (trace.HasSprite() && canBeDrawn && HasThisBlipBeenRevealed(blipIndex)) {
            DrawRadarSprite(trace.m_nBlipSprite, screenPos.x, screenPos.y, 255);
        }
        return;
    }

    if (trace.HasSprite()) {
        return;
    }

    if (FrontEndMenuManager.m_bDrawingMap && !FrontEndMenuManager.m_ShowMissionBlips) {
        return;
    }

    const eRadarTraceHeight height = [&]() {
        const float zDiff = trace.GetWorldPos().z - FindPlayerCentreOfWorldForMap(PED_TYPE_PLAYER1).z;
        if (zDiff > 2.0f) {
            return RADAR_TRACE_LOW; // trace is higher
        } else if (zDiff >= -4.0f) {
            return RADAR_TRACE_NORMAL; // about the same elevation
        } else {
            return RADAR_TRACE_HIGH; // player is higher
        }
    }();

    const CRGBA color = trace.GetStaticColour();
    ShowRadarTraceWithHeight(
        screenPos.x,
        screenPos.y,
        trace.m_nBlipSize,
        color.r, color.g, color.b,
        trace.m_bBlipFade ? color.a : CalculateBlipAlpha(realDist),
        height
    );

    AddBlipToLegendList(true, blipIndex);
}
void CRadar::DrawEntityBlip(int32_t blipIndex, uint8_t arg1) {
    // src/CRadar/DrawEntityBlip_00587000.c
    // (gta-reversed stubs this via plugin::Call; converted directly from decomp.)
    //
    // NOTE on control flow: Ghidra renders the rotating-arrow branch as
    // `else if (param_2 == '\0' && ...)` paired with `if (param_2 == '\0')`,
    // which would be dead code. DrawBlips calls this twice per blip with
    // arg1 = false then true (mirroring DrawCoordBlip's sprite/non-sprite
    // passes), so the arrow branch belongs to the arg1 != 0 pass; converted
    // as a plain `else if` on the arrow condition.
    auto& trace = ms_RadarTrace[blipIndex];
    const int32_t entityHandle = static_cast<int32_t>(trace.m_nEntityHandle);

    bool isAirstrip = false;
    float distToAirstrip = 0.0f;
    const tScriptSearchlight* searchlight = nullptr;
    CVector pos{};
    float heading = 0.0f;

    switch (trace.m_nBlipType) {
    case BLIP_CAR: {
        const CVehicle* vehicle = GetVehiclePool()->GetAtRef(entityHandle);
        if (!vehicle) {
            return;
        }
        pos = vehicle->GetPosition();
        heading = vehicle->GetHeading();
        // TODO(port): 1.0 adjusts pos via CEntryExit when (entityFlags & 0x70000)
        // == 0x30000 (DrawEntityBlip_00587000.c, LAB_00587050).
        break;
    }
    case BLIP_CHAR: {
        const CPed* ped = GetPedPool()->GetAtRef(entityHandle);
        if (!ped) {
            return;
        }
        // TODO(port): 1.0 substitutes the ped's vehicle when (pedFlags & 0x100)
        // (raw struct offsets in the decomp); verify against the CPed port.
        pos = ped->GetPosition();
        heading = ped->GetHeading();
        break;
    }
    case BLIP_OBJECT: {
        const CObject* obj = GetObjectPool()->GetAtRef(entityHandle);
        if (!obj) {
            return;
        }
        pos = obj->GetPosition();
        heading = obj->GetHeading();
        break;
    }
    case BLIP_SPOTLIGHT: {
        const int32_t idx = CTheScripts::GetActualScriptThingIndex(entityHandle, SCRIPT_THING_SEARCH_LIGHT);
        if (idx < 0) {
            return;
        }
        searchlight = &CTheScripts::ScriptSearchLightArray[idx];
        if (!searchlight) {
            return;
        }
        pos = searchlight->m_Target;
        break;
    }
    case BLIP_PICKUP: {
        const int32_t idx = CPickups::GetActualPickupIndex(entityHandle);
        if (idx < 0) {
            return;
        }
        pos = CPickups::aPickUps[idx].GetPosn();
        break;
    }
    case BLIP_AIRSTRIP: {
        isAirstrip = true;
        const CVehicle* playerVehicle = FindPlayerVehicle();
        const CVector vehPos = playerVehicle ? playerVehicle->GetPosition() : CVector{};
        const airstrip_info& strip = airstrip_table[airstrip_location];
        distToAirstrip = DistanceBetweenPoints2D(strip.position, vehPos);

        if (distToAirstrip >= 500.0f) {
            s_airstripBlipOnScreen = 0;
        } else {
            // Decomp converts the ST0 float (the distance just computed) via
            // CGeneral::unk_00821b40 (float->int); flagged where ambiguous.
            if ((s_airstripFlags & 1u) == 0) {
                s_airstripFlags |= 1u;
                s_airstripDist2 = FloatToInt(distToAirstrip);
            }
            if ((s_airstripFlags & 2u) == 0) {
                s_airstripFlags |= 2u;
                s_airstripDist1 = FloatToInt(distToAirstrip);
            }

            // Slide the blip along the runway direction.
            const float dirRad = strip.direction * DegreesToRadians(1.0f);
            const float offset = static_cast<float>(s_airstripDist2);
            pos = CVector{
                trace.m_vPosition.x + std::cos(dirRad) * offset,
                trace.m_vPosition.y - std::sin(dirRad) * offset,
                trace.m_vPosition.z,
            };

            CVector2D radar = TransformRealWorldPointToRadarSpace({pos.x, pos.y});
            const uint32_t now = CTimer::GetTimeInMSPauseMode();
            const float radarDist = CVector2DMagnitude(radar);
            if (radarDist >= 0.9f || now - s_airstripLastTime > 3) {
                s_airstripDist2 += 100;
                if (strip.radius < static_cast<float>(s_airstripDist2)) {
                    s_airstripDist2 = FloatToInt(radarDist); // TODO(port): verify ST0 source
                }
                if (s_airstripDist1 == s_airstripDist2) {
                    s_airstripBlipOnScreen = 0;
                }
                if (radarDist < 0.9f) {
                    s_airstripBlipOnScreen = 1;
                    s_airstripDist1 = s_airstripDist2;
                }
                s_airstripLastTime = now;
            }
            LimitRadarPoint(radar);
        }
        break;
    }
    default:
        return;
    }

    const uint32_t blipColor = GetRadarTraceColour(trace.m_nColour, trace.m_bBright, trace.m_bFriendly);
    if (trace.m_pEntryExit) {
        trace.m_pEntryExit->GetPositionRelativeToOutsideWorld(pos);
    }

    if (trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BOTH && trace.m_nBlipDisplayFlag != BLIP_DISPLAY_BLIPONLY) {
        return;
    }

    CVector2D radarPos = TransformRealWorldPointToRadarSpace({pos.x, pos.y});
    const float radarDist = LimitRadarPoint(radarPos);
    const CVector2D screenPos = TransformRadarPointToScreenSpace(radarPos);

    if (trace.m_bShortRange && (radarDist > 1.0f && !FrontEndMenuManager.m_bDrawingMap)) {
        return;
    }

    // RadarZoomValue == 0 || (in a vehicle || type == OBJECT || type == PICKUP)
    if (CTheScripts::RadarZoomValue != 0
        && FindPlayerVehicle() == nullptr
        && trace.m_nBlipType != BLIP_OBJECT
        && trace.m_nBlipType != BLIP_PICKUP) {
        return;
    }

    // TODO(port): sprite w/h below come from float->int conversions whose source
    // expressions are not recoverable from the decomp (CGeneral::unk_00821b40
    // reads ST0); using 8px like DrawRadarSprite.
    const uint32_t spriteW = static_cast<uint32_t>(SCREEN_STRETCH_X(8.0f));
    const uint32_t spriteH = static_cast<uint32_t>(SCREEN_STRETCH_Y(8.0f));
    const CRGBA white{255, 255, 255, 255};

    if (arg1 == 0) {
        if (trace.m_nBlipSprite == RADAR_SPRITE_NONE
            && (!FrontEndMenuManager.m_bDrawingMap || FrontEndMenuManager.m_bMapLoaded)) {
            if (!searchlight) {
                if (isAirstrip) {
                    // Airstrip approach blip: light sprite at the slid position...
                    if (!FrontEndMenuManager.m_bDrawingMap && radarDist < 0.9f && distToAirstrip < 500.0f) {
                        DrawRotatingRadarSprite(RadarBlipSprites[RADAR_SPRITE_LIGHT],
                            screenPos.x, screenPos.y, 0.0f, spriteW, spriteH, white);
                    }
                    // ...and runway sprite at the trace's original position.
                    const CVector2D origRadar = TransformRealWorldPointToRadarSpace(
                        {trace.m_vPosition.x, trace.m_vPosition.y});
                    CVector2D lim = origRadar;
                    LimitRadarPoint(lim);
                    const CVector2D origScreen = TransformRadarPointToScreenSpace(lim);
                    const airstrip_info& strip = airstrip_table[airstrip_location];
                    if (!FrontEndMenuManager.m_bDrawingMap) {
                        if (s_airstripBlipOnScreen) {
                            return;
                        }
                        DrawRotatingRadarSprite(RadarBlipSprites[RADAR_SPRITE_RUNWAY],
                            origScreen.x, origScreen.y,
                            -m_fRadarOrientation - (strip.direction - 90.0f) * DegreesToRadians(1.0f),
                            spriteW, spriteH, white);
                    } else {
                        const float angle = (airstrip_location != AIRSTRIP_SF_AIRPORT)
                            ? (strip.direction - 90.0f) * DegreesToRadians(1.0f)
                            : strip.direction * DegreesToRadians(1.0f);
                        DrawRotatingRadarSprite(RadarBlipSprites[RADAR_SPRITE_RUNWAY],
                            origScreen.x, origScreen.y, angle, spriteW, spriteH, white);
                        AddBlipToLegendList(false, RADAR_SPRITE_RUNWAY);
                    }
                } else {
                    // Height-based trace for normal entity blips.
                    const float playerZ = FindPlayerCentreOfWorldForMap(0).z;
                    const float z = pos.z;
                    eRadarTraceHeight h;
                    if (z - 2.0f <= playerZ) { // TODO(port): verify 2.0f constant (_unk_00858ca0)
                        h = (playerZ <= z + 4.0f) ? RADAR_TRACE_NORMAL : RADAR_TRACE_HIGH;
                    } else {
                        h = RADAR_TRACE_LOW;
                    }
                    const CRGBA c = CRGBAFromInt(blipColor);
                    ShowRadarTraceWithHeight(screenPos.x, screenPos.y, trace.m_nBlipSize,
                                             c.r, c.g, c.b, c.a, h);
                }
            } else {
                // Searchlight: two concentric circles at the screen position.
                if (radarDist < 0.9f) {
                    if (FrontEndMenuManager.m_bDrawingMap) {
                        return;
                    }
                    const float radius =
                        (SCREEN_STRETCH_X(94.0f) / m_radarRange) * searchlight->m_fTargetRadius * 0.6f;
                    CSprite2d::DrawCircleAtNearClip(screenPos, radius, CRGBA{0, 0, 0, 0x96}, 15);
                    CSprite2d::DrawCircleAtNearClip(screenPos, radius - SCREEN_STRETCH_X(1.0f),
                                                    CRGBA{0xDC, 0xDC, 0xDC, 200}, 15);
                }
            }
            AddBlipToLegendList(true, blipIndex);
            return;
        }

        if (trace.m_nBlipSprite != RADAR_SPRITE_NONE && HasThisBlipBeenRevealed(blipIndex)) {
            DrawRadarSprite(trace.m_nBlipSprite, screenPos.x, screenPos.y, 255);
        }
    } else if (trace.m_bBlipRemain || radarDist < 1.0f) {
        // Rotating direction arrow for the entity.
        if (!FrontEndMenuManager.m_bDrawingMap || FrontEndMenuManager.m_bMapLoaded) {
            const float playerZ = FindPlayerCentreOfWorldForMap(0).z;
            const float z = pos.z;
            const CRGBA c = CRGBAFromInt(blipColor);
            if (z - 2.0f <= playerZ) {
                if (playerZ <= z + 4.0f) {
                    // TODO(port): 1.0 checks a player-state word here
                    // ((&DAT_00b6f1a8)[playerIdx * 0x11c] == 1) to pick the
                    // heading + PI variant; verify against the player port.
                    DrawRotatingRadarSprite(RadarBlipSprites[RADAR_SPRITE_CENTRE],
                        screenPos.x, screenPos.y,
                        heading - (m_fRadarOrientation + PI),
                        spriteW, spriteH, CRGBA{c.r, c.g, c.b, 255});
                    return;
                }
                if (CTheScripts::RadarShowBlipOnAllLevels) {
                    ShowRadarTraceWithHeight(screenPos.x, screenPos.y, trace.m_nBlipSize,
                                             c.r, c.g, c.b, c.a, RADAR_TRACE_HIGH);
                    return;
                }
            } else if (CTheScripts::RadarShowBlipOnAllLevels) {
                ShowRadarTraceWithHeight(screenPos.x, screenPos.y, trace.m_nBlipSize,
                                         c.r, c.g, c.b, c.a, RADAR_TRACE_LOW);
                return;
            }
        }
    }
}

// --- Save/load shims (not yet ported) -------------------------------------
// (CGenericGameStorage is defined in the shim section above.)
// EntryExit pool access for Load/Save. The 1.0 binary indexes the pool
// directly (slot size 0x3C); kept opaque until CEntryExitManager is ported.
struct CEntryExitPool {
    static CEntryExit* IndexToPointer(uint32_t poolIndex); // 1-based; null if free
    static uint32_t PointerToIndex(const CEntryExit* entryExit); // 1-based; 0 if invalid
};

void CRadar::ClearActualBlip(tRadarTrace& trace) {
    // src/CRadar/ClearActualBlip_00587c10.c (inlined core)
    // Decomp sets the raw flag byte to 0x01 (bright on, rest off) and ANDs the
    // display/type byte with 0xC0 (clears display flag and type, keeps appearance).
    trace.m_nBlipSize = 1;
    trace.m_bBright = true;
    trace.m_bTrackingBlip = false;
    trace.m_bShortRange = false;
    trace.m_bFriendly = false;
    trace.m_bBlipRemain = false;
    trace.m_bBlipFade = false;
    trace.m_nCoordBlipAppearance = 0;
    trace.m_fSphereRadius = 1.0f;
    trace.m_pEntryExit = nullptr;
    trace.m_nBlipSprite = RADAR_SPRITE_NONE;
    trace.m_nBlipDisplayFlag = static_cast<eBlipDisplay>(0);
    trace.m_nBlipType = static_cast<eBlipType>(0);
    // m_nAppearance preserved.
}

void CRadar::ClearActualBlip(int32_t blipIndex) {
    // src/CRadar/ClearActualBlip_00587c10.c
    if (blipIndex >= 0 && blipIndex < MAX_RADAR_TRACES) {
        ClearActualBlip(ms_RadarTrace[blipIndex]);
    }
}

void CRadar::ClearBlipForEntity(eBlipType blipType, int32_t entityHandle) {
    // src/CRadar/ClearBlipForEntity_00587c60.c
    for (int32_t i = 0; i < MAX_RADAR_TRACES; i++) {
        auto& trace = ms_RadarTrace[i];
        if (trace.m_nBlipType == blipType
            && trace.m_nEntityHandle == static_cast<uint32_t>(entityHandle)) {
            ClearActualBlip(trace);
        }
    }
}

void CRadar::ClearBlipForEntity(CPed* ped) {
    // No .c file in src/CRadar/; clears the CHAR blip tracking this ped.
    // TODO(port): verify the handle derivation against the CPed/pool port.
    // The entityHandle stored by SetEntityBlip is the pool index; using it directly.
    ClearBlipForEntity(BLIP_CHAR, GetPedPool()->GetIndex(ped));
}

void CRadar::ClearBlip(tBlipHandle blip) {
    // src/CRadar/ClearBlip_00587ce0.c
    if (blip == 0xFFFFFFFFu) {
        return;
    }
    const uint32_t blipIndex = blip & 0xFFFFu;
    if (blipIndex < MAX_RADAR_TRACES
        && static_cast<uint16_t>(blip >> 16) == ms_RadarTrace[blipIndex].m_nCounter
        && ms_RadarTrace[blipIndex].m_bTrackingBlip) {
        ClearActualBlip(static_cast<int32_t>(blipIndex));
    }
}

void CRadar::SetupAirstripBlips() {
    // src/CRadar/SetupAirstripBlips_00587d20.c
    const CVehicle* vehicle = FindPlayerVehicle();
    if (!vehicle || !vehicle->IsSubPlane() || vehicle->m_nModelIndex == 0x21b /* Vortex */) {
        // Not flying: remove the airstrip blip.
        // Decomp inlines GetActualBlipArrayIndex + ClearActualBlip here;
        // equivalent to ClearBlip.
        if (airstrip_blip != 0) {
            ClearBlip(airstrip_blip);
            airstrip_blip = 0;
        }
        return;
    }

    // Re-evaluate the nearest airstrip every 4th frame.
    // (gta-reversed changes this to a time-based check; the 1.0 binary uses
    // the frame counter.)
    if ((CTimer::m_FrameCounter & 4) != 0) {
        float distances[NUM_AIRSTRIPS]{};
        for (int32_t i = 0; i < NUM_AIRSTRIPS; i++) {
            distances[i] = DistanceBetweenPoints2D(airstrip_table[i].position, vehicle->GetPosition());
        }

        // Pick the strictly-closest airstrip; ties fall through to VERDANT_MEADOWS.
        // (Decomp is a nested-if decision tree; converted to the equivalent if/else chain.)
        eAirstripLocation nearest;
        if (!(distances[AIRSTRIP_SF_AIRPORT] <= distances[AIRSTRIP_LS_AIRPORT]
              || distances[AIRSTRIP_LV_AIRPORT] <= distances[AIRSTRIP_LS_AIRPORT]
              || distances[AIRSTRIP_VERDANT_MEADOWS] <= distances[AIRSTRIP_LS_AIRPORT])) {
            nearest = AIRSTRIP_LS_AIRPORT;
        } else if (!(distances[AIRSTRIP_LS_AIRPORT] <= distances[AIRSTRIP_SF_AIRPORT]
                     || distances[AIRSTRIP_LV_AIRPORT] <= distances[AIRSTRIP_SF_AIRPORT]
                     || distances[AIRSTRIP_VERDANT_MEADOWS] <= distances[AIRSTRIP_SF_AIRPORT])) {
            nearest = AIRSTRIP_SF_AIRPORT;
        } else if (!(distances[AIRSTRIP_LS_AIRPORT] <= distances[AIRSTRIP_LV_AIRPORT]
                     || distances[AIRSTRIP_SF_AIRPORT] <= distances[AIRSTRIP_LV_AIRPORT]
                     || distances[AIRSTRIP_VERDANT_MEADOWS] <= distances[AIRSTRIP_LV_AIRPORT])) {
            nearest = AIRSTRIP_LV_AIRPORT;
        } else {
            nearest = AIRSTRIP_VERDANT_MEADOWS;
        }

        if (airstrip_location != nearest) {
            if (airstrip_blip != 0) {
                ClearBlip(airstrip_blip);
                airstrip_location = nearest;
            }
            // Fall through to create the blip below.
        } else if (airstrip_blip != 0) {
            return;
        }
    } else if (airstrip_blip != 0) {
        return;
    }

    const airstrip_info& strip = airstrip_table[airstrip_location];
    airstrip_blip = SetCoordBlip(BLIP_AIRSTRIP,
        CVector{strip.position.x, strip.position.y, 0.0f},
        BLIP_COLOUR_RED, BLIP_DISPLAY_BLIPONLY, "CODEAIR");
}

void CRadar::DrawBlips() {
    // src/CRadar/DrawBlips_00588050.c
    // NOTE: the decomp's loop structure differs from gta-reversed's. The 1.0
    // binary does two arg1 passes (1 = arrows, then 0 = traces/sprites), each
    // with a 1..3 priority loop for DisplayThisBlip, plus a waypoint pass.
    // Converted from the decomp.
    SetupAirstripBlips();

    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(false));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(false));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(true));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(false));

    const bool mouseLButtonPressed = CPad::NewMouseControllerState.isMouseLeftButtonPressed;

    // Map-crosshair bounds (float->int via CGeneral::unk_00821b40 in the decomp).
    const int32_t mouseMinX = FloatToInt(SCREEN_STRETCH_X(60.0f));
    const int32_t mouseMaxX = FloatToInt(SCREEN_STRETCH_FROM_RIGHT(60.0f));
    const int32_t mouseMinY = FloatToInt(SCREEN_STRETCH_Y(60.0f));
    const int32_t mouseMaxY = FloatToInt(SCREEN_STRETCH_FROM_BOTTOM(60.0f));

    if (!FrontEndMenuManager.m_bDrawingMap) {
        // North blip at the top of the radar disc.
        CVector2D northRadar = TransformRealWorldPointToRadarSpace({
            vec2DRadarOrigin.x,
            vec2DRadarOrigin.y + m_radarRange * 1.4142135f, // SQRT_2
        });
        LimitRadarPoint(northRadar);
        const CVector2D northScreen = TransformRadarPointToScreenSpace(northRadar);
        DrawRadarSprite(RADAR_SPRITE_NORTH, northScreen.x, northScreen.y, 255);
    } else if (FrontEndMenuManager.m_bMapLoaded
               && ((!mouseLButtonPressed
                    && mouseMinX < FrontEndMenuManager.m_nMousePosX
                    && FrontEndMenuManager.m_nMousePosX < mouseMaxX
                    && mouseMinY < FrontEndMenuManager.m_nMousePosY
                    && FrontEndMenuManager.m_nMousePosY < mouseMaxY)
                   || !FrontEndMenuManager.m_DisplayTheMouse)) {
        // Crosshair at the mouse position on the map.
        CVector2D drawPos = TransformRealWorldPointToRadarSpace({
            FrontEndMenuManager.m_vMousePos.x,
            FrontEndMenuManager.m_vMousePos.y,
        });
        LimitToMap(drawPos.x, drawPos.y);
        const CRGBA gold = HudColour.GetRGB(HUD_COLOUR_GOLD);
        CSprite2d::DrawRect(CRect{
            SCREEN_STRETCH_X(drawPos.x) - 1.0f, 0.0f,
            SCREEN_STRETCH_X(drawPos.x) + 1.0f, static_cast<float>(SCREEN_HEIGHT),
        }, gold);
        CSprite2d::DrawRect(CRect{
            0.0f, SCREEN_STRETCH_Y(drawPos.y) - 1.0f,
            static_cast<float>(SCREEN_WIDTH), SCREEN_STRETCH_Y(drawPos.y) + 1.0f,
        }, gold);
    }

    // Two passes: arg1=1 draws arrows, arg1=0 draws traces/sprites.
    for (int32_t pass = 0; pass < 2; pass++) {
        const uint8_t arg1 = (pass == 0) ? 1 : 0;
        for (int8_t priority = 1; priority <= 3; priority++) {
            for (int32_t i = 0; i < MAX_RADAR_TRACES; i++) {
                const auto& trace = ms_RadarTrace[i];
                if (!trace.m_bTrackingBlip) {
                    continue;
                }
                switch (trace.m_nBlipType) {
                case BLIP_CAR:
                case BLIP_CHAR:
                case BLIP_OBJECT:
                case BLIP_PICKUP:
                    if (DisplayThisBlip(trace.m_nBlipSprite, priority)) {
                        DrawEntityBlip(i, arg1);
                    }
                    break;
                case BLIP_COORD:
                case BLIP_CONTACTPOINT:
                    if (trace.m_nBlipSprite != RADAR_SPRITE_WAYPOINT
                        && DisplayThisBlip(trace.m_nBlipSprite, priority)) {
                        DrawCoordBlip(i, arg1 != 0);
                    }
                    break;
                case BLIP_SPOTLIGHT:
                case BLIP_AIRSTRIP:
                    if (priority == 3
                        && (!CTheScripts::bPlayerIsOffTheMap || !FrontEndMenuManager.m_bDrawingMap)) {
                        DrawEntityBlip(i, arg1);
                    }
                    break;
                default:
                    break;
                }
            }
        }

        // Waypoint blips (sprite == WAYPOINT) are drawn in a separate pass.
        for (int32_t i = 0; i < MAX_RADAR_TRACES; i++) {
            const auto& trace = ms_RadarTrace[i];
            if (trace.m_bTrackingBlip
                && (trace.m_nBlipType == BLIP_COORD || trace.m_nBlipType == BLIP_CONTACTPOINT)
                && trace.m_nBlipSprite == RADAR_SPRITE_WAYPOINT) {
                if (CGame::currArea == 0) {
                    FindPlayerPed(-1); // return value discarded in the decomp
                }
                DrawCoordBlip(i, arg1 != 0);
            }
        }
    }

    if (FrontEndMenuManager.m_bDrawingMap) {
        // "You are here" marker on the map. The transform is done manually
        // because TransformRealWorldPointToRadarSpace early-outs when drawingMap.
        const CVector p = FindPlayerCentreOfWorldForMap(0);
        const CVector2D radar{
            cachedCos * (p.x - vec2DRadarOrigin.x) / m_radarRange
                + cachedSin * (p.y - vec2DRadarOrigin.y) / m_radarRange,
            (p.y - vec2DRadarOrigin.y) / m_radarRange * cachedCos
                - cachedSin * (p.x - vec2DRadarOrigin.x) / m_radarRange,
        };
        const CVector2D screen = TransformRadarPointToScreenSpace(radar);
        DrawYouAreHereSprite(screen.x, screen.y);
        return;
    }

    // Player markers (radar mode).
    for (int32_t playerId = 0; playerId < 2; playerId++) {
        if (!FindPlayerPed(playerId)) {
            continue;
        }
        const CVehicle* vehicle = FindPlayerVehicle(playerId);
        if (vehicle && vehicle->IsSubPlane() && vehicle->m_nModelIndex != 0x21b /* Vortex */) {
            continue;
        }
        // NOTE: the 1.0 binary uses player 0's position for both markers
        // (FindPlayerCentreOfWorldForMap(..., 0) is hardcoded).
        const CVector p = FindPlayerCentreOfWorldForMap(0);
        CVector2D radar{
            (p.x - vec2DRadarOrigin.x) / m_radarRange,
            (p.y - vec2DRadarOrigin.y) / m_radarRange,
        };
        radar = {
            radar.x * cachedCos + cachedSin * radar.y,
            radar.y * cachedCos - radar.x * cachedSin,
        };
        const float mag = CVector2DMagnitude(radar);
        if (mag > 1.0f) {
            radar = radar / mag;
        }
        const CVector2D screen = TransformRadarPointToScreenSpace(radar);
        const float heading = FindPlayerHeading(playerId);
        const CRGBA color = CPlayerPed::IsHidden(CWorld::Players[playerId].m_pPed)
            ? CRGBA{0x32, 0x32, 0x50, 0xFF}
            : CRGBA{0xFF, 0xFF, 0xFF, 0xFF};
        // TODO(port): width comes from a float->int whose source is not
        // recoverable from the decomp (CGeneral::unk_00821b40 reads ST0);
        // using 8px like DrawRadarSprite. The player-state word check
        // ((&DAT_00b6f1a8)[...] == 1, picks heading + PI) also needs the
        // player port to resolve; using the heading - (orientation + PI) branch.
        const uint32_t width = static_cast<uint32_t>(SCREEN_STRETCH_X(8.0f));
        const float angle = heading - (m_fRadarOrientation + PI);
        DrawRotatingRadarSprite(RadarBlipSprites[RADAR_SPRITE_CENTRE],
            screen.x, screen.y, angle, width, width, color);
    }
}

bool CRadar::Load() {
    // src/CRadar/Load_005d53c0.c
    Initialise();
    for (int32_t i = 0; i < MAX_RADAR_TRACES; i++) {
        auto& trace = ms_RadarTrace[i];
        CGenericGameStorage::LoadDataFromWorkBuffer(trace);
        const uint32_t poolIndex = trace.m_EntryExitPoolInd;
        if (poolIndex != 0) {
            trace.m_pEntryExit = CEntryExitPool::IndexToPointer(poolIndex);
        }
    }
    return true;
}

bool CRadar::Save() {
    // src/CRadar/Save_005d5860.c
    for (int32_t i = 0; i < MAX_RADAR_TRACES; i++) {
        auto& trace = ms_RadarTrace[i];

        // Temporarily swap the EntryExit pointer for its 1-based pool index.
        CEntryExit* savedEntryExit = nullptr;
        if (trace.m_pEntryExit) {
            const uint32_t poolIndex = CEntryExitPool::PointerToIndex(trace.m_pEntryExit);
            if (poolIndex != 0) {
                savedEntryExit = trace.m_pEntryExit;
                trace.m_EntryExitPoolInd = poolIndex;
            }
        }

        // Clear the tracking bit for non-coord blips before saving.
        bool wasTracking = false;
        if (trace.m_nBlipType != BLIP_COORD && trace.m_nBlipType != BLIP_CONTACTPOINT
            && trace.m_bTrackingBlip) {
            trace.m_bTrackingBlip = false;
            wasTracking = true;
        }

        CGenericGameStorage::SaveDataToWorkBuffer(&trace, sizeof(trace));

        if (savedEntryExit) {
            trace.m_pEntryExit = savedEntryExit;
        }
        if (wasTracking) {
            trace.m_bTrackingBlip = true;
        }
    }
    return true;
}

const GxtChar* CRadar::GetBlipName(eRadarSprite sprite) {
    // No .c file in src/CRadar/; mapping verified against
    // src/CRadar/DrawLegend_005828a0.c and gta-reversed (Radar.cpp:2057).
    switch (sprite) {
    case RADAR_SPRITE_MAP_HERE:      return TheText.Get("LG_01");
    case RADAR_SPRITE_AIRYARD:       return TheText.Get("LG_02");
    case RADAR_SPRITE_AMMUGUN:       return TheText.Get("LG_03");
    case RADAR_SPRITE_BARBERS:       return TheText.Get("LG_04");
    case RADAR_SPRITE_BIGSMOKE:      return TheText.Get("LG_05");
    case RADAR_SPRITE_BOATYARD:      return TheText.Get("LG_06");
    case RADAR_SPRITE_BURGERSHOT:    return TheText.Get("LG_07");
    case RADAR_SPRITE_CATALINAPINK:  return TheText.Get("LG_09");
    case RADAR_SPRITE_CESARVIAPANDO: return TheText.Get("LG_10");
    case RADAR_SPRITE_CHICKEN:       return TheText.Get("LG_11");
    case RADAR_SPRITE_CJ:            return TheText.Get("LG_12");
    case RADAR_SPRITE_CRASH1:        return TheText.Get("LG_13");
    case RADAR_SPRITE_EMMETGUN:      return TheText.Get("LG_15");
    case RADAR_SPRITE_ENEMYATTACK:   return TheText.Get("LG_16");
    case RADAR_SPRITE_FIRE:          return TheText.Get("LG_17");
    case RADAR_SPRITE_GIRLFRIEND:    return TheText.Get("LG_18");
    case RADAR_SPRITE_HOSTPITAL:     return TheText.Get("LG_19");
    case RADAR_SPRITE_LOGOSYNDICATE: return TheText.Get("LG_20");
    case RADAR_SPRITE_MADDOG:        return TheText.Get("LG_21");
    case RADAR_SPRITE_MAFIACASINO:   return TheText.Get("LG_22");
    case RADAR_SPRITE_MCSTRAP:       return TheText.Get("LG_23");
    case RADAR_SPRITE_MODGARAGE:     return TheText.Get("LG_24");
    case RADAR_SPRITE_OGLOC:         return TheText.Get("LG_25");
    case RADAR_SPRITE_PIZZA:         return TheText.Get("LG_26");
    case RADAR_SPRITE_POLICE:        return TheText.Get("LG_27");
    case RADAR_SPRITE_PROPERTYG:     return TheText.Get("LG_28");
    case RADAR_SPRITE_PROPERTYR:     return TheText.Get("LG_29");
    case RADAR_SPRITE_RACE:          return TheText.Get("LG_30");
    case RADAR_SPRITE_RYDER:         return TheText.Get("LG_31");
    case RADAR_SPRITE_SAVEGAME:      return TheText.Get("LG_32");
    case RADAR_SPRITE_SCHOOL:        return TheText.Get("LG_33");
    case RADAR_SPRITE_SPRAY:         return TheText.Get("LG_34");
    case RADAR_SPRITE_SWEET:         return TheText.Get("LG_35");
    case RADAR_SPRITE_TATTOO:        return TheText.Get("LG_36");
    case RADAR_SPRITE_THETRUTH:      return TheText.Get("LG_37");
    case RADAR_SPRITE_TORENORANCH:   return TheText.Get("LG_39");
    case RADAR_SPRITE_TRIADS:        return TheText.Get("LG_40");
    case RADAR_SPRITE_TRIADSCASINO:  return TheText.Get("LG_41");
    case RADAR_SPRITE_TSHIRT:        return TheText.Get("LG_42");
    case RADAR_SPRITE_WOOZIE:        return TheText.Get("LG_43");
    case RADAR_SPRITE_ZERO:          return TheText.Get("LG_44");
    case RADAR_SPRITE_DATEDISCO:     return TheText.Get("LG_45");
    case RADAR_SPRITE_DATEDRINK:     return TheText.Get("LG_46");
    case RADAR_SPRITE_DATEFOOD:      return TheText.Get("LG_47");
    case RADAR_SPRITE_TRUCK:         return TheText.Get("LG_48");
    case RADAR_SPRITE_DESTINATION:   return TheText.Get("LG_49");
    case RADAR_SPRITE_OBJECT:        return TheText.Get("LG_50");
    case RADAR_SPRITE_CASH:          return TheText.Get("LG_51");
    case RADAR_SPRITE_FLAG:          return TheText.Get("LG_52");
    case RADAR_SPRITE_GYM:           return TheText.Get("LG_53");
    case RADAR_SPRITE_FRIEND:        return TheText.Get("LG_54");
    case RADAR_SPRITE_THREAT:        return TheText.Get("LG_55");
    case RADAR_SPRITE_PLAYER_INTEREST: return TheText.Get("LG_56");
    case RADAR_SPRITE_IMPOUND:       return TheText.Get("LG_57");
    case RADAR_SPRITE_GANGB:         return TheText.Get("LG_58");
    case RADAR_SPRITE_GANGP:         return TheText.Get("LG_59");
    case RADAR_SPRITE_GANGY:         return TheText.Get("LG_60");
    case RADAR_SPRITE_GANGN:         return TheText.Get("LG_61");
    case RADAR_SPRITE_GANGG:         return TheText.Get("LG_62");
    case RADAR_SPRITE_QMARK:         return TheText.Get("LG_63");
    case RADAR_SPRITE_WAYPOINT:      return TheText.Get("LG_64");
    case RADAR_SPRITE_RUNWAY:        return TheText.Get("LG_65");
    case RADAR_SPRITE_BULLDOZER:      return TheText.Get("LG_66");
    case RADAR_SPRITE_DINER:         return TheText.Get("LG_67");
    default:
        break;
    }
    return nullptr;
}

int32_t CRadar::FindTraceNotTrackingBlipIndex() {
    // No .c file in src/CRadar/; trivial scan per the header doc comment.
    for (int32_t i = 0; i < MAX_RADAR_TRACES; i++) {
        if (!ms_RadarTrace[i].m_bTrackingBlip) {
            return i;
        }
    }
    return -1;
}

// --- Free functions --------------------------------------------------------
// No .c files in src/CRadar/; from gta-reversed (Radar.cpp), which matches the
// 1.0 addresses noted there.

// 0x584B00: returns true if either coord had to be clipped.
bool ClipRadarTileCoords(int32_t& x, int32_t& y) {
    const int32_t ox = x, oy = y;
    x = std::clamp(x, 0, static_cast<int32_t>(MAX_RADAR_WIDTH_TILES) - 1);
    y = std::clamp(y, 0, static_cast<int32_t>(MAX_RADAR_HEIGHT_TILES) - 1);
    return ox != x || oy != y;
}

// 0x584D40: point inside the radar rect (spans -1..1 in both directions).
bool IsPointInsideRadar(const CVector2D& point) {
    return std::abs(point.x) < 1.0f && std::abs(point.y) < 1.0f;
}

// 0x584D90: corners of tile (x, y) in world coordinates.
void GetTextureCorners(int32_t x, int32_t y, CVector2D* corners) {
    corners[0] = {500.0f * static_cast<float>(x - 6), 500.0f * static_cast<float>(5 - y)};
    corners[1] = {500.0f * static_cast<float>(x - 5), 500.0f * static_cast<float>(5 - y)};
    corners[2] = {500.0f * static_cast<float>(x - 5), 500.0f * static_cast<float>(6 - y)};
    corners[3] = {500.0f * static_cast<float>(x - 6), 500.0f * static_cast<float>(6 - y)};
}

// 0x584E00: clips a line to the radar box; returns the side hit (or -1).
int32_t LineRadarBoxCollision(CVector2D& result, const CVector2D& lineStart, const CVector2D& lineEnd) {
    float closestIntersectionParam = 1.0f;
    int32_t intersectionSide = -1;

    const float deltaY = lineEnd.y - lineStart.y;
    const float deltaX = lineEnd.x - lineStart.x;

    const auto CheckIntersection = [&](float edgeCoord, bool isX, float startDist, float endDist, int32_t side) {
        if (startDist * endDist < 0.0f) {
            const float t = startDist / (startDist - endDist);
            const float v = (isX ? deltaY : deltaX) * t + (isX ? lineStart.y : lineStart.x);
            if (v >= -1.0f && v <= 1.0f && t >= 0.0f && t <= closestIntersectionParam) {
                closestIntersectionParam = t;
                if (isX) {
                    result.x = edgeCoord;
                    result.y = v;
                } else {
                    result.x = v;
                    result.y = edgeCoord;
                }
                intersectionSide = side;
                return t == 0.0f;
            }
        }
        return false;
    };

    if (CheckIntersection(-1.0f, true, -1.0f - lineStart.x, -1.0f - lineEnd.x, 3)) {
        return 3;
    }
    if (CheckIntersection(1.0f, true, lineStart.x - 1.0f, lineEnd.x - 1.0f, 1)) {
        return 1;
    }
    if (CheckIntersection(-1.0f, false, -1.0f - lineStart.y, -1.0f - lineEnd.y, 0)) {
        return 0;
    }
    if (CheckIntersection(1.0f, false, lineStart.y - 1.0f, lineEnd.y - 1.0f, 2)) {
        return 2;
    }
    return intersectionSide;
}
