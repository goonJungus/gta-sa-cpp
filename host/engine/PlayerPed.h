#pragma once
// CJ: the player ped, built and animated the way retail does it.
//
// Model: CClothesBuilder::CreateSkinnedClump assembles the player from
// player.img clothing parts (torso, head, hands, legs, feet), whose bones are
// re-indexed onto the base player.dff clump (gta3.img): that clump supplies
// the bone hierarchy and the skin-to-bone matrices. The "Normal" body variant is used
// (retail blends Ripped/Fat/Normal by the muscle and fat stats). Textures are
// composited like CClothesBuilder::ConstructTextures: the clothing texture is
// placed on top of the player's skin texture (vest over player_torso, jeans
// over player_legs).
//
// Animation: a list of blend associations, as in CAnimBlendAssociation
// (blend amount, blend delta per second, looping, delete when blended out).
// Movement clips (walk/run/sprint) advance phase-locked, as
// RpAnimBlendClumpUpdateAnimations does. CJ is moved by the root translation
// the blended clips extract (FrameUpdateCallBackWithVelocityExtraction), so
// speed builds up with the clips: there is no separate speed constant.
//
// Classic control is CTaskSimplePlayerOnFoot::PlayerControlZelda +
// CPlayerPed::SetRealMoveAnim + CTaskSimpleInAir / CTaskComplexInAirAndLand.
// Rewrite control layers momentum, damped turning and body lean on top
// (see Movement.h).

#include "D3DRenderer.h"
#include "IfpLoader.h"
#include "Movement.h"
#include "SkinnedDff.h"

#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// scan(mode, ped origin, rotation, out pos, out rotation); see ClimbScan.h.
using ClimbScanFn = std::function<bool(int, const float*, float, float*, float*)>;

class PlayerPed {
public:
    struct Input {
        float dt = 0.0f;
        float stickX = 0.0f, stickY = 0.0f;   // world-space move request, length 0..1
        bool walk = false, sprint = false;    // walk / sprint buttons held
        bool onGround = true;
        bool jumped = false;                  // a jump started this frame
        float velZ = 0.0f;                    // m/s, up positive
        float heightAboveGround = 0.0f;       // feet to the ground below (1e3 if none)
        bool noclip = false;
        bool blocked = false;                 // collision stopped CJ last frame
        bool rewrite = false;                 // rewrite handling (Movement.h)
        float camYaw = 0.0f;                  // for the rewrite heading / lean
        bool firstPerson = false;             // face the camera, move along the stick
        // Ground height at (x, y) searching down from z (rewrite foot IK);
        // returns < -1e5 when there is none.
        std::function<float(float, float, float)> groundAt;
        float worldX = 0.0f, worldY = 0.0f, feetZ = 0.0f;
        bool footIk = false;                  // rewrite foot IK on
        bool jumpHeld = false;                // CPad::GetJump
        bool jumpDown = false;                // CPad::JumpJustDown
        float waterZ = -1e9f;                 // water surface here (< -1e8: none)
        bool diveDown = false, diveUp = false; // swimming: descend / ascend held
        bool diveJust = false;                // swimming: dive pressed this frame
        bool exitPressed = false;             // CPad::GetExitVehicle (lets go)
        ClimbScanFn climbScan;
    };

    bool Init(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);
    bool Ready() const { return m_ready; }

    // CClothes fat / muscle stats, 0..1 each (the sum is capped at 1).
    void SetBodyShape(float fat, float muscle);
    void Update(const Input& in);
    // feet = world position of the ground contact point.
    void Draw(D3DRenderer& r, float x, float y, float feetZ, bool hideHead = false);

