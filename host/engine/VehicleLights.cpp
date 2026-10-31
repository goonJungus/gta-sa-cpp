// Vehicle head and tail lights: CVehicle::GetVehicleLightsStatus, DoVehicleLights,
// DoHeadLightEffect and DoTailLightEffect. See Vehicle.h.

#define NOMINMAX
#include "Vehicle.h"

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

void Vehicles::RenderLights(D3DRenderer& r, const float cam[3], const LightEnv& env) const {
    std::vector<D3DRenderer::GlowSprite> glow;
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
        const bool on = LightsOn(c, env);
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
        if (on) {
            if (head(0, true, !frOk)) head(2, true, !frOk);
            if (twin && head(0, false, !flOk)) head(2, false, !flOk);
        }
        tail(1, true, !rearOk, on);
        tail(3, true, !rearOk, on);
        if (twin) { tail(1, false, !rearOk, on); tail(3, false, !rearOk, on); }
    }
    if (!glow.empty()) r.DrawCoronas(glow.data(), (int)glow.size());
}
