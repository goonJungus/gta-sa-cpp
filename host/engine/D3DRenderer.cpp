#include "D3DRenderer.h"
#include <cmath>
#include <cstring>
#include <vector>

struct TestVertex {
    float x, y, z;
    DWORD color;
};
#define TEST_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)

// Per-channel helpers for baked vertex colours (0xAARRGGBB).
// RW stores prelit colours at half brightness (128 = unshaded), and the
// original D3D8 pipeline doubled them via a 2.0 material diffuse. We fold the
// same gain into the bake instead of relying on a material.
static const float kPrelitGain = 2.0f;
static inline uint32_t BlendColor(uint32_t a, uint32_t b, float t) {
    auto ch = [t](uint32_t x, uint32_t y, int shift) {
        float v = (float)((x >> shift) & 0xFF) +
                  ((float)((y >> shift) & 0xFF) - (float)((x >> shift) & 0xFF)) * t;
        int i = (int)(v + 0.5f);
        return (uint32_t)(i < 0 ? 0 : (i > 255 ? 255 : i));
    };
    return 0xFF000000u | (ch(a, b, 16) << 16) | (ch(a, b, 8) << 8) | ch(a, b, 0);
}

static inline uint32_t ScaleColor(uint32_t c, float r, float g, float b) {
    auto ch = [](float mul, uint32_t x, int shift) {
        int i = (int)((float)((x >> shift) & 0xFF) * mul + 0.5f);
        return (uint32_t)(i < 0 ? 0 : (i > 255 ? 255 : i));
    };
    return 0xFF000000u | (ch(r, c, 16) << 16) | (ch(g, c, 8) << 8) | ch(b, c, 0);
}

D3DRenderer::D3DRenderer()
    : m_d3d(nullptr), m_device(nullptr), m_width(0), m_height(0) {
    m_adapterDesc[0] = '\0';
    m_view = MatrixIdentity();
}

D3DRenderer::~D3DRenderer() {
    Shutdown();
}

bool D3DRenderer::Init(HWND hwnd, int width, int height) {
    m_width = width;
    m_height = height;

    m_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!m_d3d)
        return false;

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    // COPY (not DISCARD) so SaveScreenshot can read the backbuffer after
    // Present. Slightly slower; irrelevant for this test harness.
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth = width;
    pp.BackBufferHeight = height;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;   // stencil: the circular radar mask; D16 if unavailable
    // IMMEDIATE (no vsync) by default: it keeps headless captures fast and the
    // RDP/virtual presenters used for remote sessions behave better unpaced.
    // The menu VSync toggle paces Present to the display instead.
    pp.PresentationInterval = m_vsync ? D3DPRESENT_INTERVAL_ONE
                                      : D3DPRESENT_INTERVAL_IMMEDIATE;

    // Try every adapter in turn: virtual display drivers (RDP/vGPU shims)
    // may not support D3D9 HAL, so D3DADAPTER_DEFAULT is not reliable.
    UINT adapterCount = m_d3d->GetAdapterCount();
    for (int attempt = 0; attempt < 2 && !m_device; attempt++) {
    if (attempt == 1) pp.AutoDepthStencilFormat = D3DFMT_D16;
    for (UINT a = 0; a < adapterCount && !m_device; a++) {
        D3DADAPTER_IDENTIFIER9 ident = {};
        m_d3d->GetAdapterIdentifier(a, 0, &ident);
        HRESULT hr = m_d3d->CreateDevice(
            a,
            D3DDEVTYPE_HAL,
            hwnd,
            D3DCREATE_HARDWARE_VERTEXPROCESSING,
            &pp,
            &m_device);
        if (FAILED(hr)) {
            hr = m_d3d->CreateDevice(
                a,
                D3DDEVTYPE_HAL,
                hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                &pp,
                &m_device);
        }
        if (SUCCEEDED(hr) && m_device) {
            strncpy(m_adapterDesc, ident.Description, sizeof(m_adapterDesc) - 1);
            m_adapterDesc[sizeof(m_adapterDesc) - 1] = '\0';
        }
    }
    }
    if (!m_device) {
        m_d3d->Release();
        m_d3d = nullptr;
        return false;
    }
    m_pp = pp;  // keep for Resize()

    ApplyDeviceState();
    return true;
}

// Render states that must be (re)set after the device is created or Reset.
// A device Reset clears all render/sampler states back to defaults, so this is
// called from both Init() and Resize().
void D3DRenderer::ApplyDeviceState() {
    if (!m_device) return;
    // SA's world data is right-handed (X east, Y north, Z up) while D3D is
    // left-handed, so MatrixPerspectiveFov negates its horizontal scale to
    // reflect the image back to the correct handedness. That reflection also
    // reverses the screen-space winding of every triangle, so back-face culling
    // is inverted to match: outward-facing RH triangles (the DFF loader emits
    // them unchanged, confirmed by the signed-volume probe) end up CLOCKWISE on
    // screen, which D3DCULL_CW keeps.
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
    m_currentLit = true;        // force SetLightState to write the device
    SetLightState(false);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    // M3: texture sampling for stage 0
    m_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC);
    m_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
    m_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    m_device->SetSamplerState(0, D3DSAMP_MAXANISOTROPY, 4);
    m_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    m_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    // Alpha test for cutout foliage/fences. Ref 0x80 (not 0x40) discards the
    // anti-aliased *edge* texels, whose RGB fades toward the texture's black
    // transparent border; keeping them opaque (low ref) gave every leaf a dark
    // fringe that stacked into a near-black canopy. 0x80 keeps only solid cores.
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
    m_device->SetRenderState(D3DRS_ALPHAREF, 0x80);
    // World lighting (SetWorldLighting): white material so the vertex colour
    // becomes ambient + directional*NdotL, with the texture modulated on top.
    m_device->SetRenderState(D3DRS_NORMALIZENORMALS, TRUE);
    m_device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    D3DMATERIAL9 mat;
    ZeroMemory(&mat, sizeof(mat));
    mat.Ambient.r = mat.Ambient.g = mat.Ambient.b = mat.Ambient.a = 1.0f;
    mat.Diffuse = mat.Ambient;
    mat.Power = 1.0f;
    m_device->SetMaterial(&mat);
}

