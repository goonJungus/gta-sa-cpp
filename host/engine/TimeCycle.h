#pragma once
// CTimeCycle data: data/timecyc.dat (23 weathers x 8 hour rows, 51 floats/row).
//
// Only the fields this renderer actually consumes are exposed by name:
// ambient/directional tints, sky gradient colours, sun core/corona + size,
// FarClp (draw distance / fog far), FogSt (fog start offset) and the cloud /
// water colour blocks.
//
// Verified column layout of a row (index -> meaning), see BUILD_NOTES:
//   [0..2]   Ambient RGB
//   [3..5]   Amb_Obj RGB
//   [6..8]   Directional RGB
//   [9..11]  Sky top RGB
//   [12..14] Sky bot RGB
//   [15..17] SunCore RGB
//   [18..20] SunCorona RGB
//   [21]     SunSize      [22] SprSize      [23] SprBright
//   [24..26] Shadow RGB
//   [27]     FarClp       [28] FogSt        [29] LightOnGround
//   [30..32] LowClouds RGB
//   [33..35] BottomClouds RGB
//   [36..39] Water RGBA
//   [40]     Alpha1       [41..43] RGB1
//   [44]     Alpha2       [45..47] RGB2
//   [48..50] CloudAlpha / trailing triple

#include <cstdint>
#include <string>
#include <vector>

// One hour row of one weather block. 0..255 colour components unless noted.
struct TimeCycleHour {
    float ambient[3] = {0, 0, 0};
    float ambientObj[3] = {0, 0, 0};
    float directional[3] = {0, 0, 0};
    float skyTop[3] = {0, 0, 0};
    float skyBot[3] = {0, 0, 0};
    float sunCore[3] = {0, 0, 0};
    float sunCorona[3] = {0, 0, 0};
    float sunSize = 0.0f;
    float sprSize = 0.0f;
    float sprBright = 0.0f;
    float shadow[3] = {0, 0, 0};
    float farClip = 300.0f;   // [27]
    float fogSt = 10.0f;      // [28]
    float lightOnGround = 0.0f;
    float lowClouds[3] = {0, 0, 0};
    float bottomClouds[3] = {0, 0, 0};
    float water[4] = {0, 0, 0, 0};
    float raw[51] = {};
};

struct TimeCycleWeather {
    std::string name;
    TimeCycleHour hours[8];
};

class TimeCycle {
public:
    // Parses data/timecyc.dat. Returns false when the file is missing/short.
    bool Load(const std::string& path);

    bool Loaded() const { return !m_weathers.empty(); }
    size_t WeatherCount() const { return m_weathers.size(); }
    const std::string& WeatherName(size_t w) const;
    // Resolve "SUNNY_LA" -> index, or -1 when unknown.
    int FindWeather(const std::string& name) const;

    // Raw hour slot 0..7 (Midnight, 5AM, 6AM, 7AM, Midday, 7PM, 8PM, 10PM).
    const TimeCycleHour& Hour(size_t weather, int slot) const;

    // Linearly interpolated sample at hourOfDay in [0,24).
    void Sample(size_t weather, float hourOfDay, TimeCycleHour& out) const;

    // The same sample for the first weather (safe fallback before Load).
    void Sample(float hourOfDay, TimeCycleHour& out) const;

    // Clock hour of slot 0..7 in retail order.
    static float SlotHour(int slot);

private:
    std::vector<TimeCycleWeather> m_weathers;
};
