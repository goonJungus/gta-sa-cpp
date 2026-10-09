// CPostEffects.cpp - GTA SA 1.0 clean-room C++ conversion
// Method bodies ported from src/CPostEffects/*.c with guidance from
// gta-reversed/source/game_sa/PostEffects.cpp.
// See BUILD_NOTES.md (2026-10-09) for divergences found during verification.
//
// C++17 (project rule): no designated initializers, no std::bit_ceil,
// no std::ranges. gta-reversed's StaticRef<T>(addr) become file-local
// statics; plugin-sdk notsa:: helpers are inlined.

#include "CPostEffects.h"

#include "CTimer.h"
#include "CWeather.h"
#include "CVector.h" // CVector (FindPlayerSpeed)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib> // std::srand
#include <utility> // std::exchange

// ============================================================================
// TODO(port): external subsystem shims. Minimal declarations verified against
// gta-reversed and the decompiled code; delete entries as subsystems are ported.
// Follows the CWeapon.cpp shim pattern (declarations only).
// ============================================================================

// --- RenderWare render states (numeric IDs from the decompiled code) ---
enum RwRenderState {
    rwRENDERSTATETEXTURERASTER      = 1,
    rwRENDERSTATETEXTUREADDRESS     = 2,
    rwRENDERSTATEZTESTENABLE        = 6,
    rwRENDERSTATESHADEMODE          = 7,
    rwRENDERSTATEZWRITEENABLE       = 8,
    rwRENDERSTATETEXTUREFILTER      = 9,
    rwRENDERSTATESRCBLEND           = 10,
    rwRENDERSTATEDESTBLEND          = 11,
    rwRENDERSTATEVERTEXALPHAENABLE  = 12,
    rwRENDERSTATEFOGENABLE          = 14,
    rwRENDERSTATECULLMODE           = 20,
    rwRENDERSTATESTENCILPASS        = 21, // 0x15
    rwRENDERSTATESTENCILFUNCTION    = 22, // 0x16
    rwRENDERSTATESTENCILFUNCTIONREF = 23, // 0x17
    rwRENDERSTATESTENCILFUNCTIONMASK = 24, // 0x18
    rwRENDERSTATESTENCILFUNCTIONWRITEMASK = 25, // 0x19
    rwRENDERSTATEALPHATESTENABLE   = 26, // 0x1a
};

// RenderWare's RWRSTATE macro casts small ints/pointers to void*.
#define RWRSTATE(v) (reinterpret_cast<void*>(v))

void RwRenderStateSet(RwRenderState state, void* value);
void RwRenderStateGet(RwRenderState state, void* outValue);

// --- RenderWare raster/camera/immediate-mode ---
struct RwCamera; // opaque
struct RwTexture; // opaque
// RwRaster is forward-declared in RenderTypes.h; do not redefine.

RwRaster* RwRasterCreate(int32_t width, int32_t height, int32_t depth, int32_t flags);
uint8_t*  RwRasterLock(RwRaster* raster, uint8_t level, int32_t lockMode);
void      RwRasterUnlock(RwRaster* raster);
void      RwRasterDestroy(RwRaster* raster);
int32_t   RwRasterGetWidth(RwRaster* raster);
int32_t   RwRasterGetHeight(RwRaster* raster);
int32_t   RwRasterGetDepth(RwRaster* raster);
RwRaster* RwCameraGetRaster(RwCamera* camera);
float     RwCameraGetNearClipPlane(RwCamera* camera);
void      RwCameraClear(RwCamera* camera, RwRGBA* color, int32_t clearMode);
void      RwCameraEndUpdate(RwCamera* camera);
void      RsCameraBeginUpdate(RwCamera* camera);
void      RwRasterPushContext(RwRaster* raster);
void      RwRasterPopContext();
void      RwRasterRenderFast(RwRaster* raster, int32_t x, int32_t y);
float     RwIm2DGetNearScreenZ();
void      RwIm2DRenderPrimitive(int32_t primType, RwIm2DVertex* verts, int32_t count);
void      RwIm2DRenderIndexedPrimitive(int32_t primType, RwIm2DVertex* verts, int32_t numVerts,
                                       uint16_t* indices, int32_t numIndices);
