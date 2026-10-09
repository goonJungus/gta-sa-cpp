// CWeather - adapted from Ghidra decomp for clean-room C++ build
// Decompiled bodies: src/CWeather/*.c
// Binary: gta_sa.exe 1.0 (US)

#include "CWeather.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

// ============================================================
// Shim block: external subsystem interfaces used by CWeather.
// The RW/D3D9 render layer is NOT ported (replaced by Bevy later);
// game-side logic (weather state, interpolation, particles) is ported.
// TODO(port): replace these shims with real subsystem bindings.
// ============================================================

// ---- CTimer ----
struct CTimerPort {
    static uint32_t m_FrameCounter;       // 0xA4025C-ish
    static uint32_t m_snTimeInMilliseconds;
    static float ms_fTimeStep;
    static bool m_CodePause;
};
uint32_t CTimerPort::m_FrameCounter = 0;
uint32_t CTimerPort::m_snTimeInMilliseconds = 0;
float CTimerPort::ms_fTimeStep = 0.0f;
bool CTimerPort::m_CodePause = false;

// ---- CClock ----
struct CClockPort {
    static uint8_t ms_nGameClockHours;
    static uint8_t ms_nGameClockMinutes;
    static uint8_t ms_nGameClockSeconds;
};
uint8_t CClockPort::ms_nGameClockHours = 12;
uint8_t CClockPort::ms_nGameClockMinutes = 0;
uint8_t CClockPort::ms_nGameClockSeconds = 0;

// ---- CCullZones ----
struct CCullZonesPort {
    static uint32_t CurrentFlags_Camera;
    static bool CamNoRain() { return false; }      // TODO(port)
    static bool PlayerNoRain() { return false; }   // TODO(port)
};
uint32_t CCullZonesPort::CurrentFlags_Camera = 0;

// ---- CGame ----
struct CGamePort {
    static int32_t currArea; // 0 = normal world
};
int32_t CGamePort::currArea = 0;

// ---- CTimeCycle (only the fields CWeather touches) ----
struct CTimeCyclePort {
    static CVector m_VectorToSun[16];
    static int32_t m_CurrentStoredValue;
    struct Colours { int32_t m_nWaterFogAlpha; };
    static Colours m_CurrentColours;
};
CVector CTimeCyclePort::m_VectorToSun[16] = {};
int32_t CTimeCyclePort::m_CurrentStoredValue = 0;
CTimeCyclePort::Colours CTimeCyclePort::m_CurrentColours = {};

// ---- CPostEffects (fields only) ----
struct CPostEffectsPort {
    static float m_fHeatHazeFXFadeSpeed;
    static float m_fHeatHazeFXInsideBuildingFadeSpeed;
    static int32_t m_HeatHazeFXHourOfDayStart;
    static int32_t m_HeatHazeFXHourOfDayEnd;
};
float CPostEffectsPort::m_fHeatHazeFXFadeSpeed = 0.02f;
float CPostEffectsPort::m_fHeatHazeFXInsideBuildingFadeSpeed = 0.05f;
int32_t CPostEffectsPort::m_HeatHazeFXHourOfDayStart = 10;
int32_t CPostEffectsPort::m_HeatHazeFXHourOfDayEnd = 18;

