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
        if (TxdLoader::KeyOf(t.name) != "waterclear256") continue;
        m_tex = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(),
                                &t.extraMips);
        break;
    }
    std::fprintf(stderr, "[WATER] %zu polys, texture %s\n", m_polys.size(), m_tex ? "ok" : "missing");
    return !m_polys.empty();
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
