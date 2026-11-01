// Aircraft: CHeli's controls and rotor, and CVehicle::FlyingControl (0x6D85F0), the forces that keep a
// CAutomobile in the air. Like the rest of the car physics it works per 1/50 s with retail's constants.
#define NOMINMAX
#include "Vehicle.h"
#include "PhysicsCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

using namespace physics;

constexpr float kTwoPi = 6.2831855f;

enum { MODEL_LEVIATHN = 417, MODEL_SEASPAR = 447, MODEL_RCRAIDER = 465, MODEL_SPARROW = 469, MODEL_MAVERICK = 487,
       MODEL_VCNMAV = 488, MODEL_POLMAV = 497, MODEL_RCGOBLIN = 501 };

bool IsRcHeli(int model) { return model == MODEL_RCRAIDER || model == MODEL_RCGOBLIN; }

// CVehicle::HeightAboveCeiling: how far above the flight ceiling (800 m; 500 for the RC models), else -1.
float HeightAboveCeiling(float z, bool rc) {
    if (rc) {
        if (z >= 500.0f) {
            if (z < 950.0f) return z - 500.0f;
            if (z >= 1500.0f) return z - 1000.0f + 500.0f;
        }
        return -1.0f;
    }
    return z < 800.0f ? -1.0f : z - 800.0f;
}

void TurnResistance(Body& b, const HandlingData::Fly& f, float ts, float k);

// CVehicle::FlyingControl, FLIGHT_MODEL_HELI, then the turn resistance and speed caps every model shares.
// skid: the tail rotor (look right +1), upDown: pitch (+ nose up), leftRight: roll (- to the right), accel: collective.
// nozzle: the Hydra's nozzle angle (0 back .. 1.5708 down), the rotor's lift then pushes along it; < 0 for none.
V3 NozzleThrust(const Body& b, float nozzle) { return Norm(b.up * std::sin(nozzle) + b.fwd * std::cos(nozzle)); }

void FlyingControlHeli(Body& b, const HandlingData::Fly& f, float ts, const float wind[3], bool rc,
                       float skid, float upDown, float leftRight, float accel, float nozzle = -1.0f) {
    if (ts <= 0.0f) return;
    const V3 windDir(wind);
    const V3 vel = b.mv - windDir * f.windMult;   // the speed through the air
    const V3 com = b.ComW();

    b.mv *= std::pow(f.moveRes, ts);
    const V3 rotorUp = nozzle >= 0.0f ? NozzleThrust(b, nozzle) : b.up;
    float climb = Dot(rotorUp, vel);
    if (climb > 0.0f) climb += climb;
    // the rotor carries half the weight outright, damping the climb and fall
    b.ApplyMoveForce(V3(0, 0, 1) * ((0.5f - b.mv.z) * 0.008f * b.mass * ts));
    float lift = accel * f.thrust + 0.45f - climb * f.thrustFallOff;
    const float above = HeightAboveCeiling(b.pos.z, rc);
    if (above > 0.0f) lift = lift / (above + 10.0f) * 10.0f;
    b.ApplyMoveForce(rotorUp * (0.008f * lift * b.mass * ts));

    // self-righting: toward the vertical (leaning into the wind), or flipping back over when upside down
    if (b.up.z <= 0.0f) {
        const float r = b.right.z >= 0.0f ? -f.formLift : f.formLift;
        b.ApplyTurnForce(b.up * (r * f.attackLift * ts * b.turnMass), b.right + com);
        const float p = b.fwd.z >= 0.0f ? -f.formLift : f.formLift;
        b.ApplyTurnForce(b.up * (p * f.attackLift * ts * b.turnMass), b.fwd + com);
    } else {
        const V3 target = Norm(V3(0, 0, 1) + windDir * f.windMult);
        const float r = std::clamp(Dot(target, b.right), -f.formLift, f.formLift);
        b.ApplyTurnForce(b.up * (-r * f.attackLift * ts * b.turnMass), b.right + com);
        const float p = std::clamp(Dot(target, b.fwd), -f.formLift, f.formLift);
        b.ApplyTurnForce(b.up * (-p * f.attackLift * ts * b.turnMass), b.fwd + com);
    }

    b.ApplyTurnForce(b.up * (upDown * f.pitch * b.turnMass * ts), com + b.fwd);
    b.ApplyTurnForce(b.up * (leftRight * f.roll * b.turnMass * ts), com + b.right);
    const float side = -Dot(vel, b.right);
    const float slip = std::fabs(side) * f.sideSlip * side;
    b.ApplyMoveForce(b.right * (slip * b.mass * ts));
    b.ApplyTurnForce(b.right * ((skid * f.yaw + std::fabs(side) * f.yawStab * side) * ts * b.turnMass), com - b.fwd);
    b.ApplyTurnForce(b.fwd * (skid * f.yaw * b.turnMass * ts), com + b.right);
    TurnResistance(b, f, ts, Len(b.mv) + 1.0f);
}

