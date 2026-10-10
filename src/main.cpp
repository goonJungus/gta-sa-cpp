// gtasa_cpp.exe - standalone GTA San Andreas C++ rewrite entry point.
// Milestone 2: load a DFF model and render it with D3D9.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include "D3DRenderer.h"
#include "DffLoader.h"

static D3DRenderer g_renderer;
static bool g_running = true;
static const wchar_t* WINDOW_CLASS = L"GTASACppWindow";
static const wchar_t* WINDOW_TITLE = L"GTA SA C++";

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
// the GTA_SA_DIR env var or a sibling "gta-sa" directory.
static std::string FindGameFile(const std::string& rel) {
    const char* env = getenv("GTA_SA_DIR");
    std::string base = env ? env : "";
    if (!base.empty()) {
        std::string p = base + "\\" + rel;
        DWORD attr = GetFileAttributesA(p.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES)
            return p;
    }
    // Fallback: original decomp-time location
    std::string p = "C:\\Users\\fufid\\Documents\\Decomps\\gta-sa\\" + rel;
    return p;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nShow) {
    const int WIDTH = 1280;
    const int HEIGHT = 720;

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
        MessageBoxW(hwnd, L"Failed to initialize D3D9", WINDOW_TITLE, MB_ICONERROR);
        return 3;
    }

    // Camera: perspective + look-at origin from an angle
    g_renderer.SetProjMatrix(MatrixPerspectiveFov(3.14159f / 4.0f,
        (float)WIDTH / (float)HEIGHT, 0.1f, 1000.0f));
    g_renderer.SetViewMatrix(MatrixLookAt(8, 6, 8, 0, 0, 0, 0, 1, 0));

    // Load test model
    std::vector<D3DRenderMesh*> meshes;
    std::string dffPath = FindGameFile("models\\generic\\arrow.DFF");
    DffModel model = DffLoader::Load(dffPath);
    if (model.valid) {
        for (auto& dm : model.meshes) {
            std::vector<MeshVertex> verts(dm.vertices.size());
            for (size_t i = 0; i < verts.size(); i++) {
                verts[i].x = dm.vertices[i].x;
                verts[i].y = dm.vertices[i].y;
                verts[i].z = dm.vertices[i].z;
                verts[i].nx = dm.vertices[i].nx;
                verts[i].ny = dm.vertices[i].ny;
                verts[i].nz = dm.vertices[i].nz;
                verts[i].u = dm.vertices[i].u;
                verts[i].v = dm.vertices[i].v;
            }
            D3DRenderMesh* rm = g_renderer.CreateMesh(
                verts.data(), (uint32_t)verts.size(),
                dm.indices.data(), (uint32_t)dm.indices.size());
            if (rm)
                meshes.push_back(rm);
        }
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

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

        g_renderer.BeginFrame(0.1f, 0.1f, 0.35f);
        if (!meshes.empty()) {
            for (auto m : meshes)
                g_renderer.DrawMesh(m);
        } else {
            g_renderer.DrawTestTriangle();  // fallback if DFF failed to load
        }
        g_renderer.EndFrame();
    }

    for (auto m : meshes)
        g_renderer.DestroyMesh(m);
    g_renderer.Shutdown();
    return 0;
}
