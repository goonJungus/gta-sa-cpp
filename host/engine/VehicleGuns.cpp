#include "VehicleGuns.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace VehicleGuns {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;
float s_lastBigGunMs = -1e9f;   // CPlayerInfo::m_nLastTimeBigGunFired
}  // namespace

Kind KindOf(int model) {
    if (model == kRhino) return Kind::Tank;
    if (model == kFireTruck || model == kSwatVan) return Kind::WaterCannon;
    return Kind::None;
}

void ResetClock() { s_lastBigGunMs = -1e9f; }

namespace {
// GetMatrix().InverseTransformVector of the camera front: world -> car space (heading 0 faces +y, positive turns left).
void ToCar(const VehicleInstance& c, const float f[3], float out[3]) {
    const float cs = std::cos(c.heading), sn = std::sin(c.heading);
    out[0] = f[0] * cs + f[1] * sn;
    out[1] = -f[0] * sn + f[1] * cs;
    out[2] = f[2];
}
// Car space direction -> world.
void ToWorld(const VehicleInstance& c, const float f[3], float out[3]) {
    const float cs = std::cos(c.heading), sn = std::sin(c.heading);
    out[0] = f[0] * cs - f[1] * sn;
    out[1] = f[0] * sn + f[1] * cs;
    out[2] = f[2];
}
}  // namespace

void AimTank(VehicleInstance& c, const float camFront[3], float dt) {
    const float ts = dt * 50.0f;   // CTimer::GetTimeStep
    float fd[3];
    ToCar(c, camFront, fd);
    float wantV = std::atan2(-fd[0], fd[1]);
    const float wantH = std::atan2(fd[2], std::sqrt(fd[0] * fd[0] + fd[1] * fd[1])) + 15.0f * kDeg;
    if (wantV < c.doomV - kPi) wantV += 2.0f * kPi;
    else if (wantV > c.doomV + kPi) wantV -= 2.0f * kPi;
    float step = ts * 0.015f;
    const float dv = wantV - c.doomV;
    if (dv < -step) c.doomV -= step;
    else if (dv > step) c.doomV += step;
    step = ts * 0.005f;
    const float dh = wantH - c.doomH;
    if (dh > step) c.doomH += step;
    else if (dh >= -step) c.doomH = wantH;
    else c.doomH -= step;

    if (c.doomV > kPi) c.doomV -= 2.0f * kPi;
    else if (c.doomV < -kPi) c.doomV += 2.0f * kPi;

    const float hMax = 35.0f * kDeg, hMin = -7.0f * kDeg;
    if (c.doomH > hMax) {
        c.doomH = hMax;
    } else if (c.doomV > 90.0f * kDeg || c.doomV < -90.0f * kDeg) {   // the barrel must clear the hull when it points backwards
        const float cv = std::cos(c.doomV) * 1.3f;
        float h = -3.0f * kDeg;
        if (cv >= -1.0f) h = hMin - cv * (h - hMin);
        if (h > c.doomH) c.doomH = h;
    } else if (c.doomH < hMin) {
        c.doomH = hMin;
    }
}

void AimWater(VehicleInstance& c, const float camFront[3], float dt) {
    const float ts = dt * 50.0f;
    const bool swat = c.model == kSwatVan;
    float fd[3];
    ToCar(c, camFront, fd);
    float wantV = std::atan2(-fd[0], fd[1]);
    float wantH = std::atan2(fd[2], std::sqrt(fd[0] * fd[0] + fd[1] * fd[1])) + (swat ? 22.0f : 15.0f) * kDeg;
    if (wantV > c.doomV + kPi) wantV -= 2.0f * kPi;
    else if (wantV < c.doomV - kPi) wantV += 2.0f * kPi;
    float step = ts / 20.0f;
    const float dv = wantV - c.doomV;
    if (dv > step) c.doomV += step;
    else if (dv < -step) c.doomV -= step;
    step = dt;   // GetTimeStepInSeconds
    const float dh = wantH - c.doomH;
    if (dh > step) c.doomH += step;
    else if (dh < -step) c.doomH -= step;
    if (c.doomV > kPi) c.doomV -= 2.0f * kPi;
    else if (c.doomV < -kPi) c.doomV += 2.0f * kPi;
    if (swat) c.doomH = std::clamp(c.doomH, -10.0f * kDeg, 35.0f * kDeg);
    else c.doomH = std::clamp(c.doomH, -20.0f * kDeg, 20.0f * kDeg);
}

