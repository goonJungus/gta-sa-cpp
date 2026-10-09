// CFont - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Font.h
// Decompiled bodies: src/CFont/*.c
// TODO: verify each method against decomp.

#pragma once

#include "CVector.h"     // CVector2D
#include "CRect.h"       // CRect by value in GetTextRect()
#include "CSprite2d.h"   // CSprite2d by value in Sprite/ButtonSprite arrays
#include "RenderTypes.h" // CRGBA, GxtChar

#include <array>
#include <cstdint>

// todo: move
struct CFontChar {
    uint8_t   m_cLetter;
    uint8_t   m_dLetter;
    CVector2D m_vPosn;
    float     m_fWidth;
    float     m_fHeight;
    CRGBA     m_color;
    float     m_fWrap;
    float     m_fSlant;
    CVector2D m_vSlanRefPoint;
    bool      m_bContainImages;
    uint8_t   m_nFontStyle;
    bool      m_bPropOn;
    uint16_t  m_wFontTexture;
    uint8_t   m_nOutline;

public:
    // 0x718E50
    void Set(const CFontChar& setup) {
        m_cLetter        = setup.m_cLetter;
        m_dLetter        = setup.m_dLetter;
        m_vPosn          = setup.m_vPosn;
        m_fWidth         = setup.m_fWidth;
        m_fHeight        = setup.m_fHeight;
        m_color          = setup.m_color;
        m_fWrap          = setup.m_fWrap;
        m_fSlant         = setup.m_fSlant;
        m_vSlanRefPoint  = setup.m_vSlanRefPoint;
        m_bContainImages = setup.m_bContainImages;
        m_nFontStyle     = setup.m_nFontStyle;
        m_bPropOn        = setup.m_bPropOn;
        m_wFontTexture   = setup.m_wFontTexture;
        m_nOutline       = setup.m_nOutline;
    }
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CFontChar) == 0x30, "CFontChar layout changed");
#endif

struct tFontData {
    std::array<uint8_t, 208> m_propValues;
    uint8_t m_spaceValue;
    uint8_t m_unpropValue;
};

enum eExtraFontSymbol : uint8_t {
    EXSYMBOL_NONE       = 0, // invalid
    EXSYMBOL_DPAD_UP    = 1,
    EXSYMBOL_DPAD_DOWN  = 2,
    EXSYMBOL_DPAD_LEFT  = 3,
    EXSYMBOL_DPAD_RIGHT = 4,
    EXSYMBOL_CROSS      = 5,
    EXSYMBOL_CIRCLE     = 6,
    EXSYMBOL_SQUARE     = 7,
    EXSYMBOL_TRIANGLE   = 8,
    EXSYMBOL_KEY        = 9, // followed by buttons, L1?
    EXSYMBOL_L2         = 10,
    EXSYMBOL_L3         = 11,
    EXSYMBOL_R1         = 12,
    EXSYMBOL_R2         = 13,
    EXSYMBOL_R3         = 14
};

enum eFontStyle : uint8_t {
    FONT_GOTHIC,
    FONT_SUBTITLES,
    FONT_MENU,
    FONT_PRICEDOWN
};
// NOTSA_WENUM_DEFS_FOR(eFontStyle) dropped: WEnum (gta-reversed extensions/)
// is not in this build.

enum class eFontAlignment : int8_t {
    ALIGN_UNDEFINED = -1,
    ALIGN_CENTER    =  0,
    ALIGN_LEFT      =  1,
    ALIGN_RIGHT     =  2
};

