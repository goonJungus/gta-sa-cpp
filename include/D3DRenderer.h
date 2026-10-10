#pragma once
// Minimal D3D9 renderer for gtasa_cpp.exe
// Replaces RenderWare for the standalone build. No D3DX dependency.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdint>

struct D3DRenderMesh {
    IDirect3DVertexBuffer9* vb = nullptr;
    IDirect3DIndexBuffer9*  ib = nullptr;
    IDirect3DTexture9* texture = nullptr;  // M3: optional diffuse texture (untextured if null)
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

// Vertex: position + normal + uv
struct MeshVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};
#define MESH_FVF (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)

class D3DRenderer {
public:
    D3DRenderer();
    ~D3DRenderer();

    bool Init(HWND hwnd, int width, int height);
    void Shutdown();

    void BeginFrame(float r, float g, float b);
    void DrawTestTriangle();
    void EndFrame();

    // Sky (M4): gradient dome + sun billboard. Call after BeginFrame.
    void RenderSky(float camX, float camY, float camZ);

    // Mesh rendering
    D3DRenderMesh* CreateMesh(const MeshVertex* verts, uint32_t numVerts,
                              const uint16_t* indices, uint32_t numIndices);
    void DrawMesh(D3DRenderMesh* mesh);
    void DestroyMesh(D3DRenderMesh* mesh);

    // Textures (M3: raw D3D9, no D3DX). data is the level-0 mip in the
    // format's native byte order; DXT blocks are copied per block-row.
    IDirect3DTexture9* CreateTexture(uint32_t width, uint32_t height,
                                     D3DFORMAT fmt, const void* data,
                                     uint32_t dataSize);
    void DestroyTexture(IDirect3DTexture9* tex);

    // Test helper: save the current backbuffer to a 32-bit BMP file.
    bool SaveScreenshot(const char* path);

    // Camera (raw D3DMATRIX, column-major as D3D expects)
    void SetViewMatrix(const D3DMATRIX& view);
    void SetProjMatrix(const D3DMATRIX& proj);

    bool IsValid() const { return m_device != nullptr; }
    IDirect3DDevice9* GetDevice() const { return m_device; }
    const char* GetAdapterDesc() const { return m_adapterDesc; }

private:
    IDirect3D9*       m_d3d;
    IDirect3DDevice9* m_device;
    int               m_width;
    int               m_height;
    char              m_adapterDesc[128];

    // Sky resources (M4), built lazily on first RenderSky.
    D3DMATRIX m_view;  // cached view matrix (billboard orientation)
    IDirect3DVertexBuffer9* m_skyVB = nullptr;
    IDirect3DIndexBuffer9*  m_skyIB = nullptr;
    uint32_t m_skyIndexCount = 0;
    IDirect3DTexture9* m_sunTex = nullptr;
    void BuildSkyResources();
};

// Math helpers (no D3DX)
D3DMATRIX MatrixIdentity();
D3DMATRIX MatrixPerspectiveFov(float fovY, float aspect, float zn, float zf);
D3DMATRIX MatrixLookAt(float ex, float ey, float ez,
                       float tx, float ty, float tz,
                       float ux, float uy, float uz);
