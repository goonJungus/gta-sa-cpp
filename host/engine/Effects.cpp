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
    for (Trace& t : m_traces) t.exists = false;
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

void Effects::Splash(const float p[3], float strength) {
    const int foam = 3 + (int)(6 * strength), drops = 4 + (int)(14 * strength);
    for (int i = 0; i < foam; i++) {
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k] + Rnd(-0.12f, 0.12f) * (0.5f + strength);
        f.v[0] = Rnd(-0.5f, 0.5f) * (0.5f + strength); f.v[1] = Rnd(-0.5f, 0.5f) * (0.5f + strength); f.v[2] = Rnd(0.4f, 1.2f) * (0.5f + strength);
        f.life = Rnd(0.5f, 0.9f);
        f.size0 = 0.10f; f.size1 = Rnd(0.35f, 0.6f) * (0.6f + strength);
        f.rgb = 0xE6F2FA; f.alpha = 0.5f;
        m_puffs.push_back(f);
    }
    for (int i = 0; i < drops; i++) {
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k];
        f.v[0] = Rnd(-1.6f, 1.6f) * (0.4f + strength); f.v[1] = Rnd(-1.6f, 1.6f) * (0.4f + strength); f.v[2] = Rnd(1.0f, 3.2f) * (0.4f + strength);
        f.life = Rnd(0.3f, 0.6f);
        f.size0 = 0.05f; f.size1 = 0.03f;
        f.rgb = 0xCFE6FF; f.alpha = 0.9f; f.spark = true;
        m_puffs.push_back(f);
    }
}

void Effects::HeliDust(const float p[3], float t, int prim, float dt, float& acc) {
    // heli_dust.fxs: emitter keys at the system's const time; particles fly out flat (EMANGLE 90, any heading)
    auto lerp = [&](float a, float b) { return a + (b - a) * t; };
    auto lerp3 = [&](float a, float m, float b) { return t < 0.5f ? a + (m - a) * t * 2.0f : m + (b - m) * (t - 0.5f) * 2.0f; };
    const float life = prim == 2 ? lerp(1.0f, 2.0f) : lerp(0.5f, 2.0f);
    const float speed = prim == 2 ? lerp(15.0f, 25.0f) : lerp(1.0f, 25.0f);
    const float rate = prim == 0 ? lerp(5.0f, 50.0f) : prim == 1 ? lerp3(5.0f, 25.0f, 50.0f) : lerp3(2.0f, 25.0f, 50.0f);
    static const uint32_t kRgb[3] = {0xFFFFFF, 0xCEAB91, 0xE6E6FF};
    static const float kAlpha[3] = {64.0f / 255.0f, 48.0f / 255.0f, 32.0f / 255.0f};
    for (acc += rate * dt; acc >= 1.0f; acc -= 1.0f) {
        const float h = Rnd(0.0f, 6.2831853f);
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k];
        f.v[0] = std::cos(h) * speed; f.v[1] = std::sin(h) * speed;
        f.life = life;
        f.size0 = 0.0f; f.size1 = 32.0f;
        f.rgb = kRgb[prim]; f.alpha = kAlpha[prim]; f.ring = true;
        m_puffs.push_back(f);
    }
}

void Effects::Dust(const float p[3], float strength) {
    const int n = 3 + (int)(5 * strength);
    for (int i = 0; i < n; i++) {
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k] + Rnd(-0.1f, 0.1f);
        f.v[0] = Rnd(-1.0f, 1.0f) * (0.5f + strength); f.v[1] = Rnd(-1.0f, 1.0f) * (0.5f + strength); f.v[2] = Rnd(0.1f, 0.4f);
        f.life = Rnd(0.5f, 0.9f);
        f.size0 = 0.12f; f.size1 = Rnd(0.4f, 0.7f) * (0.6f + strength);
        f.rgb = 0xB4A58C; f.alpha = 0.4f;
        m_puffs.push_back(f);
    }
}

