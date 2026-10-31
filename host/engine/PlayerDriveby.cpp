// CJ seated in a car: CTaskSimpleCarDrive's sit/steer/look-back clips (CVehicle::ProcessDrivingAnims)
// and the drive-by (CTaskSimpleGangDriveBy). Retail only lets the player drive by with an SMG:
// CPed::ReplaceWeaponForVehicle swaps one into the hand on entry and ReplaceWeaponWhenExitingVehicle
// gives the old weapon back. The Driveby_x clip holds at 0.2667 s while aiming, the round leaves at
// 0.30 s and the clip loops back from 0.4333 s while the button stays down. See PlayerPed.h.

#define NOMINMAX
#include "PlayerPed.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kHoldTime = 0.2667f;      // CTaskSimpleGangDriveBy: the aim pose
constexpr float kShotTime = 0.30f;        // the round leaves the muzzle
constexpr float kLoopEnd = 0.4333f;       // the clip wraps back to the hold
constexpr float kSteerRange = 0.61f;      // look clips are fully in at this steering angle
constexpr float kSlowSpeed = 0.4f * 50.0f; // CVehicle::m_vecMoveSpeed is per game frame (50 per second)

struct RideSet { const char *idle, *left, *right, *back; };   // CRideAnims (aDriveAnimIds*)
constexpr RideSet kLow = {"car_lsit", "drive_lo_l", "drive_lo_r", "car_lb"};
constexpr RideSet kBad = {"car_sit_weak", "drive_l_weak", "drive_r_weak", "car_lb_weak"};
constexpr RideSet kBadSlow = {"car_sit_weak", "drive_l_weak_slow", "drive_r_weak_slow", "car_lb_weak"};
constexpr RideSet kStd = {"car_sit", "drive_l", "drive_r", "car_lb"};
constexpr RideSet kStdSlow = {"car_sit", "drive_l_slow", "drive_r_slow", "car_lb"};
constexpr RideSet kPro = {"car_sit_pro", "drive_l_pro", "drive_r_pro", "car_lb_pro"};
constexpr RideSet kProSlow = {"car_sit_pro", "drive_l_pro_slow", "drive_r_pro_slow", "car_lb_pro"};
constexpr const RideSet* kSets[] = {&kLow, &kBad, &kBadSlow, &kStd, &kStdSlow, &kPro, &kProSlow};

bool IsSmg(const std::string& label) { return label == "Micro SMG" || label == "MP5" || label == "Tec-9"; }   // the weapon.dat SMG slot
}  // namespace

bool PlayerPed::CanDriveby() const {
    if (m_gun < 0) return false;
    const Gun& g = m_guns[m_gun];
    return IsSmg(g.name) && g.ammo + g.total > 0;
}

// CPed::ReplaceWeaponForVehicle: an SMG with ammo goes in the hand, the old weapon is put by.
void PlayerPed::EnterVehicle(bool lowCar, int bike) {
    if (m_seated) return;
    const int cur = WeaponIndex();
    int smg = -1;
    if (cur >= 0 && IsSmg(m_guns[cur].name) && m_guns[cur].ammo + m_guns[cur].total > 0) smg = cur;
    for (const char* name : {"Micro SMG", "Tec-9", "MP5"}) {
        if (smg >= 0) break;
        for (int i = 0; i < (int)m_guns.size(); i++)
            if (m_guns[i].name == name && m_guns[i].ammo + m_guns[i].total > 0) { smg = i; break; }
    }
    m_savedWeapon = -2;
    if (smg != cur) {
        m_savedWeapon = cur;
        SelectGun(smg);   // no SMG: the hand is empty for the ride (-1 = fists)
    }
    m_seated = true;
    m_seatLow = lowCar;
    m_bike = bike;
    m_busy = m_aiming = m_ducking = m_inAir = m_jumping = false;
    m_firing = m_reloading = false;
    m_gunFireClip = m_gunRelClip = nullptr;
    m_velX = m_velY = 0.0f;
    m_ratio = 0.0f;
    m_dbClip = nullptr;
    m_dbFiring = m_dbShot = false;
    for (Assoc& a : m_assocs) a.delta = -8.0f;   // the on-foot clips fade out under the sit pose
}

