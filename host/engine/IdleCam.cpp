#include "IdleCam.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
constexpr float kPi = 3.14159265f;
constexpr float kDeg = 0.017453292f;

float Rand01() { return (float)std::rand() / (float)RAND_MAX; }   // CGeneral::GetRandomNumber() * 3.051851e-05

// sin(270 - t * 180) eased 0..1 (the idle camera's slerp and zoom curve)
float Ease(float t) { return (std::sin((270.0f - t * 180.0f) * kDeg) + 1.0f) * 0.5f; }

float WrapAround(float a, float ref) {   // a within pi of ref
    if (a - ref > kPi) a -= 2.0f * kPi;
    else if (a - ref < -kPi) a += 2.0f * kPi;
    return a;
}
}

IdleCam::IdleCam() { m_shaker.Reset(); Reset(); }

void IdleCam::Shaker::Reset() {
    for (int k = 0; k < 3; k++) { ang[k] = 0.0f; vel[k] = Rand01() * motion[k]; }
}

// CHandShaker::Process: each axis swings within its limit, pushed back toward the middle (harder near the
// limit, harder still while moving outward), with the odd random twitch; the result is a rotation.
void IdleCam::Shaker::Process(float dtMs, float degree, float m[3][3]) {
    for (int k = 0; k < 3; k++) {
        float react = std::fabs(ang[k] / lim[k]) * (reactMax - reactMin) + reactMin;
        if ((ang[k] > 0.0f && vel[k] > 0.0f) || (ang[k] < 0.0f && vel[k] < 0.0f)) react *= slow[k];
        float push = Rand01() * motion[k] * react;
        if (ang[k] > 0.0f) push = -push;
        vel[k] += push;
    }
    const float step = dtMs * 0.03f;
    if (twitchFreq > 0 && std::rand() % twitchFreq == 1)
        for (int k = 0; k < 3; k++) vel[k] += Rand01() * 2.0f * twitchVel - twitchVel;
    for (int k = 0; k < 3; k++) ang[k] = std::clamp(ang[k] + step * vel[k], -lim[k], lim[k]);
    // CMatrix::SetRotate(x, y, z): rows right, up (forward), at
    const float x = degree * ang[0], y = degree * ang[1], z = degree * ang[2];
    const float cX = std::cos(x), sX = std::sin(x), cY = std::cos(y), sY = std::sin(y), cZ = std::cos(z), sZ = std::sin(z);
    const float r[3][3] = {{cZ * cY - sZ * sX * sY, cZ * sX * sY + sZ * cY, -sY * cX},
                           {-sZ * cX, cZ * cX, sX},
                           {cZ * sY + sZ * sX * cY, sZ * sY - cZ * sX * cY, cY * cX}};
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m[i][j] = r[i][j];
}

void IdleCam::Reset() {   // CIdleCam::Reset
    for (int k = 0; k < 3; k++) m_slerpFrom[k] = m_lastIdlePos[k] = 0.0f;
    m_timeStarted = m_timeTargetSelected = m_timeZoomStarted = m_timeLastZoomIn = -1.0f;
    m_zoomFrom = m_zoomTo = -1.0f;
    m_target = Target();
    m_zoomState = 3;
    m_forceZoomOut = false;
    m_curFov = 70.0f;
    m_losCounter = 0;
    m_hasZoomedIn = false;
    m_slerpTime = 1.0f;
}

void IdleCam::LookAt(const Target& t, float out[3]) const {   // GetLookAtPositionOnTarget
    out[0] = t.pos[0]; out[1] = t.pos[1]; out[2] = t.pos[2] + (t.ped ? t.lookUp : 0.0f);
}

void IdleCam::SetTarget(const Target& t, const float eye[3], const float curFront[3]) {   // CIdleCam::SetTarget
    if (!m_target.valid) for (int k = 0; k < 3; k++) m_slerpFrom[k] = eye[k] + curFront[k];   // from where the camera looks now
    else for (int k = 0; k < 3; k++) m_slerpFrom[k] = m_lastIdlePos[k];
    m_target = t;
    m_timeTargetSelected = m_now;
    m_losCounter = 0;
    m_hasZoomedIn = false;
}