// Resize the backbuffer to match a new client area. All GPU resources are
// MANAGED (survive Reset), so we only re-Reset and reapply render states.
// m_lightingReady state is preserved; main.cpp re-sends SetWorldLighting every
// frame, so the D3DLIGHT9 is reinstalled next frame.
bool D3DRenderer::Resize(int width, int height) {
    if (!m_device || width <= 0 || height <= 0) return false;
    if (width == m_width && height == m_height) return true;
    m_pp.BackBufferWidth = (UINT)width;
    m_pp.BackBufferHeight = (UINT)height;
    HRESULT hr = m_device->Reset(&m_pp);
    if (FAILED(hr)) return false;
    m_width = width;
    m_height = height;
    ApplyDeviceState();
    return true;
}

// Runtime vsync toggle: the present interval lives in the present
// parameters, so changing it needs a Reset (safe, everything is MANAGED).
bool D3DRenderer::SetVsync(bool on) {
    if (on == m_vsync) return true;
    m_vsync = on;
    if (!m_device) return true;   // picked up by Init
    m_pp.PresentationInterval = on ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    HRESULT hr = m_device->Reset(&m_pp);
    if (FAILED(hr)) return false;
    ApplyDeviceState();
    return true;
}

void D3DRenderer::Shutdown() {
    if (m_sunTex) { m_sunTex->Release(); m_sunTex = nullptr; }
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
    if (m_d3d) {
        m_d3d->Release();
        m_d3d = nullptr;
    }
}

// Dev/test: fill mode toggle (F2 in --play). Wireframe is a pure render state,
// so it survives BeginFrame's clear and applies to every DrawMesh.
void D3DRenderer::SetWireframe(bool on) {
    if (!m_device)
        return;
    m_device->SetRenderState(D3DRS_FILLMODE, on ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
}

void D3DRenderer::SetLightState(bool on) {
    if (!m_device)
        return;
    if (m_currentLit == on)
        return;
    m_device->SetRenderState(D3DRS_LIGHTING, on ? TRUE : FALSE);
    m_currentLit = on;
}

void D3DRenderer::BeginFrame(float r, float g, float b) {
    if (!m_device)
        return;
    DWORD clearColor = D3DCOLOR_COLORVALUE(r, g, b, 1.0f);
    m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, clearColor, 1.0f, 0);
    m_device->BeginScene();
    // World geometry is lit once SetWorldLighting has supplied the timecyc
    // colours; the sky and the overlay text turn lighting off for themselves.
    SetLightState(m_lightingReady && m_lightingAllowed);
    m_rebakeBudget = 200;  // prelit meshes re-baked per frame (see DrawMesh)
    if (!m_device)
        return;
    TestVertex verts[3] = {
        {  0.0f,  0.5f, 0.5f, D3DCOLOR_XRGB(255, 0, 0) },
        {  0.5f, -0.5f, 0.5f, D3DCOLOR_XRGB(0, 255, 0) },
        { -0.5f, -0.5f, 0.5f, D3DCOLOR_XRGB(0, 0, 255) },
    };
    m_device->SetFVF(TEST_FVF);
    m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, verts, sizeof(TestVertex));
}

void D3DRenderer::EndFrame() {
    if (!m_device)
        return;
    m_device->EndScene();
    m_device->Present(nullptr, nullptr, nullptr, nullptr);
}

bool D3DRenderer::UpdateMeshVertices(D3DRenderMesh* mesh, const MeshVertex* verts, uint32_t count) {
    if (!mesh || !mesh->vb || !verts || count > mesh->vertexCount) return false;
    void* p = nullptr;
    if (FAILED(mesh->vb->Lock(0, count * sizeof(MeshVertex), &p, 0))) return false;
    memcpy(p, verts, count * sizeof(MeshVertex));
    mesh->vb->Unlock();
    return true;
}

D3DRenderMesh* D3DRenderer::CreateMesh(const MeshVertex* verts, uint32_t numVerts,
                                       const uint16_t* indices, uint32_t numIndices,
                                       bool prelit, const uint32_t* nightColors) {
    if (!m_device || !verts || !indices || !numVerts || !numIndices)
        return nullptr;
    D3DRenderMesh* mesh = new D3DRenderMesh();
    if (FAILED(m_device->CreateVertexBuffer(numVerts * sizeof(MeshVertex), 0,
                                            MESH_FVF, D3DPOOL_MANAGED,
                                            &mesh->vb, nullptr))) {
        delete mesh;
        return nullptr;
    }
    void* p = nullptr;
    if (SUCCEEDED(mesh->vb->Lock(0, 0, &p, 0))) {
        memcpy(p, verts, numVerts * sizeof(MeshVertex));
        mesh->vb->Unlock();    }
    if (FAILED(m_device->CreateIndexBuffer(numIndices * sizeof(uint16_t), 0,
                                           D3DFMT_INDEX16, D3DPOOL_MANAGED,
                                           &mesh->ib, nullptr))) {
        mesh->vb->Release();
        delete mesh;
        return nullptr;
    }
    if (SUCCEEDED(mesh->ib->Lock(0, 0, &p, 0))) {
        memcpy(p, indices, numIndices * sizeof(uint16_t));
        mesh->ib->Unlock();
    }
    mesh->vertexCount = numVerts;
    mesh->indexCount = numIndices;
    mesh->prelit = prelit;
    if (prelit) {
        // Keep the day colours so the buffer can be re-baked when the
        // day/night balance moves (the buffer itself is the only other copy).
        mesh->dayColors.resize(numVerts);
        for (uint32_t i = 0; i < numVerts; i++)
            mesh->dayColors[i] = verts[i].color;
        if (nightColors)
            mesh->nightColors.assign(nightColors, nightColors + numVerts);
        mesh->bakedBalance = -1.0f;
        mesh->bakedStamp = 0;
        // Bake now (load time) rather than on first draw: a first-draw bake
        // re-locked the buffer in the very frame the model popped in.
        BakeMeshColors(mesh);
    }
    // Managed resources upload to the GPU lazily, on their first draw - so a
    // freshly streamed model used to stall the frame it popped in (the
    // "micro-freeze"). PreLoad pushes the upload into the streaming step,
    // which is time-budgeted and shows up in the profiler's load time.
    mesh->vb->PreLoad();
    mesh->ib->PreLoad();
    return mesh;
}