bool FireTank(Vehicles& v, size_t car, float nowMs, TankShot* shot) {
    if (car >= v.Count() || nowMs <= s_lastBigGunMs + 800.0f) return false;
    VehicleInstance& c = v.Car(car);
    s_lastBigGunMs = nowMs;
    const float local[3] = {std::sin(-c.doomV), std::cos(c.doomV), std::sin(c.doomH)};
    float dir[3];
    ToWorld(c, local, dir);
    float origin[3];
    if (!v.NodeWorldPos(car, 2, origin)) {   // no misc_c: TANK_SHOT_DOOM_POS + the turned default target offset
        const float sv = std::sin(c.doomV), cv = std::cos(c.doomV);
        const float pos[3] = {0.0f, -1.394f, 2.296f}, tgt[3] = {0.0f, 2.95f, 2.97f};
        const float off[3] = {pos[0] + cv * tgt[0] - sv * tgt[1], pos[1] + cv * tgt[1] + sv * tgt[0], pos[2] + tgt[2] - 1.0f};
        float w[3];
        ToWorld(c, off, w);
        origin[0] = c.x + w[0]; origin[1] = c.y + w[1]; origin[2] = c.z + w[2];
    } else {   // the muzzle moves with the car: GetSpeed(muzzle - centre) * timeStep
        origin[0] += c.vx * 0.02f; origin[1] += c.vy * 0.02f;
    }
    // ApplyForce(point * -0.1 * mass): the speed drops by 0.1 m per 20 ms step along the barrel.
    c.vx += dir[0] * -0.1f * 50.0f;
    c.vy += dir[1] * -0.1f * 50.0f;
    c.speed = c.vx * -std::sin(c.heading) + c.vy * std::cos(c.heading);
    if (shot) {
        for (int i = 0; i < 3; i++) { shot->origin[i] = origin[i]; shot->dir[i] = dir[i]; shot->end[i] = origin[i] + dir[i] * 60.0f; }
    }
    return true;
}

void FireWater(Vehicles& v, size_t car, WaterCannons& wc, float nowMs, float dt) {
    if (car >= v.Count()) return;
    VehicleInstance& c = v.Car(car);
    const bool swat = c.model == kSwatVan;
    const float cv = std::cos(c.doomV), sv = std::sin(c.doomV), ch = std::cos(c.doomH), sh = std::sin(c.doomH);
    const float local[3] = {-(sv * ch), cv * ch, sh};
    float dir[3];
    ToWorld(c, local, dir);
    float pos[3];
    const float step[3] = {c.vx / 50.0f, c.vy / 50.0f, 0.0f};   // metres per 20 ms step
    if (v.NodeWorldPos(car, 0, pos)) {
        const float len = swat ? 2.0f : 1.2f;
        for (int i = 0; i < 3; i++) pos[i] += dir[i] * len;
        pos[0] += step[0] * dt * 50.0f; pos[1] += step[1] * dt * 50.0f;   // GetSpeed(offset) * timeStep
    } else {
        const float off[3] = {0.0f, 1.5f, 1.8f};
        float w[3];
        ToWorld(c, off, w);
        pos[0] = c.x + w[0]; pos[1] = c.y + w[1]; pos[2] = c.z + w[2];
    }
    dir[2] += (float)(std::rand() % 16) / 1000.0f;
    const float k = swat ? 0.4f : 0.5f;
    const float end[3] = {step[0] + dir[0] * k, step[1] + dir[1] * k, step[2] * 0.3f + dir[2] * k};
    wc.UpdateOne((uint32_t)car + 1u, pos, end, nowMs);
}

}  // namespace VehicleGuns
