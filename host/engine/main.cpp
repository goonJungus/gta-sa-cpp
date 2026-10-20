// gtasa_cpp.exe - PLAYABLE BUILD
// Grove Street with GTA SA-style controls (WASD + mouse), debug overlay.
//
// Modes:
//   (no flag)     The game (WASD + mouse, debug info)
//   --flyover     Scripted camera flyover
//
// Play controls:
//   W/A/S/D - move (camera-relative)
//   Mouse   - look around
//   Space   - sprint
//   Shift   - jump
//   ESC / P - pause menu (Options > Display Setup > Advanced: frame limiter,
//             VSync, draw distance; Quit Game exits)
//
// Options:
//   --game DIR    GTA:SA install folder (else $GTA_SA_DIR, the ini, the exe's
//                 folder, common install paths, or a folder picker)
//   --res WxH     window/backbuffer size (default 1280x720)
//   --menu N      open the pause menu on SA screen N (headless menu captures)
// Display settings persist in gtasa_cpp.ini next to the exe.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <cstring>
#include "D3DRenderer.h"
#include "DffLoader.h"
#include "TxdLoader.h"
#include "ImgLoader.h"
#include "Cheat.h"
#include "IdeLoader.h"
#include "IplLoader.h"
#include "BinaryIpl.h"
#include "ColLoader.h"
#include "World.h"
#include "TimeCycle.h"
#include "AuditCollision.h"
#include "Profiler.h"
#include "FeMenu.h"
#include "Hud.h"
#include "Vehicle.h"
#include "Script.h"
#include "ScriptGame.h"
#include "Radio.h"
#include "Peds.h"
#include "Projectiles.h"
#include "BulletInfo.h"
#include "ShotInfo.h"
#include "Pickups.h"
#include "EntryExit.h"
#include "WeaponInfo.h"
#include "Speech.h"
#include "Delirium.h"
#include "PlayerPed.h"
#include "Camera.h"
#include "CameraShake.h"
#include "Water.h"
#include "Sky.h"
#include "ClimbScan.h"
#include "Audio.h"
#include "Effects.h"
#include "AimTest.h"
#include "Gamepad.h"
#include "Fire.h"
#include "Explosion.h"
#include "Stats.h"
#include "VehicleGuns.h"
#include "WaterCannon.h"

static D3DRenderer g_renderer;
static EntryExit g_enex;     // CEntryExitManager: the doors to the interiors
static Pickups g_pickups;   // CPickups: 620 slots
static WaterCannons g_waterCannons;   // CWaterCannons
static std::vector<std::array<float, 3>> g_tankBooms;   // rhino shells that landed this frame
static int g_wanted = getenv("GTA_WANTED") ? atoi(getenv("GTA_WANTED")) : 0;
static int g_collectBuf = 0;   // CPickups::CollectPickupBuffer
static ShotInfo g_shots;   // flamethrower / spray can / extinguisher shots (CShotInfo)
static FireManager::World g_fireWorld;   // the world hooks the fire manager runs with, kept for the shots
static BulletInfo g_bullets;   // scoped sniper bullets in flight (CBulletInfo)
static Projectiles g_proj;   // thrown weapons in flight
static FireManager g_fires;          // CFireManager
static ExplosionManager g_expl;      // CExplosion
static std::vector<std::array<float, 2>> g_shakeQ;   // camera shakes waiting for the shake clock: strength, distance
static std::vector<std::array<float, 3>> g_carBooms;   // wrecked cars that blew up this frame
static std::vector<std::array<float, 3>> g_dynBooms;   // explosive props that went off this frame
static PlayerPed g_cj;   // CJ: skinned player model + animations (replaces the box)
static Water g_water;    // CWaterLevel: water.dat surfaces + the open sea
static Sky g_sky;        // CClouds: moon, stars, low clouds
static TimeCycleHour g_tcNow;  // this frame's timecyc sample
static bool g_startThirdPerson = false;   // --third
static CameraShake g_shake;
PlayerCamera g_playerCam;          // on-foot camera views (V)
static bool g_hideCjHead = false;         // first-person view
static float g_nearClip = 0.5f;
static float g_fovDeg = 70.0f;           // vertical FOV in use (rewrite can change it)
static int  g_forceStyle = -1;            // --classic / --rewrite (not saved)

// Per-frame hitch profiler (shown in the F1 overlay; R resets the worst frame).
// A frame is logged as a HITCH when it is over kHitchMs and >2x the average.
static FrameProfiler g_prof;
static const double kHitchMs = 25.0;

// Data-driven world (gta.dat IDE + text/binary IPLs + real COL) replaces the
// hand-placed Grove Street scene when it initializes successfully.
static GameWorld g_world;
static bool worldReady = false;
static unsigned g_solidBlockCount = 0;

// ---- Dev/test controls (play mode keys) ---------------------------------
// F1 overlay   F2 wireframe   F3 collision view   F4 noclip   F5 screenshot
// F6 time flow  F7/F8 hour -/+  F9 next weather  F10 world lighting  P pause menu  V 1st/3rd
static bool g_devOverlay   = true;
static bool g_devWireframe = false;
static bool g_devCollision = true;     // response is always on; F3 shows the volumes
static bool g_devShowCollision = false; // F3: pink wireframe COL view
static bool g_devNoClip    = false;
static bool g_devPaused    = false;
static bool g_devLighting  = true;
static int  g_devShotIdx   = 0;
static bool g_autoWalk     = false;    // --walk: force-walk forward + log pos (headless collision probe)
static float g_stamina     = 1.0f;     // sprint stamina 0..1
static float g_timeCanRun  = 0.0f;     // CPlayerData::m_fTimeCanRun
static bool  g_runInit     = false;
static int  g_autoGait     = 0;        // --gait: 0 run, 1 walk, 2 sprint, 3 run + keep jumping (with --walk)
static bool g_probeCollision = false;  // --probe-collision: dump a solidity report at --pos after streaming
static bool g_keyState[256] = { false };
// Window focus (WM_ACTIVATE). GetAsyncKeyState reads the GLOBAL keyboard, so
// every game key goes through GameKey: typing in another window must not move
// the player or fire dev keys.
static volatile bool g_hasFocus = true;
// Dev (headless tests): GTA_KEYS="from-to:KEYS;..." holds keys over frame
// ranges, e.g. "0-120:W;120-200:WA;200-260:W+;260:_" (+ = Shift, ^ = Alt,
// _ = Space, # = F4, other letters = their key).
static int g_scriptFrame = 0;
static bool ScriptedKey(int vk) {
    static const char* spec = getenv("GTA_KEYS");
    if (!spec) return false;
    for (const char* p = spec; *p;) {
        int a = atoi(p), b = a;
        while (*p && *p != '-' && *p != ':') p++;
        if (*p == '-') { b = atoi(++p); while (*p && *p != ':') p++; }
        if (*p != ':') break;
        const char* keys = ++p;
        while (*p && *p != ';') p++;
        if (g_scriptFrame >= a && g_scriptFrame <= b)
            for (const char* k = keys; k < p; k++) {
                const int want = *k == '+' ? VK_SHIFT : *k == '^' ? VK_MENU : *k == '_' ? VK_SPACE : *k == '#' ? VK_F4 : *k == '`' ? VK_OEM_3 : *k == '~' ? VK_RBUTTON : *k == '|' ? VK_LBUTTON
                               : toupper((unsigned char)*k);
                if (want == vk) return true;
            }
        if (*p == ';') p++;
    }
    return false;
}
static Gamepad g_pad;
static bool g_wheelOpen = false;   // the weapon wheel is up
static bool g_freeCam = false;     // dev free camera (` key): CJ stands still, the camera flies
static float g_fcPos[3] = {0, 0, 0};
static float g_wheelX = 0.0f, g_wheelY = 0.0f;   // pick vector on the wheel (screen axes, length 0..1)
static int g_wheelSel = 0;         // highlighted slot (0 = fists)
static int g_wheelCat = 0;         // highlighted GTA V style category 0..7
static int g_wheelPick[8];         // the item shown in each category (-1 empty)
// weapon.dat slot -> wheel category: melee, handguns, SMGs, shotguns, assault, sniper, heavy, thrown and misc
static int WheelCatOfSlot(int slot) {
    switch (slot) { case 0: case 1: case 10: return 0; case 2: return 1; case 4: return 2; case 3: return 3; case 5: return 4; case 6: return 5; case 7: return 6; default: return 7; }
}
static int WheelItemCat(int item) { return item <= 0 ? 0 : WheelCatOfSlot(g_cj.GunSlotAt(item - 1)); }
static bool WheelItemOwned(int item) { return item <= 0 || g_cj.GunOwnedAt(item - 1); }
// A controller drives the same virtual keys the keyboard does: A sprint, X jump, B crouch, Y exit / use,
// RB reload, LB weapon wheel, triggers aim (left) and fire (right).
static bool PadKey(int vk) {
    if (!g_pad.Connected()) return false;
    switch (vk) {
    case VK_SPACE:   return g_pad.Down(Gamepad::A);
    case VK_SHIFT:   return g_pad.Down(Gamepad::X);
    case 'C':        return g_pad.Down(Gamepad::B);
    case 'F':        return g_pad.Down(Gamepad::Y);
    case 'R':        return g_pad.Down(Gamepad::RB);
    case 'Q':        return g_pad.Down(Gamepad::LB);
    case VK_LBUTTON: return g_pad.RT() > 0.35f;
    case VK_RBUTTON: return g_pad.LT() > 0.35f;
    case VK_MENU:    return g_pad.Down(Gamepad::LTHUMB);
    }
    return false;
}
// Mouse buttons and pointer position tracked from window messages: under Wine on
// Wayland GetAsyncKeyState misses some buttons and GetCursorPos can be stale.
static volatile bool g_msgBtn[3] = { false, false, false };   // left, right, middle
static volatile long g_msgMouseX = 0, g_msgMouseY = 0;        // client coords, last WM_MOUSEMOVE
static volatile bool g_msgMouseSeen = false;
static volatile long g_dbgRaw = 0, g_dbgMove = 0;   // MOUSE: log line counters
static bool g_freeHave = false;                      // free-pointer look has a previous position
static bool g_cursorHidden = false;                  // ShowCursor(FALSE) is in effect
static SHORT RawGameKey(int vk) {
    if (ScriptedKey(vk)) return (SHORT)0x8000;
    if (g_hasFocus && PadKey(vk)) return (SHORT)0x8000;
    if (g_hasFocus && (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) &&
        g_msgBtn[vk == VK_LBUTTON ? 0 : vk == VK_RBUTTON ? 1 : 2]) return (SHORT)0x8000;
    return g_hasFocus ? GetAsyncKeyState(vk) : 0;
}
static int g_driveCar = -1;   // index of the car CJ is driving, -1 on foot
static SHORT GameKey(int vk) {
    // While driving the pedestrian controls are muted; the car reads the raw keys.
    if (g_driveCar >= 0 && (vk == 'W' || vk == 'A' || vk == 'S' || vk == 'D' || vk == VK_SPACE || vk == VK_SHIFT ||
                            vk == 'C' || vk == 'F' || vk == 'Q' || vk == 'E' || vk == VK_RETURN)) return 0;
    return RawGameKey(vk);
}
// Edge-triggered key read: true only on the frame the key goes down.
static bool DevKeyPressed(int vk) {
    if (vk < 0 || vk > 255) return false;
    bool down = (GameKey(vk) & 0x8000) != 0;
    bool was = g_keyState[vk];
    g_keyState[vk] = down;
    return down && !was;
}

// Time of day (data/timecyc.dat): drives sky gradient, sun, fog and draw
// distance. Retail advances the clock at 1 game minute per real second.
static TimeCycle g_timecyc;
static bool g_timecycOk = false;
static size_t g_weatherIdx = 0;
static float g_gameSeconds = 12.0f * 3600.0f;
static int   g_gameDay     = 0;         // days elapsed (moon phase)
// Weather: each region of the map has its own forecast; the change crosses over ~20 game minutes.
static bool   g_weatherAuto  = true;
static size_t g_weatherPrev  = 0;
static float  g_weatherBlend = 1.0f;    // 0 = still the previous weather, 1 = arrived
static float  g_weatherTimer = 0.0f;    // game hours until the next forecast change
static int    g_weatherRegion = -1;
static uint32_t g_weatherRng = 12345u;
static int WeatherRegionAt(float x, float y) {   // 0 LA, 1 SF, 2 Las Venturas, 3 desert, 4 countryside
    if (x > 800.0f && y > 400.0f) return 2;
    if (x > 900.0f && y < -300.0f) return 0;
    if (x < -1200.0f && x > -3000.0f && y > -1000.0f && y < 1800.0f) return 1;
    if (y > 400.0f && x > -1700.0f && x < 800.0f) return 3;
    return 4;
}
static bool g_timeFlow = true;
static DWORD g_fogColor = 0xFF35A2E3;  // = timecyc SkyBot (horizon)
static float g_waterRGBA[4] = {90, 110, 120, 240};  // timecyc Water, refreshed every frame
static bool  g_fogEnable = true;        // timecyc decides (clear weather = no band)
// World lighting: last timecyc ambient (0..255, incl. the night lift) and the
// sun visibility 0..1 that scales the directional light. Overlay only.
static float g_lightAmb[3] = {0.0f, 0.0f, 0.0f};
static float g_lightSunFade = 0.0f;
// Moonlight/haze floor added to the timecyc ambient at night, in 0..255 units
// (0.22 of full brightness). Retail gets its night look from the DFF night
// prelit vertex colours, which we do not carry into the mesh yet; without this
// the city would render almost black after dusk.
static const float kNightLightLift = 56.0f;
static bool g_running = true;

// SA pause menu (FeMenu) + its persisted display settings.
static FeMenu g_menu;
static Hud g_hud;
static Audio g_audio;
static Radio g_radio;
static int g_radioStation = 0;   // the station that plays while driving
static Effects g_fx;
static bool g_scoped = false;     // looking through a sniper / camera scope
static float g_scopeFov = 70.0f;  // its zoom
static float g_bulletMs = 0.0f;   // game clock for the sniper bullets
static float g_camFlash = 0.0f;   // the camera flash wash, fading
static int g_photos = 0;          // photographs taken
static bool g_aimTest = false;      // --aimtest: the shooting range
static AimTest g_aim;
static PlayerPed g_dummy;           // the pose every range dummy shares
static float g_aimEye[3] = {0, 0, 0}, g_aimDir[3] = {0, 1, 0};   // the camera ray bullets follow
// CCamera::m_f3rdPersonCHairMultX / Y: the classic third-person crosshair sits right of and above
// the screen centre; the rewrite crosshair is centred.
static float g_chairX = 0.5f, g_chairY = 0.5f;
static float g_aspect = 16.0f / 9.0f;   // render width / height
static float g_camFovScale = 1.0f;
static int g_stealthPed = -1;            // the ped a knife takedown would hit
static int g_lockPed = -1;              // locked-on pedestrian (CPlayerPed::m_pTargetedObject), -1 none
static float g_lockPos[3] = {0, 0, 0};  // its aim point
static D3DMATRIX g_lastView = {};
static std::vector<RadarBlip> g_blips;  // CRadar::ms_RadarTrace
static bool g_cutHideCj = false;   // CJ is hidden while a cutscene plays
static ScriptGame g_sg;          // the mission script's game commands
static int g_nextBlip = 1;       // the frame's view matrix (lock-on marker projection)      // aim camera zoom: tan(fov / 2) scale
// CCamera::Find3rdPersonCamTargetVector: the ray from the camera through the crosshair.
static void CrosshairRay(float yaw, float pitch, float out[3]) {
    const float cp = cosf(pitch), sp = sinf(pitch);
    const float f[3] = {cosf(yaw) * cp, sinf(yaw) * cp, sp};
    const float r[3] = {sinf(yaw), -cosf(yaw), 0.0f};                       // f x up
    const float u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};
    const float tanV = tanf(g_fovDeg * 3.14159265f / 360.0f);
    const float cu = tanV * (0.5f - g_chairY) * 2.0f, cr = tanV * g_aspect * (g_chairX - 0.5f) * 2.0f;
    float d[3];
    for (int k = 0; k < 3; k++) d[k] = f[k] + u[k] * cu + r[k] * cr;
    const float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (int k = 0; k < 3; k++) out[k] = l > 1e-6f ? d[k] / l : f[k];
}
static bool g_hudReady = false;
static bool g_menuReady = false;
static bool g_escOpensMenu = false;      // play mode (not the capture modes)
static int  g_money = getenv("GTA_MONEY") ? atoi(getenv("GTA_MONEY")) : 0;
static int  g_keyQueue[64];              // WM_KEYDOWN -> main loop (menu input)
static int  g_keyQueueN = 0;
static int  g_wheelSteps = 0;   // mouse wheel notches since the last frame
static char g_iniPath[MAX_PATH] = {};    // <exe_dir>\gtasa_cpp.ini

static PedClothesDesc g_storedOutfit;   // STORE_CLOTHES_STATE

// [Outfit] in the ini: the worn clothes as player.img file stems, M0..M9 per model slot and T0..T17 per texture part.
static void SaveOutfit(const PedClothesDesc& d) {
    char k[8];
    for (int i = 0; i < CLOTHES_MODEL_TOTAL; i++) {
        std::snprintf(k, sizeof k, "M%d", i);
        const std::string& n = Clothes::Name(d.modelKeys[i]);
        WritePrivateProfileStringA("Outfit", k, d.modelKeys[i] && !n.empty() ? n.c_str() : nullptr, g_iniPath);
    }
    for (int i = 0; i < CLOTHES_TEXTURE_TOTAL; i++) {
        std::snprintf(k, sizeof k, "T%d", i);
        const std::string& n = Clothes::Name(d.textureKeys[i]);
        WritePrivateProfileStringA("Outfit", k, d.textureKeys[i] && !n.empty() ? n.c_str() : nullptr, g_iniPath);
    }
}
static bool LoadOutfit(PedClothesDesc& d) {
    bool any = false;
    char k[8], v[64];
    for (int i = 0; i < CLOTHES_MODEL_TOTAL; i++) {
        std::snprintf(k, sizeof k, "M%d", i);
        if (GetPrivateProfileStringA("Outfit", k, "", v, sizeof v, g_iniPath) && v[0]) { d.SetModel(v, (ClothesModelPart)i); any = true; }
    }
    for (int i = 0; i < CLOTHES_TEXTURE_TOTAL; i++) {
        std::snprintf(k, sizeof k, "T%d", i);
        if (GetPrivateProfileStringA("Outfit", k, "", v, sizeof v, g_iniPath) && v[0]) { d.textureKeys[i] = Clothes::Key(v); any = true; }
    }
    return any;
}


static void LoadSettings(FeSettings& s) {
    s.frameLimit = (int)GetPrivateProfileIntA("Display", "FrameLimit", 0, g_iniPath);
    if (s.frameLimit != 60 && s.frameLimit != 120 && s.frameLimit != 144) s.frameLimit = 0;
    s.vsync = GetPrivateProfileIntA("Display", "VSync", 0, g_iniPath) != 0;
    char buf[32] = {};
    GetPrivateProfileStringA("Display", "DrawDistance", "1.1", buf, sizeof(buf), g_iniPath);
    float dd = (float)atof(buf);
    s.drawDist = dd < 0.925f ? 0.925f : (dd > 1.8f ? 1.8f : dd);
    s.movementStyle = GetPrivateProfileIntA("Gameplay", "MovementStyle", 0, g_iniPath) == 1 ? 1 : 0;
    s.cameraStyle = GetPrivateProfileIntA("Gameplay", "CameraStyle", 0, g_iniPath) == 1 ? 1 : 0;
    auto flt = [&](const char* sec, const char* key, float def, float lo, float hi) {
        char d[32], b[32] = {};
        snprintf(d, sizeof(d), "%g", def);
        GetPrivateProfileStringA(sec, key, d, b, sizeof(b), g_iniPath);
        float v = (float)atof(b);
        return v < lo ? lo : (v > hi ? hi : v);
    };
    auto flag = [&](const char* sec, const char* key, bool def) {
        return GetPrivateProfileIntA(sec, key, def ? 1 : 0, g_iniPath) != 0;
    };
    s.fov = flt("Display", "FieldOfView", 70.0f, 50.0f, 110.0f);
    s.hudScale = flt("Hud", "Scale", 1.0f, 0.6f, 1.6f);
    s.showHud = flag("Hud", "Show", true);
    s.radarMode = (int)GetPrivateProfileIntA("Hud", "RadarMode", 0, g_iniPath) == 1 ? 1 : 0;
    s.masterVol = flt("Audio", "Master", 0.8f, 0.0f, 1.0f);
    s.sfxVol = flt("Audio", "Sfx", 1.0f, 0.0f, 1.0f);
    s.musicVol = flt("Audio", "Music", 0.7f, 0.0f, 1.0f);
    s.hitMarker = flag("Rewrite", "HitMarker", true);
    s.damageFlash = flag("Rewrite", "DamageFlash", true);
    s.crosshairDot = flag("Rewrite", "CrosshairDot", true);
    s.autoReload = flag("Rewrite", "AutoReload", true);
    s.mouseSens = flt("Controls", "MouseSensitivity", 1.0f, 0.25f, 3.0f);
    s.invertY = flag("Controls", "InvertY", false);
    s.windowSize = (int)GetPrivateProfileIntA("Display", "WindowSize", 1, g_iniPath);
    s.language = (int)GetPrivateProfileIntA("Display", "Language", 0, g_iniPath);
    s.mipmaps = GetPrivateProfileIntA("Display", "MipMaps", 1, g_iniPath) != 0;
    s.mapLegend = GetPrivateProfileIntA("Display", "MapLegend", 0, g_iniPath) != 0;
    s.antiAlias = std::clamp((int)GetPrivateProfileIntA("Display", "AntiAlias", 0, g_iniPath), 0, 2);
    {
        char bb[32];
        GetPrivateProfileStringA("Display", "Brightness", "0.5", bb, sizeof(bb), g_iniPath);
        s.brightness = std::clamp((float)atof(bb), 0.0f, 1.0f);
    }
    if (s.language < 0 || s.language >= kLanguageCount) s.language = 0;
    if (s.windowSize < 0 || s.windowSize >= kWindowSizeCount) s.windowSize = 1;
}
static void SaveSettings(const FeSettings& s) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", s.frameLimit);
    WritePrivateProfileStringA("Display", "FrameLimit", buf, g_iniPath);
    WritePrivateProfileStringA("Display", "VSync", s.vsync ? "1" : "0", g_iniPath);
    snprintf(buf, sizeof(buf), "%.4f", s.drawDist);
    WritePrivateProfileStringA("Display", "DrawDistance", buf, g_iniPath);
    WritePrivateProfileStringA("Gameplay", "MovementStyle", s.movementStyle ? "1" : "0", g_iniPath);
    WritePrivateProfileStringA("Gameplay", "CameraStyle", s.cameraStyle ? "1" : "0", g_iniPath);
    auto flt = [&](const char* sec, const char* key, float v) {
        snprintf(buf, sizeof(buf), "%.4f", v);
        WritePrivateProfileStringA(sec, key, buf, g_iniPath);
    };
    auto flag = [&](const char* sec, const char* key, bool v) {
        WritePrivateProfileStringA(sec, key, v ? "1" : "0", g_iniPath);
    };
    flt("Display", "FieldOfView", s.fov);
    flt("Hud", "Scale", s.hudScale);
    flag("Hud", "Show", s.showHud);
    snprintf(buf, sizeof(buf), "%d", s.radarMode);
    WritePrivateProfileStringA("Hud", "RadarMode", buf, g_iniPath);
    flt("Audio", "Master", s.masterVol);
    flt("Audio", "Sfx", s.sfxVol);
    flt("Audio", "Music", s.musicVol);
    flag("Rewrite", "HitMarker", s.hitMarker);
    flag("Rewrite", "DamageFlash", s.damageFlash);
    flag("Rewrite", "CrosshairDot", s.crosshairDot);
    flag("Rewrite", "AutoReload", s.autoReload);
    flt("Controls", "MouseSensitivity", s.mouseSens);
    flag("Controls", "InvertY", s.invertY);
    snprintf(buf, sizeof(buf), "%d", s.windowSize);
    WritePrivateProfileStringA("Display", "WindowSize", buf, g_iniPath);
    snprintf(buf, sizeof(buf), "%d", s.language);
    WritePrivateProfileStringA("Display", "Language", buf, g_iniPath);
    WritePrivateProfileStringA("Display", "MipMaps", s.mipmaps ? "1" : "0", g_iniPath);
    WritePrivateProfileStringA("Display", "MapLegend", s.mapLegend ? "1" : "0", g_iniPath);
    snprintf(buf, sizeof(buf), "%d", s.antiAlias);
    WritePrivateProfileStringA("Display", "AntiAlias", buf, g_iniPath);
    snprintf(buf, sizeof(buf), "%.4f", s.brightness);
    WritePrivateProfileStringA("Display", "Brightness", buf, g_iniPath);
}
static HWND g_hwnd = nullptr;
// Pending window resize (WndProc -> main loop): keep the backbuffer matched to
// the client area so the image is never a stretched/blurry upscale.
static volatile bool g_pendingResize = false;
static volatile int  g_resizeW = 0, g_resizeH = 0;
static const wchar_t* WINDOW_CLASS = L"GTASACppWindow";
static const wchar_t* WINDOW_TITLE = L"GTA SA C++ (Playable)";

// Raw mouse motion (WM_INPUT), accumulated per frame. Raw input reports true
// relative deltas straight from the device, bypassing pointer acceleration and
// the cursor clamp at the screen edge -- the old "warp to centre, diff the
// cursor" scheme spun forever on trackpads because precision touchpads report
// absolute positions and acceleration left the cursor off-centre after each
// warp. g_rawMouseAbs tracks the last absolute position for devices that only
// send MOUSE_MOVE_ABSOLUTE (precision touchpads, RDP, some tablets).
static volatile long g_rawMouseDX = 0, g_rawMouseDY = 0;
static bool g_rawMouseHaveAbs = false;
static long g_rawMouseAbsX = 0, g_rawMouseAbsY = 0;
static bool g_cursorCentred = false;   // the cursor was pinned to the window centre last frame
static bool g_rawMouseOk = false;  // RegisterRawInputDevices succeeded

static FILE* g_log = nullptr;
static void Log(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

// Mouse look hides the pointer and confines it to the window; menus need it free and visible.
// MouseMode (ini [Input] / GTA_MOUSEMODE): 0 = hidden + confined pointer (relative motion from the
// compositor's pointer lock), 1 = free pointer that is warped back to the window centre each frame
// (works when the compositor never delivers relative motion, e.g. remote or absolute pointers).
static int MouseMode() {
    static int mode = -1;
    if (mode < 0) {
        mode = (int)GetPrivateProfileIntA("Input", "MouseMode", 1, g_iniPath);
        if (const char* e = getenv("GTA_MOUSEMODE")) mode = atoi(e);
        Log("MOUSE: mode %d", mode);
    }
    return mode;
}

// A fully transparent cursor: the pointer stays "visible" to the system, so a Wayland compositor does not
// lock it (a null cursor makes Wine request a pointer lock, which stops all absolute motion).
static HCURSOR BlankCursor() {
    static HCURSOR c = nullptr;
    if (!c) {
        BYTE andMask[32 * 4], xorMask[32 * 4];
        memset(andMask, 0xFF, sizeof(andMask));
        memset(xorMask, 0x00, sizeof(xorMask));
        c = CreateCursor(GetModuleHandleA(nullptr), 0, 0, 32, 32, andMask, xorMask);
    }
    return c;
}

static void SetPointerCapture(bool on) {
    if (on == g_cursorHidden) return;
    g_cursorHidden = on;
    if (MouseMode() == 0) {
        if (on) { ShowCursor(FALSE); }
        else { ClipCursor(nullptr); while (ShowCursor(TRUE) < 0) {} }
    } else {
        ClipCursor(nullptr);   // the cursor image is hidden by WM_SETCURSOR while captured
        SetCursor(on ? BlankCursor() : LoadCursor(nullptr, IDC_ARROW));
    }
    Log("MOUSE: pointer %s", on ? "captured (hidden, confined)" : "released (visible, free)");
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_ACTIVATE:
        g_hasFocus = LOWORD(wp) != WA_INACTIVE;
        if (!g_hasFocus) for (int i = 0; i < 3; i++) g_msgBtn[i] = false;
        if (!g_hasFocus) ClipCursor(nullptr);
        if (g_hasFocus) SetFocus(hwnd);   // keyboard back to us on click/alt-tab return
        Log("WINDOW: %s (state %u, fg=%p self=%p focus=%p)", g_hasFocus ? "activated" : "deactivated",
            (unsigned)LOWORD(wp), (void*)GetForegroundWindow(), (void*)hwnd, (void*)GetFocus());
        return 0;
    case WM_SETCURSOR:
        if (g_cursorHidden && LOWORD(lp) == HTCLIENT) { SetCursor(MouseMode() == 0 ? nullptr : BlankCursor()); return TRUE; }
        break;
    case WM_MOUSEACTIVATE:
        return MA_ACTIVATE;               // a click on the game always brings it back
    case WM_SYSCOMMAND:
        // Alt is the walk key (as in SA PC): don't let a lone Alt press open
        // the window menu, which would freeze the game loop until dismissed.
        if ((wp & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_KEYDOWN:
        // Play mode: ESC opens the pause menu (quit is Quit Game in there);
        // the capture modes have no menu, so ESC still exits them.
        if (getenv("GTA_KEYLOG")) Log("KEY %u", (unsigned)wp);
        if (wp == VK_ESCAPE && !g_escOpensMenu) { g_running = false; return 0; }
        if (wp >= 'A' && wp <= 'Z' && !(lp & (1 << 30))) Cheat::Push((char)wp);   // CCheat::AddToCheatString
        if (g_keyQueueN < (int)(sizeof(g_keyQueue) / sizeof(g_keyQueue[0])))
            g_keyQueue[g_keyQueueN++] = (int)wp;   // auto-repeat included: menu scrolling
        return 0;
    case WM_INPUT: {
        RAWINPUT ri;
        UINT sz = sizeof(ri);
        if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, &ri, &sz,
                            sizeof(RAWINPUTHEADER)) == (UINT)-1)
            break;
        if (ri.header.dwType == RIM_TYPEMOUSE) {
            const RAWMOUSE& m = ri.data.mouse;
            g_dbgRaw = g_dbgRaw + 1;
            if (g_dbgRaw <= 40) Log("MOUSE raw #%ld flags=0x%x dx=%ld dy=%ld buttons=0x%x", (long)g_dbgRaw, (unsigned)m.usFlags, (long)m.lLastX, (long)m.lLastY, (unsigned)m.usButtonFlags);
            if (m.usFlags & MOUSE_MOVE_ABSOLUTE) {
                // Precision touchpad / RDP: absolute coords, so diff them.
                if (g_rawMouseHaveAbs) {
                    g_rawMouseDX += m.lLastX - g_rawMouseAbsX;
                    g_rawMouseDY += m.lLastY - g_rawMouseAbsY;
                }
                g_rawMouseAbsX = m.lLastX; g_rawMouseAbsY = m.lLastY;
                g_rawMouseHaveAbs = true;
            } else {
                // Ordinary mouse: lLastX/Y are already relative deltas.
                g_rawMouseDX += m.lLastX;
                g_rawMouseDY += m.lLastY;
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
        g_msgBtn[msg == WM_LBUTTONDOWN ? 0 : msg == WM_RBUTTONDOWN ? 1 : 2] = true;
        SetCapture(hwnd);
        return 0;
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP:
        g_msgBtn[msg == WM_LBUTTONUP ? 0 : msg == WM_RBUTTONUP ? 1 : 2] = false;
        if (!g_msgBtn[0] && !g_msgBtn[1] && !g_msgBtn[2]) ReleaseCapture();
        return 0;
    case WM_CAPTURECHANGED:
        for (int i = 0; i < 3; i++) g_msgBtn[i] = false;
        break;
    case WM_MOUSEMOVE:
        g_msgMouseX = (short)LOWORD(lp); g_msgMouseY = (short)HIWORD(lp);
        g_msgMouseSeen = true;
        g_dbgMove = g_dbgMove + 1;
        if (g_dbgMove <= 40) Log("MOUSE move #%ld client=(%d,%d)", (long)g_dbgMove, (int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        return 0;
    case WM_MOUSEWHEEL:
        g_wheelSteps += GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA;
        return 0;
    case WM_SIZE:
        // Defer the device Reset to the render loop (WM_SIZE can fire many
        // times during a drag); ignore minimize (0x0 client).
        if (wp != SIZE_MINIMIZED) {
            g_resizeW = LOWORD(lp);
            g_resizeH = HIWORD(lp);
            g_pendingResize = (g_resizeW > 0 && g_resizeH > 0);
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ------------------------------------------------------------ game folder
// The user's own GTA:SA install (PC 1.0). Resolved once at startup by
// ResolveGameDir; nothing machine-specific is compiled in.
static std::string g_gameDir;
static Vehicles g_vehicles;
static Peds g_peds;
static Delirium g_delirium;   // optional active-ragdoll plugin (runtime-loaded DLL)
static Speech g_speech;   // CAEPedSpeechAudioEntity
static int g_crowdTarget = 0;   // ambient pedestrians around CJ (key O toggles, GTA_PEDS=n sets it)
static int g_vehSpawnIdx = 0;

static bool IsGameDir(const std::string& d) {
    if (d.empty()) return false;
    DWORD a = GetFileAttributesA((d + "\\models\\gta3.img").c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string TrimSlash(std::string d) {
    while (!d.empty() && (d.back() == '\\' || d.back() == '/')) d.pop_back();
    return d;
}

// Ask for the install folder with the standard folder picker.
static std::string PickGameDir() {
    std::string out;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return out;
    BROWSEINFOA bi = {};
    bi.lpszTitle = "Select your GTA San Andreas folder (the one containing the "
                   "\"models\" folder).";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    for (;;) {
        PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
        if (!pidl) break;   // cancelled
        char path[MAX_PATH] = {};
        BOOL ok = SHGetPathFromIDListA(pidl, path);
        CoTaskMemFree(pidl);
        if (ok && IsGameDir(path)) { out = path; break; }
        MessageBoxA(nullptr, "That folder does not contain models\\gta3.img.\n"
                    "Pick the folder GTA San Andreas is installed in.",
                    "gta-sa-cpp", MB_OK | MB_ICONWARNING);
    }
    CoUninitialize();
    return out;
}

// Order: --game <dir>, $GTA_SA_DIR, [Paths] GameDir in gtasa_cpp.ini, the
// exe's folder and its parents (drop the build into the game folder), the
// common Steam / Rockstar install folders, then a folder picker (interactive
// runs only). A found folder is remembered in the ini.
static bool ResolveGameDir(const std::string& cli, bool interactive) {
    std::vector<std::string> tries;
    if (!cli.empty()) tries.push_back(cli);
    if (const char* env = getenv("GTA_SA_DIR"); env && *env) tries.push_back(env);
    char ini[MAX_PATH] = {};
    GetPrivateProfileStringA("Paths", "GameDir", "", ini, sizeof(ini), g_iniPath);
    if (*ini) tries.push_back(ini);
    char exe[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exe, sizeof(exe));
    for (std::string d = exe; ;) {
        size_t s = d.find_last_of('\\');
        if (s == std::string::npos) break;
        d.resize(s);
        tries.push_back(d);
    }
    char cwd[MAX_PATH] = {};
    GetCurrentDirectoryA(sizeof(cwd), cwd);
    tries.push_back(cwd);
    for (const char* pf : {"ProgramFiles(x86)", "ProgramFiles"}) {
        const char* base = getenv(pf);
        if (!base) continue;
        tries.push_back(std::string(base) + "\\Steam\\steamapps\\common\\Grand Theft Auto San Andreas");
        tries.push_back(std::string(base) + "\\Rockstar Games\\GTA San Andreas");
        tries.push_back(std::string(base) + "\\Rockstar Games\\Grand Theft Auto San Andreas");
    }
    for (const std::string& t : tries) {
        if (IsGameDir(TrimSlash(t))) { g_gameDir = TrimSlash(t); break; }
    }
    if (g_gameDir.empty() && interactive) g_gameDir = PickGameDir();
    if (g_gameDir.empty()) return false;
    if (_stricmp(ini, g_gameDir.c_str()) != 0)
        WritePrivateProfileStringA("Paths", "GameDir", g_gameDir.c_str(), g_iniPath);
    return true;
}

static const std::string& GameDir() { return g_gameDir; }
static std::string FindGameFile(const std::string& rel) {
    return GameDir() + "\\" + rel;
}
static void WideToUtf8(LPCWSTR w, char* out, int outSize) {
    WideCharToMultiByte(CP_ACP, 0, w, -1, out, outSize - 1, nullptr, nullptr);
    out[outSize - 1] = '\0';
}

static D3DMATRIX QuatToD3DMatrix(float qx, float qy, float qz, float qw,
                                 float tx, float ty, float tz) {
    // Same as World.cpp QuatToMatrix (D3D row-vector convention).
    // (Was the transpose, which disagreed with the world renderer.)
    D3DMATRIX m;
    float xx = qx*qx, yy = qy*qy, zz = qz*qz;
    float xy = qx*qy, xz = qx*qz, yz = qy*qz;
    float wx = qw*qx, wy = qw*qy, wz = qw*qz;
    m.m[0][0] = 1-2*(yy+zz); m.m[0][1] = 2*(xy+wz);    m.m[0][2] = 2*(xz-wy);    m.m[0][3] = 0;
    m.m[1][0] = 2*(xy-wz);   m.m[1][1] = 1-2*(xx+zz);  m.m[1][2] = 2*(yz+wx);    m.m[1][3] = 0;
    m.m[2][0] = 2*(xz+wy);   m.m[2][1] = 2*(yz-wx);    m.m[2][2] = 1-2*(xx+yy);  m.m[2][3] = 0;
    m.m[3][0] = tx;          m.m[3][1] = ty;           m.m[3][2] = tz;           m.m[3][3] = 1;
    return m;
}

// World-space collision box from Rockstar COL data (placement-transformed).
struct ColBox { float minX, minY, minZ, maxX, maxY, maxZ; };
struct MapObject {
    std::vector<D3DRenderMesh*> meshes;
    D3DMATRIX worldMatrix;
    // World-space collision AABB (from DFF vertex bounds).
    float cMinX = 0, cMinY = 0, cMinZ = 0;
    float cMaxX = 0, cMaxY = 0, cMaxZ = 0;
    std::vector<ColBox> colBoxes; // world-space COL boxes; empty = fall back to cMin/cMax AABB
    bool solid = true;   // false = walk-through visual (roads, grass, tags...)
    std::string name;
    // LOD streaming (Rockstar-style): HD meshes + LOD meshes, switch by camera distance.
    std::vector<D3DRenderMesh*> lodMeshes;
    float lodDist = 60.0f;  // render HD when closer than this, LOD when farther
    float objX = 0, objY = 0, objZ = 0;  // object origin for distance checks
    bool hasLod = false;    // true when lodMeshes is populated
};

// ---- Simple 8x8 bitmap font for debug overlay ----
// Each character is 8 bytes (8 rows of 8 pixels, MSB = leftmost pixel).
// Covers ASCII 32-127.
static const uint8_t kFont[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // space
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}, // !
    {0x66,0x66,0x66,0x00,0x00,0x00,0x00,0x00}, // "
    {0x66,0x66,0xFF,0x66,0xFF,0x66,0x66,0x00}, // #
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // $
    {0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0x00}, // %
    {0x1C,0x36,0x1C,0x38,0x6F,0x66,0x3B,0x00}, // &
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00}, // '
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}, // (
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ,
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
    {0x06,0x0C,0x18,0x30,0x60,0x40,0x00,0x00}, // /
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    {0x3C,0x66,0x06,0x0C,0x30,0x60,0x7E,0x00}, // 2
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 3
    {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}, // 4
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 5
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 6
    {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00}, // 7
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 8
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, // 9
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // :
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // ;
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}, // <
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // =
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00}, // >
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, // ?
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, // @
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}, // A
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // B
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // C
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // D
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}, // E
    {0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}, // F
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // G
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // J
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // K
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // L
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // N
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // O
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // P
    {0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0x00}, // Q
    {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}, // R
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // S
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // U
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // X
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // Y
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // Z
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // [
    {0x60,0x30,0x18,0x0C,0x06,0x02,0x00,0x00}, // backslash
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ]
    {0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x18,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, // a
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // b
    {0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0x00}, // c
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // d
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // e
    {0x1C,0x36,0x30,0x78,0x30,0x30,0x30,0x00}, // f
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, // g
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // h
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // i
    {0x0C,0x00,0x1C,0x0C,0x0C,0x0C,0x6C,0x38}, // j
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // k
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // l
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // m
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // n
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // o
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // p
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, // q
    {0x00,0x00,0x6C,0x76,0x60,0x60,0x60,0x00}, // r
    {0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}, // s
    {0x30,0x30,0x7C,0x30,0x30,0x34,0x18,0x00}, // t
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, // u
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // v
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x66,0x00}, // w
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, // x
    {0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}, // y
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // z
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // {
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // |
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // }
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
};

// Debug text renderer: draws 8x8 bitmap font as screen-space quads.
// Uses DrawPrimitiveUP with pre-transformed vertices (XYZRHW).
struct DebugVertex {
    float x, y, z, rhw;
    DWORD color;
};
#define DEBUG_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)

static void DrawDebugText(IDirect3DDevice9* dev, const char* text, float px, float py,
                          float scale, DWORD color) {
    if (!dev || !text) return;
    const float cw = 8.0f * scale;
    const float ch = 8.0f * scale;
    float cx = px;
    // Pre-transformed quads: 2 triangles per pixel would be slow;
    // instead draw one quad per set pixel.
    // To keep it simple, batch all pixels into one DrawPrimitiveUP call.
    // Max: 64 pixels/char * 128 chars = 8192 quads = 16384 tris. Fine.
    static DebugVertex verts[16384 * 3];
    const int kMaxVerts = (int)(sizeof(verts) / sizeof(verts[0]));
    int vcount = 0;
    // Flush in batches instead of stopping at a fixed cap: the old
    // vcount < 16380 cutoff silently truncated long overlays mid-line.
    auto flush = [&]() {
        if (vcount <= 0) return;
        dev->SetFVF(DEBUG_FVF);
        dev->SetTexture(0, nullptr);
        // Disable depth test for overlay, and lighting: these are pre-transformed
        // XYZRHW vertices with no normals.
        DWORD wasLighting = FALSE, wasCull = D3DCULL_CCW;
        dev->GetRenderState(D3DRS_LIGHTING, &wasLighting);
        dev->GetRenderState(D3DRS_CULLMODE, &wasCull);
        dev->SetRenderState(D3DRS_LIGHTING, FALSE);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        // HUD quads are pre-transformed screen-space triangles; the world cull
        // mode (CW since the projection mirror) would cull them, so disable
        // culling around the HUD draw and restore afterwards.
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vcount / 3, verts, sizeof(DebugVertex));
        dev->SetRenderState(D3DRS_CULLMODE, wasCull);
        dev->SetRenderState(D3DRS_ZENABLE, TRUE);
        dev->SetRenderState(D3DRS_LIGHTING, wasLighting);
        vcount = 0;
    };
    for (const char* p = text; *p; p++) {
        if (vcount > kMaxVerts - 64 * 6) flush();  // room for one full glyph
        unsigned char c = (unsigned char)*p;
        if (c == '\n') { cx = px; py += ch + 2; continue; }
        if (c < 32 || c > 127) { cx += cw; continue; }
        const uint8_t* glyph = kFont[c - 32];
        for (int row = 0; row < 8; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                if (bits & (0x80 >> col)) {
                    float x0 = cx + col * scale;
                    float y0 = py + row * scale;
                    float x1 = x0 + scale;
                    float y1 = y0 + scale;
                    // Two triangles
                    verts[vcount++] = {x0, y0, 0.5f, 1.0f, color};
                    verts[vcount++] = {x1, y0, 0.5f, 1.0f, color};
                    verts[vcount++] = {x0, y1, 0.5f, 1.0f, color};
                    verts[vcount++] = {x0, y1, 0.5f, 1.0f, color};
                    verts[vcount++] = {x1, y0, 0.5f, 1.0f, color};
                    verts[vcount++] = {x1, y1, 0.5f, 1.0f, color};
                }
            }
        }
        cx += cw;
    }
    flush();
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    // Per-monitor DPI awareness: without it Windows bitmap-stretches the whole
    // window on a scaled display, so a "1280x720" render is resampled to the
    // physical pixels and looks soft/pixelated.
    SetProcessDPIAware();

    int WIDTH = 1280;   // render resolution (backbuffer)
    int HEIGHT = 720;
    int WIN_W = 0;      // window client size; 0 = borderless fullscreen
    int WIN_H = 0;
    bool resGiven = false;  // was --res passed? (else borderless uses desktop res)

    int maxFrames = 0;
    std::string shotPath;
    std::string logPath = "gtasa_cpp.log";
    std::string gameDirArg;   // --game <dir>: the GTA:SA install folder
    float camYawOverride = 0.0f, camPitchOverride = 0.0f;
    float camPosOverrideX = 0.0f, camPosOverrideY = 0.0f, camPosOverrideZ = 0.0f;
    bool hasYawOverride = false, hasPitchOverride = false, hasPosOverride = false;
    bool playMode = true;   // the game is the default; --grove / --housetest are the old static scenes
    bool scripted = false;
    bool flyover = false;
    bool auditCollision = false;
    bool auditSigns = false;
    float startHour = 12.0f;
    int startMenuScreen = -1;
    std::string weatherName;
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; i < argc; i++) {
            char arg[512] = {};
            WideToUtf8(argv[i], arg, sizeof(arg));
            std::string a = arg;
            if (a == "--frames" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                maxFrames = atoi(n); i++;
            } else if (a == "--screenshot" && i + 1 < argc) {
                char p[512] = {};
                WideToUtf8(argv[i + 1], p, sizeof(p));
                shotPath = p; i++;
            } else if (a == "--game" && i + 1 < argc) {
                char p[MAX_PATH] = {};
                WideToUtf8(argv[i + 1], p, sizeof(p));
                gameDirArg = p; i++;
            } else if (a == "--log" && i + 1 < argc) {
                char p[512] = {};
                WideToUtf8(argv[i + 1], p, sizeof(p));
                logPath = p; i++;
            } else if (a == "--play") {   // legacy no-op: the game is the default
            } else if (a == "--scripted") {
                scripted = true;
                playMode = true;  // scripted implies play mode
            } else if (a == "--yaw" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                camYawOverride = (float)atof(n) * 3.14159265f / 180.0f;
                hasYawOverride = true; i++;
            } else if (a == "--pitch" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                camPitchOverride = (float)atof(n) * 3.14159265f / 180.0f;
                hasPitchOverride = true; i++;
            } else if (a == "--pos" && i + 1 < argc) {
                char n[128] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                if (sscanf(n, "%f,%f,%f", &camPosOverrideX, &camPosOverrideY, &camPosOverrideZ) == 3)
                    hasPosOverride = true;
                i++;
            } else if (a == "--flyover") {
                flyover = true;
                playMode = true;  // flyover implies play mode
            } else if (a == "--hour" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                startHour = (float)atof(n); i++;
            } else if (a == "--weather" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                weatherName = n;
                g_weatherAuto = false;
                for (auto& c : weatherName) c = (char)toupper((unsigned char)c);
                i++;
            } else if (a == "--notimeflow") {
                g_timeFlow = false;
            } else if (a == "--menu" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                startMenuScreen = atoi(n); i++;   // headless: open the pause menu on screen N
            } else if (a == "--debug-collision") {
                g_devShowCollision = true;  // start with the pink COL view on
            } else if (a == "--rewrite") {
                g_forceStyle = 1;           // headless: rewrite movement + cameras
            } else if (a == "--classic") {
                g_forceStyle = 0;
            } else if (a == "--aimtest") {
                g_aimTest = true;           // a shooting range with dummy peds in front of the spawn
            } else if (a == "--third") {
                g_startThirdPerson = true;  // headless: start in the third-person camera
            } else if (a == "--walk") {
                g_autoWalk = true;          // headless: auto-walk forward, log pos (collision probe)
            } else if (a == "--gait" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                const std::string gait = n;  // headless --walk speed: run (default), walk, sprint, jump
                g_autoGait = gait == "walk" ? 1 : gait == "sprint" ? 2 : gait == "jump" ? 3 : 0;
                i++;
            } else if (a == "--audit-collision") {
                auditCollision = true;      // CPU-only: audit every model vs its COL, write TSV, exit
            } else if (a == "--probe-collision") {
                g_probeCollision = true;    // after streaming at --pos, dump a collision solidity report
                playMode = true;
            } else if (a == "--audit-signs") {
                auditSigns = true;          // CPU-only: audit every billboard/sign texture+collision
            } else if (a == "--window" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                int ww = 0, wh = 0;
                if (sscanf(n, "%dx%d", &ww, &wh) == 2 && ww >= 320 && wh >= 240 &&
                    ww <= 7680 && wh <= 4320) {
                    WIN_W = ww;
                    WIN_H = wh;
                } else {
                    Log("WARN: bad --window '%s' (want WxH, e.g. 1600x900)", n);
                }
                i++;
            } else if (a == "--res" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[i + 1], n, sizeof(n));
                int rw = 0, rh = 0;
                if (sscanf(n, "%dx%d", &rw, &rh) == 2 && rw >= 320 && rh >= 240 &&
                    rw <= 7680 && rh <= 4320) {
                    WIDTH = rw;
                    HEIGHT = rh;
                    resGiven = true;
                } else {
                    Log("WARN: bad --res '%s' (want WxH, e.g. 1920x1080)", n);
                }
                i++;
            }
        }
        LocalFree(argv);
    }

    g_log = fopen(logPath.c_str(), "w");

    // Screenshot directory: <exe_dir>\screenshots (created if missing)
    char shotDir[MAX_PATH] = {};
    {
        char exePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, exePath, sizeof(exePath));
        char* slash = strrchr(exePath, '\\');
        if (slash) *slash = '\0';
        snprintf(shotDir, sizeof(shotDir), "%s\\screenshots", exePath);
        CreateDirectoryA(shotDir, nullptr);
        snprintf(g_iniPath, sizeof(g_iniPath), "%s\\gtasa_cpp.ini", exePath);
    }
    if (!ResolveGameDir(gameDirArg, maxFrames == 0)) {
        Log("FATAL: GTA San Andreas folder not found (use --game <dir> or set GTA_SA_DIR)");
        if (maxFrames == 0)
            MessageBoxA(nullptr, "GTA San Andreas was not found.\n\n"
                        "Run with --game \"<your GTA San Andreas folder>\", set GTA_SA_DIR, "
                        "or put this program inside the game folder.",
                        "gta-sa-cpp", MB_OK | MB_ICONERROR);
        if (g_log) fclose(g_log);
        return 1;
    }
    Log("GAME: %s", g_gameDir.c_str());
    // Display settings from the pause menu (frame limiter, VSync, draw distance).
    LoadSettings(g_menu.Settings());
    if (g_forceStyle >= 0) g_menu.Settings().movementStyle = g_menu.Settings().cameraStyle = g_forceStyle;
    g_renderer.SetVsync(g_menu.Settings().vsync);   // before Init: picks the present interval
    g_world.SetHdDrawScale(g_menu.Settings().drawDist);
    g_escOpensMenu = playMode && !scripted && !flyover;
    Log("SETTINGS: %s frameLimit=%d vsync=%d drawDist=%.3f", g_iniPath,
        g_menu.Settings().frameLimit, (int)g_menu.Settings().vsync, g_menu.Settings().drawDist);

    { char cwd[MAX_PATH] = {}; GetCurrentDirectoryA(sizeof(cwd), cwd);
      Log("gtasa_cpp starting (cwd=%s) play=%d shotDir=%s",
          cwd, playMode, shotDir);
      Log("  startHour=%.2f weather=%s timeFlow=%d", startHour,
          weatherName.empty() ? "(default SUNNY_LA)" : weatherName.c_str(),
          (int)g_timeFlow); }

    // --audit-collision: pure-CPU pass (no window/device). Audit every model's
    // collision vs its visual bounds, write a TSV next to the exe, and exit.
    if (auditCollision) {
        ImgLoader aimg;
        std::string aimgPath = FindGameFile("models\\gta3.img");
        if (!aimg.Open(aimgPath)) { Log("audit: failed to open %s", aimgPath.c_str()); return 1; }
        Log("audit: IMG opened (%u entries)", (unsigned)aimg.EntryCount());
        char out[MAX_PATH];
        snprintf(out, sizeof(out), "%s\\collision_audit.tsv", shotDir);
        int flagged = RunCollisionAudit(aimg, GameDir(), out);
        Log("audit: %d flagged models; TSV=%s", flagged, out);
        return 0;
    }
    if (auditSigns) {
        ImgLoader aimg;
        std::string aimgPath = FindGameFile("models\\gta3.img");
        if (!aimg.Open(aimgPath)) { Log("audit-signs: failed to open %s", aimgPath.c_str()); return 1; }
        char out[MAX_PATH];
        snprintf(out, sizeof(out), "%s\\sign_audit.tsv", shotDir);
        int flagged = RunSignAudit(aimg, GameDir(), out);
        Log("audit-signs: %d flagged; TSV=%s", flagged, out);
        return 0;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = WINDOW_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    // Default: borderless window covering the whole monitor (windowed
    // fullscreen). `--window WxH` instead gives a normal titled/resizable
    // window of that client size. The backbuffer (render res) matches the
    // client area so the image is crisp 1:1; WM_SIZE -> device Reset keeps it
    // matched if the window is moved/resized.
    const bool windowed = (WIN_W > 0 && WIN_H > 0);
    g_menu.Settings().displayMode = windowed ? 1 : 0;
    if (windowed) {
        g_hwnd = CreateWindowExW(0, WINDOW_CLASS, WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, WIN_W, WIN_H,
            nullptr, nullptr, hInst, nullptr);
        if (!g_hwnd) { Log("CreateWindow failed"); return 1; }
        // Grow the outer window so the CLIENT area is exactly WIN_W x WIN_H.
        RECT rc = { 0, 0, WIN_W, WIN_H };
        if (AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE)) {
            SetWindowPos(g_hwnd, nullptr, 0, 0, rc.right - rc.left,
                         rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
        }
    } else {
        // Borderless fullscreen: popup covering the primary monitor. Render at
        // the desktop resolution (unless --res forced a size) so it's sharp.
        int scrW = GetSystemMetrics(SM_CXSCREEN);
        int scrH = GetSystemMetrics(SM_CYSCREEN);
        if (scrW < 320 || scrH < 240) { scrW = WIDTH; scrH = HEIGHT; } // headless fallback
        if (!resGiven) { WIDTH = scrW; HEIGHT = scrH; }
        g_hwnd = CreateWindowExW(0, WINDOW_CLASS, WINDOW_TITLE,
            WS_POPUP | WS_VISIBLE, 0, 0, scrW, scrH,
            nullptr, nullptr, hInst, nullptr);
        if (!g_hwnd) { Log("CreateWindow failed"); return 1; }
    }
    {
        RECT cr = {};
        GetClientRect(g_hwnd, &cr);
        Log("window: client %dx%d (render %dx%d) %s", cr.right - cr.left,
            cr.bottom - cr.top, WIDTH, HEIGHT, windowed ? "windowed" : "borderless-fullscreen");
    }

    // Register the mouse for raw input so look uses true device deltas (fixes
    // the trackpad spin from the old cursor-warp scheme). RIDEV_INPUTSINK lets
    // us keep receiving motion even if focus briefly shifts.
    {
        RAWINPUTDEVICE rid = {};
        rid.usUsagePage = 0x01;   // generic desktop
        rid.usUsage     = 0x02;   // mouse
        rid.dwFlags     = 0;
        rid.hwndTarget  = g_hwnd;
        g_rawMouseOk = RegisterRawInputDevices(&rid, 1, sizeof(rid)) != FALSE;
        Log("raw mouse input: %s", g_rawMouseOk ? "on" : "FAILED (cursor-delta fallback)");
    }

    if (!g_renderer.Init(g_hwnd, WIDTH, HEIGHT)) {
        Log("D3DRenderer::Init failed"); return 1;
    }
    Log("D3D9 initialized: %s", g_renderer.GetAdapterDesc());
    g_renderer.SetLightingEnabled(g_devLighting);  // keep the dev flag and the device in sync
    if (g_escOpensMenu) {
        g_menuReady = g_menu.Init(g_renderer, GameDir());
        Log("FRONTEND: pause menu %s", g_menuReady ? "ready" : "FAILED (fonts.txd / american.gxt?)");
        if (g_menuReady && startMenuScreen >= 0) { g_menu.Open(startMenuScreen); g_devPaused = true; }
        g_hudReady = g_hud.Init(g_renderer, GameDir());
        if (g_hudReady && g_menu.Settings().language > 0) g_hud.SetLanguage(GameDir(), kLanguageFiles[g_menu.Settings().language]);
        if (g_menu.Settings().antiAlias > 0) g_renderer.SetAntiAlias(g_menu.Settings().antiAlias);
        g_fx.Init(g_renderer);
        g_waterCannons.Init(g_renderer);
        Log("AUDIO: %s", g_audio.Init(GameDir()) ? "ready" : "unavailable (audio/CONFIG, audio/sfx?)");
        if (g_audio.Ready()) { g_radio.Init(GameDir(), &g_audio); g_speech.Init(&g_audio); }
        if (g_menu.Settings().movementStyle == 1) {   // optional physics plugin: only when its DLL and data are configured
            char dll[MAX_PATH] = {}, data[MAX_PATH] = {};
            GetPrivateProfileStringA("Delirium", "Dll", "", dll, sizeof(dll), g_iniPath);
            GetPrivateProfileStringA("Delirium", "Data", "", data, sizeof(data), g_iniPath);
            if (dll[0]) Log("DELIRIUM: %s", g_delirium.Load(dll, data) ? "ready" : g_delirium.Error().c_str());
        }
        if (const char* rt = getenv("GTA_RADIOTEST")) {   // dev: decode a few seconds of a station and log the level
            if (g_radio.Ready()) {
                g_radio.Tune(atoi(rt));
                std::vector<int16_t> pcm(32000 * 2 * 5);
                const size_t got = g_radio.Pull(pcm.data(), 32000 * 5);
                double sum = 0;
                for (size_t i = 0; i < got * 2; i++) sum += (double)pcm[i] * pcm[i];
                Log("RADIO: station %d decoded %zu frames, rms %.0f", atoi(rt), got, got ? std::sqrt(sum / (got * 2)) : 0.0);
            } else Log("RADIO: not ready");
        }
        if (g_audio.Ready() && getenv("GTA_AUDIOTEST"))
        {
            Log("AUDIO: weapons bank %d sounds, menu bank %d, feet %d, pain %d, swimming %d", g_audio.SoundCount(BANK_WEAPONS),
                g_audio.SoundCount(BANK_FRONTEND_MENU), g_audio.SoundCount(BANK_FEET_GENERIC), g_audio.SoundCount(BANK_PAIN_CARL),
                g_audio.SoundCount(BANK_SWIMMING));
        }
        Log("HUD: %s", g_hudReady ? "ready" : "FAILED (fonts.txd / hud.txd?)");
    }

    ImgLoader img;
    std::string imgPath = FindGameFile("models\\gta3.img");
    if (!img.Open(imgPath)) { Log("Failed to open %s", imgPath.c_str()); return 1; }
    Log("IMG opened: %u entries", (unsigned)img.EntryCount());

    // ---- Time of day (data/timecyc.dat) ----
    {
        std::string tcPath = FindGameFile("data\\timecyc.dat");
        g_timecycOk = g_timecyc.Load(tcPath);
        Log("TIMECYC: %s -> %s", tcPath.c_str(), g_timecycOk ? "OK" : "FAILED");
        for (size_t i = 0; i < g_timecyc.WeatherCount(); i++)
            Log("  weather[%zu] = %s", i, g_timecyc.WeatherName(i).c_str());
        if (g_timecycOk) {
            int w = weatherName.empty() ? g_timecyc.FindWeather("SUNNY_LA")
                                        : g_timecyc.FindWeather(weatherName);
            if (w < 0) w = 0;   // unknown name: retail default block
            g_weatherIdx = (size_t)w;
            g_gameSeconds = startHour * 3600.0f;
            if (const char* gd = getenv("GTA_DAY")) g_gameDay = atoi(gd);   // dev: day count (moon phase)
            Log("TIMECYC: weather=%s startHour=%.2f flow=%d",
                g_timecyc.WeatherName(g_weatherIdx).c_str(), startHour, (int)g_timeFlow);
        }
    }

    std::unordered_map<std::string, TxdTexture> texMap;
    std::unordered_map<std::string, bool> txdLoaded;
    std::unordered_map<std::string, IDirect3DTexture9*> d3dTexCache;

    auto ensureTxd = [&](const std::string& txdName) {
        if (txdName.empty() || txdLoaded[txdName]) return;
        txdLoaded[txdName] = true;
        std::string fname = txdName + ".txd";
        if (!img.HasFile(fname)) { Log("TXD not in IMG: %s", fname.c_str()); return; }
        std::vector<uint8_t> data = img.Extract(fname);
        if (data.empty()) { Log("TXD extract failed: %s", fname.c_str()); return; }
        std::vector<TxdTexture> texs = TxdLoader::LoadFromMemory(data.data(), data.size());
        int added = 0;
        for (auto& t : texs) {
            std::string key = TxdLoader::KeyOf(t.name);
            if (texMap.find(key) == texMap.end()) { texMap.emplace(key, std::move(t)); added++; }
        }
        int minW = 1 << 30, minH = 1 << 30;
        for (auto& t : texs) {
            if ((int)t.width < minW) minW = (int)t.width;
            if ((int)t.height < minH) minH = (int)t.height;
        }
        if (texs.empty()) { minW = 0; minH = 0; }
        Log("TXD %s: %u textures (%d new) minDim=%dx%d%s", fname.c_str(),
            (unsigned)texs.size(), added, minW, minH,
            (minW < 64 || minH < 64) ? " [LOD-TXD?]" : "");
    };

    auto getD3dTexture = [&](const std::string& texName) -> IDirect3DTexture9* {
        std::string key = TxdLoader::KeyOf(texName);
        auto it = d3dTexCache.find(key);
        if (it != d3dTexCache.end()) return it->second;
        auto texIt = texMap.find(key);
        if (texIt == texMap.end()) return nullptr;
        TxdTexture& txd = texIt->second;
        IDirect3DTexture9* d3dTex = g_renderer.CreateTexture(
            txd.width, txd.height, txd.d3dFormat, txd.mip0.data(),
            (unsigned)txd.mip0.size(), &txd.extraMips);
        d3dTexCache[key] = d3dTex;
        if (txd.width < 64 || txd.height < 64)
            Log("TEX-LOD? %s: %dx%d (small)", key.c_str(), txd.width, txd.height);
        return d3dTex;
    };

    // Transform a point by a row-major D3DMATRIX (translation in m[3]).
    auto xformPt = [](const D3DMATRIX& m, float x, float y, float z,
                      float& ox, float& oy, float& oz) {
        ox = x*m.m[0][0] + y*m.m[1][0] + z*m.m[2][0] + m.m[3][0];
        oy = x*m.m[0][1] + y*m.m[1][1] + z*m.m[2][1] + m.m[3][1];
        oz = x*m.m[0][2] + y*m.m[1][2] + z*m.m[2][2] + m.m[3][2];
    };

    // Helper: load a DFF (base name, no extension) from IMG, place with a full
    // quaternion, build a world-space collision AABB. If solid, the object is
    // sunk so its base sits at sinkZ (local street level).
    auto loadModelEx = [&](const std::string& dffBase, const std::string& txdName,
                           float x, float y, float z,
                           float qx, float qy, float qz, float qw,
                           bool solid, float sinkZ,
                           std::vector<MapObject>& out) -> bool {
        std::string df = dffBase + ".dff";
        if (!img.HasFile(df)) { Log("  SKIP: DFF not in IMG: %s", df.c_str()); return false; }
        std::vector<uint8_t> dffData = img.Extract(df);
        if (dffData.empty()) { Log("  FAIL: extract %s", df.c_str()); return false; }
        DffModel dff = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
        if (!dff.valid) { Log("  FAIL: parse %s", df.c_str()); return false; }
        ensureTxd(txdName);
        MapObject obj;
        obj.name = dffBase;
        obj.solid = solid;
        obj.worldMatrix = QuatToD3DMatrix(qx, qy, qz, qw, x, y, z);
        // Model-space bounds from DFF vertices.
        float mnx=1e30f, mny=1e30f, mnz=1e30f, mxx=-1e30f, mxy=-1e30f, mxz=-1e30f;
        for (auto& dm : dff.meshes) {
            for (auto& v : dm.vertices) {
                if (v.x<mnx) mnx=v.x; if (v.x>mxx) mxx=v.x;
                if (v.y<mny) mny=v.y; if (v.y>mxy) mxy=v.y;
                if (v.z<mnz) mnz=v.z; if (v.z>mxz) mxz=v.z;
            }
        }
        // World-space AABB: transform the 8 corners.
        float wmnx=1e30f, wmny=1e30f, wmnz=1e30f, wmxx=-1e30f, wmxy=-1e30f, wmxz=-1e30f;
        for (int cxi=0;cxi<2;cxi++) for (int cyi=0;cyi<2;cyi++) for (int czi=0;czi<2;czi++) {
            float px=cxi?mxx:mnx, py=cyi?mxy:mny, pz=czi?mxz:mnz, ox, oy, oz;
            xformPt(obj.worldMatrix, px, py, pz, ox, oy, oz);
            if (ox<wmnx) wmnx=ox; if (ox>wmxx) wmxx=ox;
            if (oy<wmny) wmny=oy; if (oy>wmxy) wmxy=oy;
            if (oz<wmnz) wmnz=oz; if (oz>wmxz) wmxz=oz;
        }
        if (solid) {
            float dz = sinkZ - wmnz;   // sink base onto the local street level
            obj.worldMatrix.m[3][2] += dz;
            wmnz += dz; wmxz += dz;
            if (dz < -0.05f || dz > 0.05f)
                Log("  SINK: %s dz=%+.2f", df.c_str(), dz);
        }
        obj.cMinX=wmnx; obj.cMinY=wmny; obj.cMinZ=wmnz;
        obj.cMaxX=wmxx; obj.cMaxY=wmxy; obj.cMaxZ=wmxz;
        for (auto& dm : dff.meshes) {
            const MeshVertex* verts = reinterpret_cast<const MeshVertex*>(dm.vertices.data());
            D3DRenderMesh* mesh = g_renderer.CreateMesh(verts, (uint32_t)dm.vertices.size(),
                dm.indices.data(), (uint32_t)dm.indices.size(), dm.hasPrelit,
                dm.nightColors.empty() ? nullptr : dm.nightColors.data());
            if (!mesh) continue;
            if (!dm.textureName.empty()) mesh->texture = getD3dTexture(dm.textureName);
            obj.meshes.push_back(mesh);
        }
        if (obj.meshes.empty()) { Log("  FAIL: 0 meshes %s", df.c_str()); return false; }
        out.push_back(std::move(obj));
        Log("  OK: %s (%u meshes) modelZ=[%.1f,%.1f] worldAABB=[%.1f,%.1f,%.1f]-[%.1f,%.1f,%.1f] solid=%d",
            df.c_str(), (unsigned)out.back().meshes.size(), mnz, mxz,
            wmnx, wmny, wmnz, wmxx, wmxy, wmxz, solid?1:0);
        return true;
    };

    // Legacy wrapper (yaw-only, no sink) used by --housetest / --grove.
    auto loadHouse = [&](const char* dffName, const char* txdName,
                         float x, float y, float z, float yawRad,
                         std::vector<MapObject>& out) -> bool {
        std::string dn = dffName ? dffName : "";
        if (dn.size() > 4 && dn.substr(dn.size()-4) == ".dff") dn.resize(dn.size()-4);
        float hy = yawRad * 0.5f;
        return loadModelEx(dn, txdName ? txdName : "", x, y, z,
                           0, 0, sinf(hy), cosf(hy), false, 0.0f, out);
    };

    std::vector<MapObject> mapObjects;
    std::vector<std::tuple<float,float,float>> roadPts; // x,y,z of road instances (play mode)
    std::vector<ColTriangle> colTris; // COL3 ground triangles (roads), world space (Agent 15)
    int solidCount = 0;

    // ---- Water (CWaterLevel): water.dat surfaces and the open sea ----
    g_water.Load(g_renderer, GameDir());
    g_sky.Load(g_renderer, GameDir());

    const float CJ_HEIGHT = 1.8132f; // measured from player.img part DFFs (feet/legs/torso/head/hands) bind-pose vertices
    // ---- CJ-scale player reference: box of exactly CJ_HEIGHT ----
    // Feet at z=0 in local space; world matrix puts base at player feet.
    // Plain safety-orange material (scale reference, not final CJ model).
    D3DRenderMesh* playerMesh = nullptr;
    IDirect3DTexture9* playerTex = nullptr;
    {
        const float hw = 0.25f;     // half-width  (0.5m wide)
        const float hd = 0.25f;     // half-depth
        const float ht = CJ_HEIGHT; // 1.8132m tall
        MeshVertex pv[8] = {
            {-hw,-hd,0,  0,0,1,  0xFFFFFFFFu,  0,0}, {hw,-hd,0,  0,0,1,  0xFFFFFFFFu,  1,0},
            {hw, hd,0,  0,0,1,  0xFFFFFFFFu,  1,1},  {-hw,hd,0,  0,0,1,  0xFFFFFFFFu,  0,1},
            {-hw,-hd,ht, 0,0,1,  0xFFFFFFFFu,  0,0}, {hw,-hd,ht, 0,0,1,  0xFFFFFFFFu,  1,0},
            {hw, hd,ht, 0,0,1,  0xFFFFFFFFu,  1,1},  {-hw,hd,ht, 0,0,1,  0xFFFFFFFFu,  0,1},
        };
        uint16_t pidx[36] = {
            0,1,2, 0,2,3,   // bottom (mirrors ground winding)
            4,5,6, 4,6,7,   // top (same pattern as ground)
            0,5,1, 0,4,5,   // front (-Y)
            3,2,6, 3,6,7,   // back (+Y)
            0,3,7, 0,7,4,   // left (-X)
            1,5,6, 1,6,2,   // right (+X)
        };
        playerMesh = g_renderer.CreateMesh(pv, 8, pidx, 36);
        IDirect3DDevice9* pdev = g_renderer.GetDevice();
        if (pdev && SUCCEEDED(pdev->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &playerTex, nullptr))) {
            D3DLOCKED_RECT lr = {};
            if (SUCCEEDED(playerTex->LockRect(0, &lr, nullptr, 0))) {
                *(uint32_t*)lr.pBits = 0xFFFF6600; // safety orange
                playerTex->UnlockRect(0);
            }
        }
        if (playerMesh) playerMesh->texture = playerTex;
        Log("PLAYER MESH: CJ_HEIGHT=%.4fm orange box (%s)", CJ_HEIGHT, playerMesh ? "OK" : "FAIL");
    }
    // CJ himself (player.img clothes + anim\ped.ifp); the box stays as a fallback.
    Stats::Init();   // CStats::Init, then the saved [Stats] values
    Stats::Load(g_iniPath);
    Log("STATS: ar_stats.dat %d values", Stats::LoadReactions(getenv("GTA_ARSTATS") ? getenv("GTA_ARSTATS") : (g_gameDir + "\\data\\ar_stats.dat").c_str())   /* dev: GTA_ARSTATS=alt file */);
    Stats::g_hooks.health = [] { return g_cj.Health() * 5.69f; };   // the stats count health in 569 = 100 hp
    Stats::g_hooks.maxHealth = [] { return g_cj.MaxHealth() * 5.69f; };
    Stats::g_hooks.addHealth = [](float amount, bool) { g_cj.AddHealth(amount / 5.69f); };
    {   // [Player] WeaponSkill (-1 = earned through play): 0 poor, 1 standard, 2 pro (twin pistols); GTA_SKILL overrides
        int skill = GetPrivateProfileIntA("Player", "WeaponSkill", -1, g_iniPath);
        if (const char* e = getenv("GTA_SKILL")) skill = atoi(e);
        PlayerPed::SetWeaponSkill(skill < 0 ? -1 : std::clamp(skill, 0, 2));
        for (int i = 0; i < WeaponStats::kCount; i++) {   // [Stats] saved weapon skill stats (0..1000)
            char buf[32] = {};
            GetPrivateProfileStringA("Stats", WeaponStats::kEntries[i].weapon, "0", buf, sizeof buf, g_iniPath);
            WeaponStats::g_stat[i] = std::clamp((float)atof(buf), 0.0f, 1000.0f);
        }
    }
    const bool cjReady = g_cj.Init(g_renderer, GameDir());
    g_cj.SetWallProbe([](const float* a, const float* b) { return g_world.RayCast(a[0], a[1], a[2], b[0], b[1], b[2]) < 1.0f; });
    if (cjReady) g_cj.SetMaxHealth(Stats::FatMuscleMod(9));
    if (cjReady) {   // [Player] FightStyle: 0 normal, 1 boxing, 2 kung fu, 3 kick boxing; GTA_FIGHTSTYLE overrides
        int fs = GetPrivateProfileIntA("Player", "FightStyle", 0, g_iniPath);
        if (const char* e = getenv("GTA_FIGHTSTYLE")) fs = atoi(e);
        g_cj.SetFightStyle(fs);
    }
    g_vehicles.Init(g_renderer, GameDir());
    g_peds.Init(g_renderer, GameDir());
    if (const char* e = getenv("GTA_PEDS")) g_crowdTarget = atoi(e);
    if (const char* e = getenv("GTA_PEDSEED")) g_peds.SetSeed((unsigned)atoi(e));   // dev
    if (getenv("GTA_SCMTEST")) {   // dev: run the main script's core language and report where it stops
        ScmMachine scm;
        if (scm.Init(GameDir())) {
            if (const char* m = getenv("GTA_SCMTEST")) if (atoi(m) > 0) scm.LaunchMission(atoi(m) - 1);
            scm.Update(0);
            for (size_t i = 0; i < scm.ThreadCount(); i++) {
                const ScmThread& t = scm.Thread(i);
                std::fprintf(stderr, "[SCM] thread '%s' ran %llu instructions, pc 0x%X, %s 0x%04X\n", t.name.c_str(),
                             (unsigned long long)t.executed, t.pc, t.finished ? "finished" : "stopped at opcode", t.stoppedAt);
            }
        }
    }
    if (cjReady) {   // [Player] Fat / Muscle in the ini (0..100), GTA_FAT / GTA_MUSCLE override
        float fat = GetPrivateProfileIntA("Player", "Fat", 0, g_iniPath) / 100.0f;
        float mus = GetPrivateProfileIntA("Player", "Muscle", 0, g_iniPath) / 100.0f;
        if (const char* e = getenv("GTA_FAT")) fat = (float)atof(e);
        if (const char* e = getenv("GTA_MUSCLE")) mus = (float)atof(e);
        if (PedClothesDesc d = g_cj.Outfit(); LoadOutfit(d)) g_cj.SetOutfit(d);   // [Outfit]
        {   // [Player] Skin: a picture in skins\<name>.bmp laid over the whole model (CPlayerSkin); GTA_SKIN overrides
            char sk[64] = {};
            const bool has = GetPrivateProfileStringA("Player", "Skin", "", sk, sizeof sk, g_iniPath) && sk[0];
            if (const char* e = getenv("GTA_SKIN")) g_cj.SetSkin(e);
            else if (has) g_cj.SetSkin(sk);
        }
        {   // [Player] StartWeapons = all (default) or none; GTA_NOGUNS=1 forces none
            char sw[16] = {};
            GetPrivateProfileStringA("Player", "StartWeapons", "all", sw, sizeof sw, g_iniPath);
            if (getenv("GTA_NOGUNS") || _stricmp(sw, "none") == 0) g_cj.ClearWeapons();
        }
        g_pickups.Load(g_renderer, GameDir());
        if (const char* e = getenv("GTA_GUN")) g_cj.SelectGun(atoi(e));   // dev: weapon index
        if (fat > 0.0f || mus > 0.0f) g_cj.SetBodyShape(fat, mus);
        Stats::Set(Stats::STAT_FAT, fat * 1000.0f); Stats::Set(Stats::STAT_MUSCLE, mus * 1000.0f);
    }
    Log("PLAYER: CJ model %s", cjReady ? "ready" : "FAILED (using the box)");
    if (g_aimTest) {
        if (cjReady && !getenv("GTA_GUN")) g_cj.SelectGun(0);
        if (!g_dummy.Init(g_renderer, GameDir())) { Log("AIMTEST: dummy model failed"); g_aimTest = false; }
    }

    // ---- Player state (first-person, real human scale) ----
    float playerX = 2505.0f, playerY = -1710.0f, playerZ = 13.7f; // eye
    float camPosX = 2505.0f, camPosY = -1710.0f, camPosZ = 13.7f; // camera world pos (the sky follows it)
    float yaw = 1.5708f;  // radians, facing north (+Y) toward CJ's house
    float pitch = 0.0f;   // radians, positive = look up
    float velZ = 0.0f;    // vertical velocity for jumping
    bool onGround = true;
    bool cjBlocked = false;   // CJ pushed into a wall last frame
    bool thirdPerson = g_startThirdPerson; // V toggles 1st/3rd person camera
    // GTA_SCM=1: run the compiled mission script (main.scm) alongside the game. Skipped commands are listed in the log.
    static ScmMachine s_scm;
    bool scmRun = false;
    double scmClock = 0.0;
    if (getenv("GTA_SCM") && s_scm.Init(GameDir())) {
        scmRun = true;
        s_scm.host.setPlayerPos = [&](float x, float y, float z) {
            playerX = camPosX = x; playerY = camPosY = y; playerZ = camPosZ = z + 1.75f;
            Log("SCM: player placed at %.1f %.1f %.1f", x, y, z);
        };
        s_scm.host.playerPos = [&](float& x, float& y, float& z) { x = playerX; y = playerY; z = playerZ - 1.75f; };
        s_scm.host.addBlip = [](float x, float y, float z, int sprite, bool shortRange) {
            RadarBlip b;
            b.handle = g_nextBlip++; b.x = x; b.y = y; b.z = z; b.sprite = sprite; b.shortRange = shortRange;
            b.display = sprite ? 2 : 3;
            g_blips.push_back(b);
            Log("SCM: blip %d sprite %d at %.1f %.1f %s", b.handle, sprite, x, y, shortRange ? "(short range)" : "");
            return b.handle;
        };
        s_scm.host.giveClothes = [&](const char* tex, const char* model, int part) {
            if (!cjReady) return;
            PedClothesDesc d = g_cj.Outfit();
            d.SetTextureAndModel(tex, model, (ClothesTexturePart)part);
            g_cj.SetOutfit(d);
            Log("SCM: clothes %s / %s on part %d", tex, model, part);
        };
        s_scm.host.wearing = [&](int part, const char* tex) {
            const bool w = cjReady && part >= 0 && part < CLOTHES_TEXTURE_TOTAL && g_cj.Outfit().textureKeys[part] == Clothes::Key(tex);
            Log("SCM: IS_PLAYER_WEARING part %d %s -> %d", part, tex, (int)w);
            return w;
        };
        s_scm.host.storeClothes = [&] { if (cjReady) g_storedOutfit = g_cj.Outfit(); Log("SCM: clothes stored"); };
        s_scm.host.restoreClothes = [&] { if (cjReady) g_cj.SetOutfit(g_storedOutfit); Log("SCM: clothes restored"); };
        if (const char* hex = getenv("GTA_SCMSNIP")) {   // dev: hex bytes run as a script thread, e.g. the clothes opcodes
            std::vector<uint8_t> code;
            for (const char* q = hex; q[0] && q[1]; q += 2) { char b[3] = {q[0], q[1], 0}; code.push_back((uint8_t)strtoul(b, nullptr, 16)); }
            s_scm.LaunchSnippet(code);
        }
        s_scm.host.removeBlip = [](int h) { g_blips.erase(std::remove_if(g_blips.begin(), g_blips.end(), [h](const RadarBlip& b) { return b.handle == h; }), g_blips.end()); };
        s_scm.host.blipDisplay = [](int h, int mode) { for (RadarBlip& b : g_blips) if (b.handle == h) b.display = mode; };
        s_scm.host.setTime = [&](int h, int m) { g_gameSeconds = h * 3600.0f + m * 60.0f; Log("SCM: time of day %02d:%02d", h, m); };
        g_sg.scm = &s_scm; g_sg.peds = &g_peds; g_sg.cars = &g_vehicles; g_sg.r = &g_renderer; g_sg.hud = &g_hud;
        g_sg.wantedLevel = [] { return g_wanted; }; g_sg.setWantedLevel = [](int w) { g_wanted = w; };
        g_sg.pickups = &g_pickups; g_sg.enex = &g_enex;
        g_sg.getArea = [] { return g_world.Area(); };
        g_sg.setArea = [](int a) { Log("SCRIPT: area %d", a); g_world.SetArea(a); g_enex.ForceArea(a); }; g_sg.nowMs = [] { return (uint32_t)g_bulletMs; };
        g_sg.playerInCar = [] { return g_driveCar >= 0; };
        g_sg.playerCar = [] { return g_driveCar; };
        g_sg.putPlayerInCar = [&](int n) {
            if (n < 0 || (size_t)n >= g_vehicles.Count() || g_driveCar == n) return;
            g_driveCar = n; Log("SCRIPT: player into car %d", n); g_radio.Tune(g_radioStation);
            float sp[3]; bool low = false;
            if (cjReady && g_vehicles.SeatWorld((size_t)n, sp, &low)) g_cj.EnterVehicle(low);
        };
        g_sg.playerLeaveCar = [&] {
            if (g_driveCar < 0) return;
            VehicleInstance& c = g_vehicles.Car((size_t)g_driveCar);
            playerX = c.x + std::cos(c.heading) * 2.4f; playerY = c.y + std::sin(c.heading) * 2.4f;
            playerZ = c.z + 1.7f; velZ = 0.0f; onGround = true; c.speed = 0.0f;
            g_driveCar = -1; Log("SCRIPT: player out of the car");
            if (cjReady) g_cj.LeaveVehicle();
            g_audio.EngineStop(); g_radio.Tune(-1);
        };
        g_sg.giveWeapon = [&](int w, int a) { if (cjReady) g_cj.GiveWeapon(w, a); Log("SCRIPT: weapon %d ammo %d", w, a); };
        g_sg.setWeapon = [&](int w) { if (cjReady) { const int i = g_cj.GunIndexOfType(w); g_cj.SelectGun(i); Log("SCRIPT: weapon in hand %d (gun %d)", w, i); } };
        g_sg.gameDir = g_gameDir;
        g_sg.drawCj = [&](D3DRenderer& rr, const AnimClip* c, float t, const float o[3]) { if (cjReady) g_cj.DrawCutscene(rr, c, t, o); };
        g_sg.showPlayer = [&](bool on) { g_cutHideCj = !on; Log("SCRIPT: player %s", on ? "shown" : "hidden"); };
        g_sg.groundAt = [&](float x, float y) { return g_world.GetGroundHeight(x, y, playerZ + 50.0f); };
        s_scm.command = [](ScmMachine::Call& c) { return g_sg.Handle(c); };
    }
    bool prevV = false;       // edge-detect for V key
    const float EYE_HEIGHT = 1.7f;   // CJ eye height in meters
    const float GROUND_Z = 9.0f;     // safety-net grass plane; below lowest retail ground (~9.13)
    const float WALK_SPEED = 5.0f;       // scripted capture paths
    const float CJ_WALK_SPEED = 1.56f;   // WALK_player root travel / duration
    const float CJ_RUN_SPEED = 5.71f;    // run_player
    const float CJ_SPRINT_SPEED = 9.56f; // sprint_civi
    const float JUMP_VEL = 8.5f;     // ~1.6 m jump apex
    const float GRAVITY = 22.0f;
    const float PLAYER_RADIUS = 0.5f;
    const float STEP_OVER = 0.55f;   // COL boxes at/below feet+this are stepped over, not hit
    const float PLAYER_HEIGHT = CJ_HEIGHT; // 1.8132m measured, not assumed

    // ---- Ground height query (Agent 17): feet placement on roads vs grass ----
    // Agent 15's collision-based GetGroundHeight was never implemented; this is
    // the measured-AABB version. Roads sit above the grass plane (cul-de-sac
    // road top ~12.6 vs grass 9.0); without this the player spawns buried in
    // the road. A road surface only counts if within STEP_UP of the current
    // feet height, so walking UNDER the elevated highway never teleports up.
    auto IsRoadObject = [](const MapObject& o) -> bool {
        std::string n = o.name;
        for (auto& c : n) c = (char)tolower((unsigned char)c);
        return n.find("road") != std::string::npos;
    };
    auto IsGroundObject = [](const MapObject& o) -> bool {
        // Retail ground pieces (yard/terrain quads from binary stream IPLs).
        return o.name.rfind("lae2_landhub", 0) == 0 || o.name == "rydbkyar1_lae2";
    };
    auto GetGroundHeight = [&](float x, float y, float feetZ) -> float {
        if (worldReady) {
            float h = g_world.GetGroundHeight(x, y, feetZ);
            if (h > ColLoader::kNoGround * 0.5f) return h;
            // No ground found: stay at current feet height instead of dropping
            // to GROUND_Z (task 1: prevents fall-through on gaps).
            return feetZ;
        }
        // COL3 triangles first: accurate Rockstar road surface (grades/curbs).
        // The step-up cap inside keeps the elevated highway from grabbing the
        // player when walking underneath it.
        float colH = ColLoader::GetGroundHeight(colTris, x, y, feetZ, 1.5f);
        if (colH > ColLoader::kNoGround * 0.5f) return colH;
        // AABB fallback for roads whose COL failed to load; grass plane last.
        float best = GROUND_Z;  // grass plane fallback - never the void
        const float STEP_UP = 1.5f;  // max curb the player can step onto
        for (auto& o : mapObjects) {
            if (!IsRoadObject(o)) continue;
            if (x < o.cMinX || x > o.cMaxX || y < o.cMinY || y > o.cMaxY) continue;
            float top = o.cMaxZ;  // top of the road's collision AABB
            if (top <= feetZ + STEP_UP && top > best) best = top;
        }
        return best;
    };

    if (playMode) {
        Log("PLAY MODE: data-driven world (gta.dat IDE + text/binary IPLs + real COL)");
        worldReady = g_world.Init(img, g_renderer, GameDir());
        Log("WORLD INIT: %s (loaded %zu models dedup, %zu solid boxes)",
            worldReady ? "OK" : "FAIL", g_world.LoadedModelCount(), g_world.SolidBoxCount());
        if (worldReady) {
            g_enex.Load(GameDir());
            g_enex.setArea = [](int a) { g_world.SetArea(a); };
            if (const char* ga = getenv("GTA_AREA")) { g_enex.ForceArea(atoi(ga)); Log("DEV: start in area %d", atoi(ga)); }   // test: begin inside an interior
            g_world.onDynSmash = [](const float* p) { g_fx.Impact(p, 1.0f); g_fx.Smoke(p, 0.3f); g_audio.Impact(0.0f); };
            g_world.onDynExplode = [](const float* p) { g_dynBooms.push_back({p[0], p[1], p[2]}); };
            // Data-driven spawn: stream once so ground COL tris are loaded,
            // then snap to real world ground (init-time cells are empty).
            playerX = 2490.0f; playerY = -1660.0f;
            g_world.Stream(playerX, playerY, GROUND_Z + 5.0f);
            float sg = g_world.GetGroundHeight(playerX, playerY, GROUND_Z + 5.0f);
            if (!(sg > ColLoader::kNoGround * 0.5f) || sg - GROUND_Z > 4.0f) sg = GROUND_Z;
            playerZ = sg + EYE_HEIGHT;
            yaw = -1.4535f; pitch = 0.0f;
            Log("SPAWN (world): (%.1f, %.1f, %.1f) ground=%.2f", playerX, playerY, playerZ, sg);
        } else {
        Log("PLAY MODE: Grove Street cul-de-sac ONLY (4 houses + 3 roads, HD+LOD streaming)");
        Log("PLAY: Rockstar LOD system: HD within lodDist, LOD beyond. COL box collision ON (houses).");

        // The 4 Grove Street cul-de-sac houses. Verified against LAe2.ide:
        //   HD id, HD model, HD TXD, HD drawdist | LOD model, LOD TXD | IPL position + quaternion
        // CJ's house:   17697 carlshou1_LAe2 / contachou1_lae2 / 60m
        // Sweet's:      17698 sweetshou1_LAe2 / contachou1_lae2 / 70m
        // Ryder's:      17573 rydhou01_LAe2   / contachou1_lae2 / 60m
        // Neighbor:     3649 ganghous01_LAx   / ganghouse1_lax  / 80m (north side)
        // ---- Rockstar COL collision boxes (Stage A: houses) ----
        // Minimal COL3 box reader, validated against retail gta3.img:
        // entry = "COL3" + u32 size + char[22] name; 88-byte header at
        // entry+0x20; payload at entry+0x78, sequential: spheres (20B each),
        // boxes (28B each: float min[3], float max[3], 4B material).
        // Only lae2_4.col + laxref.col cover the cul-de-sac scene.
        std::vector<uint8_t> colArcA, colArcB;
        if (img.HasFile("lae2_4.col")) colArcA = img.Extract("lae2_4.col");
        if (img.HasFile("laxref.col")) colArcB = img.Extract("laxref.col");
        Log("COL: lae2_4.col=%u bytes laxref.col=%u bytes",
            (unsigned)colArcA.size(), (unsigned)colArcB.size());
        auto colU16 = [](const std::vector<uint8_t>& d, size_t o) -> uint16_t {
            return (uint16_t)(d[o] | ((uint16_t)d[o+1] << 8));
        };
        auto colF32 = [](const std::vector<uint8_t>& d, size_t o) -> float {
            uint32_t u = (uint32_t)d[o] | ((uint32_t)d[o+1] << 8) |
                         ((uint32_t)d[o+2] << 16) | ((uint32_t)d[o+3] << 24);
            float f; memcpy(&f, &u, 4); return f;
        };
        auto colNameEq = [](const char* a, const std::string& b) -> bool {
            for (int i = 0; i < 22; i++) {
                char ca = a[i], cb = i < (int)b.size() ? b[i] : 0;
                if (ca == 0 && cb == 0) return true;
                if (tolower((unsigned char)ca) != tolower((unsigned char)cb)) return false;
                if (ca == 0 || cb == 0) return false;
            }
            return true;
        };
        // Find a COL3 entry offset by model name (case-insensitive). -1 if absent.
        auto findColEntry = [&](const std::vector<uint8_t>& col, const std::string& model) -> int {
            size_t pos = 0;
            while (pos + 8 < col.size()) {
                size_t m = pos;
                while (m + 4 < col.size() &&
                       !(col[m]=='C' && col[m+1]=='O' && col[m+2]=='L' && col[m+3]=='3')) m++;
                if (m + 30 > col.size()) break;
                pos = m;
                uint32_t sz = (uint32_t)col[pos+4] | ((uint32_t)col[pos+5] << 8) |
                              ((uint32_t)col[pos+6] << 16) | ((uint32_t)col[pos+7] << 24);
                if (colNameEq((const char*)col.data() + pos + 8, model)) return (int)pos;
                if (sz > 1000000 || sz < 88) break; // corrupt, bail
                pos = pos + 8 + sz;
            }
            return -1;
        };
        // Read model-space COL boxes for a model. Returns count (0 = none found).
        auto readColBoxes = [&](const std::string& model, std::vector<ColBox>& out) -> int {
            const std::vector<uint8_t>* arcs[2] = { &colArcA, &colArcB };
            for (int ai = 0; ai < 2; ai++) {
                const std::vector<uint8_t>& col = *arcs[ai];
                if (col.empty()) continue;
                int e = findColEntry(col, model);
                if (e < 0) continue;
                if ((size_t)e + 0x78 > col.size()) continue;
                uint16_t ns = colU16(col, e + 0x48), nb = colU16(col, e + 0x4A);
                if (nb == 0 || nb > 256) return 0;
                size_t base = (size_t)e + 0x78 + (size_t)ns * 20;
                for (int i = 0; i < nb; i++) {
                    size_t p = base + (size_t)i * 28;
                    if (p + 24 > col.size()) break;
                    float mnx = colF32(col, p),    mny = colF32(col, p+4),  mnz = colF32(col, p+8);
                    float mxx = colF32(col, p+12), mxy = colF32(col, p+16), mxz = colF32(col, p+20);
                    if (!(mnx <= mxx && mny <= mxy && mnz <= mxz)) continue;
                    if (!(mnx > -500 && mxx < 500 && mny > -500 && mxy < 500 &&
                          mnz > -500 && mxz < 500)) continue;
                    out.push_back({mnx, mny, mnz, mxx, mxy, mxz});
                }
                return (int)out.size();
            }
            return 0;
        };
        // Transform model-space COL boxes by the object's placement matrix and
        // attach as world-space collision volumes. Marks the object solid.
        auto attachColBoxes = [&](MapObject& obj, const char* modelName, const char* tag) -> int {
            std::vector<ColBox> cboxes;
            int ncb = readColBoxes(modelName, cboxes);
            if (ncb > 0) {
                for (auto& b : cboxes) {
                    float wmnx=1e30f, wmny=1e30f, wmnz=1e30f, wmxx=-1e30f, wmxy=-1e30f, wmxz=-1e30f;
                    for (int cxi=0;cxi<2;cxi++) for (int cyi=0;cyi<2;cyi++) for (int czi=0;czi<2;czi++) {
                        float px=cxi?b.maxX:b.minX, py=cyi?b.maxY:b.minY,
                              pz=czi?b.maxZ:b.minZ, ox, oy, oz;
                        xformPt(obj.worldMatrix, px, py, pz, ox, oy, oz);
                        if (ox<wmnx) wmnx=ox; if (ox>wmxx) wmxx=ox;
                        if (oy<wmny) wmny=oy; if (oy>wmxy) wmxy=oy;
                        if (oz<wmnz) wmnz=oz; if (oz>wmxz) wmxz=oz;
                    }
                    obj.colBoxes.push_back({wmnx, wmny, wmnz, wmxx, wmxy, wmxz});
                }
                obj.solid = true;
                Log("  HOUSECOL OK: %s %d boxes", tag, ncb);
            } else {
                Log("  HOUSECOL WARN: %s no COL boxes (walk-through)", tag);
            }
            return ncb;
        };

        struct CulHouse {
            const char* name;      // display name
            const char* hdDff;     // HD model (no extension)
            const char* hdTxd;     // HD texture dictionary
            float hdDist;          // HD draw distance from IDE = LOD switch threshold
            const char* lodDff;    // LOD model (no extension)
            const char* lodTxd;    // LOD texture dictionary
            float x, y, z;         // IPL placement (real game coordinates)
            float qx, qy, qz, qw;  // IPL quaternion (real game rotation)
            float baseZ;           // world base Z = IPL z + DFF bbox minZ (measured, grove_heights.txt)
        };
        CulHouse houses[] = {
            // CJ's (Johnson) house - north end of cul-de-sac, faces south
            {"CJ_HOUSE", "carlshou1_LAe2", "contachou1_lae2", 60.0f,
             "LOD1carlshou1_LAe", "laeast2_lod",
             2494.265625f, -1696.210938f, 17.0546875f,
             0.0f, 0.0f, -1.0f, -4.371138829e-008f, 12.416f},
            // Sweet's house - east side
            {"SWEET_HOUSE", "sweetshou1_LAe2", "contachou1_lae2", 70.0f,
             "LOD1swetho1_LAe", "laeast2_lod",
             2529.890625f, -1677.664063f, 16.7265625f,
             0.0f, 0.0f, 0.0f, 1.0f, 13.829f},
            // Ryder's house - west side
            {"RYDER_HOUSE", "rydhou01_LAe2", "contachou1_lae2", 60.0f,
             "LODrydhou_LAe2", "laeast2_lod",
             2457.835938f, -1695.9375f, 14.2890625f,
             0.0f, 0.0f, 0.7071068287f, 0.7071067095f, 12.501f},
            // Neighbor house (ganghous01_LAx) - north side, real IPL placement
            // HD: 3649 ganghous01_LAx / ganghouse1_lax / 80m (LAxref.ide)
            {"NEIGHBOR_HOUSE", "ganghous01_LAx", "ganghouse1_lax", 80.0f,
             "LODganghous01_LAx", "gangholod1_lax",
             2517.476563f, -1644.695313f, 15.1953125f,
             0.0f, 0.0f, -0.3826832771f, 0.9238796234f, 11.360f},
        };

        // Helper: load a DFF+TXD into render meshes, with texture dimension logging.
        // Returns meshes via out param. Logs DFF stats and texture sizes.
        auto loadMeshes = [&](const std::string& dffBase, const std::string& txdName,
                              const char* tag,
                              std::vector<D3DRenderMesh*>& outMeshes,
                              float* outBounds = nullptr) -> bool {
            std::string df = dffBase + ".dff";
            if (!img.HasFile(df)) { Log("  %s SKIP: DFF not in IMG: %s", tag, df.c_str()); return false; }
            std::vector<uint8_t> dffData = img.Extract(df);
            if (dffData.empty()) { Log("  %s FAIL: extract %s", tag, df.c_str()); return false; }
            DffModel dff = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
            if (!dff.valid) { Log("  %s FAIL: parse %s", tag, df.c_str()); return false; }
            size_t totalVerts = 0, totalTris = 0;
            for (auto& dm : dff.meshes) { totalVerts += dm.vertices.size(); totalTris += dm.indices.size() / 3; }
            if (outBounds) {
                // Model-space bounds (min xyz, max xyz) for world AABB computation.
                outBounds[0]=outBounds[1]=outBounds[2]=1e30f;
                outBounds[3]=outBounds[4]=outBounds[5]=-1e30f;
                for (auto& dm : dff.meshes)
                    for (auto& v : dm.vertices) {
                        if (v.x<outBounds[0]) outBounds[0]=v.x;
                        if (v.y<outBounds[1]) outBounds[1]=v.y;
                        if (v.z<outBounds[2]) outBounds[2]=v.z;
                        if (v.x>outBounds[3]) outBounds[3]=v.x;
                        if (v.y>outBounds[4]) outBounds[4]=v.y;
                        if (v.z>outBounds[5]) outBounds[5]=v.z;
                    }
            }
            ensureTxd(txdName);
            // Log texture dimensions for this TXD (detect low-res).
            Log("  %s: dff=%s verts=%u tris=%u txd=%s", tag, df.c_str(),
                (unsigned)totalVerts, (unsigned)totalTris, txdName.c_str());
            for (auto& dm : dff.meshes) {
                const MeshVertex* verts = reinterpret_cast<const MeshVertex*>(dm.vertices.data());
                D3DRenderMesh* rm = g_renderer.CreateMesh(
                    verts, (unsigned)dm.vertices.size(),
                    dm.indices.data(), (unsigned)dm.indices.size(), dm.hasPrelit,
                    dm.nightColors.empty() ? nullptr : dm.nightColors.data());
                if (rm && !dm.textureName.empty()) rm->texture = getD3dTexture(dm.textureName);
                if (rm) outMeshes.push_back(rm);
                else Log("  %s WARN: mesh create failed (tex=%s)", tag, dm.textureName.c_str());
            }
            return !outMeshes.empty();
        };

        int loaded = 0;
        for (auto& h : houses) {
            MapObject obj;
            obj.name = h.name;
            obj.solid = false;  // attachColBoxes() below enables real COL collision when boxes exist
            float placeZ = h.z;      // RESTORED: raw IPL Z (Rockstar authored, grove_heights.txt)
            // h.q is a raw IPL quat (hardcoded literal, bypasses the IPL
            // loaders) -> conjugate to match streamed buildings. See BinaryIpl.cpp.
            obj.worldMatrix = QuatToD3DMatrix(-h.qx, -h.qy, -h.qz, h.qw, h.x, h.y, placeZ);
            obj.objX = h.x; obj.objY = h.y; obj.objZ = placeZ;
            obj.lodDist = h.hdDist;
            // Load HD meshes.
            bool hdOk = loadMeshes(h.hdDff, h.hdTxd, h.name, obj.meshes);
            // Load LOD meshes.
            bool lodOk = loadMeshes(h.lodDff, h.lodTxd, h.name, obj.lodMeshes);
            obj.hasLod = lodOk;
            if (hdOk) {
                attachColBoxes(obj, h.hdDff, h.name);
                mapObjects.push_back(std::move(obj));
                loaded++;
                Log("  HOUSE OK: %s base=%.2f ground=%.2f HD=%s",
                    h.name, h.baseZ, GROUND_Z, h.hdDff);
            } else {
                Log("  HOUSE FAIL: %s (HD missing)", h.name);
            }
        }
        Log("PLAY: %d/4 cul-de-sac houses loaded (collision ON, LOD streaming ON)", loaded);

        // Ring houses + strip mall + pawn shop - HD instances from binary stream IPLs
        // (lae2_stream0/2.ipl in gta3.img). The full cul-de-sac circle per Q's
        // reference screenshots. Positions = stream HD instances (preferred over
        // text-IPL LOD positions). baseZ measured from DFF bbox (see grove_heights).
        CulHouse ringHouses[] = {
            // compfukhouse3 #1 - west side of circle
            // HD: 3589 compfukhouse3 / comedhos1_la / 80m (LAxref.ide)
            {"RING_HOUSE_W", "compfukhouse3", "comedhos1_la", 80.0f,
             "LODpfukhouse3", "gangholod1_lax",
             2451.7344f, -1637.4844f, 15.1328f,
             0.0f, 0.0f, -1.0f, 0.0f, 12.405f},
            // compfukhouse3 #2 - north side of circle
            {"RING_HOUSE_N", "compfukhouse3", "comedhos1_la", 80.0f,
             "LODpfukhouse3", "gangholod1_lax",
             2498.3047f, -1638.3281f, 15.1797f,
             0.0f, 0.0f, -1.0f, 0.0f, 12.452f},
            // compfukhouse3 #3 - east side of circle
            {"RING_HOUSE_E", "compfukhouse3", "comedhos1_la", 80.0f,
             "LODpfukhouse3", "gangholod1_lax",
             2528.6328f, -1658.4453f, 16.8906f,
             0.0f, 0.0f, -0.7071f, 0.7071f, 14.163f},
            // ganghous02_LAx - north-west of circle (stream HD instance)
            // HD: 3648 ganghous02_LAx / ganghouse1_lax / 80m (LAxref.ide)
            {"GANG_HOUSE_2", "ganghous02_LAx", "ganghouse1_lax", 80.0f,
             "LODganghous02_LAx", "gangholod1_lax",
             2470.8203f, -1640.8203f, 15.0234f,
             0.0f, 0.0f, -0.7071f, 0.7071f, 12.280f},
            // ganghous05_LAx - south-east, near Sweet's (stream HD instance)
            // HD: 3646 ganghous05_LAx / ganghouse1_lax / 80m (LAxref.ide)
            {"GANG_HOUSE_5", "ganghous05_LAx", "ganghouse1_lax", 80.0f,
             "LODgnghos05_LAx", "gangholod1_lax",
             2520.1875f, -1694.8516f, 14.8828f,
             0.0f, 0.0f, 0.3420f, 0.9397f, 11.046f},
            // Strip mall - north side of circle (stream HD instance)
            // HD: 17699 mcstraps_LAe2 / contachou1_lae2 / 70m (LAe2.ide)
            {"STRIP_MALL", "mcstraps_LAe2", "contachou1_lae2", 70.0f,
             "LODmcstraps_LAe2", "laeast2_lod",
             2485.9062f, -1639.3281f, 16.9531f,
             0.0f, 0.0f, 0.7071f, 0.7071f, 12.314f},
            // Pawn shop - south of circle (stream HD instance)
            // HD: 17521 Pawnshp_lae2 / lae2newtempbx / 60m (LAe2.ide)
            {"PAWN_SHOP", "Pawnshp_lae2", "lae2newtempbx", 60.0f,
             "LODPwnshp_lae2", "laeast2_lod",
             2502.0156f, -1714.2031f, 16.0234f,
             0.0f, 0.0f, 0.0f, 1.0f, 12.520f},
        };
        int ringLoaded = 0;
        for (auto& h : ringHouses) {
            MapObject obj;
            obj.name = h.name;
            obj.solid = false;  // attachColBoxes() below enables real COL collision when boxes exist
            float placeZ = h.z;      // raw stream-IPL Z (Rockstar authored)
            // Raw IPL quat (hardcoded literal) -> conjugate. See BinaryIpl.cpp.
            obj.worldMatrix = QuatToD3DMatrix(-h.qx, -h.qy, -h.qz, h.qw, h.x, h.y, placeZ);
            obj.objX = h.x; obj.objY = h.y; obj.objZ = placeZ;
            obj.lodDist = h.hdDist;
            bool hdOk = loadMeshes(h.hdDff, h.hdTxd, h.name, obj.meshes);
            bool lodOk = loadMeshes(h.lodDff, h.lodTxd, h.name, obj.lodMeshes);
            obj.hasLod = lodOk;
            if (hdOk) {
                attachColBoxes(obj, h.hdDff, h.name);
                mapObjects.push_back(std::move(obj));
                ringLoaded++;
                Log("  BLDG OK: %s base=%.2f ground=%.2f HD=%s",
                    h.name, h.baseZ, GROUND_Z, h.hdDff);
            } else {
                Log("  BLDG FAIL: %s (HD missing)", h.name);
            }
        }
        Log("PLAY: %d/7 ring buildings loaded (collision ON, LOD streaming ON)", ringLoaded);


        // Grove Street roads - real IPL placements, scoped to cul-de-sac box.
        // Roads are IPL inst models with bIsRoad flag (IDE objs field 5 = 1).
        // Real engine renders them via CRenderer::RenderRoads (ambient-only lighting).
        // HD models resolve via IDE (IPL only places LOD versions).
        struct GroveRoad {
            const char* tag; const char* hdDff; const char* hdTxd;
            const char* lodDff; const char* lodTxd; float lodDist;
            float x, y, z;
        };
        GroveRoad roads[] = {
            // THE cul-de-sac road - runs north-south through the houses
            // HD: 17613 Lae2_roads89 / lae2roadshub / 150m / flags=1 (IsRoad)
            {"CULDESAC_ROAD", "Lae2_roads89", "lae2roadshub",
             "LODLae2_roads89", "laeast2_lod", 150.0f,
             2489.296875f, -1668.5f, 12.296875f},
            // North street connection
            // HD: 17655 Lae2_roads46 / lae2roads / 150m / flags=1 (IsRoad)
            {"ROAD_NORTH", "Lae2_roads46", "lae2roads",
             "LODLae2_roads46", "laeast2_lod", 150.0f,
             2433.070313f, -1611.554688f, 12.03125f},
            // Elevated highway west of cul-de-sac (the highway Q mentioned)
            // HD: 17656 Lae2_roads50 / lae2roads / 150m / flags=1 (IsRoad)
            {"HIGHWAY_WEST", "Lae2_roads50", "lae2roads",
             "LODLae2_roads50", "laeast2_lod", 150.0f,
             2431.054688f, -1677.429688f, 20.3125f},
            // East street connection (2 eastern lamppost2s plant on this road)
            // HD: 17654 Lae2_roads44 / lae2roads / 150m / flags=1 (IsRoad)
            // Binary stream lae2_stream0.ipl: (2556.35, -1612.91, 15.91)
            // No LOD DFF in retail IMG -> HD only (hasLod=false).
            {"ROAD_EAST", "Lae2_roads44", "lae2roads",
             "LODLae2_roads44", "laeast2_lod", 150.0f,
             2556.351563f, -1612.914063f, 15.90625f},
        };
        int roadsLoaded = 0;
        for (auto& r : roads) {
            MapObject road;
            road.name = r.tag;
            road.solid = false;  // walk-through, no collision
            // Roads keep their real IPL Z (highway is elevated at z~20).
            road.worldMatrix = QuatToD3DMatrix(0.0f, 0.0f, 0.0f, 1.0f, r.x, r.y, r.z);
            road.objX = r.x; road.objY = r.y; road.objZ = r.z;
            road.lodDist = r.lodDist;
            bool rhd = loadMeshes(r.hdDff, r.hdTxd, r.tag, road.meshes);
            bool rlod = loadMeshes(r.lodDff, r.lodTxd, r.tag, road.lodMeshes);
            road.hasLod = rlod;
            if (rhd) {
                mapObjects.push_back(std::move(road));
                roadsLoaded++;
                Log("  ROAD OK: %s at (%.2f, %.2f, %.2f)", r.hdDff, r.x, r.y, r.z);
            } else {
                Log("  ROAD FAIL: %s HD missing", r.hdDff);
            }
        }
        Log("PLAY: %d/4 Grove Street roads loaded (collision ON)", roadsLoaded);

        // ---- COL3 ground triangles (Agent 15): Rockstar road collision ----
        // Accurate surface (grades/curbs) from lae2_4.col, transformed by the
        // same IPL placement matrix as the visual meshes (identity rotation +
        // translation for these roads). Reuses colArcA extracted above.
        {
            struct RoadCol { const char* model; float x, y, z; };
            RoadCol rcs[] = {
                {"Lae2_roads89", 2489.296875f, -1668.5f, 12.296875f},
                {"Lae2_roads46", 2433.070313f, -1611.554688f, 12.03125f},
                {"Lae2_roads50", 2431.054688f, -1677.429688f, 20.3125f},
                {"Lae2_roads44", 2556.351563f, -1612.914063f, 15.90625f},
            };
            int colRoads = 0;
            for (auto& rc : rcs) {
                std::vector<ColTriangle> tris;
                if (!colArcA.empty() && ColLoader::LoadModelTriangles(
                        colArcA.data(), colArcA.size(), rc.model,
                        0.0f, 0.0f, 0.0f, 1.0f, rc.x, rc.y, rc.z, tris)) {
                    colTris.insert(colTris.end(), tris.begin(), tris.end());
                    colRoads++;
                    Log("  COL OK: %s tris=%u", rc.model, (unsigned)tris.size());
                } else {
                    Log("  COL FAIL: %s (no COL entry)", rc.model);
                }
            }
            Log("COL: %d/3 road models, %u ground triangles", colRoads, (unsigned)colTris.size());
        }

        // ---- Binary stream IPL: Grove Street props & vegetation ----
        // Rockstar streams HD instances + props via binary IPLs in gta3.img
        // (lae2_stream*.ipl). The text IPLs only hold LOD placeholders.
        // This loads the real prop/vegetation placements: palms, trees,
        // bushes, grass, street lamps, hydrants, poles, fences, trash.
        Log("STREAM: loading binary IPL props/vegetation...");
        {
            // Build model-ID -> (modelName, txdName) from IDEs.
            // Prop TXDs live in vegepart (vegetation), dynamic/dynamic2
            // (lamps, hydrants, poles), barriers (fences).
            const char* ideFiles[] = {
                "data\\maps\\generic\\vegepart.ide",
                "data\\maps\\generic\\barriers.ide",
                "data\\maps\\generic\\dynamic.ide",
                "data\\maps\\generic\\dynamic2.ide",
                "data\\maps\\generic\\multiobj.ide",
                "data\\maps\\generic\\procobj.ide",
                "data\\maps\\LA\\LAe2.ide",
                "data\\maps\\LA\\LAxref.ide",
                "data\\maps\\interior\\int_LA.ide",
                "data\\maps\\interior\\propext.ide",
            };
            std::unordered_map<int, IdeObject> ideById;
            for (auto ideRel : ideFiles) {
                std::vector<IdeObject> ideObjs;
                std::unordered_map<int, size_t> ideIdx;
                if (IdeLoader::Load(FindGameFile(ideRel), ideObjs, ideIdx)) {
                    for (auto& o : ideObjs) ideById[o.id] = o;
                } else {
                    Log("STREAM: IDE not found: %s", ideRel);
                }
            }
            Log("STREAM: IDE table: %u model IDs", (unsigned)ideById.size());

            // Category matchers (IDE model names are lowercase).
            // Retail ground: yard/terrain quads for the cul-de-sac hub
            // (hunt 2026-10-10: lae2_stream0/2 in-box scan; COL in lae2_4.col).
            auto isGround = [](const std::string& n) -> bool {
                return n.rfind("lae2_landhub", 0) == 0 || n == "rydbkyar1_lae2";
            };
            auto isVegetation = [](const std::string& n) -> bool {
                return n.rfind("veg_", 0) == 0 || n.rfind("sm_veg", 0) == 0 ||
                       n.rfind("sm_bush", 0) == 0 || n == "new_bushtest" ||
                       n.find("grass") != std::string::npos;
            };
            auto isLamp = [](const std::string& n) -> bool {
                return n.find("lamppost") != std::string::npos ||
                       n.find("streetlamp") != std::string::npos;
            };
            auto isProp = [](const std::string& n) -> bool {
                return n.find("fire_hydrant") != std::string::npos ||
                       n.find("telgrphpole") != std::string::npos ||
                       n.find("trafficlight") != std::string::npos ||
                       n.find("blackbag") != std::string::npos ||
                       n.find("cardboardbox") != std::string::npos ||
                       n.find("dyn_f_") != std::string::npos ||
                       n.find("dyn_mesh") != std::string::npos ||
                       n.find("bskball") != std::string::npos ||
                       // House/garage doors (retail static placements, closed)
                       n.find("door") != std::string::npos ||
                       // Entry facades (3D surrounds, not decals)
                       n.find("faux") != std::string::npos ||
                       // Decals: graffiti, bullet holes, alpha ground blends
                       n.find("graff") != std::string::npos ||
                       n.find("ryder_holes") != std::string::npos ||
                       n.find("hubst4alpha") != std::string::npos ||
                       n.find("hub_grnd_alpha") != std::string::npos ||
                       // Misc retail in-box props
                       n.find("hubridge_smash") != std::string::npos ||
                       n.find("starthootra1_lae") != std::string::npos ||
                       // CJ's garage building (despite the name: brick/wall
                       // textures, not vegetation; binary stream placement)
                       n == "cjsaveg";
            };
            auto isAlreadyPlaced = [](const std::string& n) -> bool {
                // Houses, ring buildings, and roads are placed by the
                // hardcoded blocks above; don't duplicate them.
                static const char* placed[] = {
                    "carlshou1_lae2", "sweetshou1_lae2", "rydhou01_lae2",
                    "ganghous01_lax", "compfukhouse3", "ganghous02_lax",
                    "ganghous05_lax", "mcstraps_lae2", "pawnshp_lae2",
                    "lae2_roads89", "lae2_roads46", "lae2_roads50",
                    "lae2_roads44",
                };
                for (auto p : placed) if (n == p) return true;
                return false;
            };
            auto skipReasonFor = [](const std::string& n, std::string& reason) -> bool {
                // Doors/faux/decals are placed statically (closed/as-authored).
                if (n.find("fuckcar") != std::string::npos) { reason = "needs vehicle code"; return true; }
                return false;
            };

            const char* streamFiles[] = { "lae2_stream0.ipl", "lae2_stream2.ipl" };
            int vegCount = 0, lampCount = 0, propCount = 0, groundCount = 0, skipCount = 0;
            for (auto sf : streamFiles) {
                if (!img.HasFile(sf)) { Log("STREAM: not in IMG: %s", sf); continue; }
                std::vector<uint8_t> iplData = img.Extract(sf);
                if (iplData.empty()) { Log("STREAM: extract failed: %s", sf); continue; }
                std::vector<BinIplInstance> insts;
                if (!BinaryIplLoader::LoadFromMemory(iplData.data(), iplData.size(), insts)) {
                    Log("STREAM: parse failed: %s", sf);
                    continue;
                }
                Log("STREAM: %s: %u instances", sf, (unsigned)insts.size());
                for (auto& in : insts) {
                    // Cul-de-sac box.
                    if (in.x < 2400.0f || in.x > 2600.0f ||
                        in.y < -1760.0f || in.y > -1590.0f)
                        continue;
                    auto it = ideById.find(in.modelId);
                    if (it == ideById.end()) {
                        Log("  STREAM SKIP: unknown model ID %d at (%.1f, %.1f, %.1f)",
                            in.modelId, in.x, in.y, in.z);
                        skipCount++;
                        continue;
                    }
                    const std::string& mname = it->second.modelName;
                    const std::string& txd = it->second.txdName;
                    if (isAlreadyPlaced(mname))
                        continue;  // houses/roads placed above; silent
                    std::string reason;
                    if (skipReasonFor(mname, reason)) {
                        Log("  STREAM SKIP: %s at (%.1f, %.1f, %.1f) (%s)",
                            mname.c_str(), in.x, in.y, in.z, reason.c_str());
                        skipCount++;
                        continue;
                    }
                    bool ground = isGround(mname);
                    bool veg = !ground && isVegetation(mname);
                    bool lamp = !ground && !veg && isLamp(mname);
                    bool prop = !ground && !veg && !lamp && isProp(mname);
                    if (!veg && !lamp && !prop && !ground) {
                        Log("  STREAM SKIP: %s at (%.1f, %.1f, %.1f) (out of scope)",
                            mname.c_str(), in.x, in.y, in.z);
                        skipCount++;
                        continue;
                    }
                    // Place it. solid=false (no collision pass yet).
                    // NOTE: interior is logged, not filtered: Rockstar's data
                    // uses interior 256/512 for some exterior bushes/poles.
                    MapObject obj;
                    obj.name = mname;
                    obj.solid = false;
                    obj.worldMatrix = QuatToD3DMatrix(
                        in.qx, in.qy, in.qz, in.qw, in.x, in.y, in.z);
                    obj.objX = in.x; obj.objY = in.y; obj.objZ = in.z;
                    obj.lodDist = it->second.drawDistance;
                    obj.hasLod = false;  // props: HD only, always drawn
                    const char* tag = ground ? "GROUND" : (veg ? "VEG" : (lamp ? "LAMP" : "PROP"));
                    std::vector<D3DRenderMesh*> meshes;
                    float gb[6];  // model-space bounds for the ground world AABB
                    if (loadMeshes(mname, txd, tag, meshes, ground ? gb : nullptr)) {
                        if (ground) {
                            // World-space AABB from the 8 model-space corners.
                            float wmnx=1e30f, wmny=1e30f, wmnz=1e30f;
                            float wmxx=-1e30f, wmxy=-1e30f, wmxz=-1e30f;
                            for (int cxi=0;cxi<2;cxi++) for (int cyi=0;cyi<2;cyi++) for (int czi=0;czi<2;czi++) {
                                float px=cxi?gb[3]:gb[0], py=cyi?gb[4]:gb[1], pz=czi?gb[5]:gb[2], ox, oy, oz;
                                xformPt(obj.worldMatrix, px, py, pz, ox, oy, oz);
                                if (ox<wmnx) wmnx=ox; if (ox>wmxx) wmxx=ox;
                                if (oy<wmny) wmny=oy; if (oy>wmxy) wmxy=oy;
                                if (oz<wmnz) wmnz=oz; if (oz>wmxz) wmxz=oz;
                            }
                            obj.cMinX=wmnx; obj.cMinY=wmny; obj.cMinZ=wmnz;
                            obj.cMaxX=wmxx; obj.cMaxY=wmxy; obj.cMaxZ=wmxz;
                            // Walkable COL triangles from lae2_4.col with the same
                            // placement matrix as the visual mesh. GetGroundHeight
                            // queries colTris first, so feet follow the real surface.
                            if (!colArcA.empty()) {
                                std::vector<ColTriangle> gtris;
                                if (ColLoader::LoadModelTriangles(colArcA.data(), colArcA.size(),
                                        mname.c_str(), in.qx, in.qy, in.qz, in.qw,
                                        in.x, in.y, in.z, gtris)) {
                                    Log("  GROUND COL: %s tris=%u", mname.c_str(), (unsigned)gtris.size());
                                    colTris.insert(colTris.end(), gtris.begin(), gtris.end());
                                } else {
                                    Log("  GROUND COL FAIL: %s (no COL entry)", mname.c_str());
                                }
                            }
                        }
                        obj.meshes = std::move(meshes);
                        mapObjects.push_back(std::move(obj));
                        if (veg) vegCount++; else if (lamp) lampCount++; else if (prop) propCount++; else groundCount++;
                        Log("  %s OK: %s at (%.2f, %.2f, %.2f) interior=%d txd=%s",
                            tag, mname.c_str(), in.x, in.y, in.z,
                            in.AreaCode(), txd.c_str());
                    } else {
                        Log("  %s FAIL: %s (DFF load failed)", tag, mname.c_str());
                        skipCount++;
                    }
                }
            }
            Log("STREAM: placed VEG=%d LAMP=%d PROP=%d GROUND=%d SKIP=%d",
                vegCount, lampCount, propCount, groundCount, skipCount);
            Log("COL: %u total ground triangles (roads + retail ground)", (unsigned)colTris.size());
        }
        // Honest "(N solid)" overlay: count collision-enabled objects.
        solidCount = 0;
        for (auto& o : mapObjects) if (o.solid) solidCount++;
        Log("PLAY: %d/%u objects solid (collision ON)", solidCount, (unsigned)mapObjects.size());

        // Player starts on the cul-de-sac, looking north toward CJ's house.
        // Cul-de-sac center ~ (2490, -1685). Start south of houses, clear of everything.
        playerX = 2490.0f; playerY = -1660.0f;
        // Snap spawn feet to the real surface: the cul-de-sac road sits ~1.3m
        // above the grass plane, so spawning at GROUND_Z buries the player.
        // Only snap UP to nearby surfaces (never down to the void, never up
        // to the elevated highway if spawning underneath it).
        float spawnGround = GROUND_Z;
        for (auto& o : mapObjects) {
            if (!IsRoadObject(o) && !IsGroundObject(o)) continue;
            if (playerX < o.cMinX || playerX > o.cMaxX ||
                playerY < o.cMinY || playerY > o.cMaxY) continue;
            if (o.cMaxZ > spawnGround && o.cMaxZ < spawnGround + 3.0f)
                spawnGround = o.cMaxZ;
        }
        playerZ = spawnGround + EYE_HEIGHT;
        yaw = -1.4535f; pitch = 0.0f;  // face CJ's house (dir ~ -Y from spawn)
        Log("SPAWN: (%.1f, %.1f, %.1f) ground=%.2f facing north to CJ's house",
            playerX, playerY, playerZ, spawnGround);
        Log("CJ_HEIGHT: %.4f m (measured from player.img part DFF bind-pose verts)", CJ_HEIGHT);
        }  // worldReady ? data-driven world : legacy cul-de-sac scene
        // Hide cursor for mouse look
        SetPointerCapture(true);
        // Center mouse
        RECT rc; GetClientRect(g_hwnd, &rc);
        POINT c = {(rc.right-rc.left)/2, (rc.bottom-rc.top)/2};
        ClientToScreen(g_hwnd, &c);
        if (MouseMode() == 0) SetCursorPos(c.x, c.y);   // a warp locks the pointer under Wayland
    }

    IDirect3DDevice9* dev = g_renderer.GetDevice();
    D3DMATRIX proj = MatrixPerspectiveFov(g_fovDeg*3.14159f/180.0f, (float)WIDTH/(float)HEIGHT, 0.5f, 3000.0f);
    g_renderer.SetProjMatrix(proj);

    Log("Entering render loop");
    int frame = 0;
    // Frame dt comes from QueryPerformanceCounter: GetTickCount only ticks
    // every ~15.6 ms, so at high FPS dt alternated between 0 and 15.6 ms and
    // movement/camera visibly juddered.
    LARGE_INTEGER qpcFreq, qpcLast;
    QueryPerformanceFrequency(&qpcFreq);
    QueryPerformanceCounter(&qpcLast);
    int fpsFrames = 0;
    float fps = 0.0f;
    char titleBuf[256];

    // Mouse look state (disabled when the camera is pinned by --yaw/--pitch)
    bool camLock = hasYawOverride || hasPitchOverride;
    bool mouseLook = playMode && !scripted && !flyover && !camLock;
    float scriptedTime = 0.0f;
    int lastScriptedSeg = -1;
    bool takeScriptedShot = false;
    int scriptedShotIdx = 0;
    if (scripted) Log("SCRIPTED MODE: auto camera path, shots every 4s");
    // Flyover: circular path around Grove St center
    float flyoverTime = 0.0f;
    int lastFlyoverShot = -1;
    bool takeFlyoverShot = false;
    int flyoverShotIdx = 0;
    const float FLY_CX = 2500.0f, FLY_CY = -1680.0f, FLY_R = 100.0f, FLY_H = 30.0f;
    if (flyover) Log("FLYOVER MODE: circular path r=100 h=30, 12 shots/loop, 2 loops");
    const float MOUSE_SENS = 0.0035f;

    // Headless verification overrides: place/orient the camera deterministically
    if (playMode) {
        if (hasPosOverride) { playerX = camPosOverrideX; playerY = camPosOverrideY; playerZ = camPosOverrideZ; }
        if (hasYawOverride) yaw = camYawOverride;
        if (hasPitchOverride) pitch = camPitchOverride;
        if (hasPosOverride) {
            g_world.Stream(playerX, playerY, playerZ);
            float og = g_world.GetGroundHeight(playerX, playerY, playerZ + 2.0f);
            Log("CAM OVERRIDE pos=(%.1f,%.1f,%.1f) yaw=%.1f pitch=%.1f ground=%.2f",
                playerX, playerY, playerZ, yaw * 57.29578f, pitch * 57.29578f, og);
        }
    }
    // Dev: GTA_CLIMBSWEEP=<radius> lists every ledge the climb scan finds around the start (headless test aid).
    if (const char* sw = getenv("GTA_CLIMBSWEEP")) {
        const float rad = (float)atof(sw);
        for (int i = 0; i < 300; i++) g_world.Stream(playerX, playerY, playerZ);   // streaming is incremental
        for (float x = playerX - rad; x <= playerX + rad; x += 1.0f)
            for (float y = playerY - rad; y <= playerY + rad; y += 1.0f) {
                const float gz = g_world.GetGroundHeight(x, y, playerZ + 2.0f);
                for (int k = 0; k < 16; k++) {
                    const float rot = k * 0.3927f, ped[3] = {x, y, gz + 1.0f};
                    float hold[3], hr = rot;
                    if (!ClimbScan(g_world, getenv("GTA_CLIMBSWEEPMODE") ? atoi(getenv("GTA_CLIMBSWEEPMODE")) : 1, ped, rot, hold, &hr)) continue;
                    const float p2[3] = {hold[0] + sinf(hr) * 0.4f, hold[1] - cosf(hr) * 0.4f, hold[2]};
                    float d[3], dr = hr;
                    const bool room = !ClimbScan(g_world, 2, p2, hr, d, &dr);
                    const bool vault = !ClimbScan(g_world, 3, p2, hr, d, &dr);
                    Log("SWEEP %.1f %.1f feet=%.2f rot=%.0f hold=%.2f rel=%.2f room=%d vault=%d hx=%.2f hy=%.2f hr=%.1f", x, y, gz, rot * 57.29578f, hold[2], hold[2] - gz, room, vault, hold[0], hold[1], hr * 57.29578f);
                }
            }
        Log("SWEEP done");
    }

    // Dev: GTA_STEPSCAN=x,y,r lists stair-like ground steps (0.12..0.25 m rise over 0.4 m) around a point.
    if (const char* ss = getenv("GTA_STEPSCAN")) {
        float cx = 0, cy = 0, cr = 40;
        sscanf(ss, "%f,%f,%f", &cx, &cy, &cr);
        int n = 0;
        for (float x = cx - cr; x <= cx + cr && n < 40; x += 0.5f)
            for (float y = cy - cr; y <= cy + cr && n < 40; y += 0.5f) {
                const float a = g_world.GetGroundHeight(x, y, 2000.0f), b = g_world.GetGroundHeight(x + 0.4f, y, 2000.0f);
                if (a > -900.0f && b > -900.0f && std::fabs(b - a) > 0.12f && std::fabs(b - a) < 0.25f) {
                    Log("STEP at %.1f %.1f z=%.2f -> %.2f", x, y, a, b);
                    n++;
                }
            }
    }
    // Dev: GTA_GROUNDMAP=<metres per pixel> writes the top collision surface over the whole map to groundmap.pgm
    // (0 = nothing there, which is open sea or a hole in the land).
    if (const char* gm = getenv("GTA_GROUNDMAP")) {
        const float step = std::max(1.0f, (float)atof(gm));
        g_world.BuildAllGroundTris();
        const int n = (int)(6000.0f / step);
        std::vector<uint8_t> img((size_t)n * n, 0);
        int filled = 0;
        for (int py = 0; py < n; py++)
            for (int px = 0; px < n; px++) {
                const float x = -3000.0f + (px + 0.5f) * step, y = 3000.0f - (py + 0.5f) * step;
                const float h = g_world.GetGroundHeight(x, y, 2000.0f);
                if (h < -900.0f) continue;
                filled++;
                img[(size_t)py * n + px] = (uint8_t)std::clamp(40.0f + (h + 100.0f) * 0.5f, 1.0f, 255.0f);
            }
        char path[512];
        snprintf(path, sizeof path, "%s\\groundmap.pgm", shotDir);
        if (FILE* f = fopen(path, "wb")) {
            fprintf(f, "P5\n%d %d\n255\n", n, n);
            fwrite(img.data(), 1, img.size(), f);
            fclose(f);
        }
        Log("GROUNDMAP %dx%d, %d filled, wrote %s", n, n, filled, path);
    }

    // Dev: GTA_COLRAY=<radius> checks collision holes. From a grid of ground points, chest-high rays
    // go out in 8 directions; wherever a ray hits geometry the walking test must block the move through it.
    if (const char* cr = getenv("GTA_COLRAY")) {
        const float rad = (float)atof(cr);
        for (int i = 0; i < 300; i++) g_world.Stream(playerX, playerY, playerZ);
        int rays = 0, holes = 0;
        for (float x = playerX - rad; x <= playerX + rad; x += 5.0f)
            for (float y = playerY - rad; y <= playerY + rad; y += 5.0f) {
                const float gz = g_world.GetGroundHeight(x, y, playerZ + 2.0f);
                if (gz < -90.0f) continue;
                if (g_world.HitsSolidCircle(x, y, gz, x + 40.0f, y, PLAYER_RADIUS, STEP_OVER, PLAYER_HEIGHT)) continue;   // inside a solid: not a place CJ can stand
                for (int k = 0; k < 8; k++) {
                    const float a = k * 0.7853982f, dx = cosf(a), dy = sinf(a);
                    int kind = 0; float hnz = 0.0f;
                    const float t = g_world.RayCast(x, y, gz + 1.0f, x + dx * 3.0f, y + dy * 3.0f, gz + 1.0f, &kind, &hnz);
                    if (t >= 1.0f) continue;
                    rays++;
                    const float d = 3.0f * t;
                    if (d < 1.0f || (kind == 0 && hnz > 0.5f)) continue;   // too close to judge, or a walkable slope
                    bool stopped = false;   // walk at into it in 0.1 m steps, as the game does
                    for (float w = d - 0.9f; w < d + 0.3f && !stopped; w += 0.1f)
                        stopped = g_world.HitsSolidCircle(x + dx * (w + 0.1f), y + dy * (w + 0.1f), gz, x + dx * w, y + dy * w,
                                                          PLAYER_RADIUS, STEP_OVER, PLAYER_HEIGHT);
                    if (!stopped) {
                        holes++;
                        Log("COLRAY HOLE at %.1f %.1f %.2f dir=%d d=%.2f kind=%d nz=%.2f", x, y, gz, k, d, kind, hnz);
                    }
                }
            }
        Log("COLRAY done: %d rays hit, %d holes", rays, holes);
    }

    MSG msg = {};
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!g_running) break;
        g_prof.FrameBegin();

        // Apply a pending window resize: match the backbuffer to the client
        // area (sharp 1:1 instead of a stretched 720p upscale) and refresh the
        // projection aspect so nothing is squashed.
        if (g_pendingResize && getenv("GTA_NORESIZE")) g_pendingResize = false;   // dev: keep the --res/--window backbuffer under a tiling window manager
        if (g_pendingResize) {
            int rw = g_resizeW, rh = g_resizeH;
            g_pendingResize = false;
            if (g_renderer.Resize(rw, rh)) {
                WIDTH = rw; HEIGHT = rh;
                D3DMATRIX rproj = MatrixPerspectiveFov(g_fovDeg*3.14159f/180.0f,
                                    (float)WIDTH/(float)HEIGHT, g_nearClip, 3000.0f);
                g_renderer.SetProjMatrix(rproj);
                Log("RESIZE: backbuffer -> %dx%d", rw, rh);
            }
        }

        g_pad.Poll();
        DWORD now = GetTickCount();  // coarse ms clock: FPS counter only
        LARGE_INTEGER qpcNow;
        QueryPerformanceCounter(&qpcNow);
        float dt = (float)((double)(qpcNow.QuadPart - qpcLast.QuadPart) /
                           (double)qpcFreq.QuadPart);
        if (dt > 0.1f) dt = 0.1f; // clamp
        qpcLast = qpcNow;
        // Headless --walk probe: wall-clock dt is ~0 at uncapped FPS, so the
        // player barely moves. Force a fixed 60Hz step so the walk covers real
        // distance deterministically (collision test, not a timing test).
        if (g_autoWalk || getenv("GTA_KEYS")) dt = 1.0f / 60.0f;   // headless motion tests: fixed step
        if (scmRun) { scmClock += dt; s_scm.Update((int32_t)(scmClock * 1000.0), 600); g_sg.Update(dt); }
        if (worldReady && playMode) {
            g_enex.warp = [&](float x, float y, float z, float hdg, int area) {
                playerX = camPosX = x; playerY = camPosY = y; playerZ = camPosZ = z - 1.0f + 0.05f + EYE_HEIGHT;   // z is the ped centre, 1 m above the feet
                yaw = hdg * 0.01745329f + 1.5707963f; velZ = 0.0f; onGround = false;
                for (int i = 0; i < 60; i++) g_world.Stream(playerX, playerY, playerZ - EYE_HEIGHT);
                Log("ENEX: warped to %.1f %.1f %.1f area %d", x, y, z, area);
            };
            const int hr = ((int)(g_gameSeconds / 3600.0f)) % 24;
            g_enex.Update(dt, playerX, playerY, playerZ - EYE_HEIGHT + 1.0f, g_driveCar >= 0, hr);
        }

        // ---- Dev/test keys (ignored in the scripted/flyover capture modes) ----
        if (playMode && !scripted && !flyover) {
            if (DevKeyPressed(VK_OEM_3)) {   // ` free camera
                g_freeCam = !g_freeCam;
                g_fcPos[0] = camPosX; g_fcPos[1] = camPosY; g_fcPos[2] = camPosZ;
                Log("DEV: free camera %s", g_freeCam ? "ON (WASD fly, Space up, Ctrl down, Shift fast)" : "off");
            }
            if (DevKeyPressed(VK_F1)) {
                g_devOverlay = !g_devOverlay;
                Log("DEV: overlay %s", g_devOverlay ? "ON" : "OFF");
            }
            if (DevKeyPressed(VK_F2)) {
                g_devWireframe = !g_devWireframe;
                g_renderer.SetWireframe(g_devWireframe);
                Log("DEV: wireframe %s", g_devWireframe ? "ON" : "OFF");
            }
            if (DevKeyPressed(VK_F3)) {
                g_devShowCollision = !g_devShowCollision;
                Log("DEV: collision view %s", g_devShowCollision ? "ON (pink wireframe)" : "OFF");
            }
            if (DevKeyPressed('B') && g_vehicles.Ready() && g_vehicles.ModelCount() > 0) {   // dev: spawn the next car ahead of CJ
                static const int s_devModel = getenv("GTA_VEHMODEL") ? atoi(getenv("GTA_VEHMODEL")) : 0;   // dev: always spawn this model id
                const int id = s_devModel ? s_devModel : g_vehicles.IdAt(g_vehSpawnIdx++ % g_vehicles.ModelCount());
                float fx = playerX - camPosX, fy = playerY - camPosY;   // away from the camera, along the view
                const float fl = std::sqrt(fx * fx + fy * fy);
                if (fl < 0.01f) { fx = cosf(yaw); fy = sinf(yaw); } else { fx /= fl; fy /= fl; }
                static int s_carSlot = 0;   // spread the spawns sideways so they do not overlap
                const float side = (float)(s_carSlot++ % 3 - 1) * 3.2f;
                const float sx = playerX + fx * 5.0f + fy * side, sy = playerY + fy * 5.0f - fx * side;
                const float gz = g_world.GetGroundHeight(sx, sy, playerZ - EYE_HEIGHT + 1.0f);
                g_vehicles.Spawn(g_renderer, id, sx, sy, gz < -500.0f ? playerZ - EYE_HEIGHT : gz, std::atan2(fy, fx) - 1.57079633f);
                Log("DEV: spawned vehicle %d at %.1f %.1f %.1f (player %.1f %.1f, cam %.1f %.1f)", id, sx, sy, gz, playerX, playerY, camPosX, camPosY);
            }
            if (DevKeyPressed('K') && g_peds.Ready()) {   // dev: a pedestrian ahead of CJ
                float fx = playerX - camPosX, fy = playerY - camPosY;
                const float fl = std::sqrt(fx * fx + fy * fy);
                if (fl < 0.01f) { fx = cosf(yaw); fy = sinf(yaw); } else { fx /= fl; fy /= fl; }
                static int s_pedSlot = 0;   // each spawn lands a little further to the right so they do not stack
                const float side = (float)(s_pedSlot++ % 5 - 2) * 1.2f;
                const float sx = playerX + fx * 3.5f + fy * side, sy = playerY + fy * 3.5f - fx * side;
                const float gz = g_world.GetGroundHeight(sx, sy, playerZ - EYE_HEIGHT + 1.0f);
                float px = sx, py = sy;
                if (getenv("GTA_PEDNEAR")) { px = playerX + std::cos(g_cj.Heading()) * 0.9f; py = playerY + std::sin(g_cj.Heading()) * 0.9f; }   // dev: within CJ's reach
                if (const char* pa = getenv("GTA_PEDAHEAD")) { px = playerX + (float)atof(pa); py = playerY; if (const char* c = strchr(pa, ',')) py += (float)atof(c + 1); }   // dev: "east[,north]" metres from CJ (yaw 0 faces east)
                const int n = g_peds.Spawn(g_renderer, -1, px, py, gz < -500.0f ? playerZ - EYE_HEIGHT : gz, std::atan2(-fy, -fx));
                if (n >= 0) if (const char* ph = getenv("GTA_PEDHP")) g_peds.SetHealth((size_t)n, (float)atof(ph));   // dev
                Log("DEV: spawned ped %d", n);
            }
            if (getenv("GTA_PEDTRACK") && g_peds.Count() > 0 && (frame % 10) == 0)   // dev: follow ped 0
                Log("PEDTRACK f%d pos %.2f %.2f %.2f speed %.2f vz %.2f flying %d clip %s", frame, g_peds.X(0), g_peds.Y(0), g_peds.Z(0), g_peds.VelX(0), g_peds.VelZ(0), (int)g_peds.Flying(0), g_peds.ClipName(0));
            if (DevKeyPressed(VK_F4)) {
                g_devNoClip = !g_devNoClip;
                Log("DEV: noclip %s", g_devNoClip ? "ON (free fly)" : "OFF");
            }
            if (DevKeyPressed(VK_F5)) {
                // Next UNUSED index: the counter restarts each launch, so
                // without the existence check a new session overwrote the
                // previous session's dev_001.. shots.
                char full[MAX_PATH];
                do {
                    snprintf(full, sizeof(full), "%s\\dev_%03d.bmp", shotDir, ++g_devShotIdx);
                } while (GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES);
                if (g_renderer.SaveScreenshot(full)) Log("DEV: screenshot %s", full);
                g_prof.IgnoreFrame();  // a 25 MB BMP write is not a streaming hitch
            }
            if (DevKeyPressed(VK_F6)) {
                g_timeFlow = !g_timeFlow;
                Log("DEV: time flow %s", g_timeFlow ? "ON" : "OFF");
            }
            if (DevKeyPressed(VK_F9)) {
                g_world.DumpNearby(playerX, playerY, playerZ, 60.0f);
                Log("DEV: dumped nearby instances to inst_dump.txt");
            }
            if (DevKeyPressed(VK_F7)) g_gameSeconds -= 3600.0f;
            if (DevKeyPressed(VK_F8)) g_gameSeconds += 3600.0f;
            while (g_gameSeconds < 0.0f) g_gameSeconds += 86400.0f;
            while (g_gameSeconds >= 86400.0f) g_gameSeconds -= 86400.0f;
            // Weather cycle: moved off F9 (which dumps nearby insts) to N so
            // the two don't fire on the same key.
            if (DevKeyPressed('N') && g_timecycOk) {
                g_weatherIdx = (g_weatherIdx + 1) % g_timecyc.WeatherCount();
                g_weatherBlend = 1.0f; g_weatherAuto = false;
                Log("DEV: weather %zu = %s", g_weatherIdx, g_timecyc.WeatherName(g_weatherIdx).c_str());
            }
            // Save / restore a player position + facing. Mark a tricky spot
            // (F11), run a collision test, then snap straight back to it (F12)
            // instead of walking there again. Restore also clears vertical
            // velocity and drops to the ground on the next frame.
            static bool haveSaved = false;
            static float saveX = 0, saveY = 0, saveZ = 0, saveYaw = 0, savePitch = 0;
            if (DevKeyPressed(VK_F11)) {
                saveX = playerX; saveY = playerY; saveZ = playerZ;
                saveYaw = yaw; savePitch = pitch; haveSaved = true;
                Log("DEV: saved position (%.1f, %.1f, %.1f)", playerX, playerY, playerZ);
            }
            if (DevKeyPressed(VK_F12)) {
                if (haveSaved) {
                    playerX = saveX; playerY = saveY; playerZ = saveZ;
                    yaw = saveYaw; pitch = savePitch; velZ = 0.0f; onGround = false;
                    Log("DEV: restored position (%.1f, %.1f, %.1f)", playerX, playerY, playerZ);
                } else {
                    Log("DEV: no saved position yet (F11 to save)");
                }
            }
            // Unstick: if a bad spawn or a geometry seam traps the player,
            // hop straight up a couple of metres so they drop back onto the
            // surface instead of being wedged inside a wall.
            if (DevKeyPressed('R')) {
                g_prof.ResetWorst();
                Log("DEV: profiler worst-frame reset");
            }
            if (DevKeyPressed('U')) {
                playerZ += 3.0f; velZ = 0.0f; onGround = false;
                Log("DEV: unstick (+3m up)");
            }
            if (DevKeyPressed(VK_F10)) {
                g_devLighting = !g_devLighting;
                g_renderer.SetLightingEnabled(g_devLighting);
                Log("DEV: world lighting %s", g_devLighting ? "ON" : "OFF (full-bright)");
            }
        }

        // ---- SA pause menu: ESC / P open it; it owns the keys while open ----
        {
            int keys[64];
            int nk = g_keyQueueN;
            memcpy(keys, g_keyQueue, sizeof(int) * (size_t)nk);
            g_keyQueueN = 0;
            if (g_pad.Connected() && g_hasFocus) {   // pad -> menu keys and the pause button
                auto push = [&](int vk) { if (nk < 63) keys[nk++] = vk; };
                static int s_stickDir = 0;
                const int dir = g_pad.LY() > 0.6f ? 1 : g_pad.LY() < -0.6f ? 2 : g_pad.LX() < -0.6f ? 3 : g_pad.LX() > 0.6f ? 4 : 0;
                if (dir != s_stickDir && dir) push(dir == 1 ? VK_UP : dir == 2 ? VK_DOWN : dir == 3 ? VK_LEFT : VK_RIGHT);
                s_stickDir = dir;
                if (g_pad.Pressed(Gamepad::DPAD_UP)) push(VK_UP);
                if (g_pad.Pressed(Gamepad::DPAD_DOWN)) push(VK_DOWN);
                if (g_pad.Pressed(Gamepad::DPAD_LEFT)) push(VK_LEFT);
                if (g_pad.Pressed(Gamepad::DPAD_RIGHT)) push(VK_RIGHT);
                if (g_pad.Pressed(Gamepad::START)) push(VK_ESCAPE);
                if (g_menuReady && g_menu.IsOpen()) {
                    if (g_pad.Pressed(Gamepad::A)) push(VK_RETURN);
                    if (g_pad.Pressed(Gamepad::B)) push(VK_ESCAPE);
                }
            }
            if (g_escOpensMenu && g_menuReady) {
                bool wasOpen = g_menu.IsOpen();
                // Retail pauses when the game loses focus (alt-tab / click away).
                if (!g_hasFocus && !g_menu.IsOpen()) g_menu.Open();
                for (int k = 0; k < nk; k++) {
                    if (g_menu.IsOpen()) g_menu.OnKey(keys[k]);
                    else if (keys[k] == VK_ESCAPE) g_menu.Open();
                }
                if (g_menu.IsOpen() && g_wheelSteps) { g_menu.OnWheel(g_wheelSteps); g_wheelSteps = 0; }
                if (g_menu.IsOpen()) {
                    // Pointer in backbuffer pixels (the client may differ from
                    // the render size when --res is used borderless).
                    POINT mp; GetCursorPos(&mp); ScreenToClient(g_hwnd, &mp);
                    if (g_msgMouseSeen) { mp.x = g_msgMouseX; mp.y = g_msgMouseY; }   // the window's own pointer messages beat GetCursorPos under Wayland
                    RECT cr; GetClientRect(g_hwnd, &cr);
                    float sx = cr.right > 0 ? (float)WIDTH / (float)cr.right : 1.0f;
                    float sy = cr.bottom > 0 ? (float)HEIGHT / (float)cr.bottom : 1.0f;
                    static bool lmbWas = false;
                    bool lmb = (GameKey(VK_LBUTTON) & 0x8000) != 0;
                    bool inside = g_hasFocus && GetForegroundWindow() == g_hwnd;
                    g_menu.OnMouse((float)mp.x * sx, (float)mp.y * sy, inside && lmb && !lmbWas, inside && lmb);
                    lmbWas = lmb;
                }
                g_menu.SetPadHints(g_pad.Connected() && g_pad.Active());
                if (g_menu.TakeSettingsChanged()) {
                    const FeSettings& s = g_menu.Settings();
                    if (s.vsync != g_renderer.Vsync() && !g_renderer.SetVsync(s.vsync))
                        Log("SETTINGS: VSync reset failed");
                    g_world.SetHdDrawScale(s.drawDist);
                    g_renderer.SetMipmaps(s.mipmaps);
                    if (s.antiAlias != g_renderer.AntiAlias() && !g_renderer.SetAntiAlias(s.antiAlias)) {
                        Log("SETTINGS: anti aliasing %d refused", s.antiAlias);
                        g_menu.Settings().antiAlias = g_renderer.AntiAlias();
                    }
                    {
                        static int lastLang = -1;
                        if (lastLang < 0) lastLang = 0;
                        if (s.language != lastLang) {
                            lastLang = s.language;
                            if (g_hudReady) g_hud.SetLanguage(GameDir(), kLanguageFiles[s.language]);
                        }
                    }
                    {
                        // Borderless <-> windowed, and the windowed size.
                        static int lastMode = -1, lastSize = -1;
                        if (lastMode < 0) { lastMode = windowed ? 1 : 0; lastSize = -2; }
                        if (s.displayMode != lastMode || (s.displayMode == 1 && s.windowSize != lastSize)) {
                            if (s.displayMode == 1) {
                                const int cw = kWindowSizes[s.windowSize][0], ch = kWindowSizes[s.windowSize][1];
                                RECT rc = { 0, 0, cw, ch };
                                AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
                                SetWindowLongPtrW(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
                                int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
                                int ow = rc.right - rc.left, oh = rc.bottom - rc.top;
                                SetWindowPos(g_hwnd, nullptr, (sw - ow) / 2 > 0 ? (sw - ow) / 2 : 0,
                                             (sh - oh) / 2 > 0 ? (sh - oh) / 2 : 0, ow, oh,
                                             SWP_NOZORDER | SWP_FRAMECHANGED);
                            } else {
                                SetWindowLongPtrW(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                                SetWindowPos(g_hwnd, nullptr, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                                             GetSystemMetrics(SM_CYSCREEN), SWP_NOZORDER | SWP_FRAMECHANGED);
                            }
                            lastMode = s.displayMode; lastSize = s.windowSize;
                        }
                    }
                    SaveSettings(s);
                    Log("SETTINGS: frameLimit=%d vsync=%d drawDist=%.3f",
                        s.frameLimit, (int)s.vsync, s.drawDist);
                }
                if (g_menu.TakeQuit()) { Log("FRONTEND: Quit Game"); g_running = false; }
                if (!wasOpen && g_menu.IsOpen()) {
                    // SA's CentreMousePointer: start the pointer mid-screen
                    // (not when we lost focus: never yank the user's cursor).
                    if (g_hasFocus) {
                        RECT cr; GetClientRect(g_hwnd, &cr);
                        POINT c; c.x = (cr.right - cr.left) / 2; c.y = (cr.bottom - cr.top) / 2;
                        ClientToScreen(g_hwnd, &c);
                        if (MouseMode() == 0) SetCursorPos(c.x, c.y);
                    }
                    Log("FRONTEND: pause menu opened");
                } else if (wasOpen && !g_menu.IsOpen()) {
                    // Drop the mouse motion made in the menu so the view does not jump.
                    g_rawMouseDX = 0; g_rawMouseDY = 0; g_rawMouseHaveAbs = false;
                    Log("FRONTEND: resumed");
                }
                g_devPaused = g_menu.IsOpen();
                SetPointerCapture(!g_menu.IsOpen() && g_hasFocus);
            }
        }
        // Pause menu = frozen simulation, rendering keeps running.
        if (g_devPaused) dt = 0.0f;
        else if (g_wheelOpen) dt *= 0.2f;   // GTA V: the world crawls while you choose a weapon

        // ---- Time of day: sky colours, sun, fog + draw distance ----
        float fogStart = 400.0f, fogEnd = 2500.0f;
        float clockHour = 12.0f;
        if (g_timeFlow && playMode) g_gameSeconds += dt * 60.0f;  // 1 real s = 1 game min
        while (g_gameSeconds >= 86400.0f) { g_gameSeconds -= 86400.0f; g_gameDay++; }
        if (g_timecycOk) {
            clockHour = fmodf(g_gameSeconds / 3600.0f, 24.0f);
            if (clockHour < 0.0f) clockHour += 24.0f;
            TimeCycleHour tc;
            if (g_weatherAuto && playMode && !g_devPaused) {
                static const char* kForecast[5][9] = {
                    {"EXTRASUNNY_LA", "SUNNY_LA", "EXTRASUNNY_SMOG_LA", "SUNNY_SMOG_LA", "CLOUDY_LA", "SUNNY_LA", "EXTRASUNNY_LA", "CLOUDY_LA", "SUNNY_LA"},
                    {"SUNNY_SF", "EXTRASUNNY_SF", "CLOUDY_SF", "RAINY_SF", "FOGGY_SF", "SUNNY_SF", "EXTRASUNNY_SF", "CLOUDY_SF", "SUNNY_SF"},
                    {"SUNNY_VEGAS", "EXTRASUNNY_VEGAS", "CLOUDY_VEGAS", "SUNNY_VEGAS", "EXTRASUNNY_VEGAS", "SUNNY_VEGAS", "CLOUDY_VEGAS", "SUNNY_VEGAS", "EXTRASUNNY_VEGAS"},
                    {"EXTRASUNNY_DESERT", "SUNNY_DESERT", "EXTRASUNNY_DESERT", "SUNNY_DESERT", "SANDSTORM_DESERT", "SUNNY_DESERT", "EXTRASUNNY_DESERT", "SUNNY_DESERT", "EXTRASUNNY_DESERT"},
                    {"EXTRASUNNY_COUNTRYSIDE", "SUNNY_COUNTRYSIDE", "CLOUDY_COUNTRYSIDE", "RAINY_COUNTRYSIDE", "SUNNY_COUNTRYSIDE", "EXTRASUNNY_COUNTRYSIDE", "CLOUDY_COUNTRYSIDE", "SUNNY_COUNTRYSIDE", "EXTRASUNNY_COUNTRYSIDE"}};
                const int region = WeatherRegionAt(playerX, playerY);
                g_weatherTimer -= dt / 60.0f;   // game minutes -> hours at 1 s = 1 min
                if (region != g_weatherRegion || g_weatherTimer <= 0.0f) {
                    g_weatherRng = g_weatherRng * 1664525u + 1013904223u;
                    const char* pick = kForecast[region][(g_weatherRng >> 16) % 9];
                    const int w = g_timecyc.FindWeather(pick);
                    if (w >= 0 && (size_t)w != g_weatherIdx) {
                        g_weatherPrev = g_weatherIdx;
                        g_weatherIdx = (size_t)w;
                        g_weatherBlend = 0.0f;
                        Log("WEATHER: %s -> %s", g_timecyc.WeatherName(g_weatherPrev).c_str(), pick);
                    }
                    g_weatherRegion = region;
                    g_weatherTimer = 3.0f + (float)((g_weatherRng >> 8) & 0xFF) / 255.0f * 3.0f;   // 3 to 6 game hours
                }
            }
            if (g_weatherBlend < 1.0f) g_weatherBlend = std::min(1.0f, g_weatherBlend + dt / 20.0f);
            g_timecyc.Sample(g_weatherIdx, clockHour, tc);
            if (g_weatherBlend < 1.0f && g_weatherPrev != g_weatherIdx) {
                TimeCycleHour from;
                g_timecyc.Sample(g_weatherPrev, clockHour, from);
                const float* fa = reinterpret_cast<const float*>(&from);
                float* ta = reinterpret_cast<float*>(&tc);
                for (size_t i = 0; i < sizeof(TimeCycleHour) / sizeof(float); i++) ta[i] = fa[i] + (ta[i] - fa[i]) * g_weatherBlend;
            }
            g_renderer.SetSkyColors(tc.skyTop[0], tc.skyTop[1], tc.skyTop[2],
                                    tc.skyBot[0], tc.skyBot[1], tc.skyBot[2]);
            // Sun path: SA coords are +X = east, +Y = north. Sunrise 06:00 in the
            // east, noon due south (northern hemisphere), sunset 18:00 in the west.
            float dayT = (clockHour - 6.0f) / 12.0f;   // 0 = 06:00, 1 = 18:00
            float elev = sinf(dayT * 3.14159265f) * 1.05f;
            float az = dayT * 3.14159265f;
            float ce = cosf(elev);
            bool sunUp = (clockHour > 5.5f && clockHour < 18.5f);
            float sunX = ce * cosf(az), sunY = -ce * sinf(az), sunZ = sinf(elev);
            {
                // Sun sprites as CTimeCycle::CalcColoursForPoint places them:
                // a = minute of the day as an angle (noon = pi), drawn by
                // CCoronas::DoSunAndMoon at 0.95 * FarClp with sizes SunSz * 2.7335
                // (core) and SunSz * 6 (corona). A corona of size s at depth z spans
                // about 1.05 * s / z (CSprite::CalcScreenCoors, 70 deg 4:3 FOV).
                const float a = clockHour * 60.0f * 0.004363323f;
                const float vx = sinf(a) + 0.7f, vy = -0.7f, vz = 0.2f - cosf(a);
                const float depth = (tc.farClip > 100.0f ? tc.farClip : 100.0f) * 0.95f;
                g_renderer.SetSun(vx, vy, vz, tc.sunCore, tc.sunCorona,
                                  1.05f * tc.sunSize * 2.7335f / depth, 1.05f * tc.sunSize * 6.0f / depth);
            }
            // ---- World lighting (timecyc ambient + directional sun) ----
            // Retail lights world geometry per vertex from two timecyc ambient
            // columns plus a directional sun light. `Ambient` alone is a dark
            // tint (Midday EXTRASUNNY_LA = 11 0 0, i.e. near black) — the
            // brightness comes from `Amb_Obj` (210 194 182), which shadows the
            // day; it is faded out at night and replaced by kNightLightLift.
            // This path now only drives geometry WITHOUT baked prelit colours
            // (hand-built meshes and the ~7% of map DFFs that have none) —
            // prelit meshes are drawn unlit from their own vertex colours.
            // The sun fades out with its elevation so dusk actually gets dark.
            float sunElev = sinf(elev);
            float sunFade = sunUp ? (sunElev >= 0.30f ? 1.0f : sunElev * 3.3f) : 0.0f;
            float nightMix = 1.0f - sunFade;
            float ambLift = nightMix * kNightLightLift;
            g_lightAmb[0] = tc.ambient[0] + tc.ambientObj[0] * sunFade + ambLift;
            g_lightAmb[1] = tc.ambient[1] + tc.ambientObj[1] * sunFade + ambLift;
            g_lightAmb[2] = tc.ambient[2] + tc.ambientObj[2] * sunFade + ambLift;
            g_lightSunFade = sunFade;
            g_renderer.SetWorldLighting(g_lightAmb[0], g_lightAmb[1], g_lightAmb[2],
                                        tc.directional[0], tc.directional[1],
                                        tc.directional[2], sunX, sunY, sunZ,
                                        sunFade);
            // ---- Baked (prelit) vertex colours ----
            // Most map geometry ships authored day prelit shading (geometry
            // flag 0x08) and, for 344/382 sampled map DFFs, an SA night set
            // (extension plugin 0x253F2F9). Those meshes are drawn unlit and
            // blend day -> night with the sun, so the world's baked shading and
            // AO survive and the night set lights up at dusk.
            // A mesh with no night set is tinted instead, normalised against
            // its own midday brightness so midday stays exactly as authored.
            float tint[3];
            for (int i = 0; i < 3; i++) {
                float dayRef = tc.ambient[i] + tc.ambientObj[i];
                float t = dayRef > 1.0f ? g_lightAmb[i] / dayRef : 1.0f;
                tint[i] = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            }
            g_renderer.SetPrelitShading(1.0f - sunFade, tint[0], tint[1], tint[2]);
            static bool lightLogged = false;
            if (!lightLogged) {
                lightLogged = true;
                Log("LIGHT: amb=(%.0f,%.0f,%.0f) ambObj=(%.0f,%.0f,%.0f) lift=%.0f -> global=(%.0f,%.0f,%.0f) dir=(%.0f,%.0f,%.0f) sunFade=%.2f tint=(%.2f,%.2f,%.2f) sunDir=(%.2f,%.2f,%.2f)",
                    tc.ambient[0], tc.ambient[1], tc.ambient[2],
                    tc.ambientObj[0], tc.ambientObj[1], tc.ambientObj[2], ambLift,
                    g_lightAmb[0], g_lightAmb[1], g_lightAmb[2],
                    tc.directional[0], tc.directional[1], tc.directional[2],
                    sunFade, tint[0], tint[1], tint[2], sunX, sunY, sunZ);
            }
            // Retail fog: FogSt shifts the fog start relative to FarClp
            // (negative FogSt = fog band pulled towards the camera, which is
            // how the foggy weathers work). Clear weather has FogSt >= 0, which
            // pushes the start past the far clip: there is no fog band, and a
            // degenerate start==end would make D3D fog saturate the whole scene
            // (everything one flat sky colour). So only run the band when it is
            // real, otherwise soften just the far-clip cut.
            fogEnd = tc.farClip;
            fogStart = tc.farClip + tc.fogSt;
            g_fogEnable = fogStart < fogEnd - 1.0f;
            if (!g_fogEnable) {
                // No real fog band (clear weather): SA still shows a visible
                // atmospheric haze over the far half of the draw distance, so
                // the city fades into the horizon instead of ending on a hard
                // edge. Start the haze at ~0.5*farClip rather than 0.75.
                fogStart = fogEnd * 0.5f;
            }
            if (fogStart < 0.0f) fogStart = 0.0f;
            static bool fogLogged = false;
            if (!fogLogged) {
                fogLogged = true;
                Log("FOG: farClip=%.0f fogSt=%.0f -> start=%.0f end=%.0f realBand=%d",
                    tc.farClip, tc.fogSt, fogStart, fogEnd, (int)g_fogEnable);
            }
            for (int i = 0; i < 4; i++) g_waterRGBA[i] = tc.water[i];
            g_tcNow = tc;
            g_fogColor = D3DCOLOR_COLORVALUE(tc.skyBot[0] / 255.0f,
                                            tc.skyBot[1] / 255.0f,
                                            tc.skyBot[2] / 255.0f, 1.0f);
            if (worldReady) g_world.SetFarClip(tc.farClip);
        }

        // FPS counter
        fpsFrames++;
        static DWORD fpsTick = 0;
        if (now - fpsTick >= 500) {
            fps = fpsFrames * 1000.0f / (float)(now - fpsTick);
            fpsFrames = 0;
            fpsTick = now;
            if (playMode) {
                snprintf(titleBuf, sizeof(titleBuf),
                    "GTA SA C++ [PLAY] FPS:%.0f Pos:(%.1f, %.1f, %.1f) Yaw:%.1f Obj:%u",
                    fps, playerX, playerY, playerZ, yaw*57.3f,
                    (unsigned)(worldReady ? g_world.VisibleInstances() : mapObjects.size()));
                SetWindowTextA(g_hwnd, titleBuf);
            }
        }

        if (playMode) {
            {   // MOUSE: one line every 3 s: which input source is alive
                static double s_t = 0.0; static long s_r = 0, s_m = 0;
                s_t += dt;
                if (s_t >= 3.0) {
                    POINT cp; GetCursorPos(&cp); RECT cl; GetClipCursor(&cl);
                    Log("MOUSE: yaw=%.3f raw=%ld moves=%ld cursor=(%ld,%ld) msg=(%ld,%ld) clip=(%ld,%ld,%ld,%ld) focus=%d menu=%d hidden=%d",
                        yaw, g_dbgRaw - s_r, g_dbgMove - s_m, cp.x, cp.y, (long)g_msgMouseX, (long)g_msgMouseY,
                        cl.left, cl.top, cl.right, cl.bottom, (int)g_hasFocus, (int)(g_escOpensMenu && g_menuReady && g_menu.IsOpen()), (int)g_cursorHidden);
                    s_r = g_dbgRaw; s_m = g_dbgMove; s_t = 0.0;
                }
            }
            // ---- Mouse look ----
            if (mouseLook && !g_devPaused) {
                int dx = 0, dy = 0;
                // Raw deltas first (no acceleration bias). Trackpads often send no raw
                // mouse motion, only the cursor moving, so the cursor offset from the
                // window centre is the fallback. The cursor is pinned to the window
                // centre every frame either way, so it can never leave the window and
                // clicks always land on the game.
                dx = (int)g_rawMouseDX; dy = (int)g_rawMouseDY;
                g_rawMouseDX = 0; g_rawMouseDY = 0;
                if (g_hasFocus && MouseMode() == 1) {
                    // Free pointer: never pinned and never warped (a warp makes Wine's Wayland driver lock
                    // the pointer at the warp target, which ends all motion). The view turns by the
                    // pointer's own movement; held against a window edge it keeps turning that way.
                    RECT rc; GetClientRect(g_hwnd, &rc);
                    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
                    POINT cur;
                    if (g_msgMouseSeen) { cur.x = g_msgMouseX; cur.y = g_msgMouseY; }
                    else { GetCursorPos(&cur); ScreenToClient(g_hwnd, &cur); }
                    static POINT s_last = {0, 0};
                    if (dx == 0 && dy == 0 && g_freeHave) { dx = cur.x - s_last.x; dy = cur.y - s_last.y; }
                    if (g_freeHave && cur.x == s_last.x && cur.y == s_last.y && w > 0 && h > 0) {
                        const int edge = 3, push = 18;   // pixels per frame while held at an edge
                        if (cur.x <= edge) dx = -push; else if (cur.x >= w - 1 - edge) dx = push;
                        if (cur.y <= edge) dy = -push; else if (cur.y >= h - 1 - edge) dy = push;
                    }
                    s_last = cur;
                    g_freeHave = true;
                    g_cursorCentred = false;
                } else if (g_hasFocus) {
                    POINT mp; GetCursorPos(&mp);
                    RECT rc; GetClientRect(g_hwnd, &rc);
                    POINT center = {(rc.right-rc.left)/2, (rc.bottom-rc.top)/2};
                    ClientToScreen(g_hwnd, &center);
                    static bool s_warpOk = true;   // false when the system ignores SetCursorPos (then no cursor look)
                    // the cursor offset only counts when the last frame left it centred
                    // (not on the first frame or after the menu moved it)
                    if (dx == 0 && dy == 0 && s_warpOk && g_cursorCentred) { dx = mp.x - center.x; dy = mp.y - center.y; }
                    // Wayland / VNC: the compositor ignores warps. Fall back to the cursor's own
                    // movement between frames; it stops at the window edge, so a swipe at the edge
                    // does nothing, but a trackpad still turns the view.
                    static POINT s_lastMp = {0, 0};
                    static bool s_haveLast = false;
                    if (!s_warpOk) {
                        if (g_msgMouseSeen) { mp.x = g_msgMouseX; mp.y = g_msgMouseY; }   // message position beats a stale GetCursorPos
                        if (dx == 0 && dy == 0 && s_haveLast) { dx = mp.x - s_lastMp.x; dy = mp.y - s_lastMp.y; }
                        s_lastMp = mp; s_haveLast = true;
                    } else s_haveLast = false;
                    if (s_warpOk && (mp.x != center.x || mp.y != center.y)) {
                        SetCursorPos(center.x, center.y);
                        POINT chk; GetCursorPos(&chk);
                        if (chk.x != center.x || chk.y != center.y) s_warpOk = false;
                    }
                    g_cursorCentred = true;
                    // keep the pointer inside the window if something moves it away
                    RECT cl = rc;
                    POINT tl = {cl.left, cl.top}, br = {cl.right, cl.bottom};
                    ClientToScreen(g_hwnd, &tl); ClientToScreen(g_hwnd, &br);
                    RECT clip = {tl.x, tl.y, br.x, br.y};
                    if (MouseMode() == 0) ClipCursor(&clip);
                }
                if (g_wheelOpen) {   // the weapon wheel takes the mouse
                    g_wheelX += (float)dx * 0.012f; g_wheelY += (float)dy * 0.012f;
                    const float wl = sqrtf(g_wheelX * g_wheelX + g_wheelY * g_wheelY);
                    if (wl > 1.0f) { g_wheelX /= wl; g_wheelY /= wl; }
                } else if (dx != 0 || dy != 0) {
                    // Post-mirror-fix sign: mouse right (dx>0) must turn the view right.
                    const FeSettings& ms = g_menu.Settings();
                    yaw   -= dx * MOUSE_SENS * ms.mouseSens;
                    pitch -= dy * MOUSE_SENS * ms.mouseSens * (ms.invertY ? -1.0f : 1.0f);
                    if (pitch >  1.5f) pitch =  1.5f;
                    if (pitch < -1.5f) pitch = -1.5f;
                }
            } else {
                // Not looking (paused / menu): drop any raw motion so it can't
                // pile up and snap the view when control resumes.
                g_rawMouseDX = 0; g_rawMouseDY = 0;
                g_cursorCentred = false;
                g_freeHave = false;
            }
            if (g_pad.Connected() && g_hasFocus && !g_menu.IsOpen() && !g_wheelOpen) {   // right stick looks
                const FeSettings& ps = g_menu.Settings();
                yaw   -= g_pad.RX() * 3.1f * ps.mouseSens * dt;
                pitch += g_pad.RY() * 2.2f * ps.mouseSens * dt * (ps.invertY ? -1.0f : 1.0f);
            }
            // Arrow keys as fallback look
            if (GameKey(VK_LEFT)  & 0x8000) yaw   += 2.5f * dt;
            if (GameKey(VK_RIGHT) & 0x8000) yaw   -= 2.5f * dt;
            if (GameKey(VK_UP)    & 0x8000) pitch += 2.0f * dt;
            if (GameKey(VK_DOWN)  & 0x8000) pitch -= 2.0f * dt;
            if (pitch >  1.5f) pitch =  1.5f;
            if (pitch < -1.5f) pitch = -1.5f;
            // Keep yaw bounded: unclamped accumulation used to reach -24000 deg
            // over a long session, quietly costing float precision.
            while (yaw >  3.14159265f) yaw -= 6.28318531f;
            while (yaw < -3.14159265f) yaw += 6.28318531f;

            // ---- WASD movement (camera-relative, on XY plane) ----
            // CJ's own speeds (root travel of WALK_player / run_player /
            // sprint_civi): run by default, Space sprints, Alt walks.
            float speed = (GameKey(VK_SPACE) & 0x8000) ? CJ_SPRINT_SPEED
                        : (GameKey(VK_MENU) & 0x8000)  ? CJ_WALK_SPEED : CJ_RUN_SPEED;
            if (g_autoWalk && g_autoGait == 1) speed = CJ_WALK_SPEED;
            if (g_autoWalk && g_autoGait == 2) speed = CJ_SPRINT_SPEED;
            float fwdX = cosf(yaw), fwdY = sinf(yaw);
            float rightX = -sinf(yaw), rightY = cosf(yaw);
            float mx = 0, my = 0;
            if (g_autoWalk) { mx += fwdX; my += fwdY; }   // --walk collision probe
            if (GameKey('W') & 0x8000) { mx += fwdX; my += fwdY; }
            if (GameKey('S') & 0x8000) { mx -= fwdX; my -= fwdY; }
            // D strafes right, A strafes left (strafe signs flipped post-mirror-fix).
            if (GameKey('D') & 0x8000) { mx -= rightX; my -= rightY; }
            if (GameKey('A') & 0x8000) { mx += rightX; my += rightY; }
            float mlen = sqrtf(mx*mx + my*my);
            if (mlen > 0.001f) { mx /= mlen; my /= mlen; }
            if (g_freeCam) { mx = my = 0.0f; mlen = 0.0f; }
            if (!g_freeCam && g_pad.Connected() && g_hasFocus && !g_menu.IsOpen() && (g_pad.LX() != 0.0f || g_pad.LY() != 0.0f)) {
                // analog: the stick's length is the pace (walk .. run), forward is up on the stick
                const float ax = fwdX * g_pad.LY() - rightX * g_pad.LX(), ay = fwdY * g_pad.LY() - rightY * g_pad.LX();
                mx = ax; my = ay;
                mlen = std::min(1.0f, sqrtf(ax * ax + ay * ay));
            }
            if (scripted) {
                // Scripted path: 5 legs x 4s, loop around Grove Street.
                scriptedTime += dt;
                int seg = (int)(scriptedTime / 4.0f);
                if (seg > 4) seg = 4;
                const float legYaw[5] = { 1.5708f, 0.0f, -1.5708f, 3.14159f, 1.5708f };
                yaw = legYaw[seg];
                pitch = 0.0f;
                mx = cosf(yaw); my = sinf(yaw);
                speed = WALK_SPEED;
                if (seg != lastScriptedSeg) {
                    // Flag the shot; taken after EndFrame so the frame is complete.
                    lastScriptedSeg = seg;
                    takeScriptedShot = true;
                    scriptedShotIdx = seg;
                }
                if (scriptedTime >= 20.0f) { g_running = false; }
            }
            if (flyover) {
                // Circular flyover: 30s per loop, shot every 30deg (12/loop), 2 loops
                flyoverTime += dt;
                float angle = flyoverTime * (2.0f * 3.14159f / 30.0f);
                playerX = FLY_CX + FLY_R * cosf(angle);
                playerY = FLY_CY + FLY_R * sinf(angle);
                playerZ = FLY_H;
                // Look at center
                float dx = FLY_CX - playerX, dy = FLY_CY - playerY, dz = 14.0f - FLY_H;
                yaw = atan2f(dy, dx);
                pitch = atan2f(dz, sqrtf(dx*dx + dy*dy));
                int shotSeg = (int)(flyoverTime / 2.5f);  // 12 shots per 30s loop
                if (shotSeg != lastFlyoverShot) {
                    lastFlyoverShot = shotSeg;
                    takeFlyoverShot = true;
                    flyoverShotIdx = shotSeg;
                }
                if (flyoverTime >= 60.0f) { g_running = false; }
            }
            // ---- Player ground height: road tops via AABB, else grass ----
            // Tracks the surface under the feet as the player walks, so road
            // grades and curbs work. Replaces the fixed-gz + dead roadPts snap,
            // which left the player buried in raised roads.
            if (worldReady) {
                g_prof.Begin(FrameProfiler::P_STREAM);
                g_world.Stream(playerX, playerY, playerZ - EYE_HEIGHT);
                g_prof.End(FrameProfiler::P_STREAM);
                g_prof.SetStreamStats(g_world.LastModelsLoaded(), g_world.LastLoadMs(),
                                      g_world.LastColBuilds(), g_world.LastColMs());
                // Physics props: step the awake ones, and let the player's body shove them.
                g_world.DynUpdate(dt, GRAVITY);
                static float s_dynPX = 0.0f, s_dynPY = 0.0f;
                if (g_driveCar < 0 && dt > 1e-4f && std::fabs(playerX - s_dynPX) < 3.0f && std::fabs(playerY - s_dynPY) < 3.0f)
                    g_world.DynPush(playerX, playerY, playerZ - EYE_HEIGHT, PLAYER_RADIUS, PLAYER_HEIGHT,
                                    (playerX - s_dynPX) / dt, (playerY - s_dynPY) / dt);
                s_dynPX = playerX; s_dynPY = playerY;
            }
            float curFeetZ = playerZ - EYE_HEIGHT;
            float gz = GetGroundHeight(playerX, playerY, curFeetZ);
            if (g_probeCollision && (frame % 25) == 0)
                std::fprintf(stderr, "[ZTRACE] f%d feet=%.2f gz=%.2f onGround=%d velZ=%.2f\n",
                             frame, curFeetZ, gz, (int)onGround, velZ);
            // ---- Swimming (CTaskSimpleSwim): in once the ped origin (feet + 1) is
            // 0.6 m under the surface, out when the feet find shallow ground ----
            float waterZ = -1e9f;
            if (cjReady && g_water.Ready() && !g_devNoClip) {
                float wz;
                if (g_water.GetWaterLevel(playerX, playerY, &wz)) waterZ = wz;
            }
            if (cjReady && !g_devNoClip) {
                const float origin = curFeetZ + 1.0f;
                if (!g_cj.Swimming() && !g_cj.Climbing() && waterZ > -1e8f && origin + 0.6f < waterZ && gz < waterZ - 1.0f) {
                    g_cj.StartSwim();
                    Log("SWIM start at (%.2f, %.2f, %.2f) water %.2f", playerX, playerY, curFeetZ, waterZ);
                } else if (g_cj.Swimming() && !g_cj.Underwater() && (waterZ < -1e8f || gz > waterZ - 1.4f)) {
                    g_cj.StopSwim();
                    Log("SWIM stop at (%.2f, %.2f, %.2f)", playerX, playerY, curFeetZ);
                }
            }
            const bool swimming = cjReady && g_cj.Swimming();
            if (swimming) { onGround = true; velZ = 0.0f; }
            // Step up onto higher ground (curbs) / start falling off edges.
            // Noclip skips the ground snap entirely so you can fly through it.
            if (!g_devNoClip && !swimming && !(cjReady && g_cj.Climbing())) {   // the climb task owns CJ's height
                if (onGround && gz > curFeetZ + 0.001f) {
                    playerZ = gz + EYE_HEIGHT;  // GetGroundHeight capped the step
                    curFeetZ = gz;
                } else if (onGround && gz < curFeetZ - 0.001f) {
                    // Peds stay on the ground they stand on: follow slopes and
                    // step down curbs; only a real drop starts a fall (which
                    // used to flash the in-air pose on every downhill frame).
                    const float hs = cjReady ? sqrtf(g_cj.VelX() * g_cj.VelX() + g_cj.VelY() * g_cj.VelY()) : speed;
                    const float stepDown = fmaxf(0.3f, hs * dt * 1.5f);
                    if (curFeetZ - gz <= stepDown) {
                        playerZ = gz + EYE_HEIGHT;
                        curFeetZ = gz;
                    } else {
                        onGround = false;  // walked off an edge - start falling
                    }
                }
            }
            // ---- AABB collision: slide along walls ----
            // Blocks only when ENTERING a box from outside; if the player is
            // already inside (bad spawn), movement is allowed so they can escape.
            auto boxHit = [&](float px, float py, const MapObject& o) -> bool {
                float fz = playerZ - EYE_HEIGHT;
                if (!o.colBoxes.empty()) {
                    // Rockstar COL boxes: horizontal circle-vs-AABB. Y is only
                    // used for step-over (low boxes) and above-head checks.
                    for (auto& b : o.colBoxes) {
                        if (b.maxZ <= fz + STEP_OVER) continue;    // step over
                        if (b.minZ >= fz + PLAYER_HEIGHT) continue; // above head
                        float cx = px < b.minX ? b.minX : (px > b.maxX ? b.maxX : px);
                        float cy = py < b.minY ? b.minY : (py > b.maxY ? b.maxY : py);
                        float dx = px - cx, dy = py - cy;
                        if (dx*dx + dy*dy < PLAYER_RADIUS*PLAYER_RADIUS) return true;
                    }
                    return false;
                }
                if (px + PLAYER_RADIUS < o.cMinX || px - PLAYER_RADIUS > o.cMaxX) return false;
                if (py + PLAYER_RADIUS < o.cMinY || py - PLAYER_RADIUS > o.cMaxY) return false;
                if (fz + PLAYER_HEIGHT < o.cMinZ || fz > o.cMaxZ) return false;
                return true;
            };
            auto hitsSolid = [&](float ox, float oy, float nx_, float ny_) -> bool {
                if (g_devNoClip) return false;  // F4
                if (worldReady) {
                    bool blocked = g_world.HitsSolidCircle(nx_, ny_, playerZ - EYE_HEIGHT,
                                                           ox, oy, PLAYER_RADIUS, STEP_OVER, PLAYER_HEIGHT);
                    if (blocked) ++g_solidBlockCount;
                    return blocked;
                }
                for (auto& o : mapObjects) {
                    if (!o.solid) continue;
                    if (boxHit(nx_, ny_, o) && !boxHit(ox, oy, o)) return true;
                }
                return false;
            };
            if (g_devNoClip) {
                // ---- F4 noclip: free fly along the view direction, no collision ----
                float nsp = (GameKey(VK_SHIFT) & 0x8000) ? 60.0f : 20.0f;
                float lcp = cosf(pitch), lsp = sinf(pitch);
                float nfx = cosf(yaw) * lcp, nfy = sinf(yaw) * lcp, nfz = lsp;
                float nrx = -sinf(yaw), nry = cosf(yaw);
                float dxm = 0.0f, dym = 0.0f, dzm = 0.0f;
                if (GameKey('W') & 0x8000) { dxm += nfx; dym += nfy; dzm += nfz; }
                if (GameKey('S') & 0x8000) { dxm -= nfx; dym -= nfy; dzm -= nfz; }
                if (GameKey('D') & 0x8000) { dxm -= nrx; dym -= nry; }
                if (GameKey('A') & 0x8000) { dxm += nrx; dym += nry; }
                if (GameKey(VK_SPACE)   & 0x8000) dzm += 1.0f;
                if (GameKey(VK_CONTROL) & 0x8000) dzm -= 1.0f;
                float nl = sqrtf(dxm*dxm + dym*dym + dzm*dzm);
                if (nl > 0.0001f) {
                    playerX += (dxm / nl) * nsp * dt;
                    playerY += (dym / nl) * nsp * dt;
                    playerZ += (dzm / nl) * nsp * dt;
                }
                velZ = 0.0f;
                onGround = true;
                if (cjReady) {   // CJ floats in the skydive pose
                    PlayerPed::Input pin;
                    pin.dt = dt;
                    pin.stickX = dxm;
                    pin.stickY = dym;
                    const float hl = sqrtf(dxm * dxm + dym * dym);
                    if (hl > 1.0f) { pin.stickX /= hl; pin.stickY /= hl; }
                    pin.noclip = true;
                    g_cj.Update(pin);
                }
            } else {
            // ---- Jump (not while CJ is landing or getting up) ----
            // Edge-triggered like GTA SA: one press is one jump, holding Shift
            // does not re-jump on landing (CPad::JumpJustDown).
            bool jumped = false;
            static bool s_prevJumpKey = false;
            const bool jumpKeyDown = (GameKey(VK_SHIFT) & 0x8000) != 0;   // PC SA: Shift jumps
            const bool jumpJustDown = (jumpKeyDown && !s_prevJumpKey) || (g_autoWalk && g_autoGait == 3);
            s_prevJumpKey = jumpKeyDown;
            // ---- Climb: CTaskSimpleJump / CTaskSimpleInAir -> TestForClimb ----
            static const ClimbScanFn climbScan = [](int mode, const float* ped, float rot, float* pos, float* orot) {
                return ClimbScan(g_world, mode, ped, rot, pos, orot);
            };
            bool climbStarted = false;
            if (swimming && worldReady && jumpJustDown) {   // BACK_TO_SURFACE: haul out onto a ledge
                if (g_cj.TryStartClimb(playerX, playerY, playerZ - EYE_HEIGHT, false, climbScan)) {
                    g_cj.StopSwim();
                    climbStarted = true;
                    Log("SWIM climb out at (%.2f, %.2f)", playerX, playerY);
                }
            }
            if (cjReady && !g_devNoClip && worldReady && !g_cj.Climbing() && !swimming) {
                const float feet = playerZ - EYE_HEIGHT;
                if (onGround && !g_cj.Busy() && jumpJustDown && !g_cj.FistLock())   // launch from the ground
                    climbStarted = g_cj.TryStartClimb(playerX, playerY, feet, true, climbScan);
                else if (!onGround && velZ > -10.0f && sqrtf(g_cj.VelX() * g_cj.VelX() + g_cj.VelY() * g_cj.VelY()) > 0.05f * 50.0f)
                    climbStarted = g_cj.TryStartClimb(playerX, playerY, feet, false, climbScan);   // grab mid-jump
                if (climbStarted) { onGround = true; velZ = 0.0f; Log("CLIMB start at (%.2f, %.2f, %.2f) %s", playerX, playerY, feet, onGround ? "" : ""); }
            }
            if (onGround && !swimming && !(cjReady && g_cj.Busy()) && jumpJustDown && !climbStarted && !(cjReady && g_cj.FistLock())) {
                velZ = JUMP_VEL * Stats::FatMuscleMod(2);
                if (getenv("GTA_STATDBG")) Log("JUMP velZ %.3f mod %.3f", velZ, Stats::FatMuscleMod(2));
                onGround = false;
                jumped = true;
            }
            // ---- CJ: the animation decides how fast he moves ----
            // (CPed extracts the root motion of the blended walk/run/sprint
            // clips, so he speeds up and slows down with them.)
            float vx = mx * speed, vy = my * speed;
            if (cjReady) {
                g_cj.SetBreathMax(Stats::FatMuscleMod(8));
                g_cj.SetJumpMod(Stats::FatMuscleMod(2));
                g_cj.SetClimbMod(Stats::FatMuscleMod(1));
                PlayerPed::Input pin;
                pin.dt = dt;
                pin.stickX = mx;
                pin.stickY = my;
                pin.walk = (GameKey(VK_MENU) & 0x8000) || (g_autoWalk && g_autoGait == 1);
                {   // CPlayerPed::ControlButtonSprint / HandleSprintEnergy (type 0, on foot): tapping charges m_fMoveSpeed,
                    // sprinting drains m_fTimeCanRun (capped by FatMuscleMod(7)) until it hits -150.
                    static bool s_prevSprintKey = false;
                    static float s_charge = 0.0f;
                    float& s_timeCanRun = g_timeCanRun;
                    bool& s_runInit = g_runInit;
                    const bool auto2 = g_autoWalk && g_autoGait == 2;
                    const bool sprintKey = ((GameKey(VK_SPACE) & 0x8000) != 0) || auto2;
                    const float step = dt * 50.0f;
                    const float cap = Stats::FatMuscleMod(7);
                    if (!s_runInit) { s_runInit = true; s_timeCanRun = cap; }   // SetInitialState
                    s_timeCanRun = std::min(s_timeCanRun, cap);
                    const bool moving = mlen > 0.001f;
                    if (sprintKey && !s_prevSprintKey) s_charge = std::min(10.0f, s_charge + 4.0f);
                    else if (sprintKey) s_charge = std::max(1.0f, s_charge - 0.7f * step);
                    else s_charge = std::max(0.0f, s_charge - 0.2f * step);
                    s_prevSprintKey = sprintKey;
                    bool sprint = false;
                    if (moving && onGround && (s_charge > 0.0f || s_timeCanRun > 0.0f) && (sprintKey || s_charge > 0.0f)) {
                        const float rate = s_charge <= 5.0f ? 1.0f : 0.5f;
                        if (!Cheat::AdrenalineOn()) s_timeCanRun = std::max(-150.0f, s_timeCanRun - step * rate);   // HandleSprintEnergy: adrenaline never tires
                        if (s_timeCanRun <= -150.0f) s_charge = 0.0f;
                        else sprint = true;
                    } else {
                        // CPlayerPed::ProcessControl: running adds 0.15 per step, walking 0.5 (HandleSprintEnergy(false, 1) / 2), standing nothing
                        if (moving) s_timeCanRun = std::min(cap, s_timeCanRun + step * (pin.walk ? 0.5f : 0.15f));
                    }
                    pin.sprint = sprint;
                    g_stamina = std::clamp(s_timeCanRun / cap, 0.0f, 1.0f);
                    if (getenv("GTA_STATDBG") && (frame % 50) == 0) Log("SPRINT charge %.2f timeCanRun %.1f cap %.1f sprint %d", s_charge, s_timeCanRun, cap, (int)sprint);
                }
                pin.onGround = onGround;
                pin.jumped = jumped;
                pin.velZ = velZ;
                pin.heightAboveGround = 1e3f;
                if (worldReady) {
                    const float g = g_world.GetGroundHeight(playerX, playerY, playerZ - EYE_HEIGHT);
                    if (g > ColLoader::kNoGround * 0.5f) pin.heightAboveGround = playerZ - EYE_HEIGHT - g;
                }
                pin.blocked = cjBlocked;
                pin.rewrite = g_menu.Settings().movementStyle == 1;
                pin.camYaw = yaw;
                pin.camPitch = pitch;
                {   // classic third person aims through the offset crosshair (camera ray of the last frame's eye)
                    const bool classicTp = g_menu.Settings().cameraStyle == PlayerCamera::kClassic && (cjReady || thirdPerson);   // CJ always has the player camera
                    g_aspect = (float)WIDTH / (float)HEIGHT;
                    g_chairX = classicTp ? 0.53f : 0.5f;
                    g_chairY = classicTp ? 0.4f : 0.5f;
                    pin.haveAimRay = (cjReady || thirdPerson) && !g_playerCam.FirstPerson(g_menu.Settings().cameraStyle);
                    CrosshairRay(yaw, pitch, pin.aimDir);
                    for (int k = 0; k < 3; k++) pin.aimEye[k] = g_aimEye[k];
                }
                pin.firstPerson = g_playerCam.FirstPerson(g_menu.Settings().cameraStyle);
                pin.worldX = playerX; pin.worldY = playerY; pin.feetZ = playerZ - EYE_HEIGHT;
                pin.jumpHeld = jumpKeyDown;
                pin.jumpDown = jumpJustDown;
                {
                    static bool s_prevDuck = false;
                    const bool duckKey = (GameKey('C') & 0x8000) != 0;
                    pin.duckJust = duckKey && !s_prevDuck;
                    pin.duckHeld = duckKey;
                    if (g_peds.Ready()) {
                        pin.groundTarget = g_peds.FallenNear(playerX, playerY, g_cj.Heading(), 1.4f);
                        pin.whipTarget = g_peds.WhipTarget(playerX, playerY, g_cj.Heading());
                        g_stealthPed = g_peds.StealthTarget(playerX, playerY, g_cj.Heading());
                        pin.stealthTarget = g_stealthPed >= 0;
                        if (g_stealthPed >= 0) { pin.stealthPos[0] = g_peds.X((size_t)g_stealthPed); pin.stealthPos[1] = g_peds.Y((size_t)g_stealthPed); }
                    }
                    s_prevDuck = duckKey;
                    // Right mouse or Z aims. The rewrite also aims while the fire button
                    // is held (trackpads have no easy right click) and for a moment after.
                    static float s_fireAim = 0.0f;
                    const bool fireDown = ((GameKey(VK_LBUTTON) | GameKey(VK_RETURN)) & 0x8000) != 0;
                    const int gi = g_cj.GunIndex();
                    const bool heavyFire = gi >= 16 && gi <= 19;   // flamethrower, minigun, spray can, extinguisher fire without aiming
                    if ((pin.rewrite || heavyFire) && gi >= 0 && fireDown) s_fireAim = 0.6f;
                    else s_fireAim = std::max(0.0f, s_fireAim - dt);
                    pin.aim = ((GameKey(VK_RBUTTON) | GameKey('Z')) & 0x8000) != 0 || s_fireAim > 0.0f;
                }
                {
                    // Lock-on (CPlayerPed::FindWeaponLockOnTarget @ 0060dc50): retail locks only without the
                    // mouse aim (joypad or the classic control setting): aiming with the pad trigger, or
                    // [Input] LockOn=1 for the mouse. The target holds while it is alive and in range.
                    static const bool s_lockMouse = GetPrivateProfileIntA("Input", "LockOn", 0, g_iniPath) != 0 || getenv("GTA_LOCKON");
                    const bool padAim = g_pad.Connected() && g_pad.LT() > 0.35f;
                    const bool lockMode = pin.aim && g_cj.GunIndex() >= 0 && (padAim || s_lockMouse);
                    const float chest[3] = {playerX, playerY, playerZ - EYE_HEIGHT + 1.15f};
                    auto visible = [&](size_t i) {
                        float tp[3];
                        g_peds.TargetPoint(i, false, tp);
                        return !worldReady || g_world.RayCast(chest[0], chest[1], chest[2], tp[0], tp[1], tp[2]) >= 0.99f;
                    };
                    const float range = g_cj.TargetRange();
                    auto pick = [&](float facing, int skip, int side) {   // side 0: best; -1 / +1: next to the left / right
                        int best = -1;
                        float bestP = -10000.0f, bestA = 10.0f;
                        for (size_t i = 0; i < g_peds.Count(); i++) {
                            if ((int)i == skip || !g_peds.Alive(i)) continue;
                            const float dx = g_peds.X(i) - playerX, dy = g_peds.Y(i) - playerY;
                            const float dz = g_peds.Z(i) - (playerZ - EYE_HEIGHT);
                            const float dist = side == 0 ? sqrtf(dx * dx + dy * dy + dz * dz) : sqrtf(dx * dx + dy * dy);   // EvaluateTarget measures in 3D
                            if (dist > range) continue;
                            float ang = atan2f(dy, dx) - facing;
                            while (ang > 3.14159265f) ang -= 6.2831853f;
                            while (ang < -3.14159265f) ang += 6.2831853f;
                            if (side != 0) {   // FindNextWeaponLockOnTarget: the nearest by angle on that side
                                if (ang * (float)side >= 0.0f || fabsf(ang) > 3.0f) continue;   // + is left
                                if (fabsf(ang) < bestA && visible(i)) { bestA = fabsf(ang); best = (int)i; }
                                continue;
                            }
                            if (fabsf(ang) >= 140.0f * 0.008726646f) continue;   // FindWeaponLockOnTarget: inside PLAYER_MAX_TARGET_VIEW_ANGLE (140 deg) around the facing
                            // CPlayerPed::EvaluateTarget: priority 0.1 * (1 - angle / 140) / dist^(1/4)
                            float mult = 1.0f - fabsf(ang) * 57.29578f / 140.0f;
                            if (dist > 1.0f) mult /= sqrtf(sqrtf(dist));
                            const float pr = 0.1f * mult;
                            if (pr > bestP && visible(i)) { bestP = pr; best = (int)i; }
                        }
                        return best;
                    };
                    static bool s_wasLock = false, s_prevL = false, s_prevR = false;
                    if (!lockMode) g_lockPed = -1;
                    else if (!s_wasLock) {
                        // the facing is the walk stick direction past 60 / 128, else the ped's heading
                        float facing = g_cj.Heading();
                        if (g_pad.Connected() && (fabsf(g_pad.LX()) > 60.0f / 128.0f || fabsf(g_pad.LY()) > 60.0f / 128.0f))
                            facing = atan2f(sinf(yaw) * g_pad.LY() - cosf(yaw) * g_pad.LX(), cosf(yaw) * g_pad.LY() + sinf(yaw) * g_pad.LX());
                        g_lockPed = pick(facing, -1, 0);
                        Log("LOCKON pick facing %.1f deg -> ped %d", facing * 57.29578f, g_lockPed);
                    }
                    else if (g_lockPed >= 0) {
                        const float dx = g_peds.X(g_lockPed) - playerX, dy = g_peds.Y(g_lockPed) - playerY;
                        if ((size_t)g_lockPed >= g_peds.Count() || !g_peds.Alive(g_lockPed) || sqrtf(dx * dx + dy * dy) > range) g_lockPed = -1;
                    }
                    // shift target left / right: pad d-pad (CPad::ShiftTargetLeftJustDown / Right)
                    const bool lDown = g_pad.Connected() && g_pad.Down(Gamepad::DPAD_LEFT), rDown = g_pad.Connected() && g_pad.Down(Gamepad::DPAD_RIGHT);
                    if (lockMode && g_lockPed >= 0 && ((lDown && !s_prevL) || (rDown && !s_prevR))) {
                        const float cdx = g_peds.X(g_lockPed) - playerX, cdy = g_peds.Y(g_lockPed) - playerY;
                        const int n = pick(atan2f(cdy, cdx), g_lockPed, lDown ? 1 : -1);
                        if (n >= 0) g_lockPed = n;
                    }
                    s_prevL = lDown; s_prevR = rDown;
                    s_wasLock = lockMode;
                    pin.lockOn = g_lockPed >= 0;
                    if (pin.lockOn) {
                        // CPlayerPed::ProcessControl: the head (bone 5) within the weapon's head range, else the spine
                        const float dx = g_peds.X(g_lockPed) - playerX, dy = g_peds.Y(g_lockPed) - playerY;
                        g_peds.TargetPoint(g_lockPed, sqrtf(dx * dx + dy * dy) <= g_cj.TargetHeadRange(), pin.lockPos);
                        for (int k = 0; k < 3; k++) g_lockPos[k] = pin.lockPos[k];
                        // CCam::Process_AimWeapon with a target: the angles turn toward it at 0.1 rad per 1/50 s
                        const float ex = pin.lockPos[0] - g_aimEye[0], ey = pin.lockPos[1] - g_aimEye[1], ez = pin.lockPos[2] - g_aimEye[2];
                        const float wantYaw = atan2f(ey, ex), wantPitch = atan2f(ez, sqrtf(ex * ex + ey * ey));
                        float dyaw = wantYaw - yaw;
                        while (dyaw > 3.14159265f) dyaw -= 6.2831853f;
                        while (dyaw < -3.14159265f) dyaw += 6.2831853f;
                        const float step = 0.1f * 50.0f * dt;
                        yaw += std::clamp(dyaw, -step, step);
                        pitch += std::clamp(wantPitch - pitch, -step, step);
                        pin.camYaw = yaw; pin.camPitch = pitch;
                    }
                }
                {
                    static bool s_prevFire = false, s_prevReload = false, s_prevNext = false;
                    const bool fireKey = ((GameKey(VK_LBUTTON) | GameKey(VK_RETURN)) & 0x8000) != 0 && !g_menu.IsOpen() && !g_wheelOpen;
                    // Classic uses the retail PC bindings: E / wheel down next weapon, Q / wheel up previous,
                    // no reload key (an empty clip reloads by itself), no weapon wheel.
                    const bool classicKeys = !pin.rewrite;
                    const bool relKey = !classicKeys && (GameKey('R') & 0x8000) != 0;
                    const bool nextKey = !classicKeys && (GameKey('Q') & 0x8000) != 0;
                    pin.fireHeld = fireKey;
                    pin.fireJust = fireKey && !s_prevFire;
                    pin.reloadJust = relKey && !s_prevReload;
                    pin.autoReload = classicKeys || g_menu.Settings().autoReload;
                    if (classicKeys && !g_menu.IsOpen()) {
                        static bool s_prevE = false, s_prevQ = false;
                        const bool eKey = (GameKey('E') & 0x8000) != 0, qKey = (GameKey('Q') & 0x8000) != 0;
                        if (eKey && !s_prevE) g_cj.NextGun();
                        if (qKey && !s_prevQ) g_cj.PrevGun();
                        if (!g_wheelOpen) {
                            for (; g_wheelSteps < 0; g_wheelSteps++) g_cj.NextGun();
                            for (; g_wheelSteps > 0; g_wheelSteps--) g_cj.PrevGun();
                        }
                        s_prevE = eKey; s_prevQ = qKey;
                    }
                    const int s_wheelNotch = g_wheelOpen ? g_wheelSteps : 0;   // the open wheel scrolls inside a category
                    g_wheelSteps = 0;   // unused notches never pile up
                    // Q: a tap cycles the guns, holding it brings up the weapon wheel.
                    static float s_qHeld = 0.0f;
                    if (nextKey) {
                        s_qHeld += dt / (g_wheelOpen ? 0.2f : 1.0f);   // dt is slowed while the wheel is up
                        if (!g_wheelOpen && s_qHeld > 0.18f && g_cj.GunCount() > 0 && !g_menu.IsOpen()) {
                            g_wheelOpen = true;
                            g_wheelX = g_wheelY = 0.0f;
                            g_wheelSel = g_cj.WeaponIndex() + 1;
                            const int nItems = g_cj.GunCount() + 1;
                            for (int c = 0; c < 8; c++) g_wheelPick[c] = -1;
                            for (int i = nItems - 1; i >= 0; i--) if (WheelItemOwned(i)) g_wheelPick[WheelItemCat(i)] = i;   // first weapon of each category
                            g_wheelCat = WheelItemCat(g_wheelSel);
                            g_wheelPick[g_wheelCat] = g_wheelSel;
                        }
                    } else {
                        if (g_wheelOpen) {
                            if (g_wheelSel >= 0) g_cj.SelectGun(g_wheelSel - 1);
                            g_wheelOpen = false;
                        } else if (s_prevNext && s_qHeld <= 0.18f) {
                            g_cj.NextGun();
                        }
                        s_qHeld = 0.0f;
                    }
                    if (g_wheelOpen) {
                        if (g_pad.Connected() && (g_pad.RX() != 0.0f || g_pad.RY() != 0.0f)) { g_wheelX = g_pad.RX(); g_wheelY = -g_pad.RY(); }
                        const float wl = sqrtf(g_wheelX * g_wheelX + g_wheelY * g_wheelY);
                        const int n = g_cj.GunCount() + 1;
                        if (wl > 0.3f) {   // the stick / mouse direction picks the category
                            float ang = atan2f(g_wheelY, g_wheelX) + 1.5707963f;   // 0 = up, clockwise
                            while (ang < 0.0f) ang += 6.2831853f;
                            g_wheelCat = (int)(ang / 6.2831853f * 8.0f + 0.5f) % 8;
                        }
                        if (s_wheelNotch) {   // scroll cycles the weapons inside the category
                            std::vector<int> inCat;
                            for (int i = 0; i < n; i++) if (WheelItemOwned(i) && WheelItemCat(i) == g_wheelCat) inCat.push_back(i);
                            if (!inCat.empty()) {
                                int at = 0;
                                for (size_t k = 0; k < inCat.size(); k++) if (inCat[k] == g_wheelPick[g_wheelCat]) at = (int)k;
                                at = ((at + (s_wheelNotch < 0 ? 1 : -1)) % (int)inCat.size() + (int)inCat.size()) % (int)inCat.size();
                                g_wheelPick[g_wheelCat] = inCat[at];
                            }
                        }
                        g_wheelSel = g_wheelPick[g_wheelCat];   // -1: nothing in this category, the current weapon stays
                    }
                    s_prevFire = fireKey; s_prevReload = relKey; s_prevNext = nextKey;
                }
                pin.waterZ = waterZ;
                {
                    static bool s_prevDive = false;
                    const bool diveKey = (GameKey('C') & 0x8000) != 0;
                    pin.diveJust = diveKey && !s_prevDive;
                    s_prevDive = diveKey;
                    pin.diveDown = diveKey;
                    pin.diveUp = jumpKeyDown;
                }
                pin.exitPressed = (GameKey('F') & 0x8000) != 0;   // CPad::GetExitVehicle lets go
                if (g_driveCar >= 0) {
                    const VehicleInstance& dc = g_vehicles.Car((size_t)g_driveCar);
                    pin.carSteer = -dc.steer;   // retail: positive = left
                    pin.carSpeed = std::sqrt(dc.vx * dc.vx + dc.vy * dc.vy);
                    pin.carReverse = dc.speed < -0.3f || (RawGameKey('S') & 0x8000) != 0;
                    g_cj.SetDrivingSkill(Stats::Get(Stats::STAT_DRIVING_SKILL) / 10.0f);
                }
                pin.jetBrake = pin.duckHeld;
                pin.jetStop = (RawGameKey(VK_MBUTTON) & 0x8000) != 0;   // look behind
                if (const char* of = getenv("GTA_OUTFIT"); of && frame == 5) {   // dev: "slot:model[:texture],..." e.g. torso:pimp:pimp
                    PedClothesDesc d = g_cj.Outfit();
                    static const char* slots[] = {"torso", "head", "hands", "legs", "feet", "necklace", "watch", "glasses", "hat", "extra1"};
                    std::string list = of;
                    for (size_t p = 0; p < list.size();) {
                        size_t e = list.find(',', p);
                        std::string it = list.substr(p, e == std::string::npos ? e : e - p);
                        p = e == std::string::npos ? list.size() : e + 1;
                        size_t c1 = it.find(':'), c2 = c1 == std::string::npos ? c1 : it.find(':', c1 + 1);
                        if (c1 == std::string::npos) continue;
                        const std::string sl = it.substr(0, c1), mo = it.substr(c1 + 1, c2 == std::string::npos ? c2 : c2 - c1 - 1);
                        const std::string tx = c2 == std::string::npos ? "" : it.substr(c2 + 1);
                        for (int i = 0; i < 10; i++) {
                            if (sl != slots[i]) continue;
                            const ClothesTexturePart tp = Clothes::GetDependentTexture((ClothesModelPart)i);
                            if (tx.empty()) d.SetModel(mo.c_str(), (ClothesModelPart)i);
                            else d.SetTextureAndModel(tx.c_str(), mo.c_str(), tp);
                        }
                    }
                    g_cj.SetOutfit(d);
                }
                if (static PedClothesDesc saved = g_cj.Outfit(); !(saved == g_cj.Outfit()) && frame > 5) {   // a changed outfit is kept
                    saved = g_cj.Outfit();
                    if (!getenv("GTA_OUTFIT")) SaveOutfit(saved);
                }
                if (const char* ty = getenv("GTA_TYPE"); ty && frame >= 10 && (size_t)(frame - 10) < strlen(ty)) Cheat::Push(ty[frame - 10]);   // dev: type a cheat, one letter a frame
                if (getenv("GTA_MEGAPUNCH") && frame == 5) Cheat::ToggleMegaPunch();   // dev: the cheat without typing it
                if (getenv("GTA_ADRENALINE") && frame == 5) { Cheat::StartAdrenaline(); g_timeCanRun = Stats::FatMuscleMod(7); }
                { const bool was = Cheat::AdrenalineOn(); Cheat::Tick(dt); if (was && !Cheat::AdrenalineOn()) Log("CHEAT adrenaline ended"); }
                if (getenv("GTA_JETPACK") && frame == 5) g_cj.StartJetpack();   // dev: the jetpack without typing the cheat
                const Cheat::Id cheatHit = Cheat::Take();
                switch (cheatHit) {   // CCheat::AddToCheatString hit
                case Cheat::Jetpack:
                    if (!g_cj.Jetpacking()) g_cj.StartJetpack();
                    if (g_hudReady) g_hud.ShowMessage(g_hud.Text().Get("CHEAT1"), 3.0f);
                    break;
                case Cheat::Adrenaline:   // CCheat::AdrenalineCheat toggles it
                    if (Cheat::AdrenalineOn()) Cheat::ClearAdrenaline();
                    else { Cheat::StartAdrenaline(); g_timeCanRun = Stats::FatMuscleMod(7); }   // ResetSprintEnergy
                    Log("CHEAT adrenaline %d", (int)Cheat::AdrenalineOn());
                    if (g_hudReady) g_hud.ShowMessage(g_hud.Text().Get("CHEAT1"), 3.0f);
                    break;
                case Cheat::Fat: case Cheat::Muscle: case Cheat::Skinny: case Cheat::WeaponSkills: case Cheat::VehicleSkills: case Cheat::Stamina: {
                    switch (cheatHit) {   // CCheat::FatCheat / MuscleCheat / SkinnyCheat / WeaponSkillsCheat / VehicleSkillsCheat
                    case Cheat::Fat: Stats::Set(Stats::STAT_FAT, 1000.0f); break;
                    case Cheat::Muscle: Stats::Set(Stats::STAT_MUSCLE, 1000.0f); break;
                    case Cheat::Stamina: Stats::Set(Stats::STAT_STAMINA, 1000.0f); break;
                    case Cheat::Skinny: Stats::Set(Stats::STAT_FAT, 0.0f); Stats::Set(Stats::STAT_MUSCLE, 0.0f); break;
                    case Cheat::WeaponSkills:
                        for (int st = Stats::STAT_PISTOL_SKILL; st <= Stats::STAT_RIFLE_SKILL; st++) Stats::Set(st, 1000.0f);
                        break;
                    default:
                        for (int st : {Stats::STAT_DRIVING_SKILL, Stats::STAT_FLYING_SKILL, Stats::STAT_BIKE_SKILL, Stats::STAT_CYCLING_SKILL}) Stats::Set(st, 1000.0f);
                    }
                    if (cheatHit >= Cheat::Fat && cheatHit <= Cheat::Skinny && g_driveCar < 0)   // RebuildPlayer unless driving
                        g_cj.SetBodyShape(Stats::Get(Stats::STAT_FAT) / 1000.0f, Stats::Get(Stats::STAT_MUSCLE) / 1000.0f);
                    Log("CHEAT stats fat %.0f muscle %.0f stamina %.0f pistol %.0f driving %.0f", Stats::Get(Stats::STAT_FAT), Stats::Get(Stats::STAT_MUSCLE), Stats::Get(Stats::STAT_STAMINA), Stats::Get(Stats::STAT_PISTOL_SKILL), Stats::Get(Stats::STAT_DRIVING_SKILL));
                    break;
                }
                case Cheat::HealthArmorMoney:
                    g_cj.RestoreVitals();
                    g_money += 250000;
                    if (g_hudReady) g_hud.ShowMessage(g_hud.Text().Get("CHEAT1"), 3.0f);
                    break;
                default: break;
                }
                pin.climbScan = climbScan;
                pin.footIk = pin.rewrite && !getenv("GTA_NOFOOTIK");
                if (worldReady) pin.groundAt = [](float x, float y, float z) { return g_world.GetGroundHeight(x, y, z); };
                g_cj.Update(pin);
                if (!scripted && !flyover) { vx = g_cj.VelX(); vy = g_cj.VelY(); }
                if (g_cj.TakeJetDrop()) {   // CTaskSimpleJetPack::DropJetPack: the pack lies on the ground as a pickup
                    const float fz = playerZ - EYE_HEIGHT;
                    float p[3] = {playerX, playerY, fz + 0.4f};   // CPickups::CreatePickupCoorsCloseToCoors: 1.5 m out at a random angle, on the ground, in clear sight
                    for (int tries = 0; tries < 32; tries++) {
                        const float ang = (float)(rand() & 0xff) * 0.024543693f;
                        const float qx = playerX + std::sin(ang) * 1.5f, qy = playerY + std::cos(ang) * 1.5f;
                        const float gz = g_world.GetGroundHeight(qx, qy, fz + 1.0f);
                        if (gz < -500.0f) continue;
                        int kd = 0; float hn = 0.0f;
                        if (g_world.RayCast(playerX, playerY, fz + 0.3f, qx, qy, gz + 0.9f, &kd, &hn) < 1.0f) continue;
                        p[0] = qx; p[1] = qy; p[2] = gz + 0.5f;
                        break;
                    }
                    const int sl = g_pickups.Add(p, Pickups::Jetpack, 0, Pickups::OnceTimeoutSlow, 0, false, (uint32_t)g_bulletMs);
                    Log("PICKUP ADD slot %d jetpack at %.2f %.2f %.2f", sl, p[0], p[1], p[2]);
                }
                {   // CTaskSimpleJetPack owns the vertical speed while it flies
                    float jv;
                    if (g_cj.JetVelZ(jv)) { onGround = false; velZ = jv; }
                    float nz[2][3];
                    if (g_cj.Jetpacking() && g_cj.JetThrust() > 0.5f && g_cj.JetNozzles(playerX, playerY, playerZ - EYE_HEIGHT, nz))
                        for (int n = 0; n < 2; n++) g_fx.Smoke(nz[n], 0.0f);
                }
                if (g_cj.Swimming()) {   // ride the surface, or dive
                    onGround = true; velZ = 0.0f;
                    const float origin = playerZ - EYE_HEIGHT + 1.0f;
                    if (g_cj.Underwater()) {
                        const float vz = g_cj.SwimVelZ();
                        playerZ += vz * dt;
                        if (waterZ > -1e8f && vz > 0.0f && origin + 0.65f > waterZ) g_cj.SwimSurface();
                    } else if (waterZ > -1e8f) {
                        const float target = waterZ - g_cj.SwimSubmerge() + 0.7f;   // eye = origin + 0.7
                        playerZ += std::clamp((target - playerZ) * 8.0f * dt, -6.0f * dt, 6.0f * dt);
                    }
                    if (gz > -1e5f && playerZ < gz + EYE_HEIGHT) playerZ = gz + EYE_HEIGHT;   // the sea floor
                }
                {   // CTaskSimpleClimb moves CJ itself
                    float sx, sy, sz, rv[3];
                    if (g_cj.TakeClimbSnap(sx, sy, sz)) { playerX = sx; playerY = sy; playerZ = sz + EYE_HEIGHT; }
                    if (g_cj.TakeClimbRelease(rv)) { velZ = rv[2]; onGround = false; }
                    if (g_cj.Climbing()) { onGround = true; velZ = 0.0f; playerZ += g_cj.ClimbVelZ() * dt; }
                }
                if (getenv("GTA_CJLOG")) {   // dev: per-frame animation trace
                    char cd[200] = "";
                    if (g_cj.Climbing()) g_cj.ClimbDebug(cd, sizeof cd);
                    else g_cj.FootDebug(cd, sizeof cd);
                    Log("CJ f%d clip=%s xy=%.2f,%.2f v=%.2f ratio=%.2f ground=%d hag=%.2f z=%.2f %s", frame, g_cj.ClipName(), playerX, playerY,
                        sqrtf(vx * vx + vy * vy), g_cj.MoveBlendRatio(), (int)onGround, pin.heightAboveGround, playerZ - EYE_HEIGHT, cd);
                }
            }
            if (cjReady && g_cj.Climbing()) { playerX += vx * dt; playerY += vy * dt; vx = vy = 0.0f; }   // ignores the ledge's collision
            const float ox = playerX, oy = playerY;
            float nx = playerX + vx * dt;
            float ny = playerY + vy * dt;
            const bool movedX = !hitsSolid(playerX, playerY, nx, playerY);
            if (movedX) playerX = nx;
            const bool movedY = !hitsSolid(playerX, playerY, playerX, ny);
            if (movedY) playerY = ny;
            if (!movedX && !movedY && (vx != 0.0f || vy != 0.0f)) {
                // Angled wall: both axis steps hit it, so CJ would stick. Slide
                // along the surface instead, like GTA SA: try headings fanned
                // out from the intended one (smallest turn first), keeping
                // only the component of speed along each.
                const float sp = sqrtf(vx * vx + vy * vy), hd = atan2f(vy, vx);
                for (int k = 1; k <= 6; k++)
                    for (int sgn = -1; sgn <= 1; sgn += 2) {
                        const float a = hd + sgn * k * 0.2618f;   // 15 degree steps up to 90
                        const float len = sp * cosf(k * 0.2618f) * dt;
                        const float sx = playerX + cosf(a) * len, sy = playerY + sinf(a) * len;
                        if (!hitsSolid(playerX, playerY, sx, sy)) { playerX = sx; playerY = sy; goto slid; }
                    }
                slid:;
            }
            {   // pushing into a wall (rewrite handling drops its momentum)
                const float want = sqrtf(vx * vx + vy * vy) * dt;
                const float got = sqrtf((playerX - ox) * (playerX - ox) + (playerY - oy) * (playerY - oy));
                cjBlocked = want > 1e-4f && got < 0.3f * want;
            }
            if (g_autoWalk) {   // collision-probe trace
                static float wStartX = playerX, wStartY = playerY, wPrevX = playerX, wPrevY = playerY;
                if ((frame % 40) == 0) {
                    float cum = sqrtf((playerX-wStartX)*(playerX-wStartX)+(playerY-wStartY)*(playerY-wStartY));
                    float d40 = sqrtf((playerX-wPrevX)*(playerX-wPrevX)+(playerY-wPrevY)*(playerY-wPrevY));
                    std::fprintf(stderr, "[WALK] f%d pos=(%.2f,%.2f,%.2f) cum=%.2f d40=%.3f %s\n",
                        frame, playerX, playerY, playerZ, cum, d40, d40 < 0.05f ? "<-- BLOCKED" : "");
                    wPrevX = playerX; wPrevY = playerY;
                }
            }

            // ---- Gravity (feet rest on gz) ----
            if (!onGround) {
                velZ -= GRAVITY * dt;
                if (cjReady) g_cj.ParaVelZ(velZ);   // the parachute script sets the fall speed
                if (cjReady) g_cj.JetVelZ(velZ);
                if (cjReady && g_hudReady && g_cj.TakeParaHelp()) g_hud.Subtitle(g_hud.Text().Get("PARA_01"), 1.0f);
                playerZ += velZ * dt;
                if (playerZ <= gz + EYE_HEIGHT) {
                    playerZ = gz + EYE_HEIGHT; velZ = 0; onGround = true;
                }
            }

            if (DevKeyPressed('O')) { g_crowdTarget = g_crowdTarget ? 0 : 12; Log("DEV: crowd target %d", g_crowdTarget); }
            g_peds.Populate(g_renderer, playerX, playerY, playerZ - EYE_HEIGHT, g_crowdTarget,
                            [](float x, float y, float z) { return g_world.GetGroundHeight(x, y, z); });
            if (g_crowdTarget && (frame % 60) == 0) Log("PEDS: %zu actors", g_peds.Count());
            g_peds.Update(dt, [](float x, float y, float z) { return g_world.GetGroundHeight(x, y, z); },
                          [](float x, float y, float ox, float oy, float z) {
                              if (g_vehicles.Nearest(x, y, 2.4f) >= 0) return true;   // keep clear of cars
                              return g_world.HitsSolidCircle(x, y, z, ox, oy, 0.4f, 0.3f, 1.8f); },
                          [&] { Peds::Player pl; pl.x = playerX; pl.y = playerY; pl.threat = cjReady && g_cj.GunIndex() >= 0 && g_cj.Aiming(); return pl; }());
            g_speech.Tick(dt);
            if (cjReady) {
                // CStats::UpdateFatAndMuscleStats @ 0055c470 with ar_stats.dat: exercise counts time at its rate;
                // every STAT_TIMELIMIT_FAT_ADJUST (150 s) of it burns STAT_DEC_FAT (15) fat and builds
                // STAT_INC_BODY_MUSCLE (10) muscle, or with no fat left costs STAT_DEC_BODY_MUSCLE (15) muscle.
                static float s_fatCounterMs = 0.0f;
                const int rate = g_cj.ExerciseRate();
                {
                    static const float s_statTime = getenv("GTA_STATTIME") ? (float)atof(getenv("GTA_STATTIME")) : 1.0f;   // dev: speed up the activity counters
                    float carSpd = 0.0f;
                    if (g_driveCar >= 0) { const VehicleInstance& dc = g_vehicles.Car((size_t)g_driveCar); carSpd = std::sqrt(dc.vx * dc.vx + dc.vy * dc.vy) / 50.0f; }
                    {   // CGameLogic::Update: CStats::UpdateRespectStat(0) and UpdateSexAppealStat
                        static int s_lastCar = -1;   // the car CJ last drove (DAT_00b79530)
                        if (g_driveCar >= 0) s_lastCar = g_driveCar;
                        const int rd = Stats::UpdateRespect(g_money, 0.0f, 0, false);
                        if (rd != 0 && g_hudReady) g_hud.StatUpdate(Stats::Get(Stats::STAT_TOTAL_RESPECT) / 10.0f, rd > 0);
                        if (rd != 0) Log("STATS: respect %s to %.1f", rd > 0 ? "rose" : "fell", Stats::Get(Stats::STAT_TOTAL_RESPECT));
                        bool have = false; float dist = 0.0f, hp = 0.0f; int cls = 0;
                        if (s_lastCar >= 0 && (size_t)s_lastCar < g_vehicles.Count()) {
                            const VehicleInstance& lc = g_vehicles.Car((size_t)s_lastCar);
                            have = true; hp = lc.health;
                            if (const char* dh = getenv("GTA_STATCARHP")) hp = (float)atof(dh);   // dev: pretend the car has this health
                            dist = std::sqrt((lc.x - playerX) * (lc.x - playerX) + (lc.y - playerY) * (lc.y - playerY));
                            if (const VehicleInfo* vi = g_vehicles.Info(lc.model)) cls = vi->vclass;
                            if (const char* dd = getenv("GTA_STATDIST"); dd && frame >= 200) dist = (float)atof(dd);   // dev: pretend the car is this far
                        }
                        Stats::UpdateSexAppeal(have, dist, hp, cls, false);
                        if (getenv("GTA_STATDBG") && (frame % 60) == 0) Log("STATDBG: last car %d dist %.1f health %.0f class %d player %.1f %.1f", s_lastCar, dist, hp, cls, playerX, playerY);
                    }
                    if (const char* si = getenv("GTA_STATINC"); si && frame == 40) {   // dev: "hurt:hp" then "id:amount" raises a stat
                        int id = 0; float amt = 0;
                        if (sscanf(si, "%d:%f", &id, &amt) == 2) {
                            if (const char* h = getenv("GTA_STATHURT")) g_cj.Hurt((float)atof(h));
                            Log("STATINC: health %.2f/%.2f before", g_cj.Health(), g_cj.MaxHealth());
                            Stats::Increment(id, amt);
                            Log("STATINC: stat %d = %.0f, health %.2f/%.2f after, fat %.0f", id, Stats::Get(id), g_cj.Health(), g_cj.MaxHealth(), Stats::Get(Stats::STAT_FAT));
                        }
                    }
                    if (getenv("GTA_MODTEST") && frame == 30) for (int m = 0; m < 16; m++) Log("STATS: modifier %d = %.4f", m, Stats::FatMuscleMod(m));   // dev
                    if (const char* ss = getenv("GTA_STATSET"); ss && frame == 80) {   // dev: "id:value" overwrites a stat
                        int id = 0; float val = 0;
                        if (sscanf(ss, "%d:%f", &id, &val) == 2) { Stats::Set(id, val); Log("STATSET: stat %d = %.0f", id, val); }
                    }
                    if (getenv("GTA_STATDBG") && (frame % 60) == 0) Log("STATDBG: respect %.1f sex %.1f hp %.1f/%.1f rate %d drive %d spd %.3f thr %d", Stats::Get(Stats::STAT_TOTAL_RESPECT), Stats::Get(Stats::STAT_SEX_APPEAL), g_cj.Health(), g_cj.MaxHealth(), rate, g_driveCar, carSpd, (int)((RawGameKey('W') & 0x8000) != 0));
                    int vehKind = 0;   // CPlayerInfo::Process: the stat update follows the vehicle appearance
                    if (g_driveCar >= 0) {
                        const VehicleInfo* dvi = g_vehicles.Info(g_vehicles.Car((size_t)g_driveCar).model);
                        const std::string dt2 = dvi ? dvi->type : "car";
                        vehKind = dt2 == "bike" ? 2 : dt2 == "plane" || dt2 == "heli" ? 3 : dt2 == "boat" || dt2 == "bmx" ? 4 : 1;
                    }
                    {   // CPlayerInfo::Process: the distance stats grow by the mover's speed every step (here metres per second x dt)
                        const float step = dt * 50.0f;
                        if (g_driveCar < 0) {
                            const float v = std::sqrt(g_cj.VelX() * g_cj.VelX() + g_cj.VelY() * g_cj.VelY()) * dt;
                            if (g_cj.Jetpacking()) Stats::Increment(Stats::STAT_TIME_ON_JETPACK, step * 16.0f);
                            else if (v > 0.0f && g_cj.Swimming()) { if (mlen > 0.001f) Stats::Increment(Stats::STAT_DISTANCE_TRAVELLED_BY_SWIMMING, v); }
                            else if (v > 0.0f) Stats::Increment(Stats::STAT_DISTANCE_TRAVELLED_ON_FOOT, v);
                        } else {
                            const VehicleInstance& dc = g_vehicles.Car((size_t)g_driveCar);
                            const VehicleInfo* dvi = g_vehicles.Info(dc.model);
                            std::string ty = dvi ? dvi->type : "car";
                            if (const char* ft = getenv("GTA_STATVEHTYPE")) ty = ft;   // dev: boats do not float yet, count this car as another type
                            const float v = std::sqrt(dc.vx * dc.vx + dc.vy * dc.vy) * dt;
                            if (dc.model == 457) Stats::Increment(Stats::STAT_DISTANCE_TRAVELLED_BY_GOLF_CART, v);
                            else if (ty == "heli") Stats::Increment(Stats::STAT_DISTANCE_TRAVELLED_BY_HELICOPTER, v);
                            else if (ty == "plane") { if (v / dt / 50.0f > 0.2f) Stats::Increment(Stats::STAT_DISTANCE_TRAVELLED_BY_PLANE, v); }
                            else if (ty == "bike") Stats::Increment(Stats::STAT_DISTANCE_TRAVELLED_BY_MOTORBIKE, v);
                            else if (ty == "boat") Stats::Increment(Stats::STAT_DISTANCE_TRAVELLED_BY_BOAT, v);
                            else if (ty == "car") Stats::Increment(Stats::STAT_DISTANCE_TRAVELLED_BY_CAR, v);
                        }
                        if (getenv("GTA_STATDBG") && (frame % 60) == 0) Log("DIST foot %.1f swim %.1f car %.1f golf %.1f bike %.1f boat %.1f heli %.1f plane %.1f jet %.0f", Stats::Get(Stats::STAT_DISTANCE_TRAVELLED_ON_FOOT), Stats::Get(Stats::STAT_DISTANCE_TRAVELLED_BY_SWIMMING), Stats::Get(Stats::STAT_DISTANCE_TRAVELLED_BY_CAR), Stats::Get(Stats::STAT_DISTANCE_TRAVELLED_BY_GOLF_CART), Stats::Get(Stats::STAT_DISTANCE_TRAVELLED_BY_MOTORBIKE), Stats::Get(Stats::STAT_DISTANCE_TRAVELLED_BY_BOAT), Stats::Get(Stats::STAT_DISTANCE_TRAVELLED_BY_HELICOPTER), Stats::Get(Stats::STAT_DISTANCE_TRAVELLED_BY_PLANE), Stats::Get(Stats::STAT_TIME_ON_JETPACK));
                    }
                    if (g_driveCar >= 0 && g_runInit) {   // CPlayerPed::ProcessControl: in a vehicle (not a BMX) HandleSprintEnergy(false, 1)
                        const VehicleInfo* bvi = g_vehicles.Info(g_vehicles.Car((size_t)g_driveCar).model);
                        if (!bvi || bvi->type != "bmx") {
                            const float cap7 = Stats::FatMuscleMod(7);
                            if (g_timeCanRun < cap7) g_timeCanRun = std::min(cap7, g_timeCanRun + dt * 50.0f * 0.5f);
                            g_stamina = std::clamp(g_timeCanRun / cap7, 0.0f, 1.0f);
                            if (getenv("GTA_STATDBG") && (frame % 50) == 0) Log("SPRINT car timeCanRun %.1f cap %.1f", g_timeCanRun, cap7);
                        }
                    }
                    const int up = Stats::UpdateActivity(dt * s_statTime, rate, g_cj.Swimming(), g_cj.Underwater(), vehKind, carSpd, true);
                    if (up == Stats::STAT_MAX_HEALTH) g_cj.SetMaxHealth(Stats::FatMuscleMod(9));
                    if (up >= 0) {
                        if (g_hudReady) g_hud.StatUpdate(Stats::Get(up) / 10.0f);
                        Log("STATS: stat %d now %.0f", up, Stats::Get(up));
                    }
                }
                if (rate > 0) {
                    s_fatCounterMs += dt * 1000.0f * (float)rate / 10.0f;
                    if (s_fatCounterMs > Stats::React(Stats::R_TIMELIMIT_FAT_ADJUST) * 1000.0f) {
                        s_fatCounterMs = 0.0f;
                        if (Stats::Get(Stats::STAT_FAT) > 0.0f) {
                            Stats::Decrement(Stats::STAT_FAT, Stats::React(Stats::R_DEC_FAT));
                            Stats::Increment(Stats::STAT_MUSCLE, Stats::React(Stats::R_INC_BODY_MUSCLE));
                        } else {
                            Stats::Decrement(Stats::STAT_MUSCLE, Stats::React(Stats::R_DEC_BODY_MUSCLE));
                        }
                        const float fatStat = Stats::Get(Stats::STAT_FAT), muscleStat = Stats::Get(Stats::STAT_MUSCLE);
                        g_cj.SetBodyShape(fatStat / 1000.0f, muscleStat / 1000.0f);   // CClothes::RebuildPlayerIfNeeded
                        char v[16];
                        snprintf(v, sizeof(v), "%d", (int)(fatStat / 10.0f));
                        WritePrivateProfileStringA("Player", "Fat", v, g_iniPath);
                        snprintf(v, sizeof(v), "%d", (int)(muscleStat / 10.0f));
                        WritePrivateProfileStringA("Player", "Muscle", v, g_iniPath);
                        Log("STATS: fat %.0f muscle %.0f", fatStat, muscleStat);
                    }
                }
            }
            {   // [Stats] S<id>: written when a value changed, at most every 5 s
                static float s_statSave = 0.0f;
                if ((s_statSave += dt) > 5.0f) { s_statSave = 0.0f; Stats::Save(g_iniPath); }
            }
            for (const Peds::SpeechEvent& e : g_peds.TakeSpeech()) {   // ped lines, quieter and panned with distance
                const float dx = g_peds.X(e.actor) - camPosX, dy = g_peds.Y(e.actor) - camPosY;
                const float dist = sqrtf(dx * dx + dy * dy);
                const float side = (dx * sinf(yaw) - dy * cosf(yaw)) / std::max(1.0f, dist);
                g_speech.SayGen(e.actor, g_peds.Voice(e.actor), e.ctx, std::clamp(1.0f - dist / 30.0f, 0.0f, 1.0f), std::clamp(side, -1.0f, 1.0f));
            }
            if (cjReady && g_cj.Aiming() && g_cj.GunIndex() >= 0 && strcmp(g_cj.GunId(), "PISTOL_SILENCED") != 0) {
                // CPlayerPed::Compute3rdPersonMouseTarget: CJ says CTX_GLOBAL_PULL_GUN when his aim finds a ped
                float tt = 0.0f;
                const int hitPed = g_lockPed >= 0 ? g_lockPed : g_peds.Trace(g_aimEye, g_aimDir, g_cj.TargetRange() + 8.0f, &tt, nullptr);
                static int s_lastTarget = -1;
                if (hitPed >= 0 && hitPed != s_lastTarget) g_speech.SayPlayer(CTX_PULL_GUN);
                s_lastTarget = hitPed;
            }
            g_vehicles.UpdateDamage(dt, [](const float* p, float dark) { g_fx.Smoke(p, dark); }, [](const float* p) { g_fx.Fire(p); },
                                    [](const float* p) { g_carBooms.push_back({p[0], p[1], p[2]}); });
            if (g_driveCar >= 0 && g_vehicles.Car((size_t)g_driveCar).wrecked) {   // thrown out of a car that blew up
                const VehicleInstance& wc = g_vehicles.Car((size_t)g_driveCar);
                playerX = wc.x + 2.5f; playerY = wc.y; playerZ = wc.z + EYE_HEIGHT; velZ = 3.0f; onGround = false;
                g_driveCar = -1; g_audio.EngineStop(); g_radio.Tune(-1);
                if (cjReady) g_cj.LeaveVehicle();
                g_cj.Hurt(60.0f);
            }
            // ---- Cars: F enters the nearest one, F again gets out; the car drives from W/S/A/D ----
            {
                static bool s_prevF = false;
                const bool fDown = (RawGameKey('F') & 0x8000) != 0 && !g_menu.IsOpen();
                const bool fJust = fDown && !s_prevF;
                s_prevF = fDown;
                if (fJust && g_driveCar < 0) {
                    const int n = g_vehicles.Nearest(playerX, playerY, 6.5f);
                    if (n >= 0 && !g_vehicles.Car((size_t)n).wrecked && g_vehicles.Car((size_t)n).health > 0.0f) {
                        g_driveCar = n; Log("DRIVE: got in car %d", n); g_radio.Tune(g_radioStation);
                        float sp[3]; bool low = false;
                        if (cjReady && g_vehicles.SeatWorld((size_t)n, sp, &low)) g_cj.EnterVehicle(low);
                    }
                } else if (fJust && g_driveCar >= 0) {
                    VehicleInstance& c = g_vehicles.Car((size_t)g_driveCar);
                    playerX = c.x + std::cos(c.heading) * 2.4f; playerY = c.y + std::sin(c.heading) * 2.4f;
                    playerZ = c.z + EYE_HEIGHT; velZ = 0.0f; onGround = true;
                    c.speed = 0.0f;
                    g_driveCar = -1;
                    if (cjReady) g_cj.LeaveVehicle();
                    g_audio.EngineStop();
                    g_radio.Tune(-1);
                    Log("DRIVE: got out");
                }
                if (g_driveCar >= 0 && !g_menu.IsOpen()) {
                    static bool s_prevQ = false, s_prevE = false;   // Q and E change the radio station
                    const bool qd = (RawGameKey('Q') & 0x8000) != 0, ed = (RawGameKey('E') & 0x8000) != 0;
                    if ((qd && !s_prevQ) || (ed && !s_prevE)) {
                        g_radio.Step(ed && !s_prevE ? 1 : -1);
                        g_radioStation = g_radio.Station();
                        if (g_hudReady) g_hud.ShowMessage(g_radio.StationName(g_radioStation), 2.5f);
                    }
                    s_prevQ = qd; s_prevE = ed;
                    const float th = (RawGameKey('W') & 0x8000) ? 1.0f : 0.0f;
                    const float br = (RawGameKey('S') & 0x8000) ? 1.0f : 0.0f;
                    const float st = ((RawGameKey('D') & 0x8000) ? 1.0f : 0.0f) - ((RawGameKey('A') & 0x8000) ? 1.0f : 0.0f);
                    const bool hb = (RawGameKey(VK_SPACE) & 0x8000) != 0;
                    g_vehicles.Step((size_t)g_driveCar, dt, th, br, st, hb,
                                    [](float x, float y, float z) { return g_world.GetGroundHeight(x, y, z); },
                                    [](float x, float y, float ox, float oy, float z) { return g_world.HitsSolidCircle(x, y, z + 0.4f, ox, oy, 1.1f, 0.3f, 1.2f, true); });
                    const VehicleInstance& c = g_vehicles.Car((size_t)g_driveCar);
                    if (worldReady) g_world.DynVehicle(c.x, c.y, c.z, c.vx, c.vy, 1.3f, 1500.0f);
                    if (const VehicleInfo* vi = g_vehicles.Info(c.model)) {
                        const HandlingData* hd = g_vehicles.Handling(vi->handling);
                        g_audio.Engine(c.model, th, c.speed, (hd ? hd->maxVelocity : 160.0f) / 3.6f, hd ? hd->numGears : 5, dt);
                    }
                    {
                        float sp[3];
                        if (cjReady && g_vehicles.SeatWorld((size_t)g_driveCar, sp)) g_cj.SetSeat(sp, c.heading + 1.5707963f);
                    }
                    if (VehicleGuns::Kind gk = VehicleGuns::KindOf(c.model); gk != VehicleGuns::Kind::None && !c.wrecked) {   // TankControl / FireTruckControl
                        static bool s_prevGun = false;
                        const bool gunKey = ((GameKey(VK_LBUTTON) | GameKey(VK_RETURN)) & 0x8000) != 0;
                        const float cf[3] = {std::cos(yaw) * std::cos(pitch), std::sin(yaw) * std::cos(pitch), std::sin(pitch)};   // the camera front
                        VehicleInstance& gc = g_vehicles.Car((size_t)g_driveCar);
                        if (gk == VehicleGuns::Kind::Tank) {
                            VehicleGuns::AimTank(gc, cf, dt);
                            VehicleGuns::TankShot ts;
                            if (gunKey && !s_prevGun && VehicleGuns::FireTank(g_vehicles, (size_t)g_driveCar, g_bulletMs, &ts)) {
                                float d[3] = {ts.end[0] - ts.origin[0], ts.end[1] - ts.origin[1], ts.end[2] - ts.origin[2]};
                                const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                                for (float& v : d) v /= len;
                                float tw = worldReady ? g_world.RayCast(ts.origin[0], ts.origin[1], ts.origin[2], ts.end[0], ts.end[1], ts.end[2]) : 1.0f;
                                float tp = 2.0f, tv = 2.0f, tt = 0.0f; bool head = false;
                                int pi2 = -1, vi2 = -1;
                                if (g_peds.Ready()) { pi2 = g_peds.Trace(ts.origin, d, len, &tt, &head); if (pi2 >= 0) tp = tt / len; }
                                if (g_vehicles.Ready()) { vi2 = g_vehicles.Trace(ts.origin, d, len, &tt); if (vi2 >= 0 && vi2 != g_driveCar) tv = tt / len; else vi2 = -1; }
                                const float tmin = std::min(tw, std::min(tp, tv));
                                float boom[3];   // the shell blows 4% short of whatever it hit, else at the end of the ray
                                for (int k = 0; k < 3; k++) boom[k] = tmin < 1.0f ? ts.origin[k] + (ts.end[k] - ts.origin[k]) * tmin * 0.96f : ts.end[k];
                                g_tankBooms.push_back({boom[0], boom[1], boom[2]});
                                Log("TANK FIRE from %.1f %.1f %.1f dir %.2f %.2f %.2f hit %s at %.1f m -> boom %.1f %.1f %.1f", ts.origin[0], ts.origin[1], ts.origin[2], d[0], d[1], d[2],
                                    tmin >= 1.0f ? "none" : tmin == tw ? "world" : tmin == tp ? "ped" : "car", tmin * len, boom[0], boom[1], boom[2]);
                                for (int k = 0; k < 3; k++) { const float sp[3] = {ts.origin[0] + d[0] * (1.0f + (float)k), ts.origin[1] + d[1] * (1.0f + (float)k), ts.origin[2] + d[2] * (1.0f + (float)k)}; g_fx.Smoke(sp, 0.2f); }
                            }
                        } else {
                            VehicleGuns::AimWater(gc, cf, dt);
                            if (gunKey) VehicleGuns::FireWater(g_vehicles, (size_t)g_driveCar, g_waterCannons, g_bulletMs, dt);
                        }
                        if (getenv("GTA_VEHGUNDBG") && frame % 15 == 0) Log("VEHGUN car %d model %d doomV %.3f doomH %.3f fire %d", g_driveCar, gc.model, gc.doomV, gc.doomH, (int)gunKey);
                        s_prevGun = gunKey;
                    }
                    playerX = c.x; playerY = c.y; playerZ = c.z + EYE_HEIGHT + 0.7f;   // the camera rides above the roof
                    velZ = 0.0f; onGround = true; vx = vy = 0.0f;
                }
            }
            }


            // ---- V cycles the camera views (Camera.h) ----
            const int camStyle = g_menu.Settings().cameraStyle;
            bool vDown = (GameKey('V') & 0x8000) != 0;
            if (vDown && !prevV) {
                g_playerCam.Cycle();
                Log("CAMERA: %s", PlayerCamera::ViewName(camStyle, g_playerCam.View()));
            }
            prevV = vDown;

            // ---- Build view matrix from yaw/pitch ----
            float cp = cosf(pitch), sp = sinf(pitch);
            float dirX = cosf(yaw) * cp, dirY = sinf(yaw) * cp, dirZ = sp;
            g_hideCjHead = false;
            float nearClip = 0.5f;
            if (cjReady && !scripted && !flyover) {
                PlayerCamera::Input ci;
                ci.dt = dt;
                ci.style = camStyle;
                ci.yaw = yaw;
                ci.pitch = pitch;
                ci.feet[0] = playerX; ci.feet[1] = playerY; ci.feet[2] = playerZ - EYE_HEIGHT;
                ci.climbing = g_cj.Climbing();
                ci.aimWeapon = camStyle == PlayerCamera::kClassic && g_cj.Aiming() && g_cj.GunIndex() >= 0 &&
                               !g_playerCam.FirstPerson(camStyle);
                {   // Process_AimWeapon: AK-47 / M4 zoom to 50, the country rifle to 35
                    const std::string gid = g_cj.GunId();
                    ci.aimFov = gid == "AK47" || gid == "M4" ? 50.0f : gid == "COUNTRYRIFLE" ? 35.0f : 70.0f;
                }
                {   // sniper / camera / rocket launchers aim first person: wheel zooms 70 -> 15 (camera 3)
                    const int gt = g_cj.GunIndex() >= 0 ? g_cj.GunType(g_cj.GunIndex()) : 0;
                    g_scoped = g_cj.Aiming() && (gt == 34 || gt == 43 || gt == 35 || gt == 36);   // the country rifle aims over the shoulder (Process_AimWeapon)
                    if (g_scoped) {
                        if (gt == 35 || gt == 36) g_scopeFov = 70.0f; else if (g_wheelSteps) { g_scopeFov = std::clamp(g_scopeFov * std::pow(1.084f, -(float)g_wheelSteps), gt == 43 ? 3.0f : 15.0f, 70.0f); g_wheelSteps = 0; }
                        ci.scope = true;
                        ci.scopeFov = g_scopeFov;
                        ci.aimWeapon = false;
                    } else g_scopeFov = 70.0f;
                }
                ci.pedHeading = g_cj.Heading();
                if (ci.aimWeapon) { pitch = std::clamp(pitch, -1.5533f, 0.7854f); ci.pitch = pitch; }
                ci.haveSpine = g_cj.BoneWorldPos(3, playerX, playerY, playerZ - EYE_HEIGHT, ci.spine);
                ci.haveHead = g_cj.BoneWorldPos(5, playerX, playerY, playerZ - EYE_HEIGHT, ci.head);
                if (worldReady && !g_devNoClip)
                    ci.ray = [](const float a[3], const float b[3]) {
                        return g_world.RayCast(a[0], a[1], a[2], b[0], b[1], b[2]);
                    };
                PlayerCamera::Output co = g_playerCam.Update(ci);
                g_camFovScale = co.fovScale;
                if (getenv("GTA_CAMDBG") && frame % 30 == 0)
                    Log("CAMDBG aim %d eye %.2f %.2f %.2f feet %.2f %.2f %.2f dist %.2f fovScale %.2f", (int)ci.aimWeapon, co.eye[0], co.eye[1], co.eye[2],
                        ci.feet[0], ci.feet[1], ci.feet[2], sqrtf((co.eye[0] - ci.feet[0]) * (co.eye[0] - ci.feet[0]) + (co.eye[1] - ci.feet[1]) * (co.eye[1] - ci.feet[1])), co.fovScale);
                if (camStyle == PlayerCamera::kRewrite) {   // rewrite shake: displaces the camera along its own axes
                    static uint32_t shakeMs = 0;
                    static const char* devShake = getenv("GTA_SHAKE");   // dev: strength fired once at frame 20
                    shakeMs += (uint32_t)(dt * 1000.0f);
                    for (const auto& q : g_shakeQ) g_shake.TriggerAt(q[0], q[1], shakeMs);
                    g_shakeQ.clear();
                    if (devShake && frame == 20) g_shake.Trigger((float)atof(devShake), shakeMs);
                    float fwd[3] = {co.dir[0], co.dir[1], co.dir[2]};
                    float rgt[3] = {fwd[1], -fwd[0], 0.0f};
                    const float rl = std::sqrt(rgt[0] * rgt[0] + rgt[1] * rgt[1]);
                    if (rl > 1e-4f) { rgt[0] /= rl; rgt[1] /= rl; } else { rgt[0] = 1.0f; rgt[1] = 0.0f; }
                    const float up[3] = {rgt[1] * fwd[2] - rgt[2] * fwd[1], rgt[2] * fwd[0] - rgt[0] * fwd[2],
                                         rgt[0] * fwd[1] - rgt[1] * fwd[0]};
                    float off[3];
                    g_shake.Update(shakeMs, dt * 50.0f, rgt, fwd, up, off);
                    for (int i = 0; i < 3; i++) co.eye[i] += off[i];
                }
                D3DMATRIX view = MatrixLookAt(co.eye[0], co.eye[1], co.eye[2], co.eye[0] + co.dir[0],
                                              co.eye[1] + co.dir[1], co.eye[2] + co.dir[2], 0, 0, 1);
                if (const char* gc = getenv("GTA_GUNCAM")) {   // dev: look at the gun hand from the side (1), above (2) the front (3) or behind (4)
                    float hp[3];
                    if (g_cj.BoneWorldPos(24, playerX, playerY, playerZ - EYE_HEIGHT, hp)) {
                        const float hl = std::sqrt(co.dir[0] * co.dir[0] + co.dir[1] * co.dir[1]) + 1e-6f;
                        const float fx = co.dir[0] / hl, fy = co.dir[1] / hl, rx = fy, ry = -fx;
                        const int m = atoi(gc);
                        float e[3] = {hp[0] + rx * 1.3f, hp[1] + ry * 1.3f, hp[2] + 0.1f};
                        if (m == 2) { e[0] = hp[0] - fx * 0.01f; e[1] = hp[1] - fy * 0.01f; e[2] = hp[2] + 1.8f; }
                        if (m == 5) { e[0] = hp[0] + rx * 2.2f; e[1] = hp[1] + ry * 2.2f; e[2] = hp[2] + 0.3f; }
                        if (m == 4) { e[0] = hp[0] - fx * 1.6f + rx * 0.5f; e[1] = hp[1] - fy * 1.6f + ry * 0.5f; e[2] = hp[2] + 0.3f; }   // from behind
                        if (m == 3) { e[0] = hp[0] + fx * 1.5f + rx * 0.3f; e[1] = hp[1] + fy * 1.5f + ry * 0.3f; e[2] = hp[2] + 0.1f; }
                        view = m == 2 ? MatrixLookAt(e[0], e[1], e[2], hp[0], hp[1], hp[2], fx, fy, 0)   // forward is up on screen
                                      : MatrixLookAt(e[0], e[1], e[2], hp[0], hp[1], hp[2], 0, 0, 1);
                        co.eye[0] = e[0]; co.eye[1] = e[1]; co.eye[2] = e[2];
                    }
                }
                g_renderer.SetViewMatrix(view);
                g_lastView = view;
                camPosX = co.eye[0]; camPosY = co.eye[1]; camPosZ = co.eye[2];
                for (int i = 0; i < 3; i++) { g_aimEye[i] = co.eye[i]; g_aimDir[i] = co.dir[i]; }
                if (!co.firstPerson) CrosshairRay(yaw, pitch, g_aimDir);   // bullets go through the crosshair
                if (g_lockPed >= 0) {   // locked on: the shot goes at the target (CWeapon::Fire with the target entity)
                    float d[3] = {g_lockPos[0] - co.eye[0], g_lockPos[1] - co.eye[1], g_lockPos[2] - co.eye[2]};
                    const float l = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                    if (l > 1e-3f) for (int k = 0; k < 3; k++) g_aimDir[k] = d[k] / l;
                }
                g_hideCjHead = co.hideHead;
                nearClip = co.nearClip;
            } else if (!thirdPerson) {
                float tx = playerX + dirX;
                float ty = playerY + dirY;
                float tz = playerZ + dirZ;
                D3DMATRIX view = MatrixLookAt(playerX, playerY, playerZ, tx, ty, tz, 0, 0, 1);
                g_renderer.SetViewMatrix(view);
                camPosX = playerX; camPosY = playerY; camPosZ = playerZ;
                g_aimEye[0] = playerX; g_aimEye[1] = playerY; g_aimEye[2] = playerZ;
                g_aimDir[0] = dirX; g_aimDir[1] = dirY; g_aimDir[2] = dirZ;
            } else {
                // Over-shoulder: camera pulled back along view dir, lifted a touch.
                const float CAM_DIST = 4.5f;
                float cx = playerX - dirX * CAM_DIST;
                float cy = playerY - dirY * CAM_DIST;
                float cz = playerZ - dirZ * CAM_DIST + 1.0f;
                float tx = playerX + dirX * 8.0f;
                float ty = playerY + dirY * 8.0f;
                float tz = playerZ + dirZ * 8.0f;
                D3DMATRIX view = MatrixLookAt(cx, cy, cz, tx, ty, tz, 0, 0, 1);
                g_renderer.SetViewMatrix(view);
                camPosX = cx; camPosY = cy; camPosZ = cz;
                g_aimEye[0] = cx; g_aimEye[1] = cy; g_aimEye[2] = cz;
                g_aimDir[0] = dirX; g_aimDir[1] = dirY; g_aimDir[2] = dirZ;
            }
            if (g_freeCam && !g_menu.IsOpen()) {
                float fx = dirX, fy = dirY, fz = dirZ;
                const float rx = fy, ry = -fx;   // right of the view, flat
                const float rl = sqrtf(rx * rx + ry * ry) + 1e-6f;
                float sp = (GameKey(VK_SHIFT) & 0x8000) ? 24.0f : (GameKey(VK_CONTROL) & 0x8000) ? 1.2f : 6.0f;
                float mvx = 0, mvy = 0, mvz = 0;
                if (GameKey('W') & 0x8000) { mvx += fx; mvy += fy; mvz += fz; }
                if (GameKey('S') & 0x8000) { mvx -= fx; mvy -= fy; mvz -= fz; }
                if (GameKey('D') & 0x8000) { mvx += rx / rl; mvy += ry / rl; }
                if (GameKey('A') & 0x8000) { mvx -= rx / rl; mvy -= ry / rl; }
                if (GameKey(VK_SPACE) & 0x8000) mvz += 1.0f;
                if (g_pad.Connected()) {
                    mvx += fx * g_pad.LY() + rx / rl * g_pad.LX(); mvy += fy * g_pad.LY() + ry / rl * g_pad.LX(); mvz += fz * g_pad.LY();
                    if (g_pad.Down(Gamepad::A)) mvz += 1.0f;
                    if (g_pad.Down(Gamepad::B)) mvz -= 1.0f;
                }
                g_fcPos[0] += mvx * sp * dt; g_fcPos[1] += mvy * sp * dt; g_fcPos[2] += mvz * sp * dt;
                D3DMATRIX view = MatrixLookAt(g_fcPos[0], g_fcPos[1], g_fcPos[2], g_fcPos[0] + fx, g_fcPos[1] + fy, g_fcPos[2] + fz, 0, 0, 1);
                g_renderer.SetViewMatrix(view);
                camPosX = g_fcPos[0]; camPosY = g_fcPos[1]; camPosZ = g_fcPos[2];
                nearClip = 0.1f;
                g_hideCjHead = false;
            }
            if (scmRun && g_sg.cam.active && !g_freeCam) {   // the script camera (SET_FIXED_CAMERA_POSITION / POINT_CAMERA_AT_POINT)
                const ScriptCamera& sc = g_sg.cam;
                float sx = 0, sy = 0, sz = 0;
                if (sc.shakeT > 0.0f && sc.shakeDur > 0.0f) {
                    const float a = sc.shakeAmp * 0.002f * (sc.shakeT / sc.shakeDur);
                    sx = a * std::sin(scmClock * 61.0); sy = a * std::sin(scmClock * 47.0 + 1.0); sz = a * std::sin(scmClock * 53.0 + 2.0);
                }
                D3DMATRIX view = MatrixLookAt(sc.pos[0] + sx, sc.pos[1] + sy, sc.pos[2] + sz, sc.look[0], sc.look[1], sc.look[2],
                                              sc.cutscene ? sc.up[0] : 0.0f, sc.cutscene ? sc.up[1] : 0.0f, sc.cutscene ? sc.up[2] : 1.0f);
                g_renderer.SetViewMatrix(view);
                camPosX = sc.pos[0]; camPosY = sc.pos[1]; camPosZ = sc.pos[2];
                if (sc.nearClip > 0.0f) nearClip = sc.nearClip;
            }
            // Rewrite: the view widens a little while sprinting (smoothed so it eases in and out).
            static float s_fovKick = 0.0f;
            {
                const float tgt = (g_menu.Settings().movementStyle == 1 && cjReady && !g_freeCam)
                                      ? std::clamp(g_cj.MoveBlendRatio() - 2.0f, 0.0f, 1.0f) * 6.0f : 0.0f;
                s_fovKick += (tgt - s_fovKick) * std::min(1.0f, dt * 5.0f);
            }
            float wantFov = (g_menu.Settings().movementStyle == 1 ? g_menu.Settings().fov : 70.0f) + s_fovKick;
            if (scmRun && g_sg.cam.active && g_sg.cam.cutscene && g_sg.cam.fov > 1.0f) wantFov = g_sg.cam.fov;   // the cutscene's own FOV
            if (g_camFovScale != 1.0f)   // the aim camera's weapon zoom
                wantFov = 2.0f * atanf(tanf(wantFov * 3.14159265f / 360.0f) * g_camFovScale) * 360.0f / 3.14159265f;
            if (nearClip != g_nearClip || wantFov != g_fovDeg) {   // first person needs a near plane at the eyes
                g_nearClip = nearClip;
                g_fovDeg = wantFov;
                g_renderer.SetProjMatrix(MatrixPerspectiveFov(g_fovDeg * 3.14159f / 180.0f, (float)WIDTH / (float)HEIGHT,
                                                              g_nearClip, 3000.0f));
            }
        }

        // Paused: dark clear colour and no sky so the menu reads clearly.
        if (g_devPaused) g_renderer.BeginFrame(0.03f, 0.03f, 0.05f);
        else             g_renderer.BeginFrame(0.4f, 0.6f, 0.9f);

        // Sky polys + sun, then the moon, stars and low clouds. Drawn first, no depth.
        if (playMode && !g_devPaused) {
            g_renderer.RenderSky(camPosX, camPosY, camPosZ);
            if (g_timecycOk)
                g_sky.Render(g_renderer, camPosX, camPosY, camPosZ, clockHour,
                             g_timecyc.WeatherName(g_weatherIdx), g_tcNow, g_gameDay);
        }

        // Distance fog in play mode (hides the sea's far edge and the
        // streaming/draw-distance cut, colour = timecyc SkyBot). TABLE (pixel)
        // fog, not vertex fog: it's computed per pixel from depth so big ground
        // triangles and sparsely-tessellated LODs haze smoothly like retail SA,
        // instead of only fogging at the few far vertices.
        if (playMode) {
            dev->SetRenderState(D3DRS_FOGENABLE, TRUE);
            dev->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
            dev->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
            dev->SetRenderState(D3DRS_FOGCOLOR, g_fogColor);
            dev->SetRenderState(D3DRS_FOGSTART, *(DWORD*)&fogStart);
            dev->SetRenderState(D3DRS_FOGEND, *(DWORD*)&fogEnd);
        }

        // Player box draw (3rd person); used between the opaque and alpha
        // passes so a fence/window in front of CJ occludes him correctly.
        auto drawPlayerBox = [&]() {
            if (!playMode) return;
            float feetZ = playerZ - EYE_HEIGHT;
            if (g_driveCar >= 0) {   // CJ sits at the car's driver seat
                if (cjReady && !scripted && !flyover && g_cj.InVehicle()) {
                    float sp[3];
                    g_cj.SeatPlacement(sp);
                    g_cj.SetHideGun(g_scoped);
                    g_cj.Draw(g_renderer, sp[0], sp[1], sp[2] - 1.0f, false);
                }
                return;
            }
            if (g_aimTest && g_dummy.Ready()) {
                for (const AimDummy& d : g_aim.Dummies()) {
                    if (!d.alive()) continue;
                    g_dummy.SetHeading(atan2f(playerY - d.y, playerX - d.x));
                    g_dummy.Draw(g_renderer, d.x, d.y, d.z);
                }
            }
            if (g_cutHideCj) return;
            if (cjReady && !scripted && !flyover) { g_cj.SetHideGun(g_scoped); g_cj.Draw(g_renderer, playerX, playerY, feetZ, g_hideCjHead); return; }
            if (!thirdPerson) return;
            if (!playerMesh) return;
            float hy = yaw * 0.5f;
            D3DMATRIX pm = QuatToD3DMatrix(0, 0, sinf(hy), cosf(hy), playerX, playerY, feetZ);
            dev->SetTransform(D3DTS_WORLD, &pm);
            dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); // double-sided box
            g_renderer.DrawMesh(playerMesh);
            dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW); // world default
        };

        // The pause menu has an opaque backdrop (retail draws it over black),
        // so the world is not drawn behind it.
        const bool menuCovers = g_menuReady && g_menu.IsOpen();
        if (menuCovers) {
        } else if (worldReady) {
            g_prof.Begin(FrameProfiler::P_RENDER);
            g_world.Render(g_renderer, playerX, playerY, playerZ);  // opaque pass
            if (g_devShowCollision)
                g_world.DrawCollision(g_renderer, playerX, playerY, playerZ, 60.0f);
            // Player is opaque: draw it after opaque world but BEFORE the
            // transparent pass, so glass/fences in front of it depth-test
            // against it instead of the old "player drawn over everything".
            drawPlayerBox();
            g_vehicles.Draw(g_renderer);
            g_peds.Draw(g_renderer);
            if (scmRun) g_sg.DrawCutscene(g_renderer);
            g_proj.Draw(g_renderer, g_cj);
            { const float camPos3[3] = {camPosX, camPosY, camPosZ};
            g_enex.DrawMarkers(g_renderer, camPos3, (uint32_t)g_bulletMs, ((int)(g_gameSeconds / 3600.0f)) % 24);
            g_pickups.Draw(g_renderer, (uint32_t)g_bulletMs, camPos3,
                           [&](int w, const float m[4][4]) { g_cj.DrawGunModel(g_renderer, g_cj.GunIndexOfType(w), m); },
                           [&](const float m[4][4]) { g_cj.DrawJetpackPickup(g_renderer, m); }); }
            // Water (translucent) before the world's alpha meshes: it's a far,
            // low surface, so nearer glass/foliage blends over it.
            // Wavyness = WindClipped + 0.3 in CWeather::Update; there is no wind yet.
            g_water.Render(g_renderer, camPosX, camPosY, fogEnd, GetTickCount(), g_waterRGBA, 0.3f);
            // Transparent world pass (glass, decals, soft foliage), back-to-front.
            g_world.RenderAlpha(g_renderer);
            // Night street-lamp glows (coronas + ground light pools). Ramp on at
            // dusk: full by the time sunFade drops under 0.1, off above 0.55.
            float lampI = (0.55f - g_lightSunFade) / 0.45f;
            lampI = lampI < 0.0f ? 0.0f : (lampI > 1.0f ? 1.0f : lampI);
            g_world.RenderLamps(g_renderer, camPosX, camPosY, camPosZ, lampI);
            // Traffic signals (lit vehicle bulb + ped walk/don't-walk), day and
            // night. The SA cycle runs on real wall-clock time (~32.8s), so use
            // GetTickCount; GTA_TLMS overrides it to inspect a specific state.
            uint32_t tlMs = GetTickCount();
            if (const char* e = getenv("GTA_TLMS")) tlMs = (uint32_t)strtoul(e, nullptr, 10);
            g_world.RenderTrafficLights(g_renderer, camPosX, camPosY, camPosZ, tlMs, lampI);
            g_fx.Render(g_renderer);
            {
                WaterCannons::RenderHooks rh;
                rh.ray = [](const float* a, const float* b, float* nz) { return worldReady ? g_world.RayCast(a[0], a[1], a[2], b[0], b[1], b[2], nullptr, nz) : 1.0f; };
                rh.splash = [](const float* p, const float*) { g_fx.Splash(p, 0.5f); };
                const float cfw[3] = {g_lastView._13, g_lastView._23, g_lastView._33};
                g_waterCannons.Render(g_renderer, cfw, rh);
            }
            g_prof.End(FrameProfiler::P_RENDER);
        } else {
            for (auto& obj : mapObjects) {
            dev->SetTransform(D3DTS_WORLD, &obj.worldMatrix);
            // Rockstar-style LOD streaming: HD when close, LOD when far.
            // Switch threshold = HD draw distance from the IDE.
            float dx = playerX - obj.objX;
            float dy = playerY - obj.objY;
            float dz = (playerZ - EYE_HEIGHT) - obj.objZ;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            bool useLod = obj.hasLod && dist >= obj.lodDist;
            const auto& drawMeshes = useLod ? obj.lodMeshes : obj.meshes;
            for (auto* mesh : drawMeshes)
                g_renderer.DrawMesh(mesh);
            }
        }

        // ---- CJ scale-reference mesh ----
        // In the world path drawPlayerBox() already ran between the opaque and
        // alpha passes. Only the non-world (mapObjects) path, which has no alpha
        // pass, needs it drawn here.
        if (!worldReady && !menuCovers) drawPlayerBox();

        // ---- Debug overlay (play mode) ----
        if (playMode && g_devOverlay && !menuCovers) {
            dev->SetRenderState(D3DRS_FOGENABLE, FALSE); // keep text unfogged
            float debugGround = GetGroundHeight(playerX, playerY, playerZ - EYE_HEIGHT);
            const char* weatherName = g_timecycOk ? g_timecyc.WeatherName(g_weatherIdx).c_str() : "-";
            size_t nLoaded  = worldReady ? g_world.LoadedModelCount() : 0;
            size_t nVisible = worldReady ? g_world.VisibleInstances() : mapObjects.size();
            size_t nDrawn   = worldReady ? g_world.LastDrawnInsts() : 0;
            size_t nCalls   = worldReady ? g_world.LastDrawMeshCalls() : 0;
            size_t nSkip    = worldReady ? g_world.LastSkippedInsts() : 0;
            size_t nSolid   = worldReady ? g_world.SolidBoxCount() : (size_t)solidCount;
            // If the feet are well above the detected ground, the player is
            // FLOATING -- every building below is then "walk-through" and only
            // tall objects (poles/trees) read as "invisible walls". This is a
            // position bug (jump/unstick/teleport), not broken collision; call
            // it out so it's not mistaken for one.
            float floatGap = (playerZ - EYE_HEIGHT) - debugGround;
            char floatWarn[96] = "";
            if (!g_devNoClip && worldReady && floatGap > 2.0f)
                snprintf(floatWarn, sizeof(floatWarn),
                         "  <<FLOATING +%.1fm: collision is BELOW your feet>>", floatGap);
            // Profiler: rolling window + the worst frame since start / last R.
            double pAvg, pP99, pWin;
            g_prof.Window(pAvg, pP99, pWin);
            const FrameProfiler::Sample& pl = g_prof.Last();
            const FrameProfiler::Sample& pw = g_prof.WorstEver();
            char profLine[512];
            snprintf(profLine, sizeof(profLine),
                "PROF avg %.2f  p99 %.2f  worst(window) %.2f ms   hitches %zu   [now: stream %.2f render %.2f present %.2f other %.2f]\n"
                "WORST %.2f ms = stream %.2f (loads %zu/%.2fms, col %zu/%.2fms) render %.2f present %.2f other %.2f   R reset\n",
                pAvg, pP99, pWin, g_prof.Hitches(),
                pl.phase[FrameProfiler::P_STREAM], pl.phase[FrameProfiler::P_RENDER],
                pl.phase[FrameProfiler::P_PRESENT], pl.Other(),
                pw.total, pw.phase[FrameProfiler::P_STREAM], pw.loads, pw.loadMs, pw.cols, pw.colMs,
                pw.phase[FrameProfiler::P_RENDER], pw.phase[FrameProfiler::P_PRESENT], pw.Other());
            char dbg[1900];
            snprintf(dbg, sizeof(dbg),
                "%s"
                "FPS %.0f (%.2f ms)   frame %d\n"
                "POS %.2f %.2f %.2f   feet %.2f m   ground %.2f m%s\n"
                "LOOK yaw %.1f deg   pitch %.1f deg\n"
                "TIME %.2f h   weather %s   flow %s\n"
                "LIGHT ambient %.0f %.0f %.0f   sun %.0f%% of directional\n"
                "DRAW far %.0f m   fog %.0f-%.0f m%s\n"
                "STREAM models %zu loaded   inst %zu visible   %zu drawn   %zu mesh calls   %zu loading\n"
                "COLLIDE solid boxes %zu (response ON, view %s)   blocked moves %u\n"
                "TOGGLES wireframe %s   noclip %s   lighting %s   camera %s   speed %s   anim %s",
                profLine,
                fps, (fps > 1.0f ? 1000.0f / fps : 0.0f), frame,
                playerX, playerY, playerZ, playerZ - EYE_HEIGHT, debugGround, floatWarn,
                yaw * 57.2958f, pitch * 57.2958f,
                clockHour, weatherName, g_timeFlow ? "on" : "off",
                g_lightAmb[0], g_lightAmb[1], g_lightAmb[2], g_lightSunFade * 100.0f,
                fogEnd, fogStart, fogEnd, g_fogEnable ? "" : " (clear day: haze only)",
                nLoaded, nVisible, nDrawn, nCalls, nSkip,
                nSolid, g_devShowCollision ? "ON pink" : "off", g_solidBlockCount,
                g_devWireframe ? "ON" : "off", g_devNoClip ? "ON" : "off",
                g_devLighting ? "ON" : "OFF (full-bright)",
                cjReady ? PlayerCamera::ViewName(g_menu.Settings().cameraStyle, g_playerCam.View())
                        : thirdPerson ? "3rd" : "1st",
                (GameKey(VK_SPACE) & 0x8000) ? "sprint" : (GameKey(VK_MENU) & 0x8000) ? "walk" : "run",
                g_cj.ClipName());
            DrawDebugText(dev, dbg, 12.0f, 12.0f, 1.4f, 0xFFFFFF00); // yellow
        }

        // ---- Player statistics (pause menu pages) and the events they share with audio ----
        static struct {
            double playSec = 0; float dist = 0;
            int shots = 0, reloads = 0, jumps = 0, hurt = 0;
            bool wasAir = false, havePrev = false; float px = 0, py = 0, lastFlash = 0;
        } st;
        int evShots = 0, evDry = 0, evReload = 0;
        if (g_menuReady && cjReady) {
            if (!g_menu.IsOpen() && !g_devPaused) {
                evShots = g_cj.TakeShotEvents(); evDry = g_cj.TakeDryEvents(); evReload = g_cj.TakeReloadEvents();
                Stats::Increment(Stats::STAT_BULLETS_FIRED, (float)evShots);
                {   // CTaskSimpleFight strike: the sphere against the crowd
                    PlayerPed::Strike sk;
                    if (g_cj.TakeStealth() && g_stealthPed >= 0 && g_peds.StealthKill(g_stealthPed)) {
                        Log("MELEE STEALTH KILL %d", g_stealthPed);
                        if (g_hudReady) g_hud.Hit(true);
                        const float bp[3] = {g_peds.X((size_t)g_stealthPed), g_peds.Y((size_t)g_stealthPed), g_peds.Z((size_t)g_stealthPed) + 1.0f};
                        g_fx.Blood(bp);
                        g_audio.Impact(0.0f);
                    }
                    const bool struck = g_cj.TakeStrike(sk);
                    if (struck && sk.ground) {
                        // AGROUND: the stomp on a fallen ped
                        if (g_peds.StompStrike(sk.pos, sk.radius) >= 0) {
                            Log("MELEE STOMP dmg %.1f", sk.damage);
                            if (g_hudReady) g_hud.Hit(false);
                            g_fx.Blood(sk.pos);
                            g_audio.Impact(0.0f);
                        }
                    } else if (struck) {
                        const char* hc[3] = {sk.hitClips[0].c_str(), sk.hitClips[1].c_str(), sk.hitClips[2].c_str()};
                        const float ap[2] = {playerX, playerY};
                        bool killed = false;
                        if (getenv("GTA_PEDDBG")) {
                            Log("STRIKE %.2f %.2f %.2f r %.2f dmg %.1f move %d; cj %.2f %.2f", sk.pos[0], sk.pos[1], sk.pos[2], sk.radius, sk.damage, sk.move, playerX, playerY);
                            for (size_t pi = 0; pi < g_peds.Count(); pi++) Log("  ped %zu at %.2f %.2f %.2f", pi, g_peds.X(pi), g_peds.Y(pi), g_peds.Z(pi));
                        }
                        const bool adr = Cheat::AdrenalineOn();
                        const bool mega = Cheat::MegaPunchOn() && sk.mega;
                        float fdmg = sk.damage, fforce = sk.force;
                        if (mega) { fdmg = 1e4f; fforce *= 20.0f; }   // ComputeWillKillPed: the damage factor becomes the ped's health
                        const float m4 = adr ? 5.0f : Stats::FatMuscleMod(4);
                        const int hp = g_peds.MeleeStrike(sk.pos, sk.radius, fdmg, ap, sk.sword && sk.move == 2, hc, sk.move, &killed, fforce, m4, adr ? 2.0f : -1.0f);
                        if (hp >= 0) {
                            Log("MELEE HIT %d move %d dmg %.1f kill %d force %.1f adr %d mega %d flying %d down %d vel %.2f", hp, sk.move, fdmg, (int)killed, fforce, (int)adr, (int)mega, (int)g_peds.Flying((size_t)hp), (int)g_peds.KnockedDown((size_t)hp), g_peds.VelX((size_t)hp));
                            if (g_hudReady) g_hud.Hit(killed);
                            g_fx.Blood(sk.pos);
                            g_audio.Impact(0.0f);
                        } else if (worldReady) {
                            // CTaskSimpleFight::FightHitCar / FightHitObj: the sphere against a car body or a loose prop
                            const float dir[3] = {std::cos(sk.heading), std::sin(sk.heading), 0.0f};
                            const float org[3] = {ap[0], ap[1], sk.pos[2]};
                            float tc = 0.0f;
                            const int car = g_vehicles.Ready() ? g_vehicles.Trace(org, dir, 0.7f + sk.radius, &tc) : -1;
                            if (car >= 0 && car != g_driveCar) {
                                Log("MELEE HIT CAR %d move %d", car, sk.move);
                                g_vehicles.Damage(car, sk.damage * 0.25f);
                                g_audio.Impact(0.0f);
                            } else if (g_world.DynShoot(org, dir, 0.7f + sk.radius, sk.damage * 2.0f, 0) >= 0) {
                                Log("MELEE HIT OBJECT move %d", sk.move);
                                g_audio.Impact(0.0f);
                            }
                        }
                    }
                }
                {   // CTaskSimpleThrowProjectile: launches, the detonator, and what the projectiles did
                    PlayerPed::Launch ln;
                    if (g_cj.TakeLaunch(ln)) {
                        Log("THROW type %d pos %.2f %.2f %.2f vel %.2f %.2f %.2f fell %d", ln.type, ln.pos[0], ln.pos[1], ln.pos[2], ln.vel[0], ln.vel[1], ln.vel[2], (int)ln.fell);
                        g_proj.Add(ln.gun, ln.type, ln.pos, ln.vel, ln.fell);
                    }
                    if (g_cj.TakeDetonate()) g_proj.Detonate();
                    Projectiles::Env env;
                    env.groundAt = [](float x, float y, float z) { return g_world.GetGroundHeight(x, y, z); };
                    env.rayCast = [](const float* a, const float* b, float* nz) { return g_world.RayCast(a[0], a[1], a[2], b[0], b[1], b[2], nullptr, nz); };
                    env.entityHit = [](const float* a, const float* b) {
                        float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
                        const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                        if (len < 1e-4f) return false;
                        for (float& v : d) v /= len;
                        float t = 0.0f;
                        if (g_peds.Ready() && g_peds.Trace(a, d, len, &t, nullptr) >= 0) return true;
                        return g_vehicles.Ready() && g_vehicles.Trace(a, d, len, &t) >= 0 && g_vehicles.Trace(a, d, len, &t) != g_driveCar;
                    };
                    env.seekTarget = [](const float* o, const float* v, float* out) {   // the nearest ped or car inside the nose cone
                        const float vl = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                        if (vl < 1e-3f) return false;
                        float best = 1e9f; bool found = false;
                        auto consider = [&](float x, float y, float z) {
                            const float d[3] = {x - o[0], y - o[1], z - o[2]};
                            const float dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                            if (dl < 1.0f || dl > 120.0f) return;
                            if ((d[0] * v[0] + d[1] * v[1] + d[2] * v[2]) / (dl * vl) < 0.5f) return;   // about 60 degrees
                            if (dl < best) { best = dl; out[0] = x; out[1] = y; out[2] = z; found = true; }
                        };
                        for (size_t i = 0; g_vehicles.Ready() && i < g_vehicles.Count(); i++)
                            if ((int)i != g_driveCar) consider(g_vehicles.Car(i).x, g_vehicles.Car(i).y, g_vehicles.Car(i).z);
                        for (size_t i = 0; g_peds.Ready() && i < g_peds.Count(); i++) consider(g_peds.X(i), g_peds.Y(i), g_peds.Z(i) + 1.0f);
                        return found;
                    };
                    std::vector<Projectiles::Event> pev;
                    if (worldReady) g_proj.Update(dt, env, pev);
                    // ---- CFireManager / CExplosion / CWorld::TriggerExplosion / SetPedsChoking ----
                    FireManager::World fw;
                    fw.groundZ = [](const float* p, float* o) {
                        const float g = g_world.GetGroundHeight(p[0], p[1], p[2]);
                        if (g < -1e5f) return false;
                        o[0] = p[0]; o[1] = p[1]; o[2] = g;
                        return true;
                    };
                    fw.underwater = [](const float* p) { float wz; return g_water.Ready() && g_water.GetWaterLevel(p[0], p[1], &wz) && p[2] < wz; };
                    fw.pedPos = [](int i, float* o) { if ((size_t)i >= g_peds.Count()) return false; o[0] = g_peds.X((size_t)i); o[1] = g_peds.Y((size_t)i); o[2] = g_peds.Z((size_t)i) + 0.5f; return true; };
                    fw.pedAlive = [](int i) { return g_peds.Alive((size_t)i); };
                    fw.pedBurning = [](int i, bool on) { g_peds.SetBurning((size_t)i, on); };
                    fw.pedBurnDamage = [](int i, float d) { return g_peds.BurnDamage((size_t)i, d); };
                    fw.carCount = g_vehicles.Ready() ? g_vehicles.Count() : 0;
                    fw.carPos = [](int i, float* o) { if (!g_vehicles.Ready() || (size_t)i >= g_vehicles.Count()) return false; const VehicleInstance& c = g_vehicles.Car((size_t)i); o[0] = c.x; o[1] = c.y; o[2] = c.z + 0.5f; return true; };
                    fw.carWrecked = [](int i) { return g_vehicles.Car((size_t)i).wrecked; };
                    fw.carDamage = [](int i, float step) { g_vehicles.Damage(i, step); };   // InflictDamage(WEAPON_FLAMETHROWER, step * 1.2)
                    fw.carBurning = [](int) {};
                    fw.player = [&](float* o) { if (g_driveCar >= 0 || g_cj.Dead()) return false; o[0] = playerX; o[1] = playerY; o[2] = playerZ - EYE_HEIGHT; return true; };
                    fw.playerBurning = []() { return g_fires.IsBurning(FireManager::Target::Player, 0); };
                    fw.playerBurn = [](float d) { g_cj.Hurt(25.0f * d); };   // CTaskComplexOnFire::ComputeFireDamage: 0.5 per 20 ms step
                    fw.objectFire = [](const float*, float) {};               // CObject::ObjectFireDamage only hurts grass plants
                    fw.emit = [](const float* p, float s, float) {
                        g_fx.Fire(p);
                        if (s > 1.0f) g_fx.Fire(p);
                        if (s > 2.0f) g_fx.Fire(p);
                        if (rand() % 8 == 0) g_fx.Smoke(p, 0.5f);
                    };
                    ExplHooks eh;
                    eh.fire = &g_fires; eh.fireWorld = &fw;
                    eh.groundZ = fw.groundZ; eh.underwater = fw.underwater;
                    const auto groundFn = fw.groundZ;
                    eh.fx = [groundFn](const float* p, ExplType t) {
                        if (t == ExplType::Molotov) { g_fx.Explosion(p, 4); g_fx.Fire(p); g_fx.Fire(p); return; }
                        // CExplosion::AddExplosion fx system per type
                        int kind = 2;
                        switch (t) {
                            case ExplType::Grenade: case ExplType::Rocket: case ExplType::WeakRocket: kind = 1; break;
                            case ExplType::Car: case ExplType::QuickCar: kind = 2; break;
                            case ExplType::Boat: case ExplType::Aircraft: case ExplType::Mine: case ExplType::Object: case ExplType::TankFire: kind = 3; break;
                            case ExplType::Small: case ExplType::RcVehicle: kind = 0; break;
                            default: break;
                        }
                        Log("EXPLOSION fx kind %d at %.1f %.1f %.1f", kind, p[0], p[1], p[2]);
                        g_fx.Explosion(p, kind); g_audio.Impact(0.0f);
                        float gz[3] = {p[0], p[1], p[2]};
                        if (groundFn && groundFn(p, gz)) g_fx.Scorch(gz);
                    };
                    eh.fuel = [](const float* p, float) { g_fx.Fire(p); g_fx.Smoke(p, 0.9f); };
                    eh.shake = [&](const float* p, float s) {
                        const float dx = playerX - p[0], dy = playerY - p[1], dz = playerZ - p[2];
                        g_shakeQ.push_back({s, std::sqrt(dx * dx + dy * dy + dz * dz)});
                    };
                    eh.setPedsOnFire = [&](const float* p, float r) {   // CWorld::SetPedsOnFire
                        for (size_t i = 0; i < g_peds.Count(); i++) {
                            if (!g_peds.Alive(i) || g_peds.Burning(i) || std::fabs(g_peds.X(i) - p[0]) > r || std::fabs(g_peds.Y(i) - p[1]) > r || std::fabs(g_peds.Z(i) + 1.0f - p[2]) > 5.0f) continue;
                            if (g_fires.StartFireOn(FireManager::Target::Ped, (int)i, 0.8f, 7000.0f, 2) >= 0) { g_peds.SetBurning(i, true); Log("PED %zu ON FIRE", i); }
                        }
                        if (g_driveCar < 0 && !g_cj.Dead() && !g_fires.IsBurning(FireManager::Target::Player, 0) && std::fabs(playerX - p[0]) <= r && std::fabs(playerY - p[1]) <= r && std::fabs(playerZ - p[2]) <= 5.0f)
                            g_fires.StartFireOn(FireManager::Target::Player, 0, 0.8f, 7000.0f, 100);
                    };
                    eh.setCarsOnFire = [&](const float* p, float r) {   // CWorld::SetCarsOnFire
                        for (size_t i = 0; g_vehicles.Ready() && i < g_vehicles.Count(); i++) {
                            const VehicleInstance& c = g_vehicles.Car(i);
                            if (c.wrecked || std::fabs(c.x - p[0]) > r || std::fabs(c.y - p[1]) > r || std::fabs(c.z - p[2]) > 5.0f) continue;
                            g_fires.StartFireOn(FireManager::Target::Vehicle, (int)i, 0.8f, 7000.0f, 100);
                        }
                    };
                    eh.setWorldOnFire = [&](const float* p, float r) {   // CWorld::SetWorldOnFire
                        const float g = g_world.GetGroundHeight(p[0], p[1], p[2] + 1.0f);
                        if (g > -1e5f && std::fabs(g - p[2]) < r && g_fires.NumInRange(p, 2.0f) == 0) { const float fp[3] = {p[0], p[1], g}; g_fires.StartFire(fp, 0.8f, true, 7000.0f, 100); }
                    };
                    eh.trigger = [&](const float* p, float radius, float visible, float damage) {   // CWorld::TriggerExplosion
                        if (worldReady) g_world.DynExplosion(p, radius, visible);
                        int killed = 0;
                        for (size_t i = 0; i < g_peds.Count(); i++) {   // peds: min(1, 2 (R - d) / R) * 250
                            if (!g_peds.Alive(i)) continue;
                            const float dx = g_peds.X(i) - p[0], dy = g_peds.Y(i) - p[1], dz = g_peds.Z(i) + 1.0f - p[2];
                            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                            if (d >= radius) continue;
                            if (g_peds.Damage((int)i, std::min(1.0f, 2.0f * (radius - d) / radius) * damage * 250.0f / 1.5f, false)) killed++;
                        }
                        if (killed) Log("EXPLOSION killed %d", killed);
                        for (size_t i = 0; g_vehicles.Ready() && i < g_vehicles.Count(); i++) {   // vehicles: InflictDamage(rel * damage * 1100)
                            const VehicleInstance& c = g_vehicles.Car(i);
                            if (c.wrecked) continue;
                            const float dx = c.x - p[0], dy = c.y - p[1], dz = c.z - p[2];
                            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                            if (d >= radius) continue;
                            g_vehicles.Damage((int)i, std::min(1.0f, 2.0f * (radius - d) / radius) * damage * 1100.0f);
                        }
                        const float dx = playerX - p[0], dy = playerY - p[1], dz = playerZ - EYE_HEIGHT + 1.0f - p[2];
                        const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
                        if (g_driveCar < 0 && d < radius) g_cj.Hurt(std::min(1.0f, 2.0f * (radius - d) / radius) * damage * 250.0f);
                    };
                    if (const char* bo = getenv("GTA_BOOM")) {   // dev: GTA_BOOM=<ExplType index>[,dx,dy] blows one up (default 8 m +x) at frame 60
                        static bool s_done = false;
                        if (!s_done && frame >= 60) {
                            s_done = true;
                            int bt = 0; float bdx = 8.0f, bdy = 0.0f;
                            sscanf(bo, "%d,%f,%f", &bt, &bdx, &bdy);
                            const float bp[3] = {playerX + bdx, playerY + bdy, playerZ - EYE_HEIGHT};
                            g_expl.Add((ExplType)bt, bp, eh);
                        }
                    }
                    for (const auto& bm : g_carBooms) g_expl.Add(ExplType::Car, bm.data(), eh);
                    g_carBooms.clear();
                    for (const auto& bm : g_dynBooms) g_expl.Add(ExplType::Object, bm.data(), eh);
                    g_dynBooms.clear();
                    for (const Projectiles::Event& e : pev) {
                        if (e.kind == Projectiles::Kind::Explode) {
                            Log("BOOM %.2f %.2f %.2f", e.pos[0], e.pos[1], e.pos[2]);
                            g_expl.Add(e.sub == 1 ? ExplType::Rocket : ExplType::Grenade, e.pos, eh);
                        } else if (e.kind == Projectiles::Kind::Trail) {
                            static int s_trail = 0;
                            if (++s_trail % 3 == 0) g_fx.Smoke(e.pos, 0.5f);
                        } else if (e.kind == Projectiles::Kind::Fire) {
                            Log("FIRE %.2f %.2f %.2f", e.pos[0], e.pos[1], e.pos[2]);
                            g_expl.Add(ExplType::Molotov, e.pos, eh);
                        } else {
                            static int s_gas = 0;
                            if (s_gas++ % 20 == 0) Log("GAS %.2f %.2f %.2f", e.pos[0], e.pos[1], e.pos[2]);
                            g_fx.Gas(e.pos);
                            g_peds.Choke(e.pos, e.radius);
                        }
                    }
                    for (const auto& bm : g_tankBooms) g_expl.Add(ExplType::TankFire, bm.data(), eh);
                    g_tankBooms.clear();
                    g_expl.Update(dt, eh);
                    g_fires.Update(dt, fw);
                    g_fireWorld = fw;
                    {   // CWaterCannons::Update
                        WaterCannons::Hooks wh;
                        wh.extinguish = [&](const float* p, float r, float s) {
                            g_fires.ExtinguishPointWithWater(p, r, s, dt, &g_fireWorld, [](const float* sp) { g_fx.Smoke(sp, 0.0f); });
                        };
                        wh.pedCount = (int)g_peds.Count();
                        wh.pedPos = [](int i, float* o) { if (!g_peds.Alive((size_t)i)) return false; o[0] = g_peds.X((size_t)i); o[1] = g_peds.Y((size_t)i); o[2] = g_peds.Z((size_t)i) + 1.0f; return true; };
                        wh.pedHit = [](int i, const float* sp, const float* ss, float ts) {
                            const bool wh2 = g_peds.WaterHit((size_t)i, ss, ts);
                            if (getenv("GTA_VEHGUNDBG")) Log("WATER pedHit %d -> %d", i, (int)wh2);
                            if (wh2) {
                                static int s_hits = 0;
                                if (getenv("GTA_VEHGUNDBG") && s_hits++ % 10 == 0) Log("WATER HIT ped %d section %.1f %.1f %.1f speed %.2f %.2f %.2f", i, sp[0], sp[1], sp[2], ss[0], ss[1], ss[2]);
                                const float pp[3] = {g_peds.X((size_t)i), g_peds.Y((size_t)i), g_peds.Z((size_t)i) + 1.0f};
                                g_fx.Smoke(pp, 0.0f);
                            }
                        };
                        if (getenv("GTA_VEHGUNDBG") && frame % 30 == 0) for (int i = 0; i < wh.pedCount; i++) { float pp[3]; if (wh.pedPos(i, pp)) Log("WATER ped %d at %.1f %.1f %.1f", i, pp[0], pp[1], pp[2]); }
                        g_waterCannons.Update(dt * 50.0f, (uint32_t)frame, g_bulletMs, wh);
                        if (getenv("GTA_VEHGUNDBG") && frame % 15 == 0 && g_waterCannons.ActiveCannons() > 0) {
                            float sp[3], sv[3];
                            const int n = g_waterCannons.ActiveSections(1);
                            const bool ok = g_waterCannons.Section(1, 0, sp, sv);
                            float fs = -1.0f;
                            for (const auto& f : g_fires.Fires()) if (f.active) fs = f.strength;
                            Log("WATER cannons %d sections %d first %s %.1f %.1f %.1f fire strength %.3f", g_waterCannons.ActiveCannons(), n, ok ? "ok" : "-", sp[0], sp[1], sp[2], fs);
                        }
                    }
                    {   // CPickups::Update
                        const uint32_t nowMs = (uint32_t)g_bulletMs;
                        const float feet = playerZ - EYE_HEIGHT;
                        static bool s_prevCollect = false;
                        {   // the collect key: F, buffered for 6 frames
                            const bool f = (GameKey('F') & 0x8000) != 0;
                            if (f && !s_prevCollect) g_collectBuf = 6; else if (g_collectBuf > 0) g_collectBuf--;
                            s_prevCollect = f;
                        }
                        if (frame == (getenv("GTA_PICKUPAT") ? atoi(getenv("GTA_PICKUPAT")) : 3) && getenv("GTA_PICKUP")) {   // dev: kind/weapon/type/x/y/ammo;...  (ammo -1 = an empty pickup)
                            static const char* kKind[] = {"weapon", "cash", "health", "armour", "adrenaline", "info", "bribe", "bonus", "jetpack"};
                            std::string spec = getenv("GTA_PICKUP");
                            size_t at = 0;
                            while (at < spec.size()) {
                                size_t end = spec.find(';', at);
                                if (end == std::string::npos) end = spec.size();
                                char kn[16] = {};
                                int wt = 0, ty = 0, am = 0;
                                float px = 0, py = 0;
                                if (sscanf(spec.substr(at, end - at).c_str(), "%15[^/]/%d/%d/%f/%f/%d", kn, &wt, &ty, &px, &py, &am) == 6) {
                                    int kind = 0;
                                    for (int k = 0; k < Pickups::kKinds; k++) if (!_stricmp(kn, kKind[k])) kind = k;
                                    const float p[3] = {px, py, GetGroundHeight(px, py, feet + 1.0f) + 0.5f};
                                    const int sl = g_pickups.Add(p, kind, wt, ty, am < 0 ? 0 : am, am < 0, nowMs);
                                    Log("PICKUP ADD slot %d %s weapon %d type %d ammo %d at %.2f %.2f %.2f", sl, kn, wt, ty, am, p[0], p[1], p[2]);
                                }
                                at = end + 1;
                            }
                        }
                        if (const char* ts = getenv("GTA_TIMESKIP")) {   // dev: "frame:ms" jumps the pickup clock
                            int tf = 0, tm = 0;
                            if (sscanf(ts, "%d:%d", &tf, &tm) == 2 && frame == tf) { g_bulletMs += (float)tm; Log("PICKUP DEV clock +%d ms", tm); }
                        }
                        if (getenv("GTA_GUNDBG") && frame % 5 == 4) { static int s_lg = -2; if (g_cj.WeaponIndex() != s_lg) { s_lg = g_cj.WeaponIndex(); Log("GUNDBG frame %d weapon index %d type %d", frame, s_lg, s_lg >= 0 ? g_cj.GunType(s_lg) : -1); } }
                        if (frame == 2) {   // dev: GTA_HURT=hp takes health off, GTA_ARMOUR=n sets armour
                            if (const char* h = getenv("GTA_HURT")) { g_cj.Hurt((float)atof(h)); Log("PICKUP DEV hurt: hp %.0f", g_cj.Health()); }
                            if (const char* ar = getenv("GTA_ARMOUR")) g_cj.SetArmor((float)atof(ar));
                        }
                        if (getenv("GTA_PICKFILL") && frame == 3) {   // dev: fill every slot with cash far away to test the cap
                            for (int k = 0; k < Pickups::kSlots + 5; k++) {
                                const float p[3] = {5000.0f + (float)k, 5000.0f, 10.0f};
                                const int sl = g_pickups.Add(p, Pickups::Cash, 0, k % 2 ? Pickups::Money : Pickups::OnceTimeout, 1 + k, false, nowMs + (uint32_t)k * 10);
                                if (k >= Pickups::kSlots - 2) Log("PICKUP FILL %d -> slot %d used %d", k, sl, g_pickups.Used());
                            }
                        }
                        if (g_peds.Ready()) {   // a dead ped drops its cash (CPed::CreateDeadPedMoney)
                            static std::vector<char> s_was;
                            static std::vector<int> s_cash;
                            for (size_t i = s_was.size(); i < g_peds.Count(); i++) {
                                s_was.push_back(g_peds.Alive(i));
                                s_cash.push_back(getenv("GTA_PEDCASH") ? atoi(getenv("GTA_PEDCASH")) : (rand() % 100 < 3 ? 400 : rand() % 25));   // GTA_PEDCASH: dev fixed wallet
                            }
                            for (size_t i = 0; i < g_peds.Count(); i++) {
                                const bool al = g_peds.Alive(i);
                                if (s_was[i] && !al) {
                                    if (s_cash[i] > 10) {
                                        const float p[3] = {g_peds.X(i), g_peds.Y(i), g_peds.Z(i) + 0.5f};
                                        g_pickups.CreateSomeMoney(p, s_cash[i], nowMs);
                                        Log("PICKUP DEADPED %zu cash %d -> %d pickups", i, s_cash[i], g_pickups.Used());
                                    }
                                    s_cash[i] = 0;
                                } else if (!s_was[i] && al) s_cash[i] = rand() % 100 < 3 ? 400 : rand() % 25;
                                s_was[i] = al;
                            }
                        }
                        Pickups::Env pe;
                        pe.player[0] = playerX; pe.player[1] = playerY; pe.player[2] = feet + 1.0f;
                        pe.alive = !g_cj.Dead();
                        pe.onFoot = g_driveCar < 0;
                        pe.health = g_cj.Health(); pe.maxHealth = g_cj.MaxHealth(); pe.armour = g_cj.Armor();
                        pe.wanted = g_wanted; pe.onJetpack = g_cj.Jetpacking();
                        pe.collectKey = g_collectBuf > 0;
                        pe.slotState = [&](int w) { return g_cj.PickupSlotState(w); };
                        pe.wantsWeapon = [&](int w) { return g_cj.WantsNewWeapon(w); };
                        pe.canPickNow = [&](int w) { return g_cj.CanPickWeaponNow(w); };
                        pe.exchangeable = [&](int w) { return g_cj.SlotExchangeable(w); };
                        pe.grantWeapon = [&](int w, int a) { g_cj.GiveWeapon(w, a); };
                        pe.grantAmmo = [&](int w, int a) { g_cj.GrantAmmo(w, a); };
                        pe.help = [&](const char* k) {
                            if (!g_hudReady) return;
                            std::string t = g_hud.Text().Get(k);
                            const std::string tok = "~k~~PED_ANSWER_PHONE~";   // the collect key is shown by name: F here
                            for (size_t at; (at = t.find(tok)) != std::string::npos;) t.replace(at, tok.size(), "F");
                            g_hud.ShowMessage(t.c_str(), 5.0f);
                        };
                        pe.giveMoney = [&](int a) { g_money += a; };
                        pe.goodie = [&](int kind) {
                            switch (kind) {
                            case Pickups::Health: g_cj.AddHealth(g_cj.MaxHealth()); break;
                            case Pickups::Armour: g_cj.SetArmor(100.0f); break;
                            case Pickups::Adrenaline: Cheat::StartAdrenaline(); g_timeCanRun = Stats::FatMuscleMod(7); break;   // ResetSprintEnergy
                            case Pickups::Bribe: g_wanted = std::max(0, g_wanted - 1); Stats::Increment(Stats::STAT_NUMBER_OF_POLICE_BRIBES, 1.0f); break;
                            case Pickups::Jetpack: g_cj.StartJetpack(); break;
                            default: break;
                            }
                        };
                        pe.log = [&](const char* what, int slot, int a, int b) {
                            Log("PICKUP %s slot %d %d %d | money %d hp %.0f armour %.0f wanted %d", what, slot, a, b, g_money, g_cj.Health(), g_cj.Armor(), g_wanted);
                        };
                        g_pickups.Update(nowMs, pe);
                    }
                    {   // CShotInfo::Update
                        static unsigned s_shotFrame = 0;
                        ShotInfo::Env se;
                        se.pedsNear = [&](int type, const float* o, float range2, float damage) {
                            bool any = false;
                            for (size_t i = 0; g_peds.Ready() && i < g_peds.Count(); i++) {   // not in a car, alive, not already burning
                                if (!g_peds.Alive(i)) continue;
                                const float dx = g_peds.X(i) - o[0], dy = g_peds.Y(i) - o[1], dz = g_peds.Z(i) + 1.0f - o[2];
                                if (getenv("GTA_SHOTDBG") && type == ShotInfo::Spray) Log("SHOTDBG shot %.2f %.2f %.2f ped %zu d %.2f %.2f %.2f", o[0], o[1], o[2], i, dx, dy, dz);
                                if (dx * dx + dy * dy + dz * dz >= range2) continue;
                                if (type == ShotInfo::Flame) {
                                    if (g_peds.Burning(i)) continue;
                                    if (g_fires.StartFireOn(FireManager::Target::Ped, (int)i, 0.8f, 7000.0f, 100) >= 0) { g_peds.SetBurning(i, true); Log("SHOT FLAME PED %zu ON FIRE", i); any = true; }
                                } else {
                                    const bool kill = g_peds.Damage((int)i, damage, false);
                                    Log("SHOT %s PED %zu damage %.0f kill %d", type == ShotInfo::Spray ? "SPRAY" : "FOAM", i, damage, (int)kill);
                                    if (g_hudReady) g_hud.Hit(kill);
                                    any = true;
                                }
                            }
                            return any;
                        };
                        se.carsAndWorldOnFire = [&](const float* o) {
                            eh.setCarsOnFire(o, 4.0f);
                            eh.setWorldOnFire(o, 0.1f);
                        };
                        se.extinguish = [&](const float* o, float range) {
                            const int before = g_fires.NumInRange(o, range);
                            if (before == 0) return false;
                            g_fires.ExtinguishPoint(o, range);
                            Log("SHOT FOAM EXTINGUISHED %d fires", before - g_fires.NumInRange(o, range));
                            return true;
                        };
                        se.sprayHit = [&](const float* o, const float* off) {
                            int kind = 0; float hnz = 0.0f;
                            const float b[3] = {o[0] + off[0], o[1] + off[1], o[2] + off[2]};
                            const bool hit = g_world.RayCast(o[0], o[1], o[2], b[0], b[1], b[2], &kind, &hnz) < 1.0f;
                            if (hit) Log("SHOT SPRAY HIT surface at %.2f %.2f %.2f", o[0], o[1], o[2]);
                            return hit;
                        };
                        if (s_shotFrame == 5) {   // dev: GTA_FIREAT=x,y starts a fire on the ground there (extinguisher test)
                            float fx = 0, fy = 0, fz = 0;
                            if (const char* e = getenv("GTA_FIREAT")) if (sscanf(e, "%f,%f,%f", &fx, &fy, &fz) >= 2) {   // optional z skips the ground probe (world may not be streamed yet)
                                const float g = std::strchr(e, ',') && std::strchr(std::strchr(e, ',') + 1, ',') ? fz : g_world.GetGroundHeight(fx, fy, playerZ);
                                const float fp[3] = {fx, fy, g};
                                Log("DEV FIRE at %.1f %.1f %.1f -> %d", fx, fy, g, g_fires.StartFire(fp, 0.8f, false, 60000.0f, 0));
                            }
                        }
                        g_shots.Update(g_bulletMs, dt * 50.0f, s_shotFrame++, se);
                        if (g_cj.GunIndex() >= 0) {
                            for (int i = 0; i < ShotInfo::kSlots; i++) {   // one effect per live shot
                                float p[3];
                                if (!g_shots.Position(i, p)) continue;
                                const int t = g_shots.TypeOf(i);
                                if (t == ShotInfo::Flame) { g_fx.Fire(p); if (rand() % 6 == 0) g_fx.Smoke(p, 0.4f); }
                                else g_fx.Spray(p, t == ShotInfo::Spray ? 0x40F040u : 0xF0F0F0u);
                            }
                        }
                    }
                    { static int s_nf = -1; if (g_fires.Num() != s_nf) { s_nf = g_fires.Num(); Log("FIRES %d", s_nf); } }
                }
                st.shots += evShots; st.reloads += evReload;
                g_fx.Update(dt);
                if (worldReady && !g_devNoClip) {   // water splashes and landing dust
                    static bool s_pGround = true, s_pSwim = false;
                    static float s_pVelZ = 0.0f, s_pX = 0.0f, s_pY = 0.0f, s_ripple = 0.0f;
                    const float feet = playerZ - EYE_HEIGHT;
                    float wz = -1e9f;
                    if (g_water.Ready()) { float w; if (g_water.GetWaterLevel(playerX, playerY, &w)) wz = w; }
                    const bool swim = g_cj.Swimming();
                    const float moved = dt > 0.0f ? hypotf(playerX - s_pX, playerY - s_pY) / dt : 0.0f;
                    const bool inWater = wz > -1e8f && feet < wz && wz - feet < 1.1f;
                    s_ripple -= dt;
                    if (swim && !s_pSwim && wz > -1e8f) { const float p[3] = {playerX, playerY, wz}; g_fx.Splash(p, 1.0f); g_audio.Splash(1.0f); }
                    else if (onGround && !s_pGround && s_pVelZ < -4.0f && !swim) {
                        const float p[3] = {playerX, playerY, feet + 0.05f};
                        const float st = std::min(1.0f, -s_pVelZ / 14.0f);
                        if (inWater) { const float w3[3] = {playerX, playerY, wz}; g_fx.Splash(w3, st); g_audio.Splash(0.5f + 0.5f * st); } else { g_fx.Dust(p, st); g_audio.Land(s_pVelZ < -9.0f, 0.4f + 0.6f * st); }
                    } else if (s_ripple <= 0.0f && moved > 1.5f && (swim || (inWater && onGround))) {
                        s_ripple = swim ? 0.45f : 0.28f;
                        const float p[3] = {playerX, playerY, wz};
                        g_fx.Splash(p, swim ? 0.25f : 0.12f);
                        g_audio.Splash(swim ? 0.35f : 0.2f);
                    }
                    s_pGround = onGround; s_pSwim = swim; s_pVelZ = velZ; s_pX = playerX; s_pY = playerY;
                }
                if (g_aimTest && worldReady) {
                    static bool s_aimInit = false;
                    if (!s_aimInit) {
                        s_aimInit = true;
                        const float rz = playerZ;
                        g_aim.Setup(playerX, playerY, std::cos(yaw), std::sin(yaw),
                                    [&](float x, float y) { return g_world.GetGroundHeight(x, y, rz); });
                        Log("AIMTEST: %zu dummies", g_aim.Dummies().size());
                    }
                    g_aim.Update(dt);
                    PlayerPed::Input di;
                    di.dt = dt;
                    g_dummy.Update(di);
                    if (g_cj.GunTotalAmmo() < g_cj.GunClipSize() * 3) g_cj.RefillAmmo();
                }
                // Each shot is a hitscan down the camera ray: a tracer from the muzzle,
                // dust and sparks and a hit sound where it lands.
                static float s_trX = 0, s_trY = 0, s_trZ = 0;   // CBulletTraces::AddTrace: speed of the player in the first-person / scope modes
                const float trSpeed = std::sqrt((playerX - s_trX) * (playerX - s_trX) + (playerY - s_trY) * (playerY - s_trY) + (playerZ - s_trZ) * (playerZ - s_trZ)) / std::max(dt * 50.0f, 1e-4f);
                s_trX = playerX; s_trY = playerY; s_trZ = playerZ;
                for (int n = 0; n < evShots && worldReady; n++) {
                    const int heavy = g_cj.GunIndex() >= 0 ? g_cj.GunType(g_cj.GunIndex()) : 0;
                    float hm[3];
                    if (heavy == 43) {   // CWeapon::TakePhotograph: 1100 ms between shots, a flash, no bullet
                        static DWORD s_lastPhoto = 0;
                        const DWORD now = GetTickCount();
                        if (now - s_lastPhoto >= 1100 || s_lastPhoto == 0) {
                            s_lastPhoto = now;
                            g_camFlash = 0.8f;
                            Log("PHOTO %d", ++g_photos);
                        }
                        continue;
                    }
                    if (heavy == 35 || heavy == 36) {   // CWeapon::FireProjectile: the rocket leaves along the camera front, 1 m from the muzzle
                        if (g_cj.MuzzleWorld(playerX, playerY, playerZ - EYE_HEIGHT, hm)) {
                            const float sp = heavy == 35 ? 20.0f : 10.0f;   // 0.4 / 0.2 per step
                            float pos[3], vel[3];
                            for (int i = 0; i < 3; i++) { pos[i] = hm[i] + g_aimDir[i]; vel[i] = g_aimDir[i] * sp; }
                            Log("ROCKET %d pos %.2f %.2f %.2f", heavy, pos[0], pos[1], pos[2]);
                            g_proj.Add(g_cj.RocketModelIndex(), heavy == 35 ? 19 : 20, pos, vel, false);
                            g_audio.Impact(0.0f);
                        }
                        continue;
                    }
                    if (heavy == 37 || heavy == 41 || heavy == 42) {   // CWeapon::FireAreaEffect: a CShotInfo shot along the aim, fire / paint / foam
                        if (g_cj.MuzzleWorld(playerX, playerY, playerZ - EYE_HEIGHT, hm)) {
                            const char* rowName = heavy == 37 ? "FTHROWER" : heavy == 41 ? "SPRAYCAN" : "EXTINGUISHER";
                            const WeaponInfo* wi = WeaponTable::Get(rowName, 1);
                            if (wi) {
                                ShotInfo::Params sp;
                                sp.speed = wi->speed; sp.range = wi->radius; sp.lifeMs = wi->lifespan; sp.spread = wi->spread; sp.damage = (float)wi->damage;
                                sp.slowsDown = wi->SlowsDown(); sp.randSpeed = wi->RandSpeed(); sp.expands = wi->RangeGrows();
                                const float tgt[3] = {hm[0] + g_aimDir[0], hm[1] + g_aimDir[1], hm[2] + g_aimDir[2]};
                                const int slot = g_shots.Add(heavy == 37 ? ShotInfo::Flame : heavy == 41 ? ShotInfo::Spray : ShotInfo::Extinguisher, sp, hm, tgt, g_bulletMs);
                                Log("SHOT ADD %s slot %d speed %.3f range %.4f life %.0f spread %.2f flags slow %d rand %d grow %d", rowName, slot, sp.speed, sp.range, sp.lifeMs, sp.spread, (int)sp.slowsDown, (int)sp.randSpeed, (int)sp.expands);
                                if (heavy == 37 && slot >= 0 && rand() % 3 == 0) {   // CWeapon::FireAreaEffect: one in three starts a creeping fire a few metres on
                                    const float dist = 3.5f + 2.5f * (float)(rand() % 1000) / 999.0f;
                                    const float cp[3] = {hm[0] + g_aimDir[0] * dist, hm[1] + g_aimDir[1] * dist, hm[2] + g_aimDir[2] * dist + 0.5f};
                                    const bool ok = g_fires.TryCreeping(cp, 0, false, 2300.0f, g_fireWorld);
                                    Log("FLAME CREEP at %.1f %.1f %.1f %s", cp[0], cp[1], cp[2], ok ? "started" : "no ground");
                                    if (ok) Stats::Increment(Stats::STAT_FIRES_STARTED);
                                }
                            }
                        }
                        continue;
                    }
                    if (heavy == 34 && g_scoped) {   // CWeapon::FireSniper: a bullet along the camera front at 16 per step, hits resolve as it flies
                        float v[3];
                        for (int i = 0; i < 3; i++) v[i] = g_aimDir[i] * 16.0f;
                        const WeaponInfo* wi = WeaponTable::Get("SNIPERRIFLE", 1);   // CBulletInfo::AddBullet takes the standard row's damage
                        if (g_bullets.Add(g_aimEye, v, wi ? wi->damage : 125, g_bulletMs)) {
                            Log("SNIPER BULLET from %.1f %.1f %.1f dir %.3f %.3f %.3f", g_aimEye[0], g_aimEye[1], g_aimEye[2], g_aimDir[0], g_aimDir[1], g_aimDir[2]);
                            g_shakeQ.push_back({0.2f / 0.35f, 0.0f});   // CamShakeNoPos(0.2)
                        }
                        continue;
                    }
                    const float range = 120.0f;
                    // CWeapon::FireInstantHit: the aimed ray wobbles on a circle (1 s period) by
                    // (100 - 95) / accuracy * attack counter * 0.75, shotguns do not (they spread pellets).
                    float shotDir[3] = {g_aimDir[0], g_aimDir[1], g_aimDir[2]};
                    const char* gid2 = g_cj.GunId();
                    const bool shotgun = !strcmp(gid2, "SHOTGUN") || !strcmp(gid2, "SAWNOFF") || !strcmp(gid2, "SPAS12");
                    if (!shotgun && g_cj.GunIndex() >= 0) {
                        const float wr = std::max(1.0f, g_cj.GunRange());
                        float amp = (100.0f - (g_cj.GunCanAim() ? 95.0f : 100.0f)) / std::max(0.01f, g_cj.GunAccuracy());
                        const float counter = std::max(0.0f, g_cj.AttackCounter() - g_cj.GunDamage() * 0.04f * (float)(evShots - n));
                        float dist;
                        if (g_lockPed >= 0) {   // lock-on: random amplitude up to min(0.2, half the scale)
                            amp = std::min(1.0f, 5.0f / wr) * amp * counter * 0.75f;
                            const float lim = std::min(0.2f, amp * 0.5f);
                            amp = lim * (2.0f * (float)rand() / (float)RAND_MAX - 1.0f);
                            dist = wr;
                        } else {
                            amp = std::min(1.0f, 15.0f / wr) * amp * counter * 0.75f;
                            dist = wr * 3.0f;
                        }
                        float rx = shotDir[1], ry = -shotDir[0], rl = std::sqrt(rx * rx + ry * ry);
                        if (rl > 1e-4f) {
                            rx /= rl; ry /= rl;
                            const float up[3] = {-shotDir[2] * ry, shotDir[2] * rx, shotDir[0] * ry - shotDir[1] * rx};   // right x dir
                            const float ang = (float)GetTickCount() * 0.0062832f;
                            const float sa = std::sin(ang) * amp, ca = std::cos(ang) * amp;
                            float e[3] = {shotDir[0] * dist + rx * sa + up[0] * ca, shotDir[1] * dist + ry * sa + up[1] * ca, shotDir[2] * dist + up[2] * ca};
                            const float el = std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
                            for (int k = 0; k < 3; k++) shotDir[k] = e[k] / el;
                        }
                    }
                    const float* g_aimDirV = shotDir;
                    float farEnd[3], hit[3], hnz = 0.0f;
                    for (int i = 0; i < 3; i++) farEnd[i] = g_aimEye[i] + g_aimDirV[i] * range;
                    int kind = 0;
                    const float t = g_world.RayCast(g_aimEye[0], g_aimEye[1], g_aimEye[2], farEnd[0], farEnd[1], farEnd[2], &kind, &hnz);
                    for (int i = 0; i < 3; i++) hit[i] = g_aimEye[i] + g_aimDirV[i] * range * t;
                    float mz[3];
                    int dummyHit = -1;
                    bool headHit = false;
                    if (g_aimTest) {
                        g_aim.shots++;
                        float dt2 = 0.0f;
                        const float reach = std::min(range * t, std::max(10.0f, g_cj.GunRange()));
                        dummyHit = g_aim.Trace(g_aimEye, g_aimDirV, reach, &dt2, &headHit);
                        if (dummyHit >= 0) {
                            for (int i = 0; i < 3; i++) hit[i] = g_aimEye[i] + g_aimDirV[i] * dt2;
                            const bool kill = g_aim.Damage(dummyHit, g_cj.GunDamage(), headHit, hit);
                            if (g_hudReady) g_hud.Hit(kill);
                            g_fx.Blood(hit);
                            g_audio.Impact(dt2);
                        }
                    }
                    if (dummyHit < 0 && g_peds.Ready()) {   // real pedestrians
                        float pt = 0.0f; bool ph = false;
                        const float reach = std::min(range * t, std::max(10.0f, g_cj.GunRange()));
                        const int pi2 = g_peds.Trace(g_aimEye, g_aimDirV, reach, &pt, &ph);
                        if (getenv("GTA_PEDDBG")) Log("PED SHOT eye %.1f %.1f %.1f dir %.2f %.2f %.2f reach %.1f -> %d; ped0 %.1f %.1f %.1f", g_aimEye[0], g_aimEye[1], g_aimEye[2], g_aimDirV[0], g_aimDirV[1], g_aimDirV[2], reach, pi2, g_peds.Count() ? g_peds.X(0) : 0.f, g_peds.Count() ? g_peds.Y(0) : 0.f, g_peds.Count() ? g_peds.Z(0) : 0.f);
                        if (pi2 >= 0) {
                            for (int i = 0; i < 3; i++) hit[i] = g_aimEye[i] + g_aimDirV[i] * pt;
                            float dmg = g_cj.GunDamage();
                            if (shotgun) {   // SetUpPelletCol: 15 pellets (4 for the SPAS-12) spread by 0.05 / accuracy * 1.3 per metre
                                const int np = !strcmp(gid2, "SPAS12") ? 4 : 15;
                                const float depth = pt * 0.05f / std::max(0.01f, g_cj.GunAccuracy()) * 1.3f;
                                float rx = g_aimDirV[1], ry = -g_aimDirV[0]; const float rl = std::max(1e-4f, std::sqrt(rx * rx + ry * ry)); rx /= rl; ry /= rl;
                                const float ux = -g_aimDirV[2] * ry, uy = g_aimDirV[2] * rx, uz = g_aimDirV[0] * ry - g_aimDirV[1] * rx;
                                int hits = 1;   // the centre line
                                for (int k = 1; k < np; k++) {
                                    const float an = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * 3.14159265f;
                                    const float sp = (float)rand() / (float)RAND_MAX * depth * 0.8f;
                                    const float o[3] = {g_aimEye[0] + rx * std::cos(an) * sp + ux * std::sin(an) * sp, g_aimEye[1] + ry * std::cos(an) * sp + uy * std::sin(an) * sp, g_aimEye[2] + uz * std::sin(an) * sp};
                                    float t2 = 0.0f;
                                    if (g_peds.Trace(o, g_aimDirV, reach, &t2, nullptr) == pi2) hits++;
                                }
                                dmg *= (float)hits;
                            } else {   // a player's shot from point blank (<= 1 m) does 150
                                float mq[3];
                                if (g_cj.MuzzleWorld(playerX, playerY, playerZ - EYE_HEIGHT, mq)) {
                                    const float dx = g_peds.X((size_t)pi2) - mq[0], dy = g_peds.Y((size_t)pi2) - mq[1], dz = g_peds.Z((size_t)pi2) + 1.0f - mq[2];
                                    if (dx * dx + dy * dy + dz * dz <= 1.0f) dmg = 150.0f;
                                }
                            }
                            const bool kill = g_peds.Shoot(pi2, dmg, ph, !strcmp(gid2, "SNIPERRIFLE") || !strcmp(gid2, "COUNTRYRIFLE"));
                            Log("PED HIT %d head %d kill %d at %.1f", pi2, (int)ph, (int)kill, pt);
                            Stats::Increment(Stats::STAT_BULLETS_THAT_HIT);
                            if (ph) Stats::Increment(Stats::STAT_NUMBER_OF_HEADSHOTS);
                            if (kill) Stats::Increment(Stats::STAT_PEOPLE_YOUVE_WASTED);
                            if (g_hudReady) g_hud.Hit(kill);
                            {   // CStats::UpdateStatsWhenWeaponHit
                                bool box = false;
                                if (WeaponStats::OnHit(gid2, &box)) {
                                    g_cj.RefreshWeaponSkills();
                                    if (box) {
                                        char sv[32];
                                        snprintf(sv, sizeof sv, "%.2f", WeaponStats::Stat(gid2));
                                        for (int i = 0; i < WeaponStats::kCount; i++) {
                                            snprintf(sv, sizeof sv, "%.2f", WeaponStats::g_stat[i]);
                                            WritePrivateProfileStringA("Stats", WeaponStats::kEntries[i].weapon, sv, g_iniPath);
                                        }
                                        if (g_hudReady) g_hud.StatUpdate(WeaponStats::Stat(gid2) / 10.0f);
                                    }
                                }
                            }
                            g_fx.Blood(hit);
                            g_audio.Impact(pt);
                            dummyHit = 0;
                        }
                    }
                    if (dummyHit < 0 && g_vehicles.Ready()) {   // cars
                        float vt = 0.0f;
                        const float reach = std::min(range * t, std::max(10.0f, g_cj.GunRange()));
                        const int vi2 = g_vehicles.Trace(g_aimEye, g_aimDirV, reach, &vt);
                        if (getenv("GTA_PEDDBG")) Log("CAR SHOT eye %.1f %.1f %.1f dir %.2f %.2f %.2f reach %.1f -> %d car0 %.1f %.1f %.1f cars %zu", g_aimEye[0], g_aimEye[1], g_aimEye[2], g_aimDirV[0], g_aimDirV[1], g_aimDirV[2], reach, vi2, g_vehicles.Count() ? g_vehicles.Car(0).x : 0.f, g_vehicles.Count() ? g_vehicles.Car(0).y : 0.f, g_vehicles.Count() ? g_vehicles.Car(0).z : 0.f, g_vehicles.Count());
                        if (vi2 >= 0 && vi2 != g_driveCar) {
                            for (int i = 0; i < 3; i++) hit[i] = g_aimEye[i] + g_aimDirV[i] * vt;
                            g_vehicles.Damage(vi2, g_cj.GunDamage() * 3.0f);
                            Log("CAR HIT %d health %.0f at %.1f", vi2, g_vehicles.Car((size_t)vi2).health, vt);
                            g_fx.Impact(hit, 0.0f);
                            g_audio.Impact(vt);
                            dummyHit = 0;
                        }
                    }
                    if (g_cj.MuzzleWorld(playerX, playerY, playerZ - EYE_HEIGHT, mz)) { g_fx.Tracer(mz, hit, g_scoped && trSpeed < 0.05f); if (getenv("GTA_TRACEDBG")) Log("TRACE active %d suppressed %d", g_fx.ActiveTraces(), (int)(g_scoped && trSpeed < 0.05f)); }
                    if (dummyHit < 0 && worldReady && t < 1.0f) g_world.DynShoot(g_aimEye, g_aimDirV, range * t + 0.1f, 40.0f, 0);
                    if (dummyHit >= 0) continue;
                    if (t < 1.0f) {
                        g_fx.Impact(hit, hnz);
                        const float dx = hit[0] - g_aimEye[0], dy = hit[1] - g_aimEye[1], dz = hit[2] - g_aimEye[2];
                        g_audio.Impact(std::sqrt(dx * dx + dy * dy + dz * dz));
                    }
                }
                g_bulletMs += dt * 1000.0f;
                g_bullets.Update(g_bulletMs, dt * 50.0f, [&](const float* a, const float* b, int dmg, float* np) {   // CBulletInfo::Update
                    float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
                    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                    if (len < 1e-4f) return false;
                    for (float& c : d) c /= len;
                    int kind = 0; float hnz = 0.0f;
                    const float tw = g_world.RayCast(a[0], a[1], a[2], b[0], b[1], b[2], &kind, &hnz);
                    float tp = 2.0f, tv = 2.0f, t = 0.0f; bool head = false;
                    int pi2 = -1, vi2 = -1;
                    if (g_peds.Ready()) { pi2 = g_peds.Trace(a, d, len, &t, &head); if (pi2 >= 0) tp = t / len; }
                    if (g_vehicles.Ready()) { vi2 = g_vehicles.Trace(a, d, len, &t); if (vi2 >= 0 && vi2 != g_driveCar) tv = t / len; else vi2 = -1; }
                    if (getenv("GTA_PEDDBG")) Log("SNIPER STEP %.1f %.1f %.1f len %.1f ped %d tp %.2f car %d tv %.2f world %.2f", a[0], a[1], a[2], len, pi2, tp, vi2, tv, tw);
                    float hit[3];
                    const float tmin = std::min(tp, std::min(tv, tw));
                    if (tmin >= 1.0f) return false;
                    for (int i = 0; i < 3; i++) hit[i] = a[i] + d[i] * len * tmin;
                    if (tmin == tp) {
                        const bool kill = g_peds.Shoot(pi2, (float)dmg, head, true);
                        Log("SNIPER HIT ped %d head %d kill %d at %.1f", pi2, (int)head, (int)kill, len * tmin);
                        Stats::Increment(Stats::STAT_BULLETS_THAT_HIT);
                        if (head) Stats::Increment(Stats::STAT_NUMBER_OF_HEADSHOTS);
                        if (kill) Stats::Increment(Stats::STAT_PEOPLE_YOUVE_WASTED);
                        if (g_hudReady) g_hud.Hit(kill);
                        g_fx.Blood(hit);
                        g_audio.Impact(len * tmin);
                        for (int i = 0; i < 3; i++) np[i] = hit[i];
                        return true;
                    }
                    if (tmin == tv) {
                        g_vehicles.Damage(vi2, (float)dmg * 3.0f);
                        Log("SNIPER HIT car %d health %.0f", vi2, g_vehicles.Car((size_t)vi2).health);
                        g_fx.Impact(hit, 0.0f);
                        g_audio.Impact(len * tmin);
                        return false;   // the bullet flies on through a car
                    }
                    g_fx.Impact(hit, hnz);
                    g_audio.Impact(len * tmin);
                    g_world.DynShoot(a, d, len * tmin + 0.1f, 40.0f, 0);
                    Log("SNIPER HIT world at %.1f %.1f %.1f", hit[0], hit[1], hit[2]);
                    return false;   // retail keeps the bullet moving past walls
                });
                st.playSec += dt;
                const bool air = g_cj.InAir();
                if (st.havePrev && !air && !g_cj.Swimming()) st.dist += hypotf(playerX - st.px, playerY - st.py);
                if (air && !st.wasAir) st.jumps++;
                st.wasAir = air; st.px = playerX; st.py = playerY; st.havePrev = true;
                const float fl = g_cj.DamageFlash();
                if (fl > 0.95f && st.lastFlash <= 0.95f) st.hurt++;
                st.lastFlash = fl;
            }
            FeMenu::PlayerInfo pi;
            pi.x = playerX; pi.y = playerY; pi.z = playerZ - EYE_HEIGHT; pi.heading = g_cj.Heading();
            pi.zone = g_hud.ZoneAt(pi.x, pi.y, pi.z);
            pi.weapon = g_cj.GunIndex() >= 0 ? g_cj.GunName() : "";
            pi.health = g_cj.Health(); pi.maxHealth = g_cj.MaxHealth(); pi.armor = g_cj.Armor();
            pi.hour = (int)clockHour; pi.minute = (int)((clockHour - floorf(clockHour)) * 60.0f);
            pi.playSec = st.playSec; pi.distFoot = st.dist;
            pi.shots = st.shots; pi.reloads = st.reloads; pi.jumps = st.jumps; pi.hurt = st.hurt;
            pi.ammoClip = g_cj.GunAmmo(); pi.ammoTotal = g_cj.GunTotalAmmo();
            g_menu.SetHud(&g_hud);
            pi.blips = g_blips;
            g_menu.SetInfo(pi);
        }

        // ---- Audio cues: menu, guns, footsteps, jumps, pain ----
        if (g_audio.Ready()) {
            const FeSettings& as = g_menu.Settings();
            g_audio.SetVolumes(as.masterVol, as.sfxVol);
            g_radio.SetVolume(as.musicVol);
            g_radio.SetClockHour((int)(g_gameSeconds / 3600.0f) % 24);
            g_radio.Update();
            if (int ui = g_menu.TakeUiEvent()) g_audio.Ui(ui);
            struct Pending { float t; const char* gun; int kind; };
            static std::vector<Pending> s_pending;
            static float s_stepDist = 0.0f, s_prevX = 0, s_prevY = 0, s_lastFlash = 0.0f;
            static bool s_wasAir = false, s_havePrev = false;
            static int s_lastGun = -2;
            {   // ambience: city murmur always, rain by the weather, wind while falling fast
                const char* wn = g_timecycOk ? g_timecyc.WeatherName(g_weatherIdx).c_str() : "";
                const bool rainy = strstr(wn, "RAIN") || strstr(wn, "STORM");
                static float s_wind = 0.0f;
                const float fall = cjReady && g_cj.InAir() ? std::min(1.0f, std::max(0.0f, (-velZ - 8.0f) / 14.0f)) : 0.0f;
                s_wind += (fall - s_wind) * std::min(1.0f, dt * 3.0f);
                g_audio.Ambient(g_menu.IsOpen() ? 0.4f : 1.0f, rainy ? 1.0f : 0.0f, s_wind);
            }
            if (cjReady && g_cj.WeaponIndex() != s_lastGun) {
                if (s_lastGun != -2) g_audio.WeaponSwitch();
                s_lastGun = g_cj.WeaponIndex();
            }
            if (cjReady && !g_menu.IsOpen() && !g_devPaused) {
                const char* gid = g_cj.GunId();
                for (int n = evShots; n > 0; n--) g_audio.Gun(gid, 0);
                for (int n = evDry; n > 0; n--) g_audio.Gun(gid, 1);
                for (int n = evReload; n > 0; n--) {
                    g_audio.Gun(gid, 2);
                    s_pending.push_back({ 0.9f, gid, 3 });
                }
                for (size_t i = 0; i < s_pending.size();) {
                    s_pending[i].t -= dt;
                    if (s_pending[i].t <= 0.0f) { g_audio.Gun(s_pending[i].gun, s_pending[i].kind); s_pending.erase(s_pending.begin() + i); }
                    else i++;
                }
                // Footsteps: one per stride walked on the ground.
                bool air = g_cj.InAir();
                if (s_havePrev && !air && !g_cj.Swimming()) {
                    s_stepDist += hypotf(playerX - s_prevX, playerY - s_prevY);
                    const float spd = hypotf(g_cj.VelX(), g_cj.VelY());
                    if (s_stepDist >= 0.6f + 0.2f * spd) { s_stepDist = 0.0f; g_audio.Step(BANK_FEET_GENERIC, 0.35f + 0.04f * std::min(spd, 6.0f)); }
                }
                if (!s_wasAir && air) g_audio.Step(BANK_FEET_GENERIC, 0.4f);   // take-off
                s_wasAir = air; s_prevX = playerX; s_prevY = playerY; s_havePrev = true;
                float fl = g_cj.DamageFlash();
                if (fl > 0.95f && s_lastFlash <= 0.95f) g_audio.Pain();
                s_lastFlash = fl;
            }
        }

        // ---- HUD (CHud::Draw): vitals, weapon, money, radar ----
        if (g_hudReady && cjReady && !g_menu.IsOpen() && !g_devPaused) {
            HudData hd;
            const FeSettings& hs = g_menu.Settings();
            hd.dt = dt;
            hd.rewrite = hs.movementStyle == 1;
            hd.scale = hs.hudScale;
            hd.stamina = g_stamina;
            hd.wheel = g_wheelOpen;
            if (g_wheelOpen) {
                hd.wheelSel = g_wheelSel;
                hd.wheelCat = g_wheelCat;
                for (int c = 0; c < 8; c++) hd.wheelPick[c] = g_wheelPick[c];
                WheelItem f; f.name = "Fists"; f.melee = true;
                hd.wheelItems.push_back(f);
                for (int i = 0; i < g_cj.GunCount(); i++) {
                    WheelItem w;
                    w.name = g_cj.GunNameAt(i); w.icon = g_cj.GunIconAt(i); w.cat = g_cj.GunOwnedAt(i) ? WheelItemCat(i + 1) : -1;
                    w.clip = g_cj.GunClipAt(i); w.total = g_cj.GunTotalAt(i); w.clipSize = g_cj.GunClipSizeAt(i);
                    hd.wheelItems.push_back(w);
                }
            }
            hd.showHud = hs.showHud;
            hd.radarOn = hs.radarMode == 0;
            hd.crosshairDot = hs.crosshairDot; hd.hitMarker = hs.hitMarker; hd.damageFlash = hs.damageFlash;
            hd.health = g_cj.Health(); hd.maxHealth = g_cj.MaxHealth(); hd.armor = g_cj.Armor();
            hd.breath = g_cj.Breath(); hd.showBreath = g_cj.ShowBreath();
            if (getenv("GTA_STATDBG") && (frame % 50) == 0) Log("BREATH %.3f hp %.1f", g_cj.Breath(), g_cj.Health());
            hd.damageFlashAmount = g_cj.DamageFlash();
            int& s_money = g_money;
            hd.money = s_money; hd.wanted = g_wanted;
            hd.hour = (int)clockHour; hd.minute = (int)((clockHour - floorf(clockHour)) * 60.0f);
            hd.armed = g_cj.GunIndex() >= 0 || g_cj.HoldingThrown();
            hd.weaponIcon = g_cj.GunIcon();
            hd.ammoClip = g_cj.GunAmmo(); hd.ammoTotal = g_cj.GunTotalAmmo(); hd.clipSize = g_cj.GunClipSize();
            hd.reloading = g_cj.Reloading();
            hd.weaponName = g_cj.GunName();
            hd.aiming = g_cj.Aiming() && !g_scoped;
            hd.crossX = g_chairX; hd.crossY = g_chairY;
            hd.crossRadius = g_cj.WeaponRadiusOnScreen();
            hd.lockOn = false;
            if (g_lockPed >= 0 && (size_t)g_lockPed < g_peds.Count()) {   // project the target for the marker
                const D3DMATRIX proj = MatrixPerspectiveFov(g_fovDeg * 3.14159f / 180.0f, (float)WIDTH / (float)HEIGHT, 0.5f, 3000.0f);
                const float* v = &g_lastView._11;
                const float* pm = &proj._11;
                const float pw[4] = {g_lockPos[0], g_lockPos[1], g_lockPos[2], 1.0f};
                float vv[4] = {}, cc[4] = {};
                for (int j = 0; j < 4; j++) for (int k = 0; k < 4; k++) vv[j] += pw[k] * v[k * 4 + j];
                for (int j = 0; j < 4; j++) for (int k = 0; k < 4; k++) cc[j] += vv[k] * pm[k * 4 + j];
                if (cc[3] > 0.1f) {
                    hd.lockOn = true;
                    hd.lockX = (cc[0] / cc[3] * 0.5f + 0.5f) * (float)WIDTH;
                    hd.lockY = (0.5f - cc[1] / cc[3] * 0.5f) * (float)HEIGHT;
                    const float dx = g_lockPos[0] - playerX, dy = g_lockPos[1] - playerY;
                    hd.lockSize = 1.0f - sqrtf(dx * dx + dy * dy) * 0.02f;
                    hd.lockHealth = g_peds.HealthFrac((size_t)g_lockPed);
                }
            }
            hd.spread = std::min(1.0f, sqrtf(g_cj.VelX() * g_cj.VelX() + g_cj.VelY() * g_cj.VelY()) / 6.0f);
            hd.firstPerson = g_playerCam.FirstPerson(hs.cameraStyle);
            hd.x = playerX; hd.y = playerY; hd.z = playerZ - EYE_HEIGHT;
            if (g_aimTest) {
                const D3DMATRIX& V = g_renderer.GetViewMatrix();
                const D3DMATRIX& P = g_renderer.GetProjMatrix();
                auto project = [&](float x, float y, float z, float& sx, float& sy) {
                    const float vx = x * V._11 + y * V._21 + z * V._31 + V._41, vy = x * V._12 + y * V._22 + z * V._32 + V._42;
                    const float vz = x * V._13 + y * V._23 + z * V._33 + V._43, vw = x * V._14 + y * V._24 + z * V._34 + V._44;
                    const float cx = vx * P._11 + vy * P._21 + vz * P._31 + vw * P._41, cy = vx * P._12 + vy * P._22 + vz * P._32 + vw * P._42;
                    const float cw = vx * P._14 + vy * P._24 + vz * P._34 + vw * P._44;
                    if (cw <= 0.05f) return false;
                    sx = (cx / cw) * 0.5f + 0.5f; sy = 0.5f - (cy / cw) * 0.5f;
                    return sx > -0.1f && sx < 1.1f && sy > -0.1f && sy < 1.1f;
                };
                for (const AimDummy& d : g_aim.Dummies()) {
                    if (!d.alive()) continue;
                    float sx, sy;
                    if (!project(d.x, d.y, d.z + 2.1f, sx, sy)) continue;
                    HudLabel l;
                    l.x = sx; l.y = sy - 0.03f;
                    char b[24];
                    std::snprintf(b, sizeof b, "%dm", (int)hypotf(d.x - playerX, d.y - playerY));
                    l.text = b; l.bar = d.health * 100.0f / d.maxHealth;
                    l.scale = std::min(1.0f, std::max(0.55f, 9.0f / std::max(1.0f, hypotf(d.x - playerX, d.y - playerY))));
                    hd.labels.push_back(l);
                }
                for (const AimFloat& f : g_aim.Floats()) {
                    float sx, sy;
                    if (!project(f.x, f.y, f.z + f.age * 0.9f, sx, sy)) continue;
                    HudLabel l;
                    l.x = sx; l.y = sy;
                    char b[24];
                    std::snprintf(b, sizeof b, f.head ? "-%d HEAD" : "-%d", f.amount);
                    l.text = b;
                    l.color = ((uint32_t)(255.0f * std::max(0.0f, 1.0f - f.age / 0.9f)) << 24) | (f.head ? 0xFFD23C : 0xFFFFFF);
                    l.scale = f.head ? 1.3f : 1.0f;
                    hd.labels.push_back(l);
                }
                char sb[96];
                std::snprintf(sb, sizeof sb, "AIM TEST   shots %d   hits %d   accuracy %d%%   headshots %d   kills %d",
                              g_aim.shots, g_aim.hits, g_aim.shots ? g_aim.hits * 100 / g_aim.shots : 0, g_aim.headshots, g_aim.kills);
                HudLabel sl;
                sl.x = 0.02f; sl.y = 0.22f; sl.text = sb; sl.left = true; sl.color = 0xFFFFFFFF;
                hd.labels.push_back(sl);
            }
            hd.camYaw = yaw; hd.heading = g_cj.Heading();
            hd.zone = g_hud.ZoneAt(hd.x, hd.y, hd.z);
            hd.hasWaypoint = g_menu.Waypoint(hd.wpX, hd.wpY);
            if (static bool s_bt = false; !s_bt && getenv("GTA_BLIPTEST")) {   // dev: a few sprite blips around CJ
                s_bt = true;
                const int sp[4] = {35, 10, 6, 21};
                for (int i = 0; i < 4; i++) {
                    RadarBlip b; b.handle = g_nextBlip++; b.sprite = sp[i]; b.shortRange = i == 2;
                    b.x = playerX + (i - 1.5f) * 60.0f; b.y = playerY + 80.0f * (i % 2 ? 1 : -1); b.z = playerZ;
                    g_blips.push_back(b);
                }
            }
            hd.blips = g_blips;
            if (g_hudReady && g_scoped) { const int sgt = g_cj.GunIndex() >= 0 ? g_cj.GunType(g_cj.GunIndex()) : 0; g_hud.Scope(g_renderer, sgt == 43, sgt == 35 || sgt == 36); }
            if (g_hudReady) g_hud.Goggles(g_renderer, g_cj.Goggles());
            if (g_hudReady && g_camFlash > 0.0f) { g_hud.Flash(g_renderer, g_camFlash); g_camFlash = std::max(0.0f, g_camFlash - dt * 3.0f); }
            g_hud.Draw(g_renderer, hd);
        }
        if (scmRun && g_hudReady) g_hud.DrawScript(g_renderer, g_sg.Overlay());
        else if (g_hudReady && g_enex.Fade() > 0.0f) { ScriptOverlay eo; eo.fade = g_enex.Fade(); g_hud.DrawScript(g_renderer, eo); }
        if (g_hudReady) g_hud.Brightness(g_renderer, g_menu.Settings().brightness);

        // ---- SA pause menu + the dev key list beside it ----
        if (g_menuReady && g_menu.IsOpen()) {
            dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
            const char* wname = g_timecycOk ? g_timecyc.WeatherName(g_weatherIdx).c_str() : "-";
            char hour[32];
            snprintf(hour, sizeof(hour), "Hour -/+ (%.1f h)", clockHour);
            auto onoff = [](bool b) { return std::string(b ? " (on)" : " (off)"); };
            g_menu.devKeys = {
                {"F1", "Debug overlay" + onoff(g_devOverlay)},
                {"F2", "Wireframe" + onoff(g_devWireframe)},
                {"F3", "Collision view" + onoff(g_devShowCollision)},
                {"F4", "Noclip fly" + onoff(g_devNoClip)},
                {"F5", "Screenshot"},
                {"F6", "Time flow" + onoff(g_timeFlow)},
                {"F7/F8", hour},
                {"F9", "Dump nearby"},
                {"F10", "Lighting" + onoff(g_devLighting)},
                {"F11/12", "Save / load pos"},
                {"N", std::string("Weather (") + wname + ")"},
                {"U", "Unstick +3m"},
                {"R", "Reset profiler"},
                {"V", std::string("Camera (") + (cjReady ? PlayerCamera::ViewName(g_menu.Settings().cameraStyle, g_playerCam.View())
                                                                 : thirdPerson ? "3rd" : "1st") + ")"},
                {"WASD", "Move, Alt walk"},
                {"Space", "Sprint"},
                {"Shift", "Jump"},
            };
            g_menu.Draw(g_renderer);
        }

        g_prof.Begin(FrameProfiler::P_PRESENT);
        g_renderer.EndFrame();
        g_prof.End(FrameProfiler::P_PRESENT);

        if (g_prof.FrameEnd(kHitchMs)) {
            const FrameProfiler::Sample& h = g_prof.Last();
            Log("HITCH f%d %.2f ms: stream %.2f (loads %zu in %.2fms, col %zu in %.2fms) "
                "render %.2f present %.2f other %.2f  pos=(%.0f,%.0f)",
                frame, h.total, h.phase[FrameProfiler::P_STREAM], h.loads, h.loadMs,
                h.cols, h.colMs, h.phase[FrameProfiler::P_RENDER],
                h.phase[FrameProfiler::P_PRESENT], h.Other(), playerX, playerY);
        }

        if (takeScriptedShot) {
            takeScriptedShot = false;
            char shotName[MAX_PATH];
            snprintf(shotName, sizeof(shotName), "%s\\scripted_%d.bmp", shotDir, scriptedShotIdx);
            g_renderer.SaveScreenshot(shotName);
            Log("SCRIPTED shot %d saved to %s at (%.1f, %.1f, %.1f) yaw=%.1f",
                scriptedShotIdx, shotName, playerX, playerY, playerZ, yaw * 57.2958f);
        }

        if (takeFlyoverShot) {
            takeFlyoverShot = false;
            char shotName[MAX_PATH];
            snprintf(shotName, sizeof(shotName), "%s\\flyover_%d.bmp", shotDir, flyoverShotIdx);
            g_renderer.SaveScreenshot(shotName);
            Log("FLYOVER shot %d saved to %s at (%.1f, %.1f, %.1f)",
                flyoverShotIdx, shotName, playerX, playerY, playerZ);
        }

        // ---- Frame limiter (pause menu: 60/120/144/off) ----
        // Sleep on a high-resolution waitable timer to ~1 ms before the
        // deadline, then spin the rest for an even cadence. Deadlines advance
        // by whole periods (no drift); a slow frame re-bases instead of
        // bursting to catch up. Headless runs (--frames) are never limited.
        {
            static HANDLE limitTimer = nullptr;
            static LONGLONG nextTick = 0;
            int limit = g_menu.Settings().frameLimit;
            if (!g_hasFocus && g_escOpensMenu) limit = 30;   // in the background: don't hog the GPU
            if (limit > 0 && maxFrames == 0) {
                if (!limitTimer) {
                    limitTimer = CreateWaitableTimerExW(nullptr, nullptr,
                        0x00000002 /*CREATE_WAITABLE_TIMER_HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
                    if (!limitTimer) limitTimer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
                }
                const LONGLONG period = qpcFreq.QuadPart / limit;
                LARGE_INTEGER t; QueryPerformanceCounter(&t);
                if (nextTick == 0 || t.QuadPart - nextTick > period) nextTick = t.QuadPart;
                nextTick += period;
                LONGLONG remain = nextTick - t.QuadPart;
                LONGLONG remainUs = remain * 1000000 / qpcFreq.QuadPart;
                if (remainUs > 1500 && limitTimer) {
                    LARGE_INTEGER due;
                    due.QuadPart = -(remainUs - 1000) * 10;   // relative, 100 ns units
                    if (SetWaitableTimer(limitTimer, &due, 0, nullptr, nullptr, FALSE))
                        WaitForSingleObject(limitTimer, INFINITE);
                }
                do { QueryPerformanceCounter(&t); } while (t.QuadPart < nextTick);
            } else {
                nextTick = 0;
            }
        }

        // Dev (headless tests): GTA_SHOTSEQ="dir|start|step|count" saves
        // dir\seq_<frame>.bmp every step frames.
        if (const char* seq = getenv("GTA_SHOTSEQ")) {
            char dir[MAX_PATH] = {};
            int start = 0, step = 1, count = 0;
            if (const char* bar = strchr(seq, '|')) {
                memcpy(dir, seq, (size_t)std::min<ptrdiff_t>(bar - seq, MAX_PATH - 1));
                sscanf(bar + 1, "%d|%d|%d", &start, &step, &count);
                if (step > 0 && frame >= start && (frame - start) % step == 0 && (frame - start) / step < count) {
                    char shot[MAX_PATH];
                    snprintf(shot, sizeof(shot), "%s\\seq_%04d.bmp", dir, frame);
                    g_renderer.SaveScreenshot(shot);
                }
            }
        }

        frame++;
        g_scriptFrame = frame;
        if (maxFrames > 0 && frame >= maxFrames) { if (g_audio.Ready()) Log("AUDIO: %d sounds started, last %s", g_audio.Played(), g_audio.LastPlayed().c_str()); break; }
    }

    if (playMode) { SetPointerCapture(false); }

    if (!shotPath.empty()) {
        // Relative screenshot paths go to shotDir
        std::string finalShot = shotPath;
        if (shotPath.find(':') == std::string::npos && shotPath[0] != '\\') {
            finalShot = std::string(shotDir) + "\\" + shotPath;
        }
        g_renderer.SaveScreenshot(finalShot.c_str());
        Log("Screenshot saved to %s", finalShot.c_str());
    }
    if (const char* dr = getenv("GTA_DUMP_NEARBY")) {   // headless: dump nearby insts to inst_dump.txt
        float rad = (float)atof(dr); if (rad <= 0) rad = 80.0f;
        if (worldReady) g_world.DumpNearby(playerX, playerY, playerZ, rad);
    }
    if (g_probeCollision && worldReady) {   // --probe-collision: solidity ground-truth report
        char pp[MAX_PATH];
        snprintf(pp, sizeof(pp), "%s\\collision_probe.txt", shotDir);
        g_world.DumpCollisionProbe(playerX, playerY, playerZ - EYE_HEIGHT, 18.0f, pp);
        Log("PROBE: collision report at (%.1f,%.1f) -> %s", playerX, playerY, pp);
    }
    Log("Done: %d frames, %u map objects", frame, (unsigned)mapObjects.size());
    {
        double a, p, w;
        g_prof.Window(a, p, w);
        const FrameProfiler::Sample& pw = g_prof.WorstEver();
        Log("PROFILE: last-window avg %.2f ms p99 %.2f ms worst %.2f ms; hitches %zu over %zu frames",
            a, p, w, g_prof.Hitches(), g_prof.Frames());
        Log("PROFILE: worst frame %.2f ms = stream %.2f (loads %zu in %.2fms, col %zu in %.2fms) "
            "render %.2f present %.2f other %.2f",
            pw.total, pw.phase[FrameProfiler::P_STREAM], pw.loads, pw.loadMs, pw.cols, pw.colMs,
            pw.phase[FrameProfiler::P_RENDER], pw.phase[FrameProfiler::P_PRESENT], pw.Other());
    }
    Log("World final: loaded=%zu visible=%zu solidBoxes=%zu drawnInsts=%zu drawMeshCalls=%zu skippedInsts=%zu",
        g_world.LoadedModelCount(), g_world.VisibleInstances(), g_world.SolidBoxCount(),
        g_world.LastDrawnInsts(), g_world.LastDrawMeshCalls(), g_world.LastSkippedInsts());
    if (worldReady) {
        float fg = g_world.GetGroundHeight(playerX, playerY, playerZ - EYE_HEIGHT + 2.0f);
        Log("Final pos: (%.1f, %.1f, %.1f) ground=%.2f", playerX, playerY, playerZ,
            fg > ColLoader::kNoGround * 0.5f ? fg : playerZ - EYE_HEIGHT);
        Log("Collision: %u blocked moves, %zu solid boxes active",
            g_solidBlockCount, g_world.SolidBoxCount());
    }
    if (g_log) fclose(g_log);

    if (worldReady) g_world.Shutdown();

    for (auto& obj : mapObjects)
        for (auto* mesh : obj.meshes) g_renderer.DestroyMesh(mesh);
    g_water.Shutdown(g_renderer);
    g_sky.Shutdown(g_renderer);
    if (playerMesh) g_renderer.DestroyMesh(playerMesh);
    if (playerTex) playerTex->Release();
    for (auto& kv : d3dTexCache)
        if (kv.second) g_renderer.DestroyTexture(kv.second);
    return 0;
}