// Rewrites the per-vertex colour field for the current day/night balance.
// Locks the (managed) vertex buffer in place so the positions/normals/UVs are
// preserved; only the DWORD colour at each stride is overwritten.
void D3DRenderer::BakeMeshColors(D3DRenderMesh* mesh) {
    if (!m_device || !mesh || !mesh->vb || !mesh->prelit || mesh->dayColors.empty())
        return;
    const uint32_t n = (uint32_t)mesh->dayColors.size();
    if (mesh->vertexCount < n)
        return;

    const bool unshaded = !m_lightingAllowed;  // F10 OFF: full-bright
    const float f = unshaded ? 0.0f : m_prelitBalance;

    void* p = nullptr;
    if (FAILED(mesh->vb->Lock(0, 0, &p, 0)))
        return;
    uint8_t* base = (uint8_t*)p;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t col;
        if (unshaded) {
            col = 0xFFFFFFFFu;
        } else if (!mesh->nightColors.empty()) {
            // Authored day -> night prelit blend; this is what makes windows
            // and neon light up as the sun goes down.
            col = BlendColor(mesh->dayColors[i], mesh->nightColors[i], f);
        } else {
            // No authored night set: darken the day colours by the timecyc
            // ambient tint so the mesh is not noon-bright at 22:00.
            col = ScaleColor(mesh->dayColors[i],
                             m_prelitTint[0] + (1.0f - m_prelitTint[0]) * (1.0f - f),
                             m_prelitTint[1] + (1.0f - m_prelitTint[1]) * (1.0f - f),
                             m_prelitTint[2] + (1.0f - m_prelitTint[2]) * (1.0f - f));
        }
        if (!unshaded)
            col = ScaleColor(col, kPrelitGain, kPrelitGain, kPrelitGain);
        *(uint32_t*)(base + (size_t)i * sizeof(MeshVertex) +
                     offsetof(MeshVertex, color)) = col;
    }
    mesh->vb->Unlock();
    mesh->bakedBalance = f;
    mesh->bakedStamp = m_prelitStamp;
}

void D3DRenderer::DrawMesh(D3DRenderMesh* mesh) {
    if (!m_device || !mesh || !mesh->vb || !mesh->ib)
        return;
    if (mesh->prelit) {
        // Baked shading: re-bake when the balance moved or the mode flipped.
        // A mode flip (F10) must take effect immediately; balance steps are
        // budgeted per frame so a dusk does not lock thousands of buffers in
        // one frame (stale meshes just catch up over the next few frames).
        if (mesh->bakedStamp != m_prelitStamp) {
            BakeMeshColors(mesh);
        } else if (fabsf(mesh->bakedBalance - m_prelitBalance) > (1.0f / 32.0f) &&
                   m_rebakeBudget > 0) {
            m_rebakeBudget--;
            BakeMeshColors(mesh);
        }
        SetLightState(false);  // colours are already shaded
    } else {
        SetLightState(m_lightingReady && m_lightingAllowed);
    }
    m_device->SetFVF(MESH_FVF);
    m_device->SetStreamSource(0, mesh->vb, 0, sizeof(MeshVertex));
    m_device->SetIndices(mesh->ib);
    // Soft-alpha textures (road/sand decals, foliage, glass) blend; 1-bit
    // alpha keeps the Init alpha test. Blended surfaces do not write depth so
    // a mostly transparent texel cannot occlude what is behind it.
    if (mesh->alphaBlend != m_alphaBlend) {
        m_alphaBlend = mesh->alphaBlend;
        m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, m_alphaBlend ? TRUE : FALSE);
        m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        m_device->SetRenderState(D3DRS_ZWRITEENABLE, m_alphaBlend ? FALSE : TRUE);
    }
    m_device->SetTexture(0, mesh->texture);  // nullptr = untextured
    // Two-sided for cutout foliage/fences so they're visible from behind.
    if (mesh->doubleSided)
        m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                   mesh->vertexCount, 0,
                                   mesh->indexCount / 3);
    if (mesh->doubleSided)
        m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);  // restore world default
    m_device->SetTexture(0, nullptr);
}