// CVehicle::FlyingControl, FLIGHT_MODEL_PLANE: engine thrust with its fall-off at speed (reverse thrust on the
// ground), side slip, yaw / roll / pitch from the controls (stronger with airspeed) and the tail's
// stabilisation, wing lift from the angle of attack, then the turn resistance and caps.
// skid: rudder, upDown: elevators, leftRight: ailerons (-1 .. 1), accel: throttle; airborne: no wheel touches.
void FlyingControlPlane(Body& b, const HandlingData::Fly& f, float tailY, float ts, const float wind[3], bool airborne, float gearStatus,
                        float skid, float upDown, float leftRight, float accel, float nozzle = -1.0f, bool vortex = false) {
    if (ts <= 0.0f) return;
    const V3 vel = b.mv - V3(wind) * f.windMult;
    const V3 com = b.ComW();
    // a diagonal stick is not weaker than a straight one
    const float stick = std::atan2(upDown, skid);
    float steerMul = 1.0f;
    constexpr float kQ = 0.7853982f;
    if (stick > -kQ && stick <= kQ) steerMul = 1.0f / std::cos(stick);
    else if (stick > kQ && stick < 3.0f * kQ) steerMul = 1.0f / std::cos(stick - 2.0f * kQ);
    else if (stick > 3.0f * kQ) steerMul = 1.0f / std::cos(stick - 4.0f * kQ);
    else if (stick < -3.0f * kQ) steerMul = 1.0f / std::cos(stick + 4.0f * kQ);
    else if (stick < -kQ) steerMul = 1.0f / std::cos(stick + 2.0f * kQ);
    const V3 tail = b.fwd * tailY;
    const float airSpeed = Dot(b.fwd, vel), airSpeed2 = airSpeed * airSpeed;

    float thrust;
    if ((airborne && !vortex) || accel > 0.0f) {   // the Vortex has no idle thrust: only the throttle pushes it
        thrust = vortex ? accel * f.thrust : (accel + 1.0f) * f.thrust * 0.5f;
        if (airSpeed > 0.0f && f.thrustFallOff < 1.0f) {
            float d = f.thrustFallOff < 0.0f ? airSpeed + f.thrustFallOff : airSpeed - f.thrustFallOff;
            d = f.thrustFallOff < 0.0f ? d * d * 3.0f : d * d * 0.65f;
            thrust *= std::max(0.0f, 1.0f - d);
        }
    } else if (accel >= 0.0f || Dot(b.fwd, b.mv) >= 0.02f) thrust = 0.0f;
    else thrust = std::min(0.0f, accel - airSpeed * 7.76f) * f.thrust;   // backing up on the ground
    b.ApplyMoveForce((nozzle >= 0.0f ? NozzleThrust(b, nozzle) : b.fwd) * (0.008f * thrust * b.mass * ts));

    const float side = -Dot(vel, b.right);
    b.ApplyMoveForce(b.right * (std::fabs(side) * f.sideSlip * side * b.mass * ts));
    const float tailSide = -Dot(b.Speed(tail), b.right);
    // the Vortex turns with its thrust as well as its speed: min(thrust, speed) going forward, max backing
    const float yawSpeed = !vortex ? airSpeed : airSpeed > 0.0f ? std::min(thrust, airSpeed) : std::max(thrust, airSpeed);
    b.ApplyTurnForce(b.right * ((yawSpeed * f.yaw * (skid * steerMul) + std::fabs(tailSide) * f.yawStab * tailSide) * ts * b.turnMass), com + tail);
    b.ApplyTurnForce(b.right * (ts * f.roll * airSpeed * leftRight * b.turnMass), com + b.up);
    {   // the wings level themselves
        V3 level = Cross(b.fwd, V3(0, 0, 1));
        if (b.up.z <= 0.0f) level = -level;
        const float sign = b.right.z > 0.0f ? -1.0f : 1.0f;
        b.ApplyTurnForce(b.right * ((1.0f - Dot(level, b.right)) * sign * (1.0f - std::fabs(b.fwd.z)) * f.rollStab * ts * b.turnMass * 0.5f), com + b.up);
    }
    const float tailUp = -Dot(b.Speed(tail), b.up);
    b.ApplyTurnForce(b.up * ((airSpeed * f.pitch * -(steerMul * upDown) + std::fabs(tailUp) * f.pitchStab * tailUp) * ts * b.turnMass), com + tail);

    // lift: the angle of attack and the wing's own lift, with the square of the airspeed
    const float attack = -std::asin(std::clamp(Dot(vel, b.up) / std::max(0.01f, Len(vel)), -1.0f, 1.0f));
    float form = f.formLift;
    if (gearStatus < 1.0f) form *= f.gearDownL;
    float lift = (attack * f.attackLift + form) * b.mass * ts * airSpeed2;
    if (lift > ts * b.mass * 0.008f) {
        const float above = HeightAboveCeiling(b.pos.z, false);
        if (above > 0.0f) lift = std::max(0.0f, 1.0f - above * 0.02f) * b.mass * ts * 0.008f;
    }
    b.ApplyMoveForce(b.up * lift);
    TurnResistance(b, f, ts, Len(b.mv) * 2.0f);
}

