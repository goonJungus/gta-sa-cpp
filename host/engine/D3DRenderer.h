#pragma once
// Minimal D3D9 renderer for gtasa_cpp.exe
// Replaces RenderWare for the standalone build. No D3DX dependency.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <vector>

struct D3DRenderMesh {
    IDirect3DVertexBuffer9* vb = nullptr;
    IDirect3DIndexBuffer9*  ib = nullptr;
    IDirect3DTexture9* texture = nullptr;  // M3: optional diffuse texture (untextured if null)
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;

    // ---- Baked (prelit) day/night vertex colours (M6) ----
    // `prelit` meshes carry authored day colours in the vertex buffer and are
    // drawn with fixed-function lighting OFF: their shading is already baked
    // into the vertex colours. `dayColors` mirrors what CreateMesh uploaded;
    // `nightColors` (parallel, empty if the DFF had no 0x253F2F9 plugin) is the
    // SA night set. DrawMesh re-bakes the colour field when the day/night
    // balance has moved, so night DFFs light up as the sun goes down.
    bool prelit = false;
    std::vector<uint32_t> dayColors;
    std::vector<uint32_t> nightColors;  // empty when the mesh has no night set
    float bakedBalance = -1.0f;         // night factor baked into the buffer
    uint32_t bakedStamp = 0;            // m_prelitStamp at bake time

    // Set for meshes whose texture has intermediate (non 0/255) alpha, e.g.
    // road/track decals and foliage cards: drawn with SRCALPHA/INVSRCALPHA
    // blending and no depth write, which an alpha TEST cannot represent.
    bool alphaBlend = false;

    // Cutout-alpha cards (foliage, fences) are thin single-face geometry; SA
    // draws them two-sided so the leaves don't vanish when you orbit behind
    // them. DrawMesh sets D3DCULL_NONE for these, D3DCULL_CW otherwise.
    bool doubleSided = false;
};

// Debug line vertex (F2 wireframe overlay, F3 collision view): an unlit
// position + colour pair drawn as a LINELIST with DrawPrimitiveUP.
struct DebugLineVertex {
    float x, y, z;
    uint32_t color;  // D3DCOLOR
};

// Vertex: position + normal + prelit colour + uv.
// The colour field must come after the normal and before the UVs to match the
// D3D fixed-function FVF order (XYZ | NORMAL | DIFFUSE | TEX1).
struct MeshVertex {
    float x, y, z;
    float nx, ny, nz;
    uint32_t color = 0xFFFFFFFFu;  // D3DCOLOR; white = no baked shading
    float u, v;
};
#define MESH_FVF (D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_DIFFUSE | D3DFVF_TEX1)

class D3DRenderer {
public:
    D3DRenderer();
    ~D3DRenderer();

    bool Init(HWND hwnd, int width, int height);
    void Shutdown();
    // Resize the backbuffer to match the window client area (fixes the blurry
    // stretch when the window is maximized/resized). Safe because every GPU
    // resource is D3DPOOL_MANAGED and survives a device Reset.
    bool Resize(int width, int height);
    int  Width()  const { return m_width; }
    int  Height() const { return m_height; }

    void BeginFrame(float r, float g, float b);
    void DrawTestTriangle();
    void EndFrame();

    // Dev/test: D3DFILL_WIREFRAME / D3DFILL_SOLID. Persists across frames
    // (BeginFrame does not touch FILLMODE), so set it once per toggle.
    void SetWireframe(bool on);

    // Sky (M4): CClouds::RenderSkyPolys + sun billboard. Call after BeginFrame.
    void RenderSky(float camX, float camY, float camZ);

    // --- Night light glows (make the world feel alive after dark) ---
    // Additive, depth-tested, camera-facing glow billboards drawn at lamp bulbs
    // (a bright core sprite + a larger dim halo give the lamp its warm glow).
    // Uses the sun glow texture; color's alpha scales brightness. Call after the
    // world + sky are drawn; the depth test keeps a lamp behind a wall from
    // glowing through it.
    struct GlowSprite { float x, y, z; float size; uint32_t color; };
    void DrawCoronas(const GlowSprite* s, int count);

