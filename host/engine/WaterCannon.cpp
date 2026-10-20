#include "WaterCannon.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {
float RandRange(float a, float b) { return a + (b - a) * ((float)(std::rand() % 10000) / 9999.0f); }

void Normalize(float v[3]) {
    const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-6f) { v[0] /= l; v[1] /= l; v[2] /= l; }
}
}  // namespace

bool WaterCannons::Init(D3DRenderer& r) {
    const uint32_t px[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    m_white = r.CreateTexture(2, 2, D3DFMT_A8R8G8B8, px, sizeof(px));
    for (Cannon& c : m_cannons) c = Cannon();
    return m_white != nullptr;
}

void WaterCannons::Shutdown(D3DRenderer& r) {
    if (m_white) r.DestroyTexture(m_white);
    m_white = nullptr;
}

void WaterCannons::InitCannon(Cannon& c, float nowMs) {   // CWaterCannon::Init
    c = Cannon();
    c.creationMs = nowMs;
}

void WaterCannons::UpdateOne(uint32_t owner, const float start[3], const float end[3], float nowMs) {
    Cannon* c = nullptr;
    for (Cannon& k : m_cannons) if (k.id == owner) { c = &k; break; }
    if (!c) {   // the first free slot
        for (Cannon& k : m_cannons) if (!k.id) { c = &k; break; }
        if (!c) return;
        InitCannon(*c, nowMs);
        c->id = owner;
    }
    for (int i = 0; i < 3; i++) { c->point[c->count][i] = start[i]; c->speed[c->count][i] = end[i]; }   // Update_NewInput
    c->used[c->count] = true;
}

void WaterCannons::OncePerFrame(Cannon& c, int index, float ts, uint32_t frame, float nowMs, const Hooks& h) {
    if (c.creationMs + 150.0f < nowMs) {   // after 150 ms the ring advances every frame and frees the slot it lands on
        c.count = (c.count + 1) % kSections;
        c.used[c.count] = false;
    }
    for (int i = 0; i < kSections; i++) {
        if (!c.used[i]) continue;
        c.speed[i][2] -= ts / 250.0f;
        for (int k = 0; k < 3; k++) c.point[i][k] += c.speed[i][k] * ts;
    }
    for (int i = 0; i < kSections; i++)
        if (c.used[i] && h.extinguish) h.extinguish(c.point[i], 2.0f, 0.5f);
    if ((uint8_t)(frame + (uint32_t)index) % 4 == 0) PushPeds(c, ts, h);
    for (int i = 0; i < kSections; i++) if (c.used[i]) return;
    c.id = 0;   // every section is gone: the cannon is free again
}

void WaterCannons::PushPeds(Cannon& c, float ts, const Hooks& h) {
    if (!h.pedPos || !h.pedHit) return;
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    bool any = false;
    for (int i = 0; i < kSections; i++) {
        if (!c.used[i]) continue;
        any = true;
        for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], c.point[i][k]); hi[k] = std::max(hi[k], c.point[i][k]); }
    }
    if (!any) return;
    for (int p = 0; p < h.pedCount; p++) {
        float pp[3];
        if (!h.pedPos(p, pp)) continue;
        if (pp[0] < lo[0] || pp[0] > hi[0] || pp[1] < lo[1] || pp[1] > hi[1] || pp[2] < lo[2] || pp[2] > hi[2]) continue;
        for (int i = 0; i < kSections; i++) {
            const float dx = pp[0] - c.point[i][0], dy = pp[1] - c.point[i][1], dz = pp[2] - c.point[i][2];
            if (dx * dx + dy * dy + dz * dz >= 5.0f) continue;
            h.pedHit(p, c.point[i], c.speed[i], ts);
            break;
        }
    }
}

void WaterCannons::Update(float ts, uint32_t frame, float nowMs, const Hooks& h) {
    int index = 0;
    for (Cannon& c : m_cannons) {
        if (c.id > 0) OncePerFrame(c, index, ts, frame, nowMs, h);
        index++;
    }
}