void D3DRenderer::DrawDebugLines(const DebugLineVertex* verts, uint32_t vertexCount) {
    if (!m_device || !verts || vertexCount < 2)
        return;
    // Debug overlay only: never leave lighting/blend/alpha-test state on for
    // the world pass that follows, and never sample a stale stage texture.
    m_device->SetTexture(0, nullptr);
    SetLightState(false);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    // Debug overlay: draw ON TOP (no depth test/write) so the collision mesh is
    // visible instead of z-fighting / being occluded by the surfaces it hugs.
    m_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    // Vertices are already in world space; the last DrawMesh left a stray
    // instance matrix in D3DTS_WORLD, which would warp the lines off-screen.
    D3DMATRIX ident = MatrixIdentity();
    m_device->SetTransform(D3DTS_WORLD, &ident);
    m_device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    m_device->DrawPrimitiveUP(D3DPT_LINELIST, vertexCount / 2, verts,
                              sizeof(DebugLineVertex));
    // Restore depth state for whatever draws next (water plane, lamps, HUD).
    m_device->SetRenderState(D3DRS_ZENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_FOGENABLE, TRUE);
    if (m_alphaBlend) {
        // DrawMesh caches this; the world pass must see the real state again.
        m_alphaBlend = false;
        m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    }
}

void D3DRenderer::DestroyMesh(D3DRenderMesh* mesh) {
    if (!mesh)
        return;
    if (mesh->vb) mesh->vb->Release();
    if (mesh->ib) mesh->ib->Release();
    // NOTE: mesh->texture is NOT released here. Textures are owned by the
    // texture cache (World::m_d3dTex / main.cpp d3dTexCache) and can be
    // shared by many meshes; releasing per-mesh would double-free shared
    // textures. Owners release once via DestroyTexture.
    delete mesh;
}

IDirect3DTexture9* D3DRenderer::CreateTexture(uint32_t width, uint32_t height,
                                              D3DFORMAT fmt, const void* data,
                                              uint32_t dataSize,
                                              const std::vector<std::vector<uint8_t>>* extraMips) {
    if (!m_device || !data || !width || !height || !dataSize)
        return nullptr;
    const uint32_t levels = 1 + (extraMips ? (uint32_t)extraMips->size() : 0);
    IDirect3DTexture9* tex = nullptr;
    if (FAILED(m_device->CreateTexture(width, height, levels, 0, fmt,
                                       D3DPOOL_MANAGED, &tex, nullptr)))
        return nullptr;
    // Row/block geometry for one mip level (dims halve, never below 1).
    auto levelRows = [fmt](uint32_t w, uint32_t h, uint32_t& rowBytes, uint32_t& numRows) {
        if (fmt == D3DFMT_DXT1 || fmt == D3DFMT_DXT3) {
            // DXT stores 4x4-texel blocks; copy whole block rows so a
            // driver pitch wider than the data still works.
            uint32_t blockBytes = (fmt == D3DFMT_DXT1) ? 8 : 16;
            rowBytes = ((w + 3) / 4) * blockBytes;
            numRows = (h + 3) / 4;
        } else {
            // Uncompressed: assume 4 bytes/pixel (A8R8G8B8).
            rowBytes = w * 4;
            numRows = h;
        }
    };
    bool ok = true;
    for (uint32_t m = 0; m < levels && ok; m++) {
        const void* srcData = data;
        uint32_t srcSize = dataSize;
        if (m > 0) {
            srcData = (*extraMips)[m - 1].data();
            srcSize = (uint32_t)(*extraMips)[m - 1].size();
        }
        uint32_t lw = width >> m;
        if (lw == 0)
            lw = 1;
        uint32_t lh = height >> m;
        if (lh == 0)
            lh = 1;
        uint32_t rowBytes = 0, numRows = 0;
        levelRows(lw, lh, rowBytes, numRows);
        uint32_t need = rowBytes * numRows;
        if (srcSize < need) {  // truncated level: drop the whole chain
            ok = false;
            break;
        }
        D3DLOCKED_RECT lr = {};
        if (FAILED(tex->LockRect(m, &lr, nullptr, 0))) {
            ok = false;
            break;
        }
        const uint8_t* src = (const uint8_t*)srcData;
        uint8_t* dst = (uint8_t*)lr.pBits;
        for (uint32_t y = 0; y < numRows; y++)
            memcpy(dst + (size_t)y * lr.Pitch, src + (size_t)y * rowBytes, rowBytes);
        tex->UnlockRect(m);
    }
    if (!ok) {
        tex->Release();
        return nullptr;
    }
    tex->PreLoad();  // upload now, not on first draw (see CreateMesh)
    return tex;
}

void D3DRenderer::DestroyTexture(IDirect3DTexture9* tex) {
    if (tex)
        tex->Release();
}

