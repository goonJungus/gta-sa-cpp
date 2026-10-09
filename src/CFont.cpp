// CFont - adapted from Ghidra decomp for clean-room C++ build
// Decompiled bodies: src/CFont/*.c
// Binary: gta_sa.exe 1.0 (US)

#include "CFont.h"

#include <cmath>
#include <cstdint>
#include <cstring>

// Static member definitions.
// Original GTA SA 1.0 addresses (from gta-reversed StaticRef) kept as comments.
// TODO: re-resolve these for the clean-room build.
CFontChar CFont::RenderState{}; // 0xC71AA0
CSprite2d CFont::Sprite[MAX_FONT_SPRITES]; // 0xC71AD0
CSprite2d CFont::ButtonSprite[MAX_FONT_BUTTON_SPRITES]; // 0xC71AD8
eExtraFontSymbol CFont::PS2Symbol;          // 0xC71A54
bool  CFont::m_bNewLine;                   // 0xC71A55
CRGBA CFont::m_Color;                       // 0xC71A60
CVector2D CFont::m_Scale;                  // 0xC71A64
float CFont::m_fSlant;                     // 0xC71A6C
CVector2D CFont::m_fSlantRefPoint;         // 0xC71A70
bool  CFont::m_bFontJustify;               // 0xC71A78
bool  CFont::m_bFontCentreAlign;           // 0xC71A79
bool  CFont::m_bFontRightAlign;            // 0xC71A7A
bool  CFont::m_bFontBackground;            // 0xC71A7B
bool  CFont::m_bEnlargeBackgroundBox;      // 0xC71A7C
bool  CFont::m_bFontPropOn;                // 0xC71A7D
bool  CFont::m_bFontIsBlip;                // 0xC71A7E
float CFont::m_fFontAlpha;                 // 0xC71A80
CRGBA CFont::m_FontBackgroundColor;        // 0xC71A84
float CFont::m_fWrapx;                     // 0xC71A88
float CFont::m_fFontCentreSize;            // 0xC71A8C
float CFont::m_fRightJustifyWrap;          // 0xC71A90
uint8_t CFont::m_FontTextureId;            // 0xC71A94
uint8_t CFont::m_FontStyle;                // 0xC71A95
uint8_t CFont::m_nFontShadow;              // 0xC71A96
CRGBA CFont::m_FontDropColor;              // 0xC71A97
uint8_t CFont::m_nFontOutlineSize;         // 0xC71A9B
uint8_t CFont::m_nFontOutline;             // 0xC71A9C
uint8_t& CFont::m_nFontOutlineOrShadow = CFont::m_nFontOutline;

// Language id (DAT_00ba67cc in 1.0). TODO(port): bind to real frontend.
static int32_t s_currentLanguage = 0;

// 1.0 float->int casts compile to FPU fistp (round-to-nearest).
static int32_t FloatToInt(float v) { return int32_t(std::nearbyint(v)); }

// ============================================================
// Methods
// ============================================================

void CFont::Initialise() {
    // src/CFont/Initialise_005ba690.c
    // TODO(port): loads font textures via RW; sets defaults.
    // Decomp: creates Sprite/ButtonSprite textures, LoadFontValues().
    LoadFontValues(); // TODO(port): verify order
}

void CFont::LoadFontValues() {
    // src/CFont/LoadFontValues_007187c0.c
    // TODO(port): reads fonts.dat values into RenderState.
}

void CFont::Shutdown() {
    // src/CFont/Shutdown_007189b0.c
    // TODO(port): destroys font textures.
}

void CFont::SetScale(float w, float h) {
    // src/CFont/SetScale_00719380.c
    m_Scale.x = w;
    m_Scale.y = h;
}

void CFont::SetScaleForCurrentLanguage(float w, float h) {
    // src/CFont/SetScaleForCurrentLanguage_007193a0.c
    switch (s_currentLanguage) { // TODO(port): real language id
    case 1: case 2: case 3: case 4:
        m_Scale.y = h;
        m_Scale.x = w * 0.8f;
        return;
    default:
        m_Scale.x = w;
        m_Scale.y = h;
        return;
    }
}

void CFont::SetSlantRefPoint(float x, float y) {
    // src/CFont/SetSlantRefPoint_00719400.c
    m_fSlantRefPoint.x = x;
    m_fSlantRefPoint.y = y;
}

