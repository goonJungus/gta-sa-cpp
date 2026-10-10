#include "D3DRenderer.h"
#include <cmath>

struct TestVertex {
    float x, y, z;
    DWORD color;
};
#define TEST_FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE)

D3DRenderer::D3DRenderer()
    : m_d3d(nullptr), m_device(nullptr), m_width(0), m_height(0) {}

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
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferWidth = width;
    pp.BackBufferHeight = height;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D16;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hr = m_d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &pp,
        &m_device);
    if (FAILED(hr)) {
        hr = m_d3d->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &pp,
            &m_device);
        if (FAILED(hr)) {
            m_d3d->Release();
            m_d3d = nullptr;
            return false;
        }
    }

    m_device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    m_device->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
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
    m_device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
                                   mesh->vertexCount, 0,
                                   mesh->indexCount / 3);
}

void D3DRenderer::DestroyMesh(D3DRenderMesh* mesh) {
    if (!mesh)
        return;
    if (mesh->vb) mesh->vb->Release();
    if (mesh->ib) mesh->ib->Release();
    delete mesh;
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
    // zaxis = normalize(eye - target)
    float zx = ex - tx, zy = ey - ty, zz = ez - tz;
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
