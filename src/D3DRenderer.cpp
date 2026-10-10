#include "D3DRenderer.h"
#include <cmath>
#include <cstring>
#include <vector>

struct TestVertex {
    float x, y, z;
    DWORD color;
};
#define TEST_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)

D3DRenderer::D3DRenderer()
    : m_d3d(nullptr), m_device(nullptr), m_width(0), m_height(0) {
    m_adapterDesc[0] = '\0';
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
    pp.AutoDepthStencilFormat = D3DFMT_D16;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // Try every adapter in turn: virtual display drivers (RDP/vGPU shims)
    // may not support D3D9 HAL, so D3DADAPTER_DEFAULT is not reliable.
    UINT adapterCount = m_d3d->GetAdapterCount();
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
    if (!m_device) {
        m_d3d->Release();
        m_d3d = nullptr;
        return false;
    }

    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    // M3: texture sampling for stage 0
    m_device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    m_device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    m_device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    m_device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    return true;
}

void D3DRenderer::Shutdown() {
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
    if (m_d3d) {
        m_d3d->Release();
        m_d3d = nullptr;
    }
}

void D3DRenderer::BeginFrame(float r, float g, float b) {
    if (!m_device)
        return;
    DWORD clearColor = D3DCOLOR_COLORVALUE(r, g, b, 1.0f);
    m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clearColor, 1.0f, 0);
    m_device->BeginScene();
}

void D3DRenderer::DrawTestTriangle() {
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

D3DRenderMesh* D3DRenderer::CreateMesh(const MeshVertex* verts, uint32_t numVerts,
                                       const uint16_t* indices, uint32_t numIndices) {
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
        mesh->vb->Unlock();
    }
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
    return mesh;
}

void D3DRenderer::DrawMesh(D3DRenderMesh* mesh) {
    if (!m_device || !mesh || !mesh->vb || !mesh->ib)
        return;
    m_device->SetFVF(MESH_FVF);
    m_device->SetStreamSource(0, mesh->vb, 0, sizeof(MeshVertex));
    m_device->SetIndices(mesh->ib);
    m_device->SetTexture(0, mesh->texture);  // nullptr = untextured
    m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                   mesh->vertexCount, 0,
                                   mesh->indexCount / 3);
    m_device->SetTexture(0, nullptr);
}

void D3DRenderer::DestroyMesh(D3DRenderMesh* mesh) {
    if (!mesh)
        return;
    if (mesh->vb) mesh->vb->Release();
    if (mesh->ib) mesh->ib->Release();
    if (mesh->texture) mesh->texture->Release();
    delete mesh;
}

IDirect3DTexture9* D3DRenderer::CreateTexture(uint32_t width, uint32_t height,
                                              D3DFORMAT fmt, const void* data,
                                              uint32_t dataSize) {
    if (!m_device || !data || !width || !height || !dataSize)
        return nullptr;
    IDirect3DTexture9* tex = nullptr;
    if (FAILED(m_device->CreateTexture(width, height, 1, 0, fmt,
                                       D3DPOOL_MANAGED, &tex, nullptr)))
        return nullptr;
    D3DLOCKED_RECT lr = {};
    bool ok = false;
    if (SUCCEEDED(tex->LockRect(0, &lr, nullptr, 0))) {
        const uint8_t* src = (const uint8_t*)data;
        uint8_t* dst = (uint8_t*)lr.pBits;
        uint32_t rowBytes = 0;
        uint32_t numRows = 0;
        if (fmt == D3DFMT_DXT1 || fmt == D3DFMT_DXT3) {
            // DXT stores 4x4-texel blocks; copy whole block rows so a
            // driver pitch wider than the data still works.
            uint32_t blocksX = (width + 3) / 4;
            uint32_t blocksY = (height + 3) / 4;
            uint32_t blockBytes = (fmt == D3DFMT_DXT1) ? 8 : 16;
            rowBytes = blocksX * blockBytes;
            numRows = blocksY;
        } else {
            // Uncompressed: assume 4 bytes/pixel (A8R8G8B8).
            rowBytes = width * 4;
            numRows = height;
        }
        uint32_t need = rowBytes * numRows;
        uint32_t have = dataSize < need ? dataSize : need;
        uint32_t rows = rowBytes ? have / rowBytes : 0;
        for (uint32_t y = 0; y < rows; y++)
            memcpy(dst + (size_t)y * lr.Pitch, src + (size_t)y * rowBytes, rowBytes);
        ok = rows > 0;
        tex->UnlockRect(0);
    }
    if (!ok) {
        tex->Release();
        return nullptr;
    }
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
    if (m_device)
        m_device->SetTransform(D3DTS_VIEW, &view);
}

void D3DRenderer::SetProjMatrix(const D3DMATRIX& proj) {
    if (m_device)
        m_device->SetTransform(D3DTS_PROJECTION, &proj);
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
    m._11 = xScale;
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