void      RwIm2DVertexSetRealRGBA(RwIm2DVertex* vert, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
RwRaster* RwTextureGetRaster(RwTexture* texture);
float     RwDeviceGetZBufferNear(); // RwEngineInstance->dOpenDevice.zBufferNear

constexpr int32_t rwRASTERFORMAT8888  = 0x500;
constexpr int32_t rwRASTERTYPEZBUFFER  = 0x01; // TODO(port): verify value (decomp uses flags=5 = ZBUFFER|TEXTURE)
constexpr int32_t rwRASTERTYPETEXTURE  = 0x04;
constexpr int32_t rwRASTERLOCKWRITE   = 1;
constexpr int32_t rwPRIMTYPETRILIST    = 1;
constexpr int32_t rwPRIMTYPETRISTRIP   = 2;

// --- Scene / RsGlobal ---
static struct {
    RwCamera* m_pRwCamera = nullptr;
} Scene;
static struct {
    int32_t maximumWidth = 0;
    int32_t maximumHeight = 0;
} RsGlobal;

#define SCREEN_WIDTH  (RsGlobal.maximumWidth)
#define SCREEN_HEIGHT (RsGlobal.maximumHeight)
#define SCREEN_STRETCH_X(x) ((x) * SCREEN_WIDTH / 640.0f)
#define SCREEN_STRETCH_Y(y) ((y) * SCREEN_HEIGHT / 448.0f)

// --- Misc small helpers ---
template<typename T>
static T sq(T x) { return x * x; }

static uint32_t BitCeil(uint32_t v) { // std::bit_ceil (C++20) replacement
    if (v <= 1) return 1;
    v--;
    v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16;
    return v + 1;
}

static float DegreesToRadians(float deg) { return deg * 3.14159265f / 180.0f; }

static RwRGBA PackARGB(uint32_t argb) {
    RwRGBA c;
    c.alpha = static_cast<uint8_t>((argb >> 24) & 0xFF);
    c.red   = static_cast<uint8_t>((argb >> 16) & 0xFF);
    c.green = static_cast<uint8_t>((argb >> 8) & 0xFF);
    c.blue  = static_cast<uint8_t>(argb & 0xFF);
    return c;
}

static void SetRGBA(CRGBA& c, uint8_t r, uint8_t g, uint8_t b) {
    c.r = r; c.g = g; c.b = b;
}

static void ScaleRGB(CRGBA& c, float s) {
    c.r = static_cast<uint8_t>(c.r * s);
    c.g = static_cast<uint8_t>(c.g * s);
    c.b = static_cast<uint8_t>(c.b * s);
}

// --- CGeneral ---
struct CGeneral {
    static uint32_t GetRandomNumber();
    static int32_t  GetRandomNumberInRange(int32_t min, int32_t max);
};
// Decompiled as CGeneral::unk_00821b40: bare call whose result is used as a
// small random int (heat-haze jitter / particle speeds). TODO(port): identify.
static int32_t Unk_00821b40() {
    // TODO(port): stub returns 0; replace with the real function.
    return 0;
}

// --- Time cycle / game / zones / clock ---
struct CTimeCycle {
    static float GetDayNightBalance();
    static void  SetDayNightBalance(float balance);
    static float GetHighLightMinIntensity();
    static CRGBA GetCurrentSkyBottomColor();
    static std::pair<CRGBA, CRGBA> GetPostFxColors(); // pass1, pass2
};
struct CGame {
    static bool CanSeeOutSideFromCurrArea();
};
struct CCullZones {
    static bool CamNoRain();
    static bool PlayerNoRain();
};
struct CClock {
    static int32_t GetGameClockHours();
};
struct CClouds {
    struct Volumetric { RwTexture* texture; };
    static Volumetric ms_vc;
};
struct CWaterLevel {
    static bool m_bWaterFogScript;
};
struct CCutsceneMgr {
    static bool ms_running;
    static bool ms_cutsceneProcessing;
};
struct CCustomBuildingDNPipeline {
    static float m_fDNBalanceParam;
};

// --- Player / vehicle access ---
// TODO(port): CPed.h needs C++20 (bitfield initializers); CVehicle members
// (m_nVehicleType, handlingFlags, m_GasPedal, ...) are not ported yet.
// These accessors bridge the gap; replace with direct member access later.
class CPlayerPed; // defined in the player subsystem
CPlayerPed* FindPlayerPed();
class CVehicle; // defined in the vehicle subsystem
CVehicle*   FindPlayerVehicle();
CVector     FindPlayerSpeed();
float       PedGetLightingFromCol(const CPlayerPed* ped, bool interior);
int32_t     VehicleGetType(const CVehicle* veh);          // m_nVehicleType
bool        VehicleHasNos(const CVehicle* veh);          // handlingFlags.bNosInst
float       VehicleGetTireTemperature(const CVehicle* veh); // AsAutomobile()->m_fTireTemperature
float       VehicleGetGasPedal(const CVehicle* veh);     // m_GasPedal
CVector     VehicleGetMoveSpeed(const CVehicle* veh);
CVector     VehicleGetForward(const CVehicle* veh);
// CPed state access for InfraredVisionStoreAndSetLightsForHeatObjects
int32_t     PedGetState(const CPed* ped);                 // m_nPedState
uint32_t    PedGetDeathTimeMS(const CPed* ped);           // m_nDeathTimeMS

enum : int32_t {
    VEHICLE_TYPE_AUTOMOBILE = 0,
    VEHICLE_TYPE_HELI       = 3,
    VEHICLE_TYPE_PLANE      = 4,
    VEHICLE_TYPE_BOAT       = 5,
    VEHICLE_TYPE_TRAIN      = 6,
};
enum : int32_t {
    PEDSTATE_DEAD = 54, // TODO(port): verify against ePedState.h
};

// --- Camera ---
struct CCameraShim {
    int32_t m_nMode;
};
CCameraShim& GetActiveCameraShim(); // CCamera::GetActiveCamera()

// --- Effects / menu / file / fx ---
struct FxManager {
    bool m_bHeatHazeEnabled;
};
extern FxManager g_fxMan;
struct FrontEndMenuManager_t {
    bool m_bIsSaveDone;
    bool m_bSavePhotos;
    bool m_bActivateMenuNextFrame;
};
extern FrontEndMenuManager_t FrontEndMenuManager;
struct CWeaponShim {
    static bool ms_bTakePhoto;
};
struct CFileMgr {
    static void SetDirMyDocuments();
    static void SetDir(const char* dir);
};
void JPegCompressScreenToFile(RwCamera* camera, const char* filename);
struct CSpecialFX {
    static bool bSnapShotActive;
    static bool SnapShotFrames;
};
struct CVisibilityPlugins {
    // RenderBuffer::RenderStuffInBuffer variants (0x734E90 / 0x734EA0)
    static void RenderTempBuffer(uint32_t primType, uint32_t stride, uint32_t numVerts);
    static void RenderTempBufferIndexed(uint32_t primType, uint32_t stride, uint32_t numVerts,
                                        uint16_t* indices, uint32_t numIndices);
    static void RenderWeaponPedsForPC();
    static void ResetWeaponPedsForPC();
};
// Infrared/night-vision light helpers (decompiled, bodies owned by the
// light/pipeline subsystem).
void SetLightsForNightVision();
void SetLightsForInfraredVisionDefaultObjects();
void SetLightsForInfraredVisionHeatObjects();
void StoreAndSetLightsForInfraredVisionHeatObjects();
void RestoreLightsForInfraredVisionHeatObjects();

// --- Shared temp vertex/index buffers ---
// The decompiled HeatHazeFX/Radiosity/UnderWaterRipple all fill
// CGlass::ReflectionPolyVertexBuffer (28-byte RwIm2DVertex entries) starting at
// index 0x200. The decompiler's field labels for this buffer are inconsistent,
// so this port addresses the documented RwIm2DVertex fields (x, y, z, rhw,
// emissiveColor, u, v) at the exact byte offsets from the decomp.
static RwIm2DVertex s_TempVertices[0x200 + 2048];
static uint16_t     s_TempIndices[4096];
static uint32_t     s_uiTempBufferVerticesStored = 0;
static uint32_t     s_uiTempBufferIndicesStored = 0;
// HeatHazeFX particle state
static int32_t s_hpX[180];
static int32_t s_hpY[180];
static int32_t s_hpS[180];
// DAT_00c402bb: HeatHazeFX path selector (0 = particle path). TODO(port): find
// the writer that sets it to 1 (alpha-mask fullscreen path).
static bool s_bHeatHazeUseFullscreenPath = false;

// Fullscreen quad used by SetFilterMainColour / ColourFilter.
static std::array<RwIm2DVertex, 4> s_ccVertices;
static const std::array<uint16_t, 6> s_ccIndices = { 0, 1, 2, 0, 2, 3 };

// SpeedFX tuning table (decomp reads 7 entries at 0x8D5190, each 16 bytes:
// { speedThreshold, passes, uvExpand, randomJitter }). The VALUES live in
// .rdata and were not exported by the decompiler.
// TODO(port): fill from the binary (gta_sa.exe 1.0, .rdata @ 0x8D5190).
struct SpeedFXEntry {
    float   speedThreshold;
    int32_t passes;       // iVar4: number of draw iterations
    int32_t uvExpand;     // iVar18: UV expansion factor
    int32_t randomJitter; // iVar20: random UV jitter factor
};
static const SpeedFXEntry s_SpeedFXTable[7] = {}; // TODO(port): values

static char s_gString[256]; // gString scratch buffer (photo filename)

constexpr int32_t GRAIN_TEXTURE_DIM = 256; // 0x100

// ============================================================================
// Static member definitions (addresses = GTA SA 1.0, kept for reference).
// ============================================================================
float CPostEffects::SCREEN_EXTRA_MULT_CHANGE_RATE = 0.0005f; // 0x8D5168
float CPostEffects::SCREEN_EXTRA_MULT_BASE_CAP = 0.35f;      // 0x8D516C
float CPostEffects::SCREEN_EXTRA_MULT_BASE_MULT = 1.0f;     // 0x8D5170

bool CPostEffects::m_bDisableAllPostEffect = false; // 0xC402CF
bool CPostEffects::m_bSavePhotoFromScript = false;  // 0xC402D0
bool CPostEffects::m_bInCutscene = false;            // 0xC402B7

float CPostEffects::m_xoffset = 4.0f;  // 0x8D5130
float CPostEffects::m_yoffset = 24.0f; // 0x8D5134

float CPostEffects::m_colour1Multiplier = 1.0f; // 0x8D5160
float CPostEffects::m_colour2Multiplier = 1.0f; // 0x8D5164
float CPostEffects::m_colourLeftUOffset = 8.0f;  // 0x8D5150
float CPostEffects::m_colourRightUOffset = 8.0f; // 0x8D5154
float CPostEffects::m_colourTopVOffset = 8.0f;   // 0x8D5158
float CPostEffects::m_colourBottomVOffset = 8.0f; // 0x8D515C

bool     CPostEffects::m_bNightVision = false;                // 0xC402B8
float    CPostEffects::m_fNightVisionSwitchOnFXCount = 0.0f;  // 0xC40300
float    CPostEffects::m_fNightVisionSwitchOnFXTime = 50.0f;  // 0x8D50B0
int32_t  CPostEffects::m_NightVisionGrainStrength = 48;       // 0x8D50A8
CRGBA    CPostEffects::m_NightVisionMainCol{ 255, 0, 130, 0 }; // 0x8D50AC

bool     CPostEffects::m_bDarknessFilter = false;             // 0xC402C4
int32_t  CPostEffects::m_DarknessFilterAlpha = 170;           // 0x8D5204
int32_t  CPostEffects::m_DarknessFilterAlphaDefault = 170;    // 0x8D50F4
int32_t  CPostEffects::m_DarknessFilterRadiosityIntensityLimit = 45; // 0x8D50F8

float CPostEffects::m_fWaterFXStartUnderWaterness = 0.535f; // 0x8D514C
float CPostEffects::m_fWaterFullDarknessDepth = 90.0f;      // 0x8D5148
bool  CPostEffects::m_bWaterDepthDarkness = true;           // 0x8D5144

bool     CPostEffects::m_bHeatHazeFX = false;         // 0xC402BA
int32_t  CPostEffects::m_HeatHazeFXSpeedMin = 6;      // 0x8D50EC
int32_t  CPostEffects::m_HeatHazeFXSpeedMax = 10;     // 0x8D50F0
int32_t  CPostEffects::m_HeatHazeFXIntensity = 150;   // 0x8D50E8
int32_t  CPostEffects::m_HeatHazeFXType = 0;          // 0xC402BC
int32_t  CPostEffects::m_HeatHazeFXTypeLast = -1;     // 0x8D50E4
int32_t  CPostEffects::m_HeatHazeFXRandomShift = 0;   // 0xC402C0
int32_t  CPostEffects::m_HeatHazeFXScanSizeX = 0;     // 0xC40304
int32_t  CPostEffects::m_HeatHazeFXScanSizeY = 0;     // 0xC40308
int32_t  CPostEffects::m_HeatHazeFXRenderSizeX = 0;   // 0xC4030C
int32_t  CPostEffects::m_HeatHazeFXRenderSizeY = 0;   // 0xC40310

bool CPostEffects::m_bFog = false; // 0xC402C6

bool     CPostEffects::m_bSpeedFX = true;                      // 0x8D5100
bool     CPostEffects::m_bSpeedFXTestMode = false;             // 0xC402C7
bool     CPostEffects::m_bSpeedFXUserFlag = true;              // 0x8D5108
bool     CPostEffects::m_bSpeedFXUserFlagCurrentFrame = true;  // 0x8D5109
float    CPostEffects::m_fSpeedFXManualSpeedCurrentFrame = 0.0f; // 0xC402C8
int32_t  CPostEffects::m_SpeedFXAlpha = 36;                   // 0x8D5104

RwRaster* CPostEffects::pRasterFrontBuffer = nullptr; // 0xC402D8

bool                  CPostEffects::m_bGrainEnable = false;   // 0xC402B4
RwRaster*             CPostEffects::m_pGrainRaster = nullptr; // 0xC402B0
std::array<char, 2>   CPostEffects::m_grainStrength{};        // 0x8D5094

bool  CPostEffects::m_bCCTV = false;            // 0xC402C5
CRGBA CPostEffects::m_CCTVcol{ 64, 0, 0, 0 };   // 0x8D50FC

bool CPostEffects::m_bRainEnable = false;  // 0xC402D1
bool CPostEffects::m_bColorEnable = true;   // 0x8D518C

bool     CPostEffects::m_bRadiosity = false;                  // 0xC402CC
bool     CPostEffects::m_bRadiosityDebug = false;             // 0xC402CD
bool     CPostEffects::m_bRadiosityLinearFilter = true;      // 0x8D510A
bool     CPostEffects::m_bRadiosityStripCopyMode = true;     // 0x8D510B
int32_t  CPostEffects::m_RadiosityFilterUCorrection = 2;      // 0x8D511C
int32_t  CPostEffects::m_RadiosityFilterVCorrection = 2;      // 0x8D5120
int32_t  CPostEffects::m_RadiosityIntensity = 35;             // 0x8D5118
int32_t  CPostEffects::m_RadiosityIntensityLimit = 220;      // 0x8D5114
bool     CPostEffects::m_bRadiosityBypassTimeCycleIntensityLimit = false; // 0xC402CE
float    CPostEffects::m_RadiosityPixelsX = 0.0f;             // 0xC40314
float    CPostEffects::m_RadiosityPixelsY = 0.0f;             // 0xC40318
uint32_t CPostEffects::m_RadiosityFilterPasses = 1;          // 0x8D5110
uint32_t CPostEffects::m_RadiosityRenderPasses = 2;          // 0x8D510C

float CPostEffects::m_VisionFXDayNightBalance = 1.0f; // 0x8D50A4

bool        CPostEffects::m_bInfraredVision = false;             // 0xC402B9
int32_t     CPostEffects::m_InfraredVisionGrainStrength = 64;    // 0x8D50B4
float       CPostEffects::m_fInfraredVisionFilterRadius = 0.003f; // 0x8D50B8
CRGBA       CPostEffects::m_InfraredVisionCol{ 0xFF, 0x3C, 0x28, 0x6E }; // 0x8D50CC
CRGBA       CPostEffects::m_InfraredVisionMainCol{ 0xFF, 0xC8, 0x00, 0x64 }; // 0x8D50D0
RwRGBAReal  CPostEffects::m_fInfraredVisionHeatObjectCol{ 1.0f, 0.0f, 0.0f, 1.0f }; // 0x8D50BC
int32_t     CPostEffects::m_HeatHazeFXHourOfDayStart = 10;      // 0x8D50D4
int32_t     CPostEffects::m_HeatHazeFXHourOfDayEnd = 19;        // 0x8D50D8
float       CPostEffects::m_fHeatHazeFXFadeSpeed = 0.05f;       // 0x8D50DC
float       CPostEffects::m_fHeatHazeFXInsideBuildingFadeSpeed = 0.5f; // 0x8D50E0

bool  CPostEffects::m_waterEnable = false;       // 0xC402D3
float CPostEffects::m_waterStrength = 64.0f;      // 0x8D512C
float CPostEffects::m_waterSpeed = 0.0015f;       // 0x8D5138
float CPostEffects::m_waterFreq = 0.04f;          // 0x8D513C
CRGBA CPostEffects::m_waterCol{ 64, 64, 64, 64 }; // 0x8D5140

CPostEffects::imf CPostEffects::ms_imf{}; // 0xC40150

// ============================================================================
// Method bodies
// ============================================================================

// 0x704630
void CPostEffects::Initialise() {
    SetupBackBufferVertex();

    m_pGrainRaster = RwRasterCreate(GRAIN_TEXTURE_DIM, GRAIN_TEXTURE_DIM, 32,
                                    rwRASTERFORMAT8888 | rwRASTERTYPETEXTURE);
    if (m_pGrainRaster) {
        uint8_t* pixels = RwRasterLock(m_pGrainRaster, 0, rwRASTERLOCKWRITE);
        for (uint32_t i = 0; i < sq<uint32_t>(GRAIN_TEXTURE_DIM); i += 4) {
            const uint8_t v = static_cast<uint8_t>(CGeneral::GetRandomNumber());
            pixels[i + 0] = v;
            pixels[i + 1] = v;
            pixels[i + 2] = v;
            pixels[i + 3] = v;
        }
        RwRasterUnlock(m_pGrainRaster);
    }
}

// 0x7010C0
void CPostEffects::Close() {
    RwRasterDestroy(m_pGrainRaster);
    m_pGrainRaster = nullptr;
    if (pRasterFrontBuffer) {
        RwRasterDestroy(std::exchange(pRasterFrontBuffer, nullptr));
    }
}

// 0x7046D0
void CPostEffects::DoScreenModeDependentInitializations() {
    ImmediateModeFilterStuffInitialize();
    HeatHazeFXInit();
}

// 0x7043D0
void CPostEffects::SetupBackBufferVertex() {
    RwRaster* raster = RwCameraGetRaster(Scene.m_pRwCamera);

    // Clamp width/height up to the next power of two.
    const auto width   = BitCeil(static_cast<uint32_t>(RwRasterGetWidth(raster)));
    const auto height  = BitCeil(static_cast<uint32_t>(RwRasterGetHeight(raster)));
    const auto fwidth  = static_cast<float>(width);
    const auto fheight = static_cast<float>(height);

    if (pRasterFrontBuffer
        && (static_cast<uint32_t>(width) != static_cast<uint32_t>(RwRasterGetWidth(pRasterFrontBuffer))
            || static_cast<uint32_t>(height) != static_cast<uint32_t>(RwRasterGetHeight(pRasterFrontBuffer)))) {
        RwRasterDestroy(std::exchange(pRasterFrontBuffer, nullptr));
    }
    if (!pRasterFrontBuffer) {
        RwRect rect{};
        rect.w = static_cast<int32_t>(width);
        rect.h = static_cast<int32_t>(height);
        pRasterFrontBuffer = RasterCreatePostEffects(rect);
        // TODO(port): original logs "Error subrastering" here.
    }

    const float nearZ = RwIm2DGetNearScreenZ();
    const float recipNear = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    s_ccVertices[0].x = 0.0f;   s_ccVertices[0].y = 0.0f;
    s_ccVertices[0].z = nearZ;  s_ccVertices[0].rhw = recipNear;
    s_ccVertices[0].u = 0.5f / fwidth; s_ccVertices[0].v = 0.5f / fheight;

    s_ccVertices[1].x = 0.0f;   s_ccVertices[1].y = fheight;
    s_ccVertices[1].z = nearZ;  s_ccVertices[1].rhw = recipNear;
    s_ccVertices[1].u = 0.5f / fwidth; s_ccVertices[1].v = (fheight + 0.5f) / fheight;

    s_ccVertices[2].x = fwidth; s_ccVertices[2].y = fheight;
    s_ccVertices[2].z = nearZ;  s_ccVertices[2].rhw = recipNear;
    s_ccVertices[2].u = (fwidth + 0.5f) / fwidth; s_ccVertices[2].v = (fheight + 0.5f) / fheight;

    s_ccVertices[3].x = fwidth; s_ccVertices[3].y = 0.0f;
    s_ccVertices[3].z = nearZ;  s_ccVertices[3].rhw = recipNear;
    s_ccVertices[3].u = (fwidth + 0.5f) / fwidth; s_ccVertices[3].v = 0.5f / fheight;

    if (pRasterFrontBuffer) {
        DoScreenModeDependentInitializations();
    }
}

// 0x7046A0
void CPostEffects::Update() {
    m_bRainEnable = CWeather::Rain > 0.0f;
    if (!pRasterFrontBuffer) {
        SetupBackBufferVertex();
    }
}

// 0x700EC0
void CPostEffects::DrawQuad(float x1, float y1, float x2, float y2, uint8_t red, uint8_t green,
                            uint8_t blue, uint8_t alpha, RwRaster* raster) {
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(raster));

    RwRGBA color{};
    color.red = red; color.green = green; color.blue = blue; color.alpha = alpha;

    ms_imf.quad[0].x = x1;
    ms_imf.quad[0].y = y1;
    ms_imf.quad[0].z = ms_imf.screenZ;
    ms_imf.quad[0].emissiveColor = color;

    ms_imf.quad[1].x = x1 + x2;
    ms_imf.quad[1].y = y1;
    ms_imf.quad[1].z = ms_imf.screenZ;
    ms_imf.quad[1].emissiveColor = color;

    ms_imf.quad[2].x = x1;
    ms_imf.quad[2].y = y1 + y2;
    ms_imf.quad[2].z = ms_imf.screenZ;
    ms_imf.quad[2].emissiveColor = color;

    ms_imf.quad[3].x = x1 + x2;
    ms_imf.quad[3].y = y1 + y2;
    ms_imf.quad[3].z = ms_imf.screenZ;
    ms_imf.quad[3].emissiveColor = color;

    RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, ms_imf.quad.data(), 4);
}