// CIdleCam::ProcessFOVZoom: zoom in on a far target the camera can see, out again when it comes close
// (or the target changes), on the sin(270 - 180 t) curve over a second.
void IdleCam::ProcessFOVZoom(float slerpT, const float eye[3], const LosFn& los) {
    const float nearest = m_zoomNearest;
    bool far = false;
    float at[3] = {};
    if (m_target.valid) {
        LookAt(m_target, at);
        const float d = std::sqrt((eye[0] - at[0]) * (eye[0] - at[0]) + (eye[1] - at[1]) * (eye[1] - at[1]) + (eye[2] - at[2]) * (eye[2] - at[2]));
        far = d > m_distStartFOVZoom;
    }
    if (slerpT >= 1.0f) {
        const int was = m_zoomState;
        if (far) {
            if (m_now - m_timeLastZoomIn > m_timeBeforeNewZoomIn) {
                const bool clear = !m_target.valid || !los || los(eye, at);
                if (m_losCounter > 10 && m_zoomState == 2) m_zoomState = 1;
                if (m_zoomState == 3 && !m_hasZoomedIn && clear) {
                    m_zoomState = 0;
                    m_zoomTo = nearest;
                    if (was != 0) { m_timeZoomStarted = m_now; m_zoomFrom = m_curFov; }
                }
            }
        } else if (m_zoomState == 2) {
            m_zoomTo = m_zoomFarthest;
            m_zoomState = 1;
            m_timeZoomStarted = m_now; m_zoomFrom = m_curFov;
        }
    }
    if (m_zoomState == 2) m_timeLastZoomIn = m_now;
    if (m_forceZoomOut && m_zoomState == 2) {
        m_timeZoomStarted = m_now; m_zoomFrom = m_curFov;
        m_zoomState = 1; m_zoomTo = m_zoomFarthest;
    }
    m_forceZoomOut = false;
    auto move = [&] { m_curFov = (m_zoomTo - m_zoomFrom) * Ease((m_now - m_timeZoomStarted) / m_durationFOVZoom) + m_zoomFrom; };
    switch (m_zoomState) {
    case 0:
        if (std::fabs(m_curFov - nearest) < 1.0f) { m_zoomState = 2; m_hasZoomedIn = true; m_curFov = nearest; }
        else move();
        break;
    case 1:
        if (std::fabs(m_curFov - m_zoomFarthest) < 1.0f) { m_zoomState = 3; m_curFov = m_zoomFarthest; }
        else move();
        break;
    case 2: m_curFov = nearest; break;
    default: m_curFov = m_zoomFarthest; break;
    }
}

// CIdleCam::IsTargetValid: the player always; anything else 4 to 80 m from the camera, and once the turn
// toward it is done in clear sight (it may be hidden for 14 frames before it is given up on).
bool IdleCam::IsTargetValid(const Target& t, const float eye[3], const LosFn& los) {
    if (!t.valid) return false;
    if (t.player) return true;
    float at[3];
    LookAt(t, at);
    const float dx = eye[0] - at[0], dy = eye[1] - at[1], dz = eye[2] - at[2];
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (d < m_distTooClose || d > m_distTooFar) return false;
    if (m_slerpTime >= 1.0f && los && !los(eye, at) && m_losCounter++ > m_losFramesToReject) return false;
    return true;
}

// CIdleCam::ProcessTargetSelection
void IdleCam::ProcessTargetSelection(const Target& player, const float eye[3], const float curFront[3], const LosFn& los,
                                     InterestingEvents* events, const InterestingEvents::Frame* frame) {
    float since = m_now - m_timeTargetSelected;
    if (m_zoomState != 3 && m_losCounter < 1) since /= m_zoomedInTimeFactor;
    const auto toPlayer = [&] { SetTarget(player, eye, curFront); m_forceZoomOut = true; };
    if (since > m_timeMinLookAtSomething && events && frame) {
        events->InvalidateNonVisible(*frame);
        const InterestingEvents::Event* ev = events->Get(frame->now);
        bool done = false;
        if (!ev) {
            if ((m_target.valid && IsTargetValid(m_target, eye, los)) || m_target.player) done = true;
            else if (m_zoomState == 3) { toPlayer(); done = true; }
        } else if (m_target.valid && m_target.ref == ev->ref) {
            if (!IsTargetValid(m_target, eye, los)) events->Invalidate(ev);
            done = true;
        } else {
            Target nt;
            nt.valid = frame->resolve && frame->resolve(ev->ref, nt.pos, &nt.ped, &nt.lookUp);
            nt.ref = ev->ref;
            if (!IsTargetValid(nt, eye, los)) done = true;
            else if (m_zoomState == 3) { SetTarget(nt, eye, curFront); done = true; }
        }
        if (!done) m_forceZoomOut = true;   // zoom out first, then it looks at the new thing
    }
    if (!m_target.valid) { m_forceZoomOut = true; toPlayer(); }
    if (!IsTargetValid(m_target, eye, los) && since > m_timeMinLookAtSomething) {
        if (m_zoomState == 3) SetTarget(player, eye, curFront);
        m_forceZoomOut = true;
        if (m_losCounter > 0) toPlayer();
    }
    if (m_losCounter > m_losFramesToReject) toPlayer();
}