void Effects::Blood(const float p[3]) {
    for (int i = 0; i < 7; i++) {
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k];
        f.v[0] = Rnd(-1.2f, 1.2f); f.v[1] = Rnd(-1.2f, 1.2f); f.v[2] = Rnd(-0.2f, 1.2f);
        f.life = Rnd(0.25f, 0.5f);
        f.size0 = 0.05f; f.size1 = 0.12f;
        f.rgb = 0xA01418; f.alpha = 0.9f; f.spark = true;   // falls like a spark, fades
        m_puffs.push_back(f);
    }
}

void Effects::Smoke(const float p[3], float dark) {
    Puff f = {};
    for (int k = 0; k < 3; k++) f.p[k] = p[k] + Rnd(-0.15f, 0.15f);
    f.v[0] = Rnd(-0.3f, 0.3f); f.v[1] = Rnd(-0.3f, 0.3f); f.v[2] = Rnd(1.0f, 1.8f);
    f.life = Rnd(1.2f, 2.0f);
    f.size0 = 0.2f; f.size1 = Rnd(0.8f, 1.4f);
    const uint32_t g = (uint32_t)(150.0f - 120.0f * dark);
    f.rgb = (g << 16) | (g << 8) | g; f.alpha = 0.45f;
    m_puffs.push_back(f);
}

void Effects::Gas(const float p[3]) {
    Puff f = {};
    for (int k = 0; k < 3; k++) f.p[k] = p[k] + Rnd(-1.5f, 1.5f);
    f.p[2] = p[2] + Rnd(0.2f, 1.5f);
    f.v[0] = Rnd(-0.4f, 0.4f); f.v[1] = Rnd(-0.4f, 0.4f); f.v[2] = Rnd(0.1f, 0.4f);
    f.life = Rnd(2.5f, 4.0f);
    f.size0 = 1.0f; f.size1 = Rnd(3.5f, 5.0f);
    f.rgb = 0xD8E0C0; f.alpha = 0.3f;
    m_puffs.push_back(f);
}

void Effects::Spray(const float p[3], uint32_t rgb) {
    Puff f = {};
    for (int k = 0; k < 3; k++) f.p[k] = p[k] + Rnd(-0.2f, 0.2f);
    f.v[0] = Rnd(-0.2f, 0.2f); f.v[1] = Rnd(-0.2f, 0.2f); f.v[2] = Rnd(0.0f, 0.3f);
    f.life = Rnd(0.8f, 1.3f);
    f.size0 = 0.7f; f.size1 = Rnd(2.0f, 2.8f);
    f.rgb = rgb; f.alpha = 0.9f;
    m_puffs.push_back(f);
}

void Effects::Fire(const float p[3]) {
    Puff f = {};
    for (int k = 0; k < 3; k++) f.p[k] = p[k] + Rnd(-0.3f, 0.3f);
    f.v[0] = Rnd(-0.3f, 0.3f); f.v[1] = Rnd(-0.3f, 0.3f); f.v[2] = Rnd(1.5f, 2.5f);
    f.life = Rnd(0.4f, 0.8f);
    f.size0 = 0.4f; f.size1 = 0.1f;
    f.rgb = 0xFF8C1E; f.alpha = 0.8f;
    m_puffs.push_back(f);
}

void Effects::Explosion(const float p[3]) {
    for (int i = 0; i < 26; i++) {
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k] + Rnd(-0.4f, 0.4f);
        f.v[0] = Rnd(-4.0f, 4.0f); f.v[1] = Rnd(-4.0f, 4.0f); f.v[2] = Rnd(0.5f, 5.0f);
        f.life = Rnd(0.5f, 1.2f);
        f.size0 = 0.6f; f.size1 = Rnd(2.0f, 3.5f);
        f.rgb = i % 3 == 0 ? 0xFFD23C : 0xFF6414; f.alpha = 0.85f;
        m_puffs.push_back(f);
    }
    for (int i = 0; i < 12; i++) {
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k];
        f.v[0] = Rnd(-6.0f, 6.0f); f.v[1] = Rnd(-6.0f, 6.0f); f.v[2] = Rnd(3.0f, 9.0f);
        f.life = Rnd(0.6f, 1.3f); f.size0 = 0.08f; f.size1 = 0.05f;
        f.rgb = 0xFFC850; f.alpha = 1.0f; f.spark = true;
        m_puffs.push_back(f);
    }
    for (int i = 0; i < 10; i++) Smoke(p, 1.0f);
}