// 0x701060
void CPostEffects::DrawQuadSetDefaultUVs() {
    DrawQuadSetUVs(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
}

// 0x700F90
void CPostEffects::DrawQuadSetUVs(float u1, float v1, float u2, float v2,
                                  float u3, float v3, float u4, float v4) {
    ms_imf.quad[0].u = u1; ms_imf.quad[0].v = v1;
    ms_imf.quad[1].u = u2; ms_imf.quad[1].v = v2;
    ms_imf.quad[2].u = u3; ms_imf.quad[2].v = v3;
    ms_imf.quad[3].u = u4; ms_imf.quad[3].v = v4;
}

// 0x700FE0
void CPostEffects::DrawQuadSetPixelUVs(float u0, float v0, float u1, float v1,
                                       float u3, float v3, float u2, float v2) {
    const float x = 1.0f / ms_imf.sizeDrawBufferX;
    const float y = 1.0f / ms_imf.sizeDrawBufferY;

    ms_imf.quad[0].u = x * u0; ms_imf.quad[0].v = y * v0;
    ms_imf.quad[1].u = x * u1; ms_imf.quad[1].v = y * v1;
    ms_imf.quad[2].u = x * u2; ms_imf.quad[2].v = y * v2;
    ms_imf.quad[3].u = x * u3; ms_imf.quad[3].v = y * v3;
}

// 0x7034B0
void CPostEffects::FilterFX_StoreAndSetDayNightBalance() {
    if (!m_bInCutscene) {
        // TODO(port): static float s_DayNightBalanceParamOld (0xC402E0)
        CCustomBuildingDNPipeline::m_fDNBalanceParam = m_VisionFXDayNightBalance;
    }
}

// 0x7034D0
void CPostEffects::FilterFX_RestoreDayNightBalance() {
    if (!m_bInCutscene) {
        // TODO(port): restore s_DayNightBalanceParamOld (0xC402E0)
        CCustomBuildingDNPipeline::m_fDNBalanceParam = 1.0f;
    }
}

// 0x703CC0
void CPostEffects::ImmediateModeFilterStuffInitialize() {
    ms_imf.screenZ          = RwIm2DGetNearScreenZ();
    ms_imf.RasterDrawBuffer = pRasterFrontBuffer;
    ms_imf.uMinTri          = 0.0f;
    ms_imf.vMinTri          = 0.0f;
    ms_imf.uMaxTri          = 2.0f;
    ms_imf.vMaxTri          = 2.0f;
    ms_imf.recipCameraZ     = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    ms_imf.sizeDrawBufferX  = RwRasterGetWidth(ms_imf.RasterDrawBuffer);
    ms_imf.sizeDrawBufferY  = RwRasterGetHeight(ms_imf.RasterDrawBuffer);

    ms_imf.triangle[0].x = 0.0f; ms_imf.triangle[0].y = 0.0f;
    ms_imf.triangle[0].z = ms_imf.screenZ; ms_imf.triangle[0].rhw = ms_imf.recipCameraZ;
    ms_imf.triangle[0].u = 0.0f; ms_imf.triangle[0].v = 0.0f;

    ms_imf.triangle[1].x = 2.0f * static_cast<float>(ms_imf.sizeDrawBufferX);
    ms_imf.triangle[1].y = 0.0f;
    ms_imf.triangle[1].z = ms_imf.screenZ; ms_imf.triangle[1].rhw = ms_imf.recipCameraZ;
    ms_imf.triangle[1].u = 2.0f; ms_imf.triangle[1].v = 0.0f;

    ms_imf.triangle[2].x = 0.0f;
    ms_imf.triangle[2].y = 2.0f * static_cast<float>(ms_imf.sizeDrawBufferY);
    ms_imf.triangle[2].z = ms_imf.screenZ; ms_imf.triangle[2].rhw = ms_imf.recipCameraZ;
    ms_imf.triangle[2].u = 0.0f; ms_imf.triangle[2].v = 2.0f;

    const RwRGBA defaultColor = PackARGB(0xFF00C800);
    for (int32_t i = 0; i < 4; i++) {
        ms_imf.quad[i].x = 0.0f;
        ms_imf.quad[i].y = 0.0f;
        ms_imf.quad[i].z = ms_imf.screenZ;
        ms_imf.quad[i].rhw = ms_imf.recipCameraZ;
        ms_imf.quad[i].emissiveColor = defaultColor;
    }
    ms_imf.quad[0].u = 0.0f; ms_imf.quad[0].v = 0.0f;
    ms_imf.quad[1].u = 1.0f; ms_imf.quad[1].v = 0.0f;
    ms_imf.quad[2].u = 0.0f; ms_imf.quad[2].v = 1.0f;
    ms_imf.quad[3].u = 1.0f; ms_imf.quad[3].v = 1.0f;

    const RwRaster* frameBuffer = RwCameraGetRaster(Scene.m_pRwCamera);
    ms_imf.fFrontBufferU1 = ms_imf.fFrontBufferV1 = 0.0f;
    ms_imf.fFrontBufferU2 = SCREEN_WIDTH / BitCeil(static_cast<uint32_t>(RwRasterGetWidth(const_cast<RwRaster*>(frameBuffer))));
    ms_imf.fFrontBufferV2 = SCREEN_HEIGHT / BitCeil(static_cast<uint32_t>(RwRasterGetHeight(const_cast<RwRaster*>(frameBuffer))));
}

// 0x700D70
void CPostEffects::ImmediateModeRenderStatesSet() {
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATECULLMODE,          RWRSTATE(rwCULLMODECULLNONE));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,         RWRSTATE(rwSHADEMODEGOURAUD));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,    RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
}

// 0x700CC0
void CPostEffects::ImmediateModeRenderStatesStore() {
    RwRenderStateGet(rwRENDERSTATESRCBLEND,          &ms_imf.blendSrc);
    RwRenderStateGet(rwRENDERSTATEDESTBLEND,         &ms_imf.blendDst);
    RwRenderStateGet(rwRENDERSTATEFOGENABLE,         &ms_imf.bFog);
    RwRenderStateGet(rwRENDERSTATECULLMODE,          &ms_imf.cullMode);
    RwRenderStateGet(rwRENDERSTATEZTESTENABLE,       &ms_imf.bZTest);
    RwRenderStateGet(rwRENDERSTATEZWRITEENABLE,      &ms_imf.bZWrite);
    RwRenderStateGet(rwRENDERSTATESHADEMODE,         &ms_imf.shadeMode);
    RwRenderStateGet(rwRENDERSTATEVERTEXALPHAENABLE, &ms_imf.bVertexAlpha);
    RwRenderStateGet(rwRENDERSTATETEXTUREADDRESS,    &ms_imf.textureAddress);
    RwRenderStateGet(rwRENDERSTATETEXTUREFILTER,     &ms_imf.textureFilter);
}

