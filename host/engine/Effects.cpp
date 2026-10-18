// Gunfire effects, see Effects.h.

#define NOMINMAX
#include "Effects.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
float Rnd() { return (float)std::rand() / (float)RAND_MAX; }
float Rnd(float a, float b) { return a + (b - a) * Rnd(); }
}  // namespace

bool Effects::Init(D3DRenderer& r) {
    // a soft round sprite, white with a smooth alpha falloff
    const int n = 32;
    std::vector<uint32_t> px(n * n);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            const float dx = (x + 0.5f) / n * 2.0f - 1.0f, dy = (y + 0.5f) / n * 2.0f - 1.0f;
            const float d = std::sqrt(dx * dx + dy * dy);
            float a = std::max(0.0f, 1.0f - d);
            a = a * a * (3.0f - 2.0f * a);
            px[y * n + x] = ((uint32_t)(a * 255.0f) << 24) | 0x00FFFFFFu;
        }
    m_tex = r.CreateTexture(n, n, D3DFMT_A8R8G8B8, px.data(), (uint32_t)(px.size() * 4));
    return m_tex != nullptr;
}

void Effects::Shutdown(D3DRenderer& r) {
    if (m_tex) r.DestroyTexture(m_tex);
    m_tex = nullptr;
    m_puffs.clear();
    m_traces.clear();
}

void Effects::Impact(const float p[3], float nz) {
    // dust: a few slow growing grey-brown puffs that drift up
    for (int i = 0; i < 4; i++) {
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k] + Rnd(-0.05f, 0.05f);
        f.v[0] = Rnd(-0.3f, 0.3f); f.v[1] = Rnd(-0.3f, 0.3f); f.v[2] = Rnd(0.2f, 0.7f);
        f.life = Rnd(0.5f, 0.9f);
        f.size0 = 0.08f; f.size1 = Rnd(0.35f, 0.6f);
        f.rgb = 0xB4A58C; f.alpha = 0.55f;
        m_puffs.push_back(f);
    }
    // sparks: tiny fast bright specks thrown out
    for (int i = 0; i < 6; i++) {
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k];
        f.v[0] = Rnd(-3.0f, 3.0f); f.v[1] = Rnd(-3.0f, 3.0f); f.v[2] = Rnd(0.5f, 3.0f) * (nz > 0.5f ? 1.0f : 0.6f);
        f.life = Rnd(0.12f, 0.28f);
        f.size0 = 0.05f; f.size1 = 0.02f;
        f.rgb = 0xFFD070; f.alpha = 1.0f; f.spark = true;
        m_puffs.push_back(f);
    }
}

void Effects::Tracer(const float a[3], const float b[3]) {
    Trace t = {};
    for (int k = 0; k < 3; k++) { t.a[k] = a[k]; t.b[k] = b[k]; }
    t.life = 0.12f;
    m_traces.push_back(t);
}

void Effects::Update(float dt) {
    for (Puff& f : m_puffs) {
        f.age += dt;
        for (int k = 0; k < 3; k++) f.p[k] += f.v[k] * dt;
        if (f.spark) f.v[2] -= 9.0f * dt;          // sparks fall
        else for (int k = 0; k < 3; k++) f.v[k] *= std::max(0.0f, 1.0f - 2.5f * dt);   // dust slows
    }
    m_puffs.erase(std::remove_if(m_puffs.begin(), m_puffs.end(), [](const Puff& f) { return f.age >= f.life; }), m_puffs.end());
    for (Trace& t : m_traces) t.age += dt;
    m_traces.erase(std::remove_if(m_traces.begin(), m_traces.end(), [](const Trace& t) { return t.age >= t.life; }), m_traces.end());
}

void Effects::Render(D3DRenderer& r) {
    if (!m_tex || (m_puffs.empty() && m_traces.empty())) return;
    const D3DMATRIX& v = r.GetViewMatrix();
    const float right[3] = {v._11, v._21, v._31}, up[3] = {v._12, v._22, v._32};
    std::vector<D3DRenderer::ColorTexVertex> vs;
    std::vector<uint16_t> ix;
    auto quad = [&](const float c[3], const float rx[3], const float ux[3], uint32_t color) {
        if (vs.size() + 4 > 65000) return;
        const uint16_t base = (uint16_t)vs.size();
        const float sx[4] = {-1, 1, 1, -1}, sy[4] = {1, 1, -1, -1};
        const float u[4] = {0, 1, 1, 0}, w[4] = {0, 0, 1, 1};
        for (int i = 0; i < 4; i++)
            vs.push_back({c[0] + rx[0] * sx[i] + ux[0] * sy[i], c[1] + rx[1] * sx[i] + ux[1] * sy[i],
                          c[2] + rx[2] * sx[i] + ux[2] * sy[i], color, u[i], w[i]});
        const uint16_t q[6] = {0, 1, 2, 0, 2, 3};
        for (uint16_t k : q) ix.push_back(base + k);
    };
    for (const Puff& f : m_puffs) {
        const float t = f.age / f.life, s = f.size0 + (f.size1 - f.size0) * t;
        const float a = f.alpha * (1.0f - t) * (f.spark ? 1.0f : std::min(1.0f, t * 8.0f));
        const float rx[3] = {right[0] * s, right[1] * s, right[2] * s}, ux[3] = {up[0] * s, up[1] * s, up[2] * s};
        quad(f.p, rx, ux, ((uint32_t)(a * 255.0f) << 24) | f.rgb);
    }
    for (const Trace& tr : m_traces) {
        const float t = tr.age / tr.life;
        float d[3] = {tr.b[0] - tr.a[0], tr.b[1] - tr.a[1], tr.b[2] - tr.a[2]};
        const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (len < 0.1f) continue;
        // the bright head runs from the muzzle to the target in the first part of its life
        const float head = std::min(1.0f, t * 4.0f), tail = std::max(0.0f, t * 4.0f - 0.5f);
        float p0[3], p1[3];
        for (int k = 0; k < 3; k++) { p0[k] = tr.a[k] + d[k] * std::min(1.0f, tail); p1[k] = tr.a[k] + d[k] * head; }
        const float mid[3] = {(p0[0] + p1[0]) * 0.5f, (p0[1] + p1[1]) * 0.5f, (p0[2] + p1[2]) * 0.5f};
        // axis along the streak, width across it facing the camera
        float ax[3] = {(p1[0] - p0[0]) * 0.5f, (p1[1] - p0[1]) * 0.5f, (p1[2] - p0[2]) * 0.5f};
        const float al = std::sqrt(ax[0] * ax[0] + ax[1] * ax[1] + ax[2] * ax[2]);
        if (al < 0.01f) continue;
        // width direction: perpendicular to the axis and the view direction
        const float vz[3] = {v._13, v._23, v._33};
        float cr[3];
        cr[0] = vz[1] * ax[2] - vz[2] * ax[1]; cr[1] = vz[2] * ax[0] - vz[0] * ax[2]; cr[2] = vz[0] * ax[1] - vz[1] * ax[0];
        const float cl = std::sqrt(cr[0] * cr[0] + cr[1] * cr[1] + cr[2] * cr[2]);
        if (cl < 1e-4f) continue;
        const float w = 0.025f / cl;
        const float wx[3] = {cr[0] * w, cr[1] * w, cr[2] * w};
        const float a = (1.0f - t) * 0.9f;
        quad(mid, ax, wx, ((uint32_t)(a * 255.0f) << 24) | 0xFFE8A0);
    }
    if (!ix.empty()) r.DrawBlendedTriangles(vs.data(), (uint32_t)vs.size(), ix.data(), (uint32_t)ix.size(), m_tex);
}
