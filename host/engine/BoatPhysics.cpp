// Boats: CBoat::ProcessControl and ProcessControlInputs, cBuoyancy::ProcessBuoyancyBoat, and
// CVehicle::ProcessBoatControl / ApplyBoatWaterResistance, in game units (per 1/50 s).
//
// The hull floats on a 3x3 grid of points over its collision box: each point under the surface lifts by the
// square of its depth (weighted by the model's volume table), the propeller pushes from under the stern while
// it is in the water, the hull planes up with speed, and the water resists along each axis. The water is
// flat here (no waves yet), so every point's surface normal is straight up.

#include "Vehicle.h"
#include "PhysicsCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

using namespace physics;

namespace {

// cBuoyancy::afBoatVolumeDistribution*: how much of each grid point's depth lifts, [rear .. front][left .. right]
const float kVolume[3][3] = {{0.75f, 0.9f, 0.75f}, {0.95f, 1.0f, 0.95f}, {0.4f, 0.7f, 0.4f}};
const float kVolumeSail[3][3] = {{0.55f, 0.95f, 0.55f}, {0.75f, 1.1f, 0.75f}, {0.3f, 0.8f, 0.3f}};
const float kVolumeDinghy[3][3] = {{0.65f, 0.85f, 0.65f}, {0.85f, 1.1f, 0.85f}, {0.65f, 0.95f, 0.65f}};
const float kVolumeSpeed[3][3] = {{0.7f, 0.9f, 0.7f}, {0.95f, 1.0f, 0.95f}, {0.6f, 0.7f, 0.6f}};

enum : int { kSkimmer = 460, kPredator = 430, kSqualo = 446, kSpeeder = 452, kReefer = 453, kTropic = 454, kCoastguard = 472, kDinghy = 473,
             kMarquis = 484, kJetmax = 493, kLaunch = 595 };

// CPlane's propeller speeds, which boats share
constexpr float kPropStd = 0.18f, kPropMax = 0.34f, kPropRoc = 0.01f;

struct Buoyancy {
    bool inWater = false;
    float level = 0.0f;       // the surface at the hull's position
    float immersion = 0.0f;   // m_fEntityWaterImmersion
    V3 turnPoint, force;
};

// cBuoyancy::ProcessBuoyancyBoat (with PreCalcSetup, FindWaterLevelNorm and SimpleSumBuoyancyData)
bool BuoyancyBoat(int model, const VehicleModel& vm, Body& b, float buoyancy, float damping, float ts, bool hitWall,
                  const VehicleWorld& world, Buoyancy& out) {
    if (!world.water || !world.water(b.pos.x, b.pos.y, &out.level)) return false;
    V3 mn(vm.colMin), mx(vm.colMax);
    switch (model) {   // PreCalcSetup: each hull's box trimmed to where it floats
    case kSqualo: mx.y *= 0.9f; mn.y *= 0.9f; break;
    case kSpeeder: mx.y *= 1.25f; mn.y *= 0.83f; break;
    case kReefer: case kJetmax: mn.y *= 0.9f; break;
    case kTropic: mx.y *= 1.3f; mn.y *= 0.82f; mn.z -= 0.2f; break;
    case kCoastguard: mx.y *= 1.1f; mn.y *= 0.9f; mn.z -= 0.3f; break;
    case kDinghy: mx.y *= 1.3f; mn.y *= 0.9f; mn.z -= 0.2f; break;
    case kMarquis: mx.y *= 1.1f; mn.y *= 0.9f; break;
    case kLaunch: mx.y *= 1.25f; mn.y *= 0.8f; mn.z -= 0.1f; break;
    default: mx.y *= 1.05f; mn.y *= 0.9f; break;
    }
    const V3 half = (mx - mn) * 0.5f;
    const float scale = (half.z > half.x && half.z > half.y) ? 1.0f / half.z : (half.y > half.x && half.y > half.z) ? 1.0f / half.y : 1.0f / half.x;
    const V3 norm = half * scale;
    const float (*vol)[3] = kVolume;
    if (model == kSqualo || model == kSpeeder || model == kJetmax || model == kLaunch) vol = kVolumeSpeed;
    else if (model == kCoastguard || model == kDinghy) vol = kVolumeDinghy;
    else if (model == kMarquis) vol = kVolumeSail;
    const float ratio = 1.0f / ((mx.z - mn.z) * 9.0f);
    float checked = 1.0f;
    for (int ix = 0; ix < 3; ix++)
        for (int iy = 0; iy < 3; iy++) {
            V3 pt(mn.x + half.x * (float)ix, mn.y + half.y * (float)iy, 0.0f);
            const V3 tr = b.Rot(pt);
            float level = out.level;
            if (!world.water(b.pos.x + tr.x, b.pos.y + tr.y, &level)) level = out.level;
            pt.z = level - (tr.z + b.pos.z);
            if (pt.z < mn.z) continue;   // this corner is out of the water
            pt.z = std::min(pt.z, mx.z);
            const float v = vol[iy][ix];
            float d = std::fabs(pt.z - mn.z) - (1.0f - v);
            if (d < 0.0f) continue;
            d = (d * v) * (d * v);
            out.immersion += d;
            const V3 cur(pt.x * norm.x, pt.y * norm.y, (pt.z + mn.z) * norm.z * 0.5f);
            const float nc = 1.0f / checked;
            out.turnPoint = cur * (nc * d) + out.turnPoint * (1.0f - nc);
            checked += 1.0f;
            out.inWater = true;
            const float lift = ts * buoyancy * (d * ratio);
            const V3 up(0.0f, 0.0f, 1.0f);   // a flat sea: the wave normal is straight up
            const V3 speed = b.Speed(b.Rot(V3(pt.x, pt.y, 0.0f)));
            const float wave = std::max(0.0f, 1.0f - Dot(speed, up) * damping) * lift;
            out.force.z += wave;
            if (!hitWall) b.ApplyTurnForce(up * wave, b.Rot(pt));
        }
    out.immersion *= ratio;
    out.turnPoint = b.Rot(out.turnPoint);
    return true;
}

}  // namespace

