#pragma once
// Camera shake. The native producers (explosions, crashes) set a force and a
// start time (CCamera::m_fCamShakeForce / m_nCamShakeStart). The rewrite
// replaces the frame-random positional shake with a smooth seeded noise:
// the force decays at 0.28 per second, is clamped to 0..2 and eased, then
// moves the camera along its right, forward and up axes.

#include <cstdint>

class CameraShake {
public:
    struct Params {
        float intensity = 1.25f;   // scales the eased force
        float amplitude = 1.1f;    // scales the final displacement
        float speed = 17.0f;       // noise cycles per second
    };

    // CamShakeNoPos: a new shake only wins over a weaker running one (native
    // decay is 1.0 per second for this comparison).
    void Trigger(float strength, uint32_t nowMs);
    // CamShake with a source: strength falls off linearly over 100 m and is
    // scaled by 0.35.
    void TriggerAt(float strength, float distance, uint32_t nowMs);

    // timestep in 1/50 s units (CTimer::ms_fTimeStep). Axes are the camera's
    // unit right, forward and up. offset receives the world displacement.
    void Update(uint32_t nowMs, float timestep, const float right[3], const float forward[3],
                const float up[3], float offset[3]);

    float Level() const { return m_level; }
    Params params;

private:
    float m_force = 0.0f;
    uint32_t m_start = 0;
    float m_level = 0.0f;     // eased force
    uint32_t m_lastMs = 0;
    bool m_active = false;
};
