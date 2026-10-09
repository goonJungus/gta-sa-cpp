// CSprite2d.cpp - GTA SA 1.0 clean-room C++ conversion
// Method bodies filled from src/CSprite2d/*.c (Ghidra decomp of 1.0).
// Decomp is authoritative where it diverges from gta-reversed (noted inline).
// RW/D3D9 calls are NOT implemented (render layer not ported) - they are
// preserved as no-op shims below, each marked TODO(port).

#include "CSprite2d.h"
#include "CFont.h"   // DrawBarChart percentage text

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

// ---- dependency shims (TODO(port): replace when owning subsystems convert) ----
// RenderWare layer (not ported). Numeric render-state ids are from the decomp's
// fpRenderStateSet calls, mapped via the gta-reversed cross-check:
//   1  = rwRENDERSTATETEXTURERASTER   0xC/12 = rwRENDERSTATEVERTEXALPHAENABLE
//   7  = rwRENDERSTATESHADEMODE       9      = rwRENDERSTATETEXTUREFILTER (2 = rwFILTERLINEAR)
//   10 = rwRENDERSTATESRCBLEND        0xB/11 = rwRENDERSTATEDESTBLEND
//        (5 = rwBLENDSRCALPHA, 6 = rwBLENDINVSRCALPHA)
// Primitives: CVisibilityPlugins::unk_00734e90(5, v, 4) = RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, v, 4)
//             CVisibilityPlugins::unk_00734ea0(3, v, nv, i, ni) = RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, ...)
//             fpIm2DRenderTriangle(v, 3, 2, 1, 0) = RwIm2DRenderTriangle(...)
struct RwRaster;
namespace RenderPort {
    inline void SetTextureRaster(RwRaster* /*raster*/) { /* TODO(port): RwRenderStateSet(rwRENDERSTATETEXTURERASTER, raster) */ }
    inline void SetVertexAlphaEnable(bool /*enable*/)  { /* TODO(port): RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, enable) */ }
    inline void SetShadeModeFlat()                     { /* TODO(port): RwRenderStateSet(rwRENDERSTATESHADEMODE, rwSHADEMODEFLAT) */ }
    inline void SetTextureFilterLinear()               { /* TODO(port): RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, rwFILTERLINEAR) */ }
    inline void SetXluBlend()                          { /* TODO(port): SRCBLEND=rwBLENDSRCALPHA, DESTBLEND=rwBLENDINVSRCALPHA */ }
    inline void RenderTriFan(RwIm2DVertex* /*verts*/, int32_t /*count*/) { /* TODO(port): RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, ...) */ }
    inline void RenderTriangle(RwIm2DVertex* /*verts*/) { /* TODO(port): RwIm2DRenderTriangle(verts, 3, 2, 1, 0) */ }
    inline void RenderIndexed(RwIm2DVertex* /*verts*/, int32_t /*nVerts*/, RwImVertexIndex* /*idx*/, int32_t /*nIdx*/) {
        /* TODO(port): RwIm2DRenderIndexedPrimitive(rwPRIMTYPETRILIST, ...) */
    }
} // namespace RenderPort

// RwTexture is opaque in RenderTypes.h (RW layer not ported); the decomp pokes
// ->raster and ->filterAddressing directly. Shim declarations keep the call
// shape; bodies are TODO(port).
// TODO(port): real RwTexture/RwRaster with the RenderWare layer.
RwRaster* RwTextureGetRaster(RwTexture* tex);
int32_t   RwRasterGetWidth(RwRaster* raster);
int32_t   RwRasterGetHeight(RwRaster* raster);
RwTexture* RwTextureRead(const char* name, const char* maskName); // RenderWare::str_null_007f3ac0
void       RwTextureDestroy(RwTexture* tex);

// CGeneral::GetRandomNumber (CGeneral not converted). Decomp labels the target
// CGeneral::unk_00821b40; no-arg calls match GetRandomNumber semantics.
// TODO(port): real CGeneral.
namespace CGeneral {
    int32_t GetRandomNumber();
}

// RsGlobal screen dimensions (RW layer not ported). Used by DrawBarChart's
// black-border insets: 2 * (maxH * 0.002232143) vertical, 2 * (maxW * 0.0015625)
// horizontal. TODO(port): real RsGlobal.
namespace RsGlobalPort {
    extern int32_t maximumWidth;
    extern int32_t maximumHeight;
}

