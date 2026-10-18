// CClouds moon, stars and low clouds. See Sky.h.

#define NOMINMAX
#include "Sky.h"

#include "TimeCycle.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

// gta_sa.exe 1.0 data: LOW_CLOUDS_{X,Y,Z}_COORDS @ 0x8D5394/0x8D53C4/0x8D53F4,
// STARS_{Y_POSITIONS,Z_POSITIONS,SIZES} @ 0x8D55EC/0x8D5610/0x8D5634.
constexpr float kLowCloudX[12] = {1.0f, 0.7f, 0.0f, -0.7f, -1.0f, -0.7f, 0.0f, 0.7f, 0.8f, -0.8f, 0.4f, -0.4f};
constexpr float kLowCloudY[12] = {0.0f, -0.7f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.4f, 0.4f, -0.8f, -0.8f};
constexpr float kLowCloudZ[12] = {0.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.3f, 0.9f, 0.4f, 1.3f, 1.4f, 1.2f, 1.7f};
constexpr float kStarY[9] = {0.0f, 0.05f, 0.13f, 0.4f, 0.7f, 0.6f, 0.27f, 0.55f, 0.75f};
constexpr float kStarZ[9] = {0.0f, 0.45f, 0.9f, 1.0f, 0.85f, 0.52f, 0.48f, 0.35f, 0.2f};
constexpr float kStarSize[9] = {1.0f, 1.4f, 0.9f, 1.0f, 0.6f, 1.5f, 1.3f, 1.0f, 0.8f};
constexpr int kMoonSize = 3;   // CCoronas::MoonSize

// CSprite::CalcScreenCoors makes a sprite of size s span s * screen height /
// depth pixels vertically; with SA's 70 degree (horizontal, 4:3) FOV that is
// s * 2 * tan(35) * 3/4 = 1.05 * s world units at the sprite's depth.
constexpr float kSpriteScale = 1.05f;

uint32_t Rgb(int r, int g, int b) {
    return 0xFF000000u | ((uint32_t)std::clamp(r, 0, 255) << 16) |
           ((uint32_t)std::clamp(g, 0, 255) << 8) | (uint32_t)std::clamp(b, 0, 255);
}

}  // namespace

bool Sky::Load(D3DRenderer& r, const std::string& gameDir) {
    for (const TxdTexture& t : TxdLoader::Load(gameDir + "\\models\\particle.txd")) {
        const std::string key = TxdLoader::KeyOf(t.name);
        IDirect3DTexture9** slot = key == "cloud1" ? &m_cloud : key == "coronastar" ? &m_star
                                 : key == "coronamoon" ? &m_moon : nullptr;
        if (slot && !*slot)
            *slot = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(),
                                    (uint32_t)t.mip0.size(), &t.extraMips);
    }
    if (!m_cloud || !m_star || !m_moon) std::fprintf(stderr, "[SKY] particle.txd sky textures missing\n");
    // Moon discs: a lit sphere per phase (0 new .. 8 full .. 15), soft rim and faint maria.
    const int N = 64;
    for (int ph = 0; ph < 16; ph++) {
        std::vector<uint32_t> px((size_t)N * N);
        const float th = 2.0f * 3.14159265f * (float)ph / 16.0f;
        const float lx = std::sin(th), lz = -std::cos(th);
        for (int y = 0; y < N; y++)
            for (int x = 0; x < N; x++) {
                const float u = ((float)x + 0.5f) / N * 2.0f - 1.0f, v = 1.0f - ((float)y + 0.5f) / N * 2.0f;
                const float rr = u * u + v * v;
                float a = 0.0f;
                if (rr < 1.0f) {
                    const float z = std::sqrt(1.0f - rr);
                    float lit = u * lx + z * lz;
                    lit = std::min(1.0f, std::max(0.0f, lit * 2.2f));   // sharp-ish terminator
                    const float maria = 0.82f + 0.18f * std::sin(u * 7.0f + 1.3f) * std::sin(v * 5.0f + 0.4f);
                    const float rim = std::min(1.0f, (1.0f - std::sqrt(rr)) * 14.0f);
                    a = lit * maria * rim;
                }
                const int c = (int)(a * 255.0f);
                px[(size_t)y * N + x] = 0xFF000000u | (c << 16) | (c << 8) | c;
            }
        m_phase[ph] = r.CreateTexture(N, N, D3DFMT_A8R8G8B8, px.data(), (uint32_t)(px.size() * 4));
    }
    return m_cloud && m_star && m_moon;
}

void Sky::Shutdown(D3DRenderer& r) {
    r.DestroyTexture(m_cloud);
    r.DestroyTexture(m_star);
    r.DestroyTexture(m_moon);
    for (auto*& t : m_phase) { if (t) r.DestroyTexture(t); t = nullptr; }
    m_cloud = m_star = m_moon = nullptr;
}

