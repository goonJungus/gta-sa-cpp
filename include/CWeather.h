// CWeather - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/Weather.h
// Decompiled bodies: src/CWeather/*.c
// TODO: verify each method against decomp.

#pragma once

#include "CVector.h" // CVector, CVector2D

#include <array>
#include <cstdint>

class CAEWeatherAudioEntity; // port with the audio subsystem

// Values from gta-reversed/source/game_sa/Enums/eWeatherType.h
enum eWeatherType : int16_t {
    WEATHER_UNDEFINED = -1,

    WEATHER_EXTRASUNNY_LA = 0,
    WEATHER_SUNNY_LA = 1,
    WEATHER_EXTRASUNNY_SMOG_LA = 2,
    WEATHER_SUNNY_SMOG_LA = 3,
    WEATHER_CLOUDY_LA = 4,

    WEATHER_SUNNY_SF = 5,
    WEATHER_EXTRASUNNY_SF = 6,
    WEATHER_CLOUDY_SF = 7,
    WEATHER_RAINY_SF = 8,
    WEATHER_FOGGY_SF = 9,

    WEATHER_SUNNY_VEGAS = 10,
    WEATHER_EXTRASUNNY_VEGAS = 11,
    WEATHER_CLOUDY_VEGAS = 12,

    WEATHER_EXTRASUNNY_COUNTRYSIDE = 13,
    WEATHER_SUNNY_COUNTRYSIDE = 14,
    WEATHER_CLOUDY_COUNTRYSIDE = 15,
    WEATHER_RAINY_COUNTRYSIDE = 16,

    WEATHER_EXTRASUNNY_DESERT = 17,
    WEATHER_SUNNY_DESERT = 18,
    WEATHER_SANDSTORM_DESERT = 19,

    WEATHER_UNDERWATER = 20,
    WEATHER_EXTRACOLOURS_1 = 21,
    WEATHER_EXTRACOLOURS_2 = 22,

    NUM_WEATHERS,
    WEATHER_EXTRA_START = WEATHER_EXTRACOLOURS_1
};

enum eWeatherRegion : int16_t {
    WEATHER_REGION_DEFAULT = 0,
    WEATHER_REGION_LA = 1,
    WEATHER_REGION_SF = 2,
    WEATHER_REGION_LV = 3,
    WEATHER_REGION_DESERT = 4
};

class CWeather {
public:
    // Static data: gta-reversed bound these to fixed GTA SA 1.0 addresses via
    // StaticRef<T>(addr). Replaced with plain static members; the original
    // addresses are kept as comments. Definitions in CWeather.cpp.
    // TODO: re-resolve for the clean-room build.
    static float TrafficLightsBrightness;       // 0xC812A8
    static bool  bScriptsForceRain;              // 0xC812AC
    static float Earthquake;                    // 0xC81340
    static uint32_t CurrentRainParticleStrength; // 0xC812B0
    static uint32_t LightningStartY;            // 0xC812B4 ; only initialized (0), not used
    static uint32_t LightningStartX;            // 0xC812B8 ; only initialized (0), not used
    static int32_t  LightningFlashLastChange;   // 0xC812BC
    static uint32_t WhenToPlayLightningSound;   // 0xC812C0
    static uint32_t LightningDuration;          // 0xC812C4 ; Duration as number of frames
    static uint32_t LightningStart;             // 0xC812C8 ; frame number
    static bool  LightningFlash;                // 0xC812CC
    static bool  LightningBurst;                // 0xC812CD
    static float HeadLightsSpectrum;            // 0xC812D0
    static float WaterFogFXControl;              // 0xC81338
    static float HeatHazeFXControl;              // 0xC812D8
    static float HeatHaze;                      // 0xC812DC
    static float SunGlare;                      // 0xC812E0
    static float Rainbow;                       // 0xC812E4
    static float Wavyness;                      // 0xC812E8
    static float WindClipped;                   // 0xC812EC
    static CVector WindDir;                     // 0xC813E0
    static float Wind;                          // 0xC812F0
    static float Sandstorm;                     // 0xC812F4
    static float Rain;                          // 0xC81324
    static float InTunnelness;                  // 0xC81334
    static float WaterDepth;                    // 0xC81330
    static float UnderWaterness;                // 0xC8132C
    static float ExtraSunnyness;                // 0xC812F8
    static float Foggyness_SF;                  // 0xC812FC
    static float Foggyness;                     // 0xC81300
    static float CloudCoverage;                 // 0xC81304
    static float WetRoads;                      // 0xC81308
    static float InterpolationValue;            // 0xC8130C
    static uint32_t WeatherTypeInList;          // 0xC81310
    static eWeatherRegion WeatherRegion;        // 0xC81314
    static eWeatherType ForcedWeatherType;      // 0xC81318
    static eWeatherType NewWeatherType;         // 0xC8131C
    static eWeatherType OldWeatherType;         // 0xC81320
    static CAEWeatherAudioEntity m_WeatherAudioEntity; // 0xC81360 ; DEFERRED: incomplete type, no definition yet (see CWeather.cpp)
    static int32_t StreamAfterRainTimer;        // 0x8D5EAC
    static float HeatHazeFXFade;                // 0xC81448
    static int32_t HeatHazeFXLastMinute;        // 0x8D6078
    static float WaterFogFXFade;                // 0xC81444
    static bool  WaterFogFXFadingOut;           // 0xC81440

    // in entity.cpp:

    static std::array<float, 16> saTreeWindOffsets;   // orig WindTabel
    static std::array<float, 32> saBannerWindOffsets; // orig BannerWindTabel

public:
    static void Init();
    static void AddRain();
    static void AddSandStormParticles();
    static const eWeatherType* FindWeatherTypesList();
    static void ForceWeather(eWeatherType weatherType);
    static void ForceWeatherNow(eWeatherType weatherType);
    static bool ForecastWeather(eWeatherType weatherType, int32_t numSteps);
    static void ReleaseWeather();
    static void RenderRainStreaks();
    static void SetWeatherToAppropriateTypeNow();
    static void Update();
    static void UpdateInTunnelness();
    /*!
    * @notsa
    * @details Based on code @ `0x72A640`
    * @return The corresponding weather region at a given 2D position, or `WEATHER_REGION_DEFAULT` if no specific region was found
    */
    static eWeatherRegion FindWeatherRegion(CVector2D pos);
    static void UpdateWeatherRegion(CVector* posn);
    static bool IsRainy();

    static bool IsUnderWater() { return UnderWaterness > 0.0f; } // NOTSA
};