bool IdleCam::Process(float dtMs, bool touched, const float eye[3], const float curFront[3], const Target& player,
                      const LosFn& los, float front[3], float& fov, InterestingEvents* events,
                      const InterestingEvents::Frame* frame) {
    m_now += dtMs;
    // CIdleCam::ProcessIdleCamTicker: the time since the pad last changed
    if (touched) m_idleMs = 0.0f; else m_idleMs += dtMs;
    if (m_idleMs <= m_kickIn) { m_active = false; return false; }
    if (!m_active) {   // it starts: from where the camera looks, onto the player, zoomed out
        Reset();
        m_timeStarted = m_now;
        SetTarget(player, eye, curFront);
        m_forceZoomOut = true;
        m_shaker.Reset();
    }
    m_active = true;

    // the target as it is now: the player where he stands, an event entity where it is (gone: no target)
    if (m_target.valid && m_target.player) m_target = player;
    else if (m_target.valid && frame && frame->resolve) {
        const InterestingEvents::Ref r = m_target.ref;
        m_target.valid = frame->resolve(r, m_target.pos, &m_target.ped, &m_target.lookUp);
    }
    ProcessTargetSelection(player, eye, curFront, los, events, frame);

    const float shake = std::min(1.0f, (m_now - m_timeStarted) / m_shakeBuildUp);

    // CIdleCam::ProcessSlerp: pitch and heading from the old look point to the target, eased
    float at[3];
    LookAt(m_target, at);
    const float fx = m_slerpFrom[0] - eye[0], fy = m_slerpFrom[1] - eye[1], fz = m_slerpFrom[2] - eye[2];
    const float tx = at[0] - eye[0], ty = at[1] - eye[1], tz = at[2] - eye[2];
    const float yawFrom = std::atan2(fy, fx) + kPi;
    const float pitchFrom = std::atan2(fz, std::sqrt(fx * fx + fy * fy));
    const float yawTo = WrapAround(std::atan2(ty, tx) + kPi, yawFrom);
    const float pitchTo = WrapAround(std::atan2(tz, std::sqrt(tx * tx + ty * ty)), pitchFrom);
    const float t = std::min(1.0f, (m_now - m_timeTargetSelected) / m_slerpDuration);
    const float e = Ease(t);
    const float pitch = (pitchTo - pitchFrom) * e + pitchFrom;
    const float yaw = (yawTo - yawFrom) * e + yawFrom;
    m_slerpTime = t;

    ProcessFOVZoom(t, eye, los);

    // CIdleCam::FinaliseIdleCamera: the front from the angles, then the hand shake turns it (up stays world up)
    float f[3] = {-std::cos(yaw) * std::cos(pitch), -std::sin(yaw) * std::cos(pitch), std::sin(pitch)};
    for (int k = 0; k < 3; k++) m_lastIdlePos[k] = eye[k] + f[k];
    float m[3][3];
    m_shaker.Process(dtMs, shake * m_degreeShake, m);
    for (int j = 0; j < 3; j++) front[j] = m[0][j] * f[0] + m[1][j] * f[1] + m[2][j] * f[2];
    const float l = std::sqrt(front[0] * front[0] + front[1] * front[1] + front[2] * front[2]);
    if (l > 1e-6f) for (int k = 0; k < 3; k++) front[k] /= l;
    if (front[0] == 0.0f && front[1] == 0.0f) front[0] = front[1] = 0.0001f;
    fov = m_curFov;
    return true;
}