void Sky::Render(D3DRenderer& r, float camX, float camY, float camZ, float hour,
                 const std::string& weather, const TimeCycleHour& tc, int day) {
    auto rnd = [this]() { m_rand = m_rand * 1103515245u + 12345u; return (m_rand >> 16) & 0x7FFF; };
    const int h = (int)hour, minutes = (int)((hour - h) * 60.0f);
    D3DRenderer::SkySprite s[16];

    // CloudCoverage / Foggyness stand-in (CWeather): 1 in overcast or foggy weathers.
    const bool cloudy = weather.find("CLOUDY") != std::string::npos ||
                        weather.find("RAIN") != std::string::npos ||
                        weather.find("STORM") != std::string::npos;
    const float cover = cloudy || weather.find("FOG") != std::string::npos ? 1.0f : 0.0f;

    // Moon: rises in the east, crosses the sky and sets in the west, about 12 hours up. The rise
    // time follows the phase (new moon with the sun, full moon opposite it). Phases cycle every 29.5 days.
    {
        const float p01 = std::fmod((float)day, 29.5f) / 29.5f;   // 0 new, 0.5 full
        const float riseHour = std::fmod(6.0f + 24.0f * p01, 24.0f);
        float t = hour - riseHour;
        while (t < 0.0f) t += 24.0f;
        if (m_moon && t < 12.0f) {
            const float a = 3.14159265f * t / 12.0f;                       // 0 rising .. pi setting
            const float edge = std::min(std::min(a, 3.14159265f - a) / 0.12f, 1.0f);   // fade at the horizon
            const bool sunUp = hour > 6.5f && hour < 17.5f;
            float b = 235.0f * edge * (1.0f - cover) * (sunUp ? 0.22f : 1.0f);
            if (b > 1.0f) {
                const float px = camX + std::cos(a) * 160.0f, pz = camZ + std::sin(a) * 120.0f + 4.0f;
                const float halo = 17.0f * kSpriteScale, disc = 8.0f * kSpriteScale;
                const int bi = (int)b;
                s[0] = {px, camY - 40.0f, pz, halo, halo, Rgb(bi / 5, bi / 5, bi / 6)};
                r.DrawSkySprites(s, 1, m_moon);
                const int ph = ((int)(p01 * 16.0f + 0.5f)) & 15;
                IDirect3DTexture9* tex = m_phase[ph] ? m_phase[ph] : m_moon;
                s[0] = {px, camY - 40.0f, pz, disc, disc, Rgb(bi, bi, bi * 92 / 100)};
                r.DrawSkySprites(s, 1, tex);
            }
        }
    }

    // Stars, 22:00 to 06:00: fade in over the 22nd hour and out over the 5th.
    if ((h > 21 || h < 6) && m_star) {
        int bright = 255;
        if (h == 22) bright = minutes * 255 / 60;
        else if (h == 5) bright = (60 - minutes) * 255 / 60;
        if (bright > 0) {
            int n = 0;
            for (int i = 0; i < 11; i++) {
                const int k = i % 9;
                const float sz = kStarSize[k] * 0.8f * kSpriteScale;
                const int c = bright * (255 - (int)(rnd() & 31)) / 255;   // twinkle
                s[n++] = {camX + (i > 8 ? -100.0f : 100.0f), camY - kStarY[k] * 90.0f,
                          camZ + kStarZ[k] * 80.0f + 10.0f, sz, sz, Rgb(c, c, c)};
            }
            const int c = bright * (255 - (int)(rnd() & 127)) / 255;      // the big one
            s[n++] = {camX + 100.0f, camY - 90.0f, camZ + 10.0f, 5.0f * kSpriteScale, 5.0f * kSpriteScale,
                      Rgb(c, c, c)};
            r.DrawSkySprites(s, n, m_star);
        }
    }

    // Low clouds: a ring 800 m out, 40..142 m above sea level, LowClouds RGB
    // scaled by the cloud coverage (additive, so clear weather shows nothing).
    if (cloudy && m_cloud) {
        for (int i = 0; i < 12; i++)
            s[i] = {camX + kLowCloudX[i] * 800.0f, camY + kLowCloudY[i] * 800.0f, kLowCloudZ[i] * 60.0f + 40.0f,
                    320.0f * kSpriteScale, 40.0f * kSpriteScale,
                    Rgb((int)tc.lowClouds[0], (int)tc.lowClouds[1], (int)tc.lowClouds[2])};
        r.DrawSkySprites(s, 12, m_cloud);
    }
}