// FlyingControl's shared tail: turn resistance per body axis (plain decay, or with a speed resistance a pull that
// grows with the spin), scaled by k, then the move speed held to 1.5 and the turn speed to 0.2.
void TurnResistance(Body& b, const HandlingData::Fly& f, float ts, float k) {
    const V3 com = b.ComW();
    const float res[3] = {std::pow(f.turnRes[0], ts), std::pow(f.turnRes[1], ts), std::pow(f.turnRes[2], ts)};
    float local[3] = {Dot(b.tv, b.right), Dot(b.tv, b.fwd), Dot(b.tv, b.up)};
    float extra[3] = {0, 0, 0};
    for (int a = 0; a < 3; a++) {
        if (f.speedRes[a] <= 0.0f) local[a] *= std::pow(res[a], k);
        else extra[a] = std::pow(res[a] / (1.0f + local[a] * local[a] * f.speedRes[a]), ts) * local[a] - local[a];
    }
    b.tv = b.Rot(V3(local[0], local[1], local[2]));
    b.ApplyTurnForce(-b.fwd * (extra[0] * k * b.turnMass), com + b.up);
    b.ApplyTurnForce(-b.up * (extra[1] * k * b.turnMass), com + b.right);
    b.ApplyTurnForce(-b.right * (extra[2] * k * b.turnMass), com + b.fwd);

    if (Len2(b.mv) > 2.25f) b.mv *= 1.5f / Len(b.mv);
    if (Len2(b.tv) > 0.04f) b.tv *= 0.2f / Len(b.tv);
}

}  // namespace

bool Vehicles::UsesHeliPhysics(int model) const {
    const VehicleInfo* info = Info(model);
    if (!info) return false;
    std::string t = info->type;
    for (char& ch : t) ch = (char)std::tolower((unsigned char)ch);
    const HandlingData* h = Handling(info->handling);
    return t == "heli" && h && h->fly.found;
}

// CHeli::ProcessControlInputs (keyboard): collective on accelerate / brake, pitch on steer up / down, roll on
// steer left / right, the tail rotor on look left / right. Holding the horn (upright) flies the hover assist: no
// tail rotor, and pitch and roll set against the drift along the level forward and side (by the pitch / roll
// stabilities, at most 2 either way). A burning heli loses its lift and swings its tail.
void Vehicles::HeliInputs(VehicleInstance& c, const HandlingData& h, float ts, bool player) {
    CarPhys& p = c.phys;
    if (!player) { p.flyAccel = p.flyUpDown = p.flyLeftRight = p.flySkid = 0.0f; return; }
    p.flyAccel = p.inAccel - p.inBrake;
    // Mouse flying (CVehicle::m_bEnableMouseFlying, on by default): the mouse's movement is added to the stick
    // (0.0025 a count; right rolls right, down pulls back) and, while it is small, eased back by 0.98 a step; the
    // stick stays where the mouse left it until a key takes over.
    const bool padSteer = p.inSteer != 0.0f || p.inLeanFwd != 0.0f;
    const bool moved = p.inMouseDX != 0.0f || p.inMouseDY != 0.0f;
    if (moved || (p.lastInputMouse && !padSteer && (p.flyLeftRight != 0.0f || p.flyUpDown != 0.0f))) {
        p.lastInputMouse = true;
        p.flyLeftRight -= p.inMouseDX * 0.0025f;
        p.flyUpDown += p.inMouseDY * 0.0025f;
        const float ease = std::pow(0.98f, ts);
        if (std::fabs(p.flyLeftRight) < 0.5f) p.flyLeftRight *= ease;
        if (std::fabs(p.flyUpDown) < 0.5f) p.flyUpDown *= ease;
    } else if (!(p.lastInputMouse && !padSteer)) {
        p.lastInputMouse = false;
        p.flyUpDown = -p.inLeanFwd;       // steer up (num 8) is the stick forward: nose down
        p.flyLeftRight = -p.inSteer;
    }
    p.flyUpDown = std::clamp(p.flyUpDown, -1.0f, 1.0f);
    p.flyLeftRight = std::clamp(p.flyLeftRight, -1.0f, 1.0f);
    p.flySkid = std::clamp(p.inSkid, -1.0f, 1.0f);
    if (p.inHorn && c.rot[8] > 0.0f) {
        p.flySkid = 0.0f;
        const V3 right(&c.rot[0]), fwd(&c.rot[3]), mv(p.mv);
        const V3 levelFwd = Norm(Cross(V3(0.0f, 0.0f, 1.0f), right));
        const V3 levelSide = Norm(Cross(fwd, V3(0.0f, 0.0f, 1.0f)));
        p.flyUpDown = std::clamp(Dot(levelFwd, mv) * h.fly.pitchStab, -2.0f, 2.0f);
        p.flyLeftRight = std::clamp(Dot(levelSide, mv) * h.fly.rollStab, -2.0f, 2.0f);
    }
    if (c.health < 250.0f) { p.flyAccel = -0.1f; p.flySkid += 0.5f; }
}