void CFont::SetSlant(float value) {
    // src/CFont/SetSlant_00719420.c
    m_fSlant = value;
}

void CFont::SetColor(CRGBA color) {
    // src/CFont/SetColor_00719430.c
    // (0x821B40 = fistp; alpha scales by m_fFontAlpha/255 when fading.)
    m_Color.r = color.r;
    m_Color.g = color.g;
    m_Color.b = color.b;
    m_Color.a = color.a;
    if (m_fFontAlpha < 255.0f) {
        m_Color.a = uint8_t(FloatToInt(float(color.a) * m_fFontAlpha * (1.0f / 255.0f)));
    }
}

void CFont::SetFontStyle(eFontStyle style) {
    // src/CFont/SetFontStyle_00719490.c
    if (style == FONT_MENU) {
        m_FontTextureId = 0;
        m_FontStyle = 2;
        return;
    }
    if (style != FONT_PRICEDOWN) {
        m_FontTextureId = uint8_t(style);
        m_FontStyle = 0;
        return;
    }
    m_FontTextureId = 1;
    m_FontStyle = 1;
}

void CFont::SetWrapx(float value) {
    // src/CFont/SetWrapx_007194d0.c
    m_fWrapx = value;
}

void CFont::SetCentreSize(float value) {
    // src/CFont/SetCentreSize_007194e0.c
    m_fFontCentreSize = value;
}

void CFont::SetRightJustifyWrap(float value) {
    // src/CFont/SetRightJustifyWrap_007194f0.c
    m_fRightJustifyWrap = value;
}

void CFont::SetAlphaFade(float alpha) {
    // src/CFont/SetAlphaFade_00719500.c
    m_fFontAlpha = alpha;
}

void CFont::SetDropColor(CRGBA color) {
    // src/CFont/SetDropColor_00719510.c
    m_FontDropColor.r = color.r;
    m_FontDropColor.g = color.g;
    m_FontDropColor.b = color.b;
    m_FontDropColor.a = color.a;
    if (m_fFontAlpha < 255.0f) {
        m_FontDropColor.a = uint8_t(FloatToInt(float(color.a) * m_fFontAlpha * (1.0f / 255.0f)));
    }
}

void CFont::SetDropShadowPosition(int16_t value) {
    // src/CFont/SetDropShadowPosition_00719570.c
    m_nFontOutlineSize = 0;
    m_nFontOutline = 0;
    m_nFontShadow = uint8_t(value);
}

void CFont::SetEdge(int8_t value) {
    // src/CFont/SetEdge_00719590.c
    m_nFontShadow = 0;
    m_nFontOutlineSize = uint8_t(value);
    m_nFontOutline = uint8_t(value);
}

void CFont::SetProportional(bool on) {
    // src/CFont/SetProportional_007195b0.c
    m_bFontPropOn = on;
}

void CFont::SetBackground(bool enable, bool includeWrap) {
    // src/CFont/SetBackground_007195c0.c
    m_bFontBackground = enable;
    m_bEnlargeBackgroundBox = includeWrap;
}

void CFont::SetBackgroundColor(CRGBA color) {
    // src/CFont/SetBackgroundColor_007195e0.c
    m_FontBackgroundColor = color;
}

void CFont::SetJustify(bool on) {
    // src/CFont/SetJustify_00719600.c
    m_bFontJustify = on;
}

void CFont::SetOrientation(eFontAlignment alignment) {
    // src/CFont/SetOrientation_00719610.c
    m_bFontCentreAlign = (alignment == eFontAlignment::ALIGN_CENTER);
    m_bFontRightAlign = (alignment == eFontAlignment::ALIGN_RIGHT);
    // (ALIGN_LEFT clears both; decomp confirms via the two bools.)
}

void CFont::InitPerFrame() {
    // src/CFont/InitPerFrame_00719800.c
    // TODO(port): resets per-frame font render state (RW).
}

void CFont::RenderFontBuffer() {
    // src/CFont/RenderFontBuffer_00719840.c
    // TODO(port): flushes the font vertex buffer via RW.
}