// GXT helpers (text subsystem not converted). TODO(port).
void AsciiToGxtChar(const char* src, GxtChar* dst);

// Static member definitions.
// Original GTA SA 1.0 addresses (from gta-reversed StaticRef) kept as comments.
// TODO: re-resolve these for the clean-room build.
int32_t CSprite2d::nextBufferIndex = 0;  // 0xC80458
int32_t CSprite2d::nextBufferVertex = 0; // 0xC8045C
float   CSprite2d::NearScreenZ = 0.0f;   // 0xC80460
float   CSprite2d::RecipNearClip = 0.0f; // 0xC80464
std::array<RwIm2DVertex, 8> CSprite2d::maVertices{}; // 0xC80468

// Temp vertex/index buffers. Decomp: the vertex buffer aliases
// CGlass::ReflectionPolyVertexBuffer[0x200] (0x5FD verts, stride 0x1C); the
// index buffer is the file-scope aTempBufferIndices (writes up to 0xFFB + 5).
// TODO(port): restore the original aliasing when CGlass converts.
static std::array<RwIm2DVertex, 0x5FD>  s_aTempBufferVertices{};
static std::array<RwImVertexIndex, 0x1000> s_aTempBufferIndices{};

// Sine LUT for DrawCircleAtNearClip. The binary indexes a 256-entry float table
// (at 0xBB3E00, unreadable in the packed file - SecuROM .data), advancing
// (360/angle)*256/360 entries per triangle with a +64 (cosine) offset for X.
// A standard sine table reproduces the circle; VERIFY against a live image if
// exactness matters.
static float s_sinLut[256];
static bool  s_sinLutInit = false;
static void InitSinLut() {
    if (s_sinLutInit) return;
    for (int i = 0; i < 256; ++i)
        s_sinLut[i] = std::sin(float(i) * (2.0f * 3.14159265f / 256.0f));
    s_sinLutInit = true;
}

// 1.0 float->int casts compile to FPU fistp (round-to-nearest), unlike modern
// (int) casts (truncate). The decomp's CGeneral::unk_00821b40 calls are these
// fistp helpers (verified by disassembly: 0x821B40 is __ftol-like).
static int32_t FloatToInt(float v) {
    return int32_t(std::nearbyint(v));
}

// CCredits' byte-fill helper (decomp CCredits::unk_007170c0): fills a CRGBA
// from four bytes. Inlined here since CCredits isn't converted.
static CRGBA MakeRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return CRGBA{ r, g, b, a };
}

// RwIm2DVertex::emissiveColor is RwRGBA, not CRGBA.
static RwRGBA ToRwRGBA(const CRGBA& c) {
    return RwRGBA{ c.r, c.g, c.b, c.a };
}

CSprite2d::CSprite2d() {
    // src/CSprite2d/Constructor_00727230.c
    m_pTexture = nullptr;
}

CSprite2d::~CSprite2d() {
    // src/CSprite2d/Destructor_007281e0.c (via Delete)
    Delete();
}

void CSprite2d::Delete() {
    // src/CSprite2d/Delete_00727240.c
    if (m_pTexture) {
        RwTextureDestroy(m_pTexture); // TODO(port)
        m_pTexture = nullptr;
    }
}

void CSprite2d::SetTexture(const char* name) {
    // src/CSprite2d/SetTexture_00727270.c
    Delete();
    if (name)
        m_pTexture = RwTextureRead(name, nullptr); // TODO(port)
}

void CSprite2d::SetTexture(const char* name, const char* maskName) {
    // src/CSprite2d/SetTexture_007272b0.c
    // NOTE: unlike the single-arg overload, 1.0 does NOT destroy the old
    // texture here (a leak in the original). Kept faithful.
    if (name && maskName)
        m_pTexture = RwTextureRead(name, maskName); // TODO(port)
}

void CSprite2d::SetAddressingUV(RwTextureAddressMode modeU, RwTextureAddressMode modeV) {
    // src/CSprite2d/SetAddressingUV_007272e0.c
    // Decomp pokes RwTexture::filterAddressing bits directly:
    //   U: filterAddressing = filterAddressing ^ ((modeU << 8) ^ filterAddressing) & 0xF00;
    //   V: filterAddressing = filterAddressing ^ ((modeV << 12) ^ filterAddressing) & 0xF000;
    // TODO(port): real addressing set with the RW layer (RwTextureSetAddressingU/V).
    (void)modeU;
    (void)modeV;
}

