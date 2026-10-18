// Fe2d.cpp - see Fe2d.h.

#include "Fe2d.h"

void Fe2d::Begin(IDirect3DDevice9* dev) {
    m_dev = dev;
    m_count = 0;
    m_ox = m_oy = 0;
    if (!m_dev) return;
    // State blocks must not survive a device Reset, so capture per Begin.
    if (FAILED(m_dev->CreateStateBlock(D3DSBT_ALL, &m_saved))) m_saved = nullptr;
    m_dev->SetFVF(kFvf);
    m_dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    m_dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    m_dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    m_dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    m_dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    m_dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    m_dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    m_dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    m_dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    m_dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    m_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    m_dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    m_dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    m_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    m_dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    m_dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    m_dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    m_dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);   // frontend
    m_dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);   // sprites clamp
    m_dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    m_dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    m_dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
}

void Fe2d::End() {
    if (!m_dev) return;
    if (m_saved) { m_saved->Apply(); m_saved->Release(); m_saved = nullptr; }
    m_dev = nullptr;
}

void Fe2d::QueueTex(float x0, float y0, float x1, float y1, uint32_t argb,
                    float u0, float v0, float u1, float v1) {
    if (m_count + 6 > kMaxQuads * 6) return;   // caller flushes per string
    // -0.5: D3D9 pixel centres, so texels map 1:1 and edges stay crisp.
    x0 += m_ox - 0.5f; y0 += m_oy - 0.5f; x1 += m_ox - 0.5f; y1 += m_oy - 0.5f;
    V a{x0, y0, 0, 1, argb, u0, v0}, b{x1, y0, 0, 1, argb, u1, v0};
    V c{x0, y1, 0, 1, argb, u0, v1}, d{x1, y1, 0, 1, argb, u1, v1};
    V* p = &m_buf[m_count];
    p[0] = a; p[1] = b; p[2] = c; p[3] = c; p[4] = b; p[5] = d;
    m_count += 6;
}

void Fe2d::Flush(IDirect3DTexture9* tex) {
    if (!m_dev || m_count == 0) { m_count = 0; return; }
    m_dev->SetTexture(0, tex);
    if (!tex) {   // untextured: colour/alpha straight from the vertex
        m_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG2);
        m_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG2);
    }
    m_dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, m_count / 3, m_buf, sizeof(V));
    if (!tex) {
        m_dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        m_dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    }
    m_count = 0;
}

void Fe2d::DrawRect(float x0, float y0, float x1, float y1, uint32_t argb) {
    Flush(nullptr);   // never mix with a pending textured batch
    QueueTex(x0, y0, x1, y1, argb, 0, 0, 0, 0);
    Flush(nullptr);
}

void Fe2d::DrawTex(IDirect3DTexture9* tex, float x0, float y0, float x1, float y1,
                   uint32_t argb, float u0, float v0, float u1, float v1) {
    QueueTex(x0, y0, x1, y1, argb, u0, v0, u1, v1);
    Flush(tex);
}