// CHeli::ProcessFlyingCarStuff, at the end of each CAutomobile::ProcessControl: the player's rotor winds up to
// 0.22 and flies once past 0.15 (not while it sits on all its wheels with the collective down); an empty heli's
// rotor winds down. Retail adds 0.001 a frame at its 30 fps; here that rate is kept per real time.
void Vehicles::HeliFlyingCarStuff(VehicleInstance& c, const HandlingData& h, float ts, bool player, const VehicleWorld& world) {
    CarPhys& p = c.phys;
    if (!player) {
        const float drop = ts * 0.00055f;
        if (p.rotorSpeed <= drop) p.rotorSpeed = 0.0f;
        else { p.fakePhysics = 0; p.rotorSpeed -= drop; }
        return;
    }
    if (p.rotorSpeed < 0.22f) p.rotorSpeed += (IsRcHeli(c.model) ? 0.003f : 0.001f) * ts * (30.0f / 50.0f);
    if (p.rotorSpeed <= 0.15f) return;
    Body b = Load(c, h);
    const bool parked = p.numContactWheels > 3 && p.flyAccel <= 0.0f && std::fabs(b.mv.x) <= 0.02f && std::fabs(b.mv.y) <= 0.02f &&
                        std::fabs(b.mv.z) <= 0.02f;
    if (parked) return;
    FlyingControlHeli(b, h.fly, ts, world.wind, IsRcHeli(c.model), p.flySkid, p.flyUpDown, p.flyLeftRight, p.flyAccel);
    Store(b, c);
}

// CHeli::ProcessFlyingCarStuff -> CVehicle::DoBladeCollision / BladeColSectorList (the player's heli, rotor past
// 0.015): retail tests a sphere of the rotor's radius at static_rotor and keeps the colpoints within 0.1 of the
// blade disc; here 16 spokes across that disc find the nearest strike in its plane. A spinning rotor (over 0.15) is
// knocked back by a soft collision (elasticity 1) and a turn force against the blade's sweep, then drops to 0.15;
// a slow one bounces back when it is under 0.075. Each strike sparks and records damage of at least
// 100 x mass / 3000. Only the map is struck here: peds and other vehicles are not yet.
void Vehicles::HeliBladeCollision(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, const VehicleWorld& world) {
    CarPhys& p = c.phys;
    if (p.rotorSpeed <= 0.015f || vm.rotorRadius <= 0.1f || !world.line) return;
    constexpr float kSoftDepthMin = 0.5f, kSoftDepthMult = 2.0f, kSoftSpeedMult = 0.95f;   // SOFTCOL_*
    Body b = Load(c, h);
    const V3 centre = b.pos + b.Rot(V3(vm.rotorPos));
    const V3 rotorUp = -b.up;   // rotorType -3: the disc faces down
    // the disc swept by 16 spokes (the nearest strike), each hit in the blade plane as the filter keeps
    float best = 1.0f;
    V3 point, normal;
    for (int k = 0; k < 16; k++) {
        const float a = (float)k * (6.2831853f / 16.0f);
        const V3 spoke = b.right * std::cos(a) + b.fwd * std::sin(a);
        const V3 end = centre + spoke * vm.rotorRadius;
        float n[3];
        const float t = world.line(&centre.x, &end.x, n);
        if (t < best) { best = t; point = centre + spoke * (vm.rotorRadius * t); normal = V3(n); }
    }
    if (best >= 1.0f) return;
    const float onUp = Dot(point - centre, rotorUp);
    const V3 onRotor = point - rotorUp * onUp;
    const V3 rotorSize = b.up * -0.2f;
    V3 force = Cross(rotorSize, onRotor - centre) + b.mv;
    const float speed = Len(force);
    if (speed > 0.0f && onBladeSparks) {
        const V3 dir = force / speed;
        onBladeSparks(&onRotor.x, &dir.x, speed * 15.0f);
    }
    float di = 0.0f;
    if (p.rotorSpeed <= 0.15f) {
        if (p.rotorSpeed < 0.075f && p.rotorSpeed > 0.0f) p.rotorSpeed = -p.rotorSpeed;
    } else {
        // CPhysical::ApplySoftCollision against a building (its collision mass uses |normal|^2, as retail)
        const V3 at = point - b.pos;
        const float along = Dot(b.Speed(at), normal);
        const float colMass = 1.0f / (1.0f / b.turnMass + 1.0f / b.mass);
        const float depth = std::max(0.0f, vm.rotorRadius - Len(centre - point));
        if (along < 0.0f) {
            di = std::min(kSoftDepthMin, depth) * ts * kSoftDepthMult * colMass * 0.008f - colMass * along * kSoftSpeedMult;
            if (di != 0.0f) { b.ApplyMoveForce(normal * di); b.ApplyTurnForce(normal * di, at); }
            di = std::fabs(di);
        }
        b.ApplyTurnForce(force * (b.turnMass * -0.0005f), onRotor - centre);
        p.rotorSpeed = 0.15f;
    }
    Store(b, c);
    const float record = std::max(di, 100.0f * b.mass / 3000.0f);   // SetDamagedPieceRecord
    if (record > p.damageIntensity) {
        p.damageIntensity = record; p.damager = -1;
        point.To(p.impactPos); normal.To(p.impactNormal);
    }
}

