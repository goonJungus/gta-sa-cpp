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

    // Mesh rendering
    D3DRenderMesh* CreateMesh(const MeshVertex* verts, uint32_t numVerts,
                              const uint16_t* indices, uint32_t numIndices);
    void DrawMesh(D3DRenderMesh* mesh);
    void DestroyMesh(D3DRenderMesh* mesh);

    // Camera (raw D3DMATRIX, column-major as D3D expects)
    void SetViewMatrix(const D3DMATRIX& view);
    void SetProjMatrix(const D3DMATRIX& proj);

    bool IsValid() const { return m_device != nullptr; }
    IDirect3DDevice9* GetDevice() const { return m_device; }

private:
    IDirect3D9*       m_d3d;
    IDirect3DDevice9* m_device;
    int               m_width;
    int               m_height;
};

// Math helpers (no D3DX)
D3DMATRIX MatrixIdentity();
D3DMATRIX MatrixPerspectiveFov(float fovY, float aspect, float zn, float zf);
D3DMATRIX MatrixLookAt(float ex, float ey, float ez,
                       float tx, float ty, float tz,
                       float ux, float uy, float uz);
