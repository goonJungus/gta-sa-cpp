// On-foot player camera. See Camera.h.

#define NOMINMAX
#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace {

float Approach(float response, float dt) { return dt > 0.0f ? 1.0f - std::exp(-response * dt) : 0.0f; }

// CCamera::Init zoom values (m_aCams[0] +0xdc / +0xe0 / +0xe4).
constexpr float kPedZoom[3] = {-0.55f, 1.5f, 3.6f};
// PEDCAM_SET exterior (0x8cc548): look-at height above the ped centre and the
// base distance.
constexpr float kPedCamHeight = 0.6f, kPedCamDist = 2.0f;
constexpr float kPedCentre = 1.0f;          // CPed position above the feet

// Rewrite shoulder camera preset.
constexpr float kOffsetX = 0.45f, kOffsetY = 0.15f, kSwapResponse = 8.0f;
constexpr float kAnchorHeight = 0.26f, kAnchorFallback = 0.82f;
constexpr float kDistance = 2.30f, kZoomResponse = 8.0f;
constexpr float kLateralResponse = 31.5f, kDepthResponse = 6.0f, kApproachResponse = 14.0f;
constexpr float kVerticalResponse = 14.0f;
constexpr float kMaxLateralLag = 0.22f, kMaxApproachLag = 0.12f, kMaxRecedeLag = 0.45f, kMaxVerticalLag = 0.35f;
constexpr float kColRadius = 0.18f, kColPadding = 0.12f, kColRecovery = 6.0f, kNearClip = 0.15f;
constexpr float kTeleport = 8.0f;

}  // namespace