void PlayerPed::SetSeatFrame(const float body[16], const float seat[3]) {
    // the seat in the world, the ped's origin its feet's distance below (as Draw puts it: seat - 1 - bind min z)
    const float* r = &body[0]; const float* f = &body[4]; const float* u = &body[8];
    float pos[3];
    for (int k = 0; k < 3; k++) pos[k] = body[12 + k] + r[k] * seat[0] + f[k] * seat[1] + u[k] * seat[2];
    m_seatPos[0] = pos[0]; m_seatPos[1] = pos[1]; m_seatPos[2] = pos[2];
    m_seatYaw = std::atan2(f[1], f[0]);
    std::memcpy(m_seatFrame, body, sizeof(m_seatFrame));
    for (int k = 0; k < 3; k++) m_seatFrame[12 + k] = pos[k] + u[k] * (-1.0f - m_bindMinZ);
    m_seatFrameOn = true;
}

float PlayerPed::BikeStillBlend() const {
    if (!m_seated || !m_bike) return 0.0f;
    const Assoc* a = const_cast<PlayerPed*>(this)->Get(Clip(m_bike == 1 ? "bmx_still" : "bikes_still"));
    return a && !a->dead ? a->blend : 0.0f;
}

// CPed::ReplaceWeaponWhenExitingVehicle. Safe to call whether or not m_seated is set:
// the animated get-out clears m_seated before this runs.
void PlayerPed::LeaveVehicle() {
    m_seated = false;
    m_seatFrameOn = false;
    if (m_dbClip) Remove(m_dbClip);
    m_dbClip = nullptr;
    m_dbFiring = m_dbShot = false;
    m_reloading = false;
    if (m_savedWeapon != -2) SelectGun(m_savedWeapon);
    m_savedWeapon = -2;
    for (Assoc& a : m_assocs) {
        if (!a.clip) continue;
        std::string n = a.clip->name;   // the IFP names keep their case (CAR_sit_weak)
        for (char& ch : n) ch = (char)std::tolower((unsigned char)ch);
        if (n.rfind("car_", 0) == 0 || n.rfind("drive", 0) == 0) a.delta = -8.0f;
    }
    if (const AnimClip* idle = Clip("idle_stance"))
        if (!Get(idle)) { Assoc* a = Add(idle, true, false); a->blend = 0.0f; a->delta = 8.0f; }
}

void PlayerPed::KnockedOffBike(int dir) {
    LeaveVehicle();
    for (Assoc& a : m_assocs) a.delta = -8.0f;   // the ride pose goes at once
    // CEventKnockOffBike::CalcForcesAndAnims: front KO_SKID_BACK, left KD_RIGHT, back KO_SKID_FRONT, right KD_LEFT
    static const char* kFall[4] = {"ko_skid_back", "kd_right", "ko_skid_front", "kd_left"};
    const char* fall = Clip(kFall[dir & 3]) ? kFall[dir & 3] : "ko_skid_front";
    if (!Clip(fall)) return;
    Blend(Clip(fall), false, false, 8.0f);
    m_busy = true;
    m_busySoft = false;
    m_busyClip = Clip(fall);
    m_busyFall = true;
    m_busyNext = Clip(!std::strcmp(fall, "ko_skid_back") ? "getup_front" : "getup");   // KO_SKID_BACK ends face down
    if (!m_busyNext) m_busyNext = Clip("getup_front");
    m_ratio = 0.0f;
    m_velX = m_velY = 0.0f;
}

// The get-in clip for this door: the low variant on a lowrider; _lhs is the driver door, _rhs the passenger door.
const AnimClip* PlayerPed::VehGetInClip(bool lowCar, bool right) const {
    const AnimClip* c = Clip(right ? (lowCar ? "car_getinl_rhs" : "car_getin_rhs") : (lowCar ? "car_getinl_lhs" : "car_getin_lhs"));
    return c ? c : Clip(right ? "car_getin_rhs" : "car_getin_lhs");
}

// CCarEnterExit::GetPositionToOpenCarDoor: the seat minus the get-in clip's net root travel (ComputeAnimDoorOffsets).
// The clip only sets how far out and along CJ stands; the side is the door's.
void PlayerPed::VehDoorOffsetFor(bool lowCar, bool right, float out[2]) const {
    const AnimClip* gi = VehGetInClip(lowCar, right);
    float dx = 1.05f, dy = 0.08f;   // fallback magnitudes
    if (gi && gi->duration > 1e-4f) {
        float x0 = 0.0f, x1 = 0.0f;
        const float y0 = RootY(gi, 0.0f, &x0), y1 = RootY(gi, gi->duration, &x1);
        const float cx = x0 - x1, cy = y0 - y1;   // seat->door, car space
        if (cx * cx + cy * cy > 0.25f) { dx = cx; dy = cy; }
    }
    out[0] = (right ? 1.0f : -1.0f) * std::clamp(std::fabs(dx), 0.8f, 1.5f);   // negative = the car's left side
    out[1] = std::clamp(dy, -0.4f, 0.4f);
}