void Effects::Explosion(const float p[3], int kind) {
    // scale of the fx system: tiny .3, small .55, medium 1, large 1.8, molotov .7
    static const float kScale[5] = {0.3f, 0.55f, 1.0f, 1.8f, 0.7f};
    const float sc = kScale[std::clamp(kind, 0, 4)];
    const bool molotov = kind == 4;
    auto add = [&](const Puff& f) { m_puffs.push_back(f); };
    {   // the initial flash: a short, very bright, wide sprite
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k];
        f.p[2] += 0.5f * sc;
        f.life = 0.18f; f.size0 = 0.8f * sc; f.size1 = 3.2f * sc; f.rgb = 0xFFF0B0; f.alpha = 1.0f;
        add(f);
    }
    const int nFire = (int)(8 + 22 * sc);
    for (int i = 0; i < nFire; i++) {   // the fire ball: orange puffs rolling outward and up
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k] + Rnd(-0.4f, 0.4f) * sc;
        f.v[0] = Rnd(-4.0f, 4.0f) * sc; f.v[1] = Rnd(-4.0f, 4.0f) * sc; f.v[2] = Rnd(0.5f, 5.0f) * (0.5f + 0.5f * sc);
        f.life = Rnd(0.5f, 1.2f) * (0.6f + 0.4f * sc);
        f.size0 = 0.6f * sc; f.size1 = Rnd(2.0f, 3.5f) * sc;
        f.rgb = molotov ? (i % 2 ? 0xFF8A1E : 0xFFB43C) : (i % 3 == 0 ? 0xFFD23C : 0xFF6414); f.alpha = 0.85f;
        add(f);
    }
    for (int i = 0; i < (int)(6 + 14 * sc); i++) {   // sparks
        Puff f = {};
        for (int k = 0; k < 3; k++) f.p[k] = p[k];
        f.v[0] = Rnd(-6.0f, 6.0f) * sc; f.v[1] = Rnd(-6.0f, 6.0f) * sc; f.v[2] = Rnd(3.0f, 9.0f) * (0.5f + 0.5f * sc);
        f.life = Rnd(0.6f, 1.3f); f.size0 = 0.08f; f.size1 = 0.05f;
        f.rgb = 0xFFC850; f.alpha = 1.0f; f.spark = true;
        add(f);
    }
    if (!molotov) {
        for (int i = 0; i < (int)(4 + 14 * sc); i++) {   // debris: dark chunks thrown out that fall back
            Puff f = {};
            for (int k = 0; k < 3; k++) f.p[k] = p[k];
            f.v[0] = Rnd(-7.0f, 7.0f) * sc; f.v[1] = Rnd(-7.0f, 7.0f) * sc; f.v[2] = Rnd(4.0f, 11.0f) * (0.5f + 0.5f * sc);
            f.life = Rnd(1.0f, 2.0f); f.size0 = Rnd(0.07f, 0.16f); f.size1 = f.size0;
            f.rgb = i % 2 ? 0x4A4038 : 0x2A2622; f.alpha = 1.0f; f.spark = true;
            add(f);
        }
        const int nSmoke = (int)(3 + 9 * sc);
        for (int i = 0; i < nSmoke; i++) Smoke(p, 1.0f);   // the rising column
    } else {
        Smoke(p, 0.5f); Smoke(p, 0.5f);
    }
}

void Effects::Scorch(const float p[3], float size) {
    Scorch_ s = {};
    for (int k = 0; k < 3; k++) s.p[k] = p[k];
    s.size = size; s.life = 30.0f;
    if (m_scorch.size() > 32) m_scorch.erase(m_scorch.begin());
    m_scorch.push_back(s);
}