// CHeli::PreRender: the main rotor turns about z (faster on the big helis), the tail rotor about x.
void Vehicles::HeliRotors(VehicleInstance& c, float ts) {
    CarPhys& p = c.phys;
    const int m = c.model;
    const bool fast = m == MODEL_SPARROW || m == MODEL_SEASPAR || m == MODEL_MAVERICK || m == MODEL_VCNMAV || m == MODEL_POLMAV;
    p.rotorZ = std::fmod(p.rotorZ - ts * p.rotorSpeed * (fast ? 1.66f : 1.0f), kTwoPi);
    p.rotor2Z = std::fmod(p.rotor2Z - ts * p.rotorSpeed * (m == MODEL_LEVIATHN ? 2.0f : 2.3f), kTwoPi);
}

// CMatrix::SetRotateZ / SetRotateX on the rotor frames: the turn replaces the frame's rotation, its place stays.
void Vehicles::HeliFrameMatrices(const VehicleInstance& c, const VehicleModel& vm, float nodes[VehicleModel::kMoveNodes][16]) const {
    for (int k = 0; k < VehicleModel::kMoveNodes; k++) {
        const VehicleModel::BikeNode& bn = vm.bikeNodes[k];
        if (!bn.found) { std::memset(nodes[k], 0, sizeof(nodes[k])); nodes[k][0] = nodes[k][5] = nodes[k][10] = nodes[k][15] = 1.0f; continue; }
        if (k < 15 || k > 18) { Mul4(bn.local, bn.parent >= 0 ? nodes[bn.parent] : bn.base, nodes[k]); continue; }   // as modelled
        float m[16];
        if (k <= 16) RotZ(c.phys.rotorZ, m);
        else RotX(c.phys.rotor2Z, m);
        for (int a = 12; a < 15; a++) m[a] = bn.local[a];
        Mul4(m, bn.parent >= 0 ? nodes[bn.parent] : bn.base, nodes[k]);
    }
}

namespace {
// CPlane's propeller: PLANE_MIN_PROP_SPEED, PLANE_STD_PROP_SPEED, PLANE_MAX_PROP_SPEED, PLANE_ROC_PROP_SPEED
constexpr float kPropMin = 0.05f, kPropStd = 0.18f, kPropMax = 0.34f, kPropRoc = 0.01f;
enum { MODEL_RCBARON = 464, MODEL_VORTEX = 539 };   // the Vortex hovers on its own rules below

void SetGear(VehicleInstance& c, bool up) {   // CPlane::SetGearUp / SetGearDown (the drag follows the gear status)
    c.phys.gearStatus = up ? 1.0f : 0.0f;
    for (uint8_t& w : c.wheelStatus) w = up ? 2 : 0;
}
}  // namespace

bool Vehicles::UsesPlanePhysics(int model) const {
    if (model == MODEL_RCBARON) return false;
    const VehicleInfo* info = Info(model);
    if (!info) return false;
    std::string t = info->type;
    for (char& ch : t) ch = (char)std::tolower((unsigned char)ch);
    const HandlingData* h = Handling(info->handling);
    return t == "plane" && h && h->fly.found;
}

// CPlane::ProcessControlInputs (keyboard): the stick and rudder ease toward the pad (a fifth per time step),
// throttle on accelerate / brake, and the landing gear key folds the gear away once every wheel is off the ground.
void Vehicles::PlaneInputs(VehicleInstance& c, const HandlingData& h, float ts, bool player) {
    CarPhys& p = c.phys;
    if (!player) { p.flyAccel = 0.0f; p.smokeEjector = false; return; }   // CPlane::ProcessControl: no driver, no smoke
    // CPlane::ProcessControl: the landing gear key turns the Cropduster's spray and the Stuntplane's smoke on and off
    if ((c.model == 512 || c.model == 513) && p.inGear && !p.prevGearKey) p.smokeEjector = !p.smokeEjector;
    p.prevGearKey = p.inGear;
    if (!c.engineOn) p.smokeEjector = false;
    // Mouse flying as the heli's, with 0.0015 a count and the 0.99 ease below 0.15; keys ease the stick a fifth a step.
    const bool padSteer = p.inSteer != 0.0f || p.inLeanFwd != 0.0f;
    const bool moved = p.inMouseDX != 0.0f || p.inMouseDY != 0.0f;
    if (moved || (p.lastInputMouse && !padSteer && (p.flyLeftRight != 0.0f || p.flyUpDown != 0.0f))) {
        p.lastInputMouse = true;
        p.flyLeftRight += p.inMouseDX * 0.0015f;
        p.flyUpDown += p.inMouseDY * 0.0015f;
        const float ease = std::pow(0.99f, ts);
        if (std::fabs(p.flyLeftRight) < 0.15f) p.flyLeftRight *= ease;
        if (std::fabs(p.flyUpDown) < 0.15f) p.flyUpDown *= ease;
    } else if (!(p.lastInputMouse && !padSteer)) {
        p.lastInputMouse = false;
        p.flyLeftRight += (p.inSteer - p.flyLeftRight) * ts * 0.2f;
        p.flyUpDown += (-p.inLeanFwd - p.flyUpDown) * ts * 0.2f;   // steer up: nose down
    }
    p.flyLeftRight = std::clamp(p.flyLeftRight, -1.0f, 1.0f);
    p.flyUpDown = std::clamp(p.flyUpDown, -1.0f, 1.0f);
    const float skidPad = c.model == MODEL_VORTEX ? p.inSteer : p.inSkid;   // the Vortex: the steering, not the look keys
    p.flySkid = std::clamp(p.flySkid + (skidPad - p.flySkid) * ts * 0.2f, -1.0f, 1.0f);
    p.flyAccel = p.inAccel - p.inBrake;
    if (c.model == 520 && std::fabs(p.inGunUpDown * 128.0f) > 10.0f) {   // the Hydra's nozzles: 25 a step at full, 0 .. 5000
        const int d = (int)(p.inGunUpDown * 128.0f * ts * 25.0f * 0.0078125f);
        c.hoist = std::min(5000, std::max(0, c.hoist + d));
    }
    const bool airborne = p.compPrev[0] == 1.0f && p.compPrev[1] == 1.0f && p.compPrev[2] == 1.0f && p.compPrev[3] == 1.0f;
    if (p.inGear && airborne && h.fly.gearUpR < 1.0f) {
        if (p.gearStatus == 0.0f) { for (uint8_t& w : c.wheelStatus) w = 2; p.gearStatus += ts * 0.02f; }
        else if (p.gearStatus == 1.0f) p.gearStatus = ts * 0.02f - 1.0f;
        else p.gearStatus = -p.gearStatus;
    } else if (p.gearStatus >= 0.0f) {
        if (p.gearStatus > 0.0f && p.gearStatus < 1.0f && (p.gearStatus += ts * 0.02f) >= 1.0f) SetGear(c, true);
    } else if ((p.gearStatus += ts * 0.02f) >= 0.0f) SetGear(c, false);
}