void CSprite2d::SetAddressing(RwTextureAddressMode modeUV) {
    // src/CSprite2d/SetAddressing_00727320.c
    // Decomp: filterAddressing = (((modeUV & 0xF) << 4 | (modeUV & 0xF)) << 8) | (filterAddressing & 0xFFFF00FF);
    // TODO(port): real addressing set with the RW layer (RwTextureSetAddressing).
    (void)modeUV;
}

void CSprite2d::SetRenderState() {
    // src/CSprite2d/SetRenderState_00727b30.c
    if (!m_pTexture)
        RenderPort::SetTextureRaster(nullptr);
    else
        RenderPort::SetTextureRaster(RwTextureGetRaster(m_pTexture)); // TODO(port)
}

void CSprite2d::Draw(float x, float y, float width, float height, const CRGBA& color) {
    // src/CSprite2d/Draw_007282c0.c
    // NOTE: decomp does NOT delegate to Draw(posn, color) (gta-reversed does).
    CRect posn{ x, y, x + width, y + height };
    SetVertices(posn, color, color, color, color);
    SetRenderState();
    RenderPort::RenderTriFan(maVertices.data(), 4);
    RenderPort::SetTextureRaster(nullptr);
}

void CSprite2d::Draw(const CRect& posn, const CRGBA& color) {
    // src/CSprite2d/Draw_00728350.c
    SetVertices(posn, color, color, color, color);
    SetRenderState();
    RenderPort::RenderTriFan(maVertices.data(), 4);
    RenderPort::SetTextureRaster(nullptr);
}

void CSprite2d::DrawWithBilinearOffset(const CRect& posn, const CRGBA& color) {
    // src/CSprite2d/DrawWithBilinearOffset_007283b0.c
    // NOTE: 1.0 dereferences m_pTexture->raster with NO null check here (would
    // crash on a null texture). Kept faithful via the shim chain.
    SetVertices(posn, color, color, color, color);
    RwRaster* raster = RwTextureGetRaster(m_pTexture); // TODO(port)
    OffsetTexCoordForBilinearFiltering(float(RwRasterGetWidth(raster)), float(RwRasterGetHeight(raster))); // TODO(port)
    SetRenderState();
    RenderPort::RenderTriFan(maVertices.data(), 4);
}

void CSprite2d::Draw(const CRect& posn, const CRGBA& color, float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4) {
    // src/CSprite2d/Draw_00728420.c (this overload: CRect + explicit UVs)
    SetVertices(posn, color, color, color, color, u1, v1, u2, v2, u3, v3, u4, v4);
    SetRenderState();
    RenderPort::RenderTriFan(maVertices.data(), 4);
    RenderPort::SetTextureRaster(nullptr);
}

void CSprite2d::Draw(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4) {
    // src/CSprite2d/Draw_007284b0.c
    SetVertices(posn, color1, color2, color3, color4);
    SetRenderState();
    RenderPort::RenderTriFan(maVertices.data(), 4);
    RenderPort::SetTextureRaster(nullptr);
}

void CSprite2d::Draw(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const CRGBA& color) {
    // src/CSprite2d/Draw_00728520.c
    SetVertices(x1, y1, x2, y2, x3, y3, x4, y4, color, color, color, color);
    SetRenderState();
    RenderPort::RenderTriFan(maVertices.data(), 4);
    RenderPort::SetTextureRaster(nullptr);
}

void CSprite2d::SetRecipNearClip() {
    // src/CSprite2d/SetRecipNearClip_00727260.c - empty in 1.0 (NOP).
}

void CSprite2d::InitPerFrame() {
    // src/CSprite2d/InitPerFrame_00727350.c
    nextBufferVertex = 0;
    nextBufferIndex = 0;
    // Decomp: RecipNearClip = 1.0f / *(float*)(RsGlobal + 0x80); gta-reversed:
    // RecipNearClip = 1.0f / RwCameraGetNearClipPlane(Scene.m_pRwCamera);
    // NearScreenZ = <dOpenDevice>.zBufferNear.
    // TODO(port): real camera near-plane / device zBufferNear with the RW layer.
}