PlayerCamera::Output PlayerCamera::Update(const Input& in) {
    Output out;
    const float cp = std::cos(in.pitch), sp = std::sin(in.pitch);
    const float dir[3] = {std::cos(in.yaw) * cp, std::sin(in.yaw) * cp, sp};
    const float fwd[3] = {std::cos(in.yaw), std::sin(in.yaw), 0.0f};   // horizontal
    const float right[3] = {std::sin(in.yaw), -std::cos(in.yaw), 0.0f};
    for (int k = 0; k < 3; k++) out.dir[k] = dir[k];
    if (in.style != m_lastStyle) {   // style changed: start from that style's default view
        m_view = in.style == kRewrite ? 2 : 1;
        m_lastStyle = in.style;
        m_anchorValid = m_eyeValid = false;
        m_colDist = -1.0f;
    }
    // Fraction along a->b to the first hit. While climbing, a hit close to the
    // start is the ledge or fence being climbed (or just climbed over), so the
    // ray carries on past it.
    m_climbGrace = in.climbing ? 1.5f : std::max(0.0f, m_climbGrace - in.dt);
    auto cast = [&](const float a[3], const float b[3]) {
        const float f = in.ray(a, b);
        if (m_climbGrace <= 0.0f || f >= 1.0f) return f;
        const float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (len < 1e-4f) return f;
        float hit = f;   // fraction of the whole segment
        for (int i = 0; i < 4 && hit * len < 2.5f; i++) {
            const float st = hit + 0.05f / len;
            if (st >= 1.0f) return 1.0f;
            const float from[3] = {a[0] + d[0] * st, a[1] + d[1] * st, a[2] + d[2] * st};
            const float g = in.ray(from, b);
            if (g >= 1.0f) return 1.0f;
            hit = st + g * (1.0f - st);
        }
        return hit;
    };
    auto clipToWorld = [&](const float a[3], float b[3], float pad) {
        if (!in.ray) return;
        const float f = cast(a, b);
        if (f >= 1.0f) return;
        const float d[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
        const float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        const float keep = std::max(0.0f, f * len - pad) / std::max(len, 1e-4f);
        for (int k = 0; k < 3; k++) b[k] = a[k] + d[k] * keep;
    };

    if (in.scope) {   // CCam::Process_M16_1stPerson (MODE_SNIPER / MODE_CAMERA): from the eyes, zoomed
        float eye[3];
        if (in.haveHead) {
            for (int k = 0; k < 3; k++) eye[k] = in.head[k] + fwd[k] * 0.10f;
            eye[2] += 0.07f;
        } else {
            for (int k = 0; k < 3; k++) eye[k] = in.feet[k];
            eye[2] += 1.6f;
        }
        for (int k = 0; k < 3; k++) out.eye[k] = eye[k];
        out.nearClip = 0.1f;
        out.hideHead = true;
        out.firstPerson = true;
        out.fovScale = std::tan(in.scopeFov * 0.5f * 3.14159265f / 180.0f) / std::tan(35.0f * 3.14159265f / 180.0f);
        m_anchorValid = m_eyeValid = false;
        return out;
    }

    if (in.style == kClassic) {
        // Process_FollowPed_SA: ease the zoom, then sit behind the look-at point.
        const float want = kPedZoom[m_view];
        const float step = 0.12f * 50.0f * in.dt;
        m_zoomSmoothed = m_zoomSmoothed < want ? std::min(want, m_zoomSmoothed + step)
                                               : std::max(want, m_zoomSmoothed - step);
        const float dist = kPedCamDist + m_zoomSmoothed;
        const float target[3] = {in.feet[0], in.feet[1], in.feet[2] + kPedCentre + kPedCamHeight};
        float eye[3] = {target[0] - dir[0] * dist, target[1] - dir[1] * dist, target[2] - dir[2] * dist};
        clipToWorld(target, eye, 0.2f);
        // CCam::Process_AimWeapon (on foot, free aim): the FOV eases to the weapon's 1 degree per
        // 1/50 s; the look-at point is 0.5 m above the ped centre (+0.1 m zoomed in) and 0.2 m to
        // his right (0.2 .. 0.3 zoomed in); the camera sits 1.0 + 1.6 cos(pitch) behind it.
        const float fovTarget = in.aimWeapon ? in.aimFov : 70.0f;
        const float fstep = 50.0f * in.dt;
        m_aimFov = m_aimFov < fovTarget ? std::min(fovTarget, m_aimFov + fstep) : std::max(fovTarget, m_aimFov - fstep);
        // CCamera::StartTransition: into MODE_AIMWEAPON 400 ms (fractions 0 / 1), back to the follow
        // camera 350 ms (fractions 0.1 / 0.9); in between a cosine ease.
        if (in.aimWeapon != m_aimIn) { m_aimIn = in.aimWeapon; m_aimT = 0.0f; m_aimFrom = m_aimBlend; }
        m_aimT += in.dt;
        {
            const float dur = m_aimIn ? 0.4f : 0.35f, sm = m_aimIn ? 0.0f : 0.1f, cu = m_aimIn ? 1.0f : 0.9f;
            const float f = std::clamp((m_aimT / dur - sm) / (cu - sm), 0.0f, 1.0f);
            const float e = 0.5f - 0.5f * std::cos(f * 3.14159265f);
            m_aimBlend = m_aimFrom + ((m_aimIn ? 1.0f : 0.0f) - m_aimFrom) * e;
        }
        if (m_aimBlend > 0.0f) {
            float side = 0.2f, lift = 0.0f;
            if (m_aimFov < 70.0f) {
                lift = std::min(1.0f, (70.0f - m_aimFov) / (70.0f - 50.0f)) * 0.1f;
                side = std::min(1.0f, (70.0f - m_aimFov) / (70.0f - 35.0f)) * 0.1f + 0.2f;
            }
            const float rx = std::sin(in.pedHeading), ry = -std::cos(in.pedHeading);   // ped right
            const float at[3] = {in.feet[0] + rx * side, in.feet[1] + ry * side, in.feet[2] + kPedCentre + 0.5f + lift};
            // vertical angle limits for on foot: -89 .. +45 degrees
            const float v = std::clamp(in.pitch, -1.5533f, 0.7854f);
            const float d = 1.0f + 1.6f * std::cos(v);
            const float cv = std::cos(v), sv = std::sin(v);
            const float ad[3] = {std::cos(in.yaw) * cv, std::sin(in.yaw) * cv, sv};
            float aimEye[3] = {at[0] - ad[0] * d, at[1] - ad[1] * d, at[2] - ad[2] * d};
            clipToWorld(at, aimEye, 0.2f);
            const float t = m_aimBlend;
            for (int k = 0; k < 3; k++) { eye[k] += (aimEye[k] - eye[k]) * t; out.dir[k] = dir[k] + (ad[k] - dir[k]) * t; }
            const float l = std::sqrt(out.dir[0] * out.dir[0] + out.dir[1] * out.dir[1] + out.dir[2] * out.dir[2]);
            for (int k = 0; k < 3; k++) out.dir[k] /= l;
        }
        out.fovScale = std::tan(m_aimFov * 0.5f * 3.14159265f / 180.0f) / std::tan(35.0f * 3.14159265f / 180.0f);
        for (int k = 0; k < 3; k++) out.eye[k] = eye[k];
        out.nearClip = 0.3f;
        m_anchorValid = m_eyeValid = false;
        return out;
    }

    if (m_view == 0) {   // first person, from the eyes
        float eye[3];
        if (in.haveHead) {
            for (int k = 0; k < 3; k++) eye[k] = in.head[k] + fwd[k] * 0.10f;
            eye[2] += 0.07f;
        } else {
            for (int k = 0; k < 3; k++) eye[k] = in.feet[k];
            eye[2] += 1.65f;
        }
        // soften the run's head bob a little
        if (!m_eyeValid || std::fabs(eye[2] - m_eyeZ) > 1.0f) m_eyeZ = eye[2];
        m_eyeZ += Approach(20.0f, in.dt) * (eye[2] - m_eyeZ);
        m_eyeValid = true;
        eye[2] = m_eyeZ;
        for (int k = 0; k < 3; k++) out.eye[k] = eye[k];
        out.nearClip = 0.06f;
        out.hideHead = true;
        out.firstPerson = true;
        m_anchorValid = false;
        return out;
    }
    m_eyeValid = false;

    // Shoulder views: the anchor on the spine...
    float anchor[3];
    if (in.haveSpine) {
        for (int k = 0; k < 3; k++) anchor[k] = in.spine[k];
        anchor[2] += kAnchorHeight;
    } else {
        for (int k = 0; k < 3; k++) anchor[k] = in.feet[k];
        anchor[2] += kPedCentre + kAnchorFallback + kAnchorHeight;
    }
    // ...followed with a lag per camera axis, within limits.
    const float jump = std::sqrt((anchor[0] - m_anchor[0]) * (anchor[0] - m_anchor[0]) +
                                 (anchor[1] - m_anchor[1]) * (anchor[1] - m_anchor[1]) +
                                 (anchor[2] - m_anchor[2]) * (anchor[2] - m_anchor[2]));
    if (!m_anchorValid || jump > kTeleport) {
        for (int k = 0; k < 3; k++) m_anchor[k] = anchor[k];
        m_anchorValid = true;
        m_colDist = -1.0f;
    } else {
        const float rel[3] = {anchor[0] - m_anchor[0], anchor[1] - m_anchor[1], anchor[2] - m_anchor[2]};
        const float lat = rel[0] * right[0] + rel[1] * right[1];
        const float dep = rel[0] * fwd[0] + rel[1] * fwd[1];
        const float aLat = Approach(kLateralResponse, in.dt) * lat;
        const float aDep = Approach(dep < 0.0f ? kApproachResponse : kDepthResponse, in.dt) * dep;
        m_anchor[0] += right[0] * aLat + fwd[0] * aDep;
        m_anchor[1] += right[1] * aLat + fwd[1] * aDep;
        m_anchor[2] += Approach(kVerticalResponse, in.dt) * rel[2];
        const float off[3] = {m_anchor[0] - anchor[0], m_anchor[1] - anchor[1], m_anchor[2] - anchor[2]};
        const float oLat = std::clamp(off[0] * right[0] + off[1] * right[1], -kMaxLateralLag, kMaxLateralLag);
        const float oDep = std::clamp(off[0] * fwd[0] + off[1] * fwd[1], -kMaxRecedeLag, kMaxApproachLag);
        m_anchor[0] = anchor[0] + right[0] * oLat + fwd[0] * oDep;
        m_anchor[1] = anchor[1] + right[1] * oLat + fwd[1] * oDep;
        m_anchor[2] = anchor[2] + std::clamp(off[2], -kMaxVerticalLag, kMaxVerticalLag);
    }

    // Shoulder side (view 1 = left, 2 = right) and distance, both eased.
    const float side = m_view == 1 ? -kOffsetX : kOffsetX;
    m_side += Approach(kSwapResponse, in.dt) * (side - m_side);
    m_dist += Approach(kZoomResponse, in.dt) * (kDistance - m_dist);
    const float dist = std::clamp(m_dist, 0.6f, 8.0f);
    // Camera up = world up tilted with the view.
    const float up[3] = {-fwd[0] * sp, -fwd[1] * sp, cp};
    float desired[3];
    for (int k = 0; k < 3; k++) desired[k] = m_anchor[k] + right[k] * m_side + up[k] * kOffsetY - dir[k] * dist;

    // Collision: five rays (centre, +-radius along right and up).
    const float full[3] = {desired[0] - m_anchor[0], desired[1] - m_anchor[1], desired[2] - m_anchor[2]};
    const float fullLen = std::sqrt(full[0] * full[0] + full[1] * full[1] + full[2] * full[2]);
    float frac = 1.0f;
    if (in.ray && fullLen > 0.01f) {
        const float offs[5][3] = {{0, 0, 0},
                                  {right[0] * kColRadius, right[1] * kColRadius, 0},
                                  {-right[0] * kColRadius, -right[1] * kColRadius, 0},
                                  {up[0] * kColRadius, up[1] * kColRadius, up[2] * kColRadius},
                                  {-up[0] * kColRadius, -up[1] * kColRadius, -up[2] * kColRadius}};
        for (const auto& o : offs) {
            const float a[3] = {m_anchor[0] + o[0], m_anchor[1] + o[1], m_anchor[2] + o[2]};
            const float b[3] = {desired[0] + o[0], desired[1] + o[1], desired[2] + o[2]};
            frac = std::min(frac, cast(a, b));
        }
    }
    const float allowed = frac >= 1.0f ? fullLen : std::max(0.03f, frac * fullLen - kColPadding);
    if (m_colDist < 0.0f || allowed < m_colDist) m_colDist = allowed;   // pull in at once
    else m_colDist += Approach(kColRecovery, in.dt) * (allowed - m_colDist);
    const float use = std::min(m_colDist, fullLen) / std::max(fullLen, 1e-4f);
    for (int k = 0; k < 3; k++) out.eye[k] = m_anchor[k] + full[k] * use;
    out.nearClip = kNearClip;
    return out;
}

namespace {

constexpr float kPi = 3.14159265f;

// CARCAM_SET (0x8cc600), one row per CarCamera::Kind: [0] height mult, [1] base distance, [2] height
// offset, [3] min distance mult, [4] lag distance, [5] alpha ease (pow per step), [6] max alpha step,
// [7] -, [8] speed damping (pow per step), [9] max beta / alpha speed, [10] swing per side speed,
// [11] max swing per step, [12] pad input scale, [13] max up angle, [14] max down angle.
constexpr float kCarCamSet[6][15] = {
    {1.3f, 1.0f, 0.4f, 10.0f, 15.0f, 0.5f, 1.0f, 1.0f, 0.85f, 0.2f, 0.075f, 0.05f, 0.8f, 0.785398f, 1.55334f},
    {1.1f, 1.0f, 0.1f, 10.0f, 11.0f, 0.5f, 1.0f, 1.0f, 0.85f, 0.2f, 0.075f, 0.05f, 0.75f, 0.785398f, 1.55334f},
    {1.1f, 1.0f, 0.2f, 10.0f, 15.0f, 0.05f, 0.05f, 0.0f, 0.9f, 0.05f, 0.01f, 0.05f, 1.0f, 0.174533f, 1.22173f},
    {1.1f, 3.5f, 0.2f, 10.0f, 25.0f, 0.5f, 1.0f, 1.0f, 0.75f, 0.1f, 0.005f, 0.2f, 1.0f, 1.55334f, 1.55334f},
    {1.3f, 1.0f, 0.4f, 10.0f, 15.0f, 0.5f, 1.0f, 0.0f, 0.9f, 0.05f, 0.005f, 0.05f, 1.0f, 0.349066f, 1.22173f},
    {1.1f, 1.0f, 0.2f, 10.0f, 5.0f, 0.5f, 1.0f, 1.0f, 0.75f, 0.1f, 0.005f, 0.2f, 1.0f, 0.785398f, 1.55334f},
};
// CCamera::CamControl car zoom per m_nCarZoom 1..3 and arrPos (0x8cc3e0 / 0x8cc3f4 / 0x8cc408).
constexpr float kCarZoom[3][5] = {{-1.0f, -0.2f, -3.2f, 0.05f, -2.41f}, {1.0f, 1.4f, 0.65f, 1.9f, 6.49f}, {6.0f, 6.0f, 15.9f, 15.9f, 15.0f}};
// ZmOne / ZmTwo / ZmThree alpha offsets (0x8cc41c / 0x8cc430 / 0x8cc444): how far the view looks down.
constexpr float kCarZoomAlpha[3][5] = {{0.08f, 0.08f, 0.15f, 0.08f, 0.08f}, {0.07f, 0.08f, 0.3f, 0.08f, 0.08f}, {0.055f, 0.05f, 0.15f, 0.06f, 0.08f}};

void FrontOf(float beta, float alpha, float f[3]) {
    const float ca = std::cos(alpha);
    f[0] = -std::cos(beta) * ca; f[1] = -std::sin(beta) * ca; f[2] = std::sin(alpha);
}
float WrapNear(float a, float ref) {
    if (a > ref + kPi) a -= 2.0f * kPi; else if (a < ref - kPi) a += 2.0f * kPi;
    return a;
}

}  // namespace

PlayerCamera::Output CarCamera::Update(const Input& in) {
    PlayerCamera::Output out;
    const float ts = in.dt * 50.0f;
    const float* s = kCarCamSet[std::clamp(in.kind, 0, 5)];
    const int ap = std::clamp(in.arrPos, 0, 4), zi = std::clamp(m_zoom, 1, 3) - 1;
    float T[3] = {in.pos[0], in.pos[1], in.pos[2]};

    // CCamera::CamControl: the zoom eases 0.12 per step.
    const float zoomBase = kCarZoom[zi][ap];
    if (m_zoomSmoothed < -1e8f) m_zoomSmoothed = zoomBase;
    else m_zoomSmoothed = m_zoomSmoothed < zoomBase ? std::min(zoomBase, m_zoomSmoothed + 0.12f * ts) : std::max(zoomBase, m_zoomSmoothed - 0.12f * ts);
    float dist = m_zoomSmoothed + s[1] + 2.0f * std::fabs(in.colMin[1]);
    float alphaOff = kCarZoomAlpha[zi][ap];
    const float height = in.colMax[2];
    if (in.kind == kHeli) {   // a helicopter is looked at from above its rotor
        for (int k = 0; k < 3; k++) T[k] += 0.6f * in.up[k] * height;
    } else {
        const float lift = height * s[0] - s[2];   // tall vehicles: look at a higher point from further back
        if (lift > 0.0f) { T[2] += lift; dist += lift; alphaOff += 0.3f / dist * lift; }
    }
    float lag = s[4];
    if (m_zoom == 1 && (in.kind == kCar || in.kind == kBike)) lag *= 0.65f;
    lag = std::max(lag, dist);

    // FOV: widens past 0.4 m per step forwards, relaxing back at pow(0.98, ts).
    const float fwdSpeed = in.mv[0] * in.fwd[0] + in.mv[1] * in.fwd[1] + in.mv[2] * in.fwd[2];
    if (in.reset) m_fov = 70.0f;
    else {
        if ((in.kind == kCar || in.kind == kBike) && fwdSpeed > 0.4f) m_fov += (fwdSpeed - 0.4f) * ts;
        if (m_fov > 70.0f) m_fov = (m_fov - 70.0f) * std::pow(0.98f, ts) + 70.0f;
        m_fov = std::clamp(m_fov, 70.0f, 100.0f);
    }
    const float heading = std::atan2(-in.fwd[0], in.fwd[1]);
    if (in.reset) {
        m_beta = heading - 0.5f * kPi;
        m_betaSpeed = m_alphaSpeed = 0.0f;
        float f[3];
        FrontOf(m_beta, 0.0f, f);
        for (int k = 0; k < 3; k++) { m_hist[0][k] = T[k] - lag * f[k]; m_hist[1][k] = T[k] - dist * f[k]; }
        m_alpha = -alphaOff;
        m_mouseHold = 0.0f;
        m_colDist = -1.0f;
    }

    // The view from the lag point decides where the camera wants to be.
    float front[3] = {T[0] - m_hist[0][0], T[1] - m_hist[0][1], T[2] - m_hist[0][2]};
    {
        const float l = std::sqrt(front[0] * front[0] + front[1] * front[1] + front[2] * front[2]);
        if (l > 1e-5f) for (float& v : front) v /= l; else FrontOf(m_beta, m_alpha, front);
    }
    const float h1[3] = {T[0] - m_hist[1][0], T[1] - m_hist[1][1], T[2] - m_hist[1][2]};
    const float hist1Dist = std::sqrt(h1[0] * h1[0] + h1[1] * h1[1] + h1[2] * h1[2]);
    float cur = std::atan2(-front[0], front[1]) - 0.5f * kPi;
    if (cur < -kPi) cur += 2.0f * kPi;
    float want = cur;
    if (std::sqrt(in.mv[0] * in.mv[0] + in.mv[1] * in.mv[1]) > 0.02f) want = std::atan2(-in.mv[0], in.mv[1]) - 0.5f * kPi;
    want = WrapNear(want, cur);
    // Swing behind the travel direction, faster the more the vehicle moves across the view.
    const float along = in.mv[0] * front[0] + in.mv[1] * front[1] + in.mv[2] * front[2];
    const float perp[3] = {in.mv[0] - along * front[0], in.mv[1] - along * front[1], in.mv[2] - along * front[2]};
    const float swingF = std::min(1.0f, ts * s[10] * std::sqrt(perp[0] * perp[0] + perp[1] * perp[1] + perp[2] * perp[2]));
    const float swingMax = ts * s[11];
    const float nb = WrapNear(cur + std::clamp((want - cur) * swingF, -swingMax, swingMax), m_beta);
    const float betaTarget = (nb - m_beta) / std::max(ts, 1.0f);

    // Vertical: follow the lag point's slope, looking down by the zoom's alpha offset.
    float alphaTarget = std::asin(std::clamp(front[2], -1.0f, 1.0f)) - alphaOff;
    float maxUp = s[13];
    const float mv2 = in.mv[0] * in.mv[0] + in.mv[1] * in.mv[1] + in.mv[2] * in.mv[2];
    const bool grounded = in.kind == kBike ? in.wheelsOnGround > 3 : in.kind == kPlane ? in.wheelsOnGround > 0 : in.kind != kHeli;
    if (mv2 < 0.04f && grounded) {   // slow: never look up from under the edge of the vehicle
        float side[3] = {in.fwd[1], -in.fwd[0], 0.0f};
        const float sl = std::sqrt(side[0] * side[0] + side[1] * side[1]);
        if (sl > 1e-5f) { side[0] /= sl; side[1] /= sl; }
        float upv[3] = {side[1] * in.fwd[2] - side[2] * in.fwd[1], side[2] * in.fwd[0] - side[0] * in.fwd[2], side[0] * in.fwd[1] - side[1] * in.fwd[0]};
        const float ul = std::sqrt(upv[0] * upv[0] + upv[1] * upv[1] + upv[2] * upv[2]);
        if (ul > 1e-5f) for (float& v : upv) v /= ul;
        if (upv[0] * front[0] + upv[1] * front[1] + upv[2] * front[2] > 0.0f) {
            const float hgt = (T[2] - in.pos[2]) - in.colMin[2];   // plus the height above the road
            const float rel = m_beta - (heading - 0.5f * kPi);
            const float a = std::asin(std::fabs(std::sin(rel)));
            const float edge = a <= std::atan2(in.colMax[0], -in.colMin[1]) ? (1.5f - in.colMin[1]) / std::cos(a)
                                                                              : (1.2f + in.colMax[0]) / std::cos(0.5f * kPi - a);
            maxUp = std::atan2(hgt, edge * 1.2f) + std::atan2(in.fwd[2], std::sqrt(in.fwd[0] * in.fwd[0] + in.fwd[1] * in.fwd[1])) * std::cos(rel);
            const float spin = in.tv[0] * in.fwd[0] + in.tv[1] * in.fwd[1] + in.tv[2] * in.fwd[2];
            if (in.kind == kCar && in.wheelsOnGround > 1 && std::fabs(spin) < 0.05f)
                maxUp += std::atan2(in.right[2], std::sqrt(in.right[0] * in.right[0] + in.right[1] * in.right[1])) * std::cos(rel + 0.5f * kPi);
        }
    }
    maxUp = std::max(maxUp, -s[14]);
    alphaTarget = std::clamp(alphaTarget, -s[14], maxUp);
    const float alphaStepMax = ts * s[6];
    const float alphaStep = std::clamp((alphaTarget - m_alpha) * (1.0f - std::pow(s[5], ts)), -alphaStepMax, alphaStepMax);

    // Mouse look moves the angles itself and keeps control for 50 steps after it stops.
    float padBeta = 0.0f, padAlpha = 0.0f;
    bool mouse = false;
    if (in.lookYaw != 0.0f || in.lookPitch != 0.0f) {
        padBeta = in.lookYaw; padAlpha = in.lookPitch;
        m_betaSpeed = m_alphaSpeed = 0.0f;
        alphaTarget = m_alpha;
        m_mouseHold = 50.0f;
        mouse = true;
    } else if (m_mouseHold > 0.0f) {
        m_betaSpeed = m_alphaSpeed = 0.0f;
        alphaTarget = m_alpha;
        m_mouseHold = std::max(0.0f, m_mouseHold - ts);
        mouse = true;
    }
    const float damp = std::pow(s[8], ts);
    const float bTarget = std::clamp(betaTarget, -s[9], s[9]);
    m_betaSpeed = damp * m_betaSpeed + (1.0f - damp) * bTarget;
    if (std::fabs(m_betaSpeed) < 1e-4f) m_betaSpeed = 0.0f;
    m_beta += mouse ? padBeta : ts * m_betaSpeed;
    while (m_beta > kPi) m_beta -= 2.0f * kPi;   // ClipBeta
    while (m_beta < -kPi) m_beta += 2.0f * kPi;

    if (!mouse && in.kind <= kBike && alphaTarget < m_alpha && dist <= hist1Dist && in.wheelsOnGround > 1)
        padAlpha += (alphaTarget - m_alpha) * 0.075f;   // settle down onto the slope
    m_alphaSpeed = damp * m_alphaSpeed + (1.0f - damp) * padAlpha;
    const float aMax = padAlpha > 0.0f ? s[9] * 0.5f : s[9];
    m_alphaSpeed = std::clamp(m_alphaSpeed, -aMax, aMax);
    if (std::fabs(m_alphaSpeed) < 1e-4f) m_alphaSpeed = 0.0f;
    if (mouse) { alphaTarget += padAlpha; m_alpha += padAlpha; }
    else { alphaTarget += ts * m_alphaSpeed; m_alpha += alphaStep; }
    if (m_alpha > maxUp) { m_alpha = maxUp; m_alphaSpeed = 0.0f; }
    else if (m_alpha < -s[14]) { m_alpha = -s[14]; m_alphaSpeed = 0.0f; }

    FrontOf(m_beta, m_alpha, out.dir);
    float eye[3];
    for (int k = 0; k < 3; k++) eye[k] = T[k] - dist * out.dir[k];
    {
        float hd[3];
        FrontOf(m_beta, alphaTarget + alphaOff, hd);
        for (int k = 0; k < 3; k++) { m_hist[0][k] = T[k] - lag * hd[k]; m_hist[1][k] = T[k] - dist * hd[k]; }
    }

    // CCamera::CameraColDetAndReact: pulled in at once in front of what is hit, easing back out.
    float allowed = dist;
    if (in.ray) {
        const float f = in.ray(T, eye);
        if (f < 1.0f) allowed = std::max(0.3f, f * dist - 0.3f);
    }
    if (m_colDist < 0.0f || allowed < m_colDist) m_colDist = allowed;
    else m_colDist += Approach(4.0f, in.dt) * (allowed - m_colDist);
    const float use = std::min(m_colDist, dist);
    for (int k = 0; k < 3; k++) out.eye[k] = T[k] - use * out.dir[k];
    out.nearClip = use < 3.0f ? 0.15f : 0.5f;   // CCamera::ImproveNearClip
    out.fovScale = std::tan(m_fov * 0.5f * kPi / 180.0f) / std::tan(35.0f * kPi / 180.0f);
    return out;
}