    float Heading() const { return m_heading; }   // radians, world yaw CJ faces
    void SetHeading(float h) { m_heading = h; m_aimHeading = h; }
    // CTaskSimpleClimb. The ped origin is CJ's pelvis (feet + 1.0).
    // TestForClimb: a handhold in front of CJ? launch = Shift pressed on the ground.
    bool TryStartClimb(float x, float y, float feetZ, bool launch, const ClimbScanFn& scan);
    bool Climbing() const { return m_climbing; }
    void FootDebug(char* out, size_t n) const;   // dev trace: per-leg foot IK weight and offset
    void ClimbDebug(char* out, size_t n);   // dev trace: clip time/blend and climb state
    // The task moved CJ: world x, y and feet z to snap to (valid once, true).
    bool TakeClimbSnap(float& x, float& y, float& feetZ);
    float ClimbVelZ() const { return m_cVelZ; }
    // Set when the task ended with a push (m/s) CJ keeps after letting go.
    bool TakeClimbRelease(float v[3]);
    // CTaskSimpleSwim. The host decides when CJ is deep enough (ped origin + 0.6
    // below the surface), starts the task, and moves CJ's height itself from
    // SwimSubmerge (surface swimming) or SwimVelZ (underwater).
    void StartSwim();
    void StopSwim();                       // wading out, climbing out
    void SwimSurface();                    // underwater swimmer reached the surface
    bool Swimming() const { return m_swimming; }
    bool Underwater() const { return m_swimming && m_swimState == SwimState::Under; }
    bool SwimJumpOut() const { return m_swimming && m_swimState == SwimState::BackToSurface; }
    float SwimSubmerge() const;            // how far below the surface the ped origin rides
    float SwimVelZ() const { return m_swimVelZ; }
    // Horizontal velocity CJ's animation moves him at this frame (m/s, world).
    float VelX() const { return m_velX; }
    float VelY() const { return m_velY; }
    // Landing / getting up: no jumping, no steering.
    bool Busy() const { return m_busy; }
    float MoveBlendRatio() const { return m_ratio; }
    const char* ClipName() const;
    // World position of a bone (model space -> world with the last Draw
    // placement); used by the first-person camera.
    bool BoneWorldPos(int boneId, float x, float y, float feetZ, float out[3]) const;

private:
    struct Part {
        std::vector<SkinVertex> verts;
        std::vector<SkinVertex> shape[3];   // Normal, Fat, Ripped (empty: use Normal)
        std::vector<int> boneFrame;      // part node index -> skeleton frame
        std::vector<RwMat> inverseBind;  // part node index -> inverse bind
        std::vector<MeshVertex> skinned; // per-frame output
        D3DRenderMesh* mesh = nullptr;
        bool isHead = false;
    };
    struct Pose { float q[4]; float t[3]; };
    struct Assoc {                       // CAnimBlendAssociation
        const AnimClip* clip = nullptr;
        float time = 0.0f, prevTime = 0.0f;
        float blend = 0.0f, delta = 0.0f;
        float speed = 1.0f;
        bool loop = true, movement = false, playing = true, finished = false;
        bool keepRoot = false;           // climb clips: root XY stays in the pose, not extracted
        bool dead = false;               // removed; erased at the next Advance
    };

    const AnimClip* Clip(const char* name) const;
    Assoc* Get(const AnimClip* c);
    bool Active(const AnimClip* c) { const Assoc* a = Get(c); return a && a->delta >= 0.0f; }
    Assoc* Add(const AnimClip* c, bool loop, bool movement);              // CAnimManager::AddAnimation
    Assoc* Blend(const AnimClip* c, bool loop, bool movement, float delta); // CAnimManager::BlendAnimation
    void Remove(const AnimClip* c);
    float RootY(const AnimClip* clip, float time, float* x = nullptr) const;
    void Advance(float dt);
    void BuildPose();
    void BuildGlobals(const std::vector<Pose>& pose);
    void ShoulderBoneRotation();
    void ApplyRewritePose();
    void ApplyFootIK(const Input& in);
    void FootRestHeights();
    void Skin();

    void ControlOnFoot(const Input& in);
    void SetRealMoveAnim(bool sprintHeld);
    void ControlInAir(const Input& in);
    void Land(const Input& in);
    void ControlSwim(const Input& in);
    void SwimWant(const char* name, float rate, bool movement);

