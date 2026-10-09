// CHud.cpp - GTA SA 1.0 clean-room C++ conversion
// Method implementations.
// Decompiled reference: src/CHud/*.c
// Logic cross-checked against gta-reversed/source/game_sa/Hud.cpp (public source,
// itself annotated from the same binary). Where gta-reversed only stubs a method
// via plugin::Call, the body below is converted directly from the decompiled .c.
//
// Bevy mapping note: these will eventually map to bevy_ui. The draw calls
// (CFont::PrintString, CSprite2d::DrawRect, etc.) are the 2D UI primitives;
// the state machines (fade timers, message queues) are UI state.

#include "CHud.h"

#include <algorithm> // std::min, std::max, std::clamp
#include <cmath>     // std::atan2f
#include <cstdio>    // std::snprintf
#include <cstring>   // std::strcmp, std::strncpy

// Ported subsystem headers used by the filled bodies below.
#include "CFont.h"
#include "CWorld.h"      // FindPlayerPed/Vehicle/Info/Coors, CPlayerInfo fwd
#include "CPlayerPed.h"  // full CPlayerPed (m_pPlayerData via CPed)
#include "CPlayerInfo.h"   // full CPlayerInfo (local shim removed 2026-10-09)
// CVehicle.h NOT included: pulls CVehicleModelInfo.h which redefines CRGBA
// (header bug). Vehicle-specific DrawRadar blocks use forward-declared
// CVehicle* with TODOs. TODO(port): fix CVehicleModelInfo.h CRGBA clash.
#include "CTimer.h"
#include "CPad.h"
#include "CCamera.h"     // TheCamera
#include "CRadar.h"
#include "CWeapon.h"     // CWeapon::ms_bTakePhoto, CWeapon
#include "CSprite2d.h"
#include "CRect.h"
#include "CVector.h"
#include "CMenuManager.h" // FrontEndMenuManager (eFontAlignment deduped, see header)
#include "eWeaponType.h"

// ============================================================================
// TODO(port): external subsystem shims.
// Minimal declarations for subsystems not yet ported to cpp/. Each entry is
// verified against gta-reversed/source/game_sa and the decompiled bodies in
// src/CHud/*.c. Delete entries as their subsystem lands; do not grow
// this list. None of these introduce link-time dependencies for -fsyntax-only
// or `ar` static-library builds.
// ============================================================================

// --- Screen scaling (decomp evidence: X factors use 1/640 = 0.0015625,
// Y factors use 1/448 ~= 0.002232143, e.g. DrawRadar_0058a330.c).
// TODO(port): verify exact SCALE-vs-STRETCH semantics against the render layer.
struct RsGlobalType {
    int32_t maximumWidth{};
    int32_t maximumHeight{};
};
extern RsGlobalType RsGlobal;
#define SCREEN_WIDTH  ((float)RsGlobal.maximumWidth)
#define SCREEN_HEIGHT ((float)RsGlobal.maximumHeight)
#define SCREEN_SCALE_X(x) ((float)(x) * SCREEN_WIDTH / 640.0f)
#define SCREEN_SCALE_Y(y) ((float)(y) * SCREEN_HEIGHT / 448.0f)
#define SCREEN_STRETCH_X(x) SCREEN_SCALE_X(x)
#define SCREEN_STRETCH_Y(y) SCREEN_SCALE_Y(y)
#define SCREEN_SCALE_FROM_RIGHT(x)  (SCREEN_WIDTH - SCREEN_SCALE_X(x))
#define SCREEN_SCALE_FROM_BOTTOM(y) (SCREEN_HEIGHT - SCREEN_SCALE_Y(y))
#define SCREEN_STRETCH_FROM_RIGHT(x)  (SCREEN_WIDTH - SCREEN_STRETCH_X(x))
#define SCREEN_STRETCH_FROM_BOTTOM(y) (SCREEN_HEIGHT - SCREEN_STRETCH_Y(y))
constexpr float DEFAULT_SCREEN_WIDTH  = 640.0f;
constexpr float DEFAULT_SCREEN_HEIGHT = 448.0f;
#ifndef M_PI
constexpr float M_PI = 3.14159265358979323846f;
#endif

// --- RenderWare render-state shims (no RW layer in this build yet).
// The bodies only ever set states; values are passed through opaquely.
using RwRenderState = int32_t;
inline void RwRenderStateSet(RwRenderState, uint32_t) {}
inline void RwRenderStateSet(RwRenderState) {}
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
};
// (rw* enums from RenderTypes.h)
#ifndef RWSTATE_ZTESTENABLE
#define RWSTATE_ZTESTENABLE 9
#define RWSTATE_ZWRITEENABLE 8
#endif
// rwALPHATESTFUNCTIONGREATER not in RenderTypes.h; define locally.
#ifndef rwALPHATESTFUNCTIONGREATER
#define rwALPHATESTFUNCTIONGREATER 5
#endif


// --- CHud sprite indices (gta-reversed Hud.cpp; textures[] in Initialise).
enum eHudSprite {
    SPRITE_FIST = 0,
    SPRITE_SITE_M16,
    SPRITE_SITE_ROCKET,
    SPRITE_RADAR_DISC,
    SPRITE_RADAR_RING_PLANE,
    SPRITE_SKIP_ICON,
};

// --- Flashing helper: true every n-th frame (gta-reversed EachFrames).
inline bool EachFrames(uint32 n) { return (CTimer::m_FrameCounter & (n - 1)) == 0; }

// CWorld statics (CWorld.h declares the API surface only).
// (CPlayerInfo was a local shim here; full definition now in include/CPlayerInfo.h)

struct CWorldHudView {
    static CPlayerInfo* Players;
    static int32_t      PlayerInFocus;
};

// --- Text / GXT shims.
class CText {
public:
    const GxtChar* Get(const char* key);
};
extern CText TheText;
inline const GxtChar* AsciiFromGxtChar(const GxtChar* s) { return s; }
inline void AsciiToGxtChar(const char* src, GxtChar* dst);

// --- Message-box text helpers (not yet ported).
struct CMessages {
    static void   StringCopy(GxtChar* dst, const GxtChar* src, size_t n);
    static bool   StringCompare(const GxtChar* a, const GxtChar* b, size_t n);
    static void   InsertPlayerControlKeysInString(GxtChar* s);
    static void   InsertNumberInString(const GxtChar* src, int32_t n1, int32_t n2,
                                      int32_t n3, int32_t n4, int32_t n5, int32_t n6,
                                      GxtChar* dst);
    static void   AddToPreviousBriefArray(const GxtChar* s);
    static size_t GetStringLength(const GxtChar* s);
};

// --- HUD colours (not yet ported).
enum eHudColour : uint8_t {
    HUD_COLOUR_LIGHT_BLUE = 0,
    HUD_COLOUR_GOLD,
    HUD_COLOUR_LIGHT_GRAY,
    HUD_COLOUR_GREEN,
    HUD_COLOUR_RED,
};
struct CHudColours {
    CRGBA GetRGBA(eHudColour c, uint8_t alpha);
    CRGBA GetRGB(eHudColour c);
};
extern CHudColours HudColour;

// --- Shared scratch buffers (gta-reversed: char gString[256], GxtChar gGxtString[400]).
extern char    gString[256];
extern GxtChar gGxtString[400];

// --- Misc single-purpose subsystems, not yet ported.
struct CClock {
    static uint8_t ms_nGameClockHours;
    static uint8_t ms_nGameClockMinutes;
    static int32_t CurrentDay;
};
struct CStats {
    static float GetFatAndMuscleModifier(int32_t stat);
    static float GetStatValue(int32_t stat);
};
enum eStats {
    STAT_FAT = 21,
    STAT_STAMINA = 22,
    STAT_MUSCLE = 23,
    STAT_TOTAL_RESPECT = 68,
    STAT_SEX_APPEAL = 25,
    STAT_LUNG_CAPACITY = 225,
}; // TODO(port): verify eStats values against the Stats port
constexpr int32_t STAT_MOD_10          = 10; // TODO(port): verify eStatsMod values
constexpr int32_t STAT_MOD_AIR_IN_LUNG = 22; // TODO(port): verify eStatsMod values
struct CGarages {
    static char MessageIDString[8];
};
struct CReplay {
    static int32_t Mode;
};
constexpr int32_t MODE_PLAYBACK = 1;
struct CCutsceneMgr {
    static bool ms_cutsceneProcessing;
    static int32_t ms_running;
    static bool IsRunning();
    static bool IsCutsceneProcessing();
};
struct CGame {
    static int32_t currArea;
};
struct CGameLogic {
    static bool SkipCanBeActivated();
    static bool IsCoopGameGoingOn();
};
struct CEntryExitManager {
    static int32_t ms_exitEnterState;
};
constexpr int32_t EXIT_ENTER_STATE_1 = 1;
constexpr int32_t EXIT_ENTER_STATE_2 = 2;
struct CDarkel {
    static bool FrenzyOnGoing();
};
struct CGangWars {
    static bool bGangWarsActive;
};
struct CZoneInfo {
    CRGBA ZoneColor{};
};
struct CPopCycle {
    static CZoneInfo* m_pCurrZoneInfo;
};
struct CKeyGen {
    static uint32_t AppendStringToKey(uint32_t key, const char* suffix);
};
struct CTheScripts {
    static bool bDisplayHud;
    static bool bDrawSubtitlesBeforeFade;
    static bool bDrawOddJobTitleBeforeFade;
    static bool bDrawCrossHair;
    static bool bPlayerIsOffTheMap;
    static bool    bUseMessageFormatting;
    static int32_t MessageWidth;
    static int32_t MessageCentre;
    struct IntroTextLine {
        char    GXTKey[8]{};
        bool    IsDrawBeforeFade{};
        CVector2D Scale{};
        CRGBA   Color{};
        bool    Justify{};
        bool    HasRightJustify{};
        bool    IsCentered{};
        float   WrapX{};
        float   CentreSize{};
        bool    HasBg{};
        CRGBA   BgColor{};
        bool    IsProportional{};
        CRGBA   DropShadowColor{};
        uint8_t TextEdge{};
        int16_t DropShadow{};
        uint8_t FontStyle{};
        int32_t NumberToInsert1{};
        int32_t NumberToInsert2{};
        CVector2D Pos{};
    };
    static IntroTextLine IntroTextLines[8];
    static void DrawScriptSpritesAndRectangles(bool isBeforeFade);
};
struct CMenuSystem {
    static constexpr int32_t MENU_UNDEFINED = -1;
    static int32_t GetNumMenusInUse();
    static void    Process(int32_t menu);
};
struct CWeaponInfoShim {
    int32_t  m_nAmmoClip{};
    uint32_t m_nModelId1{};
    int32_t  m_nWeaponFire{};
    int32_t  m_nSlot{};
};
struct CWeaponInfo {
    static CWeaponInfoShim* GetWeaponInfo(eWeaponType type);
    static CWeaponInfoShim* GetWeaponInfo(eWeaponType type, int32_t skill);
    static int32_t GetSkillStatIndex(eWeaponType type);
};
// DrawVitalStats: player->m_pIntelligence task-swim check (CTaskSimpleSwim not yet ported).
bool HudIsPlayerSwimming(const CPlayerPed* player);
// CPlayerPedData is only forward-declared in CPed.h; minimal definition for
// the m_fBreath field used by DrawPlayerInfo.
struct CPlayerPedData {
    float m_fBreath;
};
// CAudioEngine shim (not in existing shims).
struct CAudioEngine {
    void ReportFrontendAudioEvent(int32_t eventId, float a, float b);
};
extern CAudioEngine AudioEngine;
const int32_t AE_FRONTEND_DISPLAY_INFO = 0; // TODO(port): real value
// PagerXOffset (pager subsystem not yet ported).
extern int32_t PagerXOffset;
// CWanted shim (eWantedLevel from CPlayerPed.h).
struct CWanted {
    eWantedLevel m_WantedLevel;
    eWantedLevel m_WantedLevelBeforeParole;
    uint32_t m_LastTimeWantedLevelChanged;
};
CWanted* FindPlayerWanted(int32_t playerId);
constexpr int32_t WEAPON_FIRE_USE = 3; // TODO(port): verify eWeaponFire values
// Model IDs: canonical eModelID.h (deduped 2026-10-09; the constexpr here
// redefined its MODEL_VORTEX enumerator - 539 verified vs decomp).
// Ciney-cam frame marker (gta-reversed: bool gbCineyCamProcessedOnFrame).
extern int32_t gbCineyCamProcessedOnFrame;
// RenderWare texture helpers used by DrawWeaponIcon.
struct RwTexDictionary;
struct RwTexture;
RwTexture* RwTexDictionaryFindHashNamedTexture(RwTexDictionary* dict, uint32_t key);
void*      RwTextureGetRaster(RwTexture* tex);
struct CSprite {
    static void RenderOneXLUSprite(const CVector& pos, const CVector2D& size,
                                   uint8_t r, uint8_t g, uint8_t b, int16_t intensity,
                                   float recipZ, int32_t alpha, int32_t arg7, int32_t arg8);
};
RwTexDictionary* HudGetTxdDictionary(int32_t txdIndex); // shim: CTxdStore::ms_pTxdPool lookup
// DrawWeaponIcon: resolve the weapon model's TXD dictionary (shimmed; the real
// path is CTxdStore::ms_pTxdPool->GetAt(mi->m_nTxdIndex)->m_pRwDictionary).
RwTexDictionary* HudGetTxdDictionaryForWeaponIcon(uint32_t modelId);
// RenderBreathBar: player->GetPlayerData()->m_fBreath (CPlayerPedData not yet ported).
float HudGetBreath(const CPed* player);
// CTxdStore (include/CTxdStore.h not includable: missing RenderWare.h/TxdDef.h).
struct CTxdStoreShim {
    static int32_t AddTxdSlot(const char* name);
    static bool    LoadTxd(int32_t index, const char* filename);
    static void    AddRef(int32_t index);
    static void    PushCurrentTxd();
    static void    SetCurrentTxd(int32_t index);
    static void    PopCurrentTxd();
    static void    RemoveTxdSlot(int32_t index);
    static int32_t FindTxdSlot(const char* name);
};
// CGeneral::unk_00821b40() in the decomp is CTimer::GetTimeStepInMS as int.
inline int32_t HudTimeStepMS() { return (int32_t)CTimer::GetTimeStepInMS(); }

// --- CUserDisplay: onscreen timer/counters (not yet ported).
// Layouts verified against plugin-sdk COnscreenTimer.h / COnscreenCounterEntry.h
// / COnscreenTimerEntry.h (VALIDATE_OFFSET) and src/CHud/DrawMissionTimers_0058b180.c.
struct CUserDisplay {
    struct COnscreenTimerEntry {
        uint32_t m_nVarId{};
        char     m_szDescriptionTextKey[10]{};
        char     m_szDisplayedText[42]{};
        bool     m_bEnabled{};
        uint8_t  m_nTimerDirection{};
        uint8_t  _pad[2]{};
        uint32_t m_nClockBeepCountdownSecs{};
    };
    struct COnscreenCounterEntry {
        uint32_t m_nVarId{};
        uint32_t m_nMaxVarValue{};
        char     m_szDescriptionTextKey[10]{};
        uint16_t m_nType{}; // 0 - counter (%), 1 - line, 2 - counter counter (%/%) 
        char     m_szDisplayedText[42]{};
        bool     m_bEnabled{};
        bool     m_bFlashWhenFirstDisplayed{};
        uint8_t  m_nColourId{};
        uint8_t  _pad{};
    };
    struct COnscreenTimer {
        COnscreenTimerEntry   m_Clock{};
        COnscreenCounterEntry m_aCounters[4]{};
        bool m_bDisplay{};
        bool m_bPaused{};
    };
    static COnscreenTimer OnscnTimer;
};


