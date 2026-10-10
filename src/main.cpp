// gtasa_cpp.exe - Milestone 4: IMG archive + IDE/IPL map loading.
// Loads Grove Street area (LAe) from gta3.img, places objects with
// correct positions/rotations, renders textured map.
//
// Test args: --frames N  (quit after N frames)
//            --screenshot <bmp path>
//            --log <path>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <d3d9.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include "D3DRenderer.h"
#include "DffLoader.h"
#include "TxdLoader.h"
#include "ImgLoader.h"
#include "IdeLoader.h"
#include "IplLoader.h"

static D3DRenderer g_renderer;
static bool g_running = true;
static const wchar_t* WINDOW_CLASS = L"GTASACppWindow";
static const wchar_t* WINDOW_TITLE = L"GTA SA C++ (M4: map)";

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

static std::string GameDir() {
    const char* env = getenv("GTA_SA_DIR");
    if (env && *env)
        return std::string(env);
    return "C:\\Users\\fufid\\Documents\\Decomps\\gta-sa";
}

static std::string FindGameFile(const std::string& rel) {
    return GameDir() + "\\" + rel;
}

static void WideToUtf8(LPCWSTR w, char* out, int outSize) {
    WideCharToMultiByte(CP_ACP, 0, w, -1, out, outSize - 1, nullptr, nullptr);
    out[outSize - 1] = '\0';
}

// Convert quaternion to D3DMATRIX (D3D is row-major, left-handed).
// D3DMATRIX is a union with m[4][4]; we fill it row by row.
static D3DMATRIX QuatToD3DMatrix(float qx, float qy, float qz, float qw,
                                 float tx, float ty, float tz) {
    D3DMATRIX m;
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;

    m.m[0][0] = 1 - 2 * (yy + zz);  m.m[0][1] = 2 * (xy - wz);      m.m[0][2] = 2 * (xz + wy);      m.m[0][3] = 0;
    m.m[1][0] = 2 * (xy + wz);      m.m[1][1] = 1 - 2 * (xx + zz);  m.m[1][2] = 2 * (yz - wx);      m.m[1][3] = 0;
    m.m[2][0] = 2 * (xz - wy);      m.m[2][1] = 2 * (yz + wx);      m.m[2][2] = 1 - 2 * (xx + yy);  m.m[2][3] = 0;
    m.m[3][0] = tx;                 m.m[3][1] = ty;                 m.m[3][2] = tz;                 m.m[3][3] = 1;
    return m;
}

// A placed map object.
struct MapObject {
    std::vector<D3DRenderMesh*> meshes;
    D3DMATRIX worldMatrix;
};

