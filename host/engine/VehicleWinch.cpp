// Winches: the hook, rope and magnet under a helicopter (CVehicle::InitWinch / UpdateWinch, CRope reduced to a pendulum).
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr float kGravity = 9.8f;
}

void Vehicles::AttachWinch(size_t i, int type) {
    if (i >= m_cars.size()) return;
    VehicleInstance::Winch& w = m_cars[i].winch;
    w.on = true; w.type = std::clamp(type, 1, 3); w.fresh = true; w.carried = -1;
}

void Vehicles::RemoveWinch(size_t i) {
    if (i >= m_cars.size()) return;
    WinchRelease(i);
    m_cars[i].winch.on = false;
}

bool Vehicles::WinchPick(size_t i, size_t car) {
    if (i >= m_cars.size() || car >= m_cars.size() || i == car) return false;
    VehicleInstance::Winch& w = m_cars[i].winch;
    VehicleInstance& t = m_cars[car];
    if (!w.on || w.carried >= 0 || t.removed || !w.canPickVehicles) return false;
    if (UsesHeliPhysics(t.model) || UsesPlanePhysics(t.model) || UsesBoatPhysics(t.model)) return false;
    if (std::getenv("GTA_HELIDBG")) { std::fprintf(stderr, "[WINCH] heli %zu picks car %zu", i, car); std::fputc(10, stderr); }
    w.carried = (int)car;
    t.phys.active = false;   // carried: it follows the hook, not its own physics
    t.vx = t.vy = 0.0f; t.speed = 0.0f;
    return true;
}

void Vehicles::WinchRelease(size_t i) {
    if (i >= m_cars.size()) return;
    VehicleInstance::Winch& w = m_cars[i].winch;
    if (w.carried < 0 || (size_t)w.carried >= m_cars.size()) { w.carried = -1; return; }
    VehicleInstance& t = m_cars[(size_t)w.carried];
    t.vx = w.v[0]; t.vy = w.v[1];
    Activate(t);   // back to the full simulation, falling from where it hangs
    t.phys.mv[2] = w.v[2] / 50.0f;
    w.carried = -1;
}

void Vehicles::UpdateWinches(float dt, const std::function<float(float, float)>& groundAt) {
    dt = std::min(dt, 0.05f);
    for (size_t i = 0; i < m_cars.size(); i++) {
        VehicleInstance& c = m_cars[i];
        VehicleInstance::Winch& w = c.winch;
        if (!w.on) continue;
        if (c.removed) { w.on = false; w.carried = -1; continue; }
        const float up[3] = {c.phys.active ? c.rot[6] : 0.0f, c.phys.active ? c.rot[7] : 0.0f, c.phys.active ? c.rot[8] : 1.0f};
        const float a[3] = {c.x - up[0] * 1.2f, c.y - up[1] * 1.2f, c.z - up[2] * 1.2f};   // the belly
        if (w.fresh) {
            w.h[0] = a[0]; w.h[1] = a[1]; w.h[2] = a[2] - 0.5f; w.v[0] = c.vx; w.v[1] = c.vy; w.v[2] = 0.0f;
            w.fresh = false;
        }
        if (std::getenv("GTA_HELIDBG")) { static int n = 0; if (n++ % 120 == 0) { std::fprintf(stderr, "[WINCH] heli %zu at %.1f %.1f %.1f hook %.1f %.1f %.1f carried %d", i, c.x, c.y, c.z, w.h[0], w.h[1], w.h[2], w.carried); std::fputc(10, stderr); } }
        const bool loaded = w.carried >= 0;
        const float damp = std::pow(loaded ? 0.35f : 0.12f, dt);   // a loaded hook swings less
        w.v[2] -= kGravity * dt;
        for (int k = 0; k < 3; k++) { w.v[k] *= damp; w.h[k] += w.v[k] * dt; }
        float dx = w.h[0] - a[0], dy = w.h[1] - a[1], dz = w.h[2] - a[2];
        const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d > w.len) {   // the rope is taut: back onto the sphere, and its outward speed goes
            const float s = w.len / d;
            w.h[0] = a[0] + dx * s; w.h[1] = a[1] + dy * s; w.h[2] = a[2] + dz * s;
            dx = (w.h[0] - a[0]) / w.len; dy = (w.h[1] - a[1]) / w.len; dz = (w.h[2] - a[2]) / w.len;
            const float out = w.v[0] * dx + w.v[1] * dy + w.v[2] * dz;
            if (out > 0.0f) { w.v[0] -= out * dx; w.v[1] -= out * dy; w.v[2] -= out * dz; }
            // the heli drags the hook along: its own speed pulls a taut rope
            w.v[0] += (c.vx - w.v[0]) * std::min(1.0f, 2.0f * dt);
            w.v[1] += (c.vy - w.v[1]) * std::min(1.0f, 2.0f * dt);
        }
        if (groundAt && !loaded) {   // the hook rests on the ground
            const float g = groundAt(w.h[0], w.h[1]);
            if (g > -1e5f && w.h[2] < g + 0.4f) { w.h[2] = g + 0.4f; if (w.v[2] < 0.0f) w.v[2] = 0.0f; w.v[0] *= 0.9f; w.v[1] *= 0.9f; }
        }
        if (loaded) {   // hangs the car under the hook, nose along the heli's
            VehicleInstance& t = m_cars[(size_t)w.carried];
            t.x = w.h[0]; t.y = w.h[1]; t.z = w.h[2] - 1.0f;
            t.heading = c.heading; t.vx = t.vy = 0.0f; t.speed = 0.0f;
            if (c.phys.inHornJust) WinchRelease(i);   // the horn lets go
        } else if (w.type == 3 && w.canPickVehicles && std::fabs(w.v[0]) < 3.0f && std::fabs(w.v[1]) < 3.0f) {   // the magnet takes the car beneath it
            for (size_t j = 0; j < m_cars.size(); j++) {
                if (j == i) continue;
                const VehicleInstance& t = m_cars[j];
                if (t.removed) continue;
                const float hx = t.x - w.h[0], hy = t.y - w.h[1];
                if (hx * hx + hy * hy < 2.6f * 2.6f && w.h[2] - t.z > 0.2f && w.h[2] - t.z < 2.6f && WinchPick(i, j)) break;
            }
        }
    }
}