// ============================================================================
// Static member definitions.
// Original GTA SA 1.0 addresses (from gta-reversed StaticRef) kept as comments.
// TODO: re-resolve these for the clean-room build.
// ============================================================================
bool CHud::bScriptDontDisplayAreaName = false;      // 0xBAA3F8
bool CHud::bScriptDontDisplayVehicleName = false;   // 0xBAA3F9
bool CHud::bScriptForceDisplayWithCounters = false; // 0xBAA3FA
bool CHud::bScriptDontDisplayRadar = false;         // 0xBAA3FB

bool CHud::bDrawClock = false; // 0xBAA400

const GxtChar* CHud::m_pVehicleNameToPrint = nullptr; // 0xBAA444
eNameState CHud::m_VehicleState = NAME_DONT_SHOW;     // 0xBAA448
int32_t CHud::m_VehicleFadeTimer = 0;                 // 0xBAA44C
int32_t CHud::m_VehicleNameTimer = 0;                 // 0xBAA450
const GxtChar* CHud::m_pLastVehicleName = nullptr;   // 0xBAA454
const GxtChar* CHud::m_pVehicleName = nullptr;        // 0xBAA458

bool CHud::m_bDraw3dMarkers = false;    // 0xBAA45C
bool CHud::m_Wants_To_Draw_Hud = false; // 0xBAA45D

float CHud::m_fHelpMessageTime = 0.0f;            // 0xBAA460
float CHud::m_fHelpMessageBoxWidth = 200.0f;      // 0x8D0934
bool CHud::m_bHelpMessagePermanent = false;       // 0xBAA464
float CHud::m_fHelpMessageStatUpdateValue = 0.0f; // 0xBAA468
uint16_t CHud::m_nHelpMessageMaxStatValue = 0;    // 0xBAA46C
uint16_t CHud::m_nHelpMessageStatId = 0;          // 0xBAA470
bool CHud::m_bHelpMessageQuick = false;           // 0xBAA472
int32_t CHud::m_nHelpMessageState = 0;            // 0xBAA474
uint32_t CHud::m_nHelpMessageFadeTimer = 0;       // 0xBAA478
uint32_t CHud::m_nHelpMessageTimer = 0;           // 0xBAA47C
GxtChar CHud::m_pHelpMessageToPrint[400]{};       // 0xBAA480
GxtChar CHud::m_pLastHelpMessage[400]{};          // 0xBAA610
GxtChar CHud::m_pHelpMessage[400]{};              // 0xBAA7A0

eNameState CHud::m_ZoneState = NAME_DONT_SHOW; // 0xBAA930
int32_t CHud::m_ZoneFadeTimer = 0;             // 0xBAA934
uint32_t CHud::m_ZoneNameTimer = 0;            // 0xBAA938
const GxtChar* CHud::m_ZoneToPrint = nullptr;  // 0xBAB1D0
const GxtChar* CHud::m_pLastZoneName = nullptr;// 0xBAB1D4
const GxtChar* CHud::m_pZoneName = nullptr;    // 0xBAB1D8

eHudItem CHud::m_ItemToFlash = ITEM_NONE; // 0xBAB1DC
bool CHud::bDrawingVitalStats = false;    // 0xBAB1DE

int32_t CHud::m_LastBreathTime = 0; // 0xBAA3FC

uint32_t CHud::m_WeaponState = 0;     // 0xBAA404
uint32_t CHud::m_WeaponFadeTimer = 0; // 0xBAA408
uint32_t CHud::m_WeaponTimer = 0;     // 0xBAA40C
uint32_t CHud::m_LastWeapon = 0;      // 0xBAA410

uint32_t CHud::m_WantedState = 0;     // 0xBAA414
uint32_t CHud::m_WantedFadeTimer = 0; // 0xBAA418
uint32_t CHud::m_WantedTimer = 0;     // 0xBAA41C
uint32_t CHud::m_LastWanted = 0;      // 0xBAA420

uint32_t CHud::m_DisplayScoreState = 0;     // 0xBAA424
uint32_t CHud::m_DisplayScoreFadeTimer = 0; // 0xBAA428
uint32_t CHud::m_DisplayScoreTimer = 0;     // 0xBAA42C
uint32_t CHud::m_LastDisplayScore = 0;      // 0xBAA430

uint32_t CHud::m_EnergyLostState = 0;     // 0xBAA434
uint32_t CHud::m_EnergyLostFadeTimer = 0; // 0xBAA438
uint32_t CHud::m_EnergyLostTimer = 0;
uint32_t CHud::m_LastTimeEnergyLost = 0;  // 0xBAA440

GxtChar CHud::m_Message[400]{};                                       // 0xBAB040
GxtChar CHud::m_BigMessage[NUM_MESSAGE_STYLES][BIG_MESSAGE_SIZE]{};   // 0xBAACC0
GxtChar CHud::LastBigMessage[NUM_MESSAGE_STYLES][BIG_MESSAGE_SIZE]{}; // 0xBAA940
float CHud::BigMessageAlpha[NUM_MESSAGE_STYLES]{};                    // 0xBAA3A4
float CHud::BigMessageInUse[NUM_MESSAGE_STYLES]{};                    // 0xBAA3C0
float CHud::BigMessageX[NUM_MESSAGE_STYLES]{};                        // 0xBAA3DC

std::array<CSprite2d, 6> CHud::Sprites{}; // 0xBAB1FC

int16_t CHud::TimerMainCounterHideState = 0;            // 0xBAA388
bool CHud::TimerMainCounterWasDisplayed = false;         // 0xBAA38A
std::array<int16_t, 4> CHud::TimerCounterHideState{};     // 0xBAA38C
std::array<int16_t, 4> CHud::TimerCounterWasDisplayed{};   // 0xBAA394

float CHud::OddJob2OffTimer = 0.0f; // 0xBAA398
float CHud::OddJob2XOffset = 0.0f;  // 0xBAA39C
uint16_t CHud::OddJob2Timer = 0;    // 0xBAA3A0
uint16_t CHud::OddJob2On = 0;       // 0xBAB1E0

float CHud::PagerXOffset = 150.0f;    // 0x8D0938
bool CHud::HelpTripSkipShown = false; // 0xBAB229

// File-static state from the decompiled bodies (DAT_ globals Ghidra could not name).
// Addresses kept as comments; names describe the inferred purpose.
namespace {
// DrawSubtitles_0058c250.c: byte flag; set while the DAT_00b6f065 branch is
// taken (cutscene subtitles), cleared otherwise. When cleared after being set,
// the pending m_Message is discarded.
uint8_t s_subtitleCutsceneFlag = 0; // 0xBAB214
// DrawSuccessFailedMessage_0058c6a0.c: cached Y for the big message + init latch.
uint32_t s_successFailedYInit = 0; // 0xBAB21C
float    s_successFailedY = 0.0f;  // 0xBAB218
// DrawBustedWastedMessage_0058ca50.c: cached Y for the busted/wasted text + latch.
uint32_t s_bustedWastedYInit = 0; // 0xBAB224
float    s_bustedWastedY = 0.0f;  // 0xBAB220
// DrawWanted_0058d9a0.c: byte flag; 1 while the wanted level is unchanged
// (drives the flashing-star branch), 0 on change.
uint8_t  s_wantedUnchanged = 0; // 0xBAB228
// DrawSubtitles_0058c250.c / DrawAfterFade_0058d490.c: byte flag; the decomp
// takes the "cutscene" subtitle layout when nonzero. Purpose not fully
// identified from the binary; kept as an opaque flag.
uint8_t  s_cutsceneSubtitleMode = 0; // 0xB6F065
// DrawSubtitles_0058c250.c: byte flag; when 0 and a cutscene is running, the
// subtitle draw is skipped entirely.
uint8_t  s_subtitleSkipFlag = 0; // 0xBA678C
}

// ============================================================================
// Method implementations.
// ============================================================================

// 0x5BA850
void CHud::Initialise() {
    // Texture/mask name pairs, in Sprites[] order. Binary address 0x8D128C.
    static constexpr struct { const char* name; const char* mask; } textures[] = {
        { "fist",           "fistm"           }, // SPRITE_FIST
        { "siteM16",        "siteM16m"        }, // SPRITE_SITE_M16
        { "siterocket",     "siterocketm"     }, // SPRITE_SITE_ROCKET
        { "radardisc",      "radardiscA"      }, // SPRITE_RADAR_DISC
        { "radarRingPlane", "radarRingPlaneA" }, // SPRITE_RADAR_RING_PLANE
        { "SkipIcon",       "SkipIconA"       }, // SPRITE_SKIP_ICON
    };

    int32_t txd = CTxdStoreShim::AddTxdSlot("hud");
    CTxdStoreShim::LoadTxd(txd, "MODELS\\HUD.TXD");
    CTxdStoreShim::AddRef(txd);
    CTxdStoreShim::PushCurrentTxd();
    CTxdStoreShim::SetCurrentTxd(txd);

    for (size_t i = 0; i < Sprites.size(); i++) {
        Sprites[i].SetTexture(textures[i].name, textures[i].mask);
    }
    CTxdStoreShim::PopCurrentTxd();
    ReInitialise();
}

// 0x588880
void CHud::ReInitialise() {
    // Decomp (ReInitialise_00588880.c) zeroes the message buffers with
    // 4-bytes-at-a-time loops; memset is the faithful equivalent.
    std::memset(m_pHelpMessageToPrint, 0, sizeof(m_pHelpMessageToPrint));
    std::memset(m_pLastHelpMessage,    0, sizeof(m_pLastHelpMessage));
    std::memset(m_pHelpMessage,        0, sizeof(m_pHelpMessage));
    std::memset(m_Message,             0, sizeof(m_Message));
    std::memset(m_BigMessage,          0, sizeof(m_BigMessage));
    std::memset(BigMessageX,           0, sizeof(BigMessageX));

    OddJob2On       = 0;
    OddJob2Timer    = 0;
    OddJob2XOffset  = 0.0f;
    OddJob2OffTimer = 0.0f;
    PagerXOffset    = 150.0f;

    TimerCounterHideState.fill(0);
    TimerCounterWasDisplayed.fill(0);
    TimerMainCounterWasDisplayed = false;
    TimerMainCounterHideState = 0;

    // Decomp reads CWorld::Players[CWorld::PlayerInFocus].m_nLastTimeEnergyLost
    // and .m_nDisplayMoney.
    const CPlayerInfo& playerInfo = FindPlayerInfo();
    m_LastTimeEnergyLost            = playerInfo.m_nLastTimeEnergyLost;
    m_LastDisplayScore              = (uint32_t)playerInfo.m_nDisplayMoney;
    m_fHelpMessageStatUpdateValue   = 0.0f;
    m_Wants_To_Draw_Hud             = true;
    m_bDraw3dMarkers                = true;
    m_ZoneNameTimer                 = 0;
    m_pZoneName                     = nullptr;
    m_pLastZoneName                 = nullptr;
    m_ZoneState                     = NAME_DONT_SHOW;
    m_nHelpMessageTimer             = 0;
    m_nHelpMessageFadeTimer         = 0;
    m_nHelpMessageState             = 0;
    m_bHelpMessageQuick             = false;
    m_nHelpMessageStatId            = 0;
    m_nHelpMessageMaxStatValue      = 1000;
    m_bHelpMessagePermanent         = false;
    m_fHelpMessageTime              = 1.0f;
    m_fHelpMessageBoxWidth          = 200.0f;
    m_pVehicleName                  = nullptr;
    m_pLastVehicleName              = nullptr;
    m_pVehicleNameToPrint           = nullptr;
    m_VehicleNameTimer              = 0;
    m_VehicleFadeTimer              = 0;
    m_VehicleState                  = NAME_DONT_SHOW;
    bScriptDontDisplayRadar         = false;
    bScriptForceDisplayWithCounters = false;
    bScriptDontDisplayVehicleName   = false;
    bScriptDontDisplayAreaName      = false;
    m_ItemToFlash                   = ITEM_NONE;
    m_EnergyLostTimer               = 0;
    m_EnergyLostFadeTimer           = 0;
    m_EnergyLostState               = 5;
    m_DisplayScoreTimer             = 0;
    m_DisplayScoreFadeTimer         = 0;
    m_DisplayScoreState             = 5;
    m_LastWanted                    = 0;
    m_WantedTimer                   = 0;
    m_WantedFadeTimer               = 0;
    m_WantedState                   = 5;
    m_LastWeapon                    = 0;
    m_WeaponTimer                   = 0;
    m_WeaponFadeTimer               = 0;
    m_WeaponState                   = 5;
    bDrawClock                      = true;
    m_LastBreathTime                = 0;
}

// 0x588850
void CHud::Shutdown() {
    for (auto& sprite : Sprites) {
        sprite.Delete();
    }
    CTxdStoreShim::RemoveTxdSlot(CTxdStoreShim::FindTxdSlot("hud"));
}

// 0x588A50
void CHud::GetRidOfAllHudMessages(bool arg0) {
    std::memset(m_pHelpMessageToPrint, 0, sizeof(m_pHelpMessageToPrint));
    std::memset(m_pLastHelpMessage,    0, sizeof(m_pLastHelpMessage));
    std::memset(m_pHelpMessage,        0, sizeof(m_pHelpMessage));
    std::memset(m_Message,             0, sizeof(m_Message));

    m_ZoneNameTimer               = 0;
    m_pZoneName                   = nullptr;
    m_ZoneState                   = NAME_DONT_SHOW;
    m_nHelpMessageTimer           = 0;
    m_nHelpMessageFadeTimer       = 0;
    m_nHelpMessageState           = 0;
    m_bHelpMessageQuick           = false;
    m_nHelpMessageMaxStatValue    = 1000;
    m_nHelpMessageStatId          = 0;
    m_fHelpMessageStatUpdateValue = 0.0f;
    m_bHelpMessagePermanent       = false;
    m_fHelpMessageTime            = 1.0f;
    m_pVehicleName                = nullptr;
    m_pVehicleNameToPrint         = nullptr;
    m_VehicleNameTimer            = 0;
    m_VehicleFadeTimer            = 0;
    m_VehicleState                = NAME_DONT_SHOW;

    for (int32_t i = 0; i < NUM_MESSAGE_STYLES; ++i) {
        if (BigMessageX[i] != 0.0f)
            continue;
        if (arg0) {
            if (i == STYLE_BOTTOM_RIGHT || i == STYLE_MIDDLE_SMALLER_HIGHER)
                continue;
        }
        std::memset(m_BigMessage[i], 0, sizeof(m_BigMessage[i]));
    }
}

// 0x588B60
float CHud::GetYPosBasedOnHealth(uint8_t playerId, float pos, int8_t offset) {
    // Decomp (GetYPosBasedOnHealth_00588b60.c): shifts the Y down when the
    // player's max health is below 101 (i.e. the health bar is one row).
    return (float)FindPlayerInfo(playerId).m_nMaxHealth < 101.0f
               ? pos - SCREEN_SCALE_Y((float)offset)
               : pos;
}

// 0x588B50
bool CHud::HelpMessageDisplayed() {
    return m_nHelpMessageState != 0;
}

// 0x588F60
void CHud::SetMessage(const GxtChar* message) {
    // Decomp (SetMessage_00588f60.c): bounded copy of up to 400 GxtChars,
    // NUL-terminated; null input clears.
    if (message) {
        std::strncpy((char*)m_Message, (const char*)AsciiFromGxtChar(message), sizeof(m_Message) - 1);
        m_Message[sizeof(m_Message) - 1] = 0;
    } else {
        m_Message[0] = 0;
    }
}