// CTaskComplexEnterCarAsDriver from the door CCarEnterExit::GetNearestCarDoor chose: open it and get in; through the
// passenger door CJ then slides across (CTaskSimpleCarShuffle). The host walks him to the door first.
void PlayerPed::StartEnterCar(bool lowCar, bool right, bool locked) {
    if (m_vehTrans != VehTrans::None || m_seated) return;
    m_seatLow = lowCar;
    m_vehRight = right;
    m_vehLocked = false;
    VehDoorOffsetFor(lowCar, right, m_vehDoor);
    if (locked) {
        if (const AnimClip* lk = Clip(right ? "car_doorlocked_rhs" : "car_doorlocked_lhs")) {
            m_vehLocked = true;
            m_vehClip = lk;
            m_vehTrans = VehTrans::EnterLocked;
            Blend(lk, false, false, 8.0f);
            m_vehFrac = m_vehApproach = 0.0f;
            m_busy = m_aiming = m_ducking = m_inAir = m_jumping = m_firing = m_reloading = false;
            m_velX = m_velY = 0.0f;
            return;
        }
    }
    const AnimClip* gi = VehGetInClip(lowCar, right);
    const AnimClip* open = Clip(right ? "car_open_rhs" : "car_open_lhs");
    m_vehClip = open ? open : gi;
    m_vehTrans = open ? VehTrans::EnterOpen : VehTrans::EnterGetIn;
    Blend(m_vehClip, false, false, 8.0f);   // fade the on-foot clips out
    m_vehFrac = 0.0f;
    m_vehApproach = open ? 0.0f : 1.0f;   // no open clip: start already at the door
    m_busy = m_aiming = m_ducking = m_inAir = m_jumping = m_firing = m_reloading = false;
    m_velX = m_velY = 0.0f;
}

// CTaskComplexLeaveCar: get out of the driver door. m_seated clears now so the host owns CJ's position.
void PlayerPed::StartExitCar(bool lowCar) {
    if (m_vehTrans != VehTrans::None) return;
    m_seatLow = lowCar;
    m_seated = false;
    if (m_dbClip) { Remove(m_dbClip); m_dbClip = nullptr; }
    m_dbFiring = m_dbShot = false;
    const AnimClip* go = Clip(lowCar ? "car_getoutl_lhs" : "car_getout_lhs");
    if (!go) go = Clip("car_getout_lhs");
    m_vehClip = go;
    m_vehTrans = VehTrans::ExitGetOut;
    m_vehRight = false;   // CJ always leaves by the driver door
    VehDoorOffsetFor(lowCar, false, m_vehDoor);
    m_vehFrac = 1.0f;
    if (go) Blend(go, false, false, 8.0f);   // fade the sit pose out
    else { LeaveVehicle(); m_vehTrans = VehTrans::None; }   // no clip: just pop out
}

// CTaskSimpleCarOpenDoorFromOutside opens the door with the open clip; it stays open while CJ climbs in and
// CTaskSimpleCarCloseDoorFromInside pulls it shut as he settles. Getting out pushes it open and shuts it behind him.
float PlayerPed::VehDoorRatio() const {
    float r = 0.0f;
    switch (m_vehTrans) {
    case VehTrans::EnterOpen: r = m_vehApproach * 2.0f; break;
    case VehTrans::EnterGetIn: r = (1.0f - m_vehFrac) / 0.35f; break;
    case VehTrans::ExitGetOut: { const float p = 1.0f - m_vehFrac; r = std::min(p / 0.2f, (1.0f - p) / 0.3f); break; }
    case VehTrans::EnterShuffle: case VehTrans::EnterLocked: case VehTrans::None: break;
    }
    return r < 0.0f ? 0.0f : (r > 1.0f ? 1.0f : r);
}

void PlayerPed::CancelVehTransition() {
    if (m_vehTrans == VehTrans::None) return;
    m_vehTrans = VehTrans::None;
    m_vehClip = nullptr;
    LeaveVehicle();   // restore any swapped weapon and blend back to idle
}

