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

#include "Clothes.h"
#include "D3DRenderer.h"
#include "IfpLoader.h"
#include "Melee.h"
#include "Movement.h"
#include "ParaCanopy.h"
#include "SkinnedDff.h"
#include "WeaponStats.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

struct GunSpec;

// scan(mode, ped origin, rotation, out pos, out rotation); see ClimbScan.h.
using ClimbScanFn = std::function<bool(int, const float*, float, float*, float*)>;

struct GunSpec;

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
        float camPitch = 0.0f;                // radians, positive = looking up
        float camYaw = 0.0f;                  // for the rewrite heading / lean
        bool haveAimRay = false;              // third person: the camera ray through the crosshair
        bool lockOn = false;                  // locked on a target (CPlayerPed::m_pTargetedObject)
        float lockPos[3] = {0, 0, 0};         // its aim point, world
        float aimEye[3] = {0, 0, 0}, aimDir[3] = {1, 0, 0};   // world
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
        bool fireJust = false, fireHeld = false;   // CPad::GetPedFire (fire button)
        bool reloadJust = false;              // CPad::ReloadJustDown
        bool autoReload = false;              // rewrite: an emptied clip reloads by itself
        bool duckJust = false;                // CPad::DuckJustDown
        bool aim = false;                     // CPad::GetTarget (aim button held)
        bool duckHeld = false;                // the duck button is down (blocks while locked on with the fists)
        bool groundTarget = false;            // a fallen ped lies in reach in front (CTaskSimpleFight::FindTargetOnGround)
        bool stealthTarget = false;           // an unaware ped stands within reach with its back to CJ (CTaskSimpleStealthKill)
        float stealthPos[2] = {0, 0};         // where it stands
        bool whipTarget = false;              // a standing ped is close in front (CTaskSimpleUseGun::RequirePistolWhip)
        bool exitPressed = false;             // CPad::GetExitVehicle (lets go)
        float carSteer = 0.0f;                // seated: CVehicle::m_fSteerAngle, radians, positive = left
        float carSpeed = 0.0f;                // seated: the car's speed, m/s
        bool carReverse = false;              // seated: gas pedal below zero (CVehicle::m_GasPedal)
        bool jetStop = false;                 // jetpack: look behind held (CTaskSimpleJetPack stop mode)
        bool jetBrake = false;                // jetpack: brake held (negative thrust)
        ClimbScanFn climbScan;
    };

    // Weapon skill of CJ: 0 poor (a new game), 1 standard. Call before Init; picks the weapon.dat row and fire clips.
    static void SetWeaponSkill(int skill) { WeaponStats::g_forced = skill; }
    void RefreshWeaponSkills();           // re-pick weapon.dat rows after a skill stat changed
    bool Init(D3DRenderer& r, const std::string& gameDir);
    // CPedClothesDesc: what CJ wears. SetOutfit is CClothes::RebuildPlayer (models and textures rebuilt).
    static PedClothesDesc DefaultOutfit();
    const PedClothesDesc& Outfit() const { return m_outfit; }
    bool SetOutfit(const PedClothesDesc& desc);
    // CPlayerSkin: skins\<name>.bmp (or models\generic\player.bmp for "") laid over every part; a missing picture restores the clothes
    void SetSkin(const char* name);
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
    // CStats::UpdateStatsWhen*: the exercise rate of what CJ is doing (ar_stats.dat
    // STAT_EXERCISE_RATE_*, 10 = one second per second), 0 when he is not exercising.
    int ExerciseRate() const;
    bool Underwater() const { return m_swimming && m_swimState == SwimState::Under; }
    bool SwimJumpOut() const { return m_swimming && m_swimState == SwimState::BackToSurface; }
    float SwimSubmerge() const;            // how far below the surface the ped origin rides
    float SwimVelZ() const { return m_swimVelZ; }
    // CTaskSimpleDuck (secondary task) and the aiming stance (the movement part
    // of CTaskSimpleUseGun::ControlGunMove, without a weapon).
    // Firearms (PlayerWeapon.cpp): weapon.dat pistols, shotguns, SMGs and rifles
    // in the right hand, with the aim / fire / reload clips while aiming.
    void SelectGun(int index);          // -1 = fists
    void NextGun();                     // cycles through the list, then fists
    void PrevGun();                     // the other way
    void SwitchAfterEmpty();            // CPlayerPed::ProcessWeaponSwitch: out of ammo, the next lower slot with ammo
    int GunIndex() const { return m_gun; }   // the firearm in hand, -1 for fists or a melee weapon
    int WeaponIndex() const { return m_gun >= 0 ? m_gun : m_melee; }   // anything in hand, -1 = fists
    // CTaskSimpleCarDrive + CTaskSimpleGangDriveBy (PlayerDriveby.cpp). The host seats CJ at the car's
    // seat dummy; the ped origin is the pelvis. EnterVehicle is CPed::ReplaceWeaponForVehicle: the
    // SMG (Micro Uzi, Tec-9, MP5) in hand with the old weapon saved, LeaveVehicle gives it back.
    void EnterVehicle(bool lowCar);
    void LeaveVehicle();
    bool InVehicle() const { return m_seated; }
    void SetSeat(const float pos[3], float noseYaw) { m_seatPos[0] = pos[0]; m_seatPos[1] = pos[1]; m_seatPos[2] = pos[2]; m_seatYaw = noseYaw; }
    void SeatPlacement(float out[3]) const { for (int i = 0; i < 3; i++) out[i] = m_seatPos[i]; }
    bool Driveby() const { return m_seated && m_dbClip != nullptr; }
    bool CanDriveby() const;               // an SMG with ammo is in hand
    void SetDrivingSkill(float v) { m_drivingSkill = v; }   // STAT_DRIVING_SKILL 0..1000
    float WeaponRadiusOnScreen() const;   // CPlayerPed::GetWeaponRadiusOnScreen
    float TargetRange() const { return m_gun >= 0 ? m_guns[m_gun].targetRange : 0.0f; }
    // CWeaponInfo::GetTargetHeadRange: (skill + 2) * weapon range * 0.04
    float TargetHeadRange() const { return m_gun >= 0 ? (float)(m_guns[m_gun].skill + 2) * m_guns[m_gun].range * 0.04f : 0.0f; }
    // CWeapon::FireInstantHit inputs: the crosshair counter, weapon.dat accuracy and flags.
    float AttackCounter() const { return m_attackCounter; }
    float GunAccuracy() const { return m_gun >= 0 ? m_guns[m_gun].accuracy : 1.0f; }
    bool GunCanAim() const { return m_gun >= 0 && (m_guns[m_gun].flags & 1); }
    int GunCount() const;                 // guns carried (the wheel lists these plus the fists)
    const char* GunNameAt(int i) const;
    int GunSlotAt(int i) const;           // weapon.dat weapon slot (the weapon wheel categories)
    IDirect3DTexture9* GunIconAt(int i) const;
    int GunClipAt(int i) const;           // rounds in the clip
    int GunTotalAt(int i) const;          // rounds left outside the clip
    int GunClipSizeAt(int i) const;
    const char* GunId() const;            // weapon.dat name, or ""
    bool InAir() const { return m_inAir; }
    const char* GunName() const;
    int GunAmmo() const;
    int GunClipSize() const;
    int GunTotalAmmo() const;           // rounds left outside the clip
    float GunDamage() const { return m_gun >= 0 ? m_guns[m_gun].damage : 5.0f; }
    float GunRange() const { return m_gun >= 0 ? m_guns[m_gun].range : 2.0f; }
    void RefillAmmo();                  // every gun back to a full stock (shooting range)
    bool GunMelee() const { return m_gun < 0; }
    // Melee (PlayerMelee.cpp): CTaskSimpleFight on data\melee.dat.
    // The strike sphere of a swing that reached its hit frame this update.
    struct Strike {
        float pos[3] = {0, 0, 0};   // sphere centre, world (ped origin = feet + 1)
        float radius = 0.4f;
        float damage = 0.0f;        // move damage x GetFatAndMuscleModifier(4)
        int move = 0;               // Melee::MoveIdx
        bool sword = false;         // SWORD combo: HIT_3 force-death 50%
        int hitAudio = 0, altHitAudio = 0;
        float heading = 0.0f;       // attacker facing
        bool ground = false;        // AGROUND: only a fallen ped is hit
        std::string hitClips[3];    // the combo's anim group hit reactions (HIT_1..3)
    };
    bool TakeStrike(Strike& out) { if (!m_strikePending) return false; out = m_strike; m_strikePending = false; return true; }
    void SetFightStyle(int style) { m_fightStyle = std::clamp(style, 0, 3); }   // UNARMED_1..4
    bool Fighting() const { return m_fight.active; }
    // CTaskSimpleStealthKill: true once, when the knife takedown starts (the host kills the victim).
    bool TakeStealth() { const bool p = m_stealthPending; m_stealthPending = false; return p; }
    // A melee hit on CJ (CTaskSimpleFight::BeHitWhileBlocking / the CEventDamage reaction): turns to the attacker
    // and plays HIT_1..3 of the held weapon's group. Returns false when the guard took it (no damage).
    bool MeleeHit(float fromX, float fromY, float worldX, float worldY, int strength);
    bool Blocking() const { return m_fight.active && m_fight.block; }   // FightA_Block held: hits on CJ are shrugged off
    // Thrown weapons (PlayerThrow.cpp): CTaskSimpleThrowProjectile. A projectile leaves the hand at the
    // fire frame of the throw clip; the host owns it from there (Projectiles.h).
    struct Launch {
        int gun = -1;               // index of the weapon whose model the projectile wears
        int type = 0;               // eWeaponType: 16 grenade, 17 teargas, 18 molotov, 39 satchel charge
        float pos[3] = {0, 0, 0};   // world
        float vel[3] = {0, 0, 0};   // m/s
        bool fell = false;          // thrown into a wall: it just drops at CJ's feet (force 0)
    };
    bool TakeLaunch(Launch& out) { if (!m_launchPending) return false; out = m_launch; m_launchPending = false; return true; }
    bool TakeDetonate() { const bool d = m_detonatePending; m_detonatePending = false; return d; }   // CWorld::UseDetonator
    bool HoldingThrown() const { return m_melee >= 0 && m_guns[m_melee].thrown; }
    // CTaskSimpleParachute: 0 none, 1 PARA_open playing, 2 floating under the canopy.
    int Parachute() const { return m_para; }
    // The parachute script's vertical speed (m/s, up positive): true when it overrides gravity this frame.
    bool ParaVelZ(float& v) const;
    bool Skydiving() const { return m_sky != 0; }
    // CTaskSimpleJetPack: the pack on CJ's back. Velocity is the task's own, in metres per second.
    void StartJetpack();
    void StopJetpack(bool drop);
    bool Jetpacking() const { return m_jet.on; }
    bool JetVelZ(float& v) const;                       // true: the pack owns the vertical speed this frame
    bool TakeJetDrop() { const bool d = m_jet.dropPending; m_jet.dropPending = false; return d; }   // the pack was let go: make a pickup
    float JetThrust() const { return m_jet.fxKey; }     // 0..1 flame strength
    bool JetNozzles(float x, float y, float feetZ, float out[2][3]) const;   // world positions of both jetballs
    float JetTime() const { return m_jet.time; }        // STAT_TIME_ON_JETPACK, seconds
    bool TakeParaHelp() { const bool h = m_paraHelp; m_paraHelp = false; return h; }   // PARA_01 due
    void SetHideGun(bool b) { m_hideGun = b; }   // the scope view never shows the weapon
    int Goggles() const { return m_goggles; }   // 0 off, 44 night vision, 45 thermal
    bool Throwing() const { return m_throw.state != 0; }
    void SetWallProbe(std::function<bool(const float*, const float*)> f) { m_wallProbe = std::move(f); }
    void DrawGunModel(D3DRenderer& r, int gunIndex, const float world[4][4]);   // a weapon model at a world matrix
    int GunType(int gunIndex) const;                                            // eWeaponType of a weapon
    int RocketModelIndex() const;                                               // the hidden rocket model slot
    bool Reloading() const { return m_reloading; }
    IDirect3DTexture9* GunIcon() const; // hud icon texture, or null (fists)
    // One-shot events for sound and effects, taken by the host each frame.
    // Muzzle position in the world (weapon.dat fire offset on the hand), false when unarmed.
    bool MuzzleWorld(float x, float y, float feetZ, float out[3]) const;
    int TakeShotEvents() { int n = m_shotEvents; m_shotEvents = 0; return n; }
    int TakeReloadEvents() { int n = m_reloadEvents; m_reloadEvents = 0; return n; }
    int TakeDryEvents() { int n = m_dryEvents; m_dryEvents = 0; return n; }
    // Vitals (HUD). Damage sources arrive with combat; drowning works already.
    float Health() const { return m_health; }
    float MaxHealth() const { return m_maxHealth; }
    // STAT_MAX_HEALTH 569 = 100 hit points; a raised max also raises the health by the same amount
    void SetMaxHealth(float hp) { m_health += hp - m_maxHealth; m_maxHealth = hp; }
    void AddHealth(float hp) { m_health = std::min(m_maxHealth, m_health + hp); }
    float Armor() const { return m_armor; }
    void SetJumpMod(float v) { m_jumpMod = v; }
    void SetClimbMod(float v) { m_climbMod = v; }
    float Breath() const { return m_breath / m_breathMax; }   // 0..1
    // CStats::GetFatAndMuscleModifier(8), in game steps (1/50 s); the lungs start full
    void SetBreathMax(float v) { if (!m_breathInit) { m_breathInit = true; m_breath = v; } m_breathMax = v; m_breath = std::min(m_breath, v); }
    bool ShowBreath() const { return m_swimming && m_swimState == SwimState::Under || m_breath < m_breathMax * 0.999f; }
    void Hurt(float amount);            // armor first, then health
    void RestoreVitals() { m_health = m_maxHealth; m_armor = 100.0f; }   // CHEAT_HEALTH_ARMOR_MONEY
    float DamageFlash() const { return m_damageFlash; }       // 1 right after a hit, fades out
    bool Dead() const { return m_health <= 0.0f; }
    bool Ducking() const { return m_ducking; }
    bool Aiming() const { return m_aiming; }
    bool FistLock() const { return m_aiming && m_gun < 0 && m_meleeOk; }   // locked on with the fists: jump is the kick
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
        bool partial = false;            // upper-body overlay: scales the other clips down, no root motion
        bool autoRemove = false;         // ANIMATION_IS_FINISH_AUTO_REMOVE: gone once it has played through
    };

    int MoveSetId() const;
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
    void ApplyAimIK(const Input& in);
    void ApplySwimPitch();
    void UpdateArmChain(int side, bool want, const float target[3], float dt);
    void RotateTorsoForArm(const float target[3]);
    void ApplyFootIK(const Input& in);
    void FootRestHeights();
    void Skin();

    void ControlOnFoot(const Input& in);
    void SetRealMoveAnim(bool sprintHeld);
    void ControlInAir(const Input& in);
    void Land(const Input& in);
    void ControlSwim(const Input& in);
    void ControlStance(const Input& in);
    void LoadGuns(D3DRenderer& r, const std::string& gameDir);
    void ShutdownGuns(D3DRenderer& r);
    void UpdateGun(const Input& in);
    void UpdateMelee(const Input& in);
    void UpdateThrow(const Input& in);
    void ThrowStart(const Input& in);
    void ThrowFire(const Input& in);
    int DetonatorIndex() const;
    void HandWorld(const Input& in, const float off[3], float out[3]) const;
    const Melee::Combo* CurrentCombo() const;
    float FatMuscleModifier() const;
    void DrawGunAt(D3DRenderer& r, const RwMat& hand, const float world[4][4], bool flash);
    RwMat LeftGunMatrix() const;   // the twin gun's frame: left hand, X 180, then (0.04, -0.05, 0)
    void DrawGun(D3DRenderer& r, float x, float y, float feetZ);
    struct StanceW { const char* name; float w; bool move, loop; float rate; };
    void StanceBlend(const StanceW* ws, int n, float dt, float expResp, float fadeRate);
    void LeaveStance(bool rewrite, bool exitMoving, bool exitSprint);
    void SwimWant(const char* name, float rate, bool movement);

    bool m_ready = false;
    std::vector<SkinFrame> m_skeleton;
    std::vector<int> m_frameOfBone;            // bone id (0..511) -> frame index
    std::vector<RwMat> m_globals;              // per frame, relative to frame 0
    std::vector<Part> m_parts;
    std::vector<IDirect3DTexture9*> m_textures;   // CClothesBuilder::ConstructTextures: torso head legs feet necklace watch glasses hat extra1
    bool BuildOutfit(const PedClothesDesc& want);
    bool BuildTextures(const PedClothesDesc& d, float rNormal, float rFat, float rMuscle);
    D3DRenderer* m_r = nullptr;
    std::string m_gameDir;
    PedClothesDesc m_outfit;
    std::string m_skinName;
    IDirect3DTexture9* m_skinTex = nullptr;   // CPlayerSkin picture over every material
    float m_texFat = -1.0f, m_texMuscle = -1.0f;   // the body ratios the textures were blended with
    std::vector<int> m_baseNodeIds, m_baseNodeOfBone;
    std::vector<RwMat> m_baseInverse;
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

    struct Gun {
        const GunSpec* spec = nullptr;
        bool thrown = false, detonator = false;                              // grenade family / detonator
        int type = 0;                                                        // eWeaponType
        int slot = 0;                                                        // weapon.dat weapon slot (0 fists .. 12 detonator)
        const char* fireClip = nullptr;                                      // standing fire clip (poor skill uses its own)
        std::string name;
        int clipSize = 0, ammo = 0, total = 0;
        IDirect3DTexture9* icon = nullptr;
        float loopStart = 0, loopEnd = 0, fireTime = 0, reloadTime = 1.0f;   // seconds (standing loop)
        float loop2Start = 0, loop2End = 0, fire2Time = 0;                   // the crouched loop (weapon.dat anim loop 2)
        float moveSpeed = 1.0f;                                              // weapon.dat move speed
        float barrel[3] = {0, 1, 0};                                         // barrel direction in hand space (long axis of the model, grip to muzzle)
    unsigned flags = 0;                                                  // weapon.dat flags
    float accuracy = 1.0f;                                               // weapon.dat accuracy
    float targetRange = 25.0f;                                           // weapon.dat target range (lock-on)
    float damage = 10.0f, range = 30.0f;                                 // weapon.dat damage and weapon range
        std::vector<std::string> skillRows[3];                               // weapon.dat rows: poor, standard, pro
        int skill = 0;                                                       // the row in use
        bool twin = false;                                                   // flag 0x800: a gun in each hand (pro skill)
        bool aimWithArm = false;                                             // flag 0x2: the arm points, else the torso
        const Melee::Combo* combo = nullptr;                                 // melee: its data\melee.dat combo
        std::string comboName;                                               // melee: weapon.dat base combo (data\melee.dat), numbered by fight style for UNARMED
        float fireOffset[3] = {0, 0, 0};
        std::vector<D3DRenderMesh*> meshes;
        std::vector<D3DRenderMesh*> muzzle;     // the muzzle flash atomic, drawn only on a shot
        std::vector<IDirect3DTexture9*> textures;
    };
    static void ApplyGunRow(Gun& g, int skill);
    std::vector<Gun> m_guns;
    bool AttackSpread(const Gun& g) const;
    int m_gun = -1;     // the firearm in hand
    int m_melee = -1;   // the melee weapon in hand (fists when both are -1)
    int m_shotEvents = 0, m_reloadEvents = 0, m_dryEvents = 0;
    float m_climbMod = 1.0f;  // GetFatAndMuscleModifier(1): looping climb clip speed
    float m_jumpMod = 1.0f;   // CStats::GetFatAndMuscleModifier(2): jump launch, launch / land clip speed
    float m_breathMax = 1150.0f;   // steps of air; CPlayerPedData::m_fBreath
    bool m_breathInit = false;
    int m_breathLog = 0;
    float m_health = 100.0f, m_maxHealth = 100.0f, m_armor = 0.0f, m_breath = 1150.0f, m_damageFlash = 0.0f;
    bool m_reloading = false, m_firing = false, m_shotDone = false, m_fireQueued = false;
    float m_flash = 0.0f;                      // muzzle flash time left
    float m_flashL = 0.0f;
    float m_attackCounter = 0.0f;              // CPlayerPedData::m_fAttackButtonCounter: shots widen the crosshair                     // twin pistols: the left gun's flash
    bool m_shotLeftDone = false, m_lastShotLeft = false;   // twin pistols: left shot of this loop fired / the last shot was the left gun
    float m_reloadTime = 0.0f;
    const AnimClip* m_gunFireClip = nullptr;
    const AnimClip* m_gunRelClip = nullptr;
    bool m_ducking = false, m_aiming = false;
    bool m_sprintBeforeDuck = false;
    float m_aimHeadVel = 0.0f, m_aimPitch = 0.0f, m_aimPitchVel = 0.0f;   // damped aim follow, rad/s, deg
    float m_duckX = 0.0f, m_duckY = 0.0f;      // CTaskSimpleDuck::m_MoveCmd (x strafe, y: -1 forward)
    // CTaskSimpleSwim
    enum class SwimState { Tread, Sprint, Sprinting, Dive, Under, BackToSurface };
    bool m_swimming = false;
    struct IkNode { int bone = 0, frame = 0; float q[4] = {0, 0, 0, 1}; float lmin[3] = {}, lmax[3] = {}; };
    struct IkChain { bool alive = false; float blend = 0.0f, target = 0.0f, remainMs = 0.0f; std::vector<IkNode> nodes; };
    IkChain m_ikArm[2];   // right, left arm point chains (IKChainManager_c slots)
    int m_moveSet = 0;   // last frame's move set (0 player, 1 armed, 2 rocket, 3 chainsaw)
    SwimState m_swimState = SwimState::Tread;
    float m_swimPitch = 0.0f;                  // m_fRotationX: positive dives
    float m_swimVelZ = 0.0f;
    const AnimClip* m_swimClip = nullptr;

    // CTaskSimpleFight
    Melee::Table m_meleeTable;
    bool m_meleeOk = false;
    int m_fightStyle = 0;
    float m_fatStat = 0.0f, m_muscleStat = 0.0f;   // 0..1, SetBodyShape
    struct Fight {
        bool active = false;
        const Melee::Combo* combo = nullptr;
        int move = 0;                 // Melee::MoveIdx in progress
        const AnimClip* clip = nullptr;
        bool hitDone = false, queued = false;
        bool special = false;         // KICK_STD / PISTOL_WHIP rather than the held weapon's combo
        bool block = false;           // FightX_Block: holds between blockHit and blockChain while the button stays down
    } m_fight;
    bool m_strikePending = false, m_stealthPending = false;
    Strike m_strike;
    // CTaskSimpleThrowProjectile
    struct Throw {
        int state = 0;                  // 0 none, 1 WEAPON_start_throw, 2 WEAPON_throw / WEAPON_throwu
        const AnimClip* clip = nullptr; // m_Anim
        bool underhand = false;         // WEAPON_throwu
        bool released = false;          // m_bButtonReleased
        bool fired = false;
        float counterMs = 0.0f;         // m_ButtonCounter: ms since the press, frozen at the release
        float prevTime = 0.0f;
    } m_throw;
    Launch m_launch;
    bool m_hideGun = false;
    // seated in a car (PlayerDriveby.cpp)
    bool m_seated = false, m_seatLow = false;
    float m_seatPos[3] = {0, 0, 0}, m_seatYaw = 0.0f, m_drivingSkill = 0.0f;
    int m_savedWeapon = -2;                // -2 none; else the WeaponIndex() to give back
    const AnimClip* m_dbClip = nullptr;    // the Driveby_x clip in use
    bool m_dbFiring = false, m_dbShot = false;
    float m_dbTime = 0.0f;                 // the task's own clock for the hold / shot / loop
    int m_dbSide = 0;                      // +1 left window, -1 right
    void UpdateSeated(const Input& in);
    int m_goggles = 0, m_drawIdx = -1;
    int m_para = 0;
    int m_sky = 0;                       // player_parachute.scm skydive state
    float m_pTurn = 0.0f, m_pPitch = 0.0f;   // smoothed pad X / Y (script l2, l3)
    float m_paraT = 0.0f;                // seconds since the pull
    float m_paraVz = 0.0f, m_paraVz0 = 0.0f, m_paraVx0 = 0.0f, m_paraVy0 = 0.0f;
    bool m_paraHelp = false, m_paraHelpDone = false;
    int ParaGun() const;
    ParaCanopy m_canopy;   // the parachute object, posed by the PARA_*_O clip matching CJ's PARA_* clip
    const AnimClip* m_paraClip = nullptr;
    bool UpdateParachute(const Input& in);
    struct Jet {
        bool on = false;
        bool flying = false;                 // not standing: thrusting or in the air
        float fwd = 0.0f, strafe = 0.0f, angle = 0.0f;
        bool stop = false;
        float v[3] = {0, 0, 0};              // per game frame (50 Hz), as the task keeps it
        float prevHeading = 0.0f, headingDelta = 0.0f;
        float legFwd = 0.0f, legFwdSpeed = 0.0f, legSide = 0.0f, legSideSpeed = 0.0f;
        float fxKey = 0.0f, time = 0.0f, clock = 0.0f;
        int savedWeapon = -2;
        bool dropPending = false;
    } m_jet;
    bool UpdateJetpack(const Input& in);
    void DrawJetpack(D3DRenderer& r, const D3DMATRIX& world);
    bool JetPackMatrix(RwMat& out) const;               // model space, from the upper torso bone
    void LoadJetpack(D3DRenderer& r, ImgLoader& gta3);
    std::vector<D3DRenderMesh*> m_jetBody, m_jetBall[2];
    std::vector<IDirect3DTexture9*> m_jetTextures;
    const AnimClip* m_gogglesClip = nullptr;
    bool m_launchPending = false, m_detonatePending = false, m_switchToDetonator = false;
    std::function<bool(const float*, const float*)> m_wallProbe;   // segment a -> b hits the world
    void FightStart(const Input& in, int move, const Melee::Combo* use = nullptr);
    void FightEnd(bool abrupt);

    Movement m_move;                           // rewrite handling
};