// 0x588FC0
void CHud::SetBigMessage(GxtChar* message, eMessageStyle style) {
    if (BigMessageX[style] != 0.0f) {
        return;
    }

    std::strncpy((char*)m_BigMessage[style], (const char*)AsciiFromGxtChar(message),
                 sizeof(m_BigMessage[style]) - 1);
    m_BigMessage[style][sizeof(m_BigMessage[style]) - 1] = 0;

    switch (style) {
    case STYLE_WHITE_MIDDLE_SMALLER: {
        if (std::strcmp((const char*)AsciiFromGxtChar(message),
                        (const char*)AsciiFromGxtChar(LastBigMessage[STYLE_WHITE_MIDDLE_SMALLER])) != 0) {
            OddJob2OffTimer = 0.0f;
            OddJob2On = 0;
        }
        std::strncpy((char*)LastBigMessage[style], (const char*)AsciiFromGxtChar(message),
                     sizeof(LastBigMessage[style]) - 1);
        LastBigMessage[style][sizeof(LastBigMessage[style]) - 1] = 0;
        break;
    }
    default: {
        message[0] = 0;
        break;
    }
    }
}

// 0x588BE0
void CHud::SetHelpMessage(const GxtChar* text, bool quickMessage, bool permanent, bool addToBrief) {
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0] ||
        CReplay::Mode == MODE_PLAYBACK || CCutsceneMgr::IsRunning()) {
        return;
    }

    std::memset(m_pHelpMessageToPrint, 0, sizeof(m_pHelpMessageToPrint));
    std::memset(m_pLastHelpMessage,    0, sizeof(m_pLastHelpMessage));
    std::memset(m_pHelpMessage,        0, sizeof(m_pHelpMessage));

    CMessages::StringCopy(m_pHelpMessage, text, sizeof(m_pHelpMessage));
    CMessages::InsertPlayerControlKeysInString(m_pHelpMessage);
    if (m_nHelpMessageState && CMessages::StringCompare(m_pHelpMessage, m_pHelpMessageToPrint, sizeof(m_pHelpMessage)))
        return;

    std::memset(m_pLastHelpMessage, 0, sizeof(m_pLastHelpMessage));
    if (!text) {
        m_pHelpMessage[0] = 0;
        m_pHelpMessageToPrint[0] = 0;
    }

    if (permanent) {
        m_nHelpMessageState = 1;
        CMessages::StringCopy(m_pHelpMessageToPrint, m_pHelpMessage, sizeof(m_pHelpMessage));
        CMessages::StringCopy(m_pLastHelpMessage, m_pHelpMessage, sizeof(m_pHelpMessage));
    } else {
        m_nHelpMessageState = 0;
    }

    if (addToBrief)
        CMessages::AddToPreviousBriefArray(text);

    m_bHelpMessagePermanent = permanent;
    m_bHelpMessageQuick = quickMessage;
    m_nHelpMessageStatId = 0;
    m_nHelpMessageMaxStatValue = 1000;
    m_fHelpMessageStatUpdateValue = 0.0f;
}

// 0x588D40
void CHud::SetHelpMessageStatUpdate(eStatUpdateState state, uint16_t statId, float diff, float max) {
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0] ||
        CReplay::Mode == MODE_PLAYBACK || CCutsceneMgr::IsCutsceneProcessing()) {
        return;
    }

    std::memset(m_pHelpMessageToPrint, 0, sizeof(m_pHelpMessageToPrint));
    std::memset(m_pLastHelpMessage,    0, sizeof(m_pLastHelpMessage));
    std::memset(m_pHelpMessage,        0, sizeof(m_pHelpMessage));

    if (m_nHelpMessageState && CMessages::StringCompare(m_pHelpMessage, m_pHelpMessageToPrint, sizeof(m_pHelpMessage)))
        return;

    std::memset(m_pLastHelpMessage, 0, sizeof(m_pLastHelpMessage));
    m_nHelpMessageState = 0;
    m_bHelpMessageQuick = false;
    m_bHelpMessagePermanent = false;
    m_nHelpMessageStatId = statId;
    m_fHelpMessageStatUpdateValue = diff;
    m_nHelpMessageMaxStatValue = (uint16_t)max;
    std::snprintf(gString, sizeof(gString), "%s", state == STAT_UPDATE_INCREASE ? "+" : "-");
    AsciiToGxtChar(gString, m_pHelpMessage);
}

// 0x588E30
void CHud::SetHelpMessageWithNumber(const GxtChar* text, int32_t number, bool quickMessage, bool permanent) {
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] || CGarages::MessageIDString[0] ||
        CReplay::Mode == MODE_PLAYBACK || CCutsceneMgr::IsCutsceneProcessing()) {
        return;
    }

    GxtChar str[400]{};
    CMessages::InsertNumberInString(text, number, -1, -1, -1, -1, -1, str);
    CMessages::GetStringLength(str);
    CMessages::StringCopy(m_pHelpMessage, str, sizeof(m_pHelpMessage));
    CMessages::InsertPlayerControlKeysInString(m_pHelpMessage);

    std::memset(m_pLastHelpMessage, 0, sizeof(m_pLastHelpMessage));
    if (permanent) {
        m_nHelpMessageState = 1;
        CMessages::StringCopy(m_pHelpMessageToPrint, m_pHelpMessage, sizeof(m_pHelpMessage));
        CMessages::StringCopy(m_pLastHelpMessage, m_pHelpMessage, sizeof(m_pHelpMessage));
    } else {
        m_nHelpMessageState = 0;
    }

    m_bHelpMessagePermanent = permanent;
    m_bHelpMessageQuick = quickMessage;
    m_nHelpMessageStatId = 0;
    m_nHelpMessageMaxStatValue = 1000;
    m_fHelpMessageStatUpdateValue = 0.0f;
}

// 0x588F50
void CHud::SetVehicleName(const GxtChar* name) {
    m_pVehicleName = name;
}

// 0x588BB0
void CHud::SetZoneName(const GxtChar* name, bool displayImmediately) {
    if (displayImmediately) {
        m_pZoneName = name;
        return;
    }
    if (CGame::currArea || m_ZoneState != NAME_DONT_SHOW) {
        return;
    }
    m_pZoneName = name;
}

// called each frame from Render2dStuff()
// 0x58FAE0
void CHud::Draw() {
    if (CReplay::Mode == MODE_PLAYBACK || CWeapon::ms_bTakePhoto ||
        FrontEndMenuManager.m_bActivateMenuNextFrame ||
        gbCineyCamProcessedOnFrame == (int32_t)CTimer::GetFrameCounter())
        return;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATEFOGENABLE,         RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATESRCBLEND,          RWRSTATE(rwBLENDSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEDESTBLEND,         RWRSTATE(rwBLENDINVSRCALPHA));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(1));
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,    RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER,     RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,         RWRSTATE(rwSHADEMODEFLAT));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTION, RWRSTATE(rwALPHATESTFUNCTIONGREATER));
    RwRenderStateSet(rwRENDERSTATEALPHATESTFUNCTIONREF, RWRSTATE(0));

    if (!TheCamera.m_bWideScreenOn) {
        DrawCrossHairs();
        if (FrontEndMenuManager.m_bHudOn && CTheScripts::bDisplayHud) {
            DrawPlayerInfo();
            DrawWanted();
        }
        if (!bScriptDontDisplayVehicleName) {
            DrawVehicleName();
        }
        DrawMissionTimers();
    }

    if (!bScriptDontDisplayRadar && !TheCamera.m_bWideScreenOn) {
        CPed* player = FindPlayerPed();
        CPad* pad = CPad::GetPad();
        if (!pad->GetDisplayVitalStats(player) || FindPlayerVehicle()) {
            bDrawingVitalStats = false;
            DrawRadar();
        } else {
            bDrawingVitalStats = true;
            DrawVitalStats();
        }
        if (!CGameLogic::SkipCanBeActivated() || bDrawingVitalStats) {
            HelpTripSkipShown = false;
        } else {
            DrawTripSkip();
            if (!HelpTripSkipShown) {
                SetHelpMessage(TheText.Get("SKIP_1"), true, false, false);
                HelpTripSkipShown = true;
            }
        }
    }

    if (m_bDraw3dMarkers && !TheCamera.m_bWideScreenOn) {
        CRadar::Draw3dMarkers();
    }

    if (!CTimer::GetIsUserPaused()) {
        if (!m_BigMessage[STYLE_MIDDLE][0]) {
            if (CMenuSystem::GetNumMenusInUse()) {
                CMenuSystem::Process(CMenuSystem::MENU_UNDEFINED);
            }
            DrawScriptText(true);
        }
        if (CTheScripts::bDrawSubtitlesBeforeFade) {
            DrawSubtitles();
        }
        DrawHelpText();
        DrawOddJobMessage(true);
        DrawSuccessFailedMessage();
        DrawBustedWastedMessage();
    }
}

// 0x58D490
void CHud::DrawAfterFade() {
    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER,     RWRSTATE(rwFILTERNEAREST));
    RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS,    RWRSTATE(rwTEXTUREADDRESSCLAMP));
    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, RWRSTATE(0));

    if (CTimer::GetIsUserPaused() || CReplay::Mode == MODE_PLAYBACK || CWeapon::ms_bTakePhoto)
        return;

    // Decomp (DrawAfterFade_0058d490.c): DAT_00ba67a4/DAT_00b6f065 are the
    // "don't draw area name" latches; the named bScriptDontDisplayAreaName
    // covers the script side. Vehicle plane/heli check via IsSubPlane/IsSubHeli.
    auto* vehicle = FindPlayerVehicle();
    // TODO(port): CVehicle not included (header clash); vehicle type checks stubbed.
    if (!vehicle) {
        if (!CCutsceneMgr::ms_cutsceneProcessing) {
            if (!FrontEndMenuManager.m_bMenuActive && !TheCamera.m_bWideScreenOn && !bScriptDontDisplayAreaName) {
                DrawAreaName();
            }
        }
    }

    if (!m_BigMessage[STYLE_MIDDLE][0]) {
        DrawScriptText(false);
    }

    if (!CTheScripts::bDrawSubtitlesBeforeFade) {
        DrawSubtitles();
    }

    DrawMissionTitle();
    DrawOddJobMessage(false);
}

// 0x58AA50
void CHud::DrawAreaName() {
    if (!m_pZoneName) {
        return;
    }

    if (m_pZoneName != m_pLastZoneName) {
        switch (m_ZoneState) {
        case NAME_DONT_SHOW:
            if ((!CTheScripts::bPlayerIsOffTheMap && CTheScripts::bDisplayHud) ||
                CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_1 ||
                CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_2) {
                m_ZoneState = NAME_FADE_IN;
                m_ZoneNameTimer = 0;
                m_ZoneFadeTimer = 0;
                m_ZoneToPrint = m_pZoneName;
                if (m_VehicleState == NAME_SHOW || m_VehicleState == NAME_FADE_IN) {
                    m_VehicleState = NAME_FADE_OUT;
                }
            }
            break;
        case NAME_SHOW:
        case NAME_FADE_IN:
        case NAME_FADE_OUT:
            m_ZoneState = NAME_SWITCH;
            m_ZoneNameTimer = 0;
            break;
        case NAME_SWITCH:
            m_ZoneNameTimer = 0;
            break;
        default:
            break;
        }
        m_pLastZoneName = m_pZoneName;
    }

    if (!m_ZoneState)
        return;

    float alpha = 255.0f;
    switch (m_ZoneState) {
    case NAME_SHOW:
        m_ZoneFadeTimer = 1000;
        if (m_ZoneNameTimer > 3000) {
            m_ZoneState = NAME_FADE_OUT;
            m_ZoneFadeTimer = 1000;
        }
        break;

    case NAME_FADE_IN:
        if (!TheCamera.GetFading() && TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            m_ZoneFadeTimer += (int32_t)CTimer::GetTimeStepInMS();
        }
        if (m_ZoneFadeTimer > 1000) {
            m_ZoneFadeTimer = 1000;
            m_ZoneState = NAME_SHOW;
        }
        if (TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            alpha = (float)m_ZoneFadeTimer / 1000.0f * 255.0f;
            break;
        }
        m_ZoneState = NAME_FADE_OUT;
        m_ZoneFadeTimer = 1000;
        break;

    case NAME_FADE_OUT:
        if (!TheCamera.GetFading() && TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            m_ZoneFadeTimer -= (int32_t)CTimer::GetTimeStepInMS();
        }
        if (m_ZoneFadeTimer < 0) {
            m_ZoneFadeTimer = 0;
            m_ZoneState = NAME_DONT_SHOW;
        }
        if (TheCamera.GetScreenFadeStatus() != NAME_FADE_IN) {
            alpha = (float)m_ZoneFadeTimer / 1000.0f * 255.0f;
            break;
        }
        m_ZoneFadeTimer = 1000;
        break;

    case NAME_SWITCH:
        m_ZoneFadeTimer -= (int32_t)CTimer::GetTimeStepInMS();
        if (m_ZoneFadeTimer < 0) {
            m_ZoneFadeTimer = 0;
            m_ZoneState = NAME_FADE_IN;
            m_ZoneToPrint = m_pLastZoneName;
        }
        alpha = (float)m_ZoneFadeTimer / 1000.0f * 255.0f;
        break;

    default:
        break;
    }

    if (m_Message[0] || BigMessageX[STYLE_BOTTOM_RIGHT] != 0.0f || BigMessageX[STYLE_WHITE_MIDDLE] != 0.0f) {
        m_ZoneState = NAME_FADE_OUT;
        return;
    }

    m_ZoneNameTimer += (uint32_t)CTimer::GetTimeStepInMS();
    CFont::SetProportional(true);
    CFont::SetBackground(false, false);
    CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.2f), SCREEN_SCALE_Y(1.9f));
    CFont::SetEdge(2);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(SCREEN_STRETCH_X(180.0f));
    CFont::SetDropColor({ 0, 0, 0, (uint8_t)alpha });
    CFont::SetFontStyle(FONT_GOTHIC);

    const CZoneInfo* info = CPopCycle::m_pCurrZoneInfo;
    if (CGangWars::bGangWarsActive && info && info->ZoneColor.r && info->ZoneColor.g && info->ZoneColor.b) {
        CFont::SetColor({ info->ZoneColor.r, info->ZoneColor.g, info->ZoneColor.b, (uint8_t)alpha });
    } else {
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_LIGHT_BLUE, (uint8_t)alpha));
    }

    CFont::PrintStringFromBottom(SCREEN_STRETCH_FROM_RIGHT(32.0f),
                                 SCREEN_SCALE_FROM_BOTTOM(104.0f) + SCREEN_SCALE_Y(76.0f),
                                 m_ZoneToPrint);
    CFont::SetSlant(0.0f);
}

// 0x58CA50
void CHud::DrawBustedWastedMessage() {
    auto& message      = m_BigMessage[STYLE_WHITE_MIDDLE];
    auto& messageX     = BigMessageX[STYLE_WHITE_MIDDLE];
    auto& messageAlpha = BigMessageAlpha[STYLE_WHITE_MIDDLE];

    if (!message[0]) {
        messageX = 0.0f;
        return;
    }

    if (messageX == 0.0f) {
        messageX = 1.0f;
        messageAlpha = 0.0f;

        if (m_VehicleState) {
            m_VehicleState = NAME_DONT_SHOW;
        }
        if (m_ZoneState) {
            m_ZoneState = NAME_DONT_SHOW;
        }
        return;
    }

    messageAlpha += CTimer::GetTimeStepInMS() * 0.4f;
    messageAlpha = std::min(messageAlpha, 255.0f);

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(2.1f), SCREEN_SCALE_Y(2.1f));
    CFont::SetProportional(true);
    CFont::SetJustify(false);
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetFontStyle(FONT_GOTHIC);
    CFont::SetEdge(3);
    CFont::SetDropColor({ 0, 0, 0, (uint8_t)messageAlpha });
    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_LIGHT_GRAY, (uint8_t)messageAlpha));
    // Decomp (DrawBustedWastedMessage_0058ca50.c) caches the centered Y in
    // DAT_00bab220 behind the DAT_00bab224 bit-0 latch (see s_bustedWastedY).
    if (!(s_bustedWastedYInit & 1)) {
        s_bustedWastedYInit |= 1;
        s_bustedWastedY = (float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(30.0f);
    }
    CFont::PrintStringFromBottom(SCREEN_WIDTH / 2.0f, s_bustedWastedY, message);
}