bool CSprite2d::IsVertexBufferEmpty() {
    // src/CSprite2d/IsVertexBufferEmpty_00727390.c
    return nextBufferVertex == 0;
}

bool CSprite2d::IsVertexBufferFull() {
    // src/CSprite2d/IsVertexBufferFull_007273a0.c
    // NOTE: decomp uses >= (gta-reversed uses > with named constants).
    return nextBufferVertex >= 0x5FD || nextBufferIndex >= 0xFFB;
}

void CSprite2d::RenderVertexBuffer() {
    // src/CSprite2d/RenderVertexBuffer_007273d0.c
    if (nextBufferVertex <= 0)
        return;
    RenderPort::SetTextureFilterLinear();
    RenderPort::RenderIndexed(s_aTempBufferVertices.data(), nextBufferVertex,
                             s_aTempBufferIndices.data(), nextBufferIndex);
    nextBufferVertex = 0;
    nextBufferIndex = 0;
}

void CSprite2d::SetVertices(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4) {
    // src/CSprite2d/SetVertices_00727420.c
    // NOTE: decomp writes the 1/1024 UV offsets literally; gta-reversed
    // delegates to the UV overload with offset = 1/1024. Same result.
    constexpr float offset = 1.0f / 1024.0f; // 0.0009765625
    SetVertices(posn, color1, color2, color3, color4,
                offset, offset,
                1.0f + offset, offset,
                offset, 1.0f + offset,
                1.0f + offset, 1.0f + offset);
}

void CSprite2d::SetVertices(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4) {
    // src/CSprite2d/SetVertices_00727590.c
    // Vertex order in the decomp is (x3,y3,c3) (x4,y4,c4) (x2,y2,c2) (x1,y1,c1).
    maVertices[0] = RwIm2DVertex{ x3, y3, NearScreenZ, RecipNearClip, ToRwRGBA(CRGBA{ color3.r, color3.g, color3.b, color3.a }), 0.0f, 0.0f };
    maVertices[1] = RwIm2DVertex{ x4, y4, NearScreenZ, RecipNearClip, ToRwRGBA(CRGBA{ color4.r, color4.g, color4.b, color4.a }), 1.0f, 0.0f };
    maVertices[2] = RwIm2DVertex{ x2, y2, NearScreenZ, RecipNearClip, ToRwRGBA(CRGBA{ color2.r, color2.g, color2.b, color2.a }), 1.0f, 1.0f };
    maVertices[3] = RwIm2DVertex{ x1, y1, NearScreenZ, RecipNearClip, ToRwRGBA(CRGBA{ color1.r, color1.g, color1.b, color1.a }), 0.0f, 1.0f };
}

void CSprite2d::SetVertices(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4, float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4) {
    // src/CSprite2d/SetVertices_00727710.c - forwards to the RwD3D9Vertex* overload.
    // (gta-reversed changed that overload's param to RwIm2DVertex*; layout is
    // identical, 0x1C, so the cast below is faithful.)
    SetVertices(reinterpret_cast<RwD3D9Vertex*>(maVertices.data()),
                posn, color1, color2, color3, color4,
                u1, v1, u2, v2, u3, v3, u4, v4);
}

void CSprite2d::SetVertices(int32_t numVerts, const CVector2D* posn, const CVector2D* texCoors, const CRGBA& color) {
    // src/CSprite2d/SetVertices_00727890.c (decomp uses raw pointer arithmetic
    // over maVertices as RwD3D9Vertex; stride 0x1C == sizeof(RwIm2DVertex)).
    for (int32_t i = 0; i < numVerts; ++i) {
        maVertices[i].x = posn[i].x;
        maVertices[i].y = posn[i].y;
        maVertices[i].z = NearScreenZ + 0.0001f;
        maVertices[i].rhw = RecipNearClip;
        maVertices[i].u = texCoors[i].x;
        maVertices[i].v = texCoors[i].y;
        maVertices[i].emissiveColor = ToRwRGBA(CRGBA{ color.r, color.g, color.b, color.a });
    }
}

