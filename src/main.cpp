// gtasa_cpp.exe - PLAYABLE BUILD
// Grove Street with GTA SA-style controls (WASD + mouse), debug overlay.
//
// Modes:
//   --play        Playable Grove Street (WASD + mouse, debug info)
//   --grove       Static Grove Street scene (original)
//   --housetest   3-house test (original)
//   (no flag)     M4 map loading (original)
//
// Play controls:
//   W/A/S/D - move (camera-relative)
//   Mouse   - look around
//   Shift   - run (faster)
//   Space   - jump
//   ESC     - quit

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include "D3DRenderer.h"
#include "DffLoader.h"
#include "TxdLoader.h"
#include "ImgLoader.h"
#include "IdeLoader.h"
#include "IplLoader.h"

static D3DRenderer g_renderer;
static bool g_running = true;
static HWND g_hwnd = nullptr;
static const wchar_t* WINDOW_CLASS = L"GTASACppWindow";
static const wchar_t* WINDOW_TITLE = L"GTA SA C++ (Playable)";

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
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE)
            g_running = false;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static std::string GameDir() {
    const char* env = getenv("GTA_SA_DIR");
    if (env && *env) return std::string(env);
    return "C:\\Users\\fufid\\Documents\\Decomps\\gta-sa";
}
static std::string FindGameFile(const std::string& rel) {
    return GameDir() + "\\" + rel;
}
static void WideToUtf8(LPCWSTR w, char* out, int outSize) {
    WideCharToMultiByte(CP_ACP, 0, w, -1, out, outSize - 1, nullptr, nullptr);
    out[outSize - 1] = '\0';
}

static D3DMATRIX QuatToD3DMatrix(float qx, float qy, float qz, float qw,
                                 float tx, float ty, float tz) {
    D3DMATRIX m;
    float xx = qx*qx, yy = qy*qy, zz = qz*qz;
    float xy = qx*qy, xz = qx*qz, yz = qy*qz;
    float wx = qw*qx, wy = qw*qy, wz = qw*qz;
    m.m[0][0] = 1-2*(yy+zz); m.m[0][1] = 2*(xy-wz);    m.m[0][2] = 2*(xz+wy);    m.m[0][3] = 0;
    m.m[1][0] = 2*(xy+wz);   m.m[1][1] = 1-2*(xx+zz);  m.m[1][2] = 2*(yz-wx);    m.m[1][3] = 0;
    m.m[2][0] = 2*(xz-wy);   m.m[2][1] = 2*(yz+wx);    m.m[2][2] = 1-2*(xx+yy);  m.m[2][3] = 0;
    m.m[3][0] = tx;          m.m[3][1] = ty;           m.m[3][2] = tz;           m.m[3][3] = 1;
    return m;
}

struct MapObject {
    std::vector<D3DRenderMesh*> meshes;
    D3DMATRIX worldMatrix;
    // World-space collision AABB (from DFF vertex bounds).
    float cMinX = 0, cMinY = 0, cMinZ = 0;
    float cMaxX = 0, cMaxY = 0, cMaxZ = 0;
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
    int vcount = 0;
    for (const char* p = text; *p && vcount < 16380; p++) {
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
    if (vcount > 0) {
        dev->SetFVF(DEBUG_FVF);
        dev->SetTexture(0, nullptr);
        // Disable depth test for overlay
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vcount / 3, verts, sizeof(DebugVertex));
        dev->SetRenderState(D3DRS_ZENABLE, TRUE);
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    const int WIDTH = 1280;
    const int HEIGHT = 720;

    int maxFrames = 0;
    std::string shotPath;
    std::string logPath = "gtasa_cpp.log";
    bool houseTest = false;
    bool groveTest = false;
    bool playMode = false;
    bool scripted = false;
    bool flyover = false;
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
            } else if (a == "--log" && i + 1 < argc) {
                char p[512] = {};
                WideToUtf8(argv[i + 1], p, sizeof(p));
                logPath = p; i++;
            } else if (a == "--housetest") {
                houseTest = true;
            } else if (a == "--grove") {
                groveTest = true;
            } else if (a == "--play") {
                playMode = true;
            } else if (a == "--scripted") {
                scripted = true;
                playMode = true;  // scripted implies play mode
            } else if (a == "--flyover") {
                flyover = true;
                playMode = true;  // flyover implies play mode
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
    }

