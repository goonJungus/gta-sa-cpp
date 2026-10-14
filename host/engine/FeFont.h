#pragma once
// FeFont - SA's bitmap font renderer (port of CFont for the frontend).
//
// models\fonts.txd holds two 512x512 glyph sheets: "font2" (texture 0:
// gothic + the menu sub-font) and "font1" (texture 1: subtitles + the
// pricedown sub-font). A glyph id is the character - 0x20; the menu and
// pricedown styles remap ids into their sub-font region
// (CFont::FindSubFontCharacter). Cells are 1/16 wide x 1/12.8 tall in UV,
// drawn 32 x 20 units at scale 1 (CFont::PrintChar), and the pen advances by
// the per-glyph width from data\fonts.dat (CFont::LoadFontValues).
// Edge = 8 offset copies in the drop colour, shadow = one (+s,+s) copy.
//
// Scales are in backbuffer pixels per font unit, exactly like CFont::SetScale
// (callers convert SA's 640x448 numbers with Sx()/Sy()).

#include <cstdint>
#include <string>

#include "Fe2d.h"

class D3DRenderer;

class FeFont {
public:
    enum Style { GOTHIC = 0, SUBTITLES = 1, MENU = 2, PRICEDOWN = 3 };
    enum Align { LEFT, CENTER, RIGHT };

    bool Init(D3DRenderer& r, const std::string& gameDir);
    void Shutdown();
    bool Ready() const { return m_tex[0] && m_tex[1]; }

    // Screen size; Sx/Sy convert SA's 640x448 design units to pixels.
    void SetScreen(float w, float h) { m_w = w; m_h = h; }
    float Sx(float v) const { return v * m_w / 640.0f; }
    float Sy(float v) const { return v * m_h / 448.0f; }

    void SetStyle(Style s);
    void SetScale(float sx, float sy) { m_scaleX = sx; m_scaleY = sy; }
    void SetColor(uint32_t argb) { m_color = argb; }
    void SetDropColor(uint32_t argb) { m_dropColor = argb; }
    void SetEdge(int px) { m_edge = px; m_shadow = 0; }       // CFont::SetEdge
    void SetDropShadow(int px) { m_shadow = px; m_edge = 0; }
    void SetAlign(Align a) { m_align = a; }
    void SetWrapWidth(float px) { m_wrap = px; }              // 0 = no wrap

    // Draw text with its top-left (LEFT), top-centre or top-right at (x,y).
    // Handles ~n~ line breaks and word wrap; other ~x~ tokens are skipped.
    // Returns the number of lines drawn.
    int Print(Fe2d& g, float x, float y, const std::string& text);
    float StringWidth(const std::string& text) const;   // one line, no wrap
    // CFont::ProcessCurrentString: y += scale.y*32*0.5 + 2*scale.y per line.
    float LineHeight() const { return m_scaleY * 18.0f; }

private:
    int GlyphId(unsigned char c) const;   // char -> (sub-)font glyph id
    float Advance(int id) const;
    void DrawRun(Fe2d& g, float x, float y, const std::string& line, uint32_t argb);

    IDirect3DTexture9* m_tex[2] = {nullptr, nullptr};
    uint8_t m_prop[2][210] = {};
    uint8_t m_unprop[2] = {27, 27};
    int m_texId = 0, m_style = 0;
    float m_scaleX = 1, m_scaleY = 1, m_w = 640, m_h = 448, m_wrap = 0;
    uint32_t m_color = 0xFFFFFFFF, m_dropColor = 0xFF000000;
    int m_edge = 0, m_shadow = 0;
    Align m_align = LEFT;
    D3DRenderer* m_r = nullptr;
};