    // Camera-facing additive (ONE/ONE) sprites with no depth test or write:
    // CClouds' stars, moon and low clouds, drawn after the sky polys and before
    // the world so the world covers them. half* are world-space half extents.
    struct SkySprite { float x, y, z; float halfW, halfH; uint32_t color; };
    void DrawSkySprites(const SkySprite* s, int count, IDirect3DTexture9* tex);

    // Unlit, world-space, alpha-blended triangles whose vertex colour (alpha
    // included) modulates the texture: CWaterLevel's RwIm3D water batches.
    // Depth-tested without depth writes; fog is left as the caller set it.
    struct ColorTexVertex { float x, y, z; uint32_t color; float u, v; };
    void DrawBlendedTriangles(const ColorTexVertex* verts, uint32_t vertexCount,
                              const uint16_t* indices, uint32_t indexCount,
                              IDirect3DTexture9* tex);

    // Time-of-day sky inputs, straight from timecyc.dat (0..255 channels).
    // Defaults are retail SUNNY_LA midday: SkyTop (30,117,210), SkyBot (53,162,227).
    void SetSkyColors(float topR, float topG, float topB,
                      float botR, float botG, float botB);
    // Sun: direction towards it (CTimeCycle::m_VectorToSun, normalized here),
    // SunCore and SunCorona colours (0..255) and the two sprite sizes as
    // angles (corona size / distance, in radians).
    void SetSun(float dirX, float dirY, float dirZ, const float core[3],
                const float corona[3], float coreSize, float coronaSize);

    // World lighting from timecyc.dat: global ambient + one directional sun.
    // Channels are 0..255 (as read from the file); `toSun*` is the direction
    // from the scene towards the sun (same vector handed to SetSun); dirScale
    // fades the sun out at night. Once called, every DrawMesh is lit
    // (fixed-function lighting, FVF has normals) until the process ends.
    void SetWorldLighting(float ambR, float ambG, float ambB,
                          float dirR, float dirG, float dirB,
                          float toSunX, float toSunY, float toSunZ,
                          float dirScale);

    // Dev A/B switch: OFF draws the world full-bright (texture colour as-is),
    // the way the renderer behaved before timecyc lighting was wired up, and
    // also makes prelit meshes ignore their baked vertex colours.
    void SetLightingEnabled(bool on);

    // Pair Present to the display instead of presenting immediately. Works
    // before Init or at runtime (resets the device with the new interval).
    bool SetVsync(bool on);
    // Mip filtering of the world textures (the Display > Advanced MIP MAPPING switch).
    void SetMipmaps(bool on) { if (m_device) m_device->SetSamplerState(0, D3DSAMP_MIPFILTER, on ? D3DTEXF_LINEAR : D3DTEXF_NONE); }
    bool Vsync() const { return m_vsync; }

    // Baked day/night prelit blend (M6). nightFactor 0 = day prelit colours
    // (as authored), 1 = the night prelit set. Prelit meshes without a night
    // set are darkened by `tint` (0..1, from the timecyc ambient) instead, so
    // they do not stay noon-bright all night. Ignored while lighting is off.
    void SetPrelitShading(float nightFactor,
                          float tintR, float tintG, float tintB);

    // Mesh rendering. `prelit` = the vertex `color` field carries authored
    // baked day shading; `nightColors` (numVerts entries) is the optional
    // parallel night set parsed from DFF plugin 0x253F2F9.
    D3DRenderMesh* CreateMesh(const MeshVertex* verts, uint32_t numVerts,
                              const uint16_t* indices, uint32_t numIndices,
                              bool prelit = false,
                              const uint32_t* nightColors = nullptr);
    void DrawMesh(D3DRenderMesh* mesh);
    void DestroyMesh(D3DRenderMesh* mesh);
    // Rewrite a mesh's vertices in place (CPU-skinned peds, once per frame).
    // `count` must not exceed the mesh's vertexCount.
    bool UpdateMeshVertices(D3DRenderMesh* mesh, const MeshVertex* verts, uint32_t count);

    // Transient debug overlay geometry (collision volumes). `verts` is a
    // LINELIST (pairs); drawn unlit, depth-tested and depth-writing so the
    // volumes sit inside the scene rather than on top of it.
    void DrawDebugLines(const DebugLineVertex* verts, uint32_t vertexCount);