void CSprite2d::SetVertices(int32_t numVerts, const CVector2D* posn, const CRGBA& color) {
    // src/CSprite2d/SetVertices_00727920.c
    // DIVERGENCE from gta-reversed: the decomp advances the color pointer by 4
    // bytes per vertex, i.e. 1.0 reads color as a per-vertex array. The header
    // keeps gta-reversed's const-ref signature; the body replicates the exact
    // indexing (callers passing a single CRGBA with numVerts > 1 read past it,
    // exactly as the binary does).
    const CRGBA* colors = &color;
    for (int32_t i = 0; i < numVerts; ++i) {
        maVertices[i].x = posn[i].x;
        maVertices[i].y = posn[i].y;
        maVertices[i].z = NearScreenZ;
        maVertices[i].rhw = RecipNearClip;
        maVertices[i].u = 1.0f;
        maVertices[i].v = 1.0f;
        maVertices[i].emissiveColor = ToRwRGBA(CRGBA{ colors[i].r, colors[i].g, colors[i].b, colors[i].a });
    }
}

void CSprite2d::SetMaskVertices(int32_t numVerts, const CVector2D* posn, float depth) {
    // src/CSprite2d/SetMaskVertices_007279b0.c
    for (int32_t i = 0; i < numVerts; ++i) {
        maVertices[i].x = posn[i].x;
        maVertices[i].y = posn[i].y;
        maVertices[i].z = depth;
        maVertices[i].rhw = RecipNearClip;
        maVertices[i].emissiveColor = ToRwRGBA(CRGBA{ 0, 0, 0, 0 });
    }
}

void CSprite2d::SetVertices(RwD3D9Vertex* vertices, const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4, float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4) {
    // src/CSprite2d/SetVertices_00727a00.c
    // RwD3D9Vertex is opaque in this build; its 1.0 layout is identical to
    // RwIm2DVertex (0x1C: x,y,z,rhw,emissiveColor,u,v - plugin-sdk aliases the
    // same 0xC80468 array as RwD3D9Vertex*). Write through RwIm2DVertex.
    // Vertex order: (left,bottom,c3) (right,bottom,c4) (right,top,c2) (left,top,c1);
    // UV order: (u1,v1) (u2,v2) (u4,v4) (u3,v3).
    auto* v = reinterpret_cast<RwIm2DVertex*>(vertices);
    v[0] = RwIm2DVertex{ posn.left, posn.bottom, NearScreenZ, RecipNearClip, ToRwRGBA(CRGBA{ color3.r, color3.g, color3.b, color3.a }), u1, v1 };
    v[1] = RwIm2DVertex{ posn.right, posn.bottom, NearScreenZ, RecipNearClip, ToRwRGBA(CRGBA{ color4.r, color4.g, color4.b, color4.a }), u2, v2 };
    v[2] = RwIm2DVertex{ posn.right, posn.top, NearScreenZ, RecipNearClip, ToRwRGBA(CRGBA{ color2.r, color2.g, color2.b, color2.a }), u4, v4 };
    v[3] = RwIm2DVertex{ posn.left, posn.top, NearScreenZ, RecipNearClip, ToRwRGBA(CRGBA{ color1.r, color1.g, color1.b, color1.a }), u3, v3 };
}

void CSprite2d::DrawRect(const CRect& posn, const CRGBA& color) {
    // src/CSprite2d/DrawRect_00727b60.c
    // NOTE: the decomp's trailing fpRenderStateSet() call lost its arguments
    // (jumptable recovery warning); the gta-reversed cross-check restores it as
    // VERTEXALPHAENABLE(FALSE).
    RenderPort::SetTextureRaster(nullptr);
    SetVertices(posn, color, color, color, color);
    RenderPort::SetVertexAlphaEnable(color.a != 0xFF);
    RenderPort::RenderTriFan(maVertices.data(), 4);
    RenderPort::SetVertexAlphaEnable(false);
}

void CSprite2d::DrawTxRect(const CRect& posn, const CRGBA& color) {
    // src/CSprite2d/DrawTxRect_00727be0.c
    // NOTE: unlike DrawRect, 1.0 does NOT touch the texture raster or alpha
    // state here (caller sets it via SetRenderState()). Kept faithful.
    SetVertices(posn, color, color, color, color);
    RenderPort::RenderTriFan(maVertices.data(), 4);
}

void CSprite2d::DrawRect(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4) {
    // src/CSprite2d/DrawRect_00727c10.c
    RenderPort::SetTextureRaster(nullptr);
    SetVertices(posn, color1, color2, color3, color4);
    RenderPort::RenderTriFan(maVertices.data(), 4);
}