// Advance the open/get-in/get-out clips and report how far along the seat the origin should be.
void PlayerPed::UpdateVehTransition(const Input&) {
    m_heading = m_aimHeading = m_seatYaw;   // aligned with the car's nose throughout
    m_velX = m_velY = 0.0f;
    Assoc* a = m_vehClip ? Get(m_vehClip) : nullptr;
    const float dur = m_vehClip ? m_vehClip->duration : 0.0f;
    const float prog = (a && dur > 1e-4f) ? std::clamp(a->time / dur, 0.0f, 1.0f) : 1.0f;
    const bool done = !a || a->dead || a->finished || (dur > 1e-4f && a->time >= dur - 1e-4f);
    switch (m_vehTrans) {
    case VehTrans::EnterOpen:
        m_vehFrac = 0.0f;       // at the door
        m_vehApproach = prog;   // slide up to the door while it opens
        if (done) {
            m_vehApproach = 1.0f;
            m_vehClip = VehGetInClip(m_seatLow, m_vehRight);
            m_vehTrans = VehTrans::EnterGetIn;
            if (m_vehClip) Blend(m_vehClip, false, false, 8.0f);
        }
        break;
    case VehTrans::EnterGetIn:
        m_vehFrac = prog;
        if (done) {
            const AnimClip* sh = m_vehRight ? Clip(m_seatLow ? "car_lshuffle_rhs" : "car_shuffle_rhs") : nullptr;
            if (sh) {   // in the passenger seat: slide across to the wheel
                m_vehClip = sh;
                m_vehTrans = VehTrans::EnterShuffle;
                m_vehShuffle = 0.0f;
                Blend(sh, false, false, 8.0f);
            } else { m_vehTrans = VehTrans::None; m_vehClip = nullptr; EnterVehicle(m_seatLow, 0); }
        }
        break;
    case VehTrans::EnterLocked:   // up to the door, a tug at the handle, back to standing
        m_vehFrac = 0.0f;
        m_vehApproach = std::min(1.0f, prog * 3.0f);
        if (done) { m_vehTrans = VehTrans::None; m_vehClip = nullptr; LeaveVehicle(); }
        break;
    case VehTrans::EnterShuffle:
        m_vehFrac = 1.0f;
        m_vehShuffle = prog;
        if (done) { m_vehTrans = VehTrans::None; m_vehClip = nullptr; m_vehShuffle = 1.0f; EnterVehicle(m_seatLow, 0); }
        break;
    case VehTrans::ExitGetOut:
        m_vehFrac = 1.0f - prog;
        if (done) { m_vehTrans = VehTrans::None; m_vehClip = nullptr; LeaveVehicle(); }
        break;
    default: break;
    }
}

