// CJ's parachute: CTaskSimpleParachute. In the air with the parachute held, a press of fire plays
// PARA_open and floats under the canopy (PARA_float, PARA_steerL / PARA_steerR, PARA_decel); the
// landing plays PARA_Land (PARA_Land_Water in the sea), see PlayerPed::Land. See PlayerPed.h.

#define NOMINMAX
#include "PlayerPed.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kGlideSpeed = 9.0f;    // m/s forward under the canopy
constexpr float kDecelSpeed = 3.0f;    // m/s with the brake pulled
constexpr float kTurnRate = 1.4f;      // rad/s of steering
constexpr float kOpenHeight = 4.0f;    // lowest height the canopy will still open
}  // namespace

// Returns true when the parachute owns this frame's in-air animation and velocity.
bool PlayerPed::UpdateParachute(const Input& in) {
    const bool held = m_melee >= 0 && m_guns[m_melee].type == 46 && m_gun < 0;
    if (!held) {
        if (m_para) { m_para = 0; m_paraClip = nullptr; }
        return false;
    }
    if (m_para == 0) {
        if (!in.fireJust || in.heightAboveGround < kOpenHeight) return false;
        const AnimClip* c = Clip("para_open");
        if (!c) return false;
        Blend(c, false, false, 8.0f);
        m_paraClip = c;
        m_para = 1;
    }
    if (m_para == 1) {
        Assoc* a = Get(m_paraClip);
        if (!a || a->dead || a->finished) { if (a) a->delta = -4.0f; m_para = 2; }
        else { m_velX = m_airVelX; m_velY = m_airVelY; return true; }
    }
    // under the canopy: the stick steers towards where it points, back on it brakes
    const float mag = std::sqrt(in.stickX * in.stickX + in.stickY * in.stickY);
    float turn = 0.0f;
    bool brake = false;
    if (mag > 0.1f) {
        const float want = std::atan2(in.stickY, in.stickX);
        float d = want - m_heading;
        while (d > kPi) d -= 2.0f * kPi;
        while (d < -kPi) d += 2.0f * kPi;
        brake = std::fabs(d) > 2.4f;
        if (!brake) turn = std::clamp(d, -kTurnRate * in.dt, kTurnRate * in.dt);
        m_heading += turn;
        m_aimHeading = m_heading;
    }
    const float speed = brake ? kDecelSpeed : kGlideSpeed;
    m_airVelX += (std::cos(m_heading) * speed - m_airVelX) * std::min(1.0f, in.dt * 2.0f);
    m_airVelY += (std::sin(m_heading) * speed - m_airVelY) * std::min(1.0f, in.dt * 2.0f);
    m_velX = m_airVelX;
    m_velY = m_airVelY;
    const char* name = brake ? "para_decel" : turn > 0.002f ? "para_steerl" : turn < -0.002f ? "para_steerr" : "para_float";
    const AnimClip* want = Clip(name);
    if (!want) want = Clip("para_float");
    if (want && !Active(want)) {
        for (const char* n : {"para_float", "para_steerl", "para_steerr", "para_decel"})
            if (const AnimClip* o = Clip(n)) if (o != want) if (Assoc* oa = Get(o)) oa->delta = -4.0f;
        Blend(want, true, false, 4.0f);
    }
    return true;
}
