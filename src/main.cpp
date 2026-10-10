// gtasa_cpp.exe - standalone GTA San Andreas C++ rewrite entry point.
// Milestone 3: TXD texture loading. Loads models/generic/wheels.DFF
// (arrow.DFF is untextured) plus every models/*.txd, binds each mesh's
// texture, and renders the wheel set in a grid.
//
// Test args: --frames N  (quit after N frames, for automated runs)
//            --screenshot <bmp path>  (save backbuffer before exit)
//            --log <path>  (diagnostic log; default m3_test.log next to exe)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <cstdarg>
#include "D3DRenderer.h"
#include "DffLoader.h"
#include "TxdLoader.h"

static D3DRenderer g_renderer;
static bool g_running = true;
static const wchar_t* WINDOW_CLASS = L"GTASACppWindow";
static const wchar_t* WINDOW_TITLE = L"GTA SA C++ (M3: textures)";

static FILE* g_log = nullptr;
static void Log(const char* fmt, ...) {
    if (!g_log)
        return;
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

// Game files live next to the original install; the exe finds them via
// the GTA_SA_DIR env var or the decomp-time location.
static std::string GameDir() {
    const char* env = getenv("GTA_SA_DIR");
    if (env && *env)
        return std::string(env);
    return "C:\\Users\\fufid\\Documents\\Decomps\\gta-sa";
}

static std::string FindGameFile(const std::string& rel) {
    return GameDir() + "\\" + rel;
}

// Load every models/*.txd (skipping macOS "._" metadata files) into a
// lowercase-name -> texture map. First file wins on duplicates.
static std::unordered_map<std::string, TxdTexture> LoadAllTxds() {
    std::unordered_map<std::string, TxdTexture> map;
    std::string pattern = FindGameFile("models\\*.txd");
    WIN32_FIND_DATAA fd = {};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        Log("no TXD files found at %s", pattern.c_str());
        return map;
    }
    int files = 0, texTotal = 0;
    do {
        std::string name = fd.cFileName;
        if (name.rfind("._", 0) == 0)
            continue;  // AppleDouble metadata, not a TXD
        std::string path = FindGameFile(std::string("models\\") + name);
        std::vector<TxdTexture> texs = TxdLoader::Load(path);
        files++;
        for (auto& t : texs) {
            texTotal++;
            std::string key = TxdLoader::KeyOf(t.name);
            if (map.find(key) == map.end())
                map.emplace(key, std::move(t));
        }
        Log("txd %s: %u textures", name.c_str(), (unsigned)texs.size());
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    Log("loaded %d TXD files, %d textures (%u unique names)",
        files, texTotal, (unsigned)map.size());
    return map;
}

static void WideToUtf8(LPCWSTR w, char* out, int outSize) {
    WideCharToMultiByte(CP_ACP, 0, w, -1, out, outSize - 1, nullptr, nullptr);
    out[outSize - 1] = '\0';
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    const int WIDTH = 1280;
    const int HEIGHT = 720;

    // ---- test args ----
    int maxFrames = 0;          // 0 = run until closed
    std::string shotPath;
    std::string logPath = "m3_test.log";
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 1; i < argc; i++) {
            char arg[512] = {};
            WideToUtf8(argv[i], arg, sizeof(arg));
            std::string a = arg;
            if (a == "--frames" && i + 1 < argc) {
                char n[64] = {};
                WideToUtf8(argv[++i], n, sizeof(n));
                maxFrames = atoi(n);
            } else if (a == "--screenshot" && i + 1 < argc) {
                char p[512] = {};
                WideToUtf8(argv[++i], p, sizeof(p));
                shotPath = p;
            } else if (a == "--log" && i + 1 < argc) {
                char p[512] = {};
                WideToUtf8(argv[++i], p, sizeof(p));
                logPath = p;
            }
        }
        LocalFree(argv);
    }
    g_log = fopen(logPath.c_str(), "w");

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS;
    if (!RegisterClassExW(&wc))
        return 1;

    RECT rc = { 0, 0, WIDTH, HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(
        0, WINDOW_CLASS, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd)
        return 2;

    if (!g_renderer.Init(hwnd, WIDTH, HEIGHT)) {
        Log("FATAL: D3D9 init failed");
        MessageBoxW(hwnd, L"Failed to initialize D3D9", WINDOW_TITLE, MB_ICONERROR);
        return 3;
    }
    Log("D3D9 init OK (adapter: %s)", g_renderer.GetAdapterDesc());

    // Camera: wheels are small (r ~ 0.6); frame a grid of them.
    g_renderer.SetProjMatrix(MatrixPerspectiveFov(3.14159f / 4.0f,
        (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f));
    g_renderer.SetViewMatrix(MatrixLookAt(0, 5.5f, 12.5f, 0, 0, 0, 0, 1, 0));

    // ---- M3: load TXDs, then the textured test model ----
    auto txdMap = LoadAllTxds();

    std::vector<D3DRenderMesh*> meshes;
    std::string dffPath = FindGameFile("models\\generic\\wheels.DFF");
    DffModel model = DffLoader::Load(dffPath);
    Log("DFF %s: valid=%d meshes=%u", dffPath.c_str(),
        (int)model.valid, (unsigned)model.meshes.size());

    int texBound = 0, texMissing = 0, texFailed = 0;
    const int GRID_COLS = 7;
    const float GRID_GAP = 1.6f;
    for (size_t mi = 0; mi < model.meshes.size(); mi++) {
        auto& dm = model.meshes[mi];
        // Lay meshes out in a grid so every wheel is visible.
        float gx = (float)(mi % GRID_COLS) - (GRID_COLS - 1) * 0.5f;
        float gz = (float)(mi / GRID_COLS) - 2.5f;
        std::vector<MeshVertex> verts(dm.vertices.size());
        for (size_t i = 0; i < verts.size(); i++) {
            verts[i].x = dm.vertices[i].x + gx * GRID_GAP;
            verts[i].y = dm.vertices[i].y;
            verts[i].z = dm.vertices[i].z + gz * GRID_GAP;
            verts[i].nx = dm.vertices[i].nx;
            verts[i].ny = dm.vertices[i].ny;
            verts[i].nz = dm.vertices[i].nz;
            verts[i].u = dm.vertices[i].u;
            verts[i].v = dm.vertices[i].v;
        }
        D3DRenderMesh* rm = g_renderer.CreateMesh(
            verts.data(), (uint32_t)verts.size(),
            dm.indices.data(), (uint32_t)dm.indices.size());
        if (!rm) {
            Log("mesh %u: CreateMesh FAILED", (unsigned)mi);
            continue;
        }
        if (!dm.textureName.empty()) {
            std::string key = TxdLoader::KeyOf(dm.textureName);
            auto it = txdMap.find(key);
            if (it == txdMap.end()) {
                texMissing++;
                Log("mesh %u: texture '%s' NOT FOUND in TXDs",
                    (unsigned)mi, dm.textureName.c_str());
            } else {
                const TxdTexture& t = it->second;
                rm->texture = g_renderer.CreateTexture(
                    t.width, t.height, t.d3dFormat,
                    t.mip0.data(), (uint32_t)t.mip0.size());
                if (rm->texture) {
                    texBound++;
                    Log("mesh %u: texture '%s' bound (%ux%u fmt %d)",
                        (unsigned)mi, t.name.c_str(), t.width, t.height,
                        (int)t.d3dFormat);
                } else {
                    texFailed++;
                    Log("mesh %u: CreateTexture FAILED for '%s'",
                        (unsigned)mi, t.name.c_str());
                }
            }
        } else {
            Log("mesh %u: untextured (%u tris)", (unsigned)mi,
                (unsigned)(dm.indices.size() / 3));
        }
        meshes.push_back(rm);
    }
    Log("summary: meshes=%u textured=%d missing=%d createFailed=%d",
        (unsigned)meshes.size(), texBound, texMissing, texFailed);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    int frame = 0;
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!g_running)
            break;

        g_renderer.BeginFrame(0.1f, 0.1f, 0.35f);
        if (!meshes.empty()) {
            for (auto m : meshes)
                g_renderer.DrawMesh(m);
        } else {
            g_renderer.DrawTestTriangle();  // fallback if DFF failed to load
        }
        g_renderer.EndFrame();
        frame++;

        if (maxFrames > 0 && frame >= maxFrames) {
            Log("rendered %d frames", frame);
            if (!shotPath.empty()) {
                bool ok = g_renderer.SaveScreenshot(shotPath.c_str());
                Log("screenshot %s: %s", shotPath.c_str(), ok ? "OK" : "FAILED");
            }
            break;
        }
    }

    for (auto m : meshes)
        g_renderer.DestroyMesh(m);
    g_renderer.Shutdown();
    if (g_log) {
        Log("exit");
        fclose(g_log);
    }
    return 0;
}