    bool m_ready = false;
    std::vector<SkinFrame> m_skeleton;
    std::vector<int> m_frameOfBone;            // bone id (0..511) -> frame index
    std::vector<RwMat> m_globals;              // per frame, relative to frame 0
    std::vector<Part> m_parts;
    std::vector<IDirect3DTexture9*> m_textures;
    std::unordered_map<std::string, AnimClip> m_clips;

    std::deque<Assoc> m_assocs;          // stable references while a frame runs
    std::vector<Pose> m_pose;
    float m_rootDX = 0.0f, m_rootDY = 0.0f;    // root shift this frame, model space

    // CPlayerPedData / CPed state
    float m_ratio = 0.0f;                      // m_fMoveBlendRatio
    float m_heading = 1.5708f;                 // m_fCurrentRotation (+ pi/2: we face +X at 0)
    float m_aimHeading = 1.5708f;              // m_fAimingRotation
    float m_headingRate = 1.0f;                // scale of the player's 450 deg/s (0 while Run_stop)
    float m_velX = 0.0f, m_velY = 0.0f;
    float m_airVelX = 0.0f, m_airVelY = 0.0f;  // in-air horizontal speed (kept from takeoff)
    float m_minVelZ = 0.0f;                    // CTaskSimpleInAir::m_fMinZSpeed
    float m_airTime = 0.0f;
    bool m_inAir = false, m_jumping = false;
    bool m_busy = false;
    struct FootLeg { float weight = 0, offset = 0, pitch = 0, prevSole = 0; bool contact = false, valid = false; };
    FootLeg m_foot[2];                         // right, left
    float m_footRest[2] = {0.08f, 0.08f};      // bind pose ankle height above the sole
    bool m_footInit = false;
    bool m_busySoft = false;                   // FALL_land: the stick may cut it short
    const AnimClip* m_busyClip = nullptr;      // landing / collapse / get-up in progress
    const AnimClip* m_busyNext = nullptr;      // get-up after a KO skid
    // CTaskSimpleClimb
    bool m_climbing = false, m_cFinished = false, m_cChangeAnim = false, m_cChangePos = false;
    int m_cHAnim = 0, m_cHPos = 0;             // eClimbHeights: anim wanted, position state
    float m_cHold[3] = {0, 0, 0};              // handhold (ledge point)
    float m_cRot = 0.0f;                       // retail rotation facing the ledge
    const AnimClip* m_cClip = nullptr;         // m_Anim
    float m_cCounter = 0.0f;                   // m_GetToPosCounter (ms)
    bool m_cSnapValid = false, m_cRelValid = false;
    float m_cSnap[3] = {0, 0, 0}, m_cRel[3] = {0, 0, 0}, m_cVelZ = 0.0f;
    float m_cPed[3] = {0, 0, 0};               // CJ's origin as the host last reported it
    ClimbScanFn m_cScan;
    void ProcessClimb(const Input& in);
    void ClimbStartAnim();
    bool ClimbTest(int mode, int how);         // TestForVault / TestForStandUp
    const AnimClip* m_landClip = nullptr;      // JUMP_land over the run
    float m_bindMinZ = 0.0f;                   // feet in model space
    float m_stickX = 0.0f, m_stickY = 0.0f;    // the stick CJ is obeying (filtered in rewrite)
    float m_moveAngle = 0.0f;                  // first person: direction of travel

    // CTaskSimpleSwim
    enum class SwimState { Tread, Sprint, Sprinting, Dive, Under, BackToSurface };
    bool m_swimming = false;
    SwimState m_swimState = SwimState::Tread;
    float m_swimPitch = 0.0f;                  // m_fRotationX: positive dives
    float m_swimVelZ = 0.0f;
    const AnimClip* m_swimClip = nullptr;

    Movement m_move;                           // rewrite handling
};
