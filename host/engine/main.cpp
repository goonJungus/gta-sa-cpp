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
#include "PlayerPed.h"
#include "Camera.h"
#include "CameraShake.h"
#include "Water.h"
#include "Sky.h"
#include "ClimbScan.h"
#include "Audio.h"

static D3DRenderer g_renderer;
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
                const int want = *k == '+' ? VK_SHIFT : *k == '^' ? VK_MENU : *k == '_' ? VK_SPACE : *k == '#' ? VK_F4 : *k == '~' ? VK_RBUTTON : *k == '|' ? VK_LBUTTON
                               : toupper((unsigned char)*k);
                if (want == vk) return true;
            }
        if (*p == ';') p++;
    }
    return false;
}
static SHORT GameKey(int vk) {
    if (ScriptedKey(vk)) return (SHORT)0x8000;
    return g_hasFocus ? GetAsyncKeyState(vk) : 0;
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
static bool g_hudReady = false;
static bool g_menuReady = false;
static bool g_escOpensMenu = false;      // play mode (not the capture modes)
static int  g_keyQueue[64];              // WM_KEYDOWN -> main loop (menu input)
static int  g_keyQueueN = 0;
static char g_iniPath[MAX_PATH] = {};    // <exe_dir>\gtasa_cpp.ini

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

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_DESTROY:
        g_running = false;
        PostQuitMessage(0);
        return 0;
    case WM_ACTIVATE:
        g_hasFocus = LOWORD(wp) != WA_INACTIVE;
        if (g_hasFocus) SetFocus(hwnd);   // keyboard back to us on click/alt-tab return
        Log("WINDOW: %s (state %u, fg=%p self=%p focus=%p)", g_hasFocus ? "activated" : "deactivated",
            (unsigned)LOWORD(wp), (void*)GetForegroundWindow(), (void*)hwnd, (void*)GetFocus());
        return 0;
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
        Log("AUDIO: %s", g_audio.Init(GameDir()) ? "ready" : "unavailable (audio/CONFIG, audio/sfx?)");
        if (g_audio.Ready() && getenv("GTA_AUDIOTEST"))
            Log("AUDIO: weapons bank %d sounds, menu bank %d, feet %d, pain %d", g_audio.SoundCount(BANK_WEAPONS),
                g_audio.SoundCount(BANK_FRONTEND_MENU), g_audio.SoundCount(BANK_FEET_GENERIC), g_audio.SoundCount(BANK_PAIN_CARL));
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
    const bool cjReady = g_cj.Init(g_renderer, GameDir());
    if (cjReady) {   // [Player] Fat / Muscle in the ini (0..100), GTA_FAT / GTA_MUSCLE override
        float fat = GetPrivateProfileIntA("Player", "Fat", 0, g_iniPath) / 100.0f;
        float mus = GetPrivateProfileIntA("Player", "Muscle", 0, g_iniPath) / 100.0f;
        if (const char* e = getenv("GTA_FAT")) fat = (float)atof(e);
        if (const char* e = getenv("GTA_MUSCLE")) mus = (float)atof(e);
        if (const char* e = getenv("GTA_GUN")) g_cj.SelectGun(atoi(e));   // dev: weapon index
        if (fat > 0.0f || mus > 0.0f) g_cj.SetBodyShape(fat, mus);
    }
    Log("PLAYER: CJ model %s", cjReady ? "ready" : "FAILED (using the box)");

    // ---- Player state (first-person, real human scale) ----
    float playerX = 2505.0f, playerY = -1710.0f, playerZ = 13.7f; // eye
    float camPosX = 2505.0f, camPosY = -1710.0f, camPosZ = 13.7f; // camera world pos (the sky follows it)
    float yaw = 1.5708f;  // radians, facing north (+Y) toward CJ's house
    float pitch = 0.0f;   // radians, positive = look up
    float velZ = 0.0f;    // vertical velocity for jumping
    bool onGround = true;
    bool cjBlocked = false;   // CJ pushed into a wall last frame
    bool thirdPerson = g_startThirdPerson; // V toggles 1st/3rd person camera
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
        ShowCursor(FALSE);
        // Center mouse
        RECT rc; GetClientRect(g_hwnd, &rc);
        POINT c = {(rc.right-rc.left)/2, (rc.bottom-rc.top)/2};
        ClientToScreen(g_hwnd, &c);
        SetCursorPos(c.x, c.y);
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
                    if (!ClimbScan(g_world, 1, ped, rot, hold, &hr)) continue;
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

        // ---- Dev/test keys (ignored in the scripted/flyover capture modes) ----
        if (playMode && !scripted && !flyover) {
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
            if (g_escOpensMenu && g_menuReady) {
                bool wasOpen = g_menu.IsOpen();
                // Retail pauses when the game loses focus (alt-tab / click away).
                if (!g_hasFocus && !g_menu.IsOpen()) g_menu.Open();
                for (int k = 0; k < nk; k++) {
                    if (g_menu.IsOpen()) g_menu.OnKey(keys[k]);
                    else if (keys[k] == VK_ESCAPE || keys[k] == 'P') g_menu.Open();
                }
                if (g_menu.IsOpen()) {
                    // Pointer in backbuffer pixels (the client may differ from
                    // the render size when --res is used borderless).
                    POINT mp; GetCursorPos(&mp); ScreenToClient(g_hwnd, &mp);
                    RECT cr; GetClientRect(g_hwnd, &cr);
                    float sx = cr.right > 0 ? (float)WIDTH / (float)cr.right : 1.0f;
                    float sy = cr.bottom > 0 ? (float)HEIGHT / (float)cr.bottom : 1.0f;
                    static bool lmbWas = false;
                    bool lmb = (GameKey(VK_LBUTTON) & 0x8000) != 0;
                    bool inside = g_hasFocus && GetForegroundWindow() == g_hwnd;
                    g_menu.OnMouse((float)mp.x * sx, (float)mp.y * sy, inside && lmb && !lmbWas, inside && lmb);
                    lmbWas = lmb;
                }
                if (g_menu.TakeSettingsChanged()) {
                    const FeSettings& s = g_menu.Settings();
                    if (s.vsync != g_renderer.Vsync() && !g_renderer.SetVsync(s.vsync))
                        Log("SETTINGS: VSync reset failed");
                    g_world.SetHdDrawScale(s.drawDist);
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
                        SetCursorPos(c.x, c.y);
                    }
                    Log("FRONTEND: pause menu opened");
                } else if (wasOpen && !g_menu.IsOpen()) {
                    // Drop the mouse motion made in the menu so the view does not jump.
                    g_rawMouseDX = 0; g_rawMouseDY = 0; g_rawMouseHaveAbs = false;
                    Log("FRONTEND: resumed");
                }
                g_devPaused = g_menu.IsOpen();
            }
        }
        // Pause menu = frozen simulation, rendering keeps running.
        if (g_devPaused) dt = 0.0f;

        // ---- Time of day: sky colours, sun, fog + draw distance ----
        float fogStart = 400.0f, fogEnd = 2500.0f;
        float clockHour = 12.0f;
        if (g_timeFlow && playMode) g_gameSeconds += dt * 60.0f;  // 1 real s = 1 game min
        if (g_timecycOk) {
            clockHour = fmodf(g_gameSeconds / 3600.0f, 24.0f);
            if (clockHour < 0.0f) clockHour += 24.0f;
            TimeCycleHour tc;
            g_timecyc.Sample(g_weatherIdx, clockHour, tc);
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
            // ---- Mouse look ----
            if (mouseLook && !g_devPaused) {
                int dx = 0, dy = 0;
                if (g_rawMouseOk) {
                    // Consume the accumulated raw deltas (relative, no cursor
                    // warp, no acceleration bias -> no trackpad spin).
                    dx = (int)g_rawMouseDX; dy = (int)g_rawMouseDY;
                    g_rawMouseDX = 0; g_rawMouseDY = 0;
                } else {
                    // Fallback: old cursor-warp delta (plain mice only).
                    POINT mp; GetCursorPos(&mp);
                    RECT rc; GetClientRect(g_hwnd, &rc);
                    POINT center = {(rc.right-rc.left)/2, (rc.bottom-rc.top)/2};
                    ClientToScreen(g_hwnd, &center);
                    dx = mp.x - center.x; dy = mp.y - center.y;
                    if (dx != 0 || dy != 0) SetCursorPos(center.x, center.y);
                }
                if (dx != 0 || dy != 0) {
                    // Post-mirror-fix sign: mouse right (dx>0) must turn the view right.
                    yaw   -= dx * MOUSE_SENS;
                    pitch -= dy * MOUSE_SENS;
                    if (pitch >  1.5f) pitch =  1.5f;
                    if (pitch < -1.5f) pitch = -1.5f;
                }
            } else {
                // Not looking (paused / menu): drop any raw motion so it can't
                // pile up and snap the view when control resumes.
                g_rawMouseDX = 0; g_rawMouseDY = 0;
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
                if (onGround && !g_cj.Busy() && jumpJustDown)   // launch from the ground
                    climbStarted = g_cj.TryStartClimb(playerX, playerY, feet, true, climbScan);
                else if (!onGround && velZ > -10.0f && sqrtf(g_cj.VelX() * g_cj.VelX() + g_cj.VelY() * g_cj.VelY()) > 0.05f * 50.0f)
                    climbStarted = g_cj.TryStartClimb(playerX, playerY, feet, false, climbScan);   // grab mid-jump
                if (climbStarted) { onGround = true; velZ = 0.0f; Log("CLIMB start at (%.2f, %.2f, %.2f) %s", playerX, playerY, feet, onGround ? "" : ""); }
            }
            if (onGround && !swimming && !(cjReady && g_cj.Busy()) && jumpJustDown && !climbStarted) {
                velZ = JUMP_VEL;
                onGround = false;
                jumped = true;
            }
            // ---- CJ: the animation decides how fast he moves ----
            // (CPed extracts the root motion of the blended walk/run/sprint
            // clips, so he speeds up and slows down with them.)
            float vx = mx * speed, vy = my * speed;
            if (cjReady) {
                PlayerPed::Input pin;
                pin.dt = dt;
                pin.stickX = mx;
                pin.stickY = my;
                pin.walk = (GameKey(VK_MENU) & 0x8000) || (g_autoWalk && g_autoGait == 1);
                pin.sprint = (GameKey(VK_SPACE) & 0x8000) || (g_autoWalk && g_autoGait == 2);
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
                pin.firstPerson = g_playerCam.FirstPerson(g_menu.Settings().cameraStyle);
                pin.worldX = playerX; pin.worldY = playerY; pin.feetZ = playerZ - EYE_HEIGHT;
                pin.jumpHeld = jumpKeyDown;
                pin.jumpDown = jumpJustDown;
                {
                    static bool s_prevDuck = false;
                    const bool duckKey = (GameKey('C') & 0x8000) != 0;
                    pin.duckJust = duckKey && !s_prevDuck;
                    s_prevDuck = duckKey;
                    pin.aim = (GameKey(VK_RBUTTON) & 0x8000) != 0;
                }
                {
                    static bool s_prevFire = false, s_prevReload = false, s_prevNext = false;
                    const bool fireKey = (GameKey(VK_LBUTTON) & 0x8000) != 0 && !g_menu.IsOpen();
                    const bool relKey = (GameKey('R') & 0x8000) != 0, nextKey = (GameKey('Q') & 0x8000) != 0;
                    pin.fireHeld = fireKey;
                    pin.fireJust = fireKey && !s_prevFire;
                    pin.reloadJust = relKey && !s_prevReload;
                    pin.autoReload = pin.rewrite && g_menu.Settings().autoReload;
                    if (nextKey && !s_prevNext) g_cj.NextGun();
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
                pin.climbScan = climbScan;
                pin.footIk = pin.rewrite && !getenv("GTA_NOFOOTIK");
                if (worldReady) pin.groundAt = [](float x, float y, float z) { return g_world.GetGroundHeight(x, y, z); };
                g_cj.Update(pin);
                if (!scripted && !flyover) { vx = g_cj.VelX(); vy = g_cj.VelY(); }
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
                playerZ += velZ * dt;
                if (playerZ <= gz + EYE_HEIGHT) {
                    playerZ = gz + EYE_HEIGHT; velZ = 0; onGround = true;
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
                ci.haveSpine = g_cj.BoneWorldPos(3, playerX, playerY, playerZ - EYE_HEIGHT, ci.spine);
                ci.haveHead = g_cj.BoneWorldPos(5, playerX, playerY, playerZ - EYE_HEIGHT, ci.head);
                if (worldReady && !g_devNoClip)
                    ci.ray = [](const float a[3], const float b[3]) {
                        return g_world.RayCast(a[0], a[1], a[2], b[0], b[1], b[2]);
                    };
                PlayerCamera::Output co = g_playerCam.Update(ci);
                if (camStyle == PlayerCamera::kRewrite) {   // rewrite shake: displaces the camera along its own axes
                    static uint32_t shakeMs = 0;
                    static const char* devShake = getenv("GTA_SHAKE");   // dev: strength fired once at frame 20
                    shakeMs += (uint32_t)(dt * 1000.0f);
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
                g_renderer.SetViewMatrix(view);
                camPosX = co.eye[0]; camPosY = co.eye[1]; camPosZ = co.eye[2];
                g_hideCjHead = co.hideHead;
                nearClip = co.nearClip;
            } else if (!thirdPerson) {
                float tx = playerX + dirX;
                float ty = playerY + dirY;
                float tz = playerZ + dirZ;
                D3DMATRIX view = MatrixLookAt(playerX, playerY, playerZ, tx, ty, tz, 0, 0, 1);
                g_renderer.SetViewMatrix(view);
                camPosX = playerX; camPosY = playerY; camPosZ = playerZ;
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
            }
            const float wantFov = g_menu.Settings().movementStyle == 1 ? g_menu.Settings().fov : 70.0f;
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
                             g_timecyc.WeatherName(g_weatherIdx), g_tcNow);
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
            if (cjReady && !scripted && !flyover) { g_cj.Draw(g_renderer, playerX, playerY, feetZ, g_hideCjHead); return; }
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

        // ---- Audio cues: menu, guns, footsteps, jumps, pain ----
        if (g_audio.Ready()) {
            const FeSettings& as = g_menu.Settings();
            g_audio.SetVolumes(as.masterVol, as.sfxVol);
            if (int ui = g_menu.TakeUiEvent()) g_audio.Ui(ui);
            struct Pending { float t; const char* gun; int kind; };
            static std::vector<Pending> s_pending;
            static float s_stepDist = 0.0f, s_prevX = 0, s_prevY = 0, s_lastFlash = 0.0f;
            static bool s_wasAir = false, s_havePrev = false;
            if (cjReady && !g_menu.IsOpen() && !g_devPaused) {
                const char* gid = g_cj.GunId();
                for (int n = g_cj.TakeShotEvents(); n > 0; n--) g_audio.Gun(gid, 0);
                for (int n = g_cj.TakeDryEvents(); n > 0; n--) g_audio.Gun(gid, 1);
                for (int n = g_cj.TakeReloadEvents(); n > 0; n--) {
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
                    if (s_stepDist >= 1.25f) { s_stepDist = 0.0f; g_audio.Step(BANK_FEET_GENERIC, 0.5f); }
                }
                if (s_wasAir && !air) g_audio.Step(BANK_FEET_GENERIC, 0.8f);   // landing
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
            hd.showHud = hs.showHud;
            hd.radarOn = hs.radarMode == 0;
            hd.crosshairDot = hs.crosshairDot; hd.hitMarker = hs.hitMarker; hd.damageFlash = hs.damageFlash;
            hd.health = g_cj.Health(); hd.maxHealth = g_cj.MaxHealth(); hd.armor = g_cj.Armor();
            hd.breath = g_cj.Breath(); hd.showBreath = g_cj.ShowBreath();
            hd.damageFlashAmount = g_cj.DamageFlash();
            static int s_money = getenv("GTA_MONEY") ? atoi(getenv("GTA_MONEY")) : 0;
            static int s_wanted = getenv("GTA_WANTED") ? atoi(getenv("GTA_WANTED")) : 0;
            hd.money = s_money; hd.wanted = s_wanted;
            hd.hour = (int)clockHour; hd.minute = (int)((clockHour - floorf(clockHour)) * 60.0f);
            hd.armed = g_cj.GunIndex() >= 0;
            hd.weaponIcon = g_cj.GunIcon();
            hd.ammoClip = g_cj.GunAmmo(); hd.ammoTotal = g_cj.GunTotalAmmo(); hd.clipSize = g_cj.GunClipSize();
            hd.reloading = g_cj.Reloading();
            hd.weaponName = g_cj.GunName();
            hd.aiming = g_cj.Aiming();
            hd.spread = std::min(1.0f, sqrtf(g_cj.VelX() * g_cj.VelX() + g_cj.VelY() * g_cj.VelY()) / 6.0f);
            hd.firstPerson = g_playerCam.FirstPerson(hs.cameraStyle);
            hd.x = playerX; hd.y = playerY; hd.z = playerZ - EYE_HEIGHT;
            hd.camYaw = yaw; hd.heading = g_cj.Heading();
            hd.zone = g_hud.ZoneAt(hd.x, hd.y, hd.z);
            g_hud.Draw(g_renderer, hd);
        }

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

    if (playMode) ShowCursor(TRUE);

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