// CBoat::PreRender: the rudder and rear flaps turn against the steering; the still propellers turn twice as fast
// as the blurred ones, each pair in opposite senses.
void Vehicles::BoatFrameMatrices(const VehicleInstance& c, const VehicleModel& vm, float nodes[VehicleModel::kMoveNodes][16]) const {
    const CarPhys& p = c.phys;
    for (int k = 0; k < VehicleModel::kMoveNodes; k++) {
        const VehicleModel::BikeNode& bn = vm.bikeNodes[k];
        if (k < 8 || k > 14 || !bn.found) { std::memset(nodes[k], 0, sizeof(nodes[k])); nodes[k][0] = nodes[k][5] = nodes[k][10] = nodes[k][15] = 1.0f; continue; }
        float m[16], r[16];
        std::memcpy(m, bn.local, sizeof(m));
        const float spin = k == 11 ? p.propAngle * 2.0f : k == 13 ? p.propAngle * -2.0f : k == 12 ? -p.propAngle : p.propAngle;
        if (k <= 10) RotZ(-p.steerAngle, r);
        else RotY(spin, r);
        Mul4(r, m, m);
        Mul4(m, bn.parent >= 0 ? nodes[bn.parent] : bn.base, nodes[k]);
    }
}

// CBoat::ProcessControl's damage: a knock past 25 (in the handling's collision units) costs the hull health.
void Vehicles::BoatDamage(VehicleInstance& c, const HandlingData& h, float di, bool player) {
    float power = di * h.collisionDamageGame;
    if (power <= 25.0f || c.health < 250.0f) return;
    const float before = c.health;
    c.health -= (power - 25.0f) / (player ? 2.0f : 4.0f);
    if (c.health <= 0.0f && before > 0.0f) c.health = 1.0f;
}