bool D3DRenderer::SaveScreenshot(const char* path) {
    if (!m_device || !path)
        return false;
    IDirect3DSurface9* rt = nullptr;
    if (FAILED(m_device->GetRenderTarget(0, &rt)) || !rt)
        return false;
    D3DSURFACE_DESC desc = {};
    rt->GetDesc(&desc);
    IDirect3DSurface9* sys = nullptr;
    HRESULT hr = m_device->CreateOffscreenPlainSurface(
        desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr);
    bool ok = false;
    if (SUCCEEDED(hr) && sys &&
        SUCCEEDED(m_device->GetRenderTargetData(rt, sys))) {
        D3DLOCKED_RECT lr = {};
        if (SUCCEEDED(sys->LockRect(&lr, nullptr, D3DLOCK_READONLY))) {
            uint32_t bpp = (desc.Format == D3DFMT_R5G6B5 ||
                            desc.Format == D3DFMT_X1R5G5B5 ||
                            desc.Format == D3DFMT_A1R5G5B5) ? 2 : 4;
            // Write a bottom-up 32-bit BMP.
            HANDLE hf = CreateFileA(path, GENERIC_WRITE, 0, nullptr,
                                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hf != INVALID_HANDLE_VALUE) {
                uint32_t w = desc.Width, hgt = desc.Height;
                uint32_t rowPad = (4 - (w * 4) % 4) % 4;
                uint32_t imgSize = (w * 4 + rowPad) * hgt;
                uint8_t hdr[54] = {};
                hdr[0] = 'B'; hdr[1] = 'M';
                *(uint32_t*)(hdr + 2) = 54 + imgSize;
                *(uint32_t*)(hdr + 10) = 54;
                *(uint32_t*)(hdr + 14) = 40;
                *(int32_t*)(hdr + 18) = (int32_t)w;
                *(int32_t*)(hdr + 22) = (int32_t)hgt;
                *(uint16_t*)(hdr + 26) = 1;
                *(uint16_t*)(hdr + 28) = 32;
                *(uint32_t*)(hdr + 34) = imgSize;
                DWORD written = 0;
                WriteFile(hf, hdr, 54, &written, nullptr);
                const uint8_t* bits = (const uint8_t*)lr.pBits;
                std::vector<uint8_t> row(w * 4 + rowPad);
                for (int32_t y = (int32_t)hgt - 1; y >= 0; y--) {
                    const uint8_t* src = bits + (size_t)y * lr.Pitch;
                    if (bpp == 4) {
                        memcpy(row.data(), src, w * 4);
                    } else {
                        const uint16_t* s16 = (const uint16_t*)src;
                        for (uint32_t x = 0; x < w; x++) {
                            uint16_t p = s16[x];
                            row[x * 4 + 0] = (uint8_t)((p & 0x1F) << 3);
                            row[x * 4 + 1] = (uint8_t)(((p >> 5) & 0x3F) << 2);
                            row[x * 4 + 2] = (uint8_t)(((p >> 11) & 0x1F) << 3);
                            row[x * 4 + 3] = 255;
                        }
                    }
                    WriteFile(hf, row.data(), (DWORD)row.size(), &written, nullptr);
                }
                CloseHandle(hf);
                ok = true;
            }
            sys->UnlockRect();
        }
    }
    if (sys) sys->Release();
    rt->Release();
    return ok;
}

void D3DRenderer::SetViewMatrix(const D3DMATRIX& view) {
    m_view = view;  // cached for the sun billboard in RenderSky
    if (m_device)
        m_device->SetTransform(D3DTS_VIEW, &view);
}

void D3DRenderer::SetProjMatrix(const D3DMATRIX& proj) {
    m_proj = proj;  // cached for frustum culling
    if (m_device)
        m_device->SetTransform(D3DTS_PROJECTION, &proj);
}


// ---- Sky (M4) ----
// CClouds::RenderSkyPolys @ 00714650: five flat polys 30 units out along the
// camera's horizontal heading, 1.4x that wide to each side, coloured from
// timecyc.dat (SkyTop / SkyBot). Heights are in the same 30-unit steps:
//   +0.5 .. 0     SkyTop -> SkyBot (the visible gradient)
//    0 .. -0.1    SkyBot
//   -0.1 .. -0.3  SkyBot -> below-horizon grey
//   a SkyTop ceiling at +0.5 and a grey floor at -0.3, front to back.
// Clouds (CClouds::Render, VolumetricCloudsRender) are not drawn yet.

struct SkyVertex {
    float x, y, z;
    DWORD color;
    float u, v;
};
#define SKY_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

static const float SKY_RADIUS = 2500.0f;  // sun distance, inside far plane (3000)

void D3DRenderer::BuildSkyResources() {
    // --- Sun glow texture: 64x64 radial falloff, generated in code ---
    const int SS = 64;
    if (!m_device || m_sunTex) return;
    if (SUCCEEDED(m_device->CreateTexture(SS, SS, 1, 0, D3DFMT_A8R8G8B8,
                                          D3DPOOL_MANAGED, &m_sunTex, nullptr))) {
        D3DLOCKED_RECT lr;
        if (SUCCEEDED(m_sunTex->LockRect(0, &lr, nullptr, 0))) {
            for (int y = 0; y < SS; y++) {
                uint32_t* row = (uint32_t*)((uint8_t*)lr.pBits + y * lr.Pitch);
                for (int x = 0; x < SS; x++) {
                    float dx = (x + 0.5f) / SS * 2.0f - 1.0f;
                    float dy = (y + 0.5f) / SS * 2.0f - 1.0f;
                    float d = sqrtf(dx * dx + dy * dy);
                    float a = (d >= 1.0f) ? 0.0f : powf(1.0f - d, 2.2f);
                    uint32_t av = (uint32_t)(a * 255.0f);
                    row[x] = (av << 24) | 0x00FFF4D6;  // warm white, alpha=falloff
                }
            }
            m_sunTex->UnlockRect(0);
        }
    }
}

