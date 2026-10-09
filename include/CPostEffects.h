// CPostEffects - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/PostEffects.h
// Decompiled bodies: src/CPostEffects/*.c
// TODO: verify each method against decomp.

#pragma once

#include "RenderTypes.h" // RwRaster, RwRect, RwRGBA, RwRGBAReal, RwIm2DVertex,
                         // RwBlendFunction, RwBool, RwCullMode, RwShadeMode,
                         // RwTextureAddressMode, RwTextureFilterMode, CRGBA

#include <array>
#include <cstdint>

class CPed; // InfraredVisionStoreAndSetLightsForHeatObjects(CPed*)

enum eHeatHazeFXType {
    HEAT_HAZE_UNDEFINED = -1,
    HEAT_HAZE_0,
    HEAT_HAZE_1,
    HEAT_HAZE_2,
    HEAT_HAZE_3,
    HEAT_HAZE_4,

    MAX_HEAT_HAZE_TYPES
};

class CPostEffects {
public:
    static void Initialise();
    static void Close();
    static void DoScreenModeDependentInitializations();
    static void SetupBackBufferVertex();
    static void Update();

    // X2,Y2 is added to X1,Y1. So they are more like width and height in a rectangle.
    static void DrawQuad(float x1, float y1, float x2, float y2, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, RwRaster* raster);
    static void DrawQuadSetDefaultUVs();
    static void DrawQuadSetPixelUVs(float u0, float v0, float u1, float v1, float u3, float v3, float u2, float v2);
    static void DrawQuadSetUVs(float u1, float v1, float u2, float v2, float u3, float v3, float u4, float v4);

    static void FilterFX_RestoreDayNightBalance();
    static void FilterFX_StoreAndSetDayNightBalance();

    static void ImmediateModeFilterStuffInitialize();
    static void ImmediateModeRenderStatesSet();
    static void ImmediateModeRenderStatesStore();
    static void ImmediateModeRenderStatesReStore();

    static RwRaster* RasterCreatePostEffects(RwRect rect);

    static void ScriptCCTVSwitch(bool enable);
    static void ScriptDarknessFilterSwitch(bool enable, int32_t alpha);
    static void ScriptHeatHazeFXSwitch(bool enable);
    static void ScriptInfraredVisionSwitch(bool enable);
    static void ScriptNightVisionSwitch(bool enable);
    static void ScriptResetForEffects();

    static void UnderWaterRipple(CRGBA color, float xoffset, float yoffset, float strength, float speed, float freq);
    static void UnderWaterRippleFadeToFX();

    static void HeatHazeFXInit();
    static void HeatHazeFX(float fIntensity, bool bAlphaMaskMode);

    static bool IsVisionFXActive();

    static void NightVision();
    static void NightVisionSetLights();

    static void SetFilterMainColour(RwRaster* raster, RwRGBA color);
    static void InfraredVision(RwRGBA color, RwRGBA colorMain);
    static void InfraredVisionSetLightsForDefaultObjects();
    static void InfraredVisionSetLightsForHeatObjects();
    static void InfraredVisionStoreAndSetLightsForHeatObjects(CPed* ped);
    static void InfraredVisionRestoreLightsForHeatObjects();

    static void Fog();
    static void CCTV();
    static void Grain(int32_t strengthMask, bool update);
    static void SpeedFX(float speed);
    static void DarknessFilter(int32_t alpha);
    static void ColourFilter(RwRGBA pass1, RwRGBA pass2);
    static void Radiosity(int32_t intensityLimit, int32_t filterPasses, int32_t renderPasses, int32_t intensity);
    static void SetSpeedFXManualSpeedCurrentFrame(float value);

    static void Render();

public:
    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments, with the initializer comments preserved
    // from gta-reversed. Definitions in CPostEffects.cpp.
    // TODO: re-resolve for the clean-room build.
    static float SCREEN_EXTRA_MULT_CHANGE_RATE; // 0x8D5168 ; = 0.0005f
    static float SCREEN_EXTRA_MULT_BASE_CAP;   // 0x8D516C ; = 0.35f
    static float SCREEN_EXTRA_MULT_BASE_MULT;  // 0x8D5170 ; = 1.0f

