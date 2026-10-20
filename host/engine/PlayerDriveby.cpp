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
void PlayerPed::EnterVehicle(bool lowCar) {
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
    m_busy = m_aiming = m_ducking = m_inAir = m_jumping = false;
    m_firing = m_reloading = false;
    m_gunFireClip = m_gunRelClip = nullptr;
    m_velX = m_velY = 0.0f;
    m_ratio = 0.0f;
    m_dbClip = nullptr;
    m_dbFiring = m_dbShot = false;
    for (Assoc& a : m_assocs) a.delta = -8.0f;   // the on-foot clips fade out under the sit pose
}

// CPed::ReplaceWeaponWhenExitingVehicle.
void PlayerPed::LeaveVehicle() {
    if (!m_seated) return;
    m_seated = false;
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

void PlayerPed::UpdateSeated(const Input& in) {
    const float dt = in.dt;
    m_heading = m_aimHeading = m_seatYaw;
    m_velX = m_velY = 0.0f;
    m_flash = std::max(0.0f, m_flash - dt);

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