void D3DRenderer::RenderSky(float camX, float camY, float camZ) {
    if (!m_device)
        return;
    BuildSkyResources();

    // Sky draws first: no depth, no fog, no culling, no alpha test, no lighting.
    SetLightState(false);
    m_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetTexture(0, nullptr);

    // Camera heading on the ground plane: the view matrix's third column is
    // the camera's forward axis in world space.
    float fx = m_view._13, fy = m_view._23;
    const float fl = sqrtf(fx * fx + fy * fy);
    if (fl > 1e-4f) { fx /= fl; fy /= fl; } else { fx = 0.0f; fy = 1.0f; }
    const float sx = fy * 1.4f, sy = -fx * 1.4f;   // sideways, 1.4x
    auto col = [](const float c[3]) {
        return D3DCOLOR_XRGB((int)c[0], (int)c[1], (int)c[2]);
    };
    const DWORD top = col(m_skyTop), bot = col(m_skyBot), grey = col(m_belowHorizon);
    // One poly: two front points at height h0 (colour c0), two at h1 (c1);
    // `back` sends the second edge behind the camera (ceiling / floor).
    SkyVertex v[5 * 4];
    uint16_t idx[5 * 6];
    int nv = 0, ni = 0;
    auto poly = [&](float h0, DWORD c0, float h1, DWORD c1, bool back) {
        const uint16_t b = (uint16_t)nv;
        const float f1 = back ? -1.0f : 1.0f;
        const float pts[4][3] = {{fx - sx, fy - sy, h0}, {fx + sx, fy + sy, h0},
                                 {f1 * fx - sx, f1 * fy - sy, h1}, {f1 * fx + sx, f1 * fy + sy, h1}};
        for (int i = 0; i < 4; i++)
            v[nv++] = {camX + pts[i][0] * 30.0f, camY + pts[i][1] * 30.0f, camZ + pts[i][2] * 30.0f,
                       i < 2 ? c0 : c1, 0.0f, 0.0f};
        const uint16_t tri[6] = {0, 2, 1, 1, 2, 3};   // SetUpOneSkyPoly order
        for (uint16_t t : tri) idx[ni++] = (uint16_t)(b + t);
    };
    poly(0.5f, top, 0.0f, bot, false);
    poly(0.0f, bot, -0.1f, bot, false);
    poly(-0.1f, bot, -0.3f, grey, false);
    poly(0.5f, top, 0.5f, top, true);
    poly(-0.3f, grey, -0.3f, grey, true);
    D3DMATRIX world = MatrixIdentity();
    m_device->SetTransform(D3DTS_WORLD, &world);
    m_device->SetFVF(SKY_FVF);
    m_device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, nv, ni / 3, idx, D3DFMT_INDEX16,
                                     v, sizeof(SkyVertex));

    // Sun (CCoronas::DoSunAndMoon): a SunCore sprite while the sun is above
    // z = -0.1 and a larger SunCorona glow once it is above the horizon, both
    // camera-facing and additive, depth-tested so buildings occlude them.
    float sunLen = sqrtf(m_sunDir[0]*m_sunDir[0] + m_sunDir[1]*m_sunDir[1] + m_sunDir[2]*m_sunDir[2]);
    if (m_sunTex && sunLen > 0.0001f) {
        const float sdx = m_sunDir[0]/sunLen, sdy = m_sunDir[1]/sunLen, sdz = m_sunDir[2]/sunLen;
        const float dist = SKY_RADIUS * 0.94f;
        const float spx = camX + sdx*dist, spy = camY + sdy*dist, spz = camZ + sdz*dist;
        // Camera right/up in world space = columns of the view matrix.
        const float rx = m_view._11, ry = m_view._21, rz = m_view._31;
        const float ux = m_view._12, uy = m_view._22, uz = m_view._32;
        SkyVertex q[8];
        int quads = 0;
        auto sprite = [&](const float c[3], float angle) {
            const float half = angle * dist;
            const DWORD col = D3DCOLOR_COLORVALUE(c[0], c[1], c[2], 1.0f);
            const float corner[4][2] = {{-1, 1}, {1, 1}, {-1, -1}, {1, -1}};
            for (int i = 0; i < 4; i++) {
                SkyVertex& v = q[quads * 4 + i];
                v.x = spx + (rx * corner[i][0] + ux * corner[i][1]) * half;
                v.y = spy + (ry * corner[i][0] + uy * corner[i][1]) * half;
                v.z = spz + (rz * corner[i][0] + uz * corner[i][1]) * half;
                v.color = col;
                v.u = corner[i][0] > 0 ? 1.0f : 0.0f;
                v.v = corner[i][1] > 0 ? 0.0f : 1.0f;
            }
            quads++;
        };
        if (sdz > 0.0f) sprite(m_sunCorona, m_sunCoronaSize);
        if (sdz > -0.1f) sprite(m_sunCol, m_sunCoreSize);
        m_device->SetRenderState(D3DRS_ZENABLE, TRUE);
        m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        m_device->SetTexture(0, m_sunTex);
        D3DMATRIX ident = MatrixIdentity();
        m_device->SetTransform(D3DTS_WORLD, &ident);
        m_device->SetFVF(SKY_FVF);
        for (int i = 0; i < quads; i++)
            m_device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q + i * 4, sizeof(SkyVertex));
    }

    // Restore engine defaults. FOGENABLE stays FALSE here; main.cpp enables
    // it per-mode right after RenderSky.
    SetLightState(m_lightingReady && m_lightingAllowed);
    m_device->SetRenderState(D3DRS_ZENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_device->SetTexture(0, nullptr);
}

// Shared glow render state: additive, depth-tested, no z-write/cull/alpha-test.
// Restores the world defaults afterwards (same block as RenderSky's tail).
void D3DRenderer::drawGlowBatch(const void* verts, int quadCount) {
    if (!m_device || quadCount <= 0 || !m_sunTex) return;
    SetLightState(false);
    m_device->SetRenderState(D3DRS_ZENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);  // additive
    m_device->SetTexture(0, m_sunTex);
    D3DMATRIX ident = MatrixIdentity();
    m_device->SetTransform(D3DTS_WORLD, &ident);
    m_device->SetFVF(SKY_FVF);
    m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, quadCount * 2, verts, sizeof(SkyVertex));
    // Restore world defaults.
    SetLightState(m_lightingReady && m_lightingAllowed);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_device->SetTexture(0, nullptr);
}

