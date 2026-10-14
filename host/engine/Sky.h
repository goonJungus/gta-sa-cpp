#pragma once
// CClouds::Render: the moon, the stars and the low cloud ring, drawn as
// additive sprites right after the sky polys (D3DRenderer::RenderSky) and
// before the world. Positions, sizes and the cloud/star tables are retail.
//
// Approximations, where the decomp lost float arithmetic: the star flicker
// scale, and CWeather::CloudCoverage / Foggyness
// (1 in the CLOUDY / RAINY / STORM / FOGGY weathers, 0 otherwise). Moon phases (the
// "lunar" mask through destination alpha), the rainbow and the shooting star
// are not drawn.

#include "D3DRenderer.h"

#include <cstdint>
#include <string>

struct TimeCycleHour;

class Sky {
public:
    bool Load(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);

    // hour: game clock in [0,24). weather: timecyc weather name (e.g. "CLOUDY_LA").
    void Render(D3DRenderer& r, float camX, float camY, float camZ, float hour,
                const std::string& weather, const TimeCycleHour& tc);

private:
    IDirect3DTexture9* m_cloud = nullptr;   // cloud1
    IDirect3DTexture9* m_star = nullptr;    // coronastar
    IDirect3DTexture9* m_moon = nullptr;    // coronamoon
    uint32_t m_rand = 0x1234567u;
};
