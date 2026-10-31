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

// CVehicle::FlyingControl, FLIGHT_MODEL_HELI, then the turn resistance and speed caps every model shares.
// skid: the tail rotor (look right +1), upDown: pitch (+ nose up), leftRight: roll (- to the right), accel: collective.
void FlyingControlHeli(Body& b, const HandlingData::Fly& f, float ts, const float wind[3], bool rc,
                       float skid, float upDown, float leftRight, float accel) {
    if (ts <= 0.0f) return;
    const V3 windDir(wind);
    const V3 vel = b.mv - windDir * f.windMult;   // the speed through the air
    const V3 com = b.ComW();

    b.mv *= std::pow(f.moveRes, ts);
    const V3 rotorUp = b.up;
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

    // turn resistance per body axis: plain decay, or (with a speed resistance) a pull that grows with the spin
    const float res[3] = {std::pow(f.turnRes[0], ts), std::pow(f.turnRes[1], ts), std::pow(f.turnRes[2], ts)};
    const float k = Len(b.mv) + 1.0f;
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