// 0x700E00
void CPostEffects::ImmediateModeRenderStatesReStore() {
    RwRenderStateSet(rwRENDERSTATESRCBLEND,           RWRSTATE(ms_imf.blendSrc));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,          RWRSTATE(ms_imf.blendDst));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,          RWRSTATE(ms_imf.bFog));
    RwRenderStateSet(rwRENDERSTATECULLMODE,           RWRSTATE(ms_imf.cullMode));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,        RWRSTATE(ms_imf.bZTest));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,       RWRSTATE(ms_imf.bZWrite));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,          RWRSTATE(ms_imf.shadeMode));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE,  RWRSTATE(ms_imf.bVertexAlpha));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,     RWRSTATE(ms_imf.textureAddress));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,      RWRSTATE(ms_imf.textureFilter));
}

// 0x700C90
// Decomp: RwRasterCreate(rect.w, rect.h, camera-raster depth, 5).
RwRaster* CPostEffects::RasterCreatePostEffects(RwRect rect) {
    return RwRasterCreate(rect.w, rect.h,
                          RwRasterGetDepth(RwCameraGetRaster(Scene.m_pRwCamera)),
                          rwRASTERTYPEZBUFFER | rwRASTERTYPETEXTURE);
}

// 0x7011B0
void CPostEffects::ScriptCCTVSwitch(bool enable) {
    m_bCCTV = enable;
}

// 0x701170
void CPostEffects::ScriptDarknessFilterSwitch(bool enable, int32_t alpha) {
    m_bDarknessFilter = enable;
    m_DarknessFilterAlpha = (alpha == -1)
        ? m_DarknessFilterAlphaDefault
        : std::clamp(alpha, 0, 255);
}

// 0x701160
void CPostEffects::ScriptHeatHazeFXSwitch(bool enable) {
    m_bHeatHazeFX = enable;
}

// 0x701140
void CPostEffects::ScriptInfraredVisionSwitch(bool enable) {
    if (enable) {
        m_bInfraredVision = true;
        m_bNightVision = false;
    } else {
        m_bInfraredVision = false;
    }
}

// 0x701120
void CPostEffects::ScriptNightVisionSwitch(bool enable) {
    if (enable) {
        m_bNightVision = true;
        m_bInfraredVision = false;
    } else {
        m_bNightVision = false;
    }
}

// 0x7010F0
void CPostEffects::ScriptResetForEffects() {
    m_bNightVision = false;
    m_bInfraredVision = false;
    m_bHeatHazeFX = false;
    m_bDarknessFilter = false;
    m_bCCTV = false;
    CWaterLevel::m_bWaterFogScript = true;
}

// 0x7039C0
void CPostEffects::UnderWaterRipple(CRGBA color, float xoffset, float yoffset,
                                    float strength, float speed, float freq) {
    color.a = 255;
    (void)strength; // unused in the original

    RwCameraEndUpdate(Scene.m_pRwCamera);
    RsCameraBeginUpdate(Scene.m_pRwCamera);
    s_uiTempBufferVerticesStored = s_uiTempBufferIndicesStored = 0;
    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(pRasterFrontBuffer));

    const int32_t rasterWidth   = RwRasterGetWidth(pRasterFrontBuffer);
    const int32_t rasterHeight  = RwRasterGetHeight(pRasterFrontBuffer);
    const float   fRasterWidth  = static_cast<float>(rasterWidth);
    const float   fRasterHeight = static_cast<float>(rasterHeight);
    const float   recipNearClip = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    const float   nearZ         = RwIm2DGetNearScreenZ();
    const RwRGBA  vcolor        = PackARGB(
        (static_cast<uint32_t>(color.a) << 24) |
        (static_cast<uint32_t>(color.r) << 16) |
        (static_cast<uint32_t>(color.g) << 8) |
        static_cast<uint32_t>(color.b));

    RwIm2DVertex* verts = &s_TempVertices[0x200];

    auto emitVertex = [&](float wave, int32_t y) {
        const uint32_t i = s_uiTempBufferVerticesStored;

        verts[i].x = 0.0f;
        verts[i].y = static_cast<float>(y);
        verts[i].z = nearZ;
        verts[i].rhw = recipNearClip;
        verts[i].u = (wave + xoffset) / fRasterWidth;
        verts[i].v = static_cast<float>(y) / fRasterHeight;
        verts[i].emissiveColor = vcolor;

        verts[i + 1].x = static_cast<float>(static_cast<int32_t>(2.0f * xoffset) + rasterWidth);
        verts[i + 1].y = static_cast<float>(y);
        verts[i + 1].z = nearZ;
        verts[i + 1].rhw = recipNearClip;
        verts[i + 1].u = (fRasterWidth + wave - xoffset) / fRasterWidth;
        verts[i + 1].v = static_cast<float>(y) / fRasterHeight;
        verts[i + 1].emissiveColor = vcolor;

        s_uiTempBufferVerticesStored += 2;
    };

    const float timeMS = static_cast<float>(CTimer::GetTimeInMS());
    if (rasterHeight > 0) {
        int32_t y = 0;
        for (; y < rasterHeight; y = static_cast<int32_t>(static_cast<float>(y) + yoffset)) {
            emitVertex(std::sin(freq * static_cast<float>(y) + speed * timeMS) * xoffset, y);
        }
        emitVertex(std::sin(freq * static_cast<float>(y) + speed * timeMS) * xoffset, y);
    } else {
        emitVertex(std::sin(speed * timeMS) * xoffset, 0);
    }

    if (s_uiTempBufferVerticesStored > 2) {
        RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, verts, s_uiTempBufferVerticesStored);
    }
    ImmediateModeRenderStatesReStore();
}

// 0x703CB0 (unused in the original)
void CPostEffects::UnderWaterRippleFadeToFX() {
    // NOP
}

// 0x701450
void CPostEffects::HeatHazeFXInit() {
    // m_HeatHazeFXType is always HEAT_HAZE_0 in practice.
    if (m_HeatHazeFXType == m_HeatHazeFXTypeLast)
        return;

    switch (m_HeatHazeFXType) {
    case HEAT_HAZE_0:
        m_HeatHazeFXIntensity   = 80;
        m_HeatHazeFXRandomShift = 0;
        m_HeatHazeFXSpeedMin    = 12;
        m_HeatHazeFXSpeedMax    = 18;
        m_HeatHazeFXScanSizeX   = static_cast<int32_t>(SCREEN_STRETCH_X(47.0f));
        m_HeatHazeFXScanSizeY   = static_cast<int32_t>(SCREEN_STRETCH_Y(47.0f));
        m_HeatHazeFXRenderSizeX = static_cast<int32_t>(SCREEN_STRETCH_X(50.0f));
        m_HeatHazeFXRenderSizeY = static_cast<int32_t>(SCREEN_STRETCH_Y(50.0f));
        break;
    case HEAT_HAZE_1:
        m_HeatHazeFXIntensity   = 32;
        m_HeatHazeFXRandomShift = 0;
        m_HeatHazeFXSpeedMin    = 6;
        m_HeatHazeFXSpeedMax    = 10;
        m_HeatHazeFXScanSizeX   = static_cast<int32_t>(SCREEN_STRETCH_X(100.0f));
        m_HeatHazeFXScanSizeY   = static_cast<int32_t>(SCREEN_STRETCH_Y(52.0f));
        m_HeatHazeFXRenderSizeX = static_cast<int32_t>(SCREEN_STRETCH_X(100.0f));
        m_HeatHazeFXRenderSizeY = static_cast<int32_t>(SCREEN_STRETCH_Y(60.0f));
        break;
    case HEAT_HAZE_2:
        m_HeatHazeFXIntensity   = 32;
        m_HeatHazeFXRandomShift = 0;
        m_HeatHazeFXSpeedMin    = 4;
        m_HeatHazeFXSpeedMax    = 8;
        m_HeatHazeFXScanSizeX   = static_cast<int32_t>(SCREEN_STRETCH_X(70.0f));
        m_HeatHazeFXScanSizeY   = static_cast<int32_t>(SCREEN_STRETCH_Y(70.0f));
        m_HeatHazeFXRenderSizeX = static_cast<int32_t>(SCREEN_STRETCH_X(80.0f));
        m_HeatHazeFXRenderSizeY = static_cast<int32_t>(SCREEN_STRETCH_Y(80.0f));
        break;
    case HEAT_HAZE_3:
        m_HeatHazeFXRandomShift = 0;
        m_HeatHazeFXIntensity   = 150;
        m_HeatHazeFXSpeedMin    = 5;
        m_HeatHazeFXSpeedMax    = 8;
        m_HeatHazeFXScanSizeX   = static_cast<int32_t>(SCREEN_STRETCH_X(60.0f));
        m_HeatHazeFXScanSizeY   = static_cast<int32_t>(SCREEN_STRETCH_Y(24.0f));
        m_HeatHazeFXRenderSizeX = static_cast<int32_t>(SCREEN_STRETCH_X(62.0f));
        m_HeatHazeFXRenderSizeY = static_cast<int32_t>(SCREEN_STRETCH_Y(24.0f));
        break;
    case HEAT_HAZE_4:
        m_HeatHazeFXRandomShift = 1;
        m_HeatHazeFXIntensity   = 150;
        m_HeatHazeFXSpeedMin    = 5;
        m_HeatHazeFXSpeedMax    = 8;
        m_HeatHazeFXScanSizeX   = static_cast<int32_t>(SCREEN_STRETCH_X(60.0f));
        m_HeatHazeFXScanSizeY   = static_cast<int32_t>(SCREEN_STRETCH_Y(24.0f));
        m_HeatHazeFXRenderSizeX = static_cast<int32_t>(SCREEN_STRETCH_X(62.0f));
        m_HeatHazeFXRenderSizeY = static_cast<int32_t>(SCREEN_STRETCH_Y(24.0f));
        break;
    default:
        break;
    }
    // NOTE: the decompiled code fills the scan/render sizes with
    // CGeneral::unk_00821b40() calls here; gta-reversed (and this port) use the
    // SCREEN_STRETCH constants above instead.

    m_HeatHazeFXTypeLast = m_HeatHazeFXType;

    for (int32_t i = 0; i < 180; i++) {
        s_hpX[i] = CGeneral::GetRandomNumberInRange(m_HeatHazeFXScanSizeX, RwRasterGetWidth(pRasterFrontBuffer));
        s_hpY[i] = CGeneral::GetRandomNumberInRange(m_HeatHazeFXScanSizeY, RwRasterGetHeight(pRasterFrontBuffer));
        s_hpS[i] = CGeneral::GetRandomNumberInRange(m_HeatHazeFXSpeedMin, m_HeatHazeFXSpeedMax);
    }
}