int Effects::AddTrace(const float from[3], const float to[3], float radius, float lifeMs, int alpha) {
    int slot = -1;
    for (int i = 0; i < 16; i++) if (!m_traces[i].exists) { slot = i; break; }
    if (slot < 0) return -1;
    Trace& t = m_traces[slot];
    t.exists = true;
    for (int k = 0; k < 3; k++) { t.a[k] = from[k]; t.b[k] = to[k]; }
    t.createMs = m_clockMs;
    t.alpha = alpha;
    t.radius = radius;
    // the lifetime follows the slot, so a burst of traces stays visible for a moment each
    t.lifeMs = slot < 5 ? lifeMs : slot < 10 ? lifeMs / 2.0f : lifeMs / 4.0f;
    return slot;
}

void Effects::Tracer(const float a[3], const float b[3], bool suppress) {
    if (suppress) return;
    float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-4f) return;
    for (float& v : d) v /= len;
    const float off = len * (float)(rand() % 10001) / 10000.0f;
    const float r = std::min(2.0f + 3.0f * (float)(rand() % 10001) / 10000.0f, len - off);
    float from[3], to[3];
    for (int k = 0; k < 3; k++) { from[k] = a[k] + d[k] * off; to[k] = from[k] + d[k] * r; }
    AddTrace(from, to, 0.01f, 300.0f, 70);
}

int Effects::ActiveTraces() const { int n = 0; for (const Trace& t : m_traces) n += t.exists; return n; }
bool Effects::TraceInfo(int i, float* life, float* radius, int* alpha) const {
    if (i < 0 || i >= 16 || !m_traces[i].exists) return false;
    if (life) *life = m_traces[i].lifeMs;
    if (radius) *radius = m_traces[i].radius;
    if (alpha) *alpha = m_traces[i].alpha;
    return true;
}

namespace {
// CGlass::PanePolyPositions (0x8D5CD8): the five shards of a pane in its unit square
const float kPanePoly[5][3][2] = {{{0, 0}, {0, 1}, {0.4f, 0.5f}}, {{0, 1}, {1, 1}, {0.4f, 0.5f}}, {{0, 0}, {0.4f, 0.5f}, {0.7f, 0}},
                                  {{0.7f, 0}, {0.4f, 0.5f}, {1, 1}}, {{0.7f, 0}, {1, 1}, {1, 0}}};
void PaneCentre(int piece, float out[2]) {
    out[0] = out[1] = 0.0f;
    for (int v = 0; v < 3; v++) { out[0] += kPanePoly[piece][v][0] / 3.0f; out[1] += kPanePoly[piece][v][1] / 3.0f; }
}
unsigned g_glassRng = 0x1234567u;
int GlassRand() { g_glassRng = g_glassRng * 1103515245u + 12345u; return (int)((g_glassRng >> 16) & 0x7fff); }
}  // namespace