// 0x589070
void CHud::ResetWastedText() {
    BigMessageX[STYLE_WHITE_MIDDLE] = 0.0f;
    m_BigMessage[STYLE_WHITE_MIDDLE][0] = 0;

    BigMessageX[STYLE_MIDDLE] = 0.0f;
    m_BigMessage[STYLE_MIDDLE][0] = 0;
}

// 0x58D580
float CHud::DrawFadeState(DRAW_FADE_STATE fadingElement, int32_t forceFadingIn) {
    uint32_t state, timer, fadeTimer;
    switch (fadingElement) {
    case WANTED_STATE:
        fadeTimer = m_WantedFadeTimer;
        state = m_WantedState;
        timer = m_WantedTimer;
        break;
    case ENERGY_LOST_STATE:
        fadeTimer = m_EnergyLostFadeTimer;
        state = m_EnergyLostState;
        timer = m_EnergyLostTimer;
        break;
    case DISPLAY_SCORE_STATE:
        fadeTimer = m_DisplayScoreFadeTimer;
        state = m_DisplayScoreState;
        timer = m_DisplayScoreTimer;
        break;
    case WEAPON_STATE:
        fadeTimer = m_WeaponFadeTimer;
        state = m_WeaponState;
        timer = m_WeaponTimer;
        break;
    default:
        state = fadingElement;
        timer = fadingElement;
        fadeTimer = fadingElement;
        break;
    }

    if (forceFadingIn) {
        switch (state) {
        case NAME_DONT_SHOW:
            fadeTimer = 0;
            break;
        case NAME_SWITCH:
        case NAME_FADE_OUT:
            timer = 5;
            state = NAME_FADE_IN;
            break;
        default:
            break;
        }
    }

    float alpha = 255.0f;
    if (state != NAME_DONT_SHOW) {
        switch (state) {
        case NAME_SHOW:
            fadeTimer = 1000;
            if (timer > 10000) {
                fadeTimer = 3000;
                state = NAME_FADE_OUT;
            }
            break;
        case NAME_FADE_IN:
            fadeTimer += (uint32_t)CTimer::GetTimeStepInMS();
            if (fadeTimer > 1000) {
                state = NAME_SHOW;
                fadeTimer = 1000;
            }
            alpha = (float)fadeTimer / 1000.0f * 255.0f;
            break;
        case NAME_FADE_OUT:
            fadeTimer -= (uint32_t)CTimer::GetTimeStepInMS();
            if ((int32_t)fadeTimer < 0) {
                fadeTimer = 0;
                state = NAME_DONT_SHOW;
            }
            alpha = (float)fadeTimer / 1000.0f * 255.0f;
            break;
        default:
            break;
        }
        timer += (uint32_t)CTimer::GetTimeStepInMS();
    }

    switch (fadingElement) {
    case WANTED_STATE:
        m_WantedFadeTimer = fadeTimer;
        m_WantedState = state;
        m_WantedTimer = timer;
        break;
    case ENERGY_LOST_STATE:
        m_EnergyLostFadeTimer = fadeTimer;
        m_EnergyLostState = state;
        m_EnergyLostTimer = timer;
        break;
    case DISPLAY_SCORE_STATE:
        m_DisplayScoreFadeTimer = fadeTimer;
        m_DisplayScoreState = state;
        m_DisplayScoreTimer = timer;
        break;
    case WEAPON_STATE:
        m_WeaponFadeTimer = fadeTimer;
        m_WeaponState = state;
        m_WeaponTimer = timer;
        break;
    default:
        break;
    }

    return std::clamp(alpha, 0.0f, 255.0f);
}

// 0x58D240
void CHud::DrawMissionTitle() {
    auto& message      = m_BigMessage[STYLE_BOTTOM_RIGHT];
    auto& messageX     = BigMessageX[STYLE_BOTTOM_RIGHT];
    auto& messageAlpha = BigMessageAlpha[STYLE_BOTTOM_RIGHT];
    auto& messageInUse = BigMessageInUse[STYLE_BOTTOM_RIGHT];

    if (!message[0]) {
        messageX = 0.0f;
        return;
    }

    if (messageX == 0.0f) {
        messageInUse = -60.0f;
        messageX = 1.0f;
        m_ZoneState = NAME_DONT_SHOW;
        m_ZoneFadeTimer = 0;
        SetHelpMessage(nullptr, true, false, false);
        return;
    }

    CFont::SetBackground(false, false);
    CFont::SetProportional(true);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetScale(SCREEN_STRETCH_X(1.0f), SCREEN_SCALE_Y(1.3f));

    if (messageInUse >= SCREEN_WIDTH - 20.0f) {
        messageX += CTimer::GetTimeStep();
        if (messageX >= 120.0f) {
            messageX = 120.0f;
            messageAlpha -= CTimer::GetTimeStepInMS();
        }
        if (messageAlpha <= 0.0f) {
            messageAlpha = 0.0f;
            message[0] = 0;
            messageX = 0.0f;
        }
    } else {
        messageAlpha = 255.0f;
        messageInUse += CTimer::GetTimeStepInMS() * 0.3f;
    }

    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, (uint8_t)messageAlpha });
    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GOLD, (uint8_t)messageAlpha));
    CFont::PrintStringFromBottom(SCREEN_SCALE_FROM_RIGHT(20.0f), SCREEN_SCALE_FROM_BOTTOM(115.0f), message);
    CFont::SetEdge(0);
}

// 0x58CC80
void CHud::DrawOddJobMessage(bool displayImmediately) {
    const auto& m1 = m_BigMessage[STYLE_BOTTOM_RIGHT];
    const auto& m4 = m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER];
    if (displayImmediately == CTheScripts::bDrawOddJobTitleBeforeFade && !m1[0] && m4[0]) {
        CFont::SetBackground(false, false);
        CFont::SetJustify(false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.6f), SCREEN_SCALE_Y(1.35f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(350.0f));
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_GOLD));
        CFont::PrintStringFromBottom((float)(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(140.0f), m4);
    }

    if (!displayImmediately)
        return;

    const auto& m6 = m_BigMessage[STYLE_LIGHT_BLUE_TOP];
    if (m6[0]) {
        CFont::SetBackground(false, false);
        CFont::SetJustify(false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.0f), SCREEN_SCALE_Y(1.8f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(500.0f));
        CFont::SetFontStyle(FONT_PRICEDOWN);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_BLUE));
        CFont::PrintString((float)(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(60.0f), m6);
    }

    const auto& m3 = m_BigMessage[STYLE_MIDDLE_SMALLER];
    if (m3[0]) {
        CFont::SetBackground(false, false);
        CFont::SetJustify(false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.6f), SCREEN_SCALE_Y(1.35f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(500.0f));
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_GOLD));
        CFont::PrintString((float)(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(155.0f), m3);
    }

    if (OddJob2OffTimer > 0.0f) {
        OddJob2OffTimer -= CTimer::GetTimeStepInMS();
    }

    const auto& m5 = m_BigMessage[STYLE_WHITE_MIDDLE_SMALLER];
    if (!m5[0])
        return;

    if (OddJob2OffTimer > 0.0f)
        return;

    switch (OddJob2On) {
    case 0:
        OddJob2XOffset = 380.0f;
        OddJob2On = 1;
        break;
    case 1:
        if (OddJob2XOffset <= 2.0f) {
            OddJob2On = 2;
            OddJob2Timer = 0;
        } else {
            OddJob2XOffset -= std::min(OddJob2XOffset / 6.0f, 40.0f);
        }
        break;
    case 2:
        OddJob2Timer += (uint16_t)CTimer::GetTimeStepInMS();
        if (OddJob2Timer > 1500) {
            OddJob2On = 3;
        }
        break;
    case 3:
        OddJob2XOffset -= std::max(OddJob2XOffset / 5.0f, 30.0f);
        if (OddJob2XOffset < -380.0f) {
            OddJob2On = 0;
            OddJob2OffTimer = 5000.0f;
        }
        break;
    default:
        break;
    }

    if (!m1[0]) {
        CFont::SetBackground(false, false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.6f), SCREEN_SCALE_Y(1.35f));
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetProportional(true);
        CFont::SetCentreSize(SCREEN_STRETCH_X(500.0f));
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 255 });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
        CFont::PrintString((float)(RsGlobal.maximumWidth / 2), SCREEN_STRETCH_Y(217.0f), m5);
    }
}

// 0x58A330
void CHud::DrawRadar() {
    // Decomp (DrawRadar_0058a330.c) gates on CEntryExitManager::ms_exitEnterState,
    // the frontend radar mode, and the ITEM_RADAR flash timer.
    if (CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_1 ||
        CEntryExitManager::ms_exitEnterState == EXIT_ENTER_STATE_2 ||
        FrontEndMenuManager.m_nRadarMode == eRadarMode::RADAR_MODE_OFF ||
        (m_ItemToFlash == ITEM_RADAR && EachFrames(8))) {
        return;
    }

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));
    RwRenderStateSet(rwRENDERSTATESHADEMODE,     RWRSTATE(rwFILTERNEAREST));

    CRadar::DrawMap();

    if (FrontEndMenuManager.m_nRadarMode == eRadarMode::RADAR_MODE_BLIPS_ONLY) {
        CRadar::DrawBlips();
        return;
    }

    CVehicle* vehicle = FindPlayerVehicle();
    CRect rect{};
    // Decomp: plane ring sprite when in a plane (not the Vortex).
    // TODO(port): plane ring sprite needs CVehicle (IsSubPlane, m_nModelIndex, m_matrix).
    if (false) {
        float angle = 0.0f;
        CRadar::DrawRotatingRadarSprite(
            Sprites[SPRITE_RADAR_RING_PLANE],
            SCREEN_STRETCH_X(87.0f),
            SCREEN_STRETCH_FROM_BOTTOM(66.0f),
            angle,
            (uint32_t)SCREEN_STRETCH_X(78.0f),
            (uint32_t)SCREEN_STRETCH_Y(59.0f),
            CRGBA{ 255, 255, 255, 255 }
        );
    }

    CPlayerPed* player = FindPlayerPed();
    // Decomp: altimeter bar for planes/helis (not Vortex) or while parachuting.
    // TODO(port): altimeter vehicle check needs CVehicle.
    if (false ||
        player->GetActiveWeapon().m_Type == WEAPON_PARACHUTE) {
        rect.left   = SCREEN_STRETCH_X(40.0f) - SCREEN_STRETCH_X(20.0f);
        rect.bottom = SCREEN_STRETCH_FROM_BOTTOM(104.0f);
        rect.right  = SCREEN_STRETCH_X(40.0f) - SCREEN_STRETCH_X(10.0f);
        rect.top    = SCREEN_STRETCH_Y(76.0f) + SCREEN_STRETCH_FROM_BOTTOM(104.0f);
        CSprite2d::DrawRect(rect, { 10, 10, 10, 100 }); // rectangle

        const CVector pos = player->GetPosition(); // TODO(port): vehicle->GetPosition()
        float lineY = 950.0f;
        if (pos.z <= 200.0f) {
            lineY = 200.0f;
        }
        RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(0));

        rect.left   = SCREEN_STRETCH_X(40.0f) - SCREEN_STRETCH_X(25.0f);
        rect.bottom = SCREEN_STRETCH_FROM_BOTTOM(104.0f) + SCREEN_STRETCH_Y(76.0f)
                      - std::min(SCREEN_STRETCH_Y(76.0f), SCREEN_STRETCH_Y(76.0f) * pos.z / lineY);
        rect.right  = SCREEN_STRETCH_X(40.0f) - 5.0f;
        rect.top    = rect.bottom + 2.0f;
        CSprite2d::DrawRect(rect, { 200, 200, 200, 200 }); // horizontal line (current height)
    }

    const CRGBA black{ 0, 0, 0, 255 };

    rect.left   = SCREEN_STRETCH_X(36.0f);
    rect.bottom = SCREEN_STRETCH_FROM_BOTTOM(108.0f);
    rect.right  = SCREEN_STRETCH_X(87.0f);
    rect.top    = SCREEN_STRETCH_FROM_BOTTOM(66.0f);
    Sprites[SPRITE_RADAR_DISC].Draw(rect, black); // top left

    rect.bottom = SCREEN_STRETCH_FROM_BOTTOM(24.0f);
    Sprites[SPRITE_RADAR_DISC].Draw(rect, black); // bottom left

    rect.left   = SCREEN_STRETCH_X(138.0f);
    rect.bottom = SCREEN_STRETCH_FROM_BOTTOM(108.0f);
    Sprites[SPRITE_RADAR_DISC].Draw(rect, black); // top right

    rect.bottom = SCREEN_STRETCH_FROM_BOTTOM(24.0f);
    Sprites[SPRITE_RADAR_DISC].Draw(rect, black); // bottom right

    CRadar::DrawBlips();
}

// 0x58C080
void CHud::DrawScriptText(bool isBeforeFade) {
    CTheScripts::DrawScriptSpritesAndRectangles(isBeforeFade);

    for (auto& t : CTheScripts::IntroTextLines) {
        if (!t.GXTKey[0]) {
            continue;
        }
        if (t.IsDrawBeforeFade != isBeforeFade) {
            continue;
        }

        CFont::SetScale(SCREEN_SCALE_X(t.Scale.x), SCREEN_SCALE_Y(t.Scale.y / 2.0f));
        CFont::SetColor(t.Color);
        CFont::SetJustify(t.Justify);
        if (t.HasRightJustify) {
            CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
        } else {
            CFont::SetOrientation(t.IsCentered ? eFontAlignment::ALIGN_CENTER : eFontAlignment::ALIGN_LEFT);
        }
        CFont::SetWrapx(SCREEN_SCALE_X(t.WrapX));
        CFont::SetCentreSize(SCREEN_SCALE_X(t.CentreSize));
        CFont::SetBackground(t.HasBg, false);
        CFont::SetBackgroundColor(t.BgColor);
        CFont::SetProportional(t.IsProportional);
        CFont::SetDropColor(t.DropShadowColor);
        if (t.TextEdge) {
            CFont::SetEdge(t.TextEdge);
        } else {
            CFont::SetDropShadowPosition(t.DropShadow);
        }
        CFont::SetFontStyle((eFontStyle)t.FontStyle);

        GxtChar text[400]{};
        CMessages::InsertNumberInString(
            TheText.Get(t.GXTKey),
            t.NumberToInsert1,
            t.NumberToInsert2,
            -1, -1, -1, -1,
            text
        );
        CMessages::InsertPlayerControlKeysInString(text);
        CFont::PrintString(
            SCREEN_SCALE_FROM_RIGHT(DEFAULT_SCREEN_WIDTH - t.Pos.x),
            SCREEN_SCALE_FROM_BOTTOM(DEFAULT_SCREEN_HEIGHT - t.Pos.y),
            text
        );
        CFont::SetEdge(0);
    }
}