// 0x701780
// NOTE: the incoming fIntensity is overwritten as scratch below and never read;
// the effect is driven by the m_HeatHazeFX* state set up by HeatHazeFXInit.
void CPostEffects::HeatHazeFX(float fIntensity, bool bAlphaMaskMode) {
    (void)fIntensity;

    // bVar24: 16-bit depth buffer?
    // TODO(port): RwCameraGetRaster(Scene.m_pRwCamera)->depth != 16
    const bool b16BitDepth = false;

    if (bAlphaMaskMode) {
        RwRGBA black{};
        RwCameraClear(Scene.m_pRwCamera, &black, 2);
        ImmediateModeRenderStatesStore();
        ImmediateModeRenderStatesSet();
        RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
        if (b16BitDepth) {
            RwRenderStateSet(rwRENDERSTATESTENCILPASS, RWRSTATE(1));
            RwRenderStateSet(rwRENDERSTATESTENCILFUNCTION, RWRSTATE(1));
            RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONREF, RWRSTATE(1));
            RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONMASK, RWRSTATE(3));
            RwRenderStateSet(rwRENDERSTATEALPHATESTENABLE, RWRSTATE(0));
            RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONWRITEMASK, RWRSTATE(8));
        }
        DrawQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
                 0, 0, 0, 255, nullptr);
        if (b16BitDepth) {
            RwRenderStateSet(rwRENDERSTATEALPHATESTENABLE, RWRSTATE(1));
        }
        // TODO(port): Fx_c::Render(RwCamera, 1)
        ImmediateModeRenderStatesReStore();
    } else {
        s_bHeatHazeUseFullscreenPath = false;
    }

    HeatHazeFXInit();

    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwCameraEndUpdate(Scene.m_pRwCamera);
    RwRasterPushContext(pRasterFrontBuffer);
    RwRasterRenderFast(RwCameraGetRaster(Scene.m_pRwCamera), 0, 0);
    RwRasterPopContext();
    RsCameraBeginUpdate(Scene.m_pRwCamera);
    s_uiTempBufferVerticesStored = 0;
    s_uiTempBufferIndicesStored = 0;
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));

    const int32_t rnd = Unk_00821b40();
    s_uiTempBufferVerticesStored = 0;
    if (b16BitDepth) {
        RwRenderStateSet(rwRENDERSTATESTENCILFUNCTIONMASK, RWRSTATE(1));
        RwRenderStateSet(rwRENDERSTATEALPHATESTENABLE, RWRSTATE(1));
        RwRenderStateSet(rwRENDERSTATESTENCILFUNCTION, RWRSTATE(3));
    }
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(pRasterFrontBuffer));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));

    const int32_t scanW   = m_HeatHazeFXScanSizeX;
    const int32_t scanH   = m_HeatHazeFXScanSizeY;
    const int32_t renderW = m_HeatHazeFXRenderSizeX;
    const int32_t renderH = m_HeatHazeFXRenderSizeY;
    const int32_t rasterW = RwRasterGetWidth(pRasterFrontBuffer);
    const int32_t rasterH = RwRasterGetHeight(pRasterFrontBuffer);
    const float   zNear   = RwDeviceGetZBufferNear();
    const float   rhw     = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);

    RwIm2DVertex* verts = &s_TempVertices[0x200];

    if (!s_bHeatHazeUseFullscreenPath) {
        // Particle path: 180 quads sampling the front buffer with a slight
        // UV inset (scan < render), drifting upward.
        const RwRGBA vcolor = PackARGB((static_cast<uint32_t>(rnd) << 24) | 0x00FFFFFFu);
        const int32_t xOff = (renderW - scanW) / 2;
        const int32_t yOff = (renderH - scanH) / 2;

        for (int32_t i = 0; i < 180; i++) {
            int32_t x  = s_hpX[i];
            int32_t y  = s_hpY[i];
            int32_t sx = x - xOff;
            int32_t sy = y - yOff;
            if (m_HeatHazeFXRandomShift > 0) {
                CGeneral::GetRandomNumber();
                sx += Unk_00821b40() - m_HeatHazeFXRandomShift;
                CGeneral::GetRandomNumber();
                sy += Unk_00821b40() - m_HeatHazeFXRandomShift;
            }
            if (sx < 0) {
                x += xOff;
                sx = 0;
            }
            if (sx > rasterW - renderW) {
                x -= xOff;
                sx = rasterW - renderW;
            }
            if (sy < 0) {
                y += yOff;
                sy = 0;
            }
            if (sy > rasterH - renderH) {
                y -= yOff;
                sy = rasterH - renderH;
            }

            const uint32_t v = s_uiTempBufferVerticesStored;
            const float fx = (float)x / (float)rasterW;
            const float fy = (float)y / (float)rasterH;
            const float fx2 = (float)(x + scanW) / (float)rasterW;
            const float fy2 = (float)(y + scanH) / (float)rasterH;

            verts[v].x = (float)sx;             verts[v].y = (float)sy;
            verts[v].z = zNear;                 verts[v].rhw = rhw;
            verts[v].emissiveColor = vcolor;    verts[v].u = fx;  verts[v].v = fy;

            verts[v+1].x = (float)(sx + renderW); verts[v+1].y = (float)sy;
            verts[v+1].z = zNear;                 verts[v+1].rhw = rhw;
            verts[v+1].emissiveColor = vcolor;    verts[v+1].u = fx2; verts[v+1].v = fy;

            verts[v+2].x = (float)sx;             verts[v+2].y = (float)(sy + renderH);
            verts[v+2].z = zNear;                 verts[v+2].rhw = rhw;
            verts[v+2].emissiveColor = vcolor;    verts[v+2].u = fx;  verts[v+2].v = fy2;

            verts[v+3].x = (float)(sx + renderW); verts[v+3].y = (float)(sy + renderH);
            verts[v+3].z = zNear;                 verts[v+3].rhw = rhw;
            verts[v+3].emissiveColor = vcolor;    verts[v+3].u = fx2; verts[v+3].v = fy2;

            uint32_t ii = s_uiTempBufferIndicesStored;
            s_TempIndices[ii++] = (uint16_t)v;
            s_TempIndices[ii++] = (uint16_t)(v + 2);
            s_TempIndices[ii++] = (uint16_t)(v + 1);
            s_TempIndices[ii++] = (uint16_t)(v + 1);
            s_TempIndices[ii++] = (uint16_t)(v + 2);
            s_TempIndices[ii++] = (uint16_t)(v + 3);
            s_uiTempBufferIndicesStored = ii;
            s_uiTempBufferVerticesStored = v + 4;

            // Drift upward; respawn at the bottom when off-screen.
            CGeneral::GetRandomNumber();
            s_hpY[i] -= Unk_00821b40();
            if (s_hpY[i] < 0) {
                CGeneral::GetRandomNumber();
                s_hpX[i] = Unk_00821b40();
                s_hpY[i] = rasterH - scanH;
                CGeneral::GetRandomNumber();
                s_hpS[i] = Unk_00821b40() + m_HeatHazeFXSpeedMin;
            }
        }
    } else {
        // Fullscreen path (alpha-mask mode). The decompiled field mapping for
        // this branch is ambiguous; this follows the standard fullscreen-quad
        // pattern. NOTE: unreachable in this TU (flag never set to 1 here).
        const RwRGBA vcolor = PackARGB((static_cast<uint32_t>(rnd) << 24) | 0x00FF0000u);
        const uint32_t v = s_uiTempBufferVerticesStored;

        verts[v].x = 0.0f;            verts[v].y = 0.0f;
        verts[v].z = zNear;           verts[v].rhw = rhw;
        verts[v].emissiveColor = vcolor;
        verts[v].u = 0.0f;            verts[v].v = 0.0f;

        verts[v+1].x = (float)rasterW; verts[v+1].y = 0.0f;
        verts[v+1].z = zNear;         verts[v+1].rhw = rhw;
        verts[v+1].emissiveColor = vcolor;
        verts[v+1].u = 1.0f;          verts[v+1].v = 0.0f;

        verts[v+2].x = 0.0f;          verts[v+2].y = (float)rasterH;
        verts[v+2].z = zNear;         verts[v+2].rhw = rhw;
        verts[v+2].emissiveColor = vcolor;
        verts[v+2].u = 0.0f;          verts[v+2].v = 1.0f;

        verts[v+3].x = (float)rasterW; verts[v+3].y = (float)rasterH;
        verts[v+3].z = zNear;         verts[v+3].rhw = rhw;
        verts[v+3].emissiveColor = vcolor;
        verts[v+3].u = 1.0f;          verts[v+3].v = 1.0f;

        uint32_t ii = s_uiTempBufferIndicesStored;
        s_TempIndices[ii++] = (uint16_t)v;
        s_TempIndices[ii++] = (uint16_t)(v + 2);
        s_TempIndices[ii++] = (uint16_t)(v + 1);
        s_TempIndices[ii++] = (uint16_t)(v + 1);
        s_TempIndices[ii++] = (uint16_t)(v + 2);
        s_TempIndices[ii++] = (uint16_t)(v + 3);
        s_uiTempBufferIndicesStored = ii;
        s_uiTempBufferVerticesStored = v + 4;
    }

    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(pRasterFrontBuffer));
    if (s_uiTempBufferVerticesStored != 0) {
        CVisibilityPlugins::RenderTempBufferIndexed(rwPRIMTYPETRILIST, 0xC5F958,
            s_uiTempBufferVerticesStored, s_TempIndices, s_uiTempBufferIndicesStored);
    }
    s_uiTempBufferVerticesStored = 0;
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(nullptr));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
    if (b16BitDepth) {
        RwRenderStateSet(rwRENDERSTATESTENCILPASS, RWRSTATE(0));
    }
}

// 0x7030A0
void CPostEffects::SpeedFX(float speed) {
    const int32_t camMode = GetActiveCameraShim().m_nMode;
    const bool isMode1 = (camMode == 1);

    // Find the highest speed tier not exceeding `speed`.
    int32_t passes = 0;
    int32_t uvExpand = 0;
    int32_t randomJitter = 0;
    for (int32_t i = 6; i >= 0; i--) {
        if (s_SpeedFXTable[i].speedThreshold <= speed) {
            passes       = s_SpeedFXTable[i].passes;
            uvExpand     = s_SpeedFXTable[i].uvExpand;
            randomJitter = s_SpeedFXTable[i].randomJitter;
            break;
        }
    }
    if (isMode1 || camMode == 2) {
        uvExpand /= 2;
        randomJitter = 0;
    }
    if (passes <= 0)
        return;

    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();

    float jitterU = 0.0f, jitterV = 0.0f;
    if (randomJitter > 0) {
        jitterU = ms_imf.fFrontBufferU2 * (float)randomJitter * 0.004f;
        jitterV = ms_imf.fFrontBufferV2 * (float)randomJitter * 0.004f;
        jitterU *= (float)CGeneral::GetRandomNumber() * 3.051851e-05f;
        jitterV *= (float)CGeneral::GetRandomNumber() * 3.051851e-05f;
    }
    const float expandU = ms_imf.fFrontBufferU2 * (float)uvExpand * 0.0025f;
    const float expandV = (float)uvExpand * ms_imf.fFrontBufferV2 * 0.0025f;

    // Per-corner UV offsets; they expand outward each pass.
    float u0 = expandU,  v0 = expandV;
    float u1 = -expandU, v1 = expandV;
    float u2 = expandU,  v2 = -expandV;
    float u3 = -expandU, v3 = -expandV;
    float ju0 = jitterU, jv0 = jitterV;
    float ju1 = jitterU, jv1 = jitterV;

    for (; passes > 0; passes--) {
        if (isMode1 || camMode == 2) {
            u0 = u1 = u2 = u3 = 0.0f;
            v0 = v1 = v2 = v3 = 0.0f;
            ju0 = ju1 = 0.0f;
            jv0 = jv1 = 0.0f;
        }
        ms_imf.quad[0].u = ms_imf.fFrontBufferU1 + u0 + ju0;
        ms_imf.quad[0].v = ms_imf.fFrontBufferV1 + v0 + jv0;
        ms_imf.quad[1].u = (ms_imf.fFrontBufferU2 + u1) - ju1;
        ms_imf.quad[1].v = ms_imf.fFrontBufferV1 + v1 + jv0;
        ms_imf.quad[2].u = ms_imf.fFrontBufferU1 + u2 + ju0;
        ms_imf.quad[2].v = (ms_imf.fFrontBufferV2 + v2) - ju1;
        ms_imf.quad[3].u = (ms_imf.fFrontBufferU2 + u3) - ju1;
        ms_imf.quad[3].v = (ms_imf.fFrontBufferV2 + v3) - jv1;
        DrawQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
                 255, 255, 255, (uint8_t)m_SpeedFXAlpha, pRasterFrontBuffer);

        u1 -= expandU; v3 -= expandV;
        v0 += expandV; v1 += expandV;
        v2 -= expandV;
        u0 += expandU; u3 -= expandU; u2 += expandU;
    }

    // Reset quad UVs to the default fullscreen mapping.
    ms_imf.quad[0].u = 0.0f; ms_imf.quad[0].v = 0.0f;
    ms_imf.quad[1].u = 1.0f; ms_imf.quad[1].v = 0.0f;
    ms_imf.quad[2].u = 0.0f; ms_imf.quad[2].v = 1.0f;
    ms_imf.quad[3].u = 1.0f; ms_imf.quad[3].v = 1.0f;

    ImmediateModeRenderStatesReStore();
}