    static bool m_bDisableAllPostEffect; // 0xC402CF
    static bool m_bSavePhotoFromScript;  // 0xC402D0
    static bool m_bInCutscene;           // 0xC402B7

    static float m_xoffset; // 0x8D5130 ; = 4.0f
    static float m_yoffset; // 0x8D5134 ; = 24.0f

    static float m_colour1Multiplier;  // 0x8D5160 ; = 1.0f
    static float m_colour2Multiplier;  // 0x8D5164 ; = 1.0f
    static float m_colourLeftUOffset;  // 0x8D5150 ; = 8
    static float m_colourRightUOffset; // 0x8D5154 ; = 8
    static float m_colourTopVOffset;   // 0x8D5158 ; = 8
    static float m_colourBottomVOffset; // 0x8D515C ; = 8

    static bool    m_bNightVision;                // 0xC402B8
    static float   m_fNightVisionSwitchOnFXCount; // 0xC40300 ; = m_fNightVisionSwitchOnFXTime
    static float   m_fNightVisionSwitchOnFXTime;  // 0x8D50B0 ; = 50.0f
    static int32_t m_NightVisionGrainStrength;    // 0x8D50A8 ; = 48
    static CRGBA   m_NightVisionMainCol;          // 0x8D50AC ; = { 255, 0, 130, 0 }

    static bool    m_bDarknessFilter;                          // 0xC402C4
    static int32_t m_DarknessFilterAlpha;                      // 0x8D5204 ; = 170
    static int32_t m_DarknessFilterAlphaDefault;               // 0x8D50F4 ; = 170
    static int32_t m_DarknessFilterRadiosityIntensityLimit;    // 0x8D50F8 ; = 45

    static float m_fWaterFXStartUnderWaterness; // 0x8D514C ; = 0.535f
    static float m_fWaterFullDarknessDepth;    // 0x8D5148 ; = 90.0f
    static bool  m_bWaterDepthDarkness;        // 0x8D5144 ; = true

    static bool    m_bHeatHazeFX;                // 0xC402BA
    static int32_t m_HeatHazeFXSpeedMin;         // 0x8D50EC ; = 6
    static int32_t m_HeatHazeFXSpeedMax;         // 0x8D50F0 ; = 10
    static int32_t m_HeatHazeFXIntensity;        // 0x8D50E8 ; = 150
    static int32_t m_HeatHazeFXType;             // 0xC402BC ; = 0
    static int32_t m_HeatHazeFXTypeLast;         // 0x8D50E4 ; = -1
    static int32_t m_HeatHazeFXRandomShift;      // 0xC402C0
    static int32_t m_HeatHazeFXScanSizeX;        // 0xC40304
    static int32_t m_HeatHazeFXScanSizeY;        // 0xC40308
    static int32_t m_HeatHazeFXRenderSizeX;      // 0xC4030C
    static int32_t m_HeatHazeFXRenderSizeY;      // 0xC40310

    static bool m_bFog; // 0xC402C6

    static bool    m_bSpeedFX;                       // 0x8D5100 ; = true
    static bool    m_bSpeedFXTestMode;               // 0xC402C7
    static bool    m_bSpeedFXUserFlag;               // 0x8D5108 ; = true
    static bool    m_bSpeedFXUserFlagCurrentFrame;   // 0x8D5109 ; = true
    static float   m_fSpeedFXManualSpeedCurrentFrame; // 0xC402C8
    static int32_t m_SpeedFXAlpha;                   // 0x8D5104 ; = 36

    static RwRaster* pRasterFrontBuffer; // 0xC402D8

    static bool                   m_bGrainEnable; // 0xC402B4
    static RwRaster*              m_pGrainRaster; // 0xC402B0
    static std::array<char, 2>    m_grainStrength; // 0x8D5094

    static bool  m_bCCTV;   // 0xC402C5
    static CRGBA m_CCTVcol; // 0x8D50FC ; = { 64, 0, 0, 0 }