// 0x58AEA0
void CHud::DrawVehicleName() {
    if (!m_pVehicleName) {
        m_VehicleState = NAME_DONT_SHOW;
        m_VehicleNameTimer = 0;
        m_VehicleFadeTimer = 0;
        m_pLastVehicleName = nullptr;
        return;
    }

    if (m_pVehicleName != m_pLastVehicleName) {
        switch (m_VehicleState) {
        case NAME_DONT_SHOW:
            m_VehicleState = NAME_FADE_IN;
            m_VehicleNameTimer = 0;
            m_VehicleFadeTimer = 0;
            m_pVehicleNameToPrint = m_pVehicleName;
            if (m_ZoneState == NAME_SHOW || m_ZoneState == NAME_FADE_IN) {
                m_ZoneState = NAME_FADE_OUT;
            }
            break;
        case NAME_SHOW:
        case NAME_FADE_IN:
        case NAME_FADE_OUT:
        case NAME_SWITCH:
            m_VehicleState = NAME_SWITCH;
            m_VehicleNameTimer = 0;
            break;
        default:
            break;
        }
        m_pLastVehicleName = m_pVehicleName;
    }

    if (!m_VehicleState)
        return;

    float alpha = 0.0f;
    switch (m_VehicleState) {
    case NAME_SHOW:
        if (m_VehicleNameTimer > 3000) {
            m_VehicleState = NAME_FADE_OUT;
            m_VehicleFadeTimer = 1000;
        }
        alpha = 255.0f;
        break;
    case NAME_FADE_IN:
        m_VehicleFadeTimer += (int32_t)CTimer::GetTimeStepInMS();
        if (m_VehicleFadeTimer > 1000) {
            m_VehicleFadeTimer = 1000;
            m_VehicleState = NAME_SHOW;
        }
        alpha = (float)m_VehicleFadeTimer / 1000.0f * 255.0f;
        break;
    case NAME_FADE_OUT:
        m_VehicleFadeTimer -= (int32_t)CTimer::GetTimeStepInMS();
        if (m_VehicleFadeTimer < 0) {
            m_VehicleState = NAME_DONT_SHOW;
            m_VehicleFadeTimer = 0;
        }
        alpha = (float)m_VehicleFadeTimer / 1000.0f * 255.0f;
        break;
    case NAME_SWITCH:
        m_VehicleFadeTimer -= (int32_t)CTimer::GetTimeStepInMS();
        if (m_VehicleFadeTimer < 0) {
            m_VehicleNameTimer = 0;
            m_VehicleState = NAME_FADE_IN;
            m_VehicleFadeTimer = 0;
            m_pVehicleNameToPrint = m_pLastVehicleName;
        }
        alpha = (float)m_VehicleFadeTimer / 1000.0f * 255.0f;
        break;
    default:
        break;
    }

    if (!m_Message[0]) {
        m_VehicleNameTimer += (int32_t)CTimer::GetTimeStepInMS();
        CFont::SetProportional(true);
        CFont::SetBackground(false, false);
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(1.0f), SCREEN_SCALE_Y(1.5f));
        CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
        CFont::SetRightJustifyWrap(0.0f);
        CFont::SetFontStyle(FONT_MENU);
        CFont::SetEdge(2);
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GREEN, (uint8_t)alpha));
        CFont::SetDropColor({ 0, 0, 0, (uint8_t)alpha });
        if (CTheScripts::bDisplayHud) {
            CFont::PrintString(
                SCREEN_STRETCH_FROM_RIGHT(32.0f),
                SCREEN_STRETCH_FROM_BOTTOM(104.0f),
                m_pVehicleNameToPrint
            );
        }
        CFont::SetSlant(0.0f);
    }
}

// 0x5893B0
void CHud::DrawAmmo(CPed* ped, int32_t x, int32_t y, float alpha) {
    constexpr uint32_t MAX_CLIP = 9999;

    const CWeapon& weapon = ped->GetActiveWeapon();
    const uint32_t totalAmmo = weapon.m_TotalAmmo;
    const uint32_t ammoInClip = weapon.m_AmmoInClip;
    // Decomp (DrawAmmo_005893b0.c): clip size from CWeaponInfo::GetWeaponInfo.
    const int32_t ammoClip = CWeaponInfo::GetWeaponInfo(weapon.m_Type, (int32_t)ped->GetWeaponSkill())->m_nAmmoClip;

    if (ammoClip <= 1 || ammoClip >= 1000) {
        std::snprintf(gString, sizeof(gString), "%d", totalAmmo);
    } else {
        uint32_t total, current;

        if (weapon.m_Type == WEAPON_FLAMETHROWER) {
            uint32_t out = MAX_CLIP;
            if ((totalAmmo - ammoInClip) / 10 <= MAX_CLIP) {
                out = (totalAmmo - ammoInClip) / 10u;
            }
            total = out;
            current = ammoInClip / 10;
        } else {
            uint32_t out = totalAmmo - ammoInClip;
            if (totalAmmo - ammoInClip > MAX_CLIP) {
                out = MAX_CLIP;
            }
            total = out;
            current = ammoInClip;
        }
        std::snprintf(gString, sizeof(gString), "%d-%d", total, current);
    }
    AsciiToGxtChar(gString, gGxtString);

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.3f), SCREEN_STRETCH_Y(0.7f));
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetCentreSize(SCREEN_STRETCH_Y(640.0f));
    CFont::SetProportional(true);
    CFont::SetEdge(1);
    CFont::SetDropColor({ 0, 0, 0, 255 });
    CFont::SetFontStyle(FONT_SUBTITLES);

    const CWeaponInfoShim* wi = CWeaponInfo::GetWeaponInfo(weapon.m_Type);
    if (totalAmmo - weapon.m_AmmoInClip >= MAX_CLIP
        || CDarkel::FrenzyOnGoing()
        || weapon.m_Type == WEAPON_UNARMED
        || weapon.m_Type == WEAPON_DETONATOR
        || weapon.m_Type == WEAPON_DILDO1
        || weapon.m_Type == WEAPON_DILDO2
        || weapon.m_Type == WEAPON_VIBE1
        || weapon.m_Type == WEAPON_VIBE2
        || weapon.m_Type == WEAPON_FLOWERS
        || weapon.m_Type == WEAPON_CANE
        || weapon.m_Type == WEAPON_PARACHUTE
        || wi->m_nWeaponFire == WEAPON_FIRE_USE
        || wi->m_nSlot <= 1) {
        CFont::SetEdge(0);
        return;
    }

    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_LIGHT_BLUE, (uint8_t)alpha));
    CFont::PrintString((float)x, (float)y, gGxtString);
    CFont::SetEdge(0);
}

void CHud::DrawClock() {
    char ascii[16]{};
    GxtChar gxtText[16]{};
    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.55f), SCREEN_STRETCH_Y(1.1f));
    CFont::SetProportional(false);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, 255 });
    std::snprintf(ascii, sizeof(ascii), "%02d:%02d",
                  CClock::ms_nGameClockHours, CClock::ms_nGameClockMinutes);
    AsciiToGxtChar(ascii, gxtText);
    CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
    CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f), SCREEN_STRETCH_Y(22.0f), gxtText);
    CFont::SetEdge(0);
}

void CHud::DrawMoney(const CPlayerInfo& playerInfo, uint8_t alpha) {
    char ascii[16]{};
    GxtChar gxtText[16]{};

    if (playerInfo.m_nDisplayMoney < 0) {
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_RED, alpha));
        int32_t displayMoney = playerInfo.m_nDisplayMoney;
        if (displayMoney < 0) {
            displayMoney = -displayMoney;
        }
        std::snprintf(ascii, sizeof(ascii), "-$%07d", displayMoney);
    } else {
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GREEN, alpha));
        std::snprintf(ascii, sizeof(ascii), "$%08d", std::abs(playerInfo.m_nDisplayMoney));
    }
    AsciiToGxtChar(ascii, gxtText);
    CFont::SetProportional(false);
    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.55f), SCREEN_STRETCH_Y(1.1f));
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetDropShadowPosition(0);
    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, alpha });
    // Decomp uses CWorld::PlayerInFocus for the health-based Y shift.
    CFont::PrintString(SCREEN_STRETCH_FROM_RIGHT(32.0f),
                       GetYPosBasedOnHealth((uint8_t)CWorldHudView::PlayerInFocus,
                                           SCREEN_STRETCH_Y(89.0f), 12),
                       gxtText);
    CFont::SetEdge(0);
}

void CHud::DrawWeapon(CPlayerPed* ped0, CPlayerPed* ped1) {
    const float magic = SCREEN_WIDTH * 0.17343046f; // todo: magic
    if (m_WeaponState) {
        DrawWeaponIcon(ped0, (int32_t)(SCREEN_WIDTH - (SCREEN_STRETCH_X(32.0f) + magic)),
                       (int32_t)SCREEN_STRETCH_Y(20.0f), (float)m_WeaponFadeTimer);
        if (ped1) {
            const auto posX = (int32_t)(SCREEN_WIDTH - (SCREEN_STRETCH_X(32.0f) + 111.0f));
            const auto posY = (int32_t)GetYPosBasedOnHealth((uint8_t)CWorldHudView::PlayerInFocus,
                                                           SCREEN_STRETCH_Y(138.0f), 12);
            DrawWeaponIcon(ped1, posX, posY, (float)m_WeaponFadeTimer);
        }

        const auto ammoPosX = (int32_t)(SCREEN_WIDTH - (magic + SCREEN_STRETCH_X(32.0f))
                                        + SCREEN_STRETCH_X(47.0f / 2.0f));
        const float ammoPosY = SCREEN_STRETCH_Y(43.0f);
        DrawAmmo(ped0, ammoPosX, (int32_t)ammoPosY + (int32_t)SCREEN_STRETCH_Y(20.0f),
                 (float)m_WeaponFadeTimer);
        if (ped1) {
            const auto posY = (int32_t)GetYPosBasedOnHealth((uint8_t)CWorldHudView::PlayerInFocus,
                                                           ammoPosY + SCREEN_STRETCH_Y(138.0f), 12);
            DrawAmmo(ped1, ammoPosX, posY, (float)m_WeaponFadeTimer);
        }
    }
}

// 0x58A160
void CHud::DrawTripSkip() {
    CRect rect{
        SCREEN_STRETCH_X(54.0f),
        SCREEN_STRETCH_FROM_BOTTOM(189.0f),
        SCREEN_STRETCH_X(118.0f),
        SCREEN_STRETCH_FROM_BOTTOM(125.0f)
    };
    Sprites[SPRITE_SKIP_ICON].Draw(rect, CRGBA{ 255, 255, 255, 255 });

    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.3f), SCREEN_SCALE_Y(0.7f));
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetCentreSize(SCREEN_WIDTH);
    CFont::SetProportional(true);
    CFont::SetEdge(1);
    CFont::SetDropColor({ 0, 0, 0, 255 });
    CFont::SetFontStyle(FONT_MENU);
    CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
    CFont::PrintString(
        SCREEN_STRETCH_X(64.0f) / 2.0f + SCREEN_STRETCH_X(54.0f),
        SCREEN_STRETCH_FROM_BOTTOM(127.0f),
        TheText.Get("FEC_TSK") // TRIP SKIP
    );
}

// 0x58D7D0
void CHud::DrawWeaponIcon(CPed* ped, int32_t x, int32_t y, float alpha) {
    const float x0 = (float)x;
    const float y0 = (float)y;
    const float width  = SCREEN_STRETCH_X(47.0f);
    const float height = SCREEN_STRETCH_Y(58.0f);
    const float halfWidth  = width / 2.0f;
    const float halfHeight = height / 2.0f;

    RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, RWRSTATE(rwFILTERLINEAR));

    // Decomp (DrawWeaponIcon_0058d7d0.c): model id from the weapon info.
    const CWeapon& weapon = ped->GetActiveWeapon();
    const int32_t modelId = CWeaponInfo::GetWeaponInfo(weapon.m_Type)->m_nModelId1;
    if (modelId <= 0) {
        Sprites[SPRITE_FIST].Draw({ x0, y0, width + x0, height + y0 },
                                  CRGBA{ 255, 255, 255, (uint8_t)alpha });
        return;
    }

    // Texture lookup via the weapon model's TXD (shimmed; CTxdStore.h not includable).
    RwTexDictionary* txd = HudGetTxdDictionaryForWeaponIcon((uint32_t)modelId);
    if (!txd)
        return;

    RwTexture* texture = RwTexDictionaryFindHashNamedTexture(
        txd, CKeyGen::AppendStringToKey((uint32_t)modelId, "ICON"));
    if (!texture)
        return;

    RwRenderStateSet(rwRENDERSTATEZTESTENABLE,   RWRSTATE(0));
    RwRenderStateSet(rwRENDERSTATETEXTURERASTER, RWRSTATE(0)); // raster from texture (shim)
    CSprite::RenderOneXLUSprite(
        { x0 + halfWidth, y0 + halfHeight, 1.0f },
        { halfWidth, halfHeight },
        255, 255, 255, 255,
        1.0f,
        255,
        0, 0
    );
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, RWRSTATE(0));
}

// 0x5890A0
void CHud::RenderArmorBar(int32_t playerId, int32_t x, int32_t y) {
    auto* player = FindPlayerPed(playerId);
    if ((m_ItemToFlash == ITEM_ARMOUR && EachFrames(8)) || player->m_fArmour <= 1.0f)
        return;

    const CPlayerInfo& info = FindPlayerInfo(playerId);
    CSprite2d::DrawBarChart(
        (float)x,
        (float)y,
        (uint16_t)SCREEN_STRETCH_X(62.0f),
        (uint8_t)SCREEN_STRETCH_Y(9.0f),
        player->m_fArmour / (float)info.m_nMaxArmour * 100.0f,
        0,
        0,
        1,
        HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY),
        CRGBA{ 0, 0, 0, 0 }
    );
}

// 0x589190
void CHud::RenderBreathBar(int32_t playerId, int32_t x, int32_t y) {
    if (m_ItemToFlash == ITEM_BREATH && EachFrames(8))
        return;

    auto* player = FindPlayerPed(playerId);
    CSprite2d::DrawBarChart(
        (float)x,
        (float)y,
        (uint16_t)SCREEN_STRETCH_X(62.0f),
        (uint8_t)SCREEN_STRETCH_Y(9.0f),
        HudGetBreath(player) / CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG) * 100.0f,
        0,
        0,
        1,
        HudColour.GetRGB(HUD_COLOUR_LIGHT_BLUE),
        CRGBA{ 0, 0, 0, 0 }
    );
}

// 0x589270
void CHud::RenderHealthBar(int32_t playerId, int32_t x, int32_t y) {
    if (m_ItemToFlash == ITEM_HEALTH && EachFrames(8))
        return;

    auto* player = FindPlayerPed(playerId);
    if ((int32_t)player->m_fHealth < 10 && EachFrames(8))
        return;

    const float x109 = SCREEN_STRETCH_X(109.0f);
    const CPlayerInfo& info = FindPlayerInfo(playerId);
    const uint16_t totalWidth =
        (uint16_t)(x109 * (float)info.m_nMaxHealth / CStats::GetFatAndMuscleModifier(STAT_MOD_10));

    CSprite2d::DrawBarChart(
        x109 - (float)totalWidth + (float)x,
        (float)y,
        totalWidth,
        (uint8_t)SCREEN_STRETCH_Y(9.0f),
        player->m_fHealth * 100.0f / (float)info.m_nMaxHealth,
        0,
        0,
        1,
        HudColour.GetRGB(HUD_COLOUR_RED),
        CRGBA{ 0, 0, 0, 0 }
    );
}