void D3DRenderer::DrawCoronas(const GlowSprite* s, int count) {
    if (!m_device || !m_sunTex || count <= 0) return;
    // Camera right/up in WORLD space = columns of the view matrix (its inverse
    // rotation). (Rows would be world axes in camera space, which skews
    // off-centre billboards into streaks.)
    float rx = m_view._11, ry = m_view._21, rz = m_view._31;
    float ux = m_view._12, uy = m_view._22, uz = m_view._32;
    std::vector<SkyVertex> v;
    v.reserve((size_t)count * 6);
    for (int i = 0; i < count; i++) {
        const GlowSprite& g = s[i];
        float h = g.size;
        auto mk = [&](float sx, float sy, float u, float vv) {
            SkyVertex q;
            q.x = g.x + rx * sx * h + ux * sy * h;
            q.y = g.y + ry * sx * h + uy * sy * h;
            q.z = g.z + rz * sx * h + uz * sy * h;
            q.color = g.color; q.u = u; q.v = vv; return q;
        };
        SkyVertex a = mk(-1,  1, 0, 0), b = mk(1,  1, 1, 0);
        SkyVertex c = mk(-1, -1, 0, 1), d = mk(1, -1, 1, 1);
        v.push_back(a); v.push_back(b); v.push_back(c);
        v.push_back(c); v.push_back(b); v.push_back(d);
    }
    drawGlowBatch(v.data(), count);
}

void D3DRenderer::DrawSkySprites(const SkySprite* s, int count, IDirect3DTexture9* tex) {
    if (!m_device || !s || count <= 0 || !tex) return;
    const float rx = m_view._11, ry = m_view._21, rz = m_view._31;   // camera right
    const float ux = m_view._12, uy = m_view._22, uz = m_view._32;   // camera up
    std::vector<SkyVertex> v;
    v.reserve((size_t)count * 6);
    for (int i = 0; i < count; i++) {
        const SkySprite& p = s[i];
        auto mk = [&](float sx, float sy, float u, float vv) {
            SkyVertex q;
            q.x = p.x + (rx * sx * p.halfW + ux * sy * p.halfH);
            q.y = p.y + (ry * sx * p.halfW + uy * sy * p.halfH);
            q.z = p.z + (rz * sx * p.halfW + uz * sy * p.halfH);
            q.color = p.color; q.u = u; q.v = vv;
            return q;
        };
        const SkyVertex a = mk(-1, 1, 0, 0), b = mk(1, 1, 1, 0), c = mk(-1, -1, 0, 1), d = mk(1, -1, 1, 1);
        v.push_back(a); v.push_back(b); v.push_back(c);
        v.push_back(c); v.push_back(b); v.push_back(d);
    }
    SetLightState(false);
    m_device->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
    m_device->SetTexture(0, tex);
    D3DMATRIX ident = MatrixIdentity();
    m_device->SetTransform(D3DTS_WORLD, &ident);
    m_device->SetFVF(SKY_FVF);
    m_device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)count * 2, v.data(), sizeof(SkyVertex));
    // Restore world defaults (fog is re-enabled by main.cpp per frame).
    SetLightState(m_lightingReady && m_lightingAllowed);
    m_device->SetRenderState(D3DRS_ZENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_alphaBlend = false;
    m_device->SetTexture(0, nullptr);
}

void D3DRenderer::DrawBlendedTriangles(const ColorTexVertex* verts, uint32_t vertexCount,
                                       const uint16_t* indices, uint32_t indexCount,
                                       IDirect3DTexture9* tex) {
    if (!m_device || !verts || !indices || vertexCount < 3 || indexCount < 3) return;
    SetLightState(false);
    m_device->SetRenderState(D3DRS_ZENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    m_device->SetTexture(0, tex);
    D3DMATRIX ident = MatrixIdentity();
    m_device->SetTransform(D3DTS_WORLD, &ident);
    m_device->SetFVF(SKY_FVF);
    m_device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, vertexCount, indexCount / 3,
                                     indices, D3DFMT_INDEX16, verts, sizeof(ColorTexVertex));
    // Restore world defaults.
    m_device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    SetLightState(m_lightingReady && m_lightingAllowed);
    m_device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
    m_device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    m_device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    m_alphaBlend = false;
    m_device->SetTexture(0, nullptr);
}

void D3DRenderer::SetSkyColors(float topR, float topG, float topB,
                               float botR, float botG, float botB) {
    // Called every frame with a timecyc sample; skip the work when unchanged.
    if (m_skyTop[0] == topR && m_skyTop[1] == topG && m_skyTop[2] == topB &&
        m_skyBot[0] == botR && m_skyBot[1] == botG && m_skyBot[2] == botB)
        return;
    m_skyTop[0] = topR; m_skyTop[1] = topG; m_skyTop[2] = topB;
    m_skyBot[0] = botR; m_skyBot[1] = botG; m_skyBot[2] = botB;
    // CTimeCycle::m_BelowHorizonGrey is derived from SkyBot; the exact blend
    // is lost in the decomp (float->int conversions), so this halves the
    // way from SkyBot to its own grey.
    const float g = (botR + botG + botB) / 3.0f;
    for (int i = 0; i < 3; i++) m_belowHorizon[i] = (m_skyBot[i] + g) * 0.5f;
}

void D3DRenderer::SetSun(float dirX, float dirY, float dirZ, const float core[3],
                         const float corona[3], float coreSize, float coronaSize) {
    m_sunDir[0] = dirX; m_sunDir[1] = dirY; m_sunDir[2] = dirZ;
    for (int i = 0; i < 3; i++) {
        m_sunCol[i] = core[i] / 255.0f;
        m_sunCorona[i] = corona[i] / 255.0f;
    }
    m_sunCoreSize = coreSize;
    m_sunCoronaSize = coronaSize;
}

