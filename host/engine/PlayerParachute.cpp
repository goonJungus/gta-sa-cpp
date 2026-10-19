// CJ's parachute, ported from the streamed script player_parachute.scm (script.img). Falling fast
// (velZ < -10, over 20 m up) with the parachute carried starts the skydive: the stick steers the
// heading and the drift, fire pulls the cord (PARA_open), then the canopy floats and steers
// (PARA_float, PARA_steerL / PARA_steerR, PARA_decel); the landing plays PARA_Land (PARA_Land_Water
// in the sea) or, from a hard skydive without a canopy, FALL_skyDive_DIE. See PlayerPed::Land.
// The script ticks about 30 times a second: its per-tick factors are rescaled with dt.

#define NOMINMAX
#include "PlayerPed.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kTick = 30.0f;          // script ticks per second
constexpr float kPadToStick = 4.267f;   // script divides the pad (-128..127) by this
constexpr float kDeg = kPi / 180.0f;
constexpr float kMaxFall = -30.0f;      // g6016: lowest skydive vertical speed
constexpr float kSkySpeed = 32.0f;      // g6020
constexpr float kCanopySpeed = 5.0f;    // horizontal m/s under the canopy
constexpr float kOpenHide = 0.5f;       // PARA_open: canopy hidden, then grows over the next 0.5 s
constexpr float kOpenSteer = 1.0f;      // steering active from here
constexpr float kOpenBlend = 2.3333f;   // PARA_OPEN_O: length used to blend velocity
float TickLerp(float k, float dt) { return 1.0f - std::pow(1.0f - k, dt * kTick); }
}  // namespace

int PlayerPed::ParaGun() const {
    for (int i = 0; i < (int)m_guns.size(); i++)
        if (m_guns[i].type == 46) return i;
    return -1;
}

bool PlayerPed::ParaVelZ(float& v) const {
    if (m_para) { v = m_paraVz; return true; }
    if (m_sky) { v = std::max(v, kMaxFall); return true; }
    return false;
}