// 0x58C250
// Converted from src/CHud/DrawSubtitles_0058c250.c (gta-reversed only stubs it).
void CHud::DrawSubtitles() {
    if (m_Message[0] == 0) {
        return;
    }
    // Decomp: skip when a big "wasted/busted"-style message is up and no coop game.
    if (m_BigMessage[STYLE_WHITE_MIDDLE][0] != 0 && !CGameLogic::IsCoopGameGoingOn()) {
        return;
    }
    if (m_VehicleState != NAME_DONT_SHOW) {
        m_VehicleState = NAME_FADE_OUT;
    }
    if (m_ZoneState != NAME_DONT_SHOW) {
        m_ZoneState = NAME_FADE_OUT;
    }
    CFont::SetBackground(false, false);
    CFont::SetBackgroundColor({ 0, 0, 0, 0x80 });
    CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
    CFont::SetProportional(true);
    CFont::SetDropShadowPosition(0);
    CFont::SetFontStyle(FONT_SUBTITLES);
    CFont::SetColor({ 0xe1, 0xe1, 0xe1, 0xff });
    CFont::SetDropShadowPosition(2);
    CFont::SetDropColor({ 0, 0, 0, 0xff });

    float x, y;
    // Decomp: CCredits::unk_007170c0(r,g,b,a) just builds a CRGBA; the calls
    // above are inlined as literals.
    if (s_cutsceneSubtitleMode == 0) {
        if (s_subtitleCutsceneFlag != 0) {
            m_Message[0] = 0;
        }
        s_subtitleCutsceneFlag = 0;
        CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.58f), SCREEN_SCALE_Y(1.22f));
        if (!CTheScripts::bUseMessageFormatting) {
            if (!bDrawingVitalStats) {
                const float sw = SCREEN_STRETCH_X(1.0f); // 1px in scaled units
                CFont::SetCentreSize(SCREEN_WIDTH - sw * 20.0f - sw * 8.0f - (sw * 140.0f + sw * 8.0f));
                const float sh = SCREEN_SCALE_Y(1.0f);
                const float fVar2 = sw;
                const float fx = fVar2 * 140.0f + fVar2 * 8.0f;
                y = SCREEN_HEIGHT - sh * 105.0f - (sh + sh);
                x = ((SCREEN_WIDTH - fVar2 * 20.0f - fVar2 * 8.0f) - fx) * 0.5f + fx;
            } else {
                CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.58f * 0.8f),
                                                  SCREEN_SCALE_Y(1.22f));
                const float sw = SCREEN_STRETCH_X(1.0f);
                CFont::SetCentreSize(((SCREEN_WIDTH - sw * 20.0f - sw * 8.0f) -
                                      (sw * 140.0f + sw * 8.0f)) * 0.8f);
                const float sh = SCREEN_SCALE_Y(1.0f);
                const float fVar2 = sw;
                const float fx = fVar2 * 140.0f + fVar2 * 8.0f;
                y = SCREEN_HEIGHT - sh * 105.0f - (sh + sh);
                x = fVar2 * 40.0f +
                    ((SCREEN_WIDTH - fVar2 * 20.0f - fVar2 * 8.0f) - fx) * 0.5f + fx;
            }
        } else {
            CFont::SetCentreSize(SCREEN_STRETCH_X((float)CTheScripts::MessageWidth));
            const float sh = SCREEN_SCALE_Y(1.0f);
            y = SCREEN_HEIGHT - sh * 105.0f - (sh + sh);
            x = SCREEN_STRETCH_X((float)CTheScripts::MessageCentre);
        }
    } else {
        s_subtitleCutsceneFlag = 1;
        if (s_subtitleSkipFlag == 0 && CCutsceneMgr::ms_running != 0) {
            goto skipPrint;
        }
        CFont::SetCentreSize(SCREEN_WIDTH - SCREEN_STRETCH_X(60.0f));
        CFont::SetScale(SCREEN_STRETCH_X(0.58f), SCREEN_SCALE_Y(1.2f));
        y = SCREEN_HEIGHT - SCREEN_SCALE_Y(80.0f);
        x = (float)(RsGlobal.maximumWidth / 2);
    }
    CFont::PrintString(x, y, m_Message);
skipPrint:
    CFont::SetDropShadowPosition(0);
}

// 0x58C6A0
// Converted from src/CHud/DrawSuccessFailedMessage_0058c6a0.c (gta-reversed only stubs it).
void CHud::DrawSuccessFailedMessage() {
    // Decomp: one-time Y computation behind the 0xBAB21C bit-0 latch.
    if ((s_successFailedYInit & 1) == 0) {
        s_successFailedYInit |= 1;
        s_successFailedY = (float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(10.0f);
    }
    if (m_BigMessage[STYLE_MIDDLE][0] == 0) {
        BigMessageX[STYLE_MIDDLE] = 0.0f;
        return;
    }
    if (BigMessageX[STYLE_MIDDLE] != 0.0f) {
        CFont::SetBackground(false, false);
        CFont::SetScale(SCREEN_STRETCH_X(1.3f), SCREEN_SCALE_Y(1.8f));
        CFont::SetProportional(true);
        CFont::SetJustify(false);
        CFont::SetOrientation(eFontAlignment::ALIGN_CENTER);
        CFont::SetCentreSize(SCREEN_STRETCH_X(590.0f));
        CFont::SetFontStyle(FONT_PRICEDOWN);
        CFont::SetEdge(2);
        const int32_t stepMS = HudTimeStepMS();
        CFont::SetDropColor({ 0, 0, 0, (uint8_t)stepMS });
        CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GOLD, (uint8_t)stepMS));
        if (SCREEN_WIDTH - 20.0f <= BigMessageInUse[STYLE_MIDDLE]) {
            BigMessageX[STYLE_MIDDLE] = CTimer::GetTimeStep() + BigMessageX[STYLE_MIDDLE];
            if (BigMessageX[STYLE_MIDDLE] >= 120.0f) {
                BigMessageX[STYLE_MIDDLE] = 120.0f;
                // Decomp: unsigned wrap handling for the negative-step case.
                BigMessageAlpha[STYLE_MIDDLE] -= (float)(uint32_t)stepMS * 0.3f;
            }
            // Decomp: `if (BigMessageAlpha[0] < 0.0 != (BigMessageAlpha[0] == 0.0))`
            // i.e. alpha <= 0 clears the message.
            if (BigMessageAlpha[STYLE_MIDDLE] <= 0.0f) {
                BigMessageAlpha[STYLE_MIDDLE] = 0.0f;
                m_BigMessage[STYLE_MIDDLE][0] = 0;
            }
        } else {
            const float fStep = (float)(uint32_t)stepMS;
            BigMessageInUse[STYLE_MIDDLE] += fStep * 0.3f;
            BigMessageAlpha[STYLE_MIDDLE] = fStep * 0.3f + BigMessageAlpha[STYLE_MIDDLE];
            if (BigMessageAlpha[STYLE_MIDDLE] > 255.0f) {
                BigMessageAlpha[STYLE_MIDDLE] = 255.0f;
            }
        }
        CFont::PrintString(SCREEN_WIDTH / 2.0f, s_successFailedY, m_BigMessage[STYLE_MIDDLE]);
        return;
    }
    BigMessageInUse[STYLE_MIDDLE] = -60.0f;
    BigMessageX[STYLE_MIDDLE] = 1.0f;
    BigMessageAlpha[STYLE_MIDDLE] = 0.0f;
    if (m_BigMessage[STYLE_MIDDLE_SMALLER_HIGHER][0] == 0 && m_BigMessage[STYLE_WHITE_MIDDLE_SMALLER][0] == 0) {
        if (m_BigMessage[STYLE_WHITE_MIDDLE][0] == 0) {
            const int16_t lines = CFont::GetNumberLines(
                SCREEN_WIDTH / 2.0f,
                (float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(10.0f),
                m_BigMessage[STYLE_MIDDLE]);
            if (lines > 1) {
                s_successFailedY = (float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(10.0f)
                                   - SCREEN_SCALE_Y(15.0f);
                return;
            }
        }
        s_successFailedY = (float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(10.0f);
        return;
    }
    s_successFailedY = SCREEN_SCALE_Y(25.0f) +
                       ((float)(RsGlobal.maximumHeight / 2) - SCREEN_SCALE_Y(10.0f));
}

// 0x58B180
// Converted from src/CHud/DrawMissionTimers_0058b180.c (gta-reversed only stubs it).
void CHud::DrawMissionTimers() {
    // Decomp gate: skip when the oddjob big message is up (unless forced) or a
    // garage message is showing.
    if ((m_BigMessage[STYLE_MIDDLE_SMALLER][0] != 0 && !bScriptForceDisplayWithCounters) ||
        CGarages::MessageIDString[0] != 0) {
        return;
    }

    const float yUnit = SCREEN_SCALE_Y(1.0f);
    float yClock = GetYPosBasedOnHealth((uint8_t)CWorldHudView::PlayerInFocus, yUnit * 148.0f, 12);
    float yCounters = GetYPosBasedOnHealth(1, yClock, 12);
    if (FindPlayerPed(1)) {
        yClock += yUnit * 72.0f;
        yCounters += yUnit * 72.0f;
    }

    CFont::SetProportional(true);
    CFont::SetBackground(false, false);
    CFont::SetScale(SCREEN_STRETCH_X(0.5f), SCREEN_SCALE_Y(1.0f));
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetFontStyle(FONT_MENU);
    CFont::SetWrapx(SCREEN_STRETCH_X(640.0f));
    CFont::SetEdge(2);
    CFont::SetDropColor({ 0, 0, 0, 0xff });

    CUserDisplay::COnscreenTimer& timer = CUserDisplay::OnscnTimer;
    const bool clockEnabled = timer.m_Clock.m_bEnabled;
    if (FindPlayerPed(1) && !timer.m_Clock.m_bEnabled) {
        TimerMainCounterWasDisplayed = timer.m_Clock.m_bEnabled;
    }
    // Decomp: clear the WasDisplayed latch for any disabled counter.
    for (int32_t i = 0; i < 4; i++) {
        if (!timer.m_aCounters[i].m_bEnabled) {
            TimerCounterWasDisplayed[i] = 0;
        }
    }

    if (!timer.m_bDisplay) {
        return;
    }

    GxtChar localBuf[200]{};
    if (clockEnabled) {
        if (!TimerMainCounterWasDisplayed) {
            TimerMainCounterHideState = 1;
        }
        TimerMainCounterWasDisplayed = true;
        if (TimerMainCounterHideState != 0 && ++TimerMainCounterHideState > 50) {
            TimerMainCounterHideState = 0;
        }
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
        if ((CTimer::m_FrameCounter & 4) != 0 || TimerMainCounterHideState == 0) {
            AsciiToGxtChar(timer.m_Clock.m_szDisplayedText, localBuf);
            CFont::PrintString(SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f), yClock, localBuf);
            if (timer.m_Clock.m_szDescriptionTextKey[0] != 0) {
                const GxtChar* desc = TheText.Get(timer.m_Clock.m_szDescriptionTextKey);
                CFont::PrintString(SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f) - SCREEN_STRETCH_X(90.0f),
                                   yClock, desc);
            }
        }
    } else {
        yCounters = GetYPosBasedOnHealth((uint8_t)CWorldHudView::PlayerInFocus, yUnit * 148.0f, 12);
        if ((float)FindPlayerInfo(1).m_nMaxHealth < 101.0f) {
            yCounters -= yUnit * 12.0f;
        }
        if (FindPlayerPed(1)) {
            yCounters += yUnit * 72.0f;
        }
    }

    for (int32_t i = 0; i < 4; i++) {
        auto& counter = timer.m_aCounters[i];
        if (counter.m_bEnabled) {
            if (!TimerCounterWasDisplayed[i] && counter.m_bFlashWhenFirstDisplayed) {
                TimerCounterHideState[i] = 1;
            }
            TimerCounterWasDisplayed[i] = 1;
            if (TimerCounterHideState[i] != 0 && ++TimerCounterHideState[i] > 50) {
                TimerCounterHideState[i] = 0;
            }
            if ((CTimer::m_FrameCounter & 4) != 0 || TimerCounterHideState[i] == 0) {
                CFont::SetColor(HudColour.GetRGB((eHudColour)counter.m_nColourId));
                const float y = yUnit * 20.0f * (float)i * yUnit + yCounters;
                if (counter.m_nType == 1) { // LINE: progress bar
                    const long value = std::atol(counter.m_szDisplayedText);
                    CSprite2d::DrawBarChart(
                        SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f),
                        yUnit * 6.0f + y,
                        (uint16_t)HudTimeStepMS(), (uint8_t)HudTimeStepMS(),
                        (float)(int16_t)value * 0.01f * 100.0f,
                        0, 0, 1,
                        HudColour.GetRGB((eHudColour)counter.m_nColourId),
                        CRGBA{ 0, 0, 0, 0 });
                } else {
                    AsciiToGxtChar(counter.m_szDisplayedText, localBuf);
                    CFont::PrintString(SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f), y, localBuf);
                }
                if (counter.m_szDescriptionTextKey[0]) {
                    const GxtChar* desc = TheText.Get(counter.m_szDescriptionTextKey);
                    CFont::PrintString(SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f) - SCREEN_STRETCH_X(90.0f),
                                       y, desc);
                }
            }
        }
    }
}

// 0x589650
// Converted from src/CHud/DrawVitalStats_00589650.c (gta-reversed only stubs it).
//
// NOTE on CGeneral::unk_00821b40(): the decomp labels several small reads with
// this name. Where the value feeds a fade timer or an alpha it is
// CTimer::GetTimeStepInMS() (confirmed by gta-reversed's DrawFadeState). At the
// four sites below the value feeds a layout coordinate; the exact source is
// unidentified, so screen-scaled constants inferred from the DrawWindow rect
// (left=40, top=H-140*sh) are used. TODO(port): identify 0x00821b40.
void CHud::DrawVitalStats() {
    if (CReplay::Mode == MODE_PLAYBACK) {
        return;
    }
    if (s_cutsceneSubtitleMode != 0) {
        return;
    }
    CPlayerPed* player = FindPlayerPed(-1);
    const eWeaponType activeWeapon = player->GetActiveWeapon().m_Type;

    CFont::SetBackground(false, false);
    CFont::SetColor({ 0xe1, 0xe1, 0xe1, 0xff });
    CFont::SetWrapx(SCREEN_STRETCH_X(640.0f));
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetProportional(true);

    // Decomp: weapon type read as float bits; 4.48416e-44/3.92364e-44 are
    // denormal bit patterns from the float read of the weapon-type int.
    const bool showWeaponSkill = activeWeapon >= WEAPON_PISTOL && activeWeapon <= WEAPON_COUNTRYRIFLE;
    const bool isSwimming = HudIsPlayerSwimming(player);

    const float sh = SCREEN_SCALE_Y(1.0f);
    CRect window{
        40.0f,
        SCREEN_HEIGHT - sh * 140.0f,
        SCREEN_STRETCH_X(170.0f) + 40.0f,
        SCREEN_HEIGHT - sh * 140.0f + sh * 15.0f + sh * 127.0f
    };
    // Decomp draws the window with top/bottom swapped between the two branches;
    // the net rect is the same.
    FrontEndMenuManager.DrawWindow(window, "FEH_STA", 0, { 0, 0, 0, 0xbe }, false, true);

    CFont::SetFontStyle(FONT_SUBTITLES);
    CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
    CFont::SetScale(SCREEN_STRETCH_X(0.35f), SCREEN_SCALE_Y(0.9f));
    CFont::SetColor({ 0xe1, 0xe1, 0xe1, 0xff });
    CFont::SetEdge(0);

    // Layout (inferred; see NOTE above).
    const float labelX = 50.0f;
    const float barX   = 110.0f;
    float y = SCREEN_HEIGHT - sh * 120.0f;
    const float rowH = sh * 18.0f;

    auto drawStatRow = [&](const char* gxtKey, float percent) {
        CFont::PrintString(labelX, y, TheText.Get(gxtKey));
        CSprite2d::DrawBarChart(
            barX, y,
            (uint16_t)SCREEN_STRETCH_X(100.0f), (uint8_t)SCREEN_STRETCH_Y(8.0f),
            percent, 0, 0, 1,
            CRGBA{ 200, 200, 200, 0xff }, CRGBA{ 0, 0, 0, 0 });
        y += rowH;
    };

    drawStatRow("STAT068", CStats::GetStatValue(STAT_TOTAL_RESPECT) * 0.001f * 100.0f);

    if (!isSwimming && showWeaponSkill) {
        CFont::PrintString(labelX, y, TheText.Get("CURWSKL"));
        const int32_t statIndex = CWeaponInfo::GetSkillStatIndex(activeWeapon);
        // Decomp: progress = clamp(skill-based formula, 0..100). Simplified to
        // the stat value percent; the exact reaction-value formula is a TODO.
        float progress = CStats::GetStatValue((eStats)statIndex);
        if (progress > 999.0f) {
            progress = 100.0f;
        } else {
            progress = progress * 0.1f; // TODO(port): exact StatReactionValue formula
        }
        CSprite2d::DrawBarChart(
            barX, y,
            (uint16_t)SCREEN_STRETCH_X(100.0f), (uint8_t)SCREEN_STRETCH_Y(8.0f),
            progress, 0, 0, 1,
            CRGBA{ 200, 200, 200, 0xff }, CRGBA{ 0, 0, 0, 0 });
        y += rowH;
    } else if (isSwimming) {
        drawStatRow("STAT225", CStats::GetStatValue(STAT_LUNG_CAPACITY) * 0.001f * 100.0f);
    }

    drawStatRow("STAT022", CStats::GetStatValue(STAT_STAMINA) * 0.001f * 100.0f);
    drawStatRow("STAT023", CStats::GetStatValue(STAT_MUSCLE) * 0.001f * 100.0f);
    drawStatRow("STAT021", CStats::GetStatValue(STAT_FAT) * 0.001f * 100.0f);
    drawStatRow("STAT025", CStats::GetStatValue(STAT_SEX_APPEAL) * 0.001f * 100.0f);

    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
    CFont::SetScale(SCREEN_STRETCH_X(0.7f), SCREEN_SCALE_Y(0.7f));
    CFont::SetEdge(1);
    CFont::SetColor({ 200, 200, 200, 0xff });
    CFont::SetDropColor({ 0, 0, 0, 0xff });
    std::snprintf(gString, sizeof(gString), "DAY_%d", CClock::CurrentDay);
    CFont::PrintString(SCREEN_STRETCH_X(160.0f) + 40.0f, y, TheText.Get(gString));
}

