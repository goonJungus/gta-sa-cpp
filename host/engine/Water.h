#pragma once
// CWaterLevel: the water surfaces of San Andreas.
//
// data/water.dat lists axis-aligned quads and triangles (7 floats per vertex:
// x y z flowX flowY bigWaves smallWaves, then a flags int, bit 0 = visible).
// Beyond the 6000x6000 map square the sea continues as flat water at z = 0
// (m_BlocksToBeRenderedOutsideWorld).
//
// Rendering follows CWaterLevel::RenderWater: two layers of particle.txd
// "waterclear256", one tiled every 12.5 m and one every 25 m, coloured by the
// timecyc Water RGB. The timecyc Water alpha is the second layer's alpha and
// the first layer's is a*256/(256-a), so the pair composites to roughly `a`.
// The texture shift wobbles with CWeather::Wavyness. Waves (bigWaves /
// smallWaves, RenderHighDetailWaterRectangle) are not displaced yet: the
// surface is flat.

#include "D3DRenderer.h"

#include <string>
#include <vector>

class Water {
public:
    bool Load(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);
    bool Ready() const { return !m_polys.empty(); }

    // rgba: timecyc Water colour (0..255). range: how far from the camera the
    // open sea outside the map is drawn. wavyness: CWeather::Wavyness (0.3..1).
    void Render(D3DRenderer& r, float camX, float camY, float range, uint32_t timeMs,
                const float rgba[4], float wavyness);

private:
    struct Poly {
        int n = 0;               // 3 or 4 vertices
        float x[4] = {}, y[4] = {}, z[4] = {};
        bool visible = true;
    };

    std::vector<Poly> m_polys;
    IDirect3DTexture9* m_tex = nullptr;
    std::vector<D3DRenderer::ColorTexVertex> m_verts;
    std::vector<uint16_t> m_indices;
};
