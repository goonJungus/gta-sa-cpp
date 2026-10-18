// Rewrite on-foot handling. See Movement.h.

#define NOMINMAX
#include "Movement.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265f;
constexpr float kDeg = kPi / 180.0f;

float Wrap(float a) {
    while (a > kPi) a -= 2 * kPi;
    while (a < -kPi) a += 2 * kPi;
    return a;
}

// 1 - e^(-response * dt): frame-rate-independent smoothing.
float Approach(float response, float dt) {
    return dt > 0.0f ? 1.0f - std::exp(-response * dt) : 0.0f;
}

}  // namespace

void Movement::Reset() {
    m_speed = m_dirX = m_dirY = 0.0f;
    m_headingValid = false;
    m_headingVel = m_turnRateDeg = 0.0f;
    m_lean = m_envelope = m_pitch = 0.0f;
}

void Movement::FilterStick(float dt, float& stickX, float& stickY, bool walk, bool sprint, bool blocked) {
    const float mag = std::min(1.0f, std::sqrt(stickX * stickX + stickY * stickY));
    const float dz = std::clamp(p.inputDeadZone, 0.0f, 0.95f);
    float target = 0.0f, tx = 0.0f, ty = 0.0f;
    if (mag > dz) {
        const float len = std::sqrt(stickX * stickX + stickY * stickY);
        tx = stickX / len;
        ty = stickY / len;
        target = std::clamp((mag - dz) / (1.0f - dz), 0.0f, 1.0f);
    }

    // A filtered direction that went bad, or a standing start, takes the stick's.
    const float dirLen2 = m_dirX * m_dirX + m_dirY * m_dirY;
    if (target > 0.001f && (dirLen2 < 0.5f * 0.5f || m_speed <= 0.001f)) {
        m_dirX = tx;
        m_dirY = ty;
    }
    float angle = 0.0f;   // signed, radians, filtered direction -> stick
    if (target > 0.001f) {
        const float dot = std::clamp(tx * m_dirX + ty * m_dirY, -1.0f, 1.0f);
        const float cross = m_dirX * ty - m_dirY * tx;
        angle = std::atan2(cross, dot);
    }

    const float switchSpeed = std::clamp(p.reversalSwitchSpeed, 0.0f, 1.0f);
    const bool reversal = target > 0.001f && m_speed > switchSpeed &&
                          std::fabs(angle) / kDeg >= p.reversalAngleDegrees;
    if (reversal) {
        m_speed = std::max(0.0f, m_speed - std::max(0.0f, p.reversalBraking) * dt);
    } else if (target > m_speed + 0.001f) {
        const float acc = walk ? p.walkAcceleration : sprint ? p.sprintAcceleration : p.runAcceleration;
        m_speed = std::min(target, m_speed + std::max(0.0f, acc) * dt);
    } else if (target < m_speed - 0.001f) {
        m_speed = std::max(target, m_speed - std::max(0.0f, p.braking) * dt);
    }

    float step = 0.0f;
    if (target > 0.001f) {
        if (!reversal || m_speed > switchSpeed) {
            const float start = std::clamp(p.highSpeedTurnStart, 0.0f, 0.99f);
            const float t = std::clamp((m_speed - start) / (1.0f - start), 0.0f, 1.0f);
            const float lo = std::max(0.0f, p.lowSpeedTurnRate), hi = std::max(0.0f, p.highSpeedTurnRate);
            const float maxStep = ((hi - lo) * t + lo) * kDeg * dt;
            step = std::clamp(angle, -maxStep, maxStep);
            const float c = std::cos(step), s = std::sin(step);
            const float x = m_dirX;
            m_dirX = x * c - m_dirY * s;
            m_dirY = x * s + m_dirY * c;
            // Lateral grip: speed lost in proportion to how hard it turned.
            if (!reversal && std::fabs(step) > 1e-5f)
                m_speed *= std::pow(std::clamp(p.lateralGrip, 0.01f, 1.0f), std::fabs(step) / kPi);
        } else {
            m_dirX = tx;   // slowed enough in a reversal: turn round at once
            m_dirY = ty;
        }
    }
    if (target <= 0.001f && m_speed <= p.stopEpsilon) m_speed = 0.0f;
    // Pushing into a wall keeps the momentum: GTA SA keeps the legs running and
    // the ped slides along the surface (the host resolves the slide per axis).
    (void)blocked;

    stickX = m_dirX * m_speed;
    stickY = m_dirY * m_speed;
}

float Movement::Heading(float dt, float current, float aim, bool active) {
    if (!m_headingValid || !active) {
        m_heading = Wrap(current);
        m_headingValid = true;
        m_headingVel = 0.0f;
        m_turnRateDeg = 0.0f;
        if (!active) return current;
    }
    const float err = Wrap(aim - m_heading);
    const float maxW = p.maxHeadingSpeed * kDeg;
    const float wantW = std::clamp(err * p.headingResponse, -maxW, maxW);
    if (dt > 0.0f) m_headingVel += Approach(p.headingVelocityResponse, dt) * (wantW - m_headingVel);
    float step = m_headingVel * dt;
    if (err * step > 0.0f && std::fabs(err) < std::fabs(step)) {   // would overshoot: land on it
        m_headingVel = 0.0f;
        step = err;
    }
    m_heading = Wrap(m_heading + step);
    m_turnRateDeg = m_headingVel / kDeg;
    return m_heading;
}

void Movement::UpdateBody(float dt, float stickRight, float stickMag, float ratio, float turnError, bool moving, float minIntensity) {
    const float turnN = std::clamp(turnError / (p.fullTurnAngleDegrees * kDeg), -1.0f, 1.0f);
    const float velN = std::clamp(m_turnRateDeg / p.fullTiltTurnSpeed, -1.0f, 1.0f);
    const float raw = std::clamp(turnN * p.turnContribution + std::clamp(stickRight, -1.0f, 1.0f) * p.inputContribution +
                                     velN * p.angularVelocityContribution, -1.0f, 1.0f);
    // Intensity: how hard CJ is being driven (max of the stick and the
    // move blend ratio), and only while he is actually moving.
    const float intensity = moving ? std::max({std::min(stickMag, 1.0f), std::clamp(ratio, 0.0f, 1.0f), minIntensity}) : 0.0f;

    const float leanTarget = -raw * intensity * p.maxLeanDegrees;
    const float leanResp = std::fabs(m_lean) < std::fabs(leanTarget) ? p.leanInResponse : p.leanOutResponse;
    m_lean += Approach(leanResp, dt) * (leanTarget - m_lean);

    m_envelope += Approach(p.movementEnvelopeResponse, dt) * (intensity - m_envelope);
    const float d = intensity - m_envelope;   // > 0 speeding up, < 0 slowing down
    const float pitchTarget = -d * (d >= 0.0f ? p.maxAccelerationPitch : p.maxBrakingPitch);
    m_pitch += Approach(p.pitchResponse, dt) * (pitchTarget - m_pitch);
}