// World lighting: ambient colour -> D3DRS_AMBIENT (multiplied by the white
// material ambient), directional colour -> D3DLIGHT9 light 0. The light
// travels from the sun towards the scene, so its Direction is -toSun.
void D3DRenderer::SetWorldLighting(float ambR, float ambG, float ambB,
                                   float dirR, float dirG, float dirB,
                                   float toSunX, float toSunY, float toSunZ,
                                   float dirScale) {
    if (!m_device)
        return;
    float amb[3] = {ambR / 255.0f, ambG / 255.0f, ambB / 255.0f};
    float dir[3] = {dirR / 255.0f * dirScale, dirG / 255.0f * dirScale,
                    dirB / 255.0f * dirScale};
    for (int i = 0; i < 3; i++) {
        if (amb[i] < 0.0f) amb[i] = 0.0f;
        if (amb[i] > 1.0f) amb[i] = 1.0f;
        if (dir[i] < 0.0f) dir[i] = 0.0f;
        if (dir[i] > 1.0f) dir[i] = 1.0f;
    }
    m_lightAmb[0] = amb[0]; m_lightAmb[1] = amb[1]; m_lightAmb[2] = amb[2];
    m_lightDir[0] = dir[0]; m_lightDir[1] = dir[1]; m_lightDir[2] = dir[2];
    m_lightingReady = true;

    m_device->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_COLORVALUE(amb[0], amb[1], amb[2], 1.0f));

    D3DLIGHT9 light;
    ZeroMemory(&light, sizeof(light));
    light.Type = D3DLIGHT_DIRECTIONAL;
    light.Diffuse.r = dir[0]; light.Diffuse.g = dir[1]; light.Diffuse.b = dir[2];
    light.Diffuse.a = 1.0f;
    light.Ambient.r = light.Ambient.g = light.Ambient.b = 0.0f;
    light.Ambient.a = 1.0f;
    light.Specular.r = light.Specular.g = light.Specular.b = 0.0f;
    light.Specular.a = 1.0f;
    float len = sqrtf(toSunX * toSunX + toSunY * toSunY + toSunZ * toSunZ);
    if (len < 0.0001f) { toSunX = 0.0f; toSunY = 0.0f; toSunZ = 1.0f; len = 1.0f; }
    light.Direction.x = -toSunX / len;
    light.Direction.y = -toSunY / len;
    light.Direction.z = -toSunZ / len;
    m_device->SetLight(0, &light);
    m_device->LightEnable(0, TRUE);
    SetLightState(m_lightingReady && m_lightingAllowed);
}

void D3DRenderer::SetLightingEnabled(bool on) {
    m_lightingAllowed = on;
    m_prelitStamp++;  // prelit meshes re-bake (white when off)
    SetLightState(on && m_lightingReady);
}

void D3DRenderer::SetPrelitShading(float nightFactor,
                                   float tintR, float tintG, float tintB) {
    auto clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    m_prelitBalance = clamp01(nightFactor);
    m_prelitTint[0] = clamp01(tintR);
    m_prelitTint[1] = clamp01(tintG);
    m_prelitTint[2] = clamp01(tintB);
}

// ---- Math helpers (row-vector convention matching D3D) ----

D3DMATRIX MatrixIdentity() {
    D3DMATRIX m = {};
    m._11 = m._22 = m._33 = m._44 = 1.0f;
    return m;
}

D3DMATRIX MatrixPerspectiveFov(float fovY, float aspect, float zn, float zf) {
    D3DMATRIX m = {};
    float yScale = 1.0f / tanf(fovY * 0.5f);
    float xScale = yScale / aspect;
    // SA's world is right-handed (+X east, +Y north, +Z up) but D3D is
    // left-handed, so MatrixLookAt's (up x forward) basis lands world EAST on
    // the screen's LEFT: the whole 3D image came out horizontally mirrored
    // (sign text read reversed). Negating the horizontal scale reflects the
    // image back to SA's map layout. That reflection also negates the
    // screen-space winding of every triangle, so world cull modes are
    // D3DCULL_CW (see Init) to keep the same faces visible.
    m._11 = -xScale;
    m._22 = yScale;
    m._33 = zf / (zf - zn);
    m._34 = 1.0f;
    m._43 = -zn * zf / (zf - zn);
    return m;
}

D3DMATRIX MatrixLookAt(float ex, float ey, float ez,
                       float tx, float ty, float tz,
                       float ux, float uy, float uz) {
    // M3 fix: zaxis must be normalize(target - eye) for a LEFT-handed view
    // (D3D camera looks down +Z; XMMatrixLookAtLH uses Focus-Eye). The old
    // (eye-target) form is the RH variant and put everything behind the
    // camera -> blank screen in M1/M2 (never visually verified until M3).
    float zx = tx - ex, zy = ty - ey, zz = tz - ez;
    float zl = sqrtf(zx*zx + zy*zy + zz*zz);
    zx /= zl; zy /= zl; zz /= zl;
    // xaxis = normalize(cross(up, zaxis))
    float xx = uy*zz - uz*zy, xy = uz*zx - ux*zz, xz = ux*zy - uy*zx;
    float xl = sqrtf(xx*xx + xy*xy + xz*xz);
    xx /= xl; xy /= xl; xz /= xl;
    // yaxis = cross(zaxis, xaxis)
    float yx = zy*xz - zz*xy, yy = zz*xx - zx*xz, yz = zx*xy - zy*xx;

    D3DMATRIX m = {};
    m._11 = xx; m._12 = yx; m._13 = zx; m._14 = 0;
    m._21 = xy; m._22 = yy; m._23 = zy; m._24 = 0;
    m._31 = xz; m._32 = yz; m._33 = zz; m._34 = 0;
    m._41 = -(xx*ex + xy*ey + xz*ez);
    m._42 = -(yx*ex + yy*ey + yz*ez);
    m._43 = -(zx*ex + zy*ey + zz*ez);
    m._44 = 1.0f;
    return m;
}