// Returns true when the parachute owns this frame's in-air animation and velocity.
bool PlayerPed::UpdateParachute(const Input& in) {
    bool held = m_melee >= 0 && m_guns[m_melee].type == 46 && m_gun < 0;
    if (in.noclip) held = false;
    if (!held && !m_sky && !m_para) {   // auto start: the script selects the parachute once the fall is long
        const int pg = ParaGun();
        if (pg < 0 || m_guns[pg].total <= 0 || in.noclip || in.velZ >= -10.0f || in.heightAboveGround <= 20.0f) {
            m_paraHelpDone = false;
            return false;
        }
        SelectGun(pg);
        held = true;
    }
    if (!held) {   // switched away mid-air
        m_sky = 0;
        if (m_para) { m_para = 0; m_paraClip = nullptr; }
        return false;
    }
    if (!m_sky && !m_para) {
        if (in.velZ >= -10.0f || in.heightAboveGround <= 20.0f) return false;
        m_sky = 1;
        m_pTurn = m_pPitch = 0.0f;
        m_paraClip = nullptr;
        m_paraHelpDone = false;
    }
    // the pad as the script reads it: X right positive, Y down positive, -128..127
    const float sl = std::sqrt(in.stickX * in.stickX + in.stickY * in.stickY);
    float padX = 0.0f, padY = 0.0f;
    if (sl > 0.001f) {
        const float fx = std::cos(in.camYaw), fy = std::sin(in.camYaw);
        padX = -(in.stickX * -fy + in.stickY * fx) * std::min(1.0f, sl) * 127.0f;
        padY = -(in.stickX * fx + in.stickY * fy) * std::min(1.0f, sl) * 127.0f;
    }
    const float ticks = in.dt * kTick;
    const float sm = TickLerp(0.05f, in.dt);   // l += (pad / 4.267 - l) / 20
    float tx = padX / kPadToStick, ty = padY / kPadToStick;
    if (m_para == 1 && m_paraT < kOpenSteer) { tx = ty = 0.0f; }   // the pull ignores the stick

    if (m_sky && !m_para) {
        m_pTurn += (tx - m_pTurn) * sm;
        m_pPitch += (ty - m_pPitch) * sm;
        m_heading -= (m_pTurn / 5.0f) * kDeg * ticks;
        m_aimHeading = m_heading;
        const float sp = -(m_pPitch / 30.0f) * kSkySpeed;   // forward when the stick is pushed up
        const float a = TickLerp(0.01f, in.dt);
        m_airVelX += (std::cos(m_heading) * sp - m_airVelX) * a;
        m_airVelY += (std::sin(m_heading) * sp - m_airVelY) * a;
        // help text while the ground is 60..100 m away
        if (in.heightAboveGround > 60.0f && in.heightAboveGround < 100.0f && !m_paraHelpDone) {
            m_paraHelp = true;
            m_paraHelpDone = true;
        }
        const char* name = "para/fall_skydive";
        if (std::fabs(padX) > 40.0f || std::fabs(padY) > 40.0f) {
            if (std::fabs(padX) > std::fabs(padY)) name = padX >= 0.0f ? "para/fall_skydive_r" : "para/fall_skydive_l";
            else if (padY < 0.0f) name = "para/fall_skydive";
            else name = "para/fall_skydive_accel";
        }
        const AnimClip* want = Clip(name);
        if (!want) want = Clip("fall_skydive");
        if (want && want != m_paraClip) {
            m_paraClip = want;
            Blend(want, true, false, 4.0f);
        }
        if (in.fireJust && Clip("para_open")) {   // button 17: pull the cord
            const AnimClip* c = Clip("para_open");
            Blend(c, false, false, 8.0f);
            m_paraClip = c;
            m_para = 1;
            m_paraT = 0.0f;
            m_paraVz0 = in.velZ;
            m_paraVx0 = m_airVelX;
            m_paraVy0 = m_airVelY;
            m_paraVz = in.velZ;
        }
        m_velX = m_airVelX;
        m_velY = m_airVelY;
        return true;
    }

    // ---- the canopy ----
    m_paraT += in.dt;
    if (m_para == 1) {
        const Assoc* a = Get(m_paraClip);
        if (!a || a->dead || a->finished) {
            if (a) { Assoc* w = Get(m_paraClip); if (w) w->delta = -4.0f; }
            m_para = 2;
            m_paraClip = nullptr;
        }
    }
    m_pTurn += (tx - m_pTurn) * sm;
    m_pPitch += (ty - m_pPitch) * sm;
    if (m_paraT >= kOpenSteer) {
        m_heading -= (m_pTurn / 15.0f) * kDeg * ticks;
        m_aimHeading = m_heading;
    }
    const float cx = std::cos(m_heading) * kCanopySpeed, cy = std::sin(m_heading) * kCanopySpeed;
    const bool brake = m_paraT >= kOpenSteer && padY > 40.0f && std::fabs(padY) >= std::fabs(padX);
    const float vzT = brake ? -1.5f : -5.0f;
    if (m_paraT < kOpenBlend + kOpenHide) {   // velocity follows the opening progress
        const float p = std::clamp((m_paraT - kOpenHide) / kOpenBlend, 0.0f, 1.0f);
        m_airVelX = m_paraVx0 + (cx - m_paraVx0) * p;
        m_airVelY = m_paraVy0 + (cy - m_paraVy0) * p;
        m_paraVz = m_paraVz0 + (vzT - m_paraVz0) * p;
    } else {
        m_airVelX = cx;
        m_airVelY = cy;
        m_paraVz += (vzT - m_paraVz) * TickLerp(0.05f, in.dt);   // moves 1/20 per tick
    }
    m_velX = m_airVelX;
    m_velY = m_airVelY;
    if (m_para == 1) return true;   // PARA_open still plays

    // l15: steerR 2, steerL 3, decel 4, float 5 (strong forward), 1 (small stick)
    const char* name = "para_float";
    if (std::fabs(padX) > 40.0f || std::fabs(padY) > 40.0f) {
        if (std::fabs(padX) > std::fabs(padY)) name = padX >= 0.0f ? "para_steerr" : "para_steerl";
        else if (padY > 0.0f) name = "para_decel";
    }
    const AnimClip* want = Clip(name);
    if (!want) want = Clip("para_float");
    if (want && want != m_paraClip) {
        for (const char* n : {"para_float", "para_steerl", "para_steerr", "para_decel"})
            if (const AnimClip* o = Clip(n)) if (o != want) if (Assoc* oa = Get(o)) oa->delta = -4.0f;
        m_paraClip = want;
        Blend(want, true, false, 4.0f);
    }
    return true;
}