void WaterCannons::Render(D3DRenderer& r, const float camFwd[3], const RenderHooks& h) {
    static const uint16_t kIdx[18] = {0, 1, 2, 1, 3, 2, 4, 5, 6, 5, 7, 6, 8, 9, 10, 9, 11, 10};
    std::vector<D3DRenderer::ColorTexVertex> verts;
    std::vector<uint16_t> idx;
    for (Cannon& c : m_cannons) {
        if (!c.id) continue;
        int prev = c.count % kSections;
        int curr = prev == 0 ? kSections - 1 : prev - 1;
        bool haveMatrix = false;
        float up[3] = {}, right[3] = {}, fwd[3] = {};
        for (int i = 0; i < kSections; i++) {
            if (c.used[prev] && c.used[curr]) {
                const float* pp = c.point[prev];
                const float* cp = c.point[curr];
                const float d[3] = {pp[0] - cp[0], pp[1] - cp[1], pp[2] - cp[2]};
                if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] < 25.0f) {
                    if (!haveMatrix) {
                        haveMatrix = true;
                        up[0] = d[1] * camFwd[2] - d[2] * camFwd[1];
                        up[1] = d[2] * camFwd[0] - d[0] * camFwd[2];
                        up[2] = d[0] * camFwd[1] - d[1] * camFwd[0];
                        Normalize(up);
                        right[0] = RandRange(0, 1); right[1] = RandRange(0, 1); right[2] = RandRange(0, 1); Normalize(right);
                        fwd[0] = RandRange(0, 1); fwd[1] = RandRange(0, 1); fwd[2] = RandRange(0, 1); Normalize(fwd);
                        for (int k = 0; k < 3; k++) { up[k] /= 20.0f; right[k] /= 20.0f; fwd[k] /= 20.0f; }
                    }
                    const float size = (float)(i * i) / (float)kSections + 3.0f;
                    const float alpha = 64.0f * (1.0f - (float)i / (float)kSections);
                    const uint32_t col = ((uint32_t)alpha << 24) | (200u << 16) | (200u << 8) | 255u;
                    float nz = 0.0f;
                    const float t = h.ray ? h.ray(pp, cp, &nz) : 1.0f;
                    if (t < 1.0f) {   // the jet hits something: splash there and stop drawing this cannon
                        float hit[3];
                        for (int k = 0; k < 3; k++) hit[k] = pp[k] + (cp[k] - pp[k]) * t;
                        float nrm[3] = {-d[0], -d[1], 0.0f};
                        Normalize(nrm);
                        const float hz = std::sqrt(std::max(0.0f, 1.0f - nz * nz));
                        nrm[0] *= hz; nrm[1] *= hz; nrm[2] = nz;
                        const float k0 = 3.0f * RandRange(0.2f, 1.8f);
                        const float vel0[3] = {nrm[0] * k0, nrm[1] * k0, nrm[2] * k0};
                        if (h.splash) h.splash(hit, vel0);
                        break;
                    }
                    const float* dirs[3] = {up, right, fwd};
                    const uint16_t base = (uint16_t)verts.size();
                    for (int a = 0; a < 3; a++) {
                        float o[3];
                        for (int k = 0; k < 3; k++) o[k] = dirs[a][k] * size;
                        const float* pts[4] = {cp, cp, pp, pp};
                        const float sg[4] = {-1, 1, -1, 1};
                        for (int v = 0; v < 4; v++)
                            verts.push_back({pts[v][0] + o[0] * sg[v], pts[v][1] + o[1] * sg[v], pts[v][2] + o[2] * sg[v], col, 0.5f, 0.5f});
                    }
                    for (int k = 0; k < 18; k++) idx.push_back((uint16_t)(base + kIdx[k]));
                }
            }
            curr = prev;
            prev = prev == 0 ? kSections - 1 : prev - 1;
        }
    }
    if (!idx.empty() && verts.size() < 65000) r.DrawBlendedTriangles(verts.data(), (uint32_t)verts.size(), idx.data(), (uint32_t)idx.size(), m_white);
}

int WaterCannons::ActiveCannons() const {
    int n = 0;
    for (const Cannon& c : m_cannons) n += c.id ? 1 : 0;
    return n;
}

int WaterCannons::ActiveSections(uint32_t owner) const {
    for (const Cannon& c : m_cannons) {
        if (c.id != owner) continue;
        int n = 0;
        for (int i = 0; i < kSections; i++) n += c.used[i] ? 1 : 0;
        return n;
    }
    return 0;
}

bool WaterCannons::Section(uint32_t owner, int slot, float pos[3], float vel[3]) const {
    for (const Cannon& c : m_cannons) {
        if (c.id != owner || slot < 0 || slot >= kSections || !c.used[slot]) continue;
        for (int k = 0; k < 3; k++) { pos[k] = c.point[slot][k]; vel[k] = c.speed[slot][k]; }
        return true;
    }
    return false;
}