// 0x7034F0
bool CPostEffects::IsVisionFXActive() {
    return !m_bInCutscene && (m_bNightVision || m_bInfraredVision);
}

// 0x7011C0
void CPostEffects::NightVision() {
    if (m_fNightVisionSwitchOnFXCount > 0.0f) {
        m_fNightVisionSwitchOnFXCount =
            std::min(m_fNightVisionSwitchOnFXCount - CTimer::GetTimeStep(), 0.0f);

        ImmediateModeRenderStatesStore();
        ImmediateModeRenderStatesSet();
        RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDONE));
        RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDONE));

        const int32_t end = (int32_t)m_fNightVisionSwitchOnFXCount;
        for (int32_t i = 0; i < end; i++) {
            DrawQuad(0.0f, 0.0f, (float)ms_imf.sizeDrawBufferX, (float)ms_imf.sizeDrawBufferY,
                     8, 8, 8, 255, ms_imf.RasterDrawBuffer);
        }

        ImmediateModeRenderStatesReStore();
    }

    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDZERO));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDSRCCOLOR));
    DrawQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
             32, 255, 32, 255, nullptr);
    ImmediateModeRenderStatesReStore();
}

// 0x7012E0
void CPostEffects::NightVisionSetLights() {
    if (m_bNightVision && !m_bInCutscene) {
        SetLightsForNightVision();
    }
}

// 0x703520
void CPostEffects::SetFilterMainColour(RwRaster* raster, RwRGBA color) {
    (void)raster;
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(pRasterFrontBuffer));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));

    for (int32_t i = 0; i < 4; i++) {
        RwIm2DVertexSetRealRGBA(&s_ccVertices[i], color.red, color.green, color.blue, color.alpha);
    }

    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDDESTALPHA));
    RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, s_ccVertices.data(), 4,
                                 const_cast<uint16_t*>(s_ccIndices.data()), 6);

    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(nullptr));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
}

// 0x703F80
void CPostEffects::InfraredVision(RwRGBA color, RwRGBA colorMain) {
    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();

    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDONE));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDONE));

    const float radius = m_fInfraredVisionFilterRadius * 100.0f;
    for (int32_t j = 1; j <= 4; j++) {
        DrawQuadSetUVs(
            ms_imf.fFrontBufferU1, ms_imf.fFrontBufferV1,
            ms_imf.fFrontBufferU2, ms_imf.fFrontBufferV1,
            ms_imf.fFrontBufferU1, ms_imf.fFrontBufferV2,
            ms_imf.fFrontBufferU2, ms_imf.fFrontBufferV2
        );

        const float xy = (float)j * radius;
        DrawQuad(
            -xy, -xy,
            (float)SCREEN_WIDTH + xy + xy,
            (float)SCREEN_HEIGHT + xy + xy,
            color.red, color.green, color.blue, 255,
            ms_imf.RasterDrawBuffer
        );
    }

    DrawQuadSetDefaultUVs();

    RwRenderStateSet(rwRENDERSTATESRCBLEND,  RWRSTATE(rwBLENDZERO));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDSRCCOLOR));

    DrawQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
             255, 64, 255, 255, nullptr);
    ImmediateModeRenderStatesReStore();
    SetFilterMainColour(ms_imf.RasterDrawBuffer, colorMain);
}

// 0x701430
void CPostEffects::InfraredVisionSetLightsForDefaultObjects() {
    if (m_bInfraredVision && !m_bInCutscene) {
        SetLightsForInfraredVisionDefaultObjects();
    }
}

// 0x701300 (unused in the original)
void CPostEffects::InfraredVisionSetLightsForHeatObjects() {
    if (m_bInfraredVision && !m_bInCutscene) {
        SetLightsForInfraredVisionHeatObjects();
    }
}

// 0x701320
void CPostEffects::InfraredVisionStoreAndSetLightsForHeatObjects(CPed* ped) {
    if (!m_bInfraredVision || m_bInCutscene)
        return;

    // Store color.
    const float red   = m_fInfraredVisionHeatObjectCol.red;
    const float green = m_fInfraredVisionHeatObjectCol.green;
    const float blue  = m_fInfraredVisionHeatObjectCol.blue;
    const float alpha = m_fInfraredVisionHeatObjectCol.alpha;

    // Gradually shift from red (living) to blue (long dead).
    if (PedGetState(ped) == PEDSTATE_DEAD) {
        const float delta = std::abs((int32_t)(CTimer::GetTimeInMS() - PedGetDeathTimeMS(ped))) / 10000.0f;
        m_fInfraredVisionHeatObjectCol.red   = std::max(m_fInfraredVisionHeatObjectCol.red - delta, 0.0f);
        m_fInfraredVisionHeatObjectCol.green = 0.0f;
        m_fInfraredVisionHeatObjectCol.blue  = std::min(m_fInfraredVisionHeatObjectCol.blue + delta, 1.0f);
    }

    StoreAndSetLightsForInfraredVisionHeatObjects();

    // Restore color.
    m_fInfraredVisionHeatObjectCol.red   = red;
    m_fInfraredVisionHeatObjectCol.green = green;
    m_fInfraredVisionHeatObjectCol.blue  = blue;
    m_fInfraredVisionHeatObjectCol.alpha = alpha;
}

// 0x701410
void CPostEffects::InfraredVisionRestoreLightsForHeatObjects() {
    if (m_bInfraredVision && !m_bInCutscene) {
        RestoreLightsForInfraredVisionHeatObjects();
    }
}

// 0x704150
void CPostEffects::Fog() {
    // TODO(port): static float s_FogRadius (0xC402E4), s_FogAngle (0xC402DC)
    static float s_FogRadius = 0.0f;
    static float s_FogAngle  = 0.0f;

    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));

    const CVector playerSpeed = FindPlayerSpeed();
    if (playerSpeed.SquaredMagnitude() <= sq(0.06f)) {
        s_FogRadius = std::max(s_FogRadius - CTimer::GetTimeStep() / 4.0f, 0.0f);
    } else {
        s_FogRadius = std::min(s_FogRadius + CTimer::GetTimeStep() / 4.0f, 160.0f);
    }

    const CRGBA skyBottom = CTimeCycle::GetCurrentSkyBottomColor();
    for (int32_t i = 0; i < 10; i++) {
        const float angle = DegreesToRadians(36.0f * (float)i + s_FogAngle);
        DrawQuad(
            std::cos(angle) * ((float)SCREEN_WIDTH / 4.0f + s_FogRadius) + (float)SCREEN_WIDTH / 2.0f - (float)SCREEN_WIDTH / 3.0f,
            std::sin(angle) * ((float)SCREEN_HEIGHT / 4.0f + s_FogRadius) + (float)SCREEN_HEIGHT / 2.0f - (float)SCREEN_HEIGHT / 3.0f,
            2.0f * (float)SCREEN_WIDTH / 3.0f,
            2.0f * (float)SCREEN_HEIGHT / 3.0f,
            skyBottom.r, skyBottom.g, skyBottom.b, 11,
            RwTextureGetRaster(CClouds::ms_vc.texture)
        );
    }
    for (int32_t i = 0; i < 10; i++) {
        const float angle = -DegreesToRadians(36.0f * (float)i + s_FogAngle);
        DrawQuad(
            std::cos(angle) * ((float)SCREEN_WIDTH * 0.35f + s_FogRadius) + (float)SCREEN_WIDTH / 2.0f - (float)SCREEN_WIDTH / 3.0f,
            std::sin(angle) * ((float)SCREEN_HEIGHT * 0.35f + s_FogRadius) + (float)SCREEN_HEIGHT / 2.0f - (float)SCREEN_HEIGHT / 3.0f,
            2.0f * (float)SCREEN_WIDTH / 3.0f,
            2.0f * (float)SCREEN_HEIGHT / 3.0f,
            skyBottom.r, skyBottom.g, skyBottom.b, 11,
            RwTextureGetRaster(CClouds::ms_vc.texture)
        );
    }

    s_FogAngle += CTimer::GetTimeStep() / 6.0f;
    ImmediateModeRenderStatesReStore();
}

// 0x702F40
void CPostEffects::CCTV() {
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwCameraEndUpdate(Scene.m_pRwCamera);
    RwRasterPushContext(CPostEffects::pRasterFrontBuffer);
    RwRasterRenderFast(RwCameraGetRaster(Scene.m_pRwCamera), 0, 0);
    RwRasterPopContext();
    RsCameraBeginUpdate(Scene.m_pRwCamera);

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(pRasterFrontBuffer));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();

    const uint32_t lineHeight  = static_cast<uint32_t>(2.0f * SCREEN_STRETCH_Y(1.0f));
    const uint32_t linePadding = 2 * lineHeight;
    const uint32_t numLines    = static_cast<uint32_t>(SCREEN_HEIGHT) / linePadding;
    for (uint32_t i = 0, y = 0; i < numLines; i++, y += linePadding) {
        DrawQuad(0.0f, (float)y, (float)SCREEN_WIDTH, (float)lineHeight,
                 m_CCTVcol.r, m_CCTVcol.g, m_CCTVcol.b, 255, pRasterFrontBuffer);
    }
    ImmediateModeRenderStatesReStore();
}

