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
    std::vector<ColTriangle> colTris; // COL3 ground triangles (roads), world space (Agent 15)
    int solidCount = 0;

    // ---- Ground plane (grass-colored safety net under the real geometry) ----
    auto buildGround = [&]() -> D3DRenderMesh* {
        // 8000x8000 quad centered on Grove Street (2500,-1680) at z=9.0.
        // Below the lowest retail ground surface in the box (~9.13, lae2_landhub06
        // embankment), so retail ground DFFs render on top wherever they exist
        // and this quad only shows through in gaps with no retail ground.
        MeshVertex verts[4] = {
            {-1500, -5680, 9.0f,  0,0,1,  0,0},
            { 6500, -5680, 9.0f,  0,0,1,  1,0},
            { 6500,  2320, 9.0f,  0,0,1,  1,1},
            {-1500,  2320, 9.0f,  0,0,1,  0,1},
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
            {-hw,-hd,0,  0,0,1,  0,0}, {hw,-hd,0,  0,0,1,  1,0},
            {hw, hd,0,  0,0,1,  1,1},  {-hw,hd,0,  0,0,1,  0,1},
            {-hw,-hd,ht, 0,0,1,  0,0}, {hw,-hd,ht, 0,0,1,  1,0},
            {hw, hd,ht, 0,0,1,  1,1},  {-hw,hd,ht, 0,0,1,  0,1},
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

    // ---- Player state (first-person, real human scale) ----
    float playerX = 2505.0f, playerY = -1710.0f, playerZ = 13.7f; // eye
    float camPosX = 2505.0f, camPosY = -1710.0f, camPosZ = 13.7f; // camera world pos (sky dome follows)
    float yaw = 1.5708f;  // radians, facing north (+Y) toward CJ's house
    float pitch = 0.0f;   // radians, positive = look up
    float velZ = 0.0f;    // vertical velocity for jumping
    bool onGround = true;
    bool thirdPerson = false; // V toggles 1st/3rd person camera
    bool prevV = false;       // edge-detect for V key
    const float EYE_HEIGHT = 1.7f;   // CJ eye height in meters
    const float GROUND_Z = 9.0f;     // safety-net grass plane; below lowest retail ground (~9.13)
    const float WALK_SPEED = 5.0f;
    const float RUN_SPEED = 10.0f;
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
            obj.solid = false;  // attachColBoxes() below enables real COL collision when boxes exist
            float placeZ = h.z;      // RESTORED: raw IPL Z (Rockstar authored, grove_heights.txt)
            obj.worldMatrix = QuatToD3DMatrix(h.qx, h.qy, h.qz, h.qw, h.x, h.y, placeZ);
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
            obj.worldMatrix = QuatToD3DMatrix(h.qx, h.qy, h.qz, h.qw, h.x, h.y, placeZ);
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
            // ---- Player ground height: road tops via AABB, else grass ----
            // Tracks the surface under the feet as the player walks, so road
            // grades and curbs work. Replaces the fixed-gz + dead roadPts snap,
            // which left the player buried in raised roads.
            float curFeetZ = playerZ - EYE_HEIGHT;
            float gz = GetGroundHeight(playerX, playerY, curFeetZ);
            // Step up onto higher ground (curbs) / start falling off edges.
            if (onGround && gz > curFeetZ + 0.001f) {
                playerZ = gz + EYE_HEIGHT;  // GetGroundHeight capped the step
                curFeetZ = gz;
            } else if (onGround && gz < curFeetZ - 0.001f) {
                onGround = false;  // walked off an edge - start falling
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

            // ---- V toggles 1st/3rd person camera ----
            bool vDown = (GetAsyncKeyState('V') & 0x8000) != 0;
            if (vDown && !prevV) {
                thirdPerson = !thirdPerson;
                Log("CAMERA: %s", thirdPerson ? "THIRD-PERSON" : "FIRST-PERSON");
            }
            prevV = vDown;

            // ---- Build view matrix from yaw/pitch ----
            float cp = cosf(pitch), sp = sinf(pitch);
            float dirX = cosf(yaw) * cp, dirY = sinf(yaw) * cp, dirZ = sp;
            if (!thirdPerson) {
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
        }

        g_renderer.BeginFrame(0.4f, 0.6f, 0.9f);

        // Sky gradient dome + sun (M4). Drawn first, no depth.
        if (playMode) g_renderer.RenderSky(camPosX, camPosY, camPosZ);

        // Distance fog in play mode (hides the ground-plane edge).
        if (playMode) {
            float fogStart = 400.0f, fogEnd = 2500.0f;
            dev->SetRenderState(D3DRS_FOGENABLE, TRUE);
            dev->SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
            dev->SetRenderState(D3DRS_FOGCOLOR, 0xFF35A2E3);  // match sky horizon (53,162,227)
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

        // ---- CJ scale-reference mesh (third-person only) ----
        if (playMode && thirdPerson && playerMesh) {
            float feetZ = playerZ - EYE_HEIGHT;
            // Yaw rotation about Z so the box faces the view direction.
            float hy = yaw * 0.5f;
            D3DMATRIX pm = QuatToD3DMatrix(0, 0, sinf(hy), cosf(hy), playerX, playerY, feetZ);
            dev->SetTransform(D3DTS_WORLD, &pm);
            dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE); // double-sided box
            g_renderer.DrawMesh(playerMesh);
            dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
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
                "CAM: %s (V toggle)\n"
                "WASD move  Mouse look\n"
                "Shift run  Space jump\n"
                "Arrows look  ESC quit",
                fps, playerX, playerY, playerZ,
                yaw * 57.2958f, (unsigned)mapObjects.size(), solidCount,
                thirdPerson ? "3RD" : "1ST");
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
    if (playerMesh) g_renderer.DestroyMesh(playerMesh);
    if (playerTex) playerTex->Release();
    for (auto& kv : d3dTexCache)
        if (kv.second) g_renderer.DestroyTexture(kv.second);
    return 0;
}
