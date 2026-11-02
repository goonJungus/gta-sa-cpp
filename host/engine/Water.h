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
// smallWaves, RenderHighDetailWaterRectangle) lift the surface: within 96 m of
// the camera a rectangle is a 4 m grid displaced by them (fading out at the edge
// of that box), and GetWaterLevel adds the same height for boats and swimmers.

#include "D3DRenderer.h"

#include <string>
#include <vector>

// One boat's wake trail as CBoat::RenderWakePoints lays it out: the points newest first, each with its half
// width and alpha (0..255), the boat's forward direction for the first point, and the surface height.
struct BoatWake {
    int n = 0;
    float x[32] = {}, y[32] = {}, width[32] = {}, alpha[32] = {};
    float fwd[2] = {0.0f, 1.0f};
    float z = 0.0f;
};

class Water {
public:
    bool Load(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);
    bool Ready() const { return !m_polys.empty(); }

    // Surface height at (x,y). False when no water there. Visible and invisible
    // polys both count; outside the map square the open sea is z = 0.
    bool GetWaterLevel(float x, float y, float* z) const;

    // rgba: timecyc Water colour (0..255). range: how far from the camera the
    // open sea outside the map is drawn. wavyness: CWeather::Wavyness (0.3..1).
    void Render(D3DRenderer& r, float camX, float camY, float range, uint32_t timeMs,
                const float rgba[4], float wavyness);
    // CWaterLevel::RenderBoatWakes: each trail as segments of four strips across (alpha 0.4, 1, 0.2, 1, 0.4 from
    // edge to edge) in particle.txd "waterwake", tiled every 4 m. Segments over 13 m long are skipped.
    void RenderBoatWakes(D3DRenderer& r, const std::vector<BoatWake>& wakes);

private:
    struct Poly {
        int n = 0;               // 3 or 4 vertices
        float x[4] = {}, y[4] = {}, z[4] = {};
        bool visible = true;
        float bigW = 0.0f, smallW = 0.0f;   // the vertices' mean bigWaves / smallWaves
    };

    // CWaterLevel::AddWaveToResult: the wave height at (x, y) over the 2 m lattice, from the poly's amplitudes.
    float WaveHeight(float x, float y, float bigW, float smallW) const;
    uint32_t m_timeMs = 0;
    float m_wavyness = 0.5f;   // as of the last Render
    std::vector<Poly> m_polys;
    IDirect3DTexture9* m_tex = nullptr;
    IDirect3DTexture9* m_wakeTex = nullptr;
    std::vector<D3DRenderer::ColorTexVertex> m_verts;
    std::vector<uint16_t> m_indices;
};