void PlayerPed::UpdateSeated(const Input& in) {
    const float dt = in.dt;
    m_heading = m_aimHeading = m_seatYaw;
    m_velX = m_velY = 0.0f;
    m_flash = std::max(0.0f, m_flash - dt);

    if (m_bike) {   // CBmx::ProcessDrivingAnims / CBike::ProcessRiderAnims
        const char* grp = m_bike == 1 ? "bmx_" : "bikes_";
        auto nm = [&](const char* n) { return std::string(grp) + n; };
        const float spd = in.carSpeed;
        const bool pedal = m_bike == 1 && in.carGas && spd > 0.02f;
        const bool sprint = pedal && spd * 3.0f >= 2.5f;
        std::string want = spd < 0.02f && !in.carGas ? nm("still") : sprint ? nm("sprint") : pedal ? nm("pedal") : m_bike == 1 ? nm("still") : nm("ride");
        if (in.bikeRide && m_bike == 2) want = in.bikeBase == 1 ? nm("still") : in.bikeBase == 2 ? nm("pushes") : nm("ride");
        for (const char* n : {"still", "pedal", "sprint", "ride", "pushes"}) {
            const AnimClip* c = Clip(nm(n).c_str());
            if (!c) continue;
            Assoc* a = Get(c);
            const bool mine = want == nm(n);
            if (mine) {
                if (!a) { a = Add(c, true, false); a->blend = 0.0f; }
                a->delta = 4.0f;
                if (!std::strcmp(n, "pedal") || !std::strcmp(n, "sprint")) a->speed = std::max(spd * 3.0f, 0.5f);
            } else if (a) a->delta = -4.0f;
        }
        const Assoc* base = Get(Clip(want.c_str()));
        const bool ok = base && base->blend >= 1.0f;
        if (in.bikeRide) {   // CBike::ProcessRiderAnims: the lean clips held at a time by the lean, sharing what the base leaves
            float budget = 1.0f;
            for (const char* n : {"still", "pushes"})
                if (const Assoc* a = Get(Clip(nm(n).c_str())); a && !a->dead) budget -= std::min(budget, a->blend);
            auto pose = [&](const char* n, float amount, float t) {
                const AnimClip* c = Clip(nm(n).c_str());
                if (!c) return;
                Assoc* a = Get(c);
                if (!a) { a = Add(c, true, false); a->partial = true; }
                a->blend = amount;
                a->delta = 0.0f;
                a->speed = 0.0f;
                a->time = c->duration * std::clamp(t, 0.0f, 1.0f);
            };
            const bool back = in.bikeLeanFwd < 0.0f, left = in.bikeLeanLeft < 0.0f;
            pose(back ? "back" : "fwd", in.bikeFwdWeight * budget, std::fabs(in.bikeLeanFwd));
            pose(back ? "fwd" : "back", 0.0f, 0.0f);
            pose(left ? "left" : "right", in.bikeLeftWeight * budget, std::fabs(in.bikeLeanLeft));
            pose(left ? "right" : "left", 0.0f, 0.0f);
            return;
        }
        const AnimClip *cl = Clip(nm("left").c_str()), *cr = Clip(nm("right").c_str());
        const float lean = std::clamp(in.carSteer / kSteerRange, -1.0f, 1.0f);   // positive = left
        for (int side = 0; side < 2; side++) {
            const AnimClip* c = side ? cr : cl;
            if (!c) continue;
            const float amt = ok ? std::max(side ? -lean : lean, 0.0f) : 0.0f;
            Assoc* a = Get(c);
            if (amt <= 0.0f) { if (a) a->delta = -4.0f; continue; }
            if (!a) { a = Add(c, true, false); a->partial = true; }
            a->blend = amt;
            a->delta = 0.0f;
            a->speed = 0.0f;
            a->time = c->duration * amt;
        }
        return;
    }

    // ---- CVehicle::ProcessDrivingAnims: the tiered sit pose, steering and look-back overlays ----
    const RideSet* use;
    if (m_seatLow) use = &kLow;
    else {
        const bool slow = in.carSpeed <= kSlowSpeed;
        use = m_drivingSkill < 50.0f ? (slow ? &kBadSlow : &kBad)
              : m_drivingSkill < 100.0f ? (slow ? &kStdSlow : &kStd) : (slow ? &kProSlow : &kPro);
    }
    auto fade = [&](const AnimClip* c, float rate) { if (Assoc* a = Get(c)) a->delta = -rate; };
    auto idleOf = [&](bool want, const char* name) {
        const AnimClip* c = Clip(name);
        if (!c) return;
        Assoc* a = Get(c);
        if (want) {
            if (!a) { a = Add(c, true, false); a->blend = 0.0f; }
            a->delta = 4.0f;
        } else if (a) a->delta = -4.0f;
    };
    // the idle pose of this tier; the other tiers' poses fade
    for (const RideSet* s : kSets) {
        const bool mine = !std::strcmp(s->idle, use->idle);
        idleOf(mine, s->idle);
    }
    const Assoc* idle = Get(Clip(use->idle));
    const bool settled = idle && idle->blend >= 1.0f;
    // a different tier's look clips fade away, this tier's are driven below
    for (const RideSet* s : kSets) {
        for (const char* n : {s->left, s->right, s->back}) {
            if (!n) continue;
            if (!std::strcmp(n, use->left) || !std::strcmp(n, use->right) || !std::strcmp(n, use->back)) continue;
            fade(Clip(n), 4.0f);
        }
    }

    // ---- CTaskSimpleGangDriveBy ----
    Gun* g = m_gun >= 0 ? &m_guns[m_gun] : nullptr;
    const bool smg = g && IsSmg(g->name);
    const bool want = in.fireHeld || in.fireJust || in.aim;
    if (m_reloading) {   // timed reload of the clip, no animation of its own in the car
        m_reloadTime += dt;
        if (m_reloadTime >= g->reloadTime) {
            m_reloading = false;
            const int take = std::min(g->clipSize - g->ammo, g->total);
            g->ammo += take;
            g->total -= take;
        }
    } else if (smg && g->ammo < g->clipSize && g->total > 0 && (in.reloadJust || g->ammo == 0)) {
        m_reloading = true;
        m_reloadTime = 0.0f;
        m_reloadEvents++;
        m_dbFiring = false;
    }
    if (smg && !m_dbClip && want && !m_reloading && g->ammo > 0 && settled) {
        // the window the camera looks out of: its direction against the car's nose, left of nose = +90 degrees
        const float rel = std::atan2(std::sin(in.camYaw - m_seatYaw), std::cos(in.camYaw - m_seatYaw));
        m_dbSide = std::sin(rel) >= 0.0f ? 1 : -1;
        const AnimClip* c = Clip(m_dbSide > 0 ? (m_seatLow ? "drivebyl_l" : "driveby_l")
                                              : (m_seatLow ? "drivebyl_r" : "driveby_r"));
        if (c) {
            m_dbClip = c;
            Assoc* a = Add(c, false, false);
            a->partial = true;
            a->blend = 0.0f;
            a->delta = 8.0f;
            a->time = 0.0f;
            a->playing = false;
            m_dbTime = 0.0f;
            m_dbFiring = false;
            m_dbShot = false;
        }
    }
    bool driveby = false;
    if (m_dbClip) {
        Assoc* a = Get(m_dbClip);
        if (!a || a->dead) { m_dbClip = nullptr; }
        else if (!smg || !want || m_reloading || (g->ammo == 0 && !in.fireJust)) {   // let go: the clip fades out
            a->delta = -8.0f;
            a->playing = true;
            if (a->blend <= 0.0f) { a->dead = true; m_dbClip = nullptr; }
            m_dbFiring = false;
        } else {
            driveby = true;
            a->delta = 8.0f;
            // the Driveby_x clip is only 0.2 s of blend into the aim pose; the task times the hold, the shot
            // and the loop on its own clock (CTaskSimpleGangDriveBy), the pose stays at the clip's last key
            a->playing = false;
            const bool trigger = in.fireHeld || in.fireJust;
            m_dbTime += dt;
            if (!m_dbFiring) {
                if (m_dbTime >= kHoldTime) {
                    m_dbTime = kHoldTime;
                    if (trigger && g->ammo > 0) { m_dbFiring = true; m_dbShot = false; }
                }
            }
            if (m_dbFiring) {
                if (!m_dbShot && m_dbTime >= kShotTime) {
                    m_dbShot = true;
                    g->ammo--;
                    m_shotEvents++;
                    m_attackCounter += g->damage * 0.04f;
                    m_lastShotLeft = false;
                    m_flash = 0.06f;
                    std::fprintf(stderr, "[GUN] driveby %s ammo %d/%d\n", g->name.c_str(), g->ammo, g->clipSize);
                }
                if (m_dbTime >= kLoopEnd) {
                    m_dbTime = kHoldTime + (m_dbTime - kLoopEnd);
                    m_dbShot = false;
                    if (!trigger || g->ammo == 0) { m_dbFiring = false; m_dbTime = kHoldTime; }
                }
            }
            a->time = std::min(m_dbTime, a->clip->duration);
        }
    }
    if (m_dbClip) m_attackCounter *= std::pow(0.96f, dt * 50.0f);

    // steering and look-back overlays (partial clips over the sit pose); they give way to a drive-by
    const AnimClip *cl = Clip(use->left), *cr = Clip(use->right), *cb = Clip(use->back);
    auto look = [&](const AnimClip* c, float amount) {
        if (!c) return;
        Assoc* a = Get(c);
        if (!a) { a = Add(c, true, false); a->partial = true; }
        a->blend = amount;
        a->delta = 0.0f;
    };
    if (settled) {
        if (!m_seatLow && in.carReverse && !driveby) {
            if (Assoc* l = Get(cl)) l->delta = -4.0f;
            if (Assoc* r = Get(cr)) r->delta = -4.0f;
            Assoc* b = Get(cb);
            if (!b) { b = Add(cb, true, false); b->partial = true; b->blend = 0.0f; }
            b->delta = 4.0f;
        } else if (in.carSteer == 0.0f || driveby) {
            fade(cl, 4.0f); fade(cr, 4.0f); fade(cb, 4.0f);
        } else {
            const float amount = std::clamp(std::fabs(in.carSteer) / kSteerRange, 0.0f, 1.0f);
            if (in.carSteer < 0.0f) { if (Assoc* l = Get(cl)) { l->blend = 0.0f; l->delta = 0.0f; } look(cr, amount); }
            else { if (Assoc* r = Get(cr)) { r->blend = 0.0f; r->delta = 0.0f; } look(cl, amount); }
            fade(cb, 4.0f);
        }
    }
}