// ---- CVector helpers ----
static void VecNormalise(CVector& v) { // TODO(port): CVector::Normalise
    float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (len > 0.0f) { v.x /= len; v.y /= len; v.z /= len; }
}
static float VecMagnitude(const CVector& v) { // TODO(port): CVector::Magnitude
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

// ---- CCollision ----
struct CCollisionPort {
    // TODO(port): real DistToLine
    static float DistToLine(const CVector*, const CVector*, const CVector*) { return 1e9f; }
};

// ---- Player/world finders (TODO(port): real entity system) ----
struct CEntityPort { int m_AreaCode = 0; };
static CEntityPort* FindPlayerPedPort(int) { return nullptr; }
static void* FindPlayerVehiclePort(int, bool) { return nullptr; }
static void FindPlayerCoorsPort(CVector* out, int) { out->x = out->y = out->z = 0.0f; }

// Camera position/forward (DAT_00b6f02c / DAT_00b6f03c+0x30 in 1.0).
// TODO(port): bind to the real camera.
static CVector s_camPos{ 0.0f, 0.0f, 0.0f };
static float  s_camHeading = 0.0f; // DAT_00b6f038

// Rain-audio fade counter at 0xB79E48: 0..64, driven by the audio engine
// (decrements toward 0 while raining, increments toward 64 when dry).
// TODO(port): bind to the real audio state.
static int32_t s_rainAudioFade = 64;

// Tunnel segments (0xC81424 / 0xC81430, initialized on first use).
// TODO(port): confirm these are static consts in the real build.
static CVector s_tunnelSegA{ 0.0f, 0.0f, 0.0f };
static CVector s_tunnelSegB{ 0.0f, 0.0f, 0.0f };
static uint32_t s_tunnelSegInit = 0;

// FxSystem particle hooks. TODO(port): real FxSystem_c.
struct FxPrtMultPort { float r, g, b, a, size, rotation; int32_t a2, b2; };
static void AddParticlePort(int, const CVector*, const CVector*, float,
                            const FxPrtMultPort*, float, float, float, bool) {}

// Rain haze alpha (file-static in 1.0, s_RainHazeAlpha).
static float s_rainHazeAlpha = 0.0f;
// Set after rain stops; counts down while rain is off.
static bool s_rainedRecently = false;

// 1.0 float->int casts compile to FPU fistp (round-to-nearest).
static int32_t FloatToInt(float v) { return int32_t(std::nearbyint(v)); }

// Random helpers. TODO(port): CGeneral.
static uint32_t GetRandomNumber() { return uint32_t(rand()); } // TODO(port)
static float GetRandomNumberInRange(float lo, float hi) {       // TODO(port)
    return lo + (hi - lo) * (float(rand()) / float(RAND_MAX));
}
static int32_t GetRandomNumberInRangeInt(int32_t lo, int32_t hi) { // TODO(port)
    return lo + (rand() % (hi - lo + 1));
}

// ============================================================
// Static member definitions
// ============================================================
float    CWeather::TrafficLightsBrightness = 0.0f;
bool     CWeather::bScriptsForceRain = false;
float    CWeather::Earthquake = 0.0f;
uint32_t CWeather::CurrentRainParticleStrength = 0;
uint32_t CWeather::LightningStartY = 0;
uint32_t CWeather::LightningStartX = 0;
int32_t  CWeather::LightningFlashLastChange = 0;
uint32_t CWeather::WhenToPlayLightningSound = 0;
uint32_t CWeather::LightningDuration = 0;
uint32_t CWeather::LightningStart = 0;
bool     CWeather::LightningFlash = false;
bool     CWeather::LightningBurst = false;
float    CWeather::HeadLightsSpectrum = 0.0f;
float    CWeather::WaterFogFXControl = 0.0f;
float    CWeather::HeatHazeFXControl = 0.0f;
float    CWeather::HeatHaze = 0.0f;
float    CWeather::SunGlare = 0.0f;
float    CWeather::Rainbow = 0.0f;
float    CWeather::Wavyness = 0.0f;
float    CWeather::WindClipped = 0.0f;
CVector  CWeather::WindDir{ 0.0f, 0.0f, 0.0f };
float    CWeather::Wind = 0.0f;
float    CWeather::Sandstorm = 0.0f;
float    CWeather::Rain = 0.0f;
float    CWeather::InTunnelness = 0.0f;
float    CWeather::WaterDepth = 0.0f;
float    CWeather::UnderWaterness = 0.0f;
float    CWeather::ExtraSunnyness = 0.0f;
float    CWeather::Foggyness_SF = 0.0f;
float    CWeather::Foggyness = 0.0f;
float    CWeather::CloudCoverage = 0.0f;
float    CWeather::WetRoads = 0.0f;
float    CWeather::InterpolationValue = 0.0f;
uint32_t CWeather::WeatherTypeInList = 0;
eWeatherRegion CWeather::WeatherRegion = WEATHER_REGION_DEFAULT;
eWeatherType CWeather::ForcedWeatherType = WEATHER_UNDEFINED;
eWeatherType CWeather::NewWeatherType = WEATHER_EXTRASUNNY_LA;
eWeatherType CWeather::OldWeatherType = WEATHER_EXTRASUNNY_LA;
int32_t  CWeather::StreamAfterRainTimer = 0;
float    CWeather::HeatHazeFXFade = 0.0f;
int32_t  CWeather::HeatHazeFXLastMinute = 0;
float    CWeather::WaterFogFXFade = 0.0f;
bool     CWeather::WaterFogFXFadingOut = false;
std::array<float, 16> CWeather::saTreeWindOffsets = {};
std::array<float, 32> CWeather::saBannerWindOffsets = {};
// m_WeatherAudioEntity: incomplete type in this TU (defined with audio).
// CAEWeatherAudioEntity CWeather::m_WeatherAudioEntity;

// ---- Data tables (from gta_sa.exe 1.0) ----
// Wind strength per weather type (0x8D5E50, 23 floats).
static const float s_windByWeather[23] = {
    0.0f, 0.25f, 0.0f, 0.2f, 0.7f, 0.25f, 0.0f, 0.7f, 1.0f, 0.0f,
    0.2f, 0.0f, 0.4f, 0.0f, 0.3f, 0.7f, 1.0f, 0.0f, 0.3f, 1.5f,
    0.0f, 0.0f, 0.0f
};
// Wind direction tables (0x8D5FF4: 17 floats, 0x8D6038: 16 floats).
static const float s_windDirA[17] = {
    0.0f, 1.0f, 0.5f, 1.0f, 0.2f, 0.4f, 1.0f, 1.0f, 1.0f, 1.0f,
    0.8f, 0.0f, 1.0f, 1.0f, 0.7f, 1.0f, 1.0f
};
static const float s_windDirB[16] = {
    0.5f, -0.3f, 0.8f, 0.0f, -0.4f, -0.8f, 0.3f, -0.1f,
    -0.9f, -0.5f, 0.7f, 0.7f, 0.3f, 0.7f, 0.0f, -0.5f
};
// Weather-type lists per region (64 entries each).
static const int8_t s_weatherListDefault[64] = {
    13,13,13,13,13,13,13,13,14,15,9,9,15,14,13,14,
    14,13,13,13,13,14,14,13,13,13,13,13,13,13,13,13,
    13,13,14,14,14,14,14,14,15,9,9,15,16,16,16,16,
    15,14,14,14,14,13,13,13,13,13,13,13,13,13,13,13
};
static const int8_t s_weatherListLA[64] = {
    2,2,0,0,0,2,2,2,3,0,3,2,0,0,3,3,
    3,4,4,0,0,1,1,0,2,2,2,2,2,2,2,2,
    2,2,3,3,3,3,3,3,2,2,3,3,0,0,16,0,
    4,1,1,1,1,0,0,2,2,2,2,2,2,2,2,2
};
static const int8_t s_weatherListSF[64] = {
    5,5,5,5,5,7,7,9,9,7,5,5,5,5,5,7,
    7,7,8,8,7,7,5,5,5,5,6,6,5,7,9,9,
    7,5,5,7,5,5,6,6,5,5,5,7,7,8,8,7,
    7,9,9,5,5,6,6,6,5,5,6,7,7,6,6,5
};
static const int8_t s_weatherListLV[64] = {
    11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
    11,11,11,10,10,10,10,10,11,11,11,11,11,11,11,11,
    11,10,11,11,11,11,10,11,11,11,11,11,11,11,11,11,
    11,11,11,11,11,11,11,11,10,10,12,12,10,12,10,10
};
static const int8_t s_weatherListDesert[64] = {
    17,17,17,19,19,17,17,17,17,17,17,17,17,17,17,17,
    17,17,17,18,18,18,18,18,17,17,17,17,17,17,17,17,
    17,17,17,17,17,19,19,19,17,17,17,17,17,17,17,17,
    17,17,18,18,18,17,17,17,17,17,17,17,19,19,19,17
};

static const int8_t* WeatherListForRegion(eWeatherRegion r) {
    switch (r) {
    case WEATHER_REGION_LA:     return s_weatherListLA;
    case WEATHER_REGION_SF:     return s_weatherListSF;
    case WEATHER_REGION_LV:     return s_weatherListLV;
    case WEATHER_REGION_DESERT: return s_weatherListDesert;
    default:                    return s_weatherListDefault;
    }
}

// mod-64 index used by ForecastWeather/Update (handles negative via the
// 0x8000003f mask trick in 1.0).
static uint32_t Mod64(int32_t v) {
    uint32_t u = uint32_t(v) & 0x8000003f;
    if (int32_t(u) < 0)
        u = (u - 1 | 0xffffffc0) + 1;
    return u;
}

// ============================================================
// Methods
// ============================================================

void CWeather::Init() {
    // src/CWeather/Init_0072a480.c
    NewWeatherType = WEATHER_EXTRASUNNY_LA;
    OldWeatherType = WEATHER_EXTRASUNNY_LA;
    WeatherRegion = WEATHER_REGION_DEFAULT;
    InterpolationValue = 0.0f;
    WeatherTypeInList = 0;
    ForcedWeatherType = WEATHER_UNDEFINED;
    WhenToPlayLightningSound = 0;
    bScriptsForceRain = false;
    Rain = 0.0f;
    Sandstorm = 0.0f;
    CurrentRainParticleStrength = 0;
    InTunnelness = 0.0f;
    LightningStartX = 0;
    LightningStartY = 0;
    StreamAfterRainTimer = 0;
}

void CWeather::ForceWeather(eWeatherType weatherType) {
    // src/CWeather/ForceWeather_0072a4e0.c
    ForcedWeatherType = weatherType;
}

void CWeather::ForceWeatherNow(eWeatherType weatherType) {
    // src/CWeather/ForceWeatherNow_0072a4f0.c
    ForcedWeatherType = weatherType;
    OldWeatherType = weatherType;
    NewWeatherType = weatherType;
}

void CWeather::ReleaseWeather() {
    // src/CWeather/ReleaseWeather_0072a510.c
    ForcedWeatherType = WEATHER_UNDEFINED;
}

const eWeatherType* CWeather::FindWeatherTypesList() {
    // src/CWeather/FindWeatherTypesList_0072a520.c
    return reinterpret_cast<const eWeatherType*>(WeatherListForRegion(WeatherRegion));
}

bool CWeather::ForecastWeather(eWeatherType weatherType, int32_t numSteps) {
    // src/CWeather/ForecastWeather_0072a590.c
    if (numSteps < 0)
        return false;
    const int8_t* list = WeatherListForRegion(WeatherRegion);
    for (int32_t i = 0; i <= numSteps; ++i) {
        if (list[Mod64(int32_t(WeatherTypeInList) + i)] == int8_t(weatherType))
            return true;
    }
    return false;
}

eWeatherRegion CWeather::FindWeatherRegion(CVector2D pos) {
    // src/CWeather/FindWeatherRegion_0072a640.c
    // NOTE: 1.0 takes a float* (null = use camera pos); the header exposes
    // CVector2D. pos is used directly here.
    // TODO(port): null-pos camera fallback (DAT_00b6f02c / DAT_00b6f03c).
    float x = pos.x, y = pos.y;
    if (1000.0f < x && 910.0f < y) {
        WeatherRegion = WEATHER_REGION_LV;
        return WeatherRegion;
    }
    if (-850.0f < x && x < 1000.0f && 1280.0f < y) {
        WeatherRegion = WEATHER_REGION_DESERT;
        return WeatherRegion;
    }
    if (x < -1430.0f && -580.0f < y && y < 1430.0f) {
        WeatherRegion = WEATHER_REGION_SF;
        return WeatherRegion;
    }
    if (x <= 250.0f || 3000.0f <= x || y <= -3000.0f || -850.0f <= y) {
        // (the decomp's comma-expression assigns LA then tests -850<=y;
        // net effect: LA only if -850<=y, else DEFAULT)
        if (-850.0f <= y)
            WeatherRegion = WEATHER_REGION_LA;
        else
            WeatherRegion = WEATHER_REGION_DEFAULT;
    } else {
        WeatherRegion = WEATHER_REGION_DEFAULT;
    }
    return WeatherRegion;
}

void CWeather::UpdateWeatherRegion(CVector* posn) {
    // NOTSA helper; 1.0 inlines this via FindWeatherRegion.
    // TODO(port): confirm against 1.0 (no direct decomp).
    if (posn)
        FindWeatherRegion(CVector2D{ posn->x, posn->y });
}

bool CWeather::IsRainy() {
    // src/CWeather/IsRainy_004abf50.c
    return 0.2f <= Rain;
}

void CWeather::SetWeatherToAppropriateTypeNow() {
    // src/CWeather/SetWeatherToAppropriateTypeNow_0072a790.c
    CVector pos{};
    FindPlayerCoorsPort(&pos, -1); // TODO(port)
    FindWeatherRegion(CVector2D{ pos.x, pos.y });
    const int8_t* list = WeatherListForRegion(WeatherRegion);
    ForcedWeatherType = WEATHER_UNDEFINED;
    OldWeatherType = eWeatherType(list[0]);
    NewWeatherType = eWeatherType(list[0]);
}

void CWeather::UpdateInTunnelness() {
    // src/CWeather/UpdateInTunnelness_0072b630.c
    float target = 0.0f;
    if ((CCullZonesPort::CurrentFlags_Camera & 0x2000u) != 0) {
        target = 1.0f;
        float closest = 100.0f;
        CVector camPos = s_camPos; // TODO(port): real camera pos
        camPos.z = 0.0f;
        CVector dir{ -std::sin(s_camHeading), std::cos(s_camHeading), 0.0f }; // TODO(port)
        VecNormalise(dir);
        CVector far{ camPos.x + dir.x * 100.0f, camPos.y + dir.y * 100.0f,
                     camPos.z + dir.z * 100.0f };
        if ((s_tunnelSegInit & 1) == 0) {
            s_tunnelSegInit |= 1;
            s_tunnelSegA = CVector{ 85.0f, -1023.0f, 0.0f }; // 0x42AA0000, 0xC47F0000
        }
        if ((s_tunnelSegInit & 2) == 0) {
            s_tunnelSegInit |= 2;
            s_tunnelSegB = CVector{ 1065.0f, -1566.0f, 0.0f }; // 0x44D26000, 0xC4F48000
        }
        float d = CCollisionPort::DistToLine(&camPos, &far, &s_tunnelSegA); // TODO(port)
        if (d <= 100.0f) closest = d;
        d = CCollisionPort::DistToLine(&camPos, &far, &s_tunnelSegB); // TODO(port)
        if (d <= closest) closest = d;
        if (closest * 0.01f <= 1.0f)
            target = closest * 0.01f;
    }
    float step = CTimerPort::ms_fTimeStep * 0.01f;
    if (step <= std::fabs(target - InTunnelness)) {
        InTunnelness += (target >= InTunnelness) ? step : -step;
    } else {
        InTunnelness = target;
    }
}

void CWeather::AddSandStormParticles() {
    // src/CWeather/AddSandStormParticles_0072a820.c
    FxPrtMultPort mult{ 0.67f, 0.6f, 0.4f, 0.25f, 1.0f, 0.0f, 0, 0 }; // TODO(port): exact FxPrtMult_c
    CVector pos{ s_camPos.x, s_camPos.y, s_camPos.z }; // TODO(port)
    pos.x += GetRandomNumberInRange(-20.0f, 20.0f);
    pos.y += GetRandomNumberInRange(-20.0f, 20.0f);
    pos.z += GetRandomNumberInRange(-2.0f, 5.0f); // (rand*7 - 2.0); 1.0's _unk_00858ca0 = 2.0
    CVector vel{ WindDir.x * 25.0f, WindDir.y * 25.0f, WindDir.z * 25.0f };
    AddParticlePort(0, &pos, &vel, 0.0f, &mult, -1.0f, 1.2f, 0.6f, false); // TODO(port)
}

void CWeather::AddRain() {
    // src/CWeather/AddRain_0072a9a0.c
    if (CCullZonesPort::CamNoRain()) return;      // TODO(port)
    if (CCullZonesPort::PlayerNoRain()) return;   // TODO(port)
    if (0.0f < UnderWaterness) return;
    if (CGamePort::currArea != 0) return;
    CEntityPort* ped = FindPlayerPedPort(-1);      // TODO(port)
    if (ped && ped->m_AreaCode != 0) return;
    if (900.0f < s_camPos.z) return;
    // TODO(port): first-person roof check (CCamera::GetLookingLRBFirstPerson,
    // FindPlayerVehicle, CVehicle::CarHasRoof).

    if (Rain <= 0.0f) {
        if (s_rainedRecently) {
            if (0 < StreamAfterRainTimer) {
                StreamAfterRainTimer--;
            } else {
                s_rainedRecently = false;
            }
        }
    } else {
        s_rainedRecently = true;
    }
    StreamAfterRainTimer = 800;

    if (1.01f < Wind && !CCullZonesPort::CamNoRain() && // TODO(port)
        !CCullZonesPort::PlayerNoRain() && UnderWaterness == 0.0f) {
        AddSandStormParticles();
    }

    if (0.1f < Rain || s_rainHazeAlpha != 0.0f) {
        // Splash counts: (int)(Rain*5) outer, (15 - (int)(Rain*10)) inner.
        // (0x821B40 = fistp; constants 5.0 @0x858C80, 10.0 @0x85862C verified.)
        int32_t count = FloatToInt(Rain * 5.0f);
        int32_t inner = 15 - FloatToInt(Rain * 10.0f);
        FxPrtMultPort mult{ 1.0f, 1.0f, 1.0f, 0.25f, 0.0f, 0.0f, 0, 0 }; // TODO(port)
        CVector zero{};
        for (int32_t i = 0; i < count; ++i) {
            float radius = GetRandomNumberInRange(0.0f, 40.0f); // TODO(port): exact radius
            float ang;
            if ((GetRandomNumber() & 1) == 0)
                ang = float(int32_t(GetRandomNumber() & 0xff) - 0x80) * 0.00625f + s_camHeading;
            else
                ang = float(GetRandomNumber() & 0xff) * 0.02453125f;
            CVector ground{ s_camPos.x + std::sin(ang) * radius,
                           s_camPos.y + std::cos(ang) * radius, 0.0f };
            // TODO(port): CWorld::ProcessVerticalLine for ground Z; splash
            // particles at ground+0.1, `inner` per splash.
            (void)inner; (void)zero; (void)mult; (void)ground;
        }
        // Rain haze alpha easing.
        if (s_rainHazeAlpha < Rain * 0.2f) s_rainHazeAlpha += 0.0025f;
        if (Rain * 0.2f < s_rainHazeAlpha) s_rainHazeAlpha -= 0.0025f;
        if (s_rainHazeAlpha < 0.0f) s_rainHazeAlpha = 0.0f;
        if (1.0f < s_rainHazeAlpha) s_rainHazeAlpha = 1.0f;
        // TODO(port): haze particle via FxSystem_c::AddParticle.
    }
}

void CWeather::RenderRainStreaks() {
    // src/CWeather/RenderRainStreaks_0072af70.c
    // Rain streaks are camera-facing line primitives (RwIm3D). The RW render
    // calls are TODO(port); the streak-state update logic is ported.
    if (CTimerPort::m_CodePause)
        return;
    // Target strength: (64 - rainAudioFade) * (int)(Rain*110) / 64.
    // (0xB79E48 0..64 counter; 0x821B40 = fistp. Verified by disassembly.)
    int32_t target = (64 - s_rainAudioFade) * FloatToInt(Rain * 110.0f) / 64;
    // Approach by one per frame, clamp at zero.
    // (1.0's borrow-flag dance reduces to this; kept simple - VERIFY the
    // exact edge behavior against 1.0 if streak counts matter.)
    if (int32_t(CurrentRainParticleStrength) < target)
        CurrentRainParticleStrength++;
    else if (target < int32_t(CurrentRainParticleStrength))
        CurrentRainParticleStrength--;
    if (int32_t(CurrentRainParticleStrength) < 0)
        CurrentRainParticleStrength = 0;

    if (CurrentRainParticleStrength == 0) return;
    if (CCullZonesPort::CamNoRain()) return;    // TODO(port)
    if (CCullZonesPort::PlayerNoRain()) return; // TODO(port)
    if (0.0f < UnderWaterness) return;
    if (CGamePort::currArea != 0) return;
    if (900.0f < s_camPos.z) return;

    // Streak slots (32). Each has int xyz pos + strength byte.
    // TODO(port): move to file-statics with the same layout.
    struct Streak { int32_t x, y, z; uint8_t strength; };
    static Streak s_streaks[32] = {};
    static bool s_streaksInit = false;
    if (!s_streaksInit) {
        s_streaksInit = true;
        for (auto& s : s_streaks) {
            s.x = s.y = s.z = 0;
            s.strength = uint8_t(GetRandomNumber()); // TODO(port)
        }
    }
    for (uint32_t i = 0; i < 32; ++i) {
        Streak& st = s_streaks[i];
        CVector rel{ float(st.x) - s_camPos.x, float(st.y) - s_camPos.y,
                     float(st.z) - s_camPos.z };
        if (st.strength == 0 || st.z < 0 || 8.0f < VecMagnitude(rel)) {
            // Respawn near the camera.
            st.x = FloatToInt(GetRandomNumberInRange(-40.0f, 40.0f) + s_camPos.x); // TODO(port): exact ranges
            st.y = FloatToInt(GetRandomNumberInRange(-40.0f, 40.0f) + s_camPos.y);
            st.z = FloatToInt(GetRandomNumberInRange(0.0f, 40.0f) + s_camPos.z);
            st.strength = uint8_t(GetRandomNumber());
        }
        // Fall speed; strength decays by a random 2..5.
        st.z = FloatToInt(float(st.z) - 1.0f); // TODO(port): exact fall rate
        int32_t decay = GetRandomNumberInRangeInt(2, 5);
        st.strength = (decay < st.strength) ? uint8_t(st.strength - decay) : 0;
    }
    // TODO(port): build the RwIm3D vertex/index temp buffers and issue
    // RwIm3DTransform / RwIm3DRenderIndexedPrimitive / RwIm3DEnd with the
    // render states from the decomp (8,0 / 6,1 / 0xe,0 / 0x10,1 / 10,5 /
    // 0xb,6 / 0xc,1 / 1,0, then restore).
}

void CWeather::Update() {
    // src/CWeather/Update_0072b850.c
    if ((CTimerPort::m_FrameCounter & 0xf) == 0)
        FindWeatherRegion(CVector2D{ s_camPos.x, s_camPos.y }); // TODO(port): null-pos

    eWeatherType old = OldWeatherType;
    float interp = InterpolationValue;
    // TODO(port): CReplay::Mode check.
    {
        float clockHours = (float(CClockPort::ms_nGameClockSeconds) * 0.016666668f +
                            float(CClockPort::ms_nGameClockMinutes)) * 0.016666668f;
        if (clockHours < InterpolationValue) {
            FindWeatherRegion(CVector2D{ s_camPos.x, s_camPos.y }); // TODO(port)
            old = NewWeatherType;
            OldWeatherType = NewWeatherType;
            if (ForcedWeatherType < WEATHER_EXTRASUNNY_LA) {
                if (s_camPos.z < 950.0f) { // TODO(port): player z, not camera
                    WeatherTypeInList = Mod64(int32_t(WeatherTypeInList) + 1);
                    old = eWeatherType(WeatherListForRegion(WeatherRegion)[WeatherTypeInList]);
                    NewWeatherType = old;
                }
            } else {
                NewWeatherType = ForcedWeatherType;
            }
        }
        interp = clockHours;
    }
    InterpolationValue = interp;

    // Lightning.
    bool rainyOld = (old == WEATHER_RAINY_COUNTRYSIDE || old == WEATHER_RAINY_SF);
    bool rainyNew = (NewWeatherType == WEATHER_RAINY_COUNTRYSIDE ||
                     NewWeatherType == WEATHER_RAINY_SF);
    if (!rainyNew || !rainyOld ||
        CCullZonesPort::CamNoRain() || CCullZonesPort::PlayerNoRain() || // TODO(port)
        UnderWaterness != 0.0f || CGamePort::currArea != 0) {
        LightningFlash = false;
        LightningBurst = false;
    } else if (!LightningBurst) {
        if ((GetRandomNumber() & 0xffff) < 200) { // TODO(port)
            LightningStart = CTimerPort::m_FrameCounter;
            LightningFlashLastChange = int32_t(CTimerPort::m_snTimeInMilliseconds);
            LightningBurst = true;
            LightningFlash = true;
        } else {
            LightningFlash = false;
        }
    } else {
        if ((GetRandomNumber() & 0xff) < 0x18) { // TODO(port)
            LightningBurst = false;
            LightningDuration = 0x14;
            if (CTimerPort::m_FrameCounter - LightningStart < 0x15)
                LightningDuration = CTimerPort::m_FrameCounter - LightningStart;
            WhenToPlayLightningSound =
                (0x14 - LightningDuration) * 0x96 + CTimerPort::m_snTimeInMilliseconds;
            LightningFlash = false;
        } else if (0x32 < CTimerPort::m_snTimeInMilliseconds - uint32_t(LightningFlashLastChange)) {
            bool was = LightningFlash;
            LightningFlash = (GetRandomNumber() & 1) != 0; // TODO(port)
            if (was != LightningFlash)
                LightningFlashLastChange = int32_t(CTimerPort::m_snTimeInMilliseconds);
        }
    }
    if (WhenToPlayLightningSound != 0 &&
        WhenToPlayLightningSound < CTimerPort::m_snTimeInMilliseconds) {
        // TODO(port): CAEWeatherAudioEntity::AddAudioEvent(AE_THUNDER),
        // CPad::StartShake.
        WhenToPlayLightningSound = 0;
    }

    // WetRoads.
    if (rainyOld)
        WetRoads = rainyNew ? 1.0f : 1.0f - InterpolationValue;
    else if (rainyNew)
        WetRoads = InterpolationValue;
    else
        WetRoads = 0.0f;

    // Rain easing toward target.
    {
        float target = 0.0f;
        if (rainyNew) target = InterpolationValue;
        if (rainyOld) target += 1.0f - InterpolationValue;
        float wobble = float((CTimerPort::m_snTimeInMilliseconds >> 0xd) & 3) * 0.1f + 0.7f;
        target *= wobble;
        float step = CTimerPort::ms_fTimeStep * 0.005f;
        if (step <= std::fabs(target - Rain))
            Rain += (target >= Rain) ? step : -step;
        else
            Rain = target;
    }
    // Sandstorm easing.
    {
        float target = 0.0f;
        if (NewWeatherType == WEATHER_SANDSTORM_DESERT) target = InterpolationValue;
        if (old == WEATHER_SANDSTORM_DESERT) target += 1.0f - InterpolationValue;
        float wobble = float((CTimerPort::m_snTimeInMilliseconds >> 0xd) & 3) * 0.1f + 0.7f;
        target *= wobble;
        float step = CTimerPort::ms_fTimeStep * 0.005f;
        if (step <= std::fabs(target - Sandstorm))
            Sandstorm += (target >= Sandstorm) ? step : -step;
        else
            Sandstorm = target;
    }

    auto isSunny = [](eWeatherType w) {
        return w == WEATHER_SUNNY_LA || w == WEATHER_SUNNY_SMOG_LA ||
               w == WEATHER_SUNNY_COUNTRYSIDE || w == WEATHER_SUNNY_SF ||
               w == WEATHER_SUNNY_VEGAS || w == WEATHER_SUNNY_DESERT ||
               w == WEATHER_EXTRASUNNY_LA || w == WEATHER_EXTRASUNNY_SMOG_LA ||
               w == WEATHER_EXTRASUNNY_COUNTRYSIDE || w == WEATHER_EXTRASUNNY_SF ||
               w == WEATHER_EXTRASUNNY_VEGAS || w == WEATHER_EXTRASUNNY_DESERT;
    };
    auto isExtraSunny = [](eWeatherType w) {
        return w == WEATHER_EXTRASUNNY_LA || w == WEATHER_EXTRASUNNY_SMOG_LA ||
               w == WEATHER_EXTRASUNNY_COUNTRYSIDE || w == WEATHER_EXTRASUNNY_SF ||
               w == WEATHER_EXTRASUNNY_VEGAS || w == WEATHER_EXTRASUNNY_DESERT;
    };

    // CloudCoverage.
    CloudCoverage = isSunny(old) ? 0.0f : 1.0f - InterpolationValue;
    if (!isSunny(NewWeatherType))
        CloudCoverage += InterpolationValue;

    // Foggyness.
    Foggyness = (old == WEATHER_FOGGY_SF || old == WEATHER_SANDSTORM_DESERT)
                    ? 1.0f - InterpolationValue : 0.0f;
    if (NewWeatherType == WEATHER_FOGGY_SF || NewWeatherType == WEATHER_SANDSTORM_DESERT)
        Foggyness += InterpolationValue;
    Foggyness_SF = (old == WEATHER_FOGGY_SF) ? 1.0f - InterpolationValue : 0.0f;
    if (NewWeatherType == WEATHER_FOGGY_SF)
        Foggyness_SF += InterpolationValue;

    // ExtraSunnyness.
    ExtraSunnyness = isExtraSunny(old) ? 1.0f - InterpolationValue : 0.0f;
    if (isExtraSunny(NewWeatherType))
        ExtraSunnyness += InterpolationValue;

    // Rainbow: cloudy->sunny transitions, daytime only.
    {
        bool cloudyOld = (old == WEATHER_CLOUDY_LA || old == WEATHER_CLOUDY_COUNTRYSIDE ||
                          old == WEATHER_CLOUDY_VEGAS || old == WEATHER_CLOUDY_SF);
        bool sunnyNew = (NewWeatherType == WEATHER_SUNNY_LA ||
                         NewWeatherType == WEATHER_SUNNY_SMOG_LA ||
                         NewWeatherType == WEATHER_SUNNY_COUNTRYSIDE ||
                         NewWeatherType == WEATHER_SUNNY_SF ||
                         NewWeatherType == WEATHER_SUNNY_VEGAS ||
                         NewWeatherType == WEATHER_SUNNY_DESERT);
        if ((cloudyOld || !sunnyNew) ||
            (0.5f <= InterpolationValue || CClockPort::ms_nGameClockHours < 7 ||
             0x14 < CClockPort::ms_nGameClockHours)) {
            Rainbow = 0.0f;
        } else {
            Rainbow = 1.0f - std::fabs(InterpolationValue - 0.25f) * 4.0f;
        }
    }

    // SunGlare.
    SunGlare = isSunny(old) ? 1.0f - InterpolationValue : 0.0f;
    if (isSunny(NewWeatherType))
        SunGlare += InterpolationValue;
    if (0.0f < SunGlare) {
        float sunZ = CTimeCyclePort::m_VectorToSun[CTimeCyclePort::m_CurrentStoredValue].z * 7.0f; // TODO(port)
        SunGlare *= std::clamp(sunZ, 0.0f, 1.0f);
        SunGlare = std::clamp(SunGlare, 0.0f, 1.0f);
        // TODO(port): CSpecialFX::bSnapShotActive check + random flicker.
    }

    // HeatHaze.
    {
        bool hot = (old == WEATHER_SUNNY_DESERT || old == WEATHER_EXTRASUNNY_DESERT ||
                    old == WEATHER_EXTRASUNNY_VEGAS || old == WEATHER_EXTRASUNNY_LA);
        HeatHaze = hot ? 1.0f - InterpolationValue : 0.0f;
        hot = (NewWeatherType == WEATHER_SUNNY_DESERT ||
               NewWeatherType == WEATHER_EXTRASUNNY_DESERT ||
               NewWeatherType == WEATHER_EXTRASUNNY_VEGAS ||
               NewWeatherType == WEATHER_EXTRASUNNY_LA);
        if (hot) HeatHaze += InterpolationValue;
    }
    if (0.0f < HeatHaze) {
        // TODO(port): full HeatHazeFXControl state machine (hour window,
        // inside-building fade, minute tracking). Simplified: fade toward
        // target by m_fHeatHazeFXFadeSpeed.
        bool minuteChanged = (CClockPort::ms_nGameClockMinutes !=
                              uint8_t(HeatHazeFXLastMinute));
        bool inWindow = (CPostEffectsPort::m_HeatHazeFXHourOfDayStart <=
                             int(CClockPort::ms_nGameClockHours) &&
                         int(CClockPort::ms_nGameClockHours) <
                             CPostEffectsPort::m_HeatHazeFXHourOfDayEnd);
        bool inside = (CGamePort::currArea != 0); // TODO(port): + ped area + NoRain zones
        float speed = inside ? CPostEffectsPort::m_fHeatHazeFXInsideBuildingFadeSpeed
                             : CPostEffectsPort::m_fHeatHazeFXFadeSpeed;
        if (minuteChanged) {
            if (inWindow && !inside)
                HeatHazeFXFade = std::min(1.0f, HeatHazeFXFade + speed);
            else
                HeatHazeFXFade = std::max(0.0f, HeatHazeFXFade - speed);
        }
        HeatHazeFXControl = HeatHazeFXFade * HeatHaze;
        HeatHazeFXLastMinute = CClockPort::ms_nGameClockMinutes;
    }

    // WaterFogFX.
    {
        float target = float(CTimeCyclePort::m_CurrentColours.m_nWaterFogAlpha) * 0.01f; // TODO(port)
        if (CTimeCyclePort::m_CurrentColours.m_nWaterFogAlpha == 0) {
            WaterFogFXFade = 0.0f;
            if (!WaterFogFXFadingOut && WaterFogFXFade < target)
                WaterFogFXFade = target;
        } else if (CTimeCyclePort::m_CurrentColours.m_nWaterFogAlpha < 0x5f) {
            if (!WaterFogFXFadingOut && WaterFogFXFade < target)
                WaterFogFXFade = target;
        } else {
            WaterFogFXFadingOut = true;
            if (target < WaterFogFXFade) WaterFogFXFade = target;
            if (WaterFogFXFade <= 0.0f) WaterFogFXFadingOut = false;
        }
        WaterFogFXControl = std::clamp(WaterFogFXFade * 1.4f, 0.0f, 1.0f);
    }

    // Wind (interpolated) and WindDir (time-varying).
    Wind = s_windByWeather[NewWeatherType] * InterpolationValue +
           (1.0f - InterpolationValue) * s_windByWeather[old];
    WindClipped = std::min(Wind, 1.0f);
    {
        uint32_t ms = CTimerPort::m_snTimeInMilliseconds;
        uint32_t i10 = (ms >> 10) & 0xf;
        float t1 = 0.5f - std::cos(float(ms & 0x3ff) * 0.0009765625f * 3.1415927f) * 0.5f;
        float t2 = 1.0f - t1;
        float dx = (t2 * s_windDirB[i10] + t1 * s_windDirB[(i10 + 1) & 0xf]) *
                       WindClipped * 0.4f + WindClipped * 0.7f;
        float dy = (t1 * s_windDirB[(i10 + 4) & 0xf] + t2 * s_windDirB[(i10 + 3) & 0xf]) *
                       WindClipped * 0.4f + WindClipped * 0.7f;
        float dz = (t1 * s_windDirB[(i10 + 7) & 0xf] + t2 * s_windDirB[(i10 + 6) & 0xf]) *
                   WindClipped * 0.2f;
        float extra = (WindClipped - 0.5f) * 0.4f;
        if (0.0f < extra) {
            uint32_t j10 = (ms >> 8) & 0xf;
            float u = float(ms & 0xff) * 0.00390625f;
            float v = 1.0f - u;
            dx += (v * s_windDirB[j10] + u * s_windDirB[(j10 + 1) & 0xf]) * extra;
            dy += (u * s_windDirB[(j10 + 4) & 0xf] + v * s_windDirB[(j10 + 3) & 0xf]) * extra;
            dz += (u * s_windDirB[(j10 + 7) & 0xf] + v * s_windDirB[(j10 + 6) & 0xf]) * extra;
        }
        uint32_t k = ((ms >> 0xb) & 0xf) + 1;
        float s = 0.5f - std::cos(float(ms & 0x7ff) * 0.00048828125f * 3.1415927f) * 0.5f;
        float blend = s * s_windDirA[(k + 1) & 0xf] + (1.0f - s) * s_windDirA[k & 0xf];
        // NOTE: 1.0 indexes s_windDirA at 0x8D5FF4 with (uVar10 & 0xf) where
        // uVar10 = ((ms>>0xb)&0xf)+1, and s_windDirA+4 at 0x8D5FF8. The two
        // tables above are laid out to match; VERIFY the exact lane mapping.
        WindDir.x = blend * dx;
        WindDir.y = blend * dy;
        WindDir.z = blend * dz;
    }
    Wavyness = std::min(WindClipped + 0.3f, 1.0f);

    if (1.0f - UnderWaterness <= Rain)
        Rain = 1.0f - UnderWaterness;

    // TrafficLightsBrightness: on at night, ramping at 5-7am and 7-8pm.
    {
        uint8_t h = CClockPort::ms_nGameClockHours;
        if (h < 0x15) {
            if (0x13 < h)
                TrafficLightsBrightness = float(CClockPort::ms_nGameClockMinutes) * 0.016666668f;
            else if (6 < h)
                TrafficLightsBrightness = 0.0f;
            else if (5 < h)
                TrafficLightsBrightness = 1.0f - float(CClockPort::ms_nGameClockMinutes) * 0.016666668f;
            else
                TrafficLightsBrightness = 1.0f;
        } else {
            TrafficLightsBrightness = 1.0f;
        }
    }

    // HeadLightsSpectrum = max(Rain, Foggyness) clamped to 1.
    HeadLightsSpectrum = std::min(std::max(Rain, Foggyness), 1.0f);
    TrafficLightsBrightness = std::max({ TrafficLightsBrightness, WetRoads, Foggyness, Rain });

    AddRain();

    // TODO(port): task check for bVar6 (CTaskManager::GetSimplestActiveTask
    // vtable+0x10 == 0xFE) and the sunny-day FindPlayerPed(-1) stats call.
    UpdateInTunnelness();
    // TODO(port): CAEWeatherAudioEntity::Service(&m_WeatherAudioEntity).
}
