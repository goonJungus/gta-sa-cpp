#include "VehicleGuns.h"

#include "WeaponInfo.h"

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

// ---- CVehicle::ProcessWeapons ----
namespace {
enum CarWeapon { kNotUsed = 0, kHeavyGun = 1, kFreefallBomb = 2, kLockOnRocket = 3, kDoubleRocket = 4 };

struct Frame {   // the vehicle's CMatrix (right, forward, up, position) and m_vecMoveSpeed per 20 ms step
    float r[3], f[3], u[3], p[3], mv[3];
    void Point(const float l[3], float out[3]) const {
        for (int k = 0; k < 3; k++) out[k] = p[k] + r[k] * l[0] + f[k] * l[1] + u[k] * l[2];
    }
};
Frame FrameOf(Vehicles& v, size_t car) {
    float m[16];
    v.BodyMatrix(car, m);
    const VehicleInstance& c = v.Car(car);
    Frame fr;
    for (int k = 0; k < 3; k++) { fr.r[k] = m[k]; fr.f[k] = m[4 + k]; fr.u[k] = m[8 + k]; fr.p[k] = m[12 + k]; }
    if (c.phys.active) for (int k = 0; k < 3; k++) fr.mv[k] = c.phys.mv[k];
    else { fr.mv[0] = c.vx / 50.0f; fr.mv[1] = c.vy / 50.0f; fr.mv[2] = 0.0f; }
    return fr;
}
float Dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
float Len(const float a[3]) { return std::sqrt(Dot(a, a)); }
// CCollision::DistToLine: distance from p to the segment a -> b
float DistToLine(const float a[3], const float b[3], const float p[3]) {
    const float ab[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, ap[3] = {p[0] - a[0], p[1] - a[1], p[2] - a[2]};
    const float t = Dot(ap, ab), l2 = Dot(ab, ab);
    if (t <= 0.0f) return Len(ap);
    if (t >= l2) { const float bp[3] = {p[0] - b[0], p[1] - b[1], p[2] - b[2]}; return Len(bp); }
    return std::sqrt(std::max(0.0f, Dot(ap, ap) - t * t / l2));
}
float Rand01() { return (float)(std::rand() & 0x7fff) * 3.051851e-05f; }   // CGeneral::GetRandomNumber() / 32767

int NumGuns(int m) {   // GetPlaneNumGuns
    switch (m) {
    case kHunter: case kSeaspar: case kRCBaron: case kMaverick: case kPolmav: case kCargobob: return 1;
    case kRustler: return 6;
    case kHydra: return 2;
    default: return 0;
    }
}
uint32_t GunsRate(int m) {   // GetPlaneGunsRateOfFire (firing multiplier 1)
    switch (m) {
    case kSeaspar: case kRCBaron: return 40;
    case kRustler: return 80;
    case kHydra: return 17;
    case kCargobob: return 100;
    default: return 60;
    }
}
uint32_t OrdnanceRate(int m, int type) {   // GetPlaneOrdnanceRateOfFire
    switch (m) {
    case kHunter: return 500;
    case kSeaspar: case kRustler: return 1000;
    case kHydra: return type != 1 ? 1000 : 500;
    default: return 350;
    }
}
float AutoAimAngle(int m) {   // GetPlaneGunsAutoAimAngle, degrees
    switch (m) {
    case kSeaspar: return 10.0f;
    case kRustler: case kMaverick: case kPolmav: case kHydra: case kCargobob: return 15.0f;
    case 564: return 20.0f;   // RC Tiger
    case kHunter: return 25.0f;
    case kRCBaron: return 30.0f;
    default: return 0.0f;
    }
}
void GunsPosition(int m, int gun, float out[3]) {   // GetPlaneGunsPosition (the models carry no gun dummy)
    float p[3] = {0, 0, 0};
    switch (m) {
    case kHunter: p[1] = 4.41f; p[2] = -1.27f; break;   // VehicleGunOffset[0]
    case kSeaspar: p[0] = -0.5f; p[1] = 2.4f; p[2] = -0.785f; break;
    case kRCBaron: p[1] = 0.45f; break;
    case kRustler: p[0] = 2.19f + 0.2f * (float)(gun - 1); p[1] = 1.5f; p[2] = -0.58f; break;
    case kMaverick: case kPolmav: p[1] = 2.85f; p[2] = -0.5f; break;
    case kHydra: p[0] = 1.48f; p[1] = 0.44f; p[2] = -0.52f; break;
    case kCargobob: p[1] = 6.87f; p[2] = -1.65f; break;
    }
    for (int k = 0; k < 3; k++) out[k] = p[k];
}
void OrdnancePosition(int m, int type, float out[3]) {   // GetPlaneOrdnancePosition
    float p[3] = {0, 0, 0};
    if (m == kHunter) { p[0] = 2.17f; p[1] = 1.0f; p[2] = -0.8f; }
    else if (m == kRustler) { p[0] = 2.19f; p[1] = 1.5f; p[2] = -0.58f; }
    else if (m == kHydra && type == 1) { p[0] = 3.7f; p[1] = 0.98f; p[2] = -1.02f; }
    else if (m == kHydra && type == 2) { p[0] = 3.92f; p[1] = 0.98f; p[2] = -1.02f; }
    for (int k = 0; k < 3; k++) out[k] = p[k];
}

// CWeapon::FireInstantHit from a plane or helicopter: along the nose, DoDriveByAutoAiming onto the ped or car
// nearest the line (relative to its distance) inside the auto-aim cone, then a random wobble.
void InstantHit(int model, const Frame& fr, const float start[3], float range, const AirWorld& w, AirFire::Shot& shot) {
    float end[3];
    for (int k = 0; k < 3; k++) end[k] = start[k] + fr.f[k] * range;
    const float se[3] = {end[0] - start[0], end[1] - start[1], end[2] - start[2]};
    float best = 10000.0f;
    const float* target = nullptr;
    for (const auto& pt : w.aimPoints) {
        const float d[3] = {pt[0] - start[0], pt[1] - start[1], pt[2] - start[2]};
        if (Len(d) > range) continue;   // FindObjectsInRange
        const float off[3] = {pt[0] - fr.p[0], pt[1] - fr.p[1], pt[2] - fr.p[2]};
        const float score = DistToLine(start, end, pt.data()) / std::max(5.0f, Len(off));
        if (Dot(se, d) > 0.0f && score < best) { best = score; target = pt.data(); }
    }
    const float ang = AutoAimAngle(model);
    const float lim = ang <= 0.5f ? 2.5f : std::tan(ang * kDeg);
    if (target && best < lim) {   // the end moves onto the target, the range kept
        const float d[3] = {target[0] - start[0], target[1] - start[1], target[2] - start[2]};
        const float l = std::max(1e-4f, Len(d));
        for (int k = 0; k < 3; k++) end[k] = start[k] + d[k] / l * range;
    }
    float dir[3] = {end[0] - start[0], end[1] - start[1], end[2] - start[2]};
    float l = std::max(1e-4f, Len(dir));
    for (float& x : dir) x /= l;
    const float spread = (model == kSeaspar || model == 469 || model == 564) ? 0.1f : 0.3f;
    const float rz = Rand01(), ry = Rand01(), rx = Rand01();
    dir[0] += (rx * 0.4f - 0.2f) * spread;
    dir[1] += (ry * 0.4f - 0.2f) * spread;
    dir[2] += (rz * 0.2f - 0.1f) * spread;
    l = std::max(1e-4f, Len(dir));
    for (int k = 0; k < 3; k++) { shot.origin[k] = start[k]; shot.end[k] = start[k] + dir[k] / l * range; }
}

void FirePlaneGuns(Vehicles& v, size_t car, uint32_t now, float ts, const AirWorld& w, AirFire& out) {
    VehicleInstance& c = v.Car(car);
    VehicleInstance::AirGuns& g = c.air;
    if (!NumGuns(c.model) || now <= g.gunMs + GunsRate(c.model)) return;
    bool minigun = false, twin = false;
    switch (c.model) {
    case kHunter: case kCargobob: minigun = true; break;
    case kSeaspar: case kRCBaron: case kMaverick: case kPolmav: break;
    case kRustler: g.cycle = g.cycle + 1 == 3 ? 0 : g.cycle + 1; twin = true; break;
    case kHydra: minigun = true; twin = true; break;
    default: return;
    }
    const WeaponInfo* wi = WeaponTable::Get(minigun ? "MINIGUN" : "M4", 1);
    const float range = wi && wi->weaponRange > 0.0f ? wi->weaponRange : (minigun ? 75.0f : 90.0f);
    const Frame fr = FrameOf(v, car);
    float gp[3], shell[3];
    GunsPosition(c.model, g.cycle, gp);
    fr.Point(gp, shell);
    for (int k = 0; k < 3; k++) shell[k] += fr.mv[k] * ts;
    for (int i = 0; i < (twin ? 2 : 1); i++) {   // a twin gun fires both barrels from the one position
        AirFire::Shot s;
        s.minigun = minigun;
        InstantHit(c.model, fr, shell, range, w, s);
        out.shots.push_back(s);
    }
    out.shake = true;
    g.gunMs = now;
}

// CWeapon::FireProjectile(WEAPON_RLAUNCHER) from a vehicle: a metre ahead of the point, along the nose at
// 0.4 per step (0.2 for a heat seeker) plus the vehicle's speed (CProjectileInfo::AddProjectile).
void AddRocket(const Frame& fr, const float at[3], bool homing, int target, AirFire& out) {
    AirFire::Missile m;
    const float sp = (homing ? 0.2f : 0.4f) + Len(fr.mv);
    for (int k = 0; k < 3; k++) {
        m.pos[k] = at[k] + (homing ? 0.0f : fr.f[k]);
        m.vel[k] = fr.f[k] * sp * 50.0f;
    }
    m.homing = homing;
    m.target = target;
    out.missiles.push_back(m);
}

void FireUnguidedMissile(Vehicles& v, size_t car, int type, uint32_t now, float ts, AirFire& out) {
    VehicleInstance& c = v.Car(car);
    uint32_t& t = c.air.ordMs[type == 1 ? 0 : 1];
    if (now <= t + OrdnanceRate(c.model, type)) return;
    if (c.model != kHunter && c.model != kHydra) return;
    const Frame fr = FrameOf(v, car);
    const float ahead = std::max(0.0f, Dot(fr.f, fr.mv)) * ts;
    float op[3];
    OrdnancePosition(c.model, type, op);
    for (int side = 0; side < 2; side++) {   // one from each wing
        const float l[3] = {side ? -op[0] : op[0], op[1], op[2]};
        float at[3];
        fr.Point(l, at);
        for (int k = 0; k < 3; k++) at[k] += fr.f[k] * ahead;
        AddRocket(fr, at, false, -1, out);
    }
    out.shake = true;
    t = now;
}

// FireHeatSeakingMissile: one missile, the wings taking turns. The ordnance offset is added to the position
// unrotated, as retail does.
void FireHeatSeekingMissile(Vehicles& v, size_t car, int target, int type, uint32_t now, float ts, AirFire& out) {
    VehicleInstance& c = v.Car(car);
    uint32_t& t = c.air.ordMs[type == 1 ? 0 : 1];
    if (now <= t + OrdnanceRate(c.model, type)) return;
    const Frame fr = FrameOf(v, car);
    float op[3];
    OrdnancePosition(c.model, type, op);
    if (c.air.mirror) op[0] = -op[0];
    c.air.mirror = !c.air.mirror;
    const float ahead = std::max(0.0f, Dot(fr.mv, fr.f)) * ts;
    float at[3];
    for (int k = 0; k < 3; k++) at[k] = fr.p[k] + op[k] + fr.f[k] * ahead;
    AddRocket(fr, at, true, target, out);
    out.shake = true;
    t = now;
}

// CWeapon::PickTargetForHeatSeekingMissile + EvaluateTargetForHeatSeekingMissile: the car that rates lowest
// (near the line 250 m out from 5 m ahead, over its distance); aircraft and the existing target at a quarter.
int PickTarget(Vehicles& v, size_t self, const Frame& fr, int preferred) {
    const float o[3] = {fr.p[0] + fr.f[0] * 5.0f, fr.p[1] + fr.f[1] * 5.0f, fr.p[2] + fr.f[2] * 5.0f};
    const float e[3] = {o[0] + fr.f[0] * 250.0f, o[1] + fr.f[1] * 250.0f, o[2] + fr.f[2] * 250.0f};
    float best = 3.4e38f;
    int pick = -1;
    for (size_t i = 0; i < v.Count(); i++) {
        const VehicleInstance& c = v.Car(i);
        if (i == self || c.removed || c.wrecked || c.health <= 0.0f) continue;
        const float p[3] = {c.x, c.y, c.z};
        const float d[3] = {o[0] - p[0], o[1] - p[1], o[2] - p[2]};
        const float dist = Len(d), line = DistToLine(o, e, p);
        if (line * 1.2f >= dist) continue;
        float r = std::sqrt(dist) / 10.0f + line / dist;
        if (v.IsAircraft(c.model)) r *= 0.25f;
        if ((int)i == preferred) r *= 0.25f;
        if (r <= best) { best = r; pick = (int)i; }
    }
    return pick;
}
// ScanAndMarkTargetForHeatSeekingMissile: the pick if the line to it is clear; the crosshair marks it.
int ScanAndMark(Vehicles& v, size_t self, const Frame& fr, int preferred, const AirWorld& w, AirFire& out) {
    const int t = PickTarget(v, self, fr, preferred);
    if (t < 0) return -1;
    const VehicleInstance& c = v.Car((size_t)t);
    const float p[3] = {c.x, c.y, c.z};
    if (w.clear && !w.clear(fr.p, p)) return -1;
    out.markCar = t;
    return t;
}
}  // namespace

// CVehicle::DoFixedMachineGuns / FireFixedMachineGuns: the Predator's two guns 2 m either side of its bow fire a round
// each every 150 ms at a point 60 m ahead on its heading (scattered 1.92 m sideways, 2.56 m up or down), 15 damage a
// round; after 20 the guns rest 1.4 s, and letting go 1.4 s refills the clip. (DoTankDoomAiming has no target here.)
void FixedMachineGuns(Vehicles& v, size_t car, bool fired, uint32_t now, AirFire& out) {
    VehicleInstance::AirGuns& g = v.Car(car).air;
    if (!fired) {
        if (now > g.gunMs + 1400u) g.clip = 20;
        return;
    }
    if (now <= g.gunMs + 150u) return;
    g.gunMs = now;
    const Frame fr = FrameOf(v, car);
    const float fl = std::max(0.1f, std::sqrt(fr.f[0] * fr.f[0] + fr.f[1] * fr.f[1]));
    const float ahead[2] = {fr.f[0] / fl * 60.0f, fr.f[1] / fl * 60.0f};
    for (float side : {2.0f, -2.0f}) {
        const float l[3] = {side, 2.5f, 1.0f};
        AirFire::Shot s;
        s.minigun = false;
        s.damage = 15.0f;
        fr.Point(l, s.origin);
        const float ry = (float)((std::rand() & 0xff) - 0x80), rx = (float)((std::rand() & 0xff) - 0x80), rz = (float)((std::rand() & 0xff) - 0x80);
        s.end[0] = s.origin[0] + ahead[0] + rx * 0.015f;
        s.end[1] = s.origin[1] + ahead[1] + ry * 0.015f;
        s.end[2] = s.origin[2] + rz * 0.02f;
        out.shots.push_back(s);
    }
    if (--g.clip <= 0) { g.clip = 20; g.gunMs = now + 1400u; }
}

void ProcessAirWeapons(Vehicles& v, size_t car, const AirPad& pad, uint32_t now, float dt, const AirWorld& w, AirFire& out) {
    if (car >= v.Count()) return;
    VehicleInstance& c = v.Car(car);
    VehicleInstance::AirGuns& g = c.air;
    if (c.model == kPredator) { FixedMachineGuns(v, car, pad.gunFired != 0, now, out); return; }
    const float ts = dt * 50.0f;
    // GetPlaneWeaponFiringStatus
    bool status = false;
    int ordnance = 0, hsTarget = -1;
    switch (c.model) {
    case kHunter:
        if (pad.gunFired == 2) status = true;
        else if (pad.gunJustDown == 1) ordnance = 1;
        break;
    case kSeaspar: case kRCBaron: case kMaverick: case kPolmav: case kCargobob: case kRustler:
        status = pad.gunFired == 2;
        break;
    case kHydra: {
        const Frame fr = FrameOf(v, car);
        if (pad.gunJustDown == 1 && now > g.flareMs + 2000u) {   // a flare 2.5 m behind
            out.flares.push_back({fr.p[0] - fr.f[0] * 2.5f, fr.p[1] - fr.f[1] * 2.5f, fr.p[2] - fr.f[2] * 2.5f,
                                  fr.mv[0] * 50.0f, fr.mv[1] * 50.0f, fr.mv[2] * 50.0f});
            g.flareMs = now;
        }
        if (pad.gunJustDown == 2) {   // a heat seeker once the lock has held 1.5 s, else the rocket pair
            int t = -1;
            if (g.lockMs != 0 && g.lockTarget >= 0) t = ScanAndMark(v, car, fr, g.lockTarget, w, out);
            if (t >= 0 && t == g.lockTarget && now - g.lockMs > 1500u) { ordnance = 2; hsTarget = t; out.locked = true; }
            else ordnance = 1;
        } else if (pad.enterTargeting) {
            g.lockMs = now; g.lockTarget = -1;
        } else if (pad.target) {
            if (g.lockMs == 0) g.lockMs = now;
            const int t = ScanAndMark(v, car, fr, g.lockTarget, w, out);
            if (t < 0 || t != g.lockTarget) g.lockMs = now;
            out.locked = t >= 0 && now - g.lockMs > 1500u;
            g.lockTarget = t;
        } else {
            g.lockMs = 0; g.lockTarget = -1;
        }
        break;
    }
    default:
        return;
    }
    // SelectPlaneWeapon
    switch (c.model) {
    case kHunter:
        g.weapon = ordnance == 1 ? kDoubleRocket : status ? kHeavyGun : g.weapon;
        break;
    case kSeaspar: case kRCBaron:
        if (status) g.weapon = kHeavyGun;
        break;
    case kRustler:
        g.weapon = ordnance == 1 ? kFreefallBomb : status ? kHeavyGun : g.weapon;
        break;
    case kHydra:
        g.weapon = ordnance == 1 ? kDoubleRocket : ordnance == 2 ? kLockOnRocket : status ? kHeavyGun : g.weapon;
        break;
    }
    switch (g.weapon) {
    case kHeavyGun:
        if (status) FirePlaneGuns(v, car, now, ts, w, out);
        break;
    case kLockOnRocket:
        if (ordnance && hsTarget >= 0) FireHeatSeekingMissile(v, car, hsTarget, ordnance, now, ts, out);
        break;
    case kDoubleRocket:
        if (ordnance) FireUnguidedMissile(v, car, ordnance, now, ts, out);
        break;
    }
}

}  // namespace VehicleGuns