bool Vehicles::UsesBoatPhysics(int model) const {
    const VehicleInfo* info = Info(model);
    if (!info) return false;
    std::string t = info->type;
    for (char& ch : t) ch = (char)std::tolower((unsigned char)ch);
    const HandlingData* h = Handling(info->handling);
    return t == "boat" && h && h->boat.found;
}

// CBoat::ProcessControlInputs and the propeller's spin (once a frame).
void Vehicles::BoatInputs(VehicleInstance& c, const HandlingData& h, float ts, bool player) {
    CarPhys& p = c.phys;
    if (player) {
        p.brake = std::clamp(p.brake + (p.inBrake - p.brake) * 0.1f, 0.0f, 1.0f);
        if (p.brake >= 0.05f) p.gas = p.brake * -0.3f;
        else { p.brake = 0.0f; p.gas = p.inAccel; }
        p.rawSteer = std::clamp(p.rawSteer + (-p.inSteer - p.rawSteer) * 0.2f * ts, -1.0f, 1.0f);
        p.steerAngle = h.steeringLock * 0.0174532925f * std::copysign(p.rawSteer * p.rawSteer, p.rawSteer);
        // CBoat::ProcessControl: the propeller spins up towards the throttle's speed
        const float roc = p.gas != 0.0f ? kPropRoc * 5.0f : kPropRoc;
        const float target = p.gas == 0.0f ? 0.0f : p.gas < 0.0f ? (kPropStd - 0.05f) * p.gas + kPropStd : (kPropMax - kPropStd) * p.gas + kPropStd;
        p.engineSpeed += (target - p.engineSpeed) * ts * roc;
    } else {   // STATUS_ABANDONED
        p.steerAngle = 0.0f; p.brake = 0.5f; p.gas = 0.0f;
        p.engineSpeed *= 0.95f;
    }
    p.propAngle += p.engineSpeed * ts;   // CBoat::PreRender
    while (p.propAngle > 6.2831855f) p.propAngle -= 6.2831855f;
}

// One CBoat::ProcessControl step: CVehicle::ProcessBoatControl, gravity and drag, then the collision.
void Vehicles::BoatStep(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world) {
    CarPhys& p = c.phys;
    p.player = player;
    const bool hitWall = p.hitWall;   // GetHasHitWall: the last step's collision
    p.hitWall = false;
    BoatControl(c, vm, h, ts, player, hitWall, world);
    Body b = Load(c, h);
    // CPhysical::ProcessControl: ApplyFriction, ApplyGravity, ApplyAirResistance
    b.mv += b.fmv; b.tv += b.ftv;
    b.fmv = V3(); b.ftv = V3();
    b.mv.z -= ts * 0.008f;
    {
        const float air = h.dragMult <= 0.01f ? h.dragMult : h.dragMult / 1000.0f / 2.0f;
        b.mv *= std::pow(std::max(0.0f, 1.0f - Len(b.mv) * air), ts);
        b.tv *= 0.99f;
    }
    Store(b, c);
    CarCollision(c, vm, h, ts, world, false);
}

