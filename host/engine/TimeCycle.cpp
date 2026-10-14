#include "TimeCycle.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

// Slot clock hours, in timecyc row order.
const float kSlotHours[8] = {0.0f, 5.0f, 6.0f, 7.0f, 12.0f, 19.0f, 20.0f, 22.0f};

std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
    return s.substr(a, b - a);
}

void FillFromRaw(const float* v, TimeCycleHour& h) {
    for (int i = 0; i < 51; i++) h.raw[i] = v[i];
    for (int i = 0; i < 3; i++) {
        h.ambient[i] = v[0 + i];
        h.ambientObj[i] = v[3 + i];
        h.directional[i] = v[6 + i];
        h.skyTop[i] = v[9 + i];
        h.skyBot[i] = v[12 + i];
        h.sunCore[i] = v[15 + i];
        h.sunCorona[i] = v[18 + i];
        h.shadow[i] = v[24 + i];
        h.lowClouds[i] = v[30 + i];
        h.bottomClouds[i] = v[33 + i];
    }
    h.sunSize = v[21];
    h.sprSize = v[22];
    h.sprBright = v[23];
    h.farClip = v[27];
    h.fogSt = v[28];
    h.lightOnGround = v[29];
    for (int i = 0; i < 4; i++) h.water[i] = v[36 + i];
}

} // namespace

bool TimeCycle::Load(const std::string& path) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) return false;

    m_weathers.clear();
    TimeCycleWeather cur;
    int rows = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);

        // Weather header: "//////////// SUNNY_LA"
        if (line.compare(0, 4, "////") == 0) {
            size_t s = line.find_first_not_of('/');
            if (s == std::string::npos) continue;   // bare separator row
            std::string name = Trim(line.substr(s));
            if (name.empty()) continue;
            if (rows > 0 || !cur.name.empty()) {
                if (rows > 0) m_weathers.push_back(cur);
                cur = TimeCycleWeather();
            }
            cur.name = name;
            rows = 0;
            continue;
        }
        if (line.compare(0, 2, "//") == 0) continue;  // column / hour comment
        if (Trim(line).empty()) continue;
        if (rows >= 8) continue;

        // A data row: whitespace-separated numbers.
        std::istringstream ss(line);
        float v[51];
        int n = 0;
        float x;
        while (n < 51 && (ss >> x)) v[n++] = x;
        if (n < 29) continue;                    // not a data row
        for (int i = n; i < 51; i++) v[i] = (n > 0) ? v[n - 1] : 0.0f;
        FillFromRaw(v, cur.hours[rows]);
        rows++;
    }
    if (rows > 0 && !cur.name.empty()) m_weathers.push_back(cur);
    if (!f.bad() && m_weathers.empty()) return false;

    // Log the inventory the way the other data loaders do.
    fprintf(stderr, "[TimeCycle] %s: %zu weathers\n", path.c_str(), m_weathers.size());
    return !m_weathers.empty();
}

const std::string& TimeCycle::WeatherName(size_t w) const {
    static const std::string kNone;
    if (w >= m_weathers.size()) return kNone;
    return m_weathers[w].name;
}

int TimeCycle::FindWeather(const std::string& name) const {
    for (size_t i = 0; i < m_weathers.size(); i++) {
        if (m_weathers[i].name == name) return (int)i;
    }
    return -1;
}

float TimeCycle::SlotHour(int slot) {
    if (slot < 0) slot = 0;
    if (slot > 7) slot = 7;
    return kSlotHours[slot];
}

const TimeCycleHour& TimeCycle::Hour(size_t weather, int slot) const {
    static const TimeCycleHour kDefault;
    if (weather >= m_weathers.size()) return kDefault;
    if (slot < 0) slot = 0;
    if (slot > 7) slot = 7;
    return m_weathers[weather].hours[slot];
}

void TimeCycle::Sample(size_t weather, float hourOfDay, TimeCycleHour& out) const {
    if (weather >= m_weathers.size()) return;
    if (!(hourOfDay >= 0.0f)) hourOfDay = 0.0f;
    while (hourOfDay >= 24.0f) hourOfDay -= 24.0f;

    // Bracket the clock hour between two slots (22h -> next day's Midnight).
    int a = 7, b = 0;
    float ha = kSlotHours[7], hb = 24.0f + kSlotHours[0];
    for (int i = 0; i < 8; i++) {
        float h = kSlotHours[i];
        if (hourOfDay >= h) {
            a = i;
            ha = h;
            b = (i + 1) & 7;
            hb = (i + 1 < 8) ? kSlotHours[i + 1] : 24.0f + kSlotHours[0];
        }
    }
    float span = hb - ha;
    float t = (span > 0.0001f) ? (hourOfDay - ha) / span : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    const TimeCycleHour& h0 = m_weathers[weather].hours[a];
    const TimeCycleHour& h1 = m_weathers[weather].hours[b];
    float v[51];
    for (int i = 0; i < 51; i++) v[i] = h0.raw[i] + (h1.raw[i] - h0.raw[i]) * t;
    FillFromRaw(v, out);
}

void TimeCycle::Sample(float hourOfDay, TimeCycleHour& out) const {
    if (m_weathers.empty()) return;
    Sample(0, hourOfDay, out);
}
