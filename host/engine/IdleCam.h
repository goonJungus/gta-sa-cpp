// CIdleCam and CHandShaker: ninety seconds without touching the controls and the on-foot camera stays where it
// is but turns to watch something, eased over two seconds; a target over 15 m away gets a slow FOV zoom from
// 70 to 15 (at most every 12 s), and a hand-held shake builds up over three seconds. Any input gives the
// camera back. The target is the player, or the best of the CInterestingEvents once it has looked at the
// last one for five seconds (and is zoomed out): a cop, an explosion, a fight, a car crash...
#pragma once

#include <cstdint>
#include <functional>

#include "InterestingEvents.h"

class IdleCam {
public:
    IdleCam();   // CIdleCam::Init, CHandShaker::SetDefaults

    struct Target {
        bool valid = false;
        float pos[3] = {};   // the entity's position
        bool ped = false;    // GetLookAtPositionOnTarget: a ped is looked at half a metre up
        float lookUp = 0.5f; // (a woman or a prostitute a tenth)
        bool player = false;
        InterestingEvents::Ref ref;   // the event entity this is, for a target that is not the player
    };
    // line of sight from a to b is clear (ignoring the target)
    using LosFn = std::function<bool(const float*, const float*)>;

    // Once a frame in the follow-ped camera. dtMs: the frame in ms; touched: any control moved this frame.
    // eye: the camera position the follow camera chose. Returns true while the idle camera owns the view, with
    // front (normalised) and fov (degrees) filled in.
    // events: where the interesting events are kept and what the world looks like (null: only the player).
    bool Process(float dtMs, bool touched, const float eye[3], const float curFront[3], const Target& player,
                 const LosFn& los, float front[3], float& fov, InterestingEvents* events = nullptr,
                 const InterestingEvents::Frame* frame = nullptr);
    // CIdleCam::IdleCamGeneralProcess outside the follow-ped mode: the idle time keeps counting but the
    // camera starts afresh next time.
    void NotFollowingPed() { m_active = false; }
    void SetKickInMs(float ms) { m_kickIn = ms; }
    const Target& Current() const { return m_target; }
    bool Active() const { return m_active; }

private:
    struct Shaker {   // CHandShaker
        float ang[3] = {}, vel[3] = {};
        float lim[3] = {0.02f, 0.02f, 0.01f}, motion[3] = {0.0002f, 0.0002f, 0.0001f}, slow[3] = {1.3f, 1.3f, 1.4f};
        float reactMin = 0.3f, reactMax = 1.0f;
        int twitchFreq = 20;
        float twitchVel = 0.001f;
        void Reset();
        void Process(float dtMs, float degree, float m[3][3]);
    };

    void Reset();
    void SetTarget(const Target& t, const float eye[3], const float curFront[3]);
    void LookAt(const Target& t, float out[3]) const;
    void ProcessFOVZoom(float slerpT, const float eye[3], const LosFn& los);
    bool IsTargetValid(const Target& t, const float eye[3], const LosFn& los);
    void ProcessTargetSelection(const Target& player, const float eye[3], const float curFront[3], const LosFn& los,
                                InterestingEvents* events, const InterestingEvents::Frame* frame);

    // tuning (CIdleCam::Init)
    float m_distStartFOVZoom = 15.0f, m_zoomNearest = 15.0f, m_zoomFarthest = 70.0f, m_slerpDuration = 2000.0f;
    float m_kickIn = 90000.0f, m_degreeShake = 1.0f, m_shakeBuildUp = 3000.0f, m_durationFOVZoom = 1000.0f;
    float m_timeBeforeNewZoomIn = 12000.0f;
    float m_distTooClose = 4.0f, m_distTooFar = 80.0f, m_timeMinLookAtSomething = 5000.0f, m_zoomedInTimeFactor = 2.0f;
    int m_losFramesToReject = 14;

    float m_now = 0.0f;                  // ms
    float m_idleMs = 0.0f;               // m_IdleTickerFrames
    bool m_active = false;               // processed last frame
    Target m_target;
    float m_slerpFrom[3] = {}, m_lastIdlePos[3] = {};
    float m_timeStarted = -1.0f, m_timeTargetSelected = -1.0f, m_timeZoomStarted = -1.0f, m_timeLastZoomIn = -1.0f;
    float m_zoomFrom = -1.0f, m_zoomTo = -1.0f, m_curFov = 70.0f, m_slerpTime = 1.0f;
    int m_zoomState = 3;                 // 0 zooming in, 1 zooming out, 2 zoomed in, 3 zoomed out
    bool m_forceZoomOut = false, m_hasZoomedIn = false;
    int m_losCounter = 0;
    Shaker m_shaker;
};
