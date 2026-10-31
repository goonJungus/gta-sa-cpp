// Aircraft: CHeli's controls and rotor, and CVehicle::FlyingControl (0x6D85F0), the forces that keep a
// CAutomobile in the air. Like the rest of the car physics it works per 1/50 s with retail's constants.
#define NOMINMAX
#include "Vehicle.h"
#include "PhysicsCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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
                        float skid, float upDown, float leftRight, float accel, float nozzle = -1.0f) {
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
    if (airborne || accel > 0.0f) {
        thrust = (accel + 1.0f) * f.thrust * 0.5f;
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
    b.ApplyTurnForce(b.right * ((airSpeed * f.yaw * (skid * steerMul) + std::fabs(tailSide) * f.yawStab * tailSide) * ts * b.turnMass), com + tail);
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
// steer left / right, the tail rotor on look left / right. A burning heli loses its lift.
void Vehicles::HeliInputs(VehicleInstance& c, bool player) {
    CarPhys& p = c.phys;
    if (!player) { p.flyAccel = p.flyUpDown = p.flyLeftRight = p.flySkid = 0.0f; return; }
    p.flyAccel = p.inAccel - p.inBrake;
    p.flyUpDown = std::clamp(-p.inLeanFwd, -1.0f, 1.0f);       // steer up (num 8) is the stick forward: nose down
    p.flyLeftRight = std::clamp(-p.inSteer, -1.0f, 1.0f);
    p.flySkid = std::clamp(p.inSkid, -1.0f, 1.0f);
    if (c.health < 250.0f) p.flyAccel = -0.1f;
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
        if (k < 15 || !bn.found) { std::memset(nodes[k], 0, sizeof(nodes[k])); nodes[k][0] = nodes[k][5] = nodes[k][10] = nodes[k][15] = 1.0f; continue; }
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
enum { MODEL_SKIMMER = 460, MODEL_RCBARON = 464, MODEL_VORTEX = 539 };

void SetGear(VehicleInstance& c, bool up) {   // CPlane::SetGearUp / SetGearDown (the drag follows the gear status)
    c.phys.gearStatus = up ? 1.0f : 0.0f;
    for (uint8_t& w : c.wheelStatus) w = up ? 2 : 0;
}
}  // namespace

bool Vehicles::UsesPlanePhysics(int model) const {
    if (model == MODEL_SKIMMER || model == MODEL_RCBARON || model == MODEL_VORTEX) return false;
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
    if (!player) { p.flyAccel = 0.0f; return; }
    p.flyLeftRight = std::clamp(p.flyLeftRight + (p.inSteer - p.flyLeftRight) * ts * 0.2f, -1.0f, 1.0f);
    p.flyUpDown = std::clamp(p.flyUpDown + (-p.inLeanFwd - p.flyUpDown) * ts * 0.2f, -1.0f, 1.0f);   // steer up: nose down
    p.flySkid = std::clamp(p.flySkid + (p.inSkid - p.flySkid) * ts * 0.2f, -1.0f, 1.0f);
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
    const bool airborne = p.compPrev[0] == 1.0f && p.compPrev[1] == 1.0f && p.compPrev[2] == 1.0f && p.compPrev[3] == 1.0f;
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
    FlyingControlPlane(b, h.fly, vm.colMin[1], ts, world.wind, airborne, p.gearStatus, skid, upDown, leftRight, p.flyAccel, nozzle);
    Store(b, c);
}

// CPlane::PreRender: the still propellers spin (the blurred ones stay hidden), the rudder follows the pedals,
// the elevators the stick's pitch and the ailerons its roll. The hinges turn about the frame's own axes.
void Vehicles::PlaneFrameMatrices(const VehicleInstance& c, const VehicleModel& vm, float nodes[VehicleModel::kMoveNodes][16]) const {
    const CarPhys& p = c.phys;
    for (int k = 0; k < VehicleModel::kMoveNodes; k++) {
        const VehicleModel::BikeNode& bn = vm.bikeNodes[k];
        const bool moves = (k >= 11 && k <= 14) || (k >= 19 && k <= 23) || (c.model == 520 && k >= 26);
        if (!bn.found) { std::memset(nodes[k], 0, sizeof(nodes[k])); nodes[k][0] = nodes[k][5] = nodes[k][10] = nodes[k][15] = 1.0f; continue; }
        if (!moves) { Mul4(bn.local, bn.parent >= 0 ? nodes[bn.parent] : bn.base, nodes[k]); continue; }   // as modelled
        float m[16];
        switch (k) {
        case 26: case 27: RotX((float)c.hoist * 1.5707964f / 5000.0f, m); break;   // CPlane::PreRender: the Hydra's nozzles
        case 11: case 12: RotY(2.0f * p.propAngle, m); break;
        case 13: case 14: RotY(-2.0f * p.propAngle, m); break;
        case 19: RotZ(p.flySkid * 0.6981317f, m); break;
        case 20: case 21: RotX(-p.flyUpDown * 0.43633232f, m); break;
        case 22: RotX(p.flyLeftRight * 0.5235988f, m); break;
        default: RotX(-p.flyLeftRight * 0.5235988f, m); break;
        }
        for (int a = 12; a < 15; a++) m[a] = bn.local[a];
        Mul4(m, bn.parent >= 0 ? nodes[bn.parent] : bn.base, nodes[k]);
    }
}