char* CFont::GetNextSpace(char* string) {
    // NOTSA helper (no direct decomp); returns pointer to next space or end.
    // TODO(port): verify against 1.0.
    while (*string && *string != ' ') ++string;
    return string;
}

char* CFont::ParseToken(char* text, CRGBA& color, bool isBlip, char* tag) {
    // src/CFont/ParseToken_00718f00.c
    // Parses a ~X~ color/symbol token. Returns pointer past the token.
    // TODO(port): CHudColours::GetRGBA, Hoodlum::unk_0156e2d0, full token table.
    // (The 1.0 switch is large; the structure below follows the decomp.
    // Symbol tokens set PS2Symbol; color tokens set `color` via HudColours.)
    if (!text || text[0] != '~')
        return text;
    char c = text[1];
    // TODO(port): full 1.0 token mapping (colors B,G,R,Y,P,W + PS2 symbols).
    // Minimal faithful skeleton: advance past "~X~" if well-formed.
    if (text[2] == '~')
        return text + 3;
    return text + 1;
}

void CFont::PrintChar(float x, float y, char character) {
    // src/CFont/PrintChar_00718a10.c
    // TODO(port): renders one glyph via CSprite2d (RW). Game-side: advances
    // cursor by GetCharacterSize, handles slant/wrap.
    (void)x; (void)y; (void)character;
}

float CFont::GetCharacterSize(uint8_t ch) {
    // src/CFont/GetCharacterSize_00719750.c
    // TODO(port): looks up RenderState/proportional widths.
    (void)ch;
    return 0.0f;
}

uint8_t CFont::FindSubFontCharacter(uint8_t letterId, uint8_t fontStyle) {
    // src/CFont/FindSubFontCharacter_007192c0.c
    // TODO(port): sub-font lookup.
    (void)letterId; (void)fontStyle;
    return 0;
}

float CFont::GetHeight(bool a1) {
    // TODO(port): no direct decomp; returns line height.
    (void)a1;
    return 0.0f;
}

float CFont::GetStringWidth(const GxtChar* string, bool full, bool scriptText) {
    // src/CFont/GetStringWidth_0071a0e0.c
    // TODO(port): full implementation (token-aware width summation).
    (void)string; (void)full; (void)scriptText;
    return 0.0f;
}

void CFont::DrawFonts() {
    // TODO(port): no direct decomp; draws all buffered fonts.
}

int16_t CFont::ProcessCurrentString(bool print, float x, float y, const GxtChar* text) {
    // src/CFont/ProcessCurrentString_0071a220.c
    // TODO(port): full line-breaking/layout state machine.
    (void)print; (void)x; (void)y; (void)text;
    return 0;
}

int16_t CFont::GetNumberLines(float x, float y, const GxtChar* text) {
    // src/CFont/GetNumberLines_0071a5e0.c
    // TODO(port): counts lines via ProcessCurrentString(false,...).
    (void)x; (void)y; (void)text;
    return 0;
}

int16_t CFont::ProcessStringToDisplay(float x, float y, const GxtChar* text) {
    // src/CFont/ProcessStringToDisplay_0071a600.c
    // TODO(port): full display processing.
    (void)x; (void)y; (void)text;
    return 0;
}

void CFont::GetTextRect(CRect* rect, float x, float y, const GxtChar* text) {
    // src/CFont/GetTextRect_0071a620.c
    // TODO(port): computes bounding rect.
    (void)rect; (void)x; (void)y; (void)text;
}

void CFont::PrintString(float x, float y, const GxtChar* text) {
    // src/CFont/PrintString_0071a700.c
    // TODO(port): full implementation (justify/wrap/background/shadow via RW).
    (void)x; (void)y; (void)text;
}

void CFont::PrintStringFromBottom(float x, float y, const GxtChar* text) {
    // src/CFont/PrintStringFromBottom_0071a820.c
    // TODO(port): PrintString with bottom alignment.
    (void)x; (void)y; (void)text;
}

// File-scope helpers (declared in CFont.h).
void ReadFontsDat() {
    // TODO(port): parses fonts.dat.
}
float GetScriptLetterSize(uint8_t letterId) {
    // TODO(port)
    (void)letterId;
    return 0.0f;
}
float GetLetterIdPropValue(uint8_t letterId) {
    // TODO(port)
    (void)letterId;
    return 0.0f;
}
