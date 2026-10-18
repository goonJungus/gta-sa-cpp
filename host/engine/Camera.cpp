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
