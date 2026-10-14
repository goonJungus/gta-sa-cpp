// CClouds moon, stars and low clouds. See Sky.h.

#define NOMINMAX
#include "Sky.h"

#include "TimeCycle.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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

// CSprite::CalcScreenCoors sizes sprites by 1/depth at the 70 degree FOV:
// a sprite of size s spans about 1.4 * s world units at its depth.
constexpr float kSpriteScale = 1.4f;

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
    return m_cloud && m_star && m_moon;
}

void Sky::Shutdown(D3DRenderer& r) {
    r.DestroyTexture(m_cloud);
    r.DestroyTexture(m_star);
    r.DestroyTexture(m_moon);
    m_cloud = m_star = m_moon = nullptr;
}

void Sky::Render(D3DRenderer& r, float camX, float camY, float camZ, float hour,
                 const std::string& weather, const TimeCycleHour& tc) {
    auto rnd = [this]() { m_rand = m_rand * 1103515245u + 12345u; return (m_rand >> 16) & 0x7FFF; };
    const int h = (int)hour, minutes = (int)((hour - h) * 60.0f);
    D3DRenderer::SkySprite s[16];

    // Moon: 100 m south, 15 m up, brightening as the sun sets (the sun's
    // height is CTimeCycle's m_VectorToSun z).
    const float a = hour * 60.0f * 0.004363323f;
    const float vx = std::sin(a) + 0.7f, vz = 0.2f - std::cos(a);
    const float sunZ = vz / std::sqrt(vx * vx + 0.49f + vz * vz);
    if (m_moon) {
        const int b = std::clamp((int)((0.05f - sunZ) * 600.0f), 0, 255);
        if (b > 0) {
            const float half = (2.0f * kMoonSize + 4.0f) * kSpriteScale;
            s[0] = {camX, camY - 100.0f, camZ + 15.0f, half, half, Rgb(b, b, b * 85 / 100)};
            r.DrawSkySprites(s, 1, m_moon);
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
    const bool cloudy = weather.find("CLOUDY") != std::string::npos ||
                        weather.find("RAIN") != std::string::npos ||
                        weather.find("STORM") != std::string::npos;
    if (cloudy && m_cloud) {
        for (int i = 0; i < 12; i++)
            s[i] = {camX + kLowCloudX[i] * 800.0f, camY + kLowCloudY[i] * 800.0f, kLowCloudZ[i] * 60.0f + 40.0f,
                    320.0f * kSpriteScale, 40.0f * kSpriteScale,
                    Rgb((int)tc.lowClouds[0], (int)tc.lowClouds[1], (int)tc.lowClouds[2])};
        r.DrawSkySprites(s, 12, m_cloud);
    }
}
