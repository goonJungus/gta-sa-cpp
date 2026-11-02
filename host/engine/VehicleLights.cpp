// Vehicle head and tail lights: CVehicle::GetVehicleLightsStatus, DoVehicleLights,
// DoHeadLightEffect and DoTailLightEffect. See Vehicle.h.

#define NOMINMAX
#include "Vehicle.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

// The car's matrix: the simulated one, else the heading's.
void LightFrame(const VehicleInstance& c, float r[3], float f[3], float u[3]) {
    if (c.phys.active || c.hasRot) {
        for (int k = 0; k < 3; k++) { r[k] = c.rot[k]; f[k] = c.rot[3 + k]; u[k] = c.rot[6 + k]; }
        return;
    }
    const float cs = std::cos(c.heading), sn = std::sin(c.heading);
    r[0] = cs; r[1] = sn; r[2] = 0; f[0] = -sn; f[1] = cs; f[2] = 0; u[0] = 0; u[1] = 0; u[2] = 1;
}

uint32_t Argb(int a, int r, int g, int b) {
    auto c = [](int v) { return (uint32_t)std::clamp(v, 0, 255); };
    return (c(a) << 24) | (c(r) << 16) | (c(g) << 8) | c(b);
}

}  // namespace

bool Vehicles::LightsOn(const VehicleInstance& c, const LightEnv& env) {
    // Each car picks its own minute (the low six bits of its random seed) to switch on after 20:00 and off
    // before 7:00; fog and wet roads switch them on by the seed too.
    const int m = c.randomSeed & 0x3f;
    const float s = (float)c.randomSeed * 2e-5f;
    return env.hour > 20 || (env.hour > 19 && env.minute > m) || env.hour < 6 || (env.hour < 7 && env.minute < m) ||
           s < env.foggy || s < env.wet;
}

int Vehicles::ProcessCarAlarms(float dt, float x, float y) {
    int best = -1;
    float bd = 1e30f;
    const int step = (int)(dt * 1000.0f);
    for (size_t i = 0; i < m_cars.size(); i++) {
        VehicleInstance& c = m_cars[i];
        if (c.alarmMs <= 0) continue;
        if (c.wrecked || c.removed) { c.alarmMs = 0; continue; }
        c.alarmMs = c.alarmMs >= step ? c.alarmMs - step : 0;
        const float d = (c.x - x) * (c.x - x) + (c.y - y) * (c.y - y);
        if (c.alarmMs > 0 && d < bd) { bd = d; best = (int)i; }
    }
    return best;
}

bool Vehicles::UsesSiren(int model) {
    switch (model) {
    case 407: case 416: case 423: return true;      // firetruck, ambulance, mr whoopee
    case 432: return false;                         // rhino
    case 427: case 430: case 433: case 490: case 497: case 523: case 528:
    case 596: case 597: case 598: case 599: case 601: return true;   // IsLawEnforcementVehicle
    default: return false;
    }
}

