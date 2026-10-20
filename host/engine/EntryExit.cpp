#include "EntryExit.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <fstream>
#include <sstream>

#include "D3DRenderer.h"

bool EntryExitDoor::IsIn(float x, float y, float z) const {
    // CEntryExit::IsInArea: rotate the point into the rect's frame when the rect is angled.
    float dx = x - cx, dy = y - cy;
    if (angle != 0.0f) {
        const float c = std::cos(angle), s = std::sin(angle);
        const float rx = dx * c + dy * s, ry = -dx * s + dy * c;
        dx = rx; dy = ry;
    }
    return std::fabs(dx) <= halfX && std::fabs(dy) <= halfY && std::fabs(z - cz) < 1.0f;
}

static std::string Trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

int EntryExit::Load(const std::string& gameDir) {
    m_d.clear();
    std::vector<std::string> files;
    {
        std::ifstream f(gameDir + "/data/gta.dat");
        std::string line;
        while (std::getline(f, line)) {
            line = Trim(line);
            if (line.size() < 5 || strncasecmp(line.c_str(), "IPL ", 4) != 0) continue;
            std::string p = Trim(line.substr(4));
            for (char& c : p) if (c == '\\') c = '/';
            files.push_back(gameDir + "/" + p);
        }
    }
    for (const std::string& fn : files) {
        std::ifstream f(fn);
        if (!f) continue;
        std::string line;
        bool in = false;
        while (std::getline(f, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#') continue;
            if (!in) { if (strcasecmp(line.c_str(), "enex") == 0) in = true; continue; }
            if (strcasecmp(line.c_str(), "end") == 0) { in = false; continue; }
            // x, y, z, angle, rx, ry, rz, ex, ey, ez, exitAngle, area, flags, "NAME", sky, peds, timeOn, timeOff
            for (char& c : line) if (c == ',') c = ' ';
            std::string qn;
            size_t q1 = line.find('"');
            if (q1 == std::string::npos) continue;
            size_t q2 = line.find('"', q1 + 1);
            if (q2 == std::string::npos) continue;
            qn = line.substr(q1 + 1, q2 - q1 - 1);
            std::istringstream a(line.substr(0, q1)), b(line.substr(q2 + 1));
            float v[13];
            int ok = 0;
            for (; ok < 13 && (a >> v[ok]); ok++) {}
            if (ok < 13) continue;
            int sky = 0, peds = 0, ton = 0, toff = 24;
            b >> sky >> peds >> ton >> toff;
            EntryExitDoor d;
            std::strncpy(d.name, qn.c_str(), 7);
            d.cx = v[0]; d.cy = v[1]; d.cz = v[2] + 1.0f;
            d.angle = v[3] * 3.14159265f / 180.0f;
            d.halfX = v[4] * 0.5f; d.halfY = v[5] * 0.5f;
            d.ex = v[7]; d.ey = v[8]; d.ez = v[9] + 1.0f;
            d.exitAngle = v[10];
            d.area = (int)v[11];
            d.flags = (int)v[12];
            d.sky = sky; d.peds = peds; d.timeOn = ton; d.timeOff = toff;
            // CEntryExit ctor: burglary-access doors start closed until burglary houses are on
            if (d.flags & 0x1000) { d.timeOn = 0; d.timeOff = 24; d.enabled = false; }
            else if (d.flags & 0x400) { /* random time of day in retail; keep the file's times */ }
            m_d.push_back(d);
        }
    }
    // LinkEntryExit: a "create linked pair" door pairs with the same named door that is not one.
    for (size_t i = 0; i < m_d.size(); i++) {
        if (!(m_d[i].flags & 4)) continue;
        for (size_t j = 0; j < m_d.size(); j++) {
            if (j == i || (m_d[j].flags & 4)) continue;
            if (strncasecmp(m_d[i].name, m_d[j].name, 8) != 0) continue;
            m_d[i].link = (int)j;
            if (m_d[j].link < 0) m_d[j].link = (int)i;
            m_d[j].timeOn = 0; m_d[j].timeOff = 24;
            break;
        }
    }
    int linked = 0;
    for (auto& d : m_d) if (d.link >= 0) linked++;
    std::fprintf(stderr, "[EnEx] %zu doors (%d linked)\n", m_d.size(), linked);
    return (int)m_d.size();
}

bool EntryExit::InTime(const EntryExitDoor& d, int hour) {
    if (d.timeOn == d.timeOff) return true;
    if (d.timeOn < d.timeOff) return hour >= d.timeOn && hour < d.timeOff;
    return hour >= d.timeOn || hour < d.timeOff;
}

int EntryExit::Index(const char* name, int notFlags) const {
    for (size_t i = 0; i < m_d.size(); i++)
        if ((m_d[i].flags & notFlags) == 0 && strncasecmp(m_d[i].name, name, 8) == 0) return (int)i;
    return -1;
}

void EntryExit::SetEnabledByName(const char* name, bool on) {
    for (auto& d : m_d) if (strncasecmp(d.name, name, 8) == 0) d.enabled = on;
}

void EntryExit::SetBurglaryHouses(bool on) {
    for (auto& d : m_d) if (d.flags & 0x1000) d.enabled = on;
}

int EntryExit::Trigger(int idx, bool inCar) {
    if (idx < 0 || (size_t)idx >= m_d.size() || m_state != 0) return -1;
    EntryExitDoor& d = m_d[idx];
    if (inCar) {                               // CEntryExit::TransitionStarted vehicle gate
        if (!(d.flags & 0x20) && !(d.flags & 0x40)) return -1;
    } else if (d.flags & 0x800) return -1;     // disable-exit: no on-foot use
    m_active = idx;
    m_state = 1; m_t = 0;
    if (getenv("GTA_ENEXDBG")) std::fprintf(stderr, "[EnEx] trigger %s (door %d, link %d)\n", d.name, idx, d.link);
    m_last = idx;
    std::memcpy(m_lastName, d.name, 8);
    return idx;
}

bool EntryExit::Update(float dt, float px, float py, float pz, bool inCar, int hour) {
    if (m_state == 1) {
        m_t += dt;
        m_fade = m_t / 0.5f;
        if (m_fade >= 1.0f) {
            m_fade = 1.0f;
            const EntryExitDoor& d = m_d[m_active];
            // the spawn point is the linked door's exit; an unpaired door lands on its own exit
            const EntryExitDoor& s = d.link >= 0 ? m_d[d.link] : d;
            m_curArea = s.area;
            if (setArea) setArea(s.area);
            if (warp) warp(s.ex, s.ey, s.ez, s.exitAngle, s.area);
            m_state = 2; m_t = 0;
        }
        return true;
    }
    if (m_state == 2) {
        m_t += dt;
        m_fade = 1.0f - m_t / 0.5f;
        if (m_fade <= 0.0f) { m_fade = 0.0f; m_state = 0; m_active = -1; }
        return true;
    }
    if (m_disabled) return false;
    static const bool dbg = getenv("GTA_ENEXDBG") != nullptr;
    static int dbgN = 0;
    if (dbg && (dbgN++ % 60) == 0) {
        int best = -1; float bd = 1e30f;
        for (size_t i = 0; i < m_d.size(); i++) {
            const float dx = m_d[i].cx - px, dy = m_d[i].cy - py, dz = m_d[i].cz - pz;
            const float dd = dx * dx + dy * dy + dz * dz;
            if (dd < bd) { bd = dd; best = (int)i; }
        }
        if (best >= 0) {
            const EntryExitDoor& d = m_d[best];
            std::fprintf(stderr, "[EnEx] player %.2f %.2f %.2f area %d armed %d nearest %s (%.2f %.2f %.2f half %.1f %.1f area %d link %d en %d) in=%d\n", px, py, pz, m_curArea, (int)m_armed, d.name, d.cx, d.cy, d.cz, d.halfX, d.halfY, d.area, d.link, (int)d.enabled, (int)d.IsIn(px, py, pz));
        }
    }
    bool any = false;
    for (size_t i = 0; i < m_d.size(); i++) {
        const EntryExitDoor& d = m_d[i];
        if (!d.enabled || !InTime(d, hour)) continue;
        // only doors of the area the player is in (CEntryExitManager::Update marker rule)
        if (d.link >= 0 ? d.area != m_curArea : d.area == m_curArea) continue;
        if (!d.IsIn(px, py, pz)) continue;
        any = true;
        if (m_armed && Trigger((int)i, inCar) >= 0) { m_armed = false; return true; }
    }
    if (!any) m_armed = true;
    return false;
}

void EntryExit::DrawMarkers(D3DRenderer& r, const float cam[3], uint32_t nowMs, int hour) {
    if (m_state != 0 || m_disabled) return;
    std::vector<DebugLineVertex> lines;
    std::vector<D3DRenderer::GlowSprite> glow;
    const float spin = (float)(nowMs & 0x7ff) * 0.0030566407f;
    for (const EntryExitDoor& d : m_d) {
        if (!d.enabled || !InTime(d, hour)) continue;
        if (d.link >= 0 ? d.area != m_curArea : d.area == m_curArea) continue;
        const float dx = d.cx - cam[0], dy = d.cy - cam[1];
        if (dx * dx + dy * dy > 40.0f * 40.0f) continue;
        const float bob = 0.15f * std::sin(spin * 2.0f);
        const float z0 = d.cz + 1.0f + bob;          // cone point, base above (the arrow points down)
        const uint32_t col = 0xFFFFFF00;
        const int N = 8;
        float px[N], py[N];
        for (int k = 0; k < N; k++) {
            const float a = spin + k * 6.2831853f / N;
            px[k] = d.cx + std::cos(a) * 0.5f; py[k] = d.cy + std::sin(a) * 0.5f;
        }
        for (int k = 0; k < N; k++) {
            const int n = (k + 1) % N;
            lines.push_back({px[k], py[k], z0 + 1.4f, col}); lines.push_back({px[n], py[n], z0 + 1.4f, col});
            lines.push_back({px[k], py[k], z0 + 1.4f, col}); lines.push_back({d.cx, d.cy, z0, col});
        }
        glow.push_back({d.cx, d.cy, z0 + 0.7f, 1.2f, 0x60FFFF00});
    }
    if (!lines.empty()) r.DrawDebugLines(lines.data(), (uint32_t)lines.size());
    if (!glow.empty()) r.DrawCoronas(glow.data(), (int)glow.size());
}