// The wheel side of CPlane::ProcessControlInputs (and ProcessAI for an empty plane): no drive, the rudder steers
// the nose wheel (less at speed), the brake bites gently at speed, and a plane at rest holds itself.
void Vehicles::PlaneWheelInputs(VehicleInstance& c, const HandlingData& h, bool player) {
    CarPhys& p = c.phys;
    const V3 fwd(&c.rot[3]), mv(p.mv);
    const float fwdSpeed = Dot(mv, fwd);
    p.gas = 0.0f;
    if (player) {
        p.steerAngle = -(h.steeringLock * 0.017453292f * p.flySkid * std::max(0.1f, 1.0f - fwdSpeed));
        if (c.model == MODEL_VORTEX && p.handbrake) { p.steerAngle *= 1.5f; p.flySkid *= 1.5f; }
        if (fwdSpeed > 0.0f && p.inBrake > 0.0f) p.brake = fwdSpeed <= 0.35f ? p.inBrake : p.inBrake * 0.5f;
        else if (Len(mv) < 0.1f && p.inBrake < 10.0f / 255.0f && p.inAccel < 10.0f / 255.0f) p.brake = 0.5f;
        else p.brake = 0.0f;
    } else {
        p.steerAngle = 0.0f;
        p.handbrake = false;
        p.brake = Len2(mv) < 0.01f ? 0.2f : Len2(mv) < 0.1f ? 1.0f : 0.0f;
    }
}

// CPlane::ProcessFlyingCarStuff: the propeller eases toward the throttle's speed (held back above the ceiling),
// and with the engine running and the propeller turning (or the plane moving) FlyingControl flies it. A burning
// plane pulls to one side and drops its nose. An empty plane's propeller runs down.
void Vehicles::PlaneFlyingCarStuff(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world) {
    CarPhys& p = c.phys;
    p.propAngle = std::fmod(p.propAngle + ts * p.planePropSpeed, kTwoPi);   // CPlane::PreRender
    if (!player) {
        const float drop = ts * 0.001f;
        if (p.planePropSpeed <= drop) { p.planePropSpeed = 0.0f; return; }
        p.fakePhysics = 0;
        p.planePropSpeed -= p.planePropSpeed > kPropStd ? ts * 0.003f : drop;
        return;
    }
    float skid = p.flySkid, upDown = p.flyUpDown, leftRight = p.flyLeftRight;
    if (c.health < 250.0f) {
        skid += 0.5f;
        const float roll = std::fabs(std::atan2(-c.rot[2], c.rot[8]));   // CPlaceable::GetRoll
        if (roll < 2.3561945f) leftRight += 0.75f;
        if (roll > 1.5707964f) upDown += 0.5f;
    }
    float target = kPropStd;
    if (p.flyAccel > 0.0f) target += (kPropMax - kPropStd) * p.flyAccel;
    else if (p.flyAccel < 0.0f) target += (kPropStd - kPropMin) * p.flyAccel;
    const float above = HeightAboveCeiling(c.z, false);
    if (above > 0.0f) {
        target *= std::max(0.0f, above * 0.02f);
        p.flyAccel = std::max(-1.0f, p.flyAccel - above * 0.04f);
    }
    p.planePropSpeed += (target - p.planePropSpeed) * ts * kPropRoc;
    if (!c.engineOn || !(p.planePropSpeed > kPropMin || Len2(V3(p.mv)) > 0.05f)) return;
    Body b = Load(c, h);
    bool airborne = p.compPrev[0] == 1.0f && p.compPrev[1] == 1.0f && p.compPrev[2] == 1.0f && p.compPrev[3] == 1.0f;
    if (c.model == 460 && p.boatInWater) {   // FlyingControl: a Skimmer sitting in the water idles without thrust
        const V3 air = b.mv - V3(world.wind) * h.fly.windMult;
        if (Dot(b.fwd, air) <= 0.2f) airborne = false;
    }
    const float nozzle = c.model == 520 ? (float)c.hoist * 1.5707964f / 5000.0f : -1.0f;
    if (c.model == 520 && c.hoist >= 3000) {   // HARRIER_NOZZLE_SWITCH_LIMIT: hovering, it flies as a helicopter on the Hunter's flying lines
        const VehicleInfo* hi = Info(425);
        const HandlingData* hh = hi ? Handling(hi->handling) : nullptr;
        int contact = 0;
        for (float cp : p.compPrev) contact += cp < 1.0f;
        if (hh && hh->fly.found && (p.flyAccel > 0.0f || contact < 4)) {
            FlyingControlHeli(b, hh->fly, ts, world.wind, false, skid, upDown, -leftRight, 0.25f * p.flyAccel, nozzle);
            Store(b, c);
        }
        return;
    }
    FlyingControlPlane(b, h.fly, vm.colMin[1], ts, world.wind, airborne, p.gearStatus, skid, upDown, leftRight, p.flyAccel, nozzle, c.model == MODEL_VORTEX);
    Store(b, c);
}