// 0x7037C0
void CPostEffects::Grain(int32_t strengthMask, bool update) {
    // TODO(port): static uint32 s_NumberOfReseeds (0xC4031C)
    static uint32_t s_NumberOfReseeds = 0;

    if (update) {
        uint8_t* pixels = RwRasterLock(m_pGrainRaster, 0, rwRASTERLOCKWRITE);

        std::srand(CTimer::GetCurrentTimeInCycles() / CTimer::GetCyclesPerMillisecond());
        uint32_t reSeedCounter = 0;
        for (uint32_t i = 0; i < sq<uint32_t>(GRAIN_TEXTURE_DIM); i++) {
            if (++reSeedCounter >= 100) {
                reSeedCounter = 0;
                std::srand(CTimer::GetTimeInMS() + static_cast<uint32_t>(CTimer::GetTimeInMS()) + ++s_NumberOfReseeds);
            }
            const uint8_t v = static_cast<uint8_t>(CGeneral::GetRandomNumber());
            pixels[4 * i]     = v;
            pixels[4 * i + 1] = v;
            pixels[4 * i + 2] = v;
            pixels[4 * i + 3] = v;
        }
        RwRasterUnlock(m_pGrainRaster);
    }

    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSWRAP));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));

    const float ux = SCREEN_STRETCH_X(1.5f);
    const float uy = (float)SCREEN_HEIGHT / (float)SCREEN_WIDTH * ux;

    ms_imf.quad[0].u = 0.0f; ms_imf.quad[0].v = 0.0f;
    ms_imf.quad[1].u = ux;   ms_imf.quad[1].v = 0.0f;
    ms_imf.quad[2].u = 0.0f; ms_imf.quad[2].v = uy;
    ms_imf.quad[3].u = ux;   ms_imf.quad[3].v = uy;
    DrawQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
             255, 255, 255, (uint8_t)strengthMask, m_pGrainRaster);
    ms_imf.quad[0].u = 0.0f; ms_imf.quad[0].v = 0.0f;
    ms_imf.quad[1].u = 1.0f; ms_imf.quad[1].v = 0.0f;
    ms_imf.quad[2].u = 0.0f; ms_imf.quad[2].v = 1.0f;
    ms_imf.quad[3].u = 1.0f; ms_imf.quad[3].v = 1.0f;
    ImmediateModeRenderStatesReStore();
}

// 0x702F00
void CPostEffects::DarknessFilter(int32_t alpha) {
    ImmediateModeRenderStatesStore();
    ImmediateModeRenderStatesSet();
    DrawQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
             0, 0, 0, (uint8_t)alpha, nullptr);
    ImmediateModeRenderStatesReStore();
}

// 0x703650
void CPostEffects::ColourFilter(RwRGBA pass1, RwRGBA pass2) {
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,       RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,      RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(pRasterFrontBuffer));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDONE));

    for (int32_t i = 0; i < 4; i++) {
        RwIm2DVertexSetRealRGBA(&s_ccVertices[i], pass1.red, pass1.green, pass1.blue, pass1.alpha);
    }
    RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, s_ccVertices.data(), 4,
                                 const_cast<uint16_t*>(s_ccIndices.data()), 6);

    for (int32_t i = 0; i < 4; i++) {
        RwIm2DVertexSetRealRGBA(&s_ccVertices[i], pass2.red, pass2.green, pass2.blue, pass2.alpha);
    }
    RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, s_ccVertices.data(), 4,
                                 const_cast<uint16_t*>(s_ccIndices.data()), 6);

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,   RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE,  RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(nullptr));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,      RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,     RWRSTATE(rwBLENDINVSRCALPHA));
}

// 0x700BE0
void CPostEffects::SetSpeedFXManualSpeedCurrentFrame(float value) {
    m_fSpeedFXManualSpeedCurrentFrame = std::clamp(value, 0.0f, 1.0f);
}

// 0x7046E0
void CPostEffects::Render() {
    // TODO(port): static int32 s_CurrentStrength (0xC40328),
    // static float s_WaterGreen (0xC40324), static bool s_WaitForOneFrame (0xC40321),
    // static bool s_SavePhotoToGallery (0xC40320)
    static int32_t s_CurrentStrength = 0;
    static float   s_WaterGreen = 0.0f;
    static bool    s_WaitForOneFrame = false;
    static bool    s_SavePhotoToGallery = false;

    if (m_bDisableAllPostEffect) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwCameraEndUpdate(Scene.m_pRwCamera);
    RwRasterPushContext(CPostEffects::pRasterFrontBuffer);
    RwRasterRenderFast(RwCameraGetRaster(Scene.m_pRwCamera), 0, 0);
    RwRasterPopContext();
    RsCameraBeginUpdate(Scene.m_pRwCamera);

    // TODO(port): ScopedStaticRef<float> s_ExtraMult (0xC4032C, init 0xC40330);
    // runs 1 frame at 1.0f then 0.35f.
    static float s_ExtraMult = 1.0f;
    static bool s_ExtraMultInit = false;
    if (!s_ExtraMultInit) {
        s_ExtraMultInit = true;
    } else {
        s_ExtraMult = 0.35f;
    }

    if (m_bFog) {
        Fog();
    }

    auto postFxColors = CTimeCycle::GetPostFxColors();
    CRGBA pass1 = postFxColors.first;
    CRGBA pass2 = postFxColors.second;
    if (m_bNightVision || m_bInfraredVision) {
        SetRGBA(pass1, 64, 64, 64);
        SetRGBA(pass2, 64, 64, 64);
    }
    ScaleRGB(pass1, 1.0f); // gfLaRiotsLightMult (TODO(port): 1.0f in the decomp)

    float colorMultFactor = 1.0f;
    if (CPlayerPed* player = FindPlayerPed()) {
        const float step = CTimer::GetTimeStep() * SCREEN_EXTRA_MULT_CHANGE_RATE;
        float lightFromCol = PedGetLightingFromCol(player, false);

        if (std::abs(lightFromCol - s_ExtraMult) >= step) {
            lightFromCol = (lightFromCol <= s_ExtraMult)
                ? s_ExtraMult - step
                : s_ExtraMult + step;
        }
        s_ExtraMult = std::min(lightFromCol, SCREEN_EXTRA_MULT_BASE_CAP);
        colorMultFactor += (1.0f - s_ExtraMult / SCREEN_EXTRA_MULT_BASE_CAP) * SCREEN_EXTRA_MULT_BASE_MULT;
    }

    ScaleRGB(pass1, colorMultFactor);
    ScaleRGB(pass2, colorMultFactor);

    if (m_bColorEnable) {
        RwRGBA p1{ pass1.r, pass1.g, pass1.b, pass1.a };
        RwRGBA p2{ pass2.r, pass2.g, pass2.b, pass2.a };
        ColourFilter(p1, p2);
    }

    if (m_bDarknessFilter && !m_bNightVision && !m_bInfraredVision) {
        DarknessFilter(m_DarknessFilterAlpha);
        Radiosity(m_DarknessFilterRadiosityIntensityLimit, 0, 2, 255);
    }

    if (m_bSpeedFXTestMode) {
        SetSpeedFXManualSpeedCurrentFrame(1.0f);
    }

    if (m_bSpeedFX && m_bSpeedFXUserFlag && m_bSpeedFXUserFlagCurrentFrame) {
        CVehicle* veh = FindPlayerVehicle();
        if (m_fSpeedFXManualSpeedCurrentFrame == 0.0f) {
            const int32_t vehType = veh ? VehicleGetType(veh) : -1;
            if (veh && vehType != VEHICLE_TYPE_PLANE && vehType != VEHICLE_TYPE_HELI
                    && vehType != VEHICLE_TYPE_BOAT && vehType != VEHICLE_TYPE_TRAIN) {
                bool fxDrawn = false;
                if (vehType == VEHICLE_TYPE_AUTOMOBILE && VehicleHasNos(veh)
                        && VehicleGetTireTemperature(veh) < 0.0f) {
                    const CVector moveSpeed = VehicleGetMoveSpeed(veh);
                    const CVector forward = VehicleGetForward(veh);
                    const float dir = moveSpeed.x * forward.x + moveSpeed.y * forward.y + moveSpeed.z * forward.z;
                    if (dir > 0.2f) {
                        SetSpeedFXManualSpeedCurrentFrame(2.0f * dir * (VehicleGetGasPedal(veh) + 1.0f));
                        SpeedFX(m_fSpeedFXManualSpeedCurrentFrame);
                        fxDrawn = true;
                    }
                }

                if (!fxDrawn && !CCutsceneMgr::ms_running) {
                    SpeedFX(FindPlayerSpeed().Magnitude());
                }
            }
        } else {
            SpeedFX(m_fSpeedFXManualSpeedCurrentFrame);
        }
    }
    SetSpeedFXManualSpeedCurrentFrame(0.0f);
    m_bSpeedFXUserFlagCurrentFrame = true;
    m_bInCutscene = CCutsceneMgr::ms_running || CCutsceneMgr::ms_cutsceneProcessing;

    if (m_bNightVision) {
        if (!m_bInCutscene) {
            NightVision();
            Grain(m_NightVisionGrainStrength, true);
        }
    } else {
        m_fNightVisionSwitchOnFXCount = m_fNightVisionSwitchOnFXTime;
    }

    if (m_bInfraredVision && !m_bInCutscene) {
        RwRGBA col{ m_InfraredVisionCol.r, m_InfraredVisionCol.g, m_InfraredVisionCol.b, m_InfraredVisionCol.a };
        RwRGBA mainCol{ m_InfraredVisionMainCol.r, m_InfraredVisionMainCol.g, m_InfraredVisionMainCol.b, m_InfraredVisionMainCol.a };
        InfraredVision(col, mainCol);
        Grain(m_InfraredVisionGrainStrength, true);
    }

    if (m_bRadiosity && !m_bDarknessFilter) {
        if (m_bRadiosityBypassTimeCycleIntensityLimit) {
            Radiosity(m_RadiosityIntensityLimit, (int32_t)m_RadiosityFilterPasses,
                      (int32_t)m_RadiosityRenderPasses, m_RadiosityIntensity);
        } else {
            Radiosity((int32_t)CTimeCycle::GetHighLightMinIntensity(), (int32_t)m_RadiosityFilterPasses,
                      (int32_t)m_RadiosityRenderPasses, m_RadiosityIntensity);
        }
    }

    if (m_bRainEnable || s_CurrentStrength != 0) {
        const float rain = 128.0f * CWeather::Rain;
        if ((float)s_CurrentStrength < rain) {
            s_CurrentStrength++;
        } else if ((float)s_CurrentStrength > rain) {
            s_CurrentStrength--;
        }
        s_CurrentStrength = std::max(s_CurrentStrength, 0);

        // TODO(port): TheCamera.GetPosition().z -- shimmed as 0.0f (always <= 900).
        if (!CCullZones::CamNoRain() && !CCullZones::PlayerNoRain()
                && CWeather::UnderWaterness <= 0.0f /* IsUnderWater() */
                && CGame::CanSeeOutSideFromCurrArea()) {
            Grain(s_CurrentStrength / 4, true);
        }
    }

    if (m_bGrainEnable) {
        Grain(m_grainStrength[0], true);
    }

    if (!m_bHeatHazeFX) {
        if (CWeather::HeatHaze > 0.0f || g_fxMan.m_bHeatHazeEnabled) {
            if (CWeather::UnderWaterness < m_fWaterFXStartUnderWaterness) {
                if (CWeather::HeatHaze > 0.0f) {
                    HeatHazeFX(CWeather::HeatHazeFXControl, false);
                } else if (g_fxMan.m_bHeatHazeEnabled) {
                    HeatHazeFX(1.0f, true);
                }
            }
        } else if (CWeather::UnderWaterness >= m_fWaterFXStartUnderWaterness) {
            HeatHazeFX(1.0f, false);
        }
    }

    if (m_waterEnable || CWeather::UnderWaterness >= m_fWaterFXStartUnderWaterness) {
        CRGBA color = m_waterCol;
        color.r = (uint8_t)std::min((int32_t)color.r + 184, 255);
        color.g = (uint8_t)std::min((float)color.g + s_WaterGreen + 184.0f, 255.0f);
        color.b = (uint8_t)std::min((int32_t)color.b + 184, 255);

        const float depthDarkness = m_bWaterDepthDarkness
            ? 1.0f - std::min(CWeather::WaterDepth, m_fWaterFullDarknessDepth) / m_fWaterFullDarknessDepth
            : 0.0f;
        ScaleRGB(color, depthDarkness);

        s_WaterGreen = std::min(s_WaterGreen + CTimer::GetTimeStep(), 24.0f);

        UnderWaterRipple(
            color,
            SCREEN_STRETCH_X(s_WaterGreen / 24.0f * m_xoffset),
            SCREEN_STRETCH_Y(m_yoffset),
            m_waterStrength,
            m_waterSpeed,
            m_waterFreq
        );
    } else {
        s_WaterGreen = 0.0f;
    }

    if (m_bCCTV) {
        CCTV();
    }

    if (CWeaponShim::ms_bTakePhoto && FrontEndMenuManager.m_bIsSaveDone) {
        CWeaponShim::ms_bTakePhoto = false;
        m_bSavePhotoFromScript = false;
    }

    if (s_WaitForOneFrame) {
        if (CWeaponShim::ms_bTakePhoto) {
            s_WaitForOneFrame = true;
            CTimer::Suspend();
            if (s_SavePhotoToGallery) {
                CVisibilityPlugins::RenderWeaponPedsForPC();
                CVisibilityPlugins::ResetWeaponPedsForPC();
                CFileMgr::SetDirMyDocuments();

                int32_t photoIdx = 0;
                // TODO(port): fs::exists check; simplified loop.
                do {
                    photoIdx++;
                    // notsa::format_to_sz(gString, "Gallery\\gallery{}.jpg", photoIdx);
                } while (false);

                JPegCompressScreenToFile(Scene.m_pRwCamera, s_gString);
                CFileMgr::SetDir("");
            }
            CTimer::Resume();

            if (!FrontEndMenuManager.m_bActivateMenuNextFrame) {
                CSpecialFX::bSnapShotActive = true;
                CSpecialFX::SnapShotFrames  = false;
            }
            s_SavePhotoToGallery     = false;
            CWeaponShim::ms_bTakePhoto = false;
            m_bSavePhotoFromScript   = false;
        }
    } else if (CWeaponShim::ms_bTakePhoto) {
        if (FrontEndMenuManager.m_bSavePhotos) {
            s_SavePhotoToGallery = true;
        }
        s_WaitForOneFrame = true;
    }
}

