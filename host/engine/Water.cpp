// CWaterLevel water surfaces. See Water.h.

#define NOMINMAX
#include "Water.h"

#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace {

constexpr float kWorldHalf = 3000.0f;   // the map square, -3000..3000

}  // namespace

bool Water::Load(D3DRenderer& r, const std::string& gameDir) {
    std::ifstream f(gameDir + "\\data\\water.dat");
    if (!f) {
        std::fprintf(stderr, "[WATER] data\\water.dat not found\n");
        return false;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == ';' || line[0] == '*' || line[0] == 'p') continue;   // "processed"
        std::istringstream in(line);
        std::vector<float> v;
        float x;
        while (in >> x) v.push_back(x);
        // 4 or 3 vertices of 7 floats, then an optional flags field (default 1).
        Poly p;
        size_t nf = 0;
        if (v.size() >= 28) { p.n = 4; nf = 28; }
        else if (v.size() >= 21) { p.n = 3; nf = 21; }
        else continue;
        for (int i = 0; i < p.n; i++) {
            p.x[i] = v[i * 7 + 0];
            p.y[i] = v[i * 7 + 1];
            p.z[i] = v[i * 7 + 2];
        }
        const int flags = v.size() > nf ? (int)v[nf] : 1;
        p.visible = (flags & 1) != 0;
        m_polys.push_back(p);
    }

    for (const TxdTexture& t : TxdLoader::Load(gameDir + "\\models\\particle.txd")) {
        const std::string key = TxdLoader::KeyOf(t.name);
        IDirect3DTexture9** dst = key == "waterclear256" ? &m_tex : key == "waterwake" ? &m_wakeTex : nullptr;
        if (!dst || *dst) continue;
        *dst = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(), &t.extraMips);
    }
    std::fprintf(stderr, "[WATER] %zu polys, texture %s\n", m_polys.size(), m_tex ? "ok" : "missing");
    return !m_polys.empty();
}

void Water::RenderBoatWakes(D3DRenderer& r, const std::vector<BoatWake>& wakes) {
    if (!m_wakeTex || wakes.empty()) return;
    static const float kAlphaMult[5] = {0.4f, 1.0f, 0.2f, 1.0f, 0.4f};
    std::vector<D3DRenderer::ColorTexVertex> vs;
    std::vector<uint16_t> ix;
    for (const BoatWake& w : wakes) {
        float prevDir[2] = {w.fwd[0], w.fwd[1]};
        for (int i = 1; i < w.n; i++) {
            float dir[2] = {w.x[i - 1] - w.x[i], w.y[i - 1] - w.y[i]};
            const float d2 = dir[0] * dir[0] + dir[1] * dir[1];
            if (d2 >= 9.0f) { const float l = std::sqrt(d2); dir[0] /= l; dir[1] /= l; }   // only normalised past 3 m, as retail
            if (d2 <= 169.0f && vs.size() + 16 < 65000) {
                // GetPerpRight (y, -x): A = point - perp * width, B = point + perp * width
                const float pa[2] = {w.x[i - 1] - prevDir[1] * w.width[i - 1], w.y[i - 1] + prevDir[0] * w.width[i - 1]};
                const float pb[2] = {w.x[i - 1] + prevDir[1] * w.width[i - 1], w.y[i - 1] - prevDir[0] * w.width[i - 1]};
                const float ca[2] = {w.x[i] + dir[1] * w.width[i], w.y[i] - dir[0] * w.width[i]};
                const float cb[2] = {w.x[i] - dir[1] * w.width[i], w.y[i] + dir[0] * w.width[i]};
                for (int part = 0; part < 4; part++) {
                    const float f0 = part / 4.0f, f1 = (part + 1) / 4.0f;
                    const float c[4][2] = {{pb[0] + (pa[0] - pb[0]) * f0, pb[1] + (pa[1] - pb[1]) * f0},
                                           {pb[0] + (pa[0] - pb[0]) * f1, pb[1] + (pa[1] - pb[1]) * f1},
                                           {ca[0] + (cb[0] - ca[0]) * f1, ca[1] + (cb[1] - ca[1]) * f1},
                                           {ca[0] + (cb[0] - ca[0]) * f0, ca[1] + (cb[1] - ca[1]) * f0}};
                    const float al[4] = {w.alpha[i - 1] * kAlphaMult[part], w.alpha[i - 1] * kAlphaMult[part + 1],
                                         w.alpha[i] * kAlphaMult[part + 1], w.alpha[i] * kAlphaMult[part]};
                    float mu = 1e9f, mv = 1e9f;
                    for (const auto& q : c) { mu = std::min(mu, std::floor(q[0] / 4.0f)); mv = std::min(mv, std::floor(q[1] / 4.0f)); }
                    const uint16_t base = (uint16_t)vs.size();
                    for (int k = 0; k < 4; k++) {
                        const uint32_t a = (uint32_t)std::clamp(al[k], 0.0f, 255.0f);
                        // retail sits the strip at -0.03 and sways it with the waves; the water here is flat, so it
                        // rides just above the surface instead
                        vs.push_back({c[k][0], c[k][1], w.z + 0.03f, (a << 24) | 0xFFFFFFu, c[k][0] / 4.0f - mu, c[k][1] / 4.0f - mv});
                    }
                    for (uint16_t k : {0, 2, 1, 0, 3, 2}) ix.push_back((uint16_t)(base + k));
                }
            }
            prevDir[0] = dir[0]; prevDir[1] = dir[1];
        }
    }
    if (!ix.empty()) r.DrawBlendedTriangles(vs.data(), (uint32_t)vs.size(), ix.data(), (uint32_t)ix.size(), m_wakeTex, false);
}