// CPlane::PreRender: the still propellers spin (the blurred ones stay hidden), the rudder follows the pedals,
// the elevators the stick's pitch and the ailerons its roll. The hinges turn about the frame's own axes.
void Vehicles::PlaneFrameMatrices(const VehicleInstance& c, const VehicleModel& vm, float nodes[VehicleModel::kMoveNodes][16]) const {
    const CarPhys& p = c.phys;
    // CPlane::PreRender: the gear legs fold by |m_fLandingGearStatus| x the model's angle: gear_l / gear_r about one
    // axis, misc_a / misc_b about X (the Rustler, Shamal, Hydra, Nevada, AT-400, Andromada; gaps: none)
    int gearAxis = -1;   // 0 X, 1 Y
    float gearL = 0.0f, gearR = 0.0f, miscA = 0.0f, miscB = 0.0f;
    switch (c.model) {
    case 476: gearAxis = 1; gearL = -1.4835299f; gearR = 1.4835299f; break;                                         // Rustler
    case 519: case 577: gearAxis = 1; gearL = -1.4835299f; gearR = 1.4835299f; miscA = 2.268928f; break;            // Shamal, AT-400
    case 520: gearAxis = 0; gearL = gearR = -1.5707964f; miscA = -1.3962635f; miscB = 2.268928f; break;            // Hydra
    case 553: gearAxis = 0; gearL = gearR = 1.3089969f; break;                                                      // Nevada
    case 592: gearAxis = 0; gearL = gearR = 2.268928f; miscA = -2.268928f; break;                                   // Andromada
    default: break;
    }
    const float fold = std::fabs(p.gearStatus);
    for (int k = 0; k < VehicleModel::kMoveNodes; k++) {
        const VehicleModel::BikeNode& bn = vm.bikeNodes[k];
        const bool gearMoves = gearAxis >= 0 && (k == 24 || k == 25 || (k == 28 && miscA != 0.0f) || (k == 29 && miscB > 0.0f));
        const bool moves = (k >= 11 && k <= 14) || (k >= 19 && k <= 23) || (c.model == 520 && (k == 26 || k == 27)) || gearMoves;
        if (!bn.found) { std::memset(nodes[k], 0, sizeof(nodes[k])); nodes[k][0] = nodes[k][5] = nodes[k][10] = nodes[k][15] = 1.0f; continue; }
        const VehicleInstance::BouncingPanel* loose = nullptr;   // CBouncingPanel on this frame
        for (const VehicleInstance::BouncingPanel& bp : c.panels) if (bp.node == k) loose = &bp;
        if (!moves && !loose) { Mul4(bn.local, bn.parent >= 0 ? nodes[bn.parent] : bn.base, nodes[k]); continue; }   // as modelled
        float m[16];
        if (!moves) std::memcpy(m, bn.local, sizeof(m));
        else switch (k) {
        case 26: case 27: RotX((float)c.hoist * 1.5707964f / 5000.0f, m); break;   // CPlane::PreRender: the Hydra's nozzles
        case 24: case 25: {
            const float a = fold * (k == 24 ? gearL : gearR);
            if (gearAxis == 0) RotX(a, m); else RotY(a, m);
            break;
        }
        case 28: RotX(fold * miscA, m); break;
        case 29: RotX(fold * miscB, m); break;
        case 11: case 12: RotY(2.0f * p.propAngle, m); break;
        case 13: case 14: RotY(-2.0f * p.propAngle, m); break;
        case 19: RotZ(p.flySkid * 0.6981317f, m); break;
        case 20: case 21: RotX(-p.flyUpDown * 0.43633232f, m); break;
        case 22: RotX(p.flyLeftRight * 0.5235988f, m); break;
        default: RotX(-p.flyLeftRight * 0.5235988f, m); break;
        }
        for (int a = 12; a < 15; a++) m[a] = bn.local[a];
        if (loose) {   // CBouncingPanel::ProcessPanel writes its rotation into two entries of the frame matrix
            if (loose->axis == 0) { m[1] = loose->rot[2]; m[2] = loose->rot[1]; }
            else if (loose->axis == 1) { m[4] = loose->rot[2]; m[6] = loose->rot[0]; }
            else { m[9] = loose->rot[0]; m[8] = loose->rot[1]; }
        }
        Mul4(m, bn.parent >= 0 ? nodes[bn.parent] : bn.base, nodes[k]);
    }
}