void CSprite2d::DrawRectXLU(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4) {
    // src/CSprite2d/DrawRectXLU_00727c50.c
    // NOTE: decomp sets the blend states BEFORE DrawRect (gta-reversed's
    // DrawRectXLU sets them first then calls DrawRect - same order).
    SetVertices(posn, color1, color2, color3, color4);
    RenderPort::SetTextureRaster(nullptr);
    RenderPort::SetVertexAlphaEnable(true);
    RenderPort::SetXluBlend();
    RenderPort::RenderTriFan(maVertices.data(), 4);
}

void CSprite2d::DrawAnyRect(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4) {
    // src/CSprite2d/DrawAnyRect_00727cc0.c
    SetVertices(x1, y1, x2, y2, x3, y3, x4, y4, color1, color2, color3, color4);
    RenderPort::SetVertexAlphaEnable(
        color1.a != 0xFF || color2.a != 0xFF || color3.a != 0xFF || color4.a != 0xFF);
    RenderPort::RenderTriFan(maVertices.data(), 4);
}

void CSprite2d::DrawCircleAtNearClip(const CVector2D& posn, float size, const CRGBA& color, int32_t angle) {
    // src/CSprite2d/DrawCircleAtNearClip_00727d60.c
    // Disassembly-corrected: the decomp shows the 0x821B40 calls with no args,
    // but they are FPU fistp float->int conversions (0x821B40 is __ftol-like,
    // NOT GetRandomNumber). Each triangle's rim indices are deterministic:
    //   idx = (int)((i + l - 1) * step * 256/360) & 0xFF, step = 360/angle
    // (INTEGER division - the binary does idiv 360/angle), with +64 (cosine)
    // offset for the X index. gta-reversed regularized this into even angular
    // steps with GetSinFast - the LUT+integer-step version here is the binary.
    InitSinLut();
    maVertices[0] = RwIm2DVertex{ posn.x, posn.y, NearScreenZ, RecipNearClip,
                                  ToRwRGBA(CRGBA{ color.r, color.g, color.b, color.a }), 0.5f, 0.5f };
    RenderPort::SetTextureRaster(nullptr);
    RenderPort::SetVertexAlphaEnable(true);
    const float step = float(360 / angle); // NOTE: 1.0 divides integers (idiv)
    for (int32_t i = 0; i < angle; ++i) {
        for (int32_t l = 1; l <= 2; ++l) {
            const float lutUnits = float(i + l - 1) * step * (256.0f / 360.0f);
            const int32_t idxX = FloatToInt(64.0f + lutUnits) & 0xFF;
            const int32_t idxY = FloatToInt(lutUnits) & 0xFF;
            maVertices[l].x = size * s_sinLut[idxX] + posn.x;
            maVertices[l].y = size * s_sinLut[idxY] + posn.y;
            maVertices[l].z = NearScreenZ;
            maVertices[l].rhw = RecipNearClip;
            maVertices[l].u = (s_sinLut[idxX] + 1.0f) * 0.5f;
            maVertices[l].v = (s_sinLut[idxY] + 1.0f) * 0.5f;
            maVertices[l].emissiveColor = ToRwRGBA(CRGBA{ color.r, color.g, color.b, color.a });
        }
        RenderPort::RenderTriangle(maVertices.data());
    }
}