// 0x58EAF0
// Converted from src/CHud/DrawPlayerInfo_0058eaf0.c (gta-reversed only stubs it).
//
// The energy-lost / display-score / weapon fade blocks share one state machine:
//   0 = hidden, 1 = shown (timer runs; after 10s -> state 3), 2 = fading in
//   (fadeTimer += step until 1000 -> state 1), 3 = fading out
//   (fadeTimer -= step until 0 -> state 0), 5 = suppressed.
// Decomp labels the step reads CGeneral::unk_00821b40(); the fade-timer sites
// match CTimer::GetTimeStepInMS() (see gta-reversed DrawFadeState).
void CHud::DrawPlayerInfo() {
    CPlayerPed* player = CWorldHudView::Players[CWorldHudView::PlayerInFocus].m_pPed;

    if (m_bDrawClock) {
        CFont::SetBackground(false, false);
        CFont::SetScale(SCREEN_STRETCH_X(0.55f), SCREEN_SCALE_Y(1.1f));
        CFont::SetProportional(false);
        CFont::SetFontStyle(FONT_PRICEDOWN);
        CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
        CFont::SetRightJustifyWrap(0.0f);
        CFont::SetEdge(2);
        CFont::SetDropColor({ 0, 0, 0, 0xff });
        char timeBuf[32];
        std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d",
                      CClock::ms_nGameClockHours, CClock::ms_nGameClockMinutes);
        AsciiToGxtChar(timeBuf, (GxtChar*)gGxtString);
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));
        CFont::PrintString(SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f),
                           SCREEN_SCALE_Y(22.0f),
                           (GxtChar*)gGxtString);
        CFont::SetEdge(0);
    }

    // --- Energy lost (health flash) state machine ---
    {
        uint32_t timer = m_EnergyLostTimer;
        uint32_t fadeTimer = m_EnergyLostFadeTimer;
        uint32_t state = m_EnergyLostState;
        const uint32_t lastEnergyLost =
            CWorldHudView::Players[CWorldHudView::PlayerInFocus].m_nLastTimeEnergyLost;
        const uint32_t savedTimer = timer;

        if (lastEnergyLost == m_LastTimeEnergyLost) {
            if (state != 0 && state != 5) {
                if (state == 1) {
                    fadeTimer = 1000;
                    if (timer > 10000) {
                        state = 3;
                        fadeTimer = 3000;
                    }
                } else if (state == 2) {
                    fadeTimer += HudTimeStepMS();
                    if (fadeTimer > 1000) {
                        fadeTimer = 1000;
                        state = 1;
                    }
                } else if (state == 3) {
                    fadeTimer = fadeTimer > HudTimeStepMS() ? fadeTimer - HudTimeStepMS() : 0;
                    if (fadeTimer == 0) {
                        state = 0;
                    }
                }
                m_EnergyLostTimer = timer + HudTimeStepMS();
            }
        } else {
            // Energy was lost: restart the flash.
            if (state == 0) {
                fadeTimer = 0;
                state = 2;
                timer = 5;
            } else if (state == 1 || state == 3) {
                fadeTimer = 0;
                state = 2;
                timer = 5;
            } else if (state == 0 || state == 5) {
                // no change
            }
            // Decomp duplicates the state-1/2/3 progression here for the
            // just-restarted values; folded into the writeback below.
            m_EnergyLostTimer = timer + HudTimeStepMS();
            (void)savedTimer;
        }
        m_LastTimeEnergyLost = lastEnergyLost;
        m_EnergyLostState = state;
        m_EnergyLostFadeTimer = fadeTimer;

        if (state != 0) {
            const float y = GetYPosBasedOnHealth(
                (uint8_t)CWorldHudView::PlayerInFocus, SCREEN_SCALE_Y(77.0f), '\n');
            // Decomp passes two CGeneral::unk_00821b40() ints as (x, y); the
            // GetYPosBasedOnHealth result above is the intended Y.
            // TODO(port): verify RenderHealthBar x/y against 0x00821b40.
            RenderHealthBar((int32_t)CWorldHudView::PlayerInFocus, 0, (int32_t)y);
        }
    }

    // TODO(port): two-player health/armor/breath blocks (Players[1]) — the
    // decomp gates them on Players[1].m_pPed; single-player PC builds never hit
    // them. Structure preserved below in simplified form.
    {
        const float yArmor = GetYPosBasedOnHealth(
            (uint8_t)CWorldHudView::PlayerInFocus, SCREEN_SCALE_Y(48.0f), '\x03');
        RenderArmorBar((uint32_t)CWorldHudView::PlayerInFocus, 0, (int32_t)yArmor);
    }

    // --- Breath bar ---
    {
        bool showBreath = false;
        // Decomp: true while the swim task is active, while standing in a
        // sinking vehicle, or while breath < fat/muscle modifier and the last
        // breath tick was <500ms ago.
        if (HudIsPlayerSwimming(player)) {
            showBreath = true;
            m_LastBreathTime = CTimer::GetTimeInMS();
        } else if (player->GetPlayerData() &&
                   player->GetPlayerData()->m_fBreath <
                       CStats::GetFatAndMuscleModifier(8) &&
                   CTimer::GetTimeInMS() < m_LastBreathTime + 500) {
            showBreath = true;
            m_LastBreathTime = CTimer::GetTimeInMS();
        }
        if (showBreath) {
            const float y = GetYPosBasedOnHealth(
                (uint8_t)CWorldHudView::PlayerInFocus, SCREEN_SCALE_Y(62.0f), '\x06');
            RenderBreathBar((uint32_t)CWorldHudView::PlayerInFocus, 0, (int32_t)y);
        }
    }

    // --- Display score (money) state machine ---
    {
        uint32_t timer = m_DisplayScoreTimer;
        uint32_t fadeTimer = m_DisplayScoreFadeTimer;
        uint32_t state = m_DisplayScoreState;
        const int32_t displayMoney =
            CWorldHudView::Players[CWorldHudView::PlayerInFocus].m_nDisplayMoney;

        if (m_LastDisplayScore == displayMoney) {
            if (state != 0 && state != 5) {
                if (state == 1) {
                    fadeTimer = 1000;
                    if (timer > 10000) {
                        state = 3;
                        fadeTimer = 3000;
                    }
                } else if (state == 2) {
                    fadeTimer += HudTimeStepMS();
                    if (fadeTimer > 1000) {
                        fadeTimer = 1000;
                        state = 1;
                    }
                } else if (state == 3) {
                    fadeTimer = fadeTimer > HudTimeStepMS() ? fadeTimer - HudTimeStepMS() : 0;
                    if (fadeTimer == 0) {
                        state = 0;
                    }
                }
                m_DisplayScoreTimer = timer + HudTimeStepMS();
                m_DisplayScoreState = state;
                m_DisplayScoreFadeTimer = fadeTimer;
            }
        } else {
            if (state == 0) {
                state = 0;
                fadeTimer = 2;
                timer = 5;
            } else if (state == 1 || state == 3) {
                fadeTimer = 2;
                timer = 5;
            } else if (state == 0 || state == 5) {
                // no change
            }
            // (state progression as above)
            m_DisplayScoreTimer = timer + HudTimeStepMS();
            m_DisplayScoreState = state;
            m_DisplayScoreFadeTimer = fadeTimer;
        }
        m_LastDisplayScore = displayMoney;

        if (state != 0) {
            const uint8_t alpha = (uint8_t)HudTimeStepMS(); // decomp alpha site
            int32_t money = displayMoney;
            const char* fmt;
            if (money < 0) {
                CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_RED, alpha));
                money = -money;
                fmt = "-$%07d";
            } else {
                CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GREEN, alpha));
                fmt = "$%08d";
            }
            char moneyBuf[32];
            std::snprintf(moneyBuf, sizeof(moneyBuf), fmt, money);
            AsciiToGxtChar(moneyBuf, (GxtChar*)gGxtString);
            CFont::SetProportional(false);
            CFont::SetBackground(false, false);
            CFont::SetScale(SCREEN_STRETCH_X(0.55f), SCREEN_SCALE_Y(1.1f));
            CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
            CFont::SetRightJustifyWrap(0.0f);
            CFont::SetFontStyle(FONT_PRICEDOWN);
            CFont::SetDropShadowPosition(0);
            CFont::SetEdge(2);
            CFont::SetDropColor({ 0, 0, 0, alpha });
            const float y = GetYPosBasedOnHealth(
                (uint8_t)CWorldHudView::PlayerInFocus, SCREEN_SCALE_Y(89.0f), '\f');
            CFont::PrintString(SCREEN_WIDTH - SCREEN_STRETCH_X(32.0f), y,
                               (GxtChar*)gGxtString);
            CFont::SetEdge(0);
        }
    }

    // --- Weapon state machine ---
    {
        uint32_t timer = m_WeaponTimer;
        uint32_t fadeTimer = m_WeaponFadeTimer;
        uint32_t state = m_WeaponState;
        const uint32_t activeWeaponType =
            player->m_aWeapons[player->m_nActiveWeaponSlot].m_Type;
        float alpha = 255.0f;

        if (m_LastWeapon == activeWeaponType) {
            if (state != 0 && state != 5) {
                if (state == 1) {
                    fadeTimer = 1000;
                    if (timer > 10000) {
                        state = 3;
                        fadeTimer = 3000;
                    }
                } else if (state == 2) {
                    fadeTimer += HudTimeStepMS();
                    if (fadeTimer > 1000) {
                        fadeTimer = 1000;
                        state = 1;
                    }
                } else if (state == 3) {
                    fadeTimer = fadeTimer > HudTimeStepMS() ? fadeTimer - HudTimeStepMS() : 0;
                    if (fadeTimer == 0) {
                        state = 0;
                    }
                }
                m_WeaponTimer = timer + HudTimeStepMS();
            }
            m_WeaponState = state;
            m_WeaponFadeTimer = fadeTimer;
            // Decomp clamps a float alpha here (extraout); use fadeTimer.
            alpha = (float)fadeTimer * 255.0f / 1000.0f;
        } else {
            if (state == 0) {
                state = 0;
                fadeTimer = 2;
                timer = 5;
            } else if (state == 1 || state == 3) {
                fadeTimer = 2;
                timer = 5;
            }
            m_WeaponTimer = timer + HudTimeStepMS();
            m_LastWeapon = activeWeaponType;
            m_WeaponState = state;
            m_WeaponFadeTimer = fadeTimer;
            alpha = 0.0f;
        }
        if (alpha < 0.0f) alpha = 0.0f;
        if (alpha > 255.0f) alpha = 255.0f;

        if (state != 0) {
            // Decomp passes two unk_00821b40() ints as (x, y) to both calls.
            // TODO(port): verify DrawWeaponIcon/DrawAmmo x/y against 0x00821b40.
            DrawWeaponIcon(player, 0, 0, alpha);
            DrawAmmo(player, 0, 0, alpha);
        }
    }
}