    { char cwd[MAX_PATH] = {}; GetCurrentDirectoryA(sizeof(cwd), cwd);
      Log("gtasa_cpp starting (cwd=%s) play=%d grove=%d housetest=%d shotDir=%s",
          cwd, playMode, groveTest, houseTest, shotDir); }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = WINDOW_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, WINDOW_CLASS, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT,
        nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) { Log("CreateWindow failed"); return 1; }

    if (!g_renderer.Init(g_hwnd, WIDTH, HEIGHT)) {
        Log("D3DRenderer::Init failed"); return 1;
    }
    Log("D3D9 initialized: %s", g_renderer.GetAdapterDesc());

    ImgLoader img;
    std::string imgPath = FindGameFile("models\\gta3.img");
    if (!img.Open(imgPath)) { Log("Failed to open %s", imgPath.c_str()); return 1; }
    Log("IMG opened: %u entries", (unsigned)img.EntryCount());

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
            txd.width, txd.height, txd.d3dFormat, txd.mip0.data(), (unsigned)txd.mip0.size());
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
                dm.indices.data(), (uint32_t)dm.indices.size());
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
    int solidCount = 0;

    // ---- Ground plane (grass-colored safety net under the real geometry) ----
    auto buildGround = [&]() -> D3DRenderMesh* {
        // 8000x8000 quad centered on Grove Street (2500,-1680) at z=10,
        // below the lowest road (~11.1). Real roads/land/houses sit on top.
        MeshVertex verts[4] = {
            {-1500, -5680, 12,  0,0,1,  0,0},
            { 6500, -5680, 12,  0,0,1,  1,0},
            { 6500,  2320, 12,  0,0,1,  1,1},
            {-1500,  2320, 12,  0,0,1,  0,1},
        };
        uint16_t idx[6] = {0,1,2, 0,2,3};
        // Tint via vertex color? Our FVF has no color. Use untextured (white) for now.
        // Instead, we'll rely on lighting... Actually simplest: leave untextured,
        // it renders white. Better: create a 1x1 green texture.
        D3DRenderMesh* m = g_renderer.CreateMesh(verts, 4, idx, 6);
        return m;
    };

    // Green 1x1 texture for ground
    IDirect3DTexture9* groundTex = nullptr;
    {
        // Create a 4x4 green texture manually via D3D
        IDirect3DDevice9* dev = g_renderer.GetDevice();
        if (dev) {
            IDirect3DTexture9* t = nullptr;
            if (SUCCEEDED(dev->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &t, nullptr))) {
                D3DLOCKED_RECT lr = {};
                if (SUCCEEDED(t->LockRect(0, &lr, nullptr, 0))) {
                    uint32_t* p = (uint32_t*)lr.pBits;
                    for (int i = 0; i < 16; i++) p[i] = 0xFF4A7C3A; // grass green
                    t->UnlockRect(0);
                    groundTex = t;
                }
            }
        }
    }

    D3DRenderMesh* groundMesh = buildGround();
    if (groundMesh) groundMesh->texture = groundTex;

    // ---- Player state (first-person, real human scale) ----
    float playerX = 2505.0f, playerY = -1710.0f, playerZ = 13.7f; // eye
    float yaw = 1.5708f;  // radians, facing north (+Y) toward CJ's house
    float pitch = 0.0f;   // radians, positive = look up
    float velZ = 0.0f;    // vertical velocity for jumping
    bool onGround = true;
    const float EYE_HEIGHT = 1.7f;   // CJ eye height in meters
    const float GROUND_Z = 12.0f;    // nominal Ganton street level (feet)
    const float WALK_SPEED = 5.0f;
    const float RUN_SPEED = 10.0f;
    const float JUMP_VEL = 8.5f;     // ~1.6 m jump apex
    const float GRAVITY = 22.0f;
    const float PLAYER_RADIUS = 0.5f;
    const float PLAYER_HEIGHT = 1.8f;

    if (playMode) {
        Log("PLAY MODE: Grove Street cul-de-sac ONLY (4 houses, HD+LOD streaming)");
        Log("PLAY: Rockstar LOD system: HD within lodDist, LOD beyond. No collision (Q request).");

        // The 4 Grove Street cul-de-sac houses. Verified against LAe2.ide:
        //   HD id, HD model, HD TXD, HD drawdist | LOD model, LOD TXD | IPL position + quaternion
        // CJ's house:   17697 carlshou1_LAe2 / contachou1_lae2 / 60m
        // Sweet's:      17698 sweetshou1_LAe2 / contachou1_lae2 / 70m
        // Ryder's:      17573 rydhou01_LAe2   / contachou1_lae2 / 60m
        // Neighbor:     3649 ganghous01_LAx   / ganghouse1_lax  / 80m (north side)
        struct CulHouse {
            const char* name;      // display name
            const char* hdDff;     // HD model (no extension)
            const char* hdTxd;     // HD texture dictionary
            float hdDist;          // HD draw distance from IDE = LOD switch threshold
            const char* lodDff;    // LOD model (no extension)
            const char* lodTxd;    // LOD texture dictionary
            float x, y, z;         // IPL placement (real game coordinates)
            float qx, qy, qz, qw;  // IPL quaternion (real game rotation)
        };
        CulHouse houses[] = {
            // CJ's (Johnson) house - north end of cul-de-sac, faces south
            {"CJ_HOUSE", "carlshou1_LAe2", "contachou1_lae2", 60.0f,
             "LOD1carlshou1_LAe", "laeast2_lod",
             2494.265625f, -1696.210938f, 17.0546875f,
             0.0f, 0.0f, -1.0f, -4.371138829e-008f},
            // Sweet's house - east side
            {"SWEET_HOUSE", "sweetshou1_LAe2", "contachou1_lae2", 70.0f,
             "LOD1swetho1_LAe", "laeast2_lod",
             2529.890625f, -1677.664063f, 16.7265625f,
             0.0f, 0.0f, 0.0f, 1.0f},
            // Ryder's house - west side
            {"RYDER_HOUSE", "rydhou01_LAe2", "contachou1_lae2", 60.0f,
             "LODrydhou_LAe2", "laeast2_lod",
             2457.835938f, -1695.9375f, 14.2890625f,
             0.0f, 0.0f, 0.7071068287f, 0.7071067095f},
            // Neighbor house (ganghous01_LAx) - north side, real IPL placement
            // HD: 3649 ganghous01_LAx / ganghouse1_lax / 80m (LAxref.ide)
            {"NEIGHBOR_HOUSE", "ganghous01_LAx", "ganghouse1_lax", 80.0f,
             "LODganghous01_LAx", "gangholod1_lax",
             2517.476563f, -1644.695313f, 15.1953125f,
             0.0f, 0.0f, -0.3826832771f, 0.9238796234f},
        };

        // Helper: load a DFF+TXD into render meshes, with texture dimension logging.
        // Returns meshes via out param. Logs DFF stats and texture sizes.
        auto loadMeshes = [&](const std::string& dffBase, const std::string& txdName,
                              const char* tag,
                              std::vector<D3DRenderMesh*>& outMeshes) -> bool {
            std::string df = dffBase + ".dff";
            if (!img.HasFile(df)) { Log("  %s SKIP: DFF not in IMG: %s", tag, df.c_str()); return false; }
            std::vector<uint8_t> dffData = img.Extract(df);
            if (dffData.empty()) { Log("  %s FAIL: extract %s", tag, df.c_str()); return false; }
            DffModel dff = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
            if (!dff.valid) { Log("  %s FAIL: parse %s", tag, df.c_str()); return false; }
            size_t totalVerts = 0, totalTris = 0;
            for (auto& dm : dff.meshes) { totalVerts += dm.vertices.size(); totalTris += dm.indices.size() / 3; }
            ensureTxd(txdName);
            // Log texture dimensions for this TXD (detect low-res).
            Log("  %s: dff=%s verts=%u tris=%u txd=%s", tag, df.c_str(),
                (unsigned)totalVerts, (unsigned)totalTris, txdName.c_str());
            for (auto& dm : dff.meshes) {
                const MeshVertex* verts = reinterpret_cast<const MeshVertex*>(dm.vertices.data());
                D3DRenderMesh* rm = g_renderer.CreateMesh(
                    verts, (unsigned)dm.vertices.size(),
                    dm.indices.data(), (unsigned)dm.indices.size());
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
            obj.solid = false;  // Q: no collision for now - walk through walls beats invisible walls
            float placeZ = GROUND_Z; // sink base to common cul-de-sac ground level
            obj.worldMatrix = QuatToD3DMatrix(h.qx, h.qy, h.qz, h.qw, h.x, h.y, placeZ);
            obj.objX = h.x; obj.objY = h.y; obj.objZ = placeZ;
            obj.lodDist = h.hdDist;
            // Load HD meshes.
            bool hdOk = loadMeshes(h.hdDff, h.hdTxd, h.name, obj.meshes);
            // Load LOD meshes.
            bool lodOk = loadMeshes(h.lodDff, h.lodTxd, h.name, obj.lodMeshes);
            obj.hasLod = lodOk;
            if (hdOk) {
                mapObjects.push_back(std::move(obj));
                loaded++;
                Log("  HOUSE OK: %s HD=%s LOD=%s %s at (%.2f, %.2f, %.2f->%.2f) lodDist=%.0f",
                    h.name, h.hdDff, h.lodDff, lodOk ? "(LOD loaded)" : "(NO LOD)",
                    h.x, h.y, h.z, placeZ, h.hdDist);
            } else {
                Log("  HOUSE FAIL: %s (HD missing)", h.name);
            }
        }
        Log("PLAY: %d/4 cul-de-sac houses loaded (collision OFF, LOD streaming ON)", loaded);

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
        Log("PLAY: %d/3 Grove Street roads loaded (collision OFF)", roadsLoaded);

        // Player starts on the cul-de-sac, looking north toward CJ's house.
        // Cul-de-sac center ~ (2490, -1685). Start south of houses, clear of everything.
        playerX = 2490.0f; playerY = -1660.0f; playerZ = GROUND_Z + EYE_HEIGHT;
        yaw = 3.14159f; pitch = 0.0f;  // face north (toward CJ's at y=-1696)
        Log("SPAWN: (%.1f, %.1f, %.1f) facing north to CJ's house", playerX, playerY, playerZ);
        // Hide cursor for mouse look
        ShowCursor(FALSE);
        // Center mouse
        RECT rc; GetClientRect(g_hwnd, &rc);
        POINT c = {(rc.right-rc.left)/2, (rc.bottom-rc.top)/2};
        ClientToScreen(g_hwnd, &c);
        SetCursorPos(c.x, c.y);
    } else if (houseTest) {
        Log("HOUSE TEST MODE");
        struct HD { const char* dff; const char* txd; float x, y, z; };
        HD houses[] = {
            {"bdupshouse_lae.dff",    "bdupshouse_lae", 2480.0f, -1650.0f, 0.0f},
            {"santahouse02_law2.dff", "bev_law2",       2510.0f, -1650.0f, 0.0f},
            {"cehillhouse04.dff",     "lahillshilhse",  2540.0f, -1650.0f, 0.0f},
        };
        for (auto& h : houses) loadHouse(h.dff, h.txd, h.x, h.y, h.z, 0.0f, mapObjects);
        D3DMATRIX view = MatrixLookAt(2510, -1710, 25, 2510, -1650, 8, 0, 0, 1);
        D3DMATRIX proj = MatrixPerspectiveFov(60.0f*3.14159f/180.0f, (float)WIDTH/(float)HEIGHT, 1.0f, 2000.0f);
        g_renderer.SetViewMatrix(view);
        g_renderer.SetProjMatrix(proj);
    } else if (groveTest) {
        Log("GROVE STREET MODE (static)");
        struct GH { const char* dff; const char* txd; float x, y, z, yaw; };
        GH houses[] = {
            {"bdupshouse_lae.dff",   "bdupshouse_lae", 2480.0f, -1635.0f, 0.0f, 3.14159f},
            {"compmedhos1_lae.dff",  "comedhos1_la",   2500.0f, -1635.0f, 0.0f, 3.14159f},
            {"compmedhos2_lae.dff",  "comedhos1_la",   2520.0f, -1635.0f, 0.0f, 3.14159f},
            {"ganghous01_lax.dff",   "ganghouse1_lax", 2540.0f, -1635.0f, 0.0f, 3.14159f},
            {"santahouse02_law2.dff", "bev_law2",      2490.0f, -1665.0f, 0.0f, 0.0f},
            {"compmedhos3_lae.dff",   "comedhos1_la",  2510.0f, -1665.0f, 0.0f, 0.0f},
            {"cehillhouse04.dff",     "lahillshilhse", 2530.0f, -1665.0f, 0.0f, 0.0f},
            {"ganghous02_lax.dff",    "ganghouse1_lax",2550.0f, -1665.0f, 0.0f, 0.0f},
        };
        for (auto& h : houses) loadHouse(h.dff, h.txd, h.x, h.y, h.z, h.yaw, mapObjects);
        D3DMATRIX view = MatrixLookAt(2460, -1650, 18, 2530, -1650, 6, 0, 0, 1);
        D3DMATRIX proj = MatrixPerspectiveFov(60.0f*3.14159f/180.0f, (float)WIDTH/(float)HEIGHT, 1.0f, 2000.0f);
        g_renderer.SetViewMatrix(view);
        g_renderer.SetProjMatrix(proj);
    } else {
        Log("M4 map mode: use --play, --grove, or --housetest");
    }

    IDirect3DDevice9* dev = g_renderer.GetDevice();
    D3DMATRIX proj = MatrixPerspectiveFov(70.0f*3.14159f/180.0f, (float)WIDTH/(float)HEIGHT, 0.5f, 3000.0f);
    g_renderer.SetProjMatrix(proj);

    Log("Entering render loop");
    int frame = 0;
    DWORD lastTick = GetTickCount();
    int fpsFrames = 0;
    float fps = 0.0f;
    char titleBuf[256];

    // Mouse look state
    bool mouseLook = playMode && !scripted && !flyover;
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

    MSG msg = {};
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_running = false; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!g_running) break;

        DWORD now = GetTickCount();
        float dt = (now - lastTick) / 1000.0f;
        if (dt > 0.1f) dt = 0.1f; // clamp
        lastTick = now;

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
                    fps, playerX, playerY, playerZ, yaw*57.3f, (unsigned)mapObjects.size());
                SetWindowTextA(g_hwnd, titleBuf);
            }
        }

        if (playMode) {
            // ---- Mouse look ----
            if (mouseLook) {
                POINT mp;
                GetCursorPos(&mp);
                RECT rc; GetClientRect(g_hwnd, &rc);
                POINT center = {(rc.right-rc.left)/2, (rc.bottom-rc.top)/2};
                ClientToScreen(g_hwnd, &center);
                int dx = mp.x - center.x;
                int dy = mp.y - center.y;
                if (dx != 0 || dy != 0) {
                    yaw   += dx * MOUSE_SENS;
                    pitch -= dy * MOUSE_SENS;
                    if (pitch >  1.5f) pitch =  1.5f;
                    if (pitch < -1.5f) pitch = -1.5f;
                    SetCursorPos(center.x, center.y);
                }
            }
            // Arrow keys as fallback look
            if (GetAsyncKeyState(VK_LEFT)  & 0x8000) yaw   -= 2.5f * dt;
            if (GetAsyncKeyState(VK_RIGHT) & 0x8000) yaw   += 2.5f * dt;
            if (GetAsyncKeyState(VK_UP)    & 0x8000) pitch += 2.0f * dt;
            if (GetAsyncKeyState(VK_DOWN)  & 0x8000) pitch -= 2.0f * dt;
            if (pitch >  1.5f) pitch =  1.5f;
            if (pitch < -1.5f) pitch = -1.5f;

            // ---- WASD movement (camera-relative, on XY plane) ----
            float speed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? RUN_SPEED : WALK_SPEED;
            float fwdX = cosf(yaw), fwdY = sinf(yaw);
            float rightX = -sinf(yaw), rightY = cosf(yaw);
            float mx = 0, my = 0;
            if (GetAsyncKeyState('W') & 0x8000) { mx += fwdX; my += fwdY; }
            if (GetAsyncKeyState('S') & 0x8000) { mx -= fwdX; my -= fwdY; }
            if (GetAsyncKeyState('D') & 0x8000) { mx += rightX; my += rightY; }
            if (GetAsyncKeyState('A') & 0x8000) { mx -= rightX; my -= rightY; }
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
            // ---- Player ground height = nearest road z ----
            float gz = 12.0f;
            {
                float bd = 1e30f;
                for (auto& rp : roadPts) {
                    float dx = playerX - std::get<0>(rp);
                    float dy = playerY - std::get<1>(rp);
                    float d2 = dx*dx + dy*dy;
                    if (d2 < bd) { bd = d2; gz = std::get<2>(rp); }
                }
            }
            // ---- AABB collision: slide along walls ----
            // Blocks only when ENTERING a box from outside; if the player is
            // already inside (bad spawn), movement is allowed so they can escape.
            auto boxHit = [&](float px, float py, const MapObject& o) -> bool {
                float fz = playerZ - EYE_HEIGHT;
                if (px + PLAYER_RADIUS < o.cMinX || px - PLAYER_RADIUS > o.cMaxX) return false;
                if (py + PLAYER_RADIUS < o.cMinY || py - PLAYER_RADIUS > o.cMaxY) return false;
                if (fz + PLAYER_HEIGHT < o.cMinZ || fz > o.cMaxZ) return false;
                return true;
            };
            auto hitsSolid = [&](float ox, float oy, float nx_, float ny_) -> bool {
                for (auto& o : mapObjects) {
                    if (!o.solid) continue;
                    if (boxHit(nx_, ny_, o) && !boxHit(ox, oy, o)) return true;
                }
                return false;
            };
            float nx = playerX + mx * speed * dt;
            float ny = playerY + my * speed * dt;
            if (!hitsSolid(playerX, playerY, nx, playerY)) playerX = nx;
            if (!hitsSolid(playerX, playerY, playerX, ny)) playerY = ny;

            // ---- Jump / gravity (feet rest on gz) ----
            if (onGround && (GetAsyncKeyState(VK_SPACE) & 0x8000)) {
                velZ = JUMP_VEL;
                onGround = false;
            }
            if (!onGround) {
                velZ -= GRAVITY * dt;
                playerZ += velZ * dt;
                if (playerZ <= gz + EYE_HEIGHT) {
                    playerZ = gz + EYE_HEIGHT; velZ = 0; onGround = true;
                }
            }

            // ---- Build view matrix from yaw/pitch ----
            float cp = cosf(pitch), sp = sinf(pitch);
            float tx = playerX + cosf(yaw) * cp;
            float ty = playerY + sinf(yaw) * cp;
            float tz = playerZ + sp;
            D3DMATRIX view = MatrixLookAt(playerX, playerY, playerZ, tx, ty, tz, 0, 0, 1);
            g_renderer.SetViewMatrix(view);
        }

        g_renderer.BeginFrame(0.4f, 0.6f, 0.9f);

        // Distance fog in play mode (hides the ground-plane edge).
        if (playMode) {
            float fogStart = 400.0f, fogEnd = 2500.0f;
            dev->SetRenderState(D3DRS_FOGENABLE, TRUE);
            dev->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
            dev->SetRenderState(D3DRS_FOGCOLOR, 0xFF6699E6);
            dev->SetRenderState(D3DRS_FOGSTART, *(DWORD*)&fogStart);
            dev->SetRenderState(D3DRS_FOGEND, *(DWORD*)&fogEnd);
        }

        // Ground plane (identity world matrix)
        if (groundMesh) {
            D3DMATRIX ident = QuatToD3DMatrix(0,0,0,1, 0,0,0);
            dev->SetTransform(D3DTS_WORLD, &ident);
            g_renderer.DrawMesh(groundMesh);
        }

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

        // ---- Debug overlay (play mode) ----
        if (playMode) {
            dev->SetRenderState(D3DRS_FOGENABLE, FALSE); // keep text unfogged
            char dbg[512];
            snprintf(dbg, sizeof(dbg),
                "FPS: %.0f\n"
                "Pos: %.1f, %.1f, %.1f\n"
                "Yaw: %.1f deg\n"
                "Objects: %u (%d solid)\n"
                "WASD move  Mouse look\n"
                "Shift run  Space jump\n"
                "Arrows look  ESC quit",
                fps, playerX, playerY, playerZ,
                yaw * 57.2958f, (unsigned)mapObjects.size(), solidCount);
            DrawDebugText(dev, dbg, 12.0f, 12.0f, 2.0f, 0xFFFFFF00); // yellow
        }

        g_renderer.EndFrame();

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

        frame++;
        if (maxFrames > 0 && frame >= maxFrames) break;
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
    Log("Done: %d frames, %u map objects", frame, (unsigned)mapObjects.size());
    if (g_log) fclose(g_log);

    for (auto& obj : mapObjects)
        for (auto* mesh : obj.meshes) g_renderer.DestroyMesh(mesh);
    if (groundMesh) g_renderer.DestroyMesh(groundMesh);
    if (groundTex) groundTex->Release();
    for (auto& kv : d3dTexCache)
        if (kv.second) g_renderer.DestroyTexture(kv.second);
    return 0;
}
