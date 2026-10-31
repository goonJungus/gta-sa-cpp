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
        bool scope = false;       // MODE_SNIPER / MODE_CAMERA: first person through a scope
        float scopeFov = 70.0f;   // its zoomed FOV, retail degrees
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
    float m_aimFrom = 0.0f, m_aimT = 1e3f;  // blend at the last switch, seconds since it
    bool m_aimIn = false;
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

// In-vehicle chase camera, CCam::Process_FollowCar_SA. It looks at the vehicle
// from the zoom distance (CCamera::m_fCarZoomSmoothed + CARCAM_SET + twice the
// COL box's rear length) and swings in behind the direction of travel: a lag
// point 15 m back along last frame's view stands in for the camera history, and
// the beta / alpha speeds chase it (CARCAM_SET rates, damped by pow(0.85, ts)).
// Mouse look moves the angles directly and holds them for a second. Fast
// driving widens the FOV up to 100 degrees.
class CarCamera {
public:
    // CARCAM_SET rows (0x8cc600).
    enum Kind { kCar = 0, kBike = 1, kHeli = 2, kPlane = 3, kBoat = 4, kRc = 5 };

    struct Input {
        float dt = 0.0f;
        int kind = kCar;
        int arrPos = 0;                 // CCamera::GetArrPosForVehicleType: 0 car, 1 bike, 2 heli, 3 boat, 4 plane
        float pos[3] = {0, 0, 0};       // the vehicle
        float right[3] = {1, 0, 0}, fwd[3] = {0, 1, 0}, up[3] = {0, 0, 1};
        float mv[3] = {0, 0, 0};        // m_vecMoveSpeed, metres per 1/50 s
        float tv[3] = {0, 0, 0};        // m_vecTurnSpeed
        float colMin[3] = {-1, -2.5f, -0.5f}, colMax[3] = {1, 2.5f, 1};   // COL bounding box
        int wheelsOnGround = 4;
        float lookYaw = 0.0f, lookPitch = 0.0f;   // mouse / stick look this frame (radians)
        bool reset = false;                       // just got in: start behind the vehicle
        std::function<float(const float a[3], const float b[3])> ray;
    };

    // V: zoom 1 -> 2 -> 3 (CCamera::m_nCarZoom).
    void Cycle() { m_zoom = m_zoom % 3 + 1; }
    int Zoom() const { return m_zoom; }

    PlayerCamera::Output Update(const Input& in);
    // The view as yaw / pitch (the on-foot convention), for whoever takes over next.
    float Yaw() const { return m_beta + 3.14159265f; }
    float Pitch() const { return m_alpha; }

private:
    int m_zoom = 2;                     // CCamera::m_nCarZoom
    float m_zoomSmoothed = -1e9f;       // m_fCarZoomSmoothed
    float m_beta = 0.0f, m_alpha = 0.0f;           // m_fHorizontalAngle / m_fVerticalAngle
    float m_betaSpeed = 0.0f, m_alphaSpeed = 0.0f;
    float m_fov = 70.0f;
    float m_hist[2][3] = {};            // m_avecTargetHistoryPos[0] (lag point), [1] (camera distance)
    float m_mouseHold = 0.0f;           // timesteps the mouse keeps control (0xb70118)
    float m_colDist = -1.0f;
};