bool Water::GetWaterLevel(float x, float y, float* z) const {
    for (const Poly& p : m_polys) {
        bool in = false;
        if (p.n == 4) {
            const float x0 = std::min({p.x[0], p.x[1], p.x[2], p.x[3]}), x1 = std::max({p.x[0], p.x[1], p.x[2], p.x[3]});
            const float y0 = std::min({p.y[0], p.y[1], p.y[2], p.y[3]}), y1 = std::max({p.y[0], p.y[1], p.y[2], p.y[3]});
            in = x >= x0 && x <= x1 && y >= y0 && y <= y1;
        } else {
            auto side = [&](int a, int b) { return (p.x[b] - p.x[a]) * (y - p.y[a]) - (p.y[b] - p.y[a]) * (x - p.x[a]); };
            const float s0 = side(0, 1), s1 = side(1, 2), s2 = side(2, 0);
            in = (s0 >= 0 && s1 >= 0 && s2 >= 0) || (s0 <= 0 && s1 <= 0 && s2 <= 0);
        }
        if (in) { *z = p.z[0]; return true; }
    }
    if (std::fabs(x) > kWorldHalf || std::fabs(y) > kWorldHalf) { *z = 0.0f; return true; }
    return false;
}

void Water::Shutdown(D3DRenderer& r) {
    r.DestroyTexture(m_tex);
    m_tex = nullptr;
    m_polys.clear();
}