struct CachedModel {
    DffModel dff;
    std::string txdName;
    bool loaded = false;
};

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    const int WIDTH = 1280;
    const int HEIGHT = 720;

    int maxFrames = 0;
    std::string shotPath;
    std::string logPath = "m4_test.log";
    bool houseTest = false;
    bool groveTest = false;
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
                maxFrames = atoi(n);
                i++;
            } else if (a == "--screenshot" && i + 1 < argc) {
                char p[512] = {};
                WideToUtf8(argv[i + 1], p, sizeof(p));
                shotPath = p;
                i++;
            } else if (a == "--log" && i + 1 < argc) {
                char p[512] = {};
                WideToUtf8(argv[i + 1], p, sizeof(p));
                logPath = p;
                i++;
            } else if (a == "--housetest") {
                houseTest = true;
            } else if (a == "--grove") {
                groveTest = true;
            }
        }
        LocalFree(argv);
    }

    g_log = fopen(logPath.c_str(), "w");
    { char cwd[MAX_PATH] = {}; GetCurrentDirectoryA(sizeof(cwd), cwd); Log("gtasa_cpp M4 starting (cwd=%s)", cwd); }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = WINDOW_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, WINDOW_CLASS, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
        Log("CreateWindow failed");
        return 1;
    }

    if (!g_renderer.Init(hwnd, WIDTH, HEIGHT)) {
        Log("D3DRenderer::Init failed");
        return 1;
    }
    Log("D3D9 initialized: %s", g_renderer.GetAdapterDesc());

    // ---- Open IMG ----
    ImgLoader img;
    std::string imgPath = FindGameFile("models\\gta3.img");
    if (!img.Open(imgPath)) {
        Log("Failed to open %s", imgPath.c_str());
        return 1;
    }
    Log("IMG opened: %u entries", (unsigned)img.EntryCount());

    // ---- Texture cache (needed by both modes) ----
    std::unordered_map<std::string, TxdTexture> texMap;
    std::unordered_map<std::string, bool> txdLoaded;
    // D3D texture cache: txd texture name -> IDirect3DTexture9*
    std::unordered_map<std::string, IDirect3DTexture9*> d3dTexCache;

    auto ensureTxd = [&](const std::string& txdName) {
        if (txdName.empty() || txdLoaded[txdName])
            return;
        txdLoaded[txdName] = true;
        std::string fname = txdName + ".txd";
        if (!img.HasFile(fname)) {
            Log("TXD not in IMG: %s", fname.c_str());
            return;
        }
        std::vector<uint8_t> data = img.Extract(fname);
        if (data.empty()) {
            Log("TXD extract failed: %s", fname.c_str());
            return;
        }
        std::vector<TxdTexture> texs = TxdLoader::LoadFromMemory(data.data(), data.size());
        int added = 0;
        for (auto& t : texs) {
            std::string key = TxdLoader::KeyOf(t.name);
            if (texMap.find(key) == texMap.end()) {
                texMap.emplace(key, std::move(t));
                added++;
            }
        }
        Log("TXD %s: %u textures (%d new)", fname.c_str(), (unsigned)texs.size(), added);
    };

    auto getD3dTexture = [&](const std::string& texName) -> IDirect3DTexture9* {
        std::string key = TxdLoader::KeyOf(texName);
        auto it = d3dTexCache.find(key);
        if (it != d3dTexCache.end())
            return it->second;
        auto texIt = texMap.find(key);
        if (texIt == texMap.end())
            return nullptr;
        TxdTexture& txd = texIt->second;
        IDirect3DTexture9* d3dTex = g_renderer.CreateTexture(
            txd.width, txd.height, txd.d3dFormat,
            txd.mip0.data(), (unsigned)txd.mip0.size());
        d3dTexCache[key] = d3dTex;  // may be nullptr on failure; cached anyway
        return d3dTex;
    };

    std::vector<MapObject> mapObjects;

    if (houseTest) {
        // ===== HOUSE TEST MODE =====
        // Place 3 known house models on a flat plane in front of camera.
        // This bypasses IPL loading to isolate building rendering.
        Log("HOUSE TEST MODE: placing 3 houses");

        struct HouseDef {
            const char* dffName;  // in gta3.img (lowercase)
            const char* txdName;  // base name (no .txd)
            float x, y, z;
        };
        HouseDef houses[] = {
            { "bdupshouse_lae.dff",  "bdupshouse_lae", 2480.0f, -1650.0f, 0.0f },
            { "santahouse02_law2.dff", "bev_law2",     2510.0f, -1650.0f, 0.0f },
            { "cehillhouse04.dff",   "lahillshilhse",  2540.0f, -1650.0f, 0.0f },
        };

        int housesLoaded = 0;
        for (int h = 0; h < 3; h++) {
            Log("House %d: %s at (%.1f, %.1f, %.1f)",
                h, houses[h].dffName, houses[h].x, houses[h].y, houses[h].z);

            if (!img.HasFile(houses[h].dffName)) {
                Log("  FAIL: DFF not in IMG: %s", houses[h].dffName);
                continue;
            }
            std::vector<uint8_t> dffData = img.Extract(houses[h].dffName);
            if (dffData.empty()) {
                Log("  FAIL: DFF extract failed: %s", houses[h].dffName);
                continue;
            }
            Log("  DFF extracted: %u bytes", (unsigned)dffData.size());

            DffModel dff = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
            if (!dff.valid) {
                Log("  FAIL: DFF parse failed: %s", houses[h].dffName);
                continue;
            }
            Log("  DFF parsed: %u meshes", (unsigned)dff.meshes.size());
            for (size_t m = 0; m < dff.meshes.size(); m++) {
                Log("    Mesh %u: %u verts, %u indices, tex='%s'",
                    (unsigned)m,
                    (unsigned)dff.meshes[m].vertices.size(),
                    (unsigned)dff.meshes[m].indices.size(),
                    dff.meshes[m].textureName.c_str());
            }

            // Load TXD
            ensureTxd(houses[h].txdName);

            // Build MapObject with identity rotation at position
            MapObject obj;
            obj.worldMatrix = QuatToD3DMatrix(0, 0, 0, 1,  // identity quaternion
                                              houses[h].x, houses[h].y, houses[h].z);
            for (auto& dffMesh : dff.meshes) {
                const MeshVertex* verts = reinterpret_cast<const MeshVertex*>(dffMesh.vertices.data());
                D3DRenderMesh* mesh = g_renderer.CreateMesh(
                    verts,
                    (uint32_t)dffMesh.vertices.size(),
                    dffMesh.indices.data(),
                    (uint32_t)dffMesh.indices.size());
                if (!mesh) {
                    Log("  FAIL: CreateMesh failed for mesh");
                    continue;
                }
                if (!dffMesh.textureName.empty()) {
                    mesh->texture = getD3dTexture(dffMesh.textureName);
                    if (!mesh->texture)
                        Log("  WARNING: texture not found: %s", dffMesh.textureName.c_str());
                }
                obj.meshes.push_back(mesh);
            }

            if (!obj.meshes.empty()) {
                mapObjects.push_back(std::move(obj));
                housesLoaded++;
                Log("  OK: house %d has %u meshes", h, (unsigned)mapObjects.back().meshes.size());
            } else {
                Log("  FAIL: house %d has 0 meshes", h);
            }
        }

        Log("HOUSE TEST: %d/3 houses loaded, %u map objects", housesLoaded, (unsigned)mapObjects.size());

        // Camera: 60 units south of center house, 25 up, looking at houses
        float camX = 2510.0f, camY = -1710.0f, camZ = 25.0f;
        D3DMATRIX view = MatrixLookAt(camX, camY, camZ,
                                      2510.0f, -1650.0f, 8.0f,  // target: center house
                                      0.0f, 0.0f, 1.0f);
        D3DMATRIX proj = MatrixPerspectiveFov(60.0f * 3.14159f / 180.0f,
                                              (float)WIDTH / (float)HEIGHT,
                                              1.0f, 2000.0f);
        g_renderer.SetViewMatrix(view);
        g_renderer.SetProjMatrix(proj);
        Log("HOUSE TEST: camera at (%.1f, %.1f, %.1f)", camX, camY, camZ);
    } else if (groveTest) {
        // ===== GROVE STREET MODE =====
        // Street scene: houses on both sides of a road, camera looking down the street.
        Log("GROVE STREET MODE: building street scene");

        struct GroveHouse {
            const char* dffName;
            const char* txdName;
            float x, y, z;
            float rotZ;  // yaw rotation in radians (0 = facing +Y/south)
        };
        // Street runs along X axis. Houses on north (y=-1635) and south (y=-1665) sides.
        GroveHouse ghouses[] = {
            // North side (facing south toward street)
            { "bdupshouse_lae.dff",   "bdupshouse_lae", 2480.0f, -1635.0f, 0.0f, 3.14159f },
            { "compmedhos1_lae.dff",  "comedhos1_la",   2500.0f, -1635.0f, 0.0f, 3.14159f },
            { "compmedhos2_lae.dff",  "comedhos1_la",   2520.0f, -1635.0f, 0.0f, 3.14159f },
            { "ganghous01_lax.dff",   "ganghouse1_lax", 2540.0f, -1635.0f, 0.0f, 3.14159f },
            // South side (facing north toward street)
            { "santahouse02_law2.dff", "bev_law2",      2490.0f, -1665.0f, 0.0f, 0.0f },
            { "compmedhos3_lae.dff",   "comedhos1_la",  2510.0f, -1665.0f, 0.0f, 0.0f },
            { "cehillhouse04.dff",     "lahillshilhse", 2530.0f, -1665.0f, 0.0f, 0.0f },
            { "ganghous02_lax.dff",    "ganghouse1_lax",2550.0f, -1665.0f, 0.0f, 0.0f },
        };
        const int numGrove = sizeof(ghouses) / sizeof(ghouses[0]);

        int groveLoaded = 0;
        for (int h = 0; h < numGrove; h++) {
            Log("Grove house %d: %s at (%.1f, %.1f, %.1f)",
                h, ghouses[h].dffName, ghouses[h].x, ghouses[h].y, ghouses[h].z);

            if (!img.HasFile(ghouses[h].dffName)) {
                Log("  SKIP: DFF not in IMG: %s", ghouses[h].dffName);
                continue;
            }
            std::vector<uint8_t> dffData = img.Extract(ghouses[h].dffName);
            if (dffData.empty()) {
                Log("  FAIL: DFF extract failed: %s", ghouses[h].dffName);
                continue;
            }

            DffModel dff = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
            if (!dff.valid) {
                Log("  FAIL: DFF parse failed: %s", ghouses[h].dffName);
                continue;
            }

            ensureTxd(ghouses[h].txdName);

            // Yaw rotation quaternion: (0, 0, sin(yaw/2), cos(yaw/2))
            float hy = ghouses[h].rotZ * 0.5f;
            MapObject obj;
            obj.worldMatrix = QuatToD3DMatrix(0.0f, 0.0f, sinf(hy), cosf(hy),
                                              ghouses[h].x, ghouses[h].y, ghouses[h].z);
            for (auto& dffMesh : dff.meshes) {
                const MeshVertex* verts = reinterpret_cast<const MeshVertex*>(dffMesh.vertices.data());
                D3DRenderMesh* mesh = g_renderer.CreateMesh(
                    verts,
                    (uint32_t)dffMesh.vertices.size(),
                    dffMesh.indices.data(),
                    (uint32_t)dffMesh.indices.size());
                if (!mesh) continue;
                if (!dffMesh.textureName.empty()) {
                    mesh->texture = getD3dTexture(dffMesh.textureName);
                }
                obj.meshes.push_back(mesh);
            }

            if (!obj.meshes.empty()) {
                mapObjects.push_back(std::move(obj));
                groveLoaded++;
                Log("  OK: grove house %d has %u meshes", h, (unsigned)mapObjects.back().meshes.size());
            }
        }

        Log("GROVE: %d/%d houses loaded, %u map objects", groveLoaded, numGrove, (unsigned)mapObjects.size());

        // Camera: west end of street, elevated, looking east down the street
        float gcamX = 2460.0f, gcamY = -1650.0f, gcamZ = 18.0f;
        D3DMATRIX gview = MatrixLookAt(gcamX, gcamY, gcamZ,
                                       2530.0f, -1650.0f, 6.0f,   // target: east down street
                                       0.0f, 0.0f, 1.0f);
        D3DMATRIX gproj = MatrixPerspectiveFov(60.0f * 3.14159f / 180.0f,
                                               (float)WIDTH / (float)HEIGHT,
                                               1.0f, 2000.0f);
        g_renderer.SetViewMatrix(gview);
        g_renderer.SetProjMatrix(gproj);
        Log("GROVE: camera at (%.1f, %.1f, %.1f)", gcamX, gcamY, gcamZ);
    } else {
    // ---- Normal M4 map loading ----
    // (IDE/IPL loading code follows, wrapped in else block)
    std::vector<IdeObject> ideObjects;
    std::unordered_map<int, size_t> ideById;
    std::unordered_map<std::string, size_t> ideByName;

    const char* ideFiles[] = {
        "data\\maps\\LA\\LAe.ide",
        "data\\maps\\LA\\LAe2.ide",
        "data\\maps\\LA\\LAhills.ide",
        "data\\maps\\LA\\LAn.ide",
        "data\\maps\\LA\\LAn2.ide",
        "data\\maps\\LA\\LAs.ide",
        "data\\maps\\LA\\LAs2.ide",
        "data\\maps\\LA\\LAw.ide",
        "data\\maps\\LA\\LAw2.ide",
        "data\\maps\\LA\\LaWn.ide",
        "data\\maps\\LA\\LAxref.ide",
    };
    for (const char* f : ideFiles) {
        std::string path = FindGameFile(f);
        size_t before = ideObjects.size();
        if (IdeLoader::Load(path, ideObjects, ideById))
            Log("IDE %s: %u objects", f, (unsigned)(ideObjects.size() - before));
        else
            Log("IDE %s: FAILED", f);
    }
    for (size_t i = 0; i < ideObjects.size(); i++)
        ideByName[ideObjects[i].modelName] = i;
    Log("Total IDE objects: %u", (unsigned)ideObjects.size());

    // ---- Load IPL ----
    std::vector<IplInstance> instances;
    const char* iplFiles[] = {
        "data\\maps\\LA\\LAe.ipl",
        "data\\maps\\LA\\LAe2.ipl",
        "data\\maps\\LA\\LAhills.ipl",
        "data\\maps\\LA\\LAn.ipl",
        "data\\maps\\LA\\LAn2.ipl",
        "data\\maps\\LA\\LAs.ipl",
        "data\\maps\\LA\\LAs2.ipl",
        "data\\maps\\LA\\LAw.ipl",
        "data\\maps\\LA\\LAw2.ipl",
        "data\\maps\\LA\\LaWn.ipl",
    };
    for (const char* f : iplFiles) {
        std::string path = FindGameFile(f);
        size_t before = instances.size();
        if (IplLoader::Load(path, instances))
            Log("IPL %s: %u instances", f, (unsigned)(instances.size() - before));
        else
            Log("IPL %s: FAILED", f);
    }
    Log("Total instances: %u", (unsigned)instances.size());

    // ---- Filter to Grove Street area ----
    const float cx = 2500.0f, cy = -1700.0f, halfSize = 200.0f;
    std::vector<IplInstance> filtered;
    for (auto& inst : instances) {
        if (inst.interior != 0)
            continue;
        // Skip LOD models (name starts with "lod")
        if (inst.modelName.size() >= 3 &&
            inst.modelName[0] == 'l' && inst.modelName[1] == 'o' && inst.modelName[2] == 'd')
            continue;
        float dx = inst.x - cx, dy = inst.y - cy;
        if (fabsf(dx) < halfSize && fabsf(dy) < halfSize)
            filtered.push_back(inst);
    }
    Log("Filtered to %u instances near Grove Street", (unsigned)filtered.size());

    // (Texture cache already defined above - shared by both modes)

    // ---- Load models and build map objects ----
    std::unordered_map<std::string, CachedModel> modelCache;
    // (mapObjects already declared above - shared by both modes)
    int loadedModels = 0, failedModels = 0, skippedNoIde = 0;

    const size_t maxObjects = 1500;  // cap raised: all LA IPLs now load
    size_t processed = 0;

    for (auto& inst : filtered) {
        if (processed >= maxObjects)
            break;
        processed++;

        auto ideIt = ideByName.find(inst.modelName);
        if (ideIt == ideByName.end()) {
            skippedNoIde++;
            continue;
        }
        const IdeObject& ide = ideObjects[ideIt->second];

        auto cacheIt = modelCache.find(inst.modelName);
        if (cacheIt == modelCache.end()) {
            CachedModel cm;
            std::string dffName = inst.modelName + ".dff";
            if (!img.HasFile(dffName)) {
                failedModels++;
                continue;
            }
            std::vector<uint8_t> dffData = img.Extract(dffName);
            if (dffData.empty()) {
                failedModels++;
                continue;
            }
            cm.dff = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
            if (!cm.dff.valid) {
                failedModels++;
                Log("DFF parse failed: %s", dffName.c_str());
                continue;
            }
            cm.txdName = ide.txdName;
            cm.loaded = true;
            modelCache[inst.modelName] = std::move(cm);
            cacheIt = modelCache.find(inst.modelName);
            loadedModels++;
            ensureTxd(ide.txdName);
        }
        const CachedModel& cm = cacheIt->second;

        MapObject obj;
        // DEBUG: log instance data and matrix
        Log("INST %s at (%.1f, %.1f, %.1f) quat (%.3f, %.3f, %.3f, %.3f)",
            inst.modelName.c_str(), inst.x, inst.y, inst.z,
            inst.qx, inst.qy, inst.qz, inst.qw);
        obj.worldMatrix = QuatToD3DMatrix(inst.qx, inst.qy, inst.qz, inst.qw,
                                          inst.x, inst.y, inst.z);
        Log("  matrix: [%.2f %.2f %.2f %.2f] [%.2f %.2f %.2f %.2f] [%.2f %.2f %.2f %.2f] [%.2f %.2f %.2f %.2f]",
            obj.worldMatrix.m[0][0], obj.worldMatrix.m[0][1], obj.worldMatrix.m[0][2], obj.worldMatrix.m[0][3],
            obj.worldMatrix.m[1][0], obj.worldMatrix.m[1][1], obj.worldMatrix.m[1][2], obj.worldMatrix.m[1][3],
            obj.worldMatrix.m[2][0], obj.worldMatrix.m[2][1], obj.worldMatrix.m[2][2], obj.worldMatrix.m[2][3],
            obj.worldMatrix.m[3][0], obj.worldMatrix.m[3][1], obj.worldMatrix.m[3][2], obj.worldMatrix.m[3][3]);

        for (auto& dffMesh : cm.dff.meshes) {
            // DffVertex and MeshVertex have identical layout (x,y,z,nx,ny,nz,u,v)
            const MeshVertex* verts = reinterpret_cast<const MeshVertex*>(dffMesh.vertices.data());
            D3DRenderMesh* mesh = g_renderer.CreateMesh(
                verts,
                (uint32_t)dffMesh.vertices.size(),
                dffMesh.indices.data(),
                (uint32_t)dffMesh.indices.size());
            if (!mesh)
                continue;
            if (!dffMesh.textureName.empty()) {
                mesh->texture = getD3dTexture(dffMesh.textureName);
            }
            obj.meshes.push_back(mesh);
        }

        if (!obj.meshes.empty())
            mapObjects.push_back(std::move(obj));
    }

    Log("Models: %d loaded, %d failed, %d no IDE", loadedModels, failedModels, skippedNoIde);
    Log("Map objects: %u", (unsigned)mapObjects.size());

    // ---- Camera ----
    // Look at Grove Street from the south, elevated
    D3DMATRIX view = MatrixLookAt(cx, cy - 220.0f, 55.0f,   // eye (south of street)
                                   cx, cy + 80.0f, 12.0f,      // target (looking down Grove St)
                                   0.0f, 0.0f, 1.0f);         // up (Z-up world)
    D3DMATRIX proj = MatrixPerspectiveFov(60.0f * 3.14159f / 180.0f,
                                           (float)WIDTH / (float)HEIGHT,
                                           1.0f, 2000.0f);
    g_renderer.SetViewMatrix(view);
    g_renderer.SetProjMatrix(proj);

    } // end else (normal M4 mode) - houseTest mode skips to here

    IDirect3DDevice9* dev = g_renderer.GetDevice();

    Log("Entering render loop");

    int frame = 0;
    MSG msg = {};
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

        g_renderer.BeginFrame(0.4f, 0.6f, 0.9f);  // sky blue clear

        for (auto& obj : mapObjects) {
            dev->SetTransform(D3DTS_WORLD, &obj.worldMatrix);
            for (auto* mesh : obj.meshes) {
                g_renderer.DrawMesh(mesh);
            }
        }

        g_renderer.EndFrame();

        frame++;
        if (maxFrames > 0 && frame >= maxFrames)
            break;
    }

    if (!shotPath.empty()) {
        g_renderer.SaveScreenshot(shotPath.c_str());
        Log("Screenshot saved to %s", shotPath.c_str());
    }

    Log("M4 done: %d frames, %u map objects", frame, (unsigned)mapObjects.size());
    if (g_log)
        fclose(g_log);

    for (auto& obj : mapObjects) {
        for (auto* mesh : obj.meshes)
            g_renderer.DestroyMesh(mesh);
    }
    for (auto& kv : d3dTexCache) {
        if (kv.second)
            g_renderer.DestroyTexture(kv.second);
    }

    return 0;
}