// 0x702080
// NOTE ON VERTEX FIELDS: the decompiled code fills CGlass::ReflectionPolyVertexBuffer
// (declared as 36-byte RxObjSpace3DVertex, see types/layouts.json) using an explicit
// 28-byte stride (0x1c / *7). The decompiler's field labels (objVertex/objNormal/color/u/v)
// are therefore unreliable for this buffer. This port writes the documented 28-byte
// RwIm2DVertex fields (x, y, z, rhw, emissiveColor, u, v) at the exact byte offsets from
// the decomp (+0/+4/+8/+12/+16/+20/+24). Where the decomp's value is ambiguous, the
// most sensible interpretation is used and flagged.
// TODO(port): re-verify the vertex field mapping against the binary.
void CPostEffects::Radiosity(int32_t intensityLimit, int32_t filterPasses,
                             int32_t renderPasses, int32_t intensity) {
    float dimY = m_RadiosityPixelsY; // local_14
    float dimX = m_RadiosityPixelsX; // local_8

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(pRasterFrontBuffer));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));

    RwIm2DVertex* verts = &s_TempVertices[0x200];
    const float recipNear = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    const float zNear = RwDeviceGetZBufferNear();
    const int32_t rasterW = RwRasterGetWidth(pRasterFrontBuffer);
    const int32_t rasterH = RwRasterGetHeight(pRasterFrontBuffer);
    const float fRasterW = (float)rasterW;
    const float fRasterH = (float)rasterH;
    const float uCorr = (float)m_RadiosityFilterUCorrection;
    const float vCorr = (float)m_RadiosityFilterVCorrection;

    s_uiTempBufferVerticesStored = 0;
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(1));

    // Filter passes: repeatedly downsample the front buffer.
    if (filterPasses > 0) {
        RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));

        int32_t passesDone = 0;
        uint32_t v = 0;

        if (filterPasses > 3) {
            // Four passes per iteration (8 vertices).
            for (int32_t p = 3; p < filterPasses; p += 4) {
                // Each of the 4 sub-passes halves the dimensions.
                for (int32_t sub = 0; sub < 4; sub++) {
                    const int32_t halfX = (int32_t)dimX / 2;
                    const int32_t halfY = (int32_t)dimY / 2;

                    // Vertex pair for this downsample step.
                    verts[v].x = 0.0f; verts[v].y = 0.0f; verts[v].z = 0.0f;
                    verts[v].rhw = recipNear;
                    verts[v].emissiveColor = PackARGB(0xFFFFFFFF);
                    verts[v].u = uCorr / fRasterW;
                    verts[v].v = vCorr / fRasterH;

                    verts[v+1].x = 0.0f; verts[v+1].y = 0.0f; verts[v+1].z = 0.0f;
                    verts[v+1].rhw = recipNear;
                    verts[v+1].emissiveColor = PackARGB(0xFFFFFFFF);
                    verts[v+1].u = dimX / fRasterW;
                    verts[v+1].v = dimY / fRasterH;

                    v += 2;
                    dimX = (float)(halfX / 2);
                    dimY = (float)(halfY / 2);
                }
                passesDone += 4;
            }
        }

        // Remaining filter passes, one at a time (2 vertices each).
        for (; passesDone < filterPasses; passesDone++) {
            const int32_t halfX = (int32_t)dimX / 2;
            const int32_t halfY = (int32_t)dimY / 2;

            verts[v].x = 0.0f; verts[v].y = 0.0f; verts[v].z = 0.0f;
            verts[v].rhw = recipNear;
            verts[v].emissiveColor = PackARGB(0xFFFFFFFF);
            verts[v].u = uCorr / fRasterW;
            verts[v].v = vCorr / fRasterH;

            verts[v+1].x = 0.0f; verts[v+1].y = 0.0f; verts[v+1].z = 0.0f;
            verts[v+1].rhw = recipNear;
            verts[v+1].emissiveColor = PackARGB(0xFFFFFFFF);
            verts[v+1].u = dimX / fRasterW;
            verts[v+1].v = dimY / fRasterH;

            v += 2;
            dimX = (float)halfX;
            dimY = (float)halfY;
        }

        s_uiTempBufferVerticesStored = v;
    }

    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));

    // Intensity-limit quad (2 vertices).
    {
        uint32_t v = s_uiTempBufferVerticesStored;
        const uint32_t limitColor =
            ((static_cast<uint32_t>(intensityLimit) | 0xFFFF8000u) << 8 | static_cast<uint32_t>(intensityLimit)) << 8
            | static_cast<uint32_t>(intensityLimit);

        verts[v].x = 0.0f; verts[v].y = 0.0f; verts[v].z = 0.0f;
        verts[v].rhw = recipNear;
        verts[v].emissiveColor = PackARGB(limitColor);
        verts[v].u = dimX + 1.0f;
        verts[v].v = dimY + 1.0f;

        verts[v+1].x = 0.0f; verts[v+1].y = 0.0f; verts[v+1].z = 0.0f;
        verts[v+1].rhw = recipNear;
        verts[v+1].emissiveColor = PackARGB(limitColor);
        verts[v+1].u = 0.0f;
        verts[v+1].v = 0.0f;

        s_uiTempBufferVerticesStored = v + 2;
    }

    // Render passes: upsample and accumulate.
    if (renderPasses > 0) {
        if (s_uiTempBufferVerticesStored > 2) {
            CVisibilityPlugins::RenderTempBuffer(rwPRIMTYPETRILIST, 0xC5F958,
                                                 s_uiTempBufferVerticesStored);
        }
        s_uiTempBufferVerticesStored = 0;

        RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,
                         RWRSTATE(m_bRadiosityLinearFilter ? rwFILTERLINEAR : rwFILTERNEAREST));

        const float screenW = (float)SCREEN_WIDTH;
        const float screenH = (float)SCREEN_HEIGHT;
        const uint32_t intensityColor = static_cast<uint32_t>(intensity) << 24;
        const RwRGBA intColor = PackARGB(intensityColor);

        uint32_t v = 0;
        int32_t passesDone = 0;

        if (!m_bRadiosityStripCopyMode) {
            if (renderPasses > 3) {
                for (int32_t p = 3; p < renderPasses; p += 4) {
                    for (int32_t sub = 0; sub < 4; sub++) {
                        // Upsample step: fullscreen quad sampling the downsampled buffer.
                        for (int32_t k = 0; k < 2; k++) {
                            verts[v].x = 0.0f; verts[v].y = 0.0f; verts[v].z = 0.0f;
                            verts[v].rhw = recipNear;
                            verts[v].emissiveColor = m_bRadiosityDebug ? PackARGB(0xFFFFFFFF) : intColor;
                            verts[v].u = (k == 0) ? screenW : dimX / fRasterW;
                            verts[v].v = (k == 0) ? screenH : dimY / fRasterH;
                            v++;
                        }
                        dimX *= 2.0f;
                        dimY *= 2.0f;
                    }
                    passesDone += 4;
                }
            }
            for (; passesDone < renderPasses; passesDone++) {
                for (int32_t k = 0; k < 2; k++) {
                    verts[v].x = 0.0f; verts[v].y = 0.0f; verts[v].z = 0.0f;
                    verts[v].rhw = recipNear;
                    verts[v].emissiveColor = m_bRadiosityDebug ? PackARGB(0xFFFFFFFF) : intColor;
                    verts[v].u = (k == 0) ? screenW : dimX / fRasterW;
                    verts[v].v = (k == 0) ? screenH : dimY / fRasterH;
                    v++;
                }
                dimX *= 2.0f;
                dimY *= 2.0f;
            }
        } else {
            // Strip-copy mode: direct fullscreen blits.
            if (renderPasses > 3) {
                for (int32_t p = 3; p < renderPasses; p += 4) {
                    for (int32_t sub = 0; sub < 4; sub++) {
                        for (int32_t k = 0; k < 2; k++) {
                            verts[v].x = 0.0f; verts[v].y = 0.0f; verts[v].z = 0.0f;
                            verts[v].rhw = recipNear;
                            verts[v].emissiveColor = intColor;
                            verts[v].u = screenW;
                            verts[v].v = screenH;
                            v++;
                        }
                    }
                    passesDone += 4;
                }
            }
            for (; passesDone < renderPasses; passesDone++) {
                for (int32_t k = 0; k < 2; k++) {
                    verts[v].x = 0.0f; verts[v].y = 0.0f; verts[v].z = 0.0f;
                    verts[v].rhw = recipNear;
                    verts[v].emissiveColor = intColor;
                    verts[v].u = screenW;
                    verts[v].v = screenH;
                    v++;
                }
            }
        }

        s_uiTempBufferVerticesStored = v;
    }

    if (s_uiTempBufferVerticesStored > 2) {
        CVisibilityPlugins::RenderTempBuffer(rwPRIMTYPETRILIST, 0xC5F958,
                                             s_uiTempBufferVerticesStored);
    }
    s_uiTempBufferVerticesStored = 0;

    RwRenderStateSet(rwRENDERSTATEFOGENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(nullptr));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATESRCBLEND, RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, RWRSTATE(rwBLENDINVSRCALPHA));
}