void Vehicles::RenderLights(D3DRenderer& r, const float cam[3], const LightEnv& env) const {
    std::vector<D3DRenderer::GlowSprite> glow;
    std::vector<D3DRenderer::ColorTexVertex> beamVerts;
    std::vector<uint16_t> beamIdx;
    const uint32_t ms = (uint32_t)GetTickCount();
    for (const VehicleInstance& c : m_cars) {
        if (c.siren && !c.removed && !c.wrecked) {
            // CAutomobile::PreRender light bars: four lamps across the bar, each a quarter cycle (64 ms) behind
            // the last; a lamp shows colour A for the first 256 ms of every 1024, colour B for the third.
            int bar = -1;
            switch (c.model) {
            case 407: bar = 0; break; case 416: bar = 1; break; case 427: bar = 2; break;
            case 596: case 597: case 598: bar = 3; break; case 599: bar = 4; break;
            }
            static const float kBar[5][3] = {{0.9f, 3.2f, 1.3f}, {0.6f, 0.9f, 1.2f}, {0.55f, 1.1f, 1.4f}, {0.7f, -0.4f, 1.0f}, {0.7f, -0.1f, 1.2f}};
            // colour B: firetruck yellow, ambulance white, the law blue; colour A is red
            static const uint32_t kB[5] = {0xffffff00u, 0xffffffffu, 0xff0000ffu, 0xff0000ffu, 0xff0000ffu};
            if (bar >= 0) {
                float rt[3], fw[3], up[3];
                if (c.phys.active || c.hasRot) for (int k = 0; k < 3; k++) { rt[k] = c.rot[k]; fw[k] = c.rot[3 + k]; up[k] = c.rot[6 + k]; }
                else { const float cs = std::cos(c.heading), sn = std::sin(c.heading); rt[0] = cs; rt[1] = sn; rt[2] = 0; fw[0] = -sn; fw[1] = cs; fw[2] = 0; up[0] = up[1] = 0; up[2] = 1; }
                for (int i = 0; i < 4; i++) {
                    const uint32_t ph = ms + (uint32_t)i * 0x40u;
                    const bool a = (ph & 0x300u) == 0, b = ((ph >> 8) & 3u) == 2;
                    if (!a && !b) continue;
                    const float t = (float)i / 3.0f;   // from the left end (-x) to the right (+x)
                    const float l[3] = {kBar[bar][0] * (2.0f * t - 1.0f), kBar[bar][1], kBar[bar][2]};
                    float p[3];
                    for (int k = 0; k < 3; k++) p[k] = (k == 0 ? c.x : k == 1 ? c.y : c.z) + rt[k] * l[0] + fw[k] * l[1] + up[k] * l[2];
                    float d[3] = {cam[0] - p[0], cam[1] - p[1], cam[2] - p[2]};   // out of the bar's housing
                    const float dl = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
                    const float pull = dl > 1e-3f ? std::min(0.3f, dl * 0.5f) / dl : 0.0f;
                    glow.push_back({p[0] + d[0] * pull, p[1] + d[1] * pull, p[2] + d[2] * pull, 0.4f, a ? 0xffff0000u : kB[bar]});
                }
            }
        }
    }
    for (const VehicleInstance& c : m_cars) {
        if (c.removed || c.wrecked || !c.engineOn) continue;   // bEngineOn gates every light
        const float dx = c.x - cam[0], dy = c.y - cam[1];
        if (dx * dx + dy * dy > 150.0f * 150.0f) continue;
        auto mi = m_models.find(c.model);
        if (mi == m_models.end()) continue;
        const VehicleModel& vm = mi->second;
        const VehicleInfo* info = Info(c.model);
        const HandlingData* h = info ? Handling(info->handling) : nullptr;
        const bool halogen = h && (h->handlingFlags & 0x400000) != 0;   // HALOGEN_LIGHTS
        const bool twin = UsesCarPhysics(c.model);                     // CAutomobile: a pair, bikes one
        // DoVehicleLights: a ringing alarm flashes the headlights every 256 ms (with the engine running).
        const bool ringing = c.alarmMs > 0;
        const bool alarmDark = ringing && (ms & 0x100u) == 0;
        const bool on = (LightsOn(c, env) || ringing) && !alarmDark;
        // CDamageManager::GetLightStatus: front right, front left; both rear lights read the rear left (as retail).
        const bool frOk = c.lightStatus[1] == 0, flOk = c.lightStatus[0] == 0, rearOk = c.lightStatus[3] == 0;
        float rt[3], fw[3], up[3];
        LightFrame(c, rt, fw, up);
        auto world = [&](const float l[3], float out[3]) {
            for (int k = 0; k < 3; k++) out[k] = (k == 0 ? c.x : k == 1 ? c.y : c.z) + rt[k] * l[0] + fw[k] * l[1] + up[k] * l[2];
        };
        auto push = [&](const float p[3], float size, uint32_t col) {
            float d[3] = {cam[0] - p[0], cam[1] - p[1], cam[2] - p[2]};
            const float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            const float pull = l > 1e-3f ? std::min(0.3f, l * 0.5f) / l : 0.0f;   // in front of the lamp glass
            glow.push_back({p[0] + d[0] * pull, p[1] + d[1] * pull, p[2] + d[2] * pull, size, col});
        };
        // DoHeadLightEffect: seen from the front, (1 - d / 150) * 0.4 * sqrt(facing).
        auto head = [&](int id, bool right, bool disabled) {
            const float* lp = vm.lightPos[id];
            if (id == 2 && lp[0] == 0.0f && lp[1] == 0.0f && lp[2] == 0.0f) return false;
            if (!vm.lightFound[id]) return false;
            const float l[3] = {right ? std::fabs(lp[0]) : -std::fabs(lp[0]), lp[1] + 0.05f, lp[2]};
            float p[3];
            world(l, p);
            float v[3] = {cam[0] - p[0], cam[1] - p[1], cam[2] - p[2]};
            const float dist = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            if (dist > 1e-4f) for (float& e : v) e /= dist;
            const float facing = v[0] * fw[0] + v[1] * fw[1] + v[2] * fw[2];
            if (disabled || facing <= 0.0f) return !disabled;
            const float f = std::sqrt(facing);
            const float size = std::max(0.0f, 1.0f - dist / 150.0f) * 0.4f * f;
            const int rg = halogen ? (int)(190.0f * f) : (int)(210.0f * f), b = halogen ? (int)(255.0f * f) : (int)(195.0f * f);
            push(p, size, Argb(0x80, rg, rg, b));
            if (f > 0.9f && dist < 40.0f)   // CORONATYPE_HEADLIGHTLINE: the small bright core
                push(p, 0.075f, halogen ? Argb(0xff, 150, 150, 195) : Argb(0xff, 160, 160, 140));
            return true;
        };
        // DoTailLightEffect: seen from behind, (1 - d / 150) * 0.2 * facing; 128 red braking, 96 with the lights on.
        auto tail = [&](int id, bool right, bool disabled, bool staticEmission) {
            const float* lp = vm.lightPos[id];
            if (!vm.lightFound[id]) return;
            if ((c.model == 439 || c.model == 475) && c.panelStatus[6] != 0) return;   // stallion / sabre: lights in the bumper
            const float l[3] = {right ? std::fabs(lp[0]) : -std::fabs(lp[0]), lp[1], lp[2]};
            float p[3];
            world(l, p);
            float v[3] = {cam[0] - p[0], cam[1] - p[1], cam[2] - p[2]};
            const float dist = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            if (dist > 1e-4f) for (float& e : v) e /= dist;
            const float facing = -(v[0] * fw[0] + v[1] * fw[1] + v[2] * fw[2]);
            if (disabled || facing <= 0.0f) return;
            const bool braking = c.phys.brake > 0.0f && c.hasDriver;
            if (!braking && !staticEmission) return;
            const float red = (braking ? 128.0f : 96.0f) * facing;
            push(p, std::max(0.0f, 1.0f - dist / 150.0f) * 0.2f * facing, Argb(0x80, (int)red, 0, 0));
        };
        // CVehicle::DoHeadLightBeam (from CAutomobile::Render / CBike::Render): a faint additive fan of five points
        // ahead of the lit main headlight, 3 m long and 1 m across at the far edge, brightest (32) seen from the side.
        const bool beams = !vm.isHeli && c.model != 441 && c.model != 432;   // not the RC Bandit or the Rhino
        auto beam = [&](bool right) {
            if (!beams || !vm.lightFound[0]) return;
            const float* lp = vm.lightPos[0];
            const float l[3] = {right ? lp[0] : -lp[0], lp[1], lp[2]};
            float p[3];
            world(l, p);
            float v[3] = {cam[0] - p[0], cam[1] - p[1], cam[2] - p[2]};
            const float dist = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            if (dist < 1e-4f) return;
            for (float& e : v) e /= dist;
            const int a = (int)((1.0f - std::fabs(v[0] * fw[0] + v[1] * fw[1] + v[2] * fw[2])) * 32.0f);
            const float drop = c.model == 530 ? 0.5f : 0.15f;   // the forklift's forks tilt further
            float n[3] = {fw[0] - up[0] * drop, fw[1] - up[1] * drop, fw[2] - up[2] * drop};
            const float nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            for (float& e : n) e /= nl;
            float s[3] = {n[1] * v[2] - n[2] * v[1], n[2] * v[0] - n[0] * v[2], n[0] * v[1] - n[1] * v[0]};
            const float sl = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
            if (sl < 1e-5f) return;
            for (float& e : s) e /= sl;
            float o[3];
            for (int k = 0; k < 3; k++) o[k] = p[k] - fw[k] * 0.1f;
            auto at = [&](float along, float side, float out[3]) { for (int k = 0; k < 3; k++) out[k] = o[k] + n[k] * along + s[k] * side; };
            float pts[5][3];
            at(0.0f, -0.05f, pts[0]); at(0.0f, 0.05f, pts[1]); at(3.0f, -0.5f, pts[2]); at(3.0f, 0.5f, pts[3]); at(0.2f, 0.0f, pts[4]);
            const int al[5] = {a, a, 0, 0, a};
            const uint16_t base = (uint16_t)beamVerts.size();
            for (int k = 0; k < 5; k++) {
                const uint32_t g = (uint32_t)std::clamp(al[k], 0, 255);   // alpha x white, added: the blend is ONE / ONE
                beamVerts.push_back({pts[k][0], pts[k][1], pts[k][2], 0xFF000000u | (g << 16) | (g << 8) | g, 0.5f, 0.5f});
            }
            for (uint16_t i : {0, 1, 4, 1, 3, 4, 2, 3, 4, 0, 2, 4}) beamIdx.push_back((uint16_t)(base + i));
        };
        // CVehicle::DoHeadLightReflectionImpl -> CShadows::StoreCarLightShadow: a soft pool on the road ahead of the lit
        // headlights (one for a twin pair, one a light for a single), grey 45, seen within 27 m and fading from 20.
        auto pool = [&](bool twinPool, bool right) {
            if (!beams || !vm.lightFound[0] || c.phys.numContactWheels <= 0) return;
            const float* lp = vm.lightPos[0];
            const bool bike = c.model == 448 || c.model == 461 || c.model == 462 || c.model == 463 || c.model == 468 || c.model == 471 || c.model == 521 || c.model == 522 || c.model == 523 || c.model == 581 || c.model == 586;
            const float size = (!twinPool && bike) ? 1.25f : std::fabs(lp[0]) * 4.0f;
            const float fl = std::sqrt(fw[0] * fw[0] + fw[1] * fw[1]);
            if (fl < 1e-4f || size < 0.2f) return;
            const float fx = fw[0] / fl, fy = fw[1] / fl;
            const float off = size * 2.0f + 1.0f + lp[1];
            float cx = c.x + fx * off, cy = c.y + fy * off;
            if (!twinPool) { const float sgn = right ? 1.0f : -1.0f; cx += fy * lp[0] * sgn; cy -= fx * lp[0] * sgn; }
            float gz = 0.0f;
            for (int w = 0; w < 4; w++) gz += c.phys.colPt[w][2];
            gz = gz * 0.25f + 0.05f;
            const float dx = cx - cam[0], dy = cy - cam[1], dd = std::sqrt(dx * dx + dy * dy);
            if (dd >= 27.0f) return;
            const float fade = dd < 20.0f ? 1.0f : 1.0f - (dd - 20.0f) / 7.0f;
            const uint32_t g = (uint32_t)std::clamp((int)(45.0f * fade), 0, 255);
            const uint16_t base = (uint16_t)beamVerts.size();
            beamVerts.push_back({cx, cy, gz, 0xFF000000u | (g << 16) | (g << 8) | g, 0.5f, 0.5f});
            const int N = 12;
            for (int k = 0; k < N; k++) {
                const float a = 6.2831853f * (float)k / (float)N, ca = std::cos(a), sa = std::sin(a);
                const float ex = fx * ca * size * 2.0f + fy * sa * size, ey = fy * ca * size * 2.0f - fx * sa * size;
                beamVerts.push_back({cx + ex, cy + ey, gz, 0xFF000000u, 0.5f, 0.5f});
            }
            for (int k = 0; k < N; k++) {
                beamIdx.push_back(base); beamIdx.push_back((uint16_t)(base + 1 + k)); beamIdx.push_back((uint16_t)(base + 1 + (k + 1) % N));
            }
        };
        if (on) {
            if (head(0, true, !frOk)) { head(2, true, !frOk); beam(true); pool(false, true); }
            if (twin && head(0, false, !flOk)) { head(2, false, !flOk); beam(false); pool(false, false); }
        }
        tail(1, true, !rearOk, on);
        tail(3, true, !rearOk, on);
        if (twin) { tail(1, false, !rearOk, on); tail(3, false, !rearOk, on); }
    }
    if (!beamIdx.empty()) {
        static IDirect3DTexture9* s_white = nullptr;   // the beams are flat colour: one white texel
        if (!s_white) { const uint32_t px = 0xFFFFFFFFu; s_white = r.CreateTexture(1, 1, D3DFMT_A8R8G8B8, &px, 4); }
        r.DrawBlendedTriangles(beamVerts.data(), (uint32_t)beamVerts.size(), beamIdx.data(), (uint32_t)beamIdx.size(), s_white, true);
    }
    {   // winch ropes and magnets: a thin dark ribbon from the belly to the hook, a flat disc at the hook
        std::vector<D3DRenderer::ColorTexVertex> rv;
        std::vector<uint16_t> ri;
        for (const VehicleInstance& c : m_cars) {
            const VehicleInstance::Winch& w = c.winch;
            if (!w.on || c.removed || w.fresh) continue;
            const float up[3] = {c.phys.active ? c.rot[6] : 0.0f, c.phys.active ? c.rot[7] : 0.0f, c.phys.active ? c.rot[8] : 1.0f};
            const float a[3] = {c.x - up[0] * 1.2f, c.y - up[1] * 1.2f, c.z - up[2] * 1.2f};
            const float d[3] = {w.h[0] - a[0], w.h[1] - a[1], w.h[2] - a[2]};
            const float v[3] = {cam[0] - a[0], cam[1] - a[1], cam[2] - a[2]};
            float s[3] = {d[1] * v[2] - d[2] * v[1], d[2] * v[0] - d[0] * v[2], d[0] * v[1] - d[1] * v[0]};
            const float sl = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
            if (sl < 1e-5f || rv.size() + 8 > 60000) continue;
            for (float& e : s) e *= 0.03f / sl;
            const uint16_t b0 = (uint16_t)rv.size();
            const uint32_t col = 0xFF2A2A2Au;
            rv.push_back({a[0] - s[0], a[1] - s[1], a[2] - s[2], col, 0.5f, 0.5f});
            rv.push_back({a[0] + s[0], a[1] + s[1], a[2] + s[2], col, 0.5f, 0.5f});
            rv.push_back({w.h[0] + s[0], w.h[1] + s[1], w.h[2] + s[2], col, 0.5f, 0.5f});
            rv.push_back({w.h[0] - s[0], w.h[1] - s[1], w.h[2] - s[2], col, 0.5f, 0.5f});
            for (uint16_t k : {0, 1, 2, 0, 2, 3}) ri.push_back((uint16_t)(b0 + k));
            const float rad = w.type == 3 ? 0.55f : 0.25f;   // the magnet's plate
            const uint16_t b1 = (uint16_t)rv.size();
            const uint32_t pc = 0xFF505050u;
            rv.push_back({w.h[0] - rad, w.h[1] - rad, w.h[2], pc, 0.5f, 0.5f});
            rv.push_back({w.h[0] + rad, w.h[1] - rad, w.h[2], pc, 0.5f, 0.5f});
            rv.push_back({w.h[0] + rad, w.h[1] + rad, w.h[2], pc, 0.5f, 0.5f});
            rv.push_back({w.h[0] - rad, w.h[1] + rad, w.h[2], pc, 0.5f, 0.5f});
            for (uint16_t k : {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2}) ri.push_back((uint16_t)(b1 + k));
        }
        if (!ri.empty()) {
            static IDirect3DTexture9* s_whiteR = nullptr;
            if (!s_whiteR) { const uint32_t px = 0xFFFFFFFFu; s_whiteR = r.CreateTexture(1, 1, D3DFMT_A8R8G8B8, &px, 4); }
            r.DrawBlendedTriangles(rv.data(), (uint32_t)rv.size(), ri.data(), (uint32_t)ri.size(), s_whiteR, false);
        }
    }
    if (!glow.empty()) r.DrawCoronas(glow.data(), (int)glow.size());
}
