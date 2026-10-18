#pragma once
// On-foot player camera. V cycles three views; the camera style setting picks
// which three.
//
// Classic: CCam::Process_FollowPed_SA zooms (CCamera::m_nPedZoom 1..3). The
// look-at point is the ped centre plus PEDCAM_SET[0] (0.6 m); the distance is
// PEDCAM_SET[1] (2.0 m) plus the zoom value (-0.55 / 1.5 / 3.6), eased at
// 0.12 per 1/50 s timestep. The mouse aims it; it is pulled in by collision.
//
// Rewrite: first person from CJ's eyes (his body stays visible, the head is
// hidden), then left and right shoulder views. The shoulder views follow an
// anchor on CJ's spine that lags him per camera axis (lateral, approaching,
// receding, vertical) within limits, sit off to the side and slightly above,
// and pull in on collision from five rays (centre and +-radius on two axes),
// easing back out.

#include <functional>

class PlayerCamera {
public:
    enum Style { kClassic = 0, kRewrite = 1 };

    struct Input {
        float dt = 0.0f;
        int style = kClassic;
        float yaw = 0.0f, pitch = 0.0f;     // view direction (radians)
        float feet[3] = {0, 0, 0};          // CJ's ground contact point
        bool haveSpine = false;
        float spine[3] = {0, 0, 0};         // bone 3 (spine1), world
        bool haveHead = false;
        float head[3] = {0, 0, 0};          // bone 5 (head), world
        // Collision: fraction of a -> b that is clear (1 = nothing hit).
        std::function<float(const float a[3], const float b[3])> ray;
        bool climbing = false;   // on a ledge: the ledge itself must not pull the camera in
        // classic MODE_AIMWEAPON (CCam::Process_AimWeapon): aiming a gun on foot
        bool aimWeapon = false;
        float aimFov = 70.0f;     // the weapon's FOV (70; AK-47 / M4 50; country rifle 35), retail degrees
        float pedHeading = 0.0f;  // radians, the direction CJ faces
    };
    struct Output {
        float eye[3] = {0, 0, 0};
        float dir[3] = {1, 0, 0};           // unit look direction
        float nearClip = 0.5f;
        bool hideHead = false;              // first person: CJ's head would fill the screen
        bool firstPerson = false;
        float fovScale = 1.0f;              // tan(fov / 2) relative to the normal 70 degrees
    };

    // V: next view (wraps over three).
    void Cycle() { m_view = (m_view + 1) % 3; }
    int View() const { return m_view; }
    void SetView(int v) { m_view = ((v % 3) + 3) % 3; }
    bool FirstPerson(int style) const { return style == kRewrite && m_view == 0; }
    static const char* ViewName(int style, int view) {
        static const char* kNames[2][3] = {{"zoom 1", "zoom 2", "zoom 3"},
                                           {"first person", "left shoulder", "right shoulder"}};
        return kNames[style == kRewrite ? 1 : 0][((view % 3) + 3) % 3];
    }

    Output Update(const Input& in);

private:
    int m_view = 1;                         // classic: zoom 2; rewrite: left shoulder
    int m_lastStyle = -1;
    // classic
    float m_zoomSmoothed = 1.5f;
    float m_aimBlend = 0.0f;                // follow -> aim camera transition, 0..1
    float m_aimFov = 70.0f;                 // CCam::m_fFOV while aiming, eased 1 degree per 1/50 s
    // rewrite shoulder cameras
    bool m_anchorValid = false;
    float m_anchor[3] = {0, 0, 0};          // lagged anchor
    float m_side = 0.45f;                   // current shoulder offset (+ right)
    float m_dist = 2.3f;
    float m_colDist = -1.0f;                // collision-limited distance (eases out)
    float m_climbGrace = 0.0f;              // seconds left of looking past the ledge just climbed
    // first person
    float m_eyeZ = 0.0f;
    bool m_eyeValid = false;
};