// The fire_med CPlane::VehicleDamage leaves on the part it hit: flames at the frame for the timer's 2 to 4 s.
void Vehicles::PlaneStumpFire(VehicleInstance& c, float dt, const std::function<void(const float*)>& onFire) const {
    if (c.planeFireMs <= 0.0f || c.planeFireNode < 0) return;
    c.planeFireMs -= dt * 1000.0f;
    auto mi = m_models.find(c.model);
    if (mi == m_models.end()) return;
    const VehicleModel::BikeNode& bn = mi->second.bikeNodes[c.planeFireNode];
    if (!bn.found) return;
    float g[16];
    Mul4(bn.local, bn.base, g);
    V3 right, fwd, up;
    CarFrame(c, right, fwd, up);
    const V3 at = V3(c.x, c.y, c.z) + right * g[12] + fwd * g[13] + up * g[14];
    const float p[3] = {at.x, at.y, at.z};
    onFire(p);
}

// CPlane::VehicleDamage (collisions): the knock times the handling's collision multiplier costs health (halved for
// the player's plane, a quarter for the others' or two thirds when the player rammed them); a knock over
// 5000 x mass / 10000, or one that takes the last health, blows the plane up. One over 500 x max(1, mass / 10000)
// on a plane over a tonne damages the nearest of its parts (props, rudder, elevators, ailerons, gear, misc): the
// part goes loose (a control surface flaps on a CBouncingPanel) and the stump burns for 2 to 4 s.
void Vehicles::PlaneDamage(VehicleInstance& c, const HandlingData& h, const VehicleModel& vm, float di, bool player, bool byPlayer) {
    const float scale = h.mass / 10000.0f;
    const float destroy = scale * 5000.0f, threshold = std::max(scale, 1.0f) * 500.0f;
    float loss = di * h.collisionDamageGame;
    loss *= player ? 0.5f : byPlayer ? 0.6666667f : 0.25f;
    const float before = c.health;
    c.health -= loss;
    if (c.health <= 0.0f && before > 0.0f) c.health = 1.0f;
    if (di > destroy || (c.health < 1.0f && before > 0.0f)) {   // BlowUpCar
        c.health = 0.0f; c.engineStatus = 225; c.burnTime = 5.0f;
        return;
    }
    if (di <= threshold || h.mass <= 1000.0f) return;
    // the part nearest the knock (ePlaneNodes 12..24 on this model's frames)
    static const int kParts[13] = {11, 12, 13, 14, 19, 20, 21, 22, 23, 24, 25, 28, 29};
    const V3 right(&c.rot[0]), fwd(&c.rot[3]), up(&c.rot[6]);
    const V3 d = V3(c.phys.impactPos) - V3(c.x, c.y, c.z);
    const V3 hit(Dot(d, right), Dot(d, fwd), Dot(d, up));
    int part = -1;
    float best = 1000.0f * 1000.0f;
    for (int k : kParts) {
        const VehicleModel::BikeNode& bn = vm.bikeNodes[k];
        if (!bn.found) continue;
        float g[16];
        Mul4(bn.local, bn.base, g);
        const float d2 = Len2(V3(g[12], g[13], g[14]) - hit);
        if (d2 < best) { best = d2; part = k; }
    }
    if (part < 0) return;
    if (part == 19 && (RandInt(2) & 1)) {   // the rudder: half the time an elevator takes it
        if (vm.bikeNodes[20].found && (RandInt(2) & 1)) part = 20;
        else if (vm.bikeNodes[21].found) part = 21;
    }
    uint8_t& st = c.aeroDamage[part];
    if (st < 2) {   // CDamageManager::ProgressAeroplaneDamage
        st++;
        if (st == 1 && part >= 19 && part <= 25) {   // the control surfaces and gear legs flap loose
            bool have = false;
            for (const VehicleInstance::BouncingPanel& bp : c.panels) have = have || bp.node == part;
            if (!have)
                for (VehicleInstance::BouncingPanel& bp : c.panels)
                    if (bp.bumper < 0 && bp.node < 0) { bp = VehicleInstance::BouncingPanel(); bp.node = part; bp.limit = -0.02f; break; }
        }
    }
    c.planeFireNode = part;
    c.planeFireMs = (float)(2000 + RandInt(2001));
}