void CSprite2d::SetVerticesForSniper(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4) {
    // src/CSprite2d/SetVerticesForSniper_00727fd0.c
    // Ghidra recovered the CRect* param as float*; offsets map to CRect fields:
    //   x: left, [1]: top, [2]: right, [3]: bottom. Color order c4,c3,c2,c1
    // maps to (v0:c4)? No - decomp assigns v0<-param_4, v1<-param_5,
    // v2<-param_3, v3<-param_2, i.e. v0=c4? param order is (rect,c1,c2,c3,c4):
    // v0=c4? Let me keep the decomp's exact assignment: v0=4th color param,
    // v1=5th, v2=3rd, v3=2nd. NOTE: the header's (c1..c4) map to decomp params
    // (param_2..param_5) = (c1,c2,c3,c4), so v0=c4? No: v0 <- param_4 = c3...
    // Decomp: emissiveColor v0 = (param_4[3],param_4[0..2]) = param_4 = 4th
    // declared param. Declared params: (float* rect, u1*c1, u1*c2, u1*c3, u1*c4)
    // so param_4 = c3? Counting: param_1=rect, param_2=c1, param_3=c2,
    // param_4=c3, param_5=c4. v0<-param_4=c3, v1<-param_5=c4, v2<-param_3=c2,
    // v3<-param_2=c1. So v0=c3, v1=c4, v2=c2, v3=c1.
    const float z = NearScreenZ + 1e-06f;
    maVertices[0] = RwIm2DVertex{ posn.left, posn.bottom, z, RecipNearClip, ToRwRGBA(CRGBA{ color3.r, color3.g, color3.b, color3.a }), 0.0f, 0.0f };
    maVertices[1] = RwIm2DVertex{ posn.right, posn.bottom, z, RecipNearClip, ToRwRGBA(CRGBA{ color4.r, color4.g, color4.b, color4.a }), 1.0f, 0.0f };
    maVertices[2] = RwIm2DVertex{ posn.right, posn.top, z, RecipNearClip, ToRwRGBA(CRGBA{ color2.r, color2.g, color2.b, color2.a }), 1.0f, 1.0f };
    maVertices[3] = RwIm2DVertex{ posn.left, posn.top, z, RecipNearClip, ToRwRGBA(CRGBA{ color1.r, color1.g, color1.b, color1.a }), 0.0f, 1.0f };
}

void CSprite2d::OffsetTexCoordForBilinearFiltering(float width, float height) {
    // src/CSprite2d/OffsetTexCoordForBilinearFiltering_00728150.c
    const float du = 1.0f / (width + width);
    const float dv = 1.0f / (height + height);
    for (int i = 0; i < 4; ++i) {
        maVertices[i].u += du;
        maVertices[i].v += dv;
    }
}

void CSprite2d::AddToBuffer(const CRect& posn, const CRGBA& color, float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4) {
    // src/CSprite2d/AddToBuffer_00728200.c
    // Decomp writes into CGlass::ReflectionPolyVertexBuffer[0x200 + nextBufferVertex];
    // here that is s_aTempBufferVertices[nextBufferVertex].
    const uint16_t base = uint16_t(nextBufferVertex);
    SetVertices(reinterpret_cast<RwD3D9Vertex*>(&s_aTempBufferVertices[nextBufferVertex]),
                posn, color, color, color, color, u1, v1, u2, v2, u3, v3, u4, v4);
    s_aTempBufferIndices[nextBufferIndex + 0] = base;
    s_aTempBufferIndices[nextBufferIndex + 1] = base + 1;
    s_aTempBufferIndices[nextBufferIndex + 2] = base + 2;
    s_aTempBufferIndices[nextBufferIndex + 3] = base + 3;
    s_aTempBufferIndices[nextBufferIndex + 4] = base;
    s_aTempBufferIndices[nextBufferIndex + 5] = base + 2;
    nextBufferVertex += 4;
    nextBufferIndex += 6;
    if (!IsVertexBufferFull())
        return;
    RenderVertexBuffer();
}

void CSprite2d::Draw2DPolygon(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const CRGBA& color) {
    // src/CSprite2d/Draw2DPolygon_007285b0.c
    SetVertices(x1, y1, x2, y2, x3, y3, x4, y4, color, color, color, color);
    RenderPort::SetTextureRaster(nullptr);
    RenderPort::SetShadeModeFlat();
    RenderPort::SetVertexAlphaEnable(color.a != 0xFF);
    RenderPort::RenderTriFan(maVertices.data(), 4);
}

