#pragma once
// "Rewrite" on-foot handling, layered over the retail player controller.
//
//   Momentum         the move stick is filtered before CPlayerPed reads it:
//                    acceleration per gait, braking, reversal braking, a turn
//                    rate that falls with speed and lateral grip.
//   MovementHeading  CJ's facing follows a damped turn velocity instead of
//                    the retail 450 deg/s.
//   Lean / Pitch     the whole body rolls into turns and tips with
//                    acceleration and braking, pivoting at the ped
//                    centre; the lower legs are counter-rotated so the feet
//                    stay planted and the head follows a little
//                    further.
//
// Angles are in degrees; responses are exponential rates
// (1 - e^(-response * dt)).

struct MovementParams {
    // [Momentum]
    float inputDeadZone = 0.08f, stopEpsilon = 0.015f;
    // Thin momentum layer: GTA SA's own move-blend-ratio ramp governs how fast
    // CJ speeds up, so forward feel stays 1:1 with retail.
    float walkAcceleration = 5.0f, runAcceleration = 6.0f, sprintAcceleration = 6.0f;
    float braking = 7.0f, reversalBraking = 6.0f;
    float reversalAngleDegrees = 115.0f, reversalSwitchSpeed = 0.12f;
    float lowSpeedTurnRate = 540.0f, highSpeedTurnRate = 300.0f, highSpeedTurnStart = 0.60f;
    float lateralGrip = 0.93f;
    // [MovementHeading]
    float headingResponse = 6.8f, headingVelocityResponse = 14.0f;
    float maxHeadingSpeed = 245.0f, minimumMovementInput = 0.05f;
    // [Lean]
    float maxLeanDegrees = 10.0f, leanInResponse = 5.5f, leanOutResponse = 5.0f;
    float inputContribution = 0.30f, turnContribution = 0.40f, angularVelocityContribution = 1.00f;
    float fullTurnAngleDegrees = 42.0f, fullTiltTurnSpeed = 120.0f;
    // [Pitch]
    float movementEnvelopeResponse = 3.0f, pitchResponse = 7.0f;
    float maxAccelerationPitch = 10.0f, maxBrakingPitch = 7.0f;
    // [FootPlant] / [HeadFollow]
    float bankCompensation = 1.0f, pitchCompensation = 1.0f;
    float headLeanInfluence = 0.35f, headPitchInfluence = 0.30f;
    // [General]
    float minimumVisiblePoseDegrees = 0.02f;
};

class Movement {
public:
    MovementParams p;

    void Reset();

    // Momentum. stickX/stickY: the requested move, world space, length 0..1;
    // replaced by the filtered stick. walk/sprint pick the acceleration.
    // blocked: CJ walked into a wall.
    void FilterStick(float dt, float& stickX, float& stickY, bool walk, bool sprint, bool blocked);

    // MovementHeading: returns CJ's new facing (radians). aim = where CPlayerPed
    // wants to face; active = the stick is pushed or CJ is moving.
    float Heading(float dt, float current, float aim, bool active);

    // Lean / Pitch targets. stickRight: stick component to CJ's right (-1..1);
    // stickMag: 0..1; ratio: CPlayerPedData::m_fMoveBlendRatio; moving: CJ's
    // ground speed is at least 0.12 m/s.
    void UpdateBody(float dt, float stickRight, float stickMag, float ratio, float turnError, bool moving);

    float Lean() const { return m_lean; }     // degrees, about CJ's forward axis
    float Pitch() const { return m_pitch; }   // degrees, about CJ's right axis

private:
    // momentum
    float m_speed = 0.0f;               // filtered stick length
    float m_dirX = 0.0f, m_dirY = 0.0f; // filtered stick direction
    // heading
    bool m_headingValid = false;
    float m_heading = 0.0f, m_headingVel = 0.0f;   // radians, rad/s
    float m_turnRateDeg = 0.0f;                    // last turn velocity, deg/s
    // body
    float m_lean = 0.0f, m_envelope = 0.0f, m_pitch = 0.0f;
};