void Effects::GlassPanes(const float corner[3], const float fwd[3], const float right[3], const float vel[3], const float centre[3],
                         float centreDrag, float groundZ) {
    const float sy = std::sqrt(fwd[0] * fwd[0] + fwd[1] * fwd[1] + fwd[2] * fwd[2]);
    const float sx = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    if (sx < 1e-4f || sy < 1e-4f) return;
    const int nx = std::min(2 * (int)(sx + 0.75f), 3), ny = std::min(2 * (int)(sy + 0.75f), 3);   // two sections a metre
    if (nx <= 0 || ny <= 0) return;
    const float px = sx / (float)nx, py = sy / (float)ny;
    const float nr[3] = {right[0] / sx, right[1] / sx, right[2] / sx}, nf[3] = {fwd[0] / sy, fwd[1] / sy, fwd[2] / sy};
    for (int y = 0; y < ny; y++)
        for (int x = 0; x < nx; x++)
            for (int piece = 0; piece < 5; piece++) {
                Pane* pane = nullptr;
                for (Pane& q : m_panes) if (!q.exist) { pane = &q; break; }   // FindFreePane
                if (!pane) continue;
                pane->piece = piece;
                for (int k = 0; k < 3; k++) { pane->r[k] = nr[k] * px; pane->u[k] = nf[k] * py; }
                float fw[3] = {pane->r[1] * pane->u[2] - pane->r[2] * pane->u[1], pane->r[2] * pane->u[0] - pane->r[0] * pane->u[2],
                               pane->r[0] * pane->u[1] - pane->r[1] * pane->u[0]};
                const float fl = std::sqrt(fw[0] * fw[0] + fw[1] * fw[1] + fw[2] * fw[2]);
                for (int k = 0; k < 3; k++) pane->f[k] = fl > 1e-6f ? fw[k] / fl : 0.0f;
                float pc[2];
                PaneCentre(piece, pc);
                const float cx = (pc[0] + (float)x) * px, cy = (pc[1] + (float)y) * py;
                for (int k = 0; k < 3; k++) pane->p[k] = corner[k] + nf[k] * cy + nr[k] * cx;
                pane->v[0] = vel[0] + (float)(GlassRand() % 128 - 64) * 0.0015f;
                pane->v[1] = vel[1] + (float)(GlassRand() % 128 - 64) * 0.0015f;
                pane->v[2] = vel[2];
                if (centreDrag != 0.0f) {
                    float d[3] = {pane->p[0] - centre[0], pane->p[1] - centre[1], pane->p[2] - centre[2]};
                    const float dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                    if (dl > 1e-6f) for (int k = 0; k < 3; k++) pane->v[k] += d[k] / dl * centreDrag;
                }
                for (float& s : pane->spin) s = (float)(GlassRand() % 128 - 64) / 500.0f;
                pane->bornMs = m_clockMs;
                pane->groundZ = groundZ;
                pane->exist = true;
            }
}

void Effects::Update(float dt) {
    for (Puff& f : m_puffs) {
        f.age += dt;
        for (int k = 0; k < 3; k++) f.p[k] += f.v[k] * dt;
        if (f.spark) f.v[2] -= 9.0f * dt;          // sparks fall
        else if (f.ring) { const float fr = std::pow(0.985f, dt * 50.0f); for (float& v : f.v) v *= fr; }   // FxInfoFriction
        else for (int k = 0; k < 3; k++) f.v[k] *= std::max(0.0f, 1.0f - 2.5f * dt);   // dust slows
    }
    m_puffs.erase(std::remove_if(m_puffs.begin(), m_puffs.end(), [](const Puff& f) { return f.age >= f.life; }), m_puffs.end());
    m_clockMs += dt * 1000.0f;
    for (Trace& t : m_traces) if (t.exists && m_clockMs - t.createMs >= t.lifeMs) t.exists = false;   // CBulletTrace::Update
    const float ts = dt * 50.0f;
    for (Pane& q : m_panes) {   // CFallingGlassPane::Update, the slow car kind
        if (!q.exist) continue;
        for (int k = 0; k < 3; k++) q.p[k] += ts * 0.35f * q.v[k];
        q.v[2] -= ts / 100.0f;
        for (float* a : {q.r, q.f, q.u}) {
            const float c[3] = {q.spin[1] * a[2] - q.spin[2] * a[1], q.spin[2] * a[0] - q.spin[0] * a[2], q.spin[0] * a[1] - q.spin[1] * a[0]};
            for (int k = 0; k < 3; k++) a[k] += c[k] * ts;
        }
        if (q.p[2] < q.groundZ) {   // a tinkle of glass on the ground
            q.exist = false;
            for (int i = 0; i < 4; i++) {
                Puff f{};
                for (int k = 0; k < 3; k++) f.p[k] = q.p[k];
                f.p[2] = q.groundZ + 0.02f;
                f.v[0] = (float)(GlassRand() % 100 - 50) * 0.02f; f.v[1] = (float)(GlassRand() % 100 - 50) * 0.02f; f.v[2] = 1.0f;
                f.life = 0.4f; f.size0 = f.size1 = 0.02f + (float)(GlassRand() % 100) * 0.0004f;
                f.rgb = 0xFFFFFF; f.alpha = 0.6f; f.spark = true;
                m_puffs.push_back(f);
            }
        }
    }
    for (Scorch_& c : m_scorch) c.age += dt;
    m_scorch.erase(std::remove_if(m_scorch.begin(), m_scorch.end(), [](const Scorch_& c) { return c.age >= c.life; }), m_scorch.end());
}

