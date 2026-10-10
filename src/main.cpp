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
            }
        }
        LocalFree(argv);
    }

    g_log = fopen(logPath.c_str(), "w");
    { char cwd[MAX_PATH] = {}; GetCurrentDirectoryA(sizeof(cwd), cwd);
      Log("gtasa_cpp starting (cwd=%s) play=%d grove=%d housetest=%d",
          cwd, playMode, groveTest, houseTest); }

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
            {-1500, -5680, 10,  0,0,1,  0,0},
            { 6500, -5680, 10,  0,0,1,  1,0},
            { 6500,  2320, 10,  0,0,1,  1,1},
            {-1500,  2320, 10,  0,0,1,  0,1},
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
        Log("PLAY MODE: Grove Street from real game data (LAe2.ipl + LAe2.ide)");
        // LOD instance name -> HD model name (verified against LAe2.ide).
        auto lodToHd = [](const std::string& lod) -> std::string {
            if (lod == "lod1carlshou1_lae") return "carlshou1_lae2"; // CJ's house
            if (lod == "lod1swetho1_lae")   return "sweetshou1_lae2"; // Sweet's house
            if (lod == "lodrydhou_lae2")    return "rydhou01_lae2";   // Ryder's house
            if (lod == "lod1rydkyr1_lae")   return "rydhou01_lae2";   // Ryder's 2nd bldg
            if (lod == "lodriverbridge3")   return "riverbridge3_lae2"; // verified in IMG
            if (lod == "lodpmedhos3_lae")   return "compmedhos3_lae";   // verified in IMG
            if (lod == "lodpfukhouse3")     return "compfukhouse3";     // verified in IMG
            if (lod == "lodlbd3")           return "billbd3";           // verified in IMG
            if (lod.compare(0, 3, "lod") == 0) return lod.substr(3);  // generic strip
            return lod;
        };
        // Roads / grass / bridges / drains / tags are visual, not solid.
        auto isSolid = [](const std::string& hd) -> bool {
            if (hd.find("road") != std::string::npos)     return false;
            if (hd.find("lnd") != std::string::npos)      return false;
            if (hd.find("hubgrass") != std::string::npos) return false;
            if (hd.find("bridge") != std::string::npos)   return false;
            if (hd.find("drai") != std::string::npos)     return false;
            if (hd.find("tag_") != std::string::npos)     return false;
            if (hd.find("lbd") != std::string::npos)      return false;
            return true;
        };
        // IDE model -> TXD map (definitive texture source).
        std::unordered_map<std::string, std::string> ideTxd;
        {
            std::vector<IdeObject> objs; std::unordered_map<int,size_t> byId;
            const char* ideFiles[] = { "data/maps/LA/LAe2.ide", "data/maps/LA/LAe.ide", "data/maps/LA/LAxref.ide" };
            for (const char* ide : ideFiles) {
                objs.clear(); byId.clear();
                if (IdeLoader::Load(FindGameFile(ide), objs, byId)) {
                    for (auto& o : objs) ideTxd[o.modelName] = o.txdName;
                    Log("PLAY: IDE %s: %u entries", ide, (unsigned)objs.size());
                } else Log("PLAY: IDE load FAILED: %s", ide);
            }
        }
        // IPL instances in the Grove Street box (real game placements).
        std::vector<IplInstance> insts;
        if (!IplLoader::Load(FindGameFile("data/maps/LA/LAe2.ipl"), insts))
            Log("PLAY: IPL load FAILED");
        Log("PLAY: %u IPL instances total", (unsigned)insts.size());
        auto inGroveBox = [](const IplInstance& in) {
            return in.interior == 0 &&
                   in.x >= 2400 && in.x <= 2600 &&
                   in.y >= -1800 && in.y <= -1550;
        };
        // Pass 1: collect road points (street-level reference).
        for (auto& in : insts) {
            if (!inGroveBox(in)) continue;
            std::string hd = lodToHd(in.modelName);
            if (hd.find("road") != std::string::npos && in.z < 16.5f)
                roadPts.push_back(std::make_tuple(in.x, in.y, in.z)); // surface streets only
        }
        Log("PLAY: %u road reference points", (unsigned)roadPts.size());
        // Pass 2: load everything at its real position/rotation.
        int loaded = 0, skipped = 0;
        for (auto& in : insts) {
            if (!inGroveBox(in)) continue;
            std::string hd = lodToHd(in.modelName);
            std::string dffBase = hd;
            bool usedLod = false;
            if (!img.HasFile(hd + ".dff")) {
                if (img.HasFile(in.modelName + ".dff")) { dffBase = in.modelName; usedLod = true; } // LOD fallback
                else { Log("  SKIP: no DFF for %s", in.modelName.c_str()); skipped++; continue; }
            }
            std::string txd;
            auto ti = ideTxd.find(dffBase);
            if (ti != ideTxd.end()) txd = ti->second;
            else {
                auto tl = ideTxd.find(in.modelName);
                txd = (tl != ideTxd.end()) ? tl->second : "laeast2_lod";
            }
            bool solid = isSolid(dffBase);
            // Local street level = nearest road z (real terrain undulates).
            float groundZ = 12.0f, best = 1e30f;
            for (auto& rp : roadPts) {
                float dx = in.x - std::get<0>(rp), dy = in.y - std::get<1>(rp);
                float d2 = dx*dx + dy*dy;
                if (d2 < best) { best = d2; groundZ = std::get<2>(rp); }
            }
            if (loadModelEx(dffBase, txd, in.x, in.y, in.z,
                            in.qx, in.qy, in.qz, in.qw,
                            solid, groundZ, mapObjects)) {
                Log("  INST: lod=%s dff=%s%s txd=%s solid=%d", in.modelName.c_str(),
                    dffBase.c_str(), usedLod ? " [LOD-FALLBACK]" : "", txd.c_str(), solid ? 1 : 0);
                loaded++;
                if (solid) solidCount++;
                // Doors share their house's transform (modeled at house origin).
                if (in.modelName == "lod1swetho1_lae")
                    loadModelEx("sweetsdoor_lae2", "contachou1_lae2",
                                in.x, in.y, groundZ,
                                in.qx, in.qy, in.qz, in.qw, false, 0.0f, mapObjects);
                if (in.modelName == "lodcjsaveg")
                    loadModelEx("cjgaragedoor", "contachou1_lae2",
                                in.x, in.y, groundZ,
                                in.qx, in.qy, in.qz, in.qw, false, 0.0f, mapObjects);
            } else skipped++;
        }
        Log("PLAY: %d loaded (%d solid), %d skipped", loaded, solidCount, skipped);

        // ---- Grove Street decoration: lampposts, trees, trash cans ----
        // Q: "you can use every model not just those" - make it feel like a real street.
        // All from Q's own game files (IDE-verified). Non-IPL, hand-placed.
        {
            struct Deco { const char* dff; const char* txd; float x, y, z; bool solid; };
            Deco decos[] = {
                // Lampposts along both sidewalks (TXD: dynsigns)
                {"lamppost1", "dynsigns", 2492.0f, -1760.0f, 13.0f, true},
                {"lamppost1", "dynsigns", 2508.0f, -1760.0f, 13.0f, true},
                {"lamppost1", "dynsigns", 2492.0f, -1720.0f, 13.0f, true},
                {"lamppost1", "dynsigns", 2508.0f, -1720.0f, 13.0f, true},
                {"lamppost1", "dynsigns", 2492.0f, -1680.0f, 13.0f, true},
                {"lamppost1", "dynsigns", 2508.0f, -1680.0f, 13.0f, true},
                {"lamppost1", "dynsigns", 2492.0f, -1640.0f, 13.0f, true},
                {"lamppost1", "dynsigns", 2508.0f, -1640.0f, 13.0f, true},
                {"lamppost1", "dynsigns", 2492.0f, -1600.0f, 13.0f, true},
                {"lamppost1", "dynsigns", 2508.0f, -1600.0f, 13.0f, true},
                // Trees in front yards (TXD: tree2/tree1)
                {"Cedar1_hi", "tree2", 2475.0f, -1740.0f, 13.0f, true},
                {"Cedar1_hi", "tree2", 2525.0f, -1740.0f, 13.0f, true},
                {"Elmtreegrn_hi", "tree1", 2475.0f, -1700.0f, 13.0f, true},
                {"Elmtreegrn_hi", "tree1", 2525.0f, -1700.0f, 13.0f, true},
                {"Cedar1_hi", "tree2", 2475.0f, -1660.0f, 13.0f, true},
                {"Cedar1_hi", "tree2", 2525.0f, -1660.0f, 13.0f, true},
                {"Elmtreegrn_hi", "tree1", 2475.0f, -1620.0f, 13.0f, true},
                {"Elmtreegrn_hi", "tree1", 2525.0f, -1620.0f, 13.0f, true},
                // Trash cans near houses (TXD: dyn_trash)
                {"trashcan", "dyn_trash", 2488.0f, -1690.0f, 13.0f, true},
                {"trashcan", "dyn_trash", 2512.0f, -1690.0f, 13.0f, true},
                {"trashcan", "dyn_trash", 2488.0f, -1650.0f, 13.0f, true},
                {"trashcan", "dyn_trash", 2512.0f, -1650.0f, 13.0f, true},
            };
            int decoLoaded = 0, decoSkipped = 0;
            for (auto& d : decos) {
                // Identity quaternion (no rotation)
                if (loadModelEx(d.dff, d.txd, d.x, d.y, d.z,
                                0.0f, 0.0f, 0.0f, 1.0f,
                                d.solid, 0.0f, mapObjects)) {
                    Log("  DECO: dff=%s txd=%s at (%.1f, %.1f)", d.dff, d.txd, d.x, d.y);
                    decoLoaded++;
                    if (d.solid) solidCount++;
                } else {
                    Log("  DECO SKIP: no DFF for %s", d.dff);
                    decoSkipped++;
                }
            }
            Log("PLAY: decoration: %d loaded, %d skipped", decoLoaded, decoSkipped);
        }
        // Player starts on Grove Street, looking north toward CJ's house.
        playerX = 2505.0f; playerY = -1710.0f; playerZ = GROUND_Z + EYE_HEIGHT;
        yaw = 1.5708f; pitch = 0.0f;
        // ---- Spawn safety: never start inside a solid AABB ----
        {
            auto spawnBlocked = [&](float px, float py) -> const char* {
                float fz = playerZ - EYE_HEIGHT;
                for (auto& o : mapObjects) {
                    if (!o.solid) continue;
                    if (px + PLAYER_RADIUS < o.cMinX || px - PLAYER_RADIUS > o.cMaxX) continue;
                    if (py + PLAYER_RADIUS < o.cMinY || py - PLAYER_RADIUS > o.cMaxY) continue;
                    if (fz + PLAYER_HEIGHT < o.cMinZ || fz > o.cMaxZ) continue;
                    return o.name.c_str();
                }
                return nullptr;
            };
            const char* insideName = spawnBlocked(playerX, playerY);
            if (insideName) {
                Log("SPAWN BLOCKED: (%.1f, %.1f) inside solid '%s' - searching clear spot",
                    playerX, playerY, insideName);
                bool found = false;
                for (float r = 2.0f; r <= 60.0f && !found; r += 2.0f)
                    for (int a = 0; a < 16 && !found; a++) {
                        float ang = a * 6.2831853f / 16.0f;
                        float cx = 2505.0f + cosf(ang) * r, cy = -1710.0f + sinf(ang) * r;
                        if (!spawnBlocked(cx, cy)) { playerX = cx; playerY = cy; found = true; }
                    }
                // Recompute ground height at the new spot.
                {
                    float bd = 1e30f;
                    for (auto& rp : roadPts) {
                        float dx = playerX - std::get<0>(rp), dy = playerY - std::get<1>(rp);
                        float d2 = dx * dx + dy * dy;
                        if (d2 < bd) { bd = d2; playerZ = std::get<2>(rp) + EYE_HEIGHT; }
                    }
                }
                Log("SPAWN: relocated to (%.1f, %.1f, %.1f)%s",
                    playerX, playerY, playerZ, found ? "" : " [NO CLEAR SPOT]");
            } else {
                Log("SPAWN: (%.1f, %.1f, %.1f) clear of solid AABBs", playerX, playerY, playerZ);
            }
        }
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
    bool mouseLook = playMode && !scripted;
    float scriptedTime = 0.0f;
    int lastScriptedSeg = -1;
    bool takeScriptedShot = false;
    int scriptedShotIdx = 0;
    if (scripted) Log("SCRIPTED MODE: auto camera path, shots every 4s");
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
            for (auto* mesh : obj.meshes)
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
            char shotName[64];
            snprintf(shotName, sizeof(shotName), "scripted_%d.bmp", scriptedShotIdx);
            g_renderer.SaveScreenshot(shotName);
            Log("SCRIPTED shot %d at (%.1f, %.1f, %.1f) yaw=%.1f",
                scriptedShotIdx, playerX, playerY, playerZ, yaw * 57.2958f);
        }

        frame++;
        if (maxFrames > 0 && frame >= maxFrames) break;
    }

    if (playMode) ShowCursor(TRUE);

    if (!shotPath.empty()) {
        g_renderer.SaveScreenshot(shotPath.c_str());
        Log("Screenshot saved to %s", shotPath.c_str());
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