    // Textures (M3: raw D3D9, no D3DX). data is the level-0 mip in the
    // format's native byte order; DXT blocks are copied per block-row.
    IDirect3DTexture9* CreateTexture(uint32_t width, uint32_t height,
                                     D3DFORMAT fmt, const void* data,
                                     uint32_t dataSize,
                                     const std::vector<std::vector<uint8_t>>* extraMips = nullptr);
    void DestroyTexture(IDirect3DTexture9* tex);

    // Test helper: save the current backbuffer to a 32-bit BMP file.
    bool SaveScreenshot(const char* path);

    // Camera (raw D3DMATRIX, column-major as D3D expects)
    void SetViewMatrix(const D3DMATRIX& view);
    void SetProjMatrix(const D3DMATRIX& proj);

    bool IsValid() const { return m_device != nullptr; }
    IDirect3DDevice9* GetDevice() const { return m_device; }
    const char* GetAdapterDesc() const { return m_adapterDesc; }
    const D3DMATRIX& GetViewMatrix() const { return m_view; }
    const D3DMATRIX& GetProjMatrix() const { return m_proj; }

private:
    IDirect3D9*       m_d3d;
    IDirect3DDevice9* m_device;
    int               m_width;
    int               m_height;
    char              m_adapterDesc[128];
    D3DPRESENT_PARAMETERS m_pp = {};  // kept so Resize() can re-Reset with a new size
    void ApplyDeviceState();          // render states set after device create/Reset
    void drawGlowBatch(const void* verts, int quadCount);  // additive glow quads (coronas/pools)

    // Sky resources (M4): the sun glow texture, built on first RenderSky.
    D3DMATRIX m_view;  // cached view matrix (billboard orientation)
    D3DMATRIX m_proj;  // cached projection matrix (frustum culling)
    IDirect3DTexture9* m_sunTex = nullptr;
    void BuildSkyResources();

    // Time-of-day inputs for the sky (defaults = retail SUNNY_LA midday).
    float m_skyTop[3] = {30.0f, 117.0f, 210.0f};
    float m_skyBot[3] = {53.0f, 162.0f, 227.0f};
    float m_sunDir[3] = {0.30f, -0.55f, 0.72f};
    float m_sunCol[3] = {1.0f, 1.0f, 1.0f};
    float m_sunCorona[3] = {1.0f, 1.0f, 1.0f};
    float m_sunCoreSize = 0.0f, m_sunCoronaSize = 0.0f;
    float m_belowHorizon[3] = {63.0f, 135.0f, 177.0f};  // SetSkyColors

    // World lighting (SetWorldLighting), in 0..1 device units.
    bool  m_lightingReady = false;
    bool  m_lightingAllowed = true;
    bool  m_vsync = false;                        // menu VSync toggle
    float m_lightAmb[3] = {0.4f, 0.4f, 0.4f};
    float m_lightDir[3] = {1.0f, 1.0f, 1.0f};

    // Baked prelit day/night blend (M6).
    float m_prelitBalance = 0.0f;                 // 0 = day, 1 = night
    float m_prelitTint[3] = {1.0f, 1.0f, 1.0f};   // night fallback tint
    uint32_t m_prelitStamp = 1;                   // bumped when the mode flips
    bool m_currentLit = true;                     // last D3DRS_LIGHTING value
    bool m_alphaBlend = false;                     // last D3DRS_ALPHABLENDENABLE
    int m_rebakeBudget = 0;                       // prelit re-bakes left this frame

    // Sets D3DRS_LIGHTING only when it actually changes.
    void SetLightState(bool on);
    // Rewrites the colour field of a prelit mesh for the current balance.
    void BakeMeshColors(D3DRenderMesh* mesh);
};

// Math helpers (no D3DX)
D3DMATRIX MatrixIdentity();
D3DMATRIX MatrixPerspectiveFov(float fovY, float aspect, float zn, float zf);
D3DMATRIX MatrixLookAt(float ex, float ey, float ez,
                       float tx, float ty, float tz,
                       float ux, float uy, float uz);