void Effects::Render(D3DRenderer& r) {
    bool panes = false;
    for (const Pane& q : m_panes) panes |= q.exist;
    if (!m_tex || (m_puffs.empty() && !ActiveTraces() && m_scorch.empty() && !panes)) return;
    const D3DMATRIX& v = r.GetViewMatrix();
    if (panes) {   // CFallingGlassPane::Render: the highlight polys, added on top of the scene
        const float camF[3] = {v._13, v._23, v._33};
        std::vector<D3DRenderer::ColorTexVertex> gv;
        std::vector<uint16_t> gi;
        for (const Pane& q : m_panes) {
            if (!q.exist) continue;
            // CGlass::CalcAlphaWithNormal: the camera's view reflected off the shard, against light from (1, 1, -1)
            const float fl = std::sqrt(q.f[0] * q.f[0] + q.f[1] * q.f[1] + q.f[2] * q.f[2]);
            const float n[3] = {q.f[0] / fl, q.f[1] / fl, q.f[2] / fl};
            const float dn = camF[0] * n[0] + camF[1] * n[1] + camF[2] * n[2];
            const float refl[3] = {camF[0] - 2.0f * dn * n[0], camF[1] - 2.0f * dn * n[1], camF[2] - 2.0f * dn * n[2]};
            const float spec = (refl[0] + refl[1] - refl[2]) * 0.57f;
            const float alpha = std::pow(spec, 6.0f) * 235.0f + 20.0f;
            const float age = std::clamp(m_clockMs - q.bornMs, 0.0f, 500.0f);
            const uint32_t c = (uint32_t)std::max(64.0f, std::min(255.0f, alpha * age / 500.0f));
            float pc[2];
            PaneCentre(q.piece, pc);
            static const float kUv[3][2] = {{0.5f, 0.5f}, {0.5f, 0.6f}, {0.6f, 0.6f}};
            const uint16_t base = (uint16_t)gv.size();
            for (int i = 0; i < 3; i++) {
                const float ox = kPanePoly[q.piece][i][0] - pc[0], oy = kPanePoly[q.piece][i][1] - pc[1];
                gv.push_back({q.p[0] + q.r[0] * ox + q.u[0] * oy, q.p[1] + q.r[1] * ox + q.u[1] * oy, q.p[2] + q.r[2] * ox + q.u[2] * oy,
                              (c << 24) | (c << 16) | (c << 8) | c, kUv[i][0], kUv[i][1]});
            }
            for (uint16_t k : {0, 1, 2}) gi.push_back((uint16_t)(base + k));
        }
        if (!gi.empty()) r.DrawBlendedTriangles(gv.data(), (uint32_t)gv.size(), gi.data(), (uint32_t)gi.size(), m_tex, true);
    }
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
    for (const Scorch_& c : m_scorch) {   // flat dark blob on the ground, fading over the last 5 s
        const float a = 0.7f * std::min(1.0f, (c.life - c.age) / 5.0f) * std::min(1.0f, c.age * 8.0f);
        const float h = c.size * 0.5f * 0.6f;
        const float rx[3] = {h, 0, 0}, ux[3] = {0, h, 0};
        const float cp[3] = {c.p[0], c.p[1], c.p[2] + 0.04f};
        quad(cp, rx, ux, ((uint32_t)(a * 255.0f) << 24) | 0x0A0806);
    }
    for (const Puff& f : m_puffs) {
        const float t = f.age / f.life;
        float s = f.size0 + (f.size1 - f.size0) * t;
        float a = f.alpha * (1.0f - t) * (f.spark ? 1.0f : std::min(1.0f, t * 8.0f));
        if (f.ring) {   // heli_dust: size and alpha keys 0 until half life, alpha peaking at 0.6
            if (t < 0.5f) continue;
            s = f.size1 * (t - 0.5f) * 2.0f;
            a = f.alpha * (t < 0.6f ? (t - 0.5f) * 10.0f : (1.0f - t) / 0.4f);
        }
        const float rx[3] = {right[0] * s, right[1] * s, right[2] * s}, ux[3] = {up[0] * s, up[1] * s, up[2] * s};
        quad(f.p, rx, ux, ((uint32_t)(a * 255.0f) << 24) | f.rgb);
    }
    // CBulletTraces::Render: a thin band from the start to the end that narrows and fades as it ages;
    // only the vertex at the end centre carries alpha.
    const float camPos[3] = {-(v._41 * v._11 + v._42 * v._12 + v._43 * v._13), -(v._41 * v._21 + v._42 * v._22 + v._43 * v._23), -(v._41 * v._31 + v._42 * v._32 + v._43 * v._33)};
    for (const Trace& tr : m_traces) {
        if (!tr.exists) continue;
        const float t = 1.0f - (m_clockMs - tr.createMs) / tr.lifeMs;
        float dir[3] = {tr.b[0] - tr.a[0], tr.b[1] - tr.a[1], tr.b[2] - tr.a[2]};
        const float dl = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
        float c2o[3] = {tr.a[0] - camPos[0], tr.a[1] - camPos[1], tr.a[2] - camPos[2]};
        const float cl0 = std::sqrt(c2o[0] * c2o[0] + c2o[1] * c2o[1] + c2o[2] * c2o[2]);
        if (dl < 1e-4f || cl0 < 1e-4f) continue;
        for (int k = 0; k < 3; k++) { dir[k] /= dl; c2o[k] /= cl0; }
        float sz[3] = {c2o[1] * dir[2] - c2o[2] * dir[1], c2o[2] * dir[0] - c2o[0] * dir[2], c2o[0] * dir[1] - c2o[1] * dir[0]};
        const float sl = std::sqrt(sz[0] * sz[0] + sz[1] * sz[1] + sz[2] * sz[2]);
        if (sl < 1e-5f) continue;
        for (float& x : sz) x = x / sl * tr.radius * t;
        float cur[3];
        for (int k = 0; k < 3; k++) cur[k] = tr.b[k] - (tr.b[k] - tr.a[k]) * t;
        const float P[6][3] = {
            {cur[0], cur[1], cur[2]}, {cur[0] + sz[0], cur[1] + sz[1], cur[2] + sz[2]}, {cur[0] - sz[0], cur[1] - sz[1], cur[2] - sz[2]},
            {tr.b[0], tr.b[1], tr.b[2]}, {tr.b[0] + sz[0], tr.b[1] + sz[1], tr.b[2] + sz[2]}, {tr.b[0] - sz[0], tr.b[1] - sz[1], tr.b[2] - sz[2]}};
        if (vs.size() + 6 > 65000) break;
        const uint16_t base = (uint16_t)vs.size();
        for (int i = 0; i < 6; i++) {
            const uint32_t a = i == 3 ? (uint32_t)std::clamp((float)tr.alpha * t, 0.0f, 255.0f) : 0u;
            vs.push_back({P[i][0], P[i][1], P[i][2], (a << 24) | 0xFFFF80, 0.5f, 0.5f});
        }
        const uint16_t q[12] = {4, 1, 3, 1, 0, 3, 0, 2, 3, 3, 2, 5};
        for (uint16_t k : q) ix.push_back(base + k);
    }
    if (!ix.empty()) r.DrawBlendedTriangles(vs.data(), (uint32_t)vs.size(), ix.data(), (uint32_t)ix.size(), m_tex);
}