// CVehicle::ProcessBoatControl. The Skimmer floats on it too, with its own rules: no propeller in the water (its
// engine flies it), the planing lift scaled by throttle + 1, the hull barely floating once it lies upside down and
// still, and a nose-first plunge braked hard for 300 ms (no turn resistance meanwhile).
void Vehicles::BoatControl(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, bool hitWall, const VehicleWorld& world) {
    CarPhys& p = c.phys;
    const HandlingData::Boat& bh = h.boat;
    const bool skimmer = c.model == kSkimmer;
    Body b = Load(c, h);
    const bool postCollision = p.lastDI > 0.0f && p.impactNormal[2] > 0.1f;
    const float buoyancy = h.mass * 0.8f / (float)std::max(1, h.percentSubmerged);
    Buoyancy bu;
    const bool floated = BuoyancyBoat(c.model, vm, b, buoyancy, h.suspDamping, ts, hitWall, world, bu);
    if (!floated) {
        p.boatInWater = false;
    } else {
        bool onWater = ts * b.mass * 0.0008f < bu.force.z;
        const bool capsized = skimmer && b.up.z < -0.5f && std::fabs(b.mv.x) < 0.2f && std::fabs(b.mv.y) < 0.2f;
        b.ApplyMoveForce(capsized ? bu.force * 0.03f : bu.force);
        if (hitWall) b.ApplyTurnForce(bu.force * 0.4f, bu.turnPoint);
        if (skimmer && (p.skimDiveMs > 0.0f || (b.fwd.z < -0.5f && b.up.z > -0.5f && b.mv.z < -0.15f &&
                                                 ts * b.mass * 8.0e-5f < bu.force.z && bu.force.z < ts * b.mass * 0.4f * 0.008f))) {
            b.ApplyTurnForce(b.fwd * (b.fwd.z * b.turnMass * ts * -0.00017f * bu.force.z), b.up);   // the nose comes up
            onWater = false;
            b.ApplyMoveForce(b.fwd * (Dot(b.fwd, b.mv) * -0.5f * b.mass));                    // and half the way on is lost
            if (p.skimDiveMs == 0.0f) p.skimDiveMs = 300.0f;
            else if ((p.skimDiveMs -= ts * 20.0f) <= 0.0f) p.skimDiveMs = 0.0f;
        }
        if (!postCollision && onWater && b.up.z > 0.0f) {   // planing: the hull rises out of the water with speed
            float lift = Len2(b.mv) * bh.aqPlaneForce * ts * bu.force.z * 0.5f;
            lift = skimmer ? lift * (p.gas + 1.0f) : p.gas <= 0.05f ? 0.0f : lift * p.gas;
            lift = std::min(lift, ts * bh.aqPlaneLimit * b.mass / 125.0f);
            const V3 f = b.up * lift;
            b.ApplyMoveForce(f);
            b.ApplyTurnForce(f, bu.turnPoint - b.fwd * bh.aqPlaneOffset);
        }
        if (b.up.z > -0.6f) {
            const float moving = std::fabs(p.gas) <= 0.05f ? std::sqrt(b.mv.x * b.mv.x + b.mv.y * b.mv.y) : 1.0f;
            if (std::fabs(p.gas) > 0.05f || moving > 0.01f) {
                float traction = 1.0f;
                if (player) {
                    float loss = Dot(b.mv, b.fwd) * h.tractionBias;
                    if (p.handbrake) loss *= 0.5f;
                    traction = std::clamp(1.0f - loss, 0.0f, 1.0f);
                }
                const float sa = -(traction * p.steerAngle), sn = std::sin(sa), cs = std::cos(sa);
                const V3 thrust = b.Rot(V3(0.0f, vm.colMin[1] * bh.thrustY, vm.colMin[2] * bh.thrustZ));   // the propeller
                const V3 at = b.pos + thrust;
                float level = bu.level;
                world.water(at.x, at.y, &level);
                p.engineInWater = at.z - 0.5f < level;
                if (p.engineInWater) {
                    const float depth = std::min((level - at.z + 0.5f) * (level - at.z + 0.5f), 1.0f);
                    const float g = std::fabs(p.gas);
                    bool easing = g < 0.01f || skimmer;
                    if (!easing) {
                        if (g < 0.5f) easing = true;
                        V3 push = b.Rot(V3(-sn, cs, -std::fabs(p.steerAngle))) * (depth * p.gas * 40.0f * h.trans.engineAccel * b.mass);
                        if (push.z > 0.2f) push.z = (1.2f - push.z) * (1.2f - push.z) + 0.2f;
                        if (postCollision) {
                            if (p.gas < 0.0f) { push.x *= 5.0f; push.y *= 5.0f; }
                            push.z = std::max(0.0f, push.z);
                            b.ApplyMoveForce(push * ts);
                        } else {
                            b.ApplyMoveForce(push * ts);
                            b.ApplyTurnForce(push * ts, thrust - b.up * bh.thrustAppZ);
                            const float side = -Dot(push, b.right) * h.tractionMult;
                            b.ApplyTurnForce(b.right * (side * ts), b.up);
                        }
                    }
                    if (!postCollision && easing) {   // the rudder bites on the water flowing past
                        float loss = std::min(Dot(b.mv, b.fwd) * h.tractionLoss, b.turnMass * 0.01f);
                        if (g > 0.01f) loss *= (0.55f - g) * (player ? 2.6f : 5.0f);
                        if ((p.gas < 0.0f && loss > 0.0f) || (p.gas > 0.0f && loss < 0.0f)) loss = -loss;
                        const V3 v = b.Rot(V3(-sn * loss, 0.0f, 0.0f)) * (depth * ts);
                        b.ApplyMoveForce(v);
                        b.ApplyTurnForce(v, thrust);
                        b.ApplyTurnForce(b.right * (std::max(ts, 0.01f) / traction * loss * sn * -0.75f), b.up);
                    }
                }
            }
        }
        if (h.suspBias != 0.0f) {   // the hull keeps sliding sideways a little
            V3 r = Cross(b.fwd, V3(0.0f, 0.0f, 1.0f));
            const float k = Dot(r, b.mv) * h.suspBias * ts * bu.immersion * b.mass * -0.1f;
            const float x = r.x * 0.3f;
            r.x -= r.y * 0.3f;
            r.y += x;
            b.ApplyMoveForce(r * k);
        }
        if (player && p.handbrake) {
            const float d = Dot(b.mv, b.fwd);
            if (d > 0.0f) b.ApplyMoveForce(b.fwd * (d * h.suspLower * ts * bu.immersion * b.mass * -0.1f));
        }
        if (onWater && !postCollision && !hitWall) {   // ApplyBoatWaterResistance
            float k = bu.immersion * bu.immersion * h.suspForce * b.mass / 1000.0f;
            const float along = Dot(b.mv, b.fwd);
            k = 1.0f / std::fabs(k * (along * along + 0.05f) + 1.0f);
            const float half = ts * 0.5f;
            const V3 res(std::pow(bh.moveRes[0] * k, half), std::pow(bh.moveRes[1] * k, half), std::pow(bh.moveRes[2] * k, half));
            const V3 local(Dot(b.mv, b.right) * res.x, Dot(b.mv, b.fwd) * res.y, Dot(b.mv, b.up) * res.z);
            const float pitch = (res.y - 1.0f) * local.y * b.mass;
            b.mv = b.Rot(local);
            b.ApplyTurnForce(b.fwd * pitch, -b.up);
            if (b.mv.z <= 0.0f) b.mv.z *= (1.0f - res.z) * 0.5f + res.z;
            else b.mv.z *= res.z;
        }
        if (!hitWall && (!skimmer || p.skimDiveMs == 0.0f)) {   // the turn resistance about each axis
            const V3 res(std::pow(bh.turnRes[0], ts), std::pow(bh.turnRes[1], ts), std::pow(bh.turnRes[2], ts));
            V3 lt(Dot(b.tv, b.right), Dot(b.tv, b.fwd), Dot(b.tv, b.up));
            lt.y *= res.y;
            lt.z *= res.z;
            const float pitch = (res.x / (lt.x * lt.x * 1000.0f + 1.0f) * lt.x - lt.x) * b.turnMass;
            b.tv = b.Rot(lt);
            b.ApplyTurnForce(b.up * pitch, b.fwd + b.ComW());
        }
        if (!postCollision && onWater && b.up.z > 0.0f) {   // slamming into a wave
            const float slam = std::floor((bu.immersion - p.prevVolume) * 10000.0f);
            if (slam > 200.0f) {
                V3 f(0.0f, 0.0f, std::max(0.0f, std::min(Len2(b.mv) * slam / 1000.0f, h.brakeDecelGame - b.mv.z)));
                f += b.fwd * (slam * h.brakeBias * -0.01f * Dot(b.mv, b.fwd));
                f *= b.mass;
                b.ApplyMoveForce(f);
                b.ApplyTurnForce(f, bu.turnPoint);
            }
        }
        p.prevVolume = bu.immersion;
        p.boatInWater = onWater;
        p.boatForce = Len(bu.force);
    }
    Store(b, c);
}