    static bool m_bRainEnable;  // 0xC402D1
    static bool m_bColorEnable; // 0x8D518C ; = true

    static bool     m_bRadiosity;                             // 0xC402CC
    static bool     m_bRadiosityDebug;                        // 0xC402CD
    static bool     m_bRadiosityLinearFilter;                 // 0x8D510A ; = true
    static bool     m_bRadiosityStripCopyMode;                // 0x8D510B ; = true
    static int32_t  m_RadiosityFilterUCorrection;             // 0x8D511C ; = 2
    static int32_t  m_RadiosityFilterVCorrection;             // 0x8D5120 ; = 2
    static int32_t  m_RadiosityIntensity;                     // 0x8D5118 ; = 35
    static int32_t  m_RadiosityIntensityLimit;                 // 0x8D5114 ; = 220
    static bool     m_bRadiosityBypassTimeCycleIntensityLimit; // 0xC402CE
    static float    m_RadiosityPixelsX;                        // 0xC40314
    static float    m_RadiosityPixelsY;                        // 0xC40318
    static uint32_t m_RadiosityFilterPasses;                   // 0x8D5110 ; = 1
    static uint32_t m_RadiosityRenderPasses;                   // 0x8D510C ; = 2

    static float m_VisionFXDayNightBalance; // 0x8D50A4 ; = 1.0f

    static bool      m_bInfraredVision;               // 0xC402B9
    static int32_t   m_InfraredVisionGrainStrength;   // 0x8D50B4 ; = 64
    static float     m_fInfraredVisionFilterRadius;   // 0x8D50B8 ; = 0.003f
    static CRGBA     m_InfraredVisionCol;             // 0x8D50CC ; = { FF, 3C, 28, 6E }
    static CRGBA     m_InfraredVisionMainCol;         // 0x8D50D0 ; = { FF, C8, 00, 64 }
    static RwRGBAReal m_fInfraredVisionHeatObjectCol; // 0x8D50BC ; = { 1.0f, 0.0f, 0.0f, 1.0f }
    static int32_t   m_HeatHazeFXHourOfDayStart;      // 0x8D50D4 ; = 10
    static int32_t   m_HeatHazeFXHourOfDayEnd;        // 0x8D50D8 ; = 19
    static float     m_fHeatHazeFXFadeSpeed;          // 0x8D50DC ; = 0.05f
    static float     m_fHeatHazeFXInsideBuildingFadeSpeed; // 0x8D50E0 ; = 0.5f

    static bool  m_waterEnable;   // 0xC402D3
    static float m_waterStrength; // 0x8D512C ; = 64
    static float m_waterSpeed;    // 0x8D5138 ; = 0.0015f
    static float m_waterFreq;     // 0x8D513C ; = 0.04f
    static CRGBA m_waterCol;      // 0x8D5140 ; = { 64, 64, 64, 64 }

    // Immediate Mode Filter
    struct imf {
        float                       screenZ;
        float                       recipCameraZ;
        RwRaster*                   RasterDrawBuffer;
        int32_t                     sizeDrawBufferX;
        int32_t                     sizeDrawBufferY;
        float                       fFrontBufferU1;
        float                       fFrontBufferV1;
        float                       fFrontBufferU2;
        float                       fFrontBufferV2;
        std::array<RwIm2DVertex, 3> triangle;
        float                       uMinTri;
        float                       uMaxTri;
        float                       vMinTri;
        float                       vMaxTri;
        std::array<RwIm2DVertex, 6> quad;
        RwBlendFunction             blendSrc;
        RwBlendFunction             blendDst;
        RwBool                      bFog;
        RwCullMode                  cullMode;
        RwBool                      bZTest;
        RwBool                      bZWrite;
        RwShadeMode                 shadeMode;
        RwBool                      bVertexAlpha;
        RwTextureAddressMode        textureAddress;
        RwTextureFilterMode         textureFilter;
    };

    static imf ms_imf; // 0xC40150
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(CPostEffects::imf) == 0x158, "CPostEffects::imf layout changed");
#endif