class CFont {
public:
    // static variables
    static constexpr size_t MAX_FONT_SPRITES = 2;
    static constexpr size_t MAX_FONT_BUTTON_SPRITES = 15;

    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments. Definitions in CFont.cpp.
    // TODO: re-resolve for the clean-room build.
    static CFontChar RenderState; // 0xC71AA0
    // font textures array
    static CSprite2d Sprite[MAX_FONT_SPRITES]; // 0xC71AD0
    // button textures array
    static CSprite2d ButtonSprite[MAX_FONT_BUTTON_SPRITES]; // 0xC71AD8
    static eExtraFontSymbol PS2Symbol;          // 0xC71A54
    static bool  m_bNewLine;                   // 0xC71A55
    static CRGBA m_Color;                       // 0xC71A60
    static CVector2D m_Scale;                  // 0xC71A64
    static float m_fSlant;                     // 0xC71A6C
    static CVector2D m_fSlantRefPoint;         // 0xC71A70
    static bool  m_bFontJustify;               // 0xC71A78
    static bool  m_bFontCentreAlign;           // 0xC71A79
    static bool  m_bFontRightAlign;            // 0xC71A7A
    static bool  m_bFontBackground;            // 0xC71A7B
    static bool  m_bEnlargeBackgroundBox;      // 0xC71A7C
    static bool  m_bFontPropOn;                // 0xC71A7D
    static bool  m_bFontIsBlip;               // 0xC71A7E
    static float m_fFontAlpha;                 // 0xC71A80
    static CRGBA m_FontBackgroundColor;        // 0xC71A84
    static float m_fWrapx;                     // 0xC71A88
    static float m_fFontCentreSize;            // 0xC71A8C
    static float m_fRightJustifyWrap;          // 0xC71A90
    static uint8_t m_FontTextureId;            // 0xC71A94
    static uint8_t m_FontStyle;                // 0xC71A95
    static uint8_t m_nFontShadow;              // 0xC71A96
    static CRGBA m_FontDropColor;              // 0xC71A97
    static uint8_t m_nFontOutlineSize;         // 0xC71A9B
    static uint8_t m_nFontOutline;             // 0xC71A9C
    // m_nFontOutlineOrShadow is the SAME address (0xC71A9C) as m_nFontOutline:
    // kept as a reference alias so both names stay usable.
    static uint8_t& m_nFontOutlineOrShadow;    // 0xC71A9C (alias of m_nFontOutline)

public:
    static void Initialise();
    static void LoadFontValues();
    static void Shutdown();
    static void PrintChar(float x, float y, char character);
    // Get next ' ' character in a string
    static char* GetNextSpace(char* string);
    static char* ParseToken(char* text, CRGBA& color, bool isBlip, char* tag);
    static void SetScale(float w, float h);
    static void SetScaleForCurrentLanguage(float w, float h);
    static void SetSlantRefPoint(float x, float y);
    static void SetSlant(float value);
    static void SetColor(CRGBA color);
    static void SetFontStyle(eFontStyle style);
    static void SetWrapx(float value);
    static void SetCentreSize(float value);
    static void SetRightJustifyWrap(float value);
    static void SetAlphaFade(float alpha);
    static void SetDropColor(CRGBA color);
    static void SetDropShadowPosition(int16_t value);
    static void SetEdge(int8_t value);
    static void SetProportional(bool on);
    static void SetBackground(bool enable, bool includeWrap);
    static void SetBackgroundColor(CRGBA color);
    static void SetJustify(bool on);
    static void SetOrientation(eFontAlignment alignment);
    static void InitPerFrame();
    static void RenderFontBuffer();
    static float GetHeight(bool a1 = false);
    static float GetStringWidth(const GxtChar* string, bool full, bool scriptText);
    static void DrawFonts();
    static int16_t ProcessCurrentString(bool print, float x, float y, const GxtChar* text);
    static int16_t GetNumberLines(float x, float y, const GxtChar* text);
    static int16_t ProcessStringToDisplay(float x, float y, const GxtChar* text);
    static void GetTextRect(CRect* rect, float x, float y, const GxtChar* text);
    static void PrintString(float x, float y, const GxtChar* text);
    static void PrintStringFromBottom(float x, float y, const GxtChar* text);
    static float GetCharacterSize(uint8_t ch);
    static uint8_t FindSubFontCharacter(uint8_t letterId, uint8_t fontStyle);
};

// Clean-room: gta-reversed declared these `static` at file scope (internal
// linkage per TU). Declared here as plain externs instead; defined in CFont.cpp.
void ReadFontsDat();
float GetScriptLetterSize(uint8_t letterId);
float GetLetterIdPropValue(uint8_t letterId);