void Water::Render(D3DRenderer& r, float camX, float camY, float range, uint32_t timeMs,
                   const float rgba[4], float wavyness) {
    if (m_polys.empty()) return;

    // Layer alphas (RenderWater): the second layer uses the timecyc alpha a,
    // the first a*256/(256-a), clamped to 255.
    const int a = std::clamp((int)rgba[3], 0, 255);
    const int a0 = std::min(255, (a << 8) / std::max(1, 256 - a));
    const uint32_t rgb = ((uint32_t)std::clamp((int)rgba[0], 0, 255) << 16) |
                         ((uint32_t)std::clamp((int)rgba[1], 0, 255) << 8) |
                         (uint32_t)std::clamp((int)rgba[2], 0, 255);

    // Texture shifts: the 12.5 m layer circles with the wavyness; the 25 m layer bobs in V.
    const float t1 = (float)(timeMs & 0xFFF) * 0.0015339808f;   // 2*pi per 4096 ms
    const float t2 = (float)(timeMs & 0x1FFF) * 0.0007669904f;  // 2*pi per 8192 ms
    const float shift[2][2] = {
        {std::sin(t1) * wavyness * 0.08f, std::cos(t1) * wavyness * 0.08f},
        {0.0f, std::cos(t2) * 0.024f},
    };
    const float scale[2] = {0.08f, 0.04f};
    const uint32_t color[2] = {((uint32_t)a0 << 24) | rgb, ((uint32_t)a << 24) | rgb};

    m_verts.clear();
    m_indices.clear();
    auto emit = [&](int layer, int n, const float* xs, const float* ys, const float* zs) {
        const uint16_t base = (uint16_t)m_verts.size();
        for (int i = 0; i < n; i++)
            m_verts.push_back({xs[i], ys[i], zs[i], color[layer],
                               xs[i] * scale[layer] + shift[layer][0], ys[i] * scale[layer] + shift[layer][1]});
        const uint16_t tri[6] = {0, 1, 2, 3, 1, 2};   // RenderFlatWaterRectangle_OneLayer order
        for (int i = 0; i < (n == 4 ? 6 : 3); i++) m_indices.push_back((uint16_t)(base + tri[i]));
    };
    auto flush = [&]() {
        r.DrawBlendedTriangles(m_verts.data(), (uint32_t)m_verts.size(), m_indices.data(),
                               (uint32_t)m_indices.size(), m_tex);
        m_verts.clear();
        m_indices.clear();
    };

    // The open sea around the map: up to four flat rectangles that cover
    // camera +-range outside the map square.
    struct Rect { float x0, y0, x1, y1; };
    std::vector<Rect> sea;
    const float cx0 = camX - range, cx1 = camX + range, cy0 = camY - range, cy1 = camY + range;
    if (cy0 < -kWorldHalf) sea.push_back({cx0, cy0, cx1, std::min(cy1, -kWorldHalf)});
    if (cy1 > kWorldHalf) sea.push_back({cx0, std::max(cy0, kWorldHalf), cx1, cy1});
    const float my0 = std::max(cy0, -kWorldHalf), my1 = std::min(cy1, kWorldHalf);
    if (my0 < my1) {
        if (cx0 < -kWorldHalf) sea.push_back({cx0, my0, std::min(cx1, -kWorldHalf), my1});
        if (cx1 > kWorldHalf) sea.push_back({std::max(cx0, kWorldHalf), my0, cx1, my1});
    }

    const float rangeSq = range * range;
    for (int layer = 0; layer < 2; layer++) {
        for (const Poly& p : m_polys) {
            if (!p.visible) continue;
            float nx = camX, ny = camY;   // nearest point of the poly's bounds
            const float minX = *std::min_element(p.x, p.x + p.n), maxX = *std::max_element(p.x, p.x + p.n);
            const float minY = *std::min_element(p.y, p.y + p.n), maxY = *std::max_element(p.y, p.y + p.n);
            nx = std::clamp(nx, minX, maxX);
            ny = std::clamp(ny, minY, maxY);
            if ((nx - camX) * (nx - camX) + (ny - camY) * (ny - camY) > rangeSq) continue;
            emit(layer, p.n, p.x, p.y, p.z);
            if (m_verts.size() > 60000) flush();
        }
        for (const Rect& s : sea) {
            const float xs[4] = {s.x0, s.x1, s.x0, s.x1}, ys[4] = {s.y0, s.y0, s.y1, s.y1}, zs[4] = {};
            emit(layer, 4, xs, ys, zs);
        }
    }
    if (!m_verts.empty()) flush();
}
