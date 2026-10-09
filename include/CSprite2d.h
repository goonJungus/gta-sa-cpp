// CSprite2d - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Sprite2d.h
// Decompiled bodies: src/CSprite2d/*.c
// TODO: verify each method against decomp.

#pragma once

#include "CVector.h"     // CVector2D
#include "CRect.h"       // CRect by value
#include "RenderTypes.h" // RwTexture, RwTextureAddressMode, RwIm2DVertex,
                         // RwD3D9Vertex, CRGBA

#include <array>
#include <cstdint>

#define NUM_SPRITE_VERTICES 8

class CSprite2d {
public:
    RwTexture* m_pTexture;

    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments. Definitions in CSprite2d.cpp.
    // TODO: re-resolve for the clean-room build.
    static int32_t nextBufferIndex;  // 0xC80458
    static int32_t nextBufferVertex; // 0xC8045C
    static float NearScreenZ;        // 0xC80460
    static float RecipNearClip;      // 0xC80464
    static std::array<RwIm2DVertex, 8> maVertices; // 0xC80468

public:
    CSprite2d();
    ~CSprite2d();

    void Delete();

    void SetTexture(const char* name);
    void SetTexture(const char* name, const char* maskName);
    void SetAddressingUV(RwTextureAddressMode modeU, RwTextureAddressMode modeV);
    void SetAddressing(RwTextureAddressMode modeUV);
    void SetRenderState();

    void Draw(float x, float y, float width, float height, const CRGBA& color);
    void Draw(const CRect& posn, const CRGBA& color);
    void DrawWithBilinearOffset(const CRect& posn, const CRGBA& color);
    void Draw(const CRect& posn, const CRGBA& color, float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4);
    void Draw(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4);
    void Draw(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const CRGBA& color);

    static void SetRecipNearClip();
    static void InitPerFrame();
    static bool IsVertexBufferEmpty();
    static bool IsVertexBufferFull();
    static void RenderVertexBuffer();
    static void SetVertices(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4);
    static void SetVertices(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4);
    static void SetVertices(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4, float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4);
    static void SetVertices(int32_t numVerts, const CVector2D* posn, const CVector2D* texCoors, const CRGBA& color);
    static void SetVertices(int32_t numVerts, const CVector2D* posn, const CRGBA& color);
    static void SetMaskVertices(int32_t numVerts, const CVector2D* posn, float depth);
    static void SetVertices(RwD3D9Vertex* vertices, const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4, float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4);
    static void DrawRect(const CRect& posn, const CRGBA& color);
    static void DrawTxRect(const CRect& posn, const CRGBA& color);
    static void DrawRect(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4);
    static void DrawRectXLU(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4);
    static void DrawAnyRect(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4);
    static void DrawCircleAtNearClip(const CVector2D& posn, float size, const CRGBA& color, int32_t angle);
    static void SetVerticesForSniper(const CRect& posn, const CRGBA& color1, const CRGBA& color2, const CRGBA& color3, const CRGBA& color4);
    static void OffsetTexCoordForBilinearFiltering(float width, float height);
    static void AddToBuffer(const CRect& posn, const CRGBA& color, float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4);
    static void Draw2DPolygon(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const CRGBA& color);
    static void DrawBarChart(float x, float y, uint16_t width, uint8_t height, float progress, int8_t progressAdd, uint8_t drawPercentage, uint8_t drawBlackBorder, CRGBA color, CRGBA addColor);

    static auto* GetVertices()      { return maVertices.data(); }
    static auto& GetNearScreenZ()   { return NearScreenZ; }
    static auto& GetRecipNearClip() { return RecipNearClip; }
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CSprite2d) == 4, "CSprite2d layout changed");
#endif