void CSprite2d::DrawBarChart(float x, float y, uint16_t width, uint8_t height, float progress, int8_t progressAdd, uint8_t drawPercentage, uint8_t drawBlackBorder, CRGBA color, CRGBA addColor) {
    // src/CSprite2d/DrawBarChart_00728640.c
    // Disassembly-corrected: the decomp labels several 0x821B40 calls as
    // CGeneral::unk_00821b40, but they are FPU fistp float->int conversions
    // (0x821B40 is __ftol-like, NOT GetRandomNumber). Nothing here is random.
    RenderPort::SetTextureRaster(nullptr);
    RenderPort::SetShadeModeFlat();
    if (progress < 0.0f)
        progress = 0.0f;
    const float endX = x + float(width);
    const float barX = progress * 0.01f * float(width) + x;
    const float currX = std::min(barX, endX);
    const float fheight = float(height);

    // Progress rect (the filled part).
    CRect rect{};
    rect.left = x; rect.bottom = y; rect.right = currX; rect.top = y + fheight;
    DrawRect(rect, color);

    // Background rect (currX -> endX).
    // 1.0 packs (addColor.g<<16)|(addColor.r<<8)|color.a, then fills the CRGBA
    // through the byte-fill helper: r = (color.a * 0.5), g = (addColor.r * 0.5)
    // via fistp, but b = ((packed * 0.5) & 0xFF) - the WHOLE packed int halved
    // (fild of the int, not a byte extract). Verified by disassembly; the blue
    // channel looks like a 1.0 quirk - VERIFY in-game whether it should have
    // been (addColor.g * 0.5).
    {
        const uint32_t packed = (uint32_t(addColor.g) << 16) | (uint32_t(addColor.r) << 8) | color.a;
        rect.left = currX; rect.bottom = y; rect.right = endX; rect.top = y + fheight;
        DrawRect(rect, CRGBA{
            uint8_t(FloatToInt(float( packed        & 0xFF) * 0.5f)),
            uint8_t(FloatToInt(float((packed >>  8) & 0xFF) * 0.5f)),
            uint8_t(FloatToInt(float( packed)               * 0.5f)),
            uint8_t(packed & 0xFF) });
    }

    if (progressAdd != 0) {
        // "Recently added" flash segment. left = right - (float)(int8)progressAdd
        // (movsx + fisub in the binary; the decomp's CONCAT chain is just
        // sign-extension). Color is addColor with a = color.a.
        rect.right = currX;
        rect.left = rect.right - float(progressAdd);
        if (rect.left <= x - 1.0f)
            rect.left = x - 1.0f;
        rect.bottom = y; rect.top = y + fheight;
        DrawRect(rect, CRGBA{ addColor.r, addColor.g, addColor.b, color.a });
    }

    if (drawBlackBorder != 0) {
        // Border thickness: 2 * (maxH * 0.002232143) vertical,
        // 2 * (maxW * 0.0015625) horizontal. Color {0,0,0,color.a}.
        const float h = float(RsGlobalPort::maximumHeight) * 0.002232143f; // TODO(port)
        const float w = float(RsGlobalPort::maximumWidth) * 0.0015625f;    // TODO(port)
        const CRGBA black{ 0, 0, 0, color.a };
        rect.left = x; rect.top = y + h * 2.0f; rect.bottom = y; rect.right = endX;
        DrawRect(rect, black);
        rect.left = x; rect.top = y + fheight; rect.bottom = y + fheight - h * 2.0f; rect.right = endX;
        DrawRect(rect, black);
        rect.left = x; rect.top = y + fheight; rect.bottom = y; rect.right = x + w * 2.0f;
        DrawRect(rect, black);
        rect.left = endX; rect.top = y + fheight; rect.bottom = y; rect.right = endX - w * 2.0f;
        DrawRect(rect, black);
    }

    if (drawPercentage != 0) {
        // Percentage text: sprintf "%d%%" of (int)progress (fld + ftol in the
        // binary - NOT random, the decomp's no-arg unk_00821b40 hid the fld).
        char text[12]{};
        std::snprintf(text, sizeof(text), "%d%%", FloatToInt(progress));
        GxtChar gxtText[12]{};
        AsciiToGxtChar(text, gxtText); // TODO(port)
        CFont::SetWrapx(x);
        CFont::SetRightJustifyWrap(endX);
        CFont::SetColor(CRGBA{ 0, 0, 0, color.a });
        CFont::SetEdge(0);
        CFont::SetFontStyle(FONT_SUBTITLES);
        CFont::SetScale(fheight * 0.03f, fheight * 0.04f);
        // Text X: (int) of a float stack slot; compared as uint16 against
        // x + 50. Right-aligns when it fits, else left-aligns 5px further out.
        // VERIFY: the exact source float ([esp+0x10] in 1.0) - using the bar's
        // right edge (endX); could also be the progress X (currX).
        int32_t textX = FloatToInt(endX);
        if (x + 50.0f <= float(uint16_t(textX))) {
            CFont::SetOrientation(eFontAlignment::ALIGN_RIGHT);
        } else {
            CFont::SetOrientation(eFontAlignment::ALIGN_LEFT);
            textX += 5;
        }
        CFont::PrintString(float(textX), y + 2.0f, gxtText);
    }
}