// 0x58D9A0
// Converted from src/CHud/DrawWanted_0058d9a0.c (gta-reversed only stubs it).
void CHud::DrawWanted() {
    CWanted* wanted = FindPlayerWanted(-1);
    const eWantedLevel wantedLevel = wanted->m_WantedLevel;
    const eWantedLevel wantedBeforeParole = wanted->m_WantedLevelBeforeParole;

    // --- Wanted fade state machine (same pattern as DrawPlayerInfo) ---
    uint32_t timer = m_WantedTimer;
    uint32_t fadeTimer = m_WantedFadeTimer;
    uint32_t state = m_WantedState;
    float alpha = 255.0f;

    if (m_LastWanted == (uint32_t)wantedLevel) {
        if (state != 0 && state != 5) {
            if (state == 1) {
                fadeTimer = 1000;
                if (timer > 10000) {
                    state = 3;
                    fadeTimer = 3000;
                }
            } else if (state == 2) {
                fadeTimer += HudTimeStepMS();
                if (fadeTimer > 1000) {
                    fadeTimer = 1000;
                    state = 1;
                }
            } else if (state == 3) {
                fadeTimer = fadeTimer > HudTimeStepMS() ? fadeTimer - HudTimeStepMS() : 0;
                if (fadeTimer == 0)
                    state = 0;
            }
            m_WantedTimer = timer + HudTimeStepMS();
        }
        s_wantedUnchanged = 1;
        m_WantedState = state;
        m_WantedFadeTimer = fadeTimer;
    } else {
        if (state == 0) {
            state = 0;
            fadeTimer = 2;
            timer = 5;
        } else if (state == 1 || state == 3) {
            fadeTimer = 2;
            timer = 5;
        }
        // (state progression as above)
        m_WantedTimer = timer + HudTimeStepMS();
        m_WantedState = state;
        m_WantedFadeTimer = fadeTimer;
        m_LastWanted = (uint32_t)wantedLevel;
        alpha = 0.0f;
        s_wantedUnchanged = 0;
    }
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 255.0f) alpha = 255.0f;

    if (state != 0) {
        CFont::SetBackground(false, false);
        CFont::SetScale(SCREEN_STRETCH_X(0.605f), SCREEN_SCALE_Y(1.21f));
        CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
        CFont::SetProportional(true);
        CFont::SetFontStyle(FONT_GOTHIC);

        char starBuf[4] = "]";
        AsciiToGxtChar(starBuf, (GxtChar*)gGxtString);

        float x = SCREEN_WIDTH - SCREEN_STRETCH_X(29.0f);
        const uint8_t fadeAlpha = (uint8_t)alpha; // decomp alpha site

        if (((int32_t)wantedLevel > 0 && s_wantedUnchanged) || (int32_t)wantedBeforeParole > 0) {
            for (int32_t i = 0; i < 6; i++) {
                CFont::SetEdge(1);
                CFont::SetDropColor({ 0, 0, 0, fadeAlpha });
                CFont::SetScale(SCREEN_STRETCH_X(0.605f), SCREEN_SCALE_Y(1.21f));

                const uint32_t now = CTimer::GetTimeInMS();
                if (i < (int32_t)wantedLevel &&
                    (wanted->m_LastTimeWantedLevelChanged + 2000 < now ||
                     (CTimer::GetFrameCounter() & 4) != 0)) {
                    // Active wanted star (gold, flashes for 2s after change).
                    CFont::SetColor(HudColour.GetRGBA(HUD_COLOUR_GOLD, fadeAlpha));
                } else if (i < (int32_t)wantedBeforeParole &&
                           (CTimer::GetFrameCounter() & 4) != 0) {
                    // Parole "ghost" star, flashing.
                    // Decomp derives RGB from CGeneral::unk_00821b40() calls;
                    // TODO(port): verify flash color.
                    CFont::SetColor({ 0xff, 0xff, 0xff, fadeAlpha });
                } else {
                    // Inactive star slot.
                    CFont::SetEdge(0);
                    CFont::SetColor({ 0, 0, 0, (uint8_t)HudTimeStepMS() });
                    CFont::SetScale(SCREEN_STRETCH_X(0.605f * 1.2f),
                                    SCREEN_SCALE_Y(1.21f * 1.2f));
                }

                float y = GetYPosBasedOnHealth(
                    (uint8_t)CWorldHudView::PlayerInFocus, SCREEN_SCALE_Y(114.0f), '\f');
                if (i >= (int32_t)wantedLevel) {
                    // Inactive slots sit two rows lower (decomp).
                    y -= SCREEN_SCALE_Y(2.0f);
                } else if ((float)FindPlayerInfo().m_nMaxHealth < 101.0f) {
                    y -= SCREEN_SCALE_Y(12.0f);
                }
                CFont::PrintString(x, y, (GxtChar*)gGxtString);
                x -= SCREEN_STRETCH_X(18.0f);
            }
            CFont::SetEdge(0);
        }
    }
}

// 0x58B6E0
// Converted from src/CHud/DrawHelpText_0058b6e0.c (gta-reversed only stubs it).
void CHud::DrawHelpText() {
    if (m_pHelpMessage[0] == 0) {
        m_nHelpMessageState = 0;
        return;
    }

    if (!CMessages::StringCompare(m_pHelpMessage, m_pLastHelpMessage, 400)) {
        switch (m_nHelpMessageState) {
        case 0:
            m_nHelpMessageState = 2;
            m_nHelpMessageTimer = 0;
            m_nHelpMessageFadeTimer = 0;
            CMessages::StringCopy(m_pHelpMessageToPrint, m_pHelpMessage, 400);
            CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
            CFont::SetJustify(false);
            CFont::SetWrapx(SCREEN_STRETCH_X(200.0f + 34.0f - 4.0f));
            CFont::SetFontStyle(FONT_SUBTITLES);
            CFont::SetBackground(true, true);
            CFont::SetDropShadowPosition(0);
            {
                const int16_t lines = CFont::GetNumberLines(
                    SCREEN_STRETCH_X(34.0f), SCREEN_SCALE_Y(28.0f),
                    m_pHelpMessageToPrint);
                m_fHelpMessageTime = (float)(lines + 3);
            }
            CFont::SetWrapx(SCREEN_WIDTH);
            // AudioEngine.ReportFrontendAudioEvent(AE_FRONTEND_DISPLAY_INFO)
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            m_nHelpMessageState = 4;
            m_nHelpMessageTimer = 5;
            break;
        }
        CMessages::StringCopy(m_pLastHelpMessage, m_pHelpMessage, 400);
    }

    float alphaFade = 200.0f;
    if (m_nHelpMessageState == 0) {
        return;
    }
    switch (m_nHelpMessageState) {
    case 1:
        alphaFade = 200.0f;
        m_nHelpMessageFadeTimer = 600;
        if (!m_bHelpMessagePermanent &&
            (m_fHelpMessageTime * 1000.0f < (float)m_nHelpMessageTimer ||
             (m_bHelpMessageQuick && 3000.0f < (float)m_nHelpMessageTimer))) {
            m_nHelpMessageState = 3;
            m_nHelpMessageFadeTimer = 600;
        }
        break;
    case 2:
        if (s_cutsceneSubtitleMode == 0) {
            m_nHelpMessageFadeTimer += (int32_t)HudTimeStepMS() * 2;
            if (m_nHelpMessageFadeTimer > 0) {
                m_nHelpMessageFadeTimer = 0;
                m_nHelpMessageState = 1;
            }
            goto fade_computed;
        }
        break;
    case 3:
        m_nHelpMessageFadeTimer -= (int32_t)HudTimeStepMS() * 2;
        if (m_nHelpMessageFadeTimer < 0 || s_cutsceneSubtitleMode != 0) {
            m_nHelpMessageFadeTimer = 0;
            m_nHelpMessageState = 0;
        }
        goto fade_computed;
    case 4:
        m_nHelpMessageFadeTimer -= (int32_t)HudTimeStepMS() * 2;
        if (m_nHelpMessageFadeTimer < 0) {
            m_nHelpMessageFadeTimer = 0;
            m_nHelpMessageState = 2;
            CMessages::StringCopy(m_pHelpMessageToPrint, m_pLastHelpMessage, 400);
        }
    fade_computed:
        alphaFade = (float)m_nHelpMessageFadeTimer * 0.001f * 200.0f;
        break;
    }

    if (CCutsceneMgr::ms_running != 0) {
        return;
    }
    m_nHelpMessageTimer += HudTimeStepMS();

    CFont::SetAlphaFade(alphaFade);
    CFont::SetProportional(true);
    CFont::SetScaleForCurrentLanguage(SCREEN_STRETCH_X(0.52f), SCREEN_SCALE_Y(1.1f));

    float textX, textY;
    if (m_nHelpMessageStatId == 0) {
        // Regular help text box.
        if (m_BigMessage[0][0] != 0 || m_BigMessage[4][0] != 0 ||
            CGarages::MessageIDString[0] != 0) {
            goto done;
        }
        CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
        CFont::SetJustify(false);
        float wrapX;
        if (m_fHelpMessageBoxWidth == 200.0f) {
            wrapX = SCREEN_STRETCH_X(34.0f + 200.0f - 4.0f);
        } else {
            wrapX = SCREEN_STRETCH_X(34.0f) + (m_fHelpMessageBoxWidth - 4.0f) * SCREEN_STRETCH_X(1.0f);
        }
        CFont::SetWrapx(wrapX);
        CFont::SetFontStyle(FONT_SUBTITLES);
        CFont::SetBackground(true, true);
        CFont::SetDropShadowPosition(0);
        CFont::SetBackgroundColor({ 0, 0, 0, (uint8_t)HudTimeStepMS() });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));

        int32_t yOff = 0x96;
        if (s_cutsceneSubtitleMode != 0 && !s_subtitleSkipFlag) {
            yOff += 0x38;
        }
        textX = SCREEN_STRETCH_X(34.0f);
        textY = SCREEN_SCALE_Y(28.0f) + (float)(yOff - PagerXOffset) * 0.6f * SCREEN_SCALE_Y(1.0f);
    } else {
        // Stat update display with progress bar.
        if (s_cutsceneSubtitleMode != 0) {
            goto done;
        }
        char statKey[16];
        if (m_nHelpMessageStatId < 10) {
            std::snprintf(statKey, sizeof(statKey), "STAT00%d", m_nHelpMessageStatId);
        } else if (m_nHelpMessageStatId < 100) {
            std::snprintf(statKey, sizeof(statKey), "STAT0%d", m_nHelpMessageStatId);
        } else {
            std::snprintf(statKey, sizeof(statKey), "STAT%d", m_nHelpMessageStatId);
        }
        std::snprintf(gString, sizeof(gString), "%s", statKey);
        CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
        CFont::SetJustify(false);
        CFont::SetWrapx(SCREEN_WIDTH);
        CFont::SetFontStyle(FONT_SUBTITLES);
        CFont::SetBackground(true, true);
        CFont::SetDropShadowPosition(0);
        const uint8_t bgAlpha = (uint8_t)HudTimeStepMS();
        CFont::SetBackgroundColor({ 0, 0, 0, bgAlpha });
        CFont::SetColor(HudColour.GetRGB(HUD_COLOUR_LIGHT_GRAY));

        const GxtChar* statName = TheText.Get(gString);
        const float nameWidth = CFont::GetStringWidth(statName, true, false);
        const float barX = SCREEN_STRETCH_X(10.0f + 34.0f) + nameWidth;
        CFont::SetWrapx(SCREEN_STRETCH_X(75.0f) + barX);
        CFont::PrintString(SCREEN_STRETCH_X(34.0f),
                           SCREEN_SCALE_Y(28.0f) + (150.0f - (float)PagerXOffset) * 0.6f * SCREEN_SCALE_Y(1.0f),
                           statName);

        // Stat value (group member count for stat 0x150, else CStats).
        float statValue;
        if (m_nHelpMessageStatId == 0x150) {
            // TODO(port): CPedGroups member count
            statValue = 0.0f;
        } else {
            statValue = CStats::GetStatValue(m_nHelpMessageStatId);
        }

        AsciiToGxtChar("+", (GxtChar*)gGxtString);
        const bool increasing = (m_pHelpMessageToPrint[0] == ((GxtChar*)gGxtString)[0]);
        CRGBA barColor, bgColor;
        if (increasing) {
            barColor = HudColour.GetRGBA(HUD_COLOUR_GREEN, bgAlpha);
            bgColor = HudColour.GetRGBA(HUD_COLOUR_LIGHT_GRAY, bgAlpha);
        } else {
            barColor = HudColour.GetRGBA(HUD_COLOUR_RED, bgAlpha);
            bgColor = HudColour.GetRGBA(HUD_COLOUR_LIGHT_GRAY, bgAlpha);
        }

        const float maxVal = (float)m_nHelpMessageMaxStatValue;
        // Decomp clamps via CVector2D::unk_00420800; progressAdd/width/height
        // come from CGeneral::unk_00821b40() (TODO(port): identify).
        const float progress = std::clamp(statValue / maxVal * 100.0f, 0.0f, 100.0f);
        const float barY = SCREEN_SCALE_Y(28.0f) + (155.0f - (float)PagerXOffset) * 0.6f * SCREEN_SCALE_Y(1.0f);
        CSprite2d::DrawBarChart(barX, barY,
                                (uint16_t)SCREEN_STRETCH_X(100.0f),
                                (uint8_t)SCREEN_STRETCH_Y(8.0f),
                                progress, 0, 0, 0, bgColor, barColor);

        textX = barX + SCREEN_STRETCH_X(65.0f);
        textY = SCREEN_SCALE_Y(28.0f) + (150.0f - (float)PagerXOffset) * 0.6f * SCREEN_SCALE_Y(1.0f);
    }

    CFont::PrintString(textX, textY, m_pHelpMessageToPrint);
    CFont::SetWrapx(SCREEN_WIDTH);
done:
    CFont::SetAlphaFade(255.0f);
}

// 0x58E020
// Converted from src/CHud/DrawCrossHairs_0058e020.c (gta-reversed only stubs it).
//
// Camera-mode values (DAT_00b6f1a8[DAT_00b6f081*0x11c] is the active camera's
// m_nMode): 7=SNIPER, 0x10=FOLLOW_PED, 8=SNIPER_RUNABOUT, 0x33/_=various,
// 0x35/0x37/0x41=aim/weapon modes, 0x2a/0x28/0x34/0x27=3rd-person aim.
// TODO(port): verify against eCamMode when CCamera is fully ported.
void CHud::DrawCrossHairs() {
    CPlayerPed* player = CWorldHudView::Players[CWorldHudView::PlayerInFocus].m_pPed;
    if (!player) {
        return;
    }

    // TODO(port): active camera mode read (CCamera::GetActiveCam()->m_nMode).
    // Decomp reads it from DAT_00b6f1a8; using a placeholder.
    const int32_t camMode = 0; // TODO(port): real camera mode

    bool drawFixed = false;   // bVar8: 1st-person/scope crosshair
    bool drawThirdPerson = false; // bVar4: 3rd-person expanding reticle

    if (camMode == 7 || camMode == 0x10 || camMode == 8 || camMode == 0x33 ||
        camMode == 0x22 || camMode == 0x2d || camMode == 0x2e) {
        // TODO(port): Rhino/Hunter vehicle check (FindPlayerVehicle model 0x208/0x1a9)
        // TODO(port): melee weapon check (CWeapon::IsTypeMelee)
        drawFixed = true;
    }
    if (camMode == 0x2a || camMode == 0x28 || camMode == 0x34 || camMode == 0x27) {
        drawThirdPerson = true;
    }

    // TODO(port): targeted-object / gun-task / ped-state / weapon-range checks
    // for the aim reticle (needs CPedIntelligence, CTaskSimpleUseGun).

    if (!drawFixed && !drawThirdPerson /* && CTheScripts::bDrawCrossHair == NONE */) {
        return;
    }

    // TODO(port): RwEngineInstance render-state setup.
    RwRenderStateSet(RWSTATE_ZTESTENABLE, 0);
    RwRenderStateSet(RWSTATE_ZWRITEENABLE, 0);

    if (drawThirdPerson && (camMode == 0x35 || camMode == 0x37 || camMode == 0x41)) {
        // 3rd-person expanding reticle.
        const float cx = (float)RsGlobal.maximumWidth * 0.5f;  // TODO: CCamera::m_f3rdPersonCHairMultX
        const float cy = (float)RsGlobal.maximumHeight * 0.5f; // TODO: CCamera::m_f3rdPersonCHairMultY
        const float radius = 0.2f; // TODO(port): CPlayerPed::GetWeaponRadiusOnScreen
        if (radius == 0.2f) {
            CRect dot{ cx - 1.0f, cy - 1.0f, cx + 1.0f, cy + 1.0f };
            CSprite2d::DrawRect(dot, { 0xff, 0xff, 0xff, 0xff });
        }
        const float w = SCREEN_STRETCH_X(64.0f) * radius;
        const float h = SCREEN_SCALE_Y(64.0f) * radius;
        const float x0 = cx - w * 0.5f;
        const float y0 = cy - h * 0.5f;
        // TODO(port): four Sprites[1] reticle quads.
        (void)x0; (void)y0; (void)w; (void)h;
    } else {
        // 1st-person crosshair / sniper scope.
        // TODO(port): weapon-specific scope rendering (CWeaponInfo, CTxdStore).
        const float cx = SCREEN_STRETCH_X(320.0f);
        const float cy = SCREEN_SCALE_Y(224.0f);
        CRect cross{ cx - 16.0f, cy - 1.0f, cx + 16.0f, cy + 1.0f };
        CSprite2d::DrawRect(cross, { 0xff, 0xff, 0xff, 0xff });
        cross = CRect{ cx - 1.0f, cy - 16.0f, cx + 1.0f, cy + 16.0f };
        CSprite2d::DrawRect(cross, { 0xff, 0xff, 0xff, 0xff });
    }
}
