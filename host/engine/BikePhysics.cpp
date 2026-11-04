// CBike physics: two wheels on four suspension lines, steering through the handlebars, the rider's lean.
//
// Ported from CBike::ProcessControl / ProcessControlInputs / ProcessAI (the player's lean forces) /
// SetupSuspensionLines / CalculateLeanMatrix / PreRender (the moving frames) and CVehicle::ProcessBikeWheel.
// The body stays upright in the simulation (a torque keeps its right axis level with the ground); the lean is
// drawn through the lean matrix only. A bike nobody rides drops its centre of mass and falls over unless it
// stands on its side stand.
#define NOMINMAX
#include "Vehicle.h"
#include "PhysicsCore.h"
#include "Stats.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

using namespace physics;

constexpr float kRoadAdhesion = 4.5f;                // surface.dat Rubber (wheelbase) on Road
constexpr float kDampingLimitInFrame = 0.25f;        // CPhysical::DAMPING_LIMIT_IN_FRAME
constexpr float kDampingLimitOfSpringForce = 0.999f; // CPhysical::DAMPING_LIMIT_OF_SPRING_FORCE
constexpr float kWheelFriction = 0.9f;               // cHandlingDataMgr::fWheelFriction
constexpr float kBurstBikeTyreMod = 0.05f;           // fBurstBikeTyreMod (0x8D34C8)
constexpr float kBurstBikeSpeedMax = 0.12f;          // fBurstBikeSpeedMax (0x8D34CC)
constexpr float kBikeWheelTurnForce = 2.0f;          // fTweakBikeWheelTurnForce (0x8D34D0)
enum { WHEEL_NORMAL = 0, WHEEL_SPINNING = 1, WHEEL_SKIDDING = 2, WHEEL_FIXED = 3 };

// ProcessBikeWheel's file static (0xC1CDAF): set once any bike wheel has slipped, never cleared.
bool g_bikeSkidding = false;

// CVehicle::ProcessBikeWheel. special: 0 front, 1 rear.
void ProcessBikeWheel(Body& b, const HandlingData& h, const V3& wf, const V3& wr, const V3& speed, const V3& point, int wheelsOnGround,
                      float thrust, float brake, float adhesion, float destab, int wheelId, int* state, bool burst, float gas, bool bmx,
                      float ts) {
    const float speedFwd = Dot(wf, speed);
    const bool braking = brake != 0.0f;
    const bool driving = !braking && thrust != 0.0f;
    const bool reversing = !braking && thrust < 0.0f;
    if (*state != WHEEL_NORMAL) g_bikeSkidding = true;
    const bool skidding = g_bikeSkidding;
    *state = WHEEL_NORMAL;
    adhesion *= ts;
    if (skidding) adhesion *= h.tractionLoss;
    float right = 0.0f, fwd = 0.0f;
    const float speedRight = Dot(wr, speed);
    if (speedRight != 0.0f) {
        right = -(speedRight / (float)wheelsOnGround);
        if (burst) right += std::min(speedFwd, kBurstBikeSpeedMax) * RandRange(-kBurstBikeTyreMod, kBurstBikeTyreMod);
    }
    if (!driving) {
        if (speedFwd != 0.0f) {
            const float f = -(speedFwd / (float)wheelsOnGround);
            float limit = brake;
            if (!braking && std::fabs(gas) < 0.01f) {   // rolling resistance
                if (bmx) { if (f > -0.05f && f < 0.05f) limit = kWheelFriction * 0.5f / (h.mass + 200.0f); }
                else limit = kWheelFriction * 0.6f / (h.mass + 200.0f);
            }
            if (limit > adhesion) {
                fwd = f;
                if (std::fabs(speedFwd) > 0.005f) *state = WHEEL_FIXED;
            } else fwd = std::clamp(f, -limit, limit);
        }
    } else {
        fwd = thrust;
        right = std::clamp(right, -adhesion, adhesion);
    }
    const float sq = fwd * fwd + right * right;
    if (sq <= adhesion * adhesion) {
        if (destab < 1.0f) {
            if (!skidding) destab *= h.tractionLoss;
            if (adhesion * adhesion * destab * destab < sq) right *= adhesion * destab / std::sqrt(sq);
        }
    } else {
        if (*state != WHEEL_FIXED) *state = (!driving || speedFwd >= 0.1f) ? WHEEL_SKIDDING : WHEEL_SPINNING;
        const float k = (skidding ? 1.0f : h.tractionLoss) / std::sqrt(sq) * adhesion;
        fwd *= k;
        right *= k;
        if (destab < 1.0f) right *= destab;
    }
    if (fwd == 0.0f && right == 0.0f) return;
    const V3 total = wr * right + wf * fwd;
    const V3 dir = Norm(total);
    const float spd = Len(total);
    b.ApplyMoveForce(dir * (b.mass * spd));
    const V3 tf = dir * (spd / (1.0f / b.mass + Len2(Cross(point, dir)) / b.turnMass));
    const float across = Dot(tf, b.right);
    if (wheelId != 1 || (!braking && !reversing))
        b.ApplyTurnForce((tf - b.right * across) * kBikeWheelTurnForce, point - b.right * Dot(point, b.right));
    b.ApplyTurnForce(b.right * across, b.fwd * Dot(point, b.fwd));
}

}  // namespace

bool Vehicles::UsesBikePhysics(int model) const {
    const VehicleInfo* info = Info(model);
    if (!info) return false;
    std::string t = info->type;
    for (char& ch : t) ch = (char)std::tolower((unsigned char)ch);
    if (t != "bike" && t != "bmx") return false;
    auto it = m_models.find(model);
    const HandlingData* h = Handling(info->handling);
    return it != m_models.end() && it->second.isBike && h && h->bike.found;
}

// CBike::SetupSuspensionLines: lines 0 and 1 a quarter of the tyre ahead of and behind the front wheel, 2 and 3
// likewise at the rear; each runs from the upper limit down to the lower limit plus half the tyre.
void Vehicles::SetupBikeSuspension(VehicleModel& vm, const HandlingData& h) {
    SetupSuspension(vm, h);   // the body probes
    for (int i = 0; i < 4; i++) {
        const bool front = i < 2;
        const float size = front ? vm.info.wheelScaleFront : vm.info.wheelScaleRear;
        const float* wp = vm.wheelPos[front ? 0 : 2];
        vm.wheelSize[i] = size;
        const float off = (i % 2 == 0 ? 0.25f : -0.25f) * size;
        vm.suspStart[i][0] = vm.suspEnd[i][0] = wp[0];
        vm.suspStart[i][1] = vm.suspEnd[i][1] = wp[1] + off;
        vm.suspStart[i][2] = wp[2] + h.suspUpper;
        vm.suspEnd[i][2] = vm.suspStart[i][2] + (h.suspLower - h.suspUpper) - size * 0.5f;
        vm.suspSpring[i] = h.suspUpper - h.suspLower;
        vm.suspLine[i] = vm.suspStart[i][2] - vm.suspEnd[i][2];
    }
}

// One CBike::ProcessControl at time step ts, then CPhysical::ProcessCollision.
void Vehicles::BikeStep(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world) {
    CarPhys& p = c.phys;
    const HandlingData::Bike& bh = h.bike;
    const Transmission& tr = h.trans;
    const bool bmx = vm.info.type == "bmx" || vm.info.type == "BMX";
    Body b = Load(c, h);
    p.player = player;
    p.hitWall = false;
    p.brakesOn = false;
    const bool ridden = player;   // ProcessAI's extra handling flag 2: someone is on it
    bool handbrake;
    V3 com(h.com);
    if (player) {
        // ---- CBike::ProcessControlInputs ----
        handbrake = p.handbrake;
        p.rawSteer = std::clamp(p.rawSteer + (-p.inSteer - p.rawSteer) * ts * 0.2f, -1.0f, 1.0f);
        p.leanFwd = std::clamp(p.leanFwd + (p.inLeanFwd - p.leanFwd) * ts * 0.2f, -1.0f, 1.0f);
        const float fwdVel = Dot(b.mv, b.fwd);
        const float g = p.inAccel - p.inBrake;
        float gas, brake = 0.0f;
        if (std::fabs(fwdVel) >= 0.01f) {
            if (fwdVel < 0.0f) { if (g >= 0.0f) { brake = g; gas = 0.0f; } else gas = g; }
            else if (g < 0.0f) { gas = 0.0f; brake = -g; }
            else gas = g;
        } else if (p.inAccel > 150.0f / 255.0f && p.inBrake > 150.0f / 255.0f && !bmx) {   // a burnout
            gas = p.inAccel; brake = p.inBrake; p.brakesOn = true;
        } else gas = g;
        if (bmx) {   // CBmx::ProcessAI: sprint energy keeps the pedals going; a crouched hop cuts them
            if (p.bmxPedalOn && gas == 0.0f && brake == 0.0f) gas = 1.0f;
            if (p.bmxHopBlend > 0.5f) gas = 0.0f;
        }
        p.gas = gas; p.brake = brake;
        p.steerAngle = h.steeringLock * 0.0174532925f * std::copysign(p.rawSteer * p.rawSteer, p.rawSteer);
        // ---- CBike::ProcessAI, STATUS_PLAYER: the rider's weight forward or back ----
        const float speed = Len(b.mv);
        const float sp = std::min(0.1f, speed);
        if (p.leanFwd >= 0.0f) {
            com.y = bh.leanFwdCOM * p.leanFwd + h.com[1];
            if (p.brake < 0.0f || p.numContactWheels == 0) {
                const float t = Stats::FatMuscleMod(11) * (bh.leanFwdForce * b.turnMass * p.leanFwd * sp * (std::max(sp / 0.1f, p.brake) + p.brake) * 0.5f);
                b.com = com;
                b.ApplyTurnForce(b.up * -(ts * t), com + b.fwd);
            }
        } else {
            com.y = bh.leanBakCOM * p.leanFwd + h.com[1];
            if ((p.brake == 0.0f && !handbrake) || p.numContactWheels == 0) {
                const float t = Stats::FatMuscleMod(11) * (bh.leanBakForce * b.turnMass * p.leanFwd * sp * (std::max(sp / 0.1f, p.gas) + p.gas) * 0.5f);
                b.com = com;
                b.ApplyTurnForce(b.up * -(ts * t), com + b.fwd);
            }
        }
        if (bmx) {
            const bool airborne = p.comp[0] >= 1.0f && p.comp[1] >= 1.0f && p.comp[2] >= 1.0f && p.comp[3] >= 1.0f;
            const float spinUp = Dot(b.tv, b.up);
            if (airborne && ((spinUp < 0.04f && p.inSteer < 0.0f) || (spinUp > -0.04f && p.inSteer > 0.0f)))   // turning in the air
                b.ApplyTurnForce(b.up * (p.inSteer * b.turnMass * ts * 0.002f), b.fwd);
            if (p.bmxHopLaunch > 0.0f) {   // CBmx::LaunchBunnyHopCB, both wheels on the ground
                const bool front = p.wheelCounts[0] > 0.0f || p.wheelCounts[1] > 0.0f, rear = p.wheelCounts[2] > 0.0f || p.wheelCounts[3] > 0.0f;
                if (front && rear) {
                    const float power = (std::min(p.bmxHopLaunch / 25.0f, 1.0f) + 1.0f) * Stats::FatMuscleMod(6) * (p.cheatMegaJump ? 5.0f : 1.0f);   // CHEAT_MEGA_JUMP: the hop is five times as strong
                    b.mv += b.up * (0.06f * power);
                    b.ApplyTurnForce(b.up * (b.turnMass * 0.01f * power), b.fwd);
                }
                p.bmxHopLaunch = 0.0f;
            }
        }
    } else {   // STATUS_ABANDONED: the brake on while it stands
        p.brake = 0.0f;
        handbrake = Len2(b.mv) < 0.01f || p.onSideStand;
        p.gas = 0.0f;
        p.leanFwd = 0.0f;
    }
    if (p.onSideStand && (std::fabs(b.right.z) > 0.35f || std::fabs(b.fwd.z) > 0.5f)) p.onSideStand = false;
    if (!ridden && !p.onSideStand) com = V3(h.com[0], h.com[1], bh.noPlayerCOMz);   // nobody holds it up
    b.com = com;
    if (ridden || p.onSideStand) {
        // Damp the pitch and roll rates (harder in a wheelie or stoppie, by the rider's skill)
        const V3 lt(Dot(b.tv, b.right), Dot(b.tv, b.fwd), Dot(b.tv, b.up));
        float dx = 0.9995f, dy = 0.9f, pf = 1.0f;
        if (player) {
            const float m13 = Stats::FatMuscleMod(13);
            if (p.comp[0] < 1.0f || p.comp[1] < 1.0f) {
                if (p.wheelCounts[2] <= 0.0f && p.wheelCounts[3] <= 0.0f) {
                    pf = 100.0f;
                    if (b.fwd.z < 0.0f) dx *= std::min(std::fabs(bh.stoppieAng - b.fwd.z) * m13 * 0.25f, m13 * 0.075f) + 0.9f;
                }
            } else {
                pf = 100.0f;
                if ((p.comp[2] >= 1.0f && p.comp[3] >= 1.0f) || b.fwd.z <= 0.0f) dx = 0.98f;
                else dx -= std::min(std::fabs(bh.wheelieAng - b.fwd.z) * m13 * 0.2f, 0.05f);
            }
        }
        dx = std::pow(dx / (lt.x * lt.x * pf + 1.0f), ts);
        dy = std::pow(dy / (lt.y * 1000.0f * lt.y + 1.0f), ts);
        const float dPitch = lt.x * dx - lt.x, dRoll = lt.y * dy - lt.y;
        b.ApplyTurnForce(b.up * (-dRoll * b.turnMass), b.ComW() + b.right);
        b.ApplyTurnForce(b.up * (dPitch * b.turnMass), b.ComW() + b.fwd);
        if (!player) b.com = V3(h.com);
    }
    // Knocked sideways at a crawl: the tyres stop gripping across
    const bool knocked = p.lastDI > 0.0f && std::fabs(Dot(V3(p.impactNormal), b.right)) > 0.5f && Len2(b.mv) < 0.1f;
    if (!ridden && !p.onSideStand) {   // the bars flop over the way it leans
        if (b.right.z >= 0.0f) { if (p.steerAngle < 0.43633232f) p.steerAngle += ts * 0.008726646f; }
        else if (p.steerAngle > -0.43633232f) p.steerAngle -= ts * 0.008726646f;
    }

    // CPhysical::ProcessControl: ApplyFriction, ApplyGravity, ApplyAirResistance
    b.mv += b.fmv; b.tv += b.ftv;
    b.fmv = V3(); b.ftv = V3();
    b.mv.z -= ts * 0.008f;
    {
        const float air = h.dragMult <= 0.01f ? h.dragMult : h.dragMult / 1000.0f / 2.0f;
        b.mv *= std::pow(std::max(0.0f, 1.0f - Len(b.mv) * air), ts);
        b.tv *= 0.99f;
    }

    // ---- Suspension ----
    for (int i = 0; i < 4; i++) {
        const float wr = 1.0f - vm.suspSpring[i] / vm.suspLine[i];
        p.comp[i] = (p.comp[i] - wr) / (1.0f - wr);
    }
    for (int k = 0; k < 2; k++) {   // DoBurstAndSoftGroundRatios: a flat tyre sags now and then
        if (c.wheelStatus[k] != 1) continue;
        const float fwdRatio = std::fabs(Dot(b.mv, b.fwd));
        for (int i = k * 2; i < k * 2 + 2; i++)
            if (RandInt((int)(fwdRatio * 40.0f) + 98 + 1) < 100)
                p.comp[i] = std::min(1.0f, p.comp[i] + (vm.suspLine[i] - vm.suspSpring[i]) / vm.suspLine[i] / 4.0f);
    }
    {
        float limit[4] = {0, 0, 0, 0};
        V3 cp[4], dir[4];
        for (int i = 0; i < 4; i++) {
            dir[i] = -b.up;
            cp[i] = p.comp[i] < 1.0f ? V3(p.colPt[i]) - b.pos : b.Rot(V3(vm.suspEnd[i]));
        }
        for (int i = 0; i < 4; i++) {   // ApplySpringCollision(Alt)
            if (p.comp[i] >= 1.0f) continue;
            const float bias = i >= 2 ? 1.0f - h.suspBias : h.suspBias;
            V3 n(p.colN[i]);
            if (Dot(dir[i], n) > 0.0f) n = -n;
            if (n.z <= 0.35f) n = b.up;   // ApplySpringCollision pushes along the body
            limit[i] = (1.0f - p.comp[i]) * (std::min(ts, 3.0f) * b.mass) * h.suspForce * bias * 0.016f;
            b.ApplyForce(n * limit[i], cp[i]);
            if (p.colN[i][2] > 0.35f) dir[i] = -V3(p.colN[i]);
        }
        V3 cs[4];
        for (int i = 0; i < 4; i++) cs[i] = b.Speed(cp[i]);
        for (int i = 0; i < 4; i++) {   // ApplySpringDampening
            if (p.comp[i] >= 1.0f) continue;
            const float speedAlong = Dot(cs[i], dir[i]);
            const float pointAlong = Dot(b.Speed(cp[i]), dir[i]);
            const float dts = std::clamp(std::min(ts, 3.0f) * h.suspDamping, -kDampingLimitInFrame, kDampingLimitInFrame);
            float ds = -(dts * speedAlong);
            if (ds > 0.0f && ds + pointAlong > 0.0f) ds = pointAlong >= 0.0f ? 0.0f : -pointAlong;
            else if (ds < 0.0f && ds + pointAlong < 0.0f) ds = pointAlong <= 0.0f ? 0.0f : -pointAlong;
            float force = b.MassAt(cp[i] - b.ComW(), dir[i]) * ds;
            force = std::min(force, std::fabs(limit[i]) * kDampingLimitOfSpringForce);
            b.ApplyForce(dir[i] * force, cp[i]);
        }
    }

    // ---- Drive, brakes, contacts ----
    const float speedForward = Dot(b.mv, b.fwd);
    const float accel = player ? tr.DriveAcceleration(p.gas, p.gear, speedForward, nullptr, nullptr, p.driveWheelsOnGround != 0, ts) : 0.0f;
    const float brake = h.brakeDecelGame * p.brake * ts;
    const float frontBrakeBias = 2.0f * h.brakeBias, rearBrakeBias = 2.0f * (1.0f - h.brakeBias);
    const float frontTractionBias = 2.0f * h.tractionBias, rearTractionBias = 2.0f - 2.0f * h.tractionBias;
    p.numContactWheels = 0;
    p.driveWheelsOnGround = 0;
    V3 aveN;
    for (int i = 0; i < 4; i++) {
        if (p.comp[i] >= 1.0f) {
            p.wheelCounts[i] = std::max(p.wheelCounts[i] - ts, 0.0f);
            if (p.wheelCounts[i] <= 0.0f) continue;
        } else p.wheelCounts[i] = 4.0f;
        p.numContactWheels++;
        if (i >= 2) p.driveWheelsOnGround = 1;
        aveN += V3(p.colN[i]);
    }
    if (p.numContactWheels == 0) aveN = V3(0, 0, 1);
    else {
        aveN = Norm(aveN / (float)p.numContactWheels);
        if (Dot(b.up, aveN) < -0.5f) aveN = -aveN;
    }
    if (knocked) {   // flat ground under it, square to its nose
        const V3 r = Norm(Cross(V3(0, 0, 1), b.fwd));
        aveN = Norm(Cross(r, b.fwd));
    }
    // the tyre contacts: under the more squashed line of each pair
    const int fb = p.comp[1] <= p.comp[0] ? 1 : 0, rb = p.comp[3] <= p.comp[2] ? 3 : 2;
    const V3 frontPt = b.Rot(V3(0.0f, vm.suspStart[0][1], vm.suspStart[0][2] - std::min(p.comp[fb], 1.0f) * vm.suspSpring[0] - vm.wheelSize[0] * 0.5f));
    const V3 rearPt = b.Rot(V3(0.0f, vm.suspStart[3][1], vm.suspStart[2][2] - std::min(p.comp[rb], 1.0f) * vm.suspSpring[2] - vm.wheelSize[2] * 0.5f));
    const float traction = h.tractionMult * 0.004f * 0.25f;

    // the handlebars: the player's steering cut back to what the front tyre can hold at speed
    if (player || !p.onSideStand) {
        if (std::fabs(b.mv.x) < 0.01f && std::fabs(b.mv.y) < 0.01f && p.steerAngle == 0.0f) p.barSteer *= std::pow(0.96f, ts);
        else {
            float k = 1.0f;
            if (speedForward > 0.01f && (p.wheelCounts[0] > 0.0f || p.wheelCounts[1] > 0.0f) && player) {
                const float lim = std::min(kRoadAdhesion * bh.speedSteer * traction * 4.0f / (speedForward * speedForward), 1.0f);
                k = std::asin(lim) / (h.steeringLock * 0.0174532925f);
                if ((p.steerAngle < 0.0f && p.leanAngle < 0.0f) || (p.steerAngle > 0.0f && p.leanAngle > 0.0f)) k += k;
                if (k > 1.0f) k = 1.0f;
            }
            p.barSteer = k * p.steerAngle;
        }
    } else if (p.barSteer < 0.34906587f) p.barSteer += ts * 0.02617994f;

    const V3 mvBeforeWheels = b.mv;
    int states[2] = {p.wheelState[0], p.wheelState[1]};
    auto frontWheel = [&]() {
        if (p.wheelCounts[0] <= 0.0f && p.wheelCounts[1] <= 0.0f) { p.wheelAngVel[0] *= 0.95f; return; }
        const V3 n(p.colN[fb]);
        const V3 d = b.Rot(V3(-std::sin(p.barSteer), std::cos(p.barSteer), 0.0f));
        const V3 wf = Norm(d - n * Dot(d, n));
        V3 wr = Norm(Cross(wf, n));
        if (knocked) wr.z = 0.0f;
        p.lastThrust = 0.0f;
        float adhesion = kRoadAdhesion * traction;
        if (c.wheelStatus[0] == 1) adhesion *= 0.4f;
        ProcessBikeWheel(b, h, wf, wr, b.Speed(frontPt), frontPt, 2, 0.0f, frontBrakeBias * brake, adhesion * frontTractionBias, 1.0f, 0,
                         &states[0], c.wheelStatus[0] == 1, p.gas, bmx, ts);
    };
    if (!(h.handlingFlags & 0x800000u)) frontWheel();
    if (p.wheelCounts[2] > 0.0f || p.wheelCounts[3] > 0.0f) {
        const V3 n(p.colN[rb]);
        const V3 wf = Norm(b.fwd - n * Dot(b.fwd, n));
        V3 wr = Norm(Cross(wf, n));
        if (knocked) wr.z = 0.0f;
        float rearBrake = brake, rearTraction = traction;
        if (!handbrake) {
            float spin = 0.0f;
            bool apply = true;
            if (p.brakesOn) { spin = p.steerAngle * b.turnMass * -0.0007f; rearBrake = 0.0f; rearTraction = 0.0f; }
            else if (p.tireTemp < 1.0f && p.gas > 0.75f) { rearTraction *= p.tireTemp; spin = (1.0f - p.tireTemp) * p.steerAngle * b.turnMass * -0.0007f; }
            else apply = false;
            // the burnout swings the tail round (retail passes the contact point as the force, the push as the point)
            if (apply) b.ApplyTurnForce(V3(p.colPt[2]) - b.pos, b.right * (spin * ts));
        } else { rearBrake = 20000.0f; p.tireTemp = 1.0f; }
        if (p.lastThrust > 0.0f && rearBrake > 0.0f) rearBrake = 0.0f;
        p.lastThrust = accel;
        float adhesion = kRoadAdhesion * rearTraction;
        if (c.wheelStatus[1] == 1) adhesion *= 0.4f;
        ProcessBikeWheel(b, h, wf, wr, b.Speed(rearPt), rearPt, 2, accel, rearBrakeBias * rearBrake, adhesion * rearTractionBias, 1.0f, 1,
                         &states[1], c.wheelStatus[1] == 1, p.gas, bmx, ts);
    } else if (!handbrake) {   // spinning in the air
        if (accel > 0.0f) { if (p.wheelAngVel[1] > -1.0f) p.wheelAngVel[1] -= 0.1f; }
        else if (accel < 0.0f && p.wheelAngVel[1] < 1.0f) p.wheelAngVel[1] += 0.05f;
    } else p.wheelAngVel[1] = 0.0f;
    if (!p.brakesOn || states[1] != WHEEL_SPINNING) { if (p.tireTemp < 1.0f) p.tireTemp += ts * 0.005f; }
    else p.tireTemp = std::max(0.0f, p.tireTemp - ts * 0.002f);
    if (h.handlingFlags & 0x800000u) frontWheel();

    // ---- The lean (drawn only) ----
    if (!ridden) {
        if (p.onSideStand) {
            const float k = std::pow(0.97f, ts);
            p.desiredLean = k * p.desiredLean - (1.0f - k) * (std::asin(std::clamp(b.right.z, -1.0f, 1.0f)) + 0.2617994f);
        } else p.desiredLean *= std::pow(0.95f, ts);
    } else {
        // into the turn by the sideways acceleration the tyres gave this step, up to the model's limit
        const V3 groundRight = Norm(Cross(b.fwd, aveN));
        float x = Dot(b.mv - mvBeforeWheels, groundRight) / (std::max(ts, 0.01f) * 0.008f);
        const float lim = c.wheelStatus[0] == 1 ? 0.4f * bh.maxLean : bh.maxLean;
        x = std::clamp(x, -lim, lim);
        const float k = std::pow(bh.desLean, ts);
        p.desiredLean = k * p.desiredLean + (1.0f - k) * (std::asin(x) - p.riderStill * 0.17453294f);
    }
    p.leanAngle = p.desiredLean;
    p.wheelState[0] = states[0];
    p.wheelState[1] = (p.gas < 0.0f && states[1] == WHEEL_SPINNING) ? WHEEL_NORMAL : states[1];
    for (int k = 0; k < 2; k++) {   // the wheels roll with the ground
        if (p.wheelCounts[k * 2] > 0.0f || p.wheelCounts[k * 2 + 1] > 0.0f)
            p.wheelAngVel[k] = -Dot(b.mv, b.fwd) / std::max(0.1f, vm.wheelSize[k * 2] * 0.5f);
        p.wheelPitch[k] += p.wheelAngVel[k] * ts;
    }

    // at rest with no throttle it stops dead
    if (p.gas == 0.0f && std::fabs(b.mv.x) < 0.005f && std::fabs(b.mv.y) < 0.005f && std::fabs(b.mv.z) < 0.005f) {
        b.mv = V3();
        b.tv.z = 0.0f;
    }
    // ridden or on its stand: a torque keeps it square to the ground
    if (ridden || p.onSideStand) {
        const float side = std::clamp(Dot(b.right, aveN), -1.0f, 1.0f);
        b.ApplyTurnForce(b.up * (side * b.turnMass * (ridden ? -0.07f : -0.1f) * ts), b.ComW() + b.right);
        if (ridden) p.onSideStand = false;
    }
    if (player) {
        const float speed = std::min(Len(b.mv), 0.1f);
        const bool frontUp = p.wheelCounts[0] <= 0.0f && p.wheelCounts[1] <= 0.0f;
        const bool rearDown = p.wheelCounts[2] > 0.0f || p.wheelCounts[3] > 0.0f;
        if (frontUp && b.fwd.z > 0.0f && rearDown) {   // a wheelie: held near the model's angle, steered by the rider
            float d = bh.wheelieAng - b.fwd.z;
            if (d > 0.15f) { d = 0.3f - d; if (d < 0.0f) d = 0.0f; }
            else if (d < -0.08f) { d = -0.14f - d; if (d > 0.0f) d = 0.0f; }
            const float t = Stats::FatMuscleMod(12) * (d * bh.wheelieStabMult * speed) * b.turnMass * ts * 0.5f;
            b.ApplyTurnForce(b.up * t, b.ComW() + b.fwd);
            b.ApplyTurnForce(b.right * (bh.wheelieSteer * p.barSteer * b.turnMass * ts * 0.5f), b.ComW() + b.fwd);
            const float sp = Len(b.mv);
            b.ApplyMoveForce(b.right * (sp * b.mass * bh.wheelieSteer * p.barSteer * ts * 0.01f * sp));
            p.leanAngle += ts * p.barSteer * -0.1f;
        } else if (!frontUp && p.wheelCounts[2] <= 0.0f && p.wheelCounts[3] <= 0.0f && b.fwd.z < 0.0f) {   // a stoppie
            float d = bh.stoppieAng - b.fwd.z;
            if (d > 0.15f) { d = 0.3f - d; if (d < 0.0f) d = 0.0f; }
            else if (d < -0.15f) { d = -0.3f - d; if (d > 0.0f) d = 0.0f; }
            const float t = Stats::FatMuscleMod(12) * (d * bh.stoppieStabMult * speed) * b.turnMass * ts * 0.5f;
            b.ApplyTurnForce(b.up * t, b.ComW() + b.fwd);
            const float slide = Dot(b.right, b.mv) * b.turnMass * ts * 0.05f;
            b.ApplyTurnForce(b.right * -slide, -Norm(Cross(V3(0, 0, 1), b.right)));
        }
    }
    for (int i = 0; i < 4; i++) { p.compPrev[i] = p.comp[i]; p.comp[i] = 1.0f; }
    Store(b, c);
    CarCollision(c, vm, h, ts, world);
}

bool Vehicles::BikeRiderAnims(size_t i, float dt, float stillBlend, BikeRide& out) {
    if (i >= m_cars.size() || !UsesBikePhysics(m_cars[i].model)) return false;
    VehicleInstance& c = m_cars[i];
    CarPhys& p = c.phys;
    const VehicleInfo* info = Info(c.model);
    const HandlingData* hp = info ? Handling(info->handling) : nullptr;
    if (!hp) return false;
    const HandlingData& h = *hp;
    const HandlingData::Bike& bh = h.bike;
    const float ts = dt * 50.0f;
    p.riderStill = stillBlend;
    V3 r, f, u;
    if (p.active) { r = V3(&c.rot[0]); f = V3(&c.rot[3]); u = V3(&c.rot[6]); }
    else { const float cs = std::cos(c.heading), sn = std::sin(c.heading); r = V3(cs, sn, 0); f = V3(-sn, cs, 0); u = V3(0, 0, 1); }
    const float v = Dot(V3(p.mv), f);
    const float maxRev = h.trans.maxReverseVelocity;
    // the base pose: a foot down when it stands, pushing it back with the feet when reversing slowly
    float fwdTarget = 0.0f;
    out.base = 0;
    if (std::fabs(v) < 0.02f) out.base = 1;
    else if (v < 0.0f) {
        if (p.gas >= 0.0f || v <= maxRev * 1.5f) { if (v < maxRev * 1.5f) fwdTarget = -1.0f; }
        else out.base = 2;
    }
    float leanTarget = std::clamp(p.leanAngle / bh.fullAnimLean, -1.0f, 1.0f);
    if (fwdTarget == -1.0f) leanTarget = 0.0f;
    const float kl = std::pow(0.86f, ts);
    p.animLeanLeft = kl * p.animLeanLeft + (1.0f - kl) * leanTarget;
    if (fwdTarget > -1.0f) {
        fwdTarget = p.leanFwd;
        const bool frontDown = p.wheelCounts[0] > 0.0f || p.wheelCounts[1] > 0.0f;
        const bool rearDown = p.wheelCounts[2] > 0.0f || p.wheelCounts[3] > 0.0f;
        bool wheelie = false;
        if (!frontDown && f.z > 0.0f && rearDown) wheelie = bh.wheelieAng - f.z < 0.15f;   // up on the back wheel: lean in
        if (wheelie) fwdTarget = std::max(fwdTarget, 0.25f);
        else if (p.brake > 0.5f && v > 0.01f) fwdTarget = std::max(fwdTarget, 0.1f);   // braking: forward
        else if (p.gas > 0.5f && fwdTarget <= 0.0f && v < h.trans.maxFlatVelocity * 0.3f && fwdTarget >= -0.3f) fwdTarget = -0.3f;   // pulling away: back
        if (std::fabs(leanTarget) > 0.3f) fwdTarget *= std::max(0.0f, 1.0f - (std::fabs(leanTarget) - 0.3f) / 0.26f);
    }
    const float kf = std::pow(0.89f, ts);
    p.animLeanFwd = kf * p.animLeanFwd + (1.0f - kf) * fwdTarget;
    // spun too hard for the rider's skill, or reversing too fast on the wheels or upside down after a knock:
    // CEventKnockOffBike(KNOCK_OFF_TYPE_FALL), falling forward
    const float skill = RiderSkill(c.model);
    const float spin = (skill + 1.0f) * 0.3f;
    const bool spun = Len2(V3(p.tv)) > spin * spin;
    const bool onWheels = p.wheelCounts[0] > 0.0f || p.wheelCounts[1] > 0.0f || p.wheelCounts[2] > 0.0f || p.wheelCounts[3] > 0.0f;
    const bool reversed = std::fabs(v) >= 0.02f && v < (skill + 1.0f) * 3.5f * maxRev && (onWheels || (u.z < -0.5f && p.lastDI > 0.0f));
    if ((spun || reversed) && !p.knockOff) {
        p.knockOff = true;
        p.knockOffDir = 2;
        const float rise = RandRange(3.0f, 5.0f) / 70.0f;   // ApplyMoveForce(0, 0, 3..5) on a 70 kg ped
        for (int k = 0; k < 3; k++) p.knockOffVel[k] = (p.mv[k] + (k == 2 ? rise : 0.0f)) * 50.0f;
    }
    out.leanAngle = p.leanAngle;
    out.riderLeanFwd = p.leanFwd;
    out.fwdSpeed = v;
    out.gas = p.gas;
    if (c.model == 510) {   // the mountain bike: by its gear
        out.sprintBelow = 2.0f;
        out.pedalRate = p.gear == 0 ? 0.0f : v * 5.0f / ((float)p.gear * h.trans.maxFlatVelocity - 0.25f);
    } else { out.sprintBelow = 2.5f; out.pedalRate = v * 3.0f; }
    out.leanLeft = p.animLeanLeft;
    out.leanFwd = p.animLeanFwd;
    if (std::fabs(p.animLeanLeft) > 0.56f) { out.fwdWeight = 0.0f; out.leftWeight = 1.0f; }
    else if (std::fabs(p.animLeanFwd) > 0.56f) { out.fwdWeight = 1.0f; out.leftWeight = 0.0f; }
    else {
        const float l = std::sqrt(p.animLeanFwd * p.animLeanFwd + p.animLeanLeft * p.animLeanLeft);
        out.fwdWeight = l > 0.01f ? std::fabs(p.animLeanFwd / l) : std::fabs(p.animLeanFwd);
        out.leftWeight = l > 0.01f ? std::fabs(p.animLeanLeft / l) : std::fabs(p.animLeanLeft);
    }
    (void)r; (void)u;
    return true;
}

// CPed::GetBikeRidingSkill for the player: cycling skill on a bicycle, bike skill on a motorbike (0..1).
float Vehicles::RiderSkill(int model) const {
    const VehicleInfo* info = Info(model);
    const bool bmx = info && (info->type == "bmx" || info->type == "BMX");
    return std::min(1.0f, Stats::Get(bmx ? Stats::STAT_CYCLING_SKILL : Stats::STAT_BIKE_SKILL) * 0.001f);
}

void Vehicles::SetBmxRider(size_t i, const BmxRider& r) {
    if (i >= m_cars.size()) return;
    CarPhys& p = m_cars[i].phys;
    p.bmxPedalOn = r.pedalOn;
    p.bmxHopBlend = r.hopBlend;
    if (r.hopLaunch > 0.0f) p.bmxHopLaunch = r.hopLaunch;
    p.bmxPedalPhase = r.pedalPhase; p.bmxPedalBlend = r.pedalBlend;
    p.bmxSprintPhase = r.sprintPhase; p.bmxSprintBlend = r.sprintBlend;
    p.crankAngle = r.crank;
}

// CBmx::ProcessControl after CBike's: the frame sways with the sprint clip, the lean with the pedal clip.
void Vehicles::BmxSway(VehicleInstance& c) {
    CarPhys& p = c.phys;
    const bool mtb = c.model == 510;
    const float twoPi = 6.2831855f;
    if (p.bmxSprintBlend > 0.01f) p.sprintLean = std::sin(p.bmxSprintPhase * twoPi + 1.5707964f) * p.bmxSprintBlend * (mtb ? 0.087f : 0.3f);
    else {
        if (p.bmxPedalBlend > 0.01f) p.leanAngle += std::sin(p.bmxPedalPhase * twoPi) * p.bmxPedalBlend * (mtb ? 0.02f : 0.07f);
        p.sprintLean *= 0.95f;
    }
}

bool Vehicles::TakeKnockOff(size_t i, float vel[3], int* dir) {
    if (i >= m_cars.size() || !m_cars[i].phys.knockOff) return false;
    CarPhys& p = m_cars[i].phys;
    p.knockOff = false;
    p.onSideStand = false;   // left to fall over
    for (int k = 0; k < 3; k++) vel[k] = p.knockOffVel[k];
    if (dir) *dir = p.knockOffDir;
    return true;
}

// CBike::VehicleDamage for the frame's strongest knock: the rider may come off (DamageKnockOffRider), and past 25
// the bike loses health. A bike has no panels to dent.
void Vehicles::BikeDamage(VehicleInstance& c, const HandlingData& h, float di, bool player) {
    CarPhys& p = c.phys;
    const V3 r(&c.rot[0]), f(&c.rot[3]), u(&c.rot[6]), n(p.impactNormal);
    if (player && !p.knockOff) {   // DamageKnockOffRider, for the player
        const float skill = RiderSkill(c.model);
        float force = di / h.mass * 800.0f * 0.75f * (1.0f - skill * 0.5f);
        if (force > 10.0f) {
            const float fwd = Dot(f, n), up = Dot(u, n), right = Dot(r, n);
            float fwdW = 0.6f;
            if (std::fabs(fwd) > 0.85f) { const float vert = n.z < 0.85f ? 0.0f : n.z; fwdW = 7.0f * vert * vert + 0.6f; }
            if (u.z < 0.0f) fwdW = 5.0f;   // on its side or upside down
            float backW = 1.5f, upW = 0.05f;
            if (c.model == 468) { fwdW *= 0.65f; upW *= 0.75f; }   // the Sanchez
            else if (c.model == 471) { backW = 3.0f; fwdW *= 0.65f; upW *= 0.75f; }   // the Quad
            if (fwd > 0.0f) fwdW *= 1.0f - skill * 0.6f;
            force *= std::fabs(fwd) * fwdW + std::max(up, 0.0f) * upW + std::fabs(right) * 0.45f - std::min(up, 0.0f) * backW;
            if (force > 75.0f) {   // KNOCK_OFF_TYPE_SKIDBACKFRONT, from the side the knock came from
                const float ang = std::atan2(n.x, -n.y) - c.heading;   // GetLocalDirection(-impact)
                const float a = std::fmod(std::fmod(ang, 6.2831853f) + 6.2831853f + 0.78539816f, 6.2831853f);
                p.knockOff = true;
                p.knockOffDir = (int)(a / 1.5707963f) & 3;
                const float push = -0.3f * di / h.mass, rise = RandRange(3.0f, 5.0f) / 70.0f;
                p.knockOffVel[0] = (p.mv[0] + n.x * push) * 50.0f;
                p.knockOffVel[1] = (p.mv[1] + n.y * push) * 50.0f;
                p.knockOffVel[2] = (p.mv[2] + rise) * 50.0f;
            }
        }
    }
    if (p.damager < 0 && Dot(n, u) > 0.6f) return;   // landing on the ground
    const float loss = (di - 25.0f) * h.collisionDamageGame;
    if (di <= 25.0f || loss <= 0.0f) return;
    const float before = c.health;
    c.health -= player ? loss * 0.5f : loss * 0.25f;
    if (c.health < 1.0f && before > 1.0f) c.health = 1.0f;   // only fire or a bullet finishes it
}

// CBike::CalculateLeanMatrix: the drawn body tips about its nose by the lean (and a little back), dropped so the
// tyres stay on the ground.
void Vehicles::BikeLeanBody(const VehicleInstance& c, const VehicleModel& vm, float body[16], float sway) const {
    const float lean = c.phys.leanAngle;
    if (lean == 0.0f && sway == 0.0f) return;
    float rx[16], ry[16], l[16];
    RotX(std::fabs(lean) * -0.05f, rx);
    RotY(lean + sway, ry);
    Mul4(rx, ry, l);
    const float up[3] = {body[8], body[9], body[10]};
    const float drop = vm.colMin[2] * (1.0f - std::cos(lean));
    float out[16];
    Mul4(l, body, out);
    for (int k = 0; k < 3; k++) out[12 + k] = body[12 + k] + up[k] * drop;
    std::memcpy(body, out, sizeof(out));
}

// CBike::PreRender: the front forks and the bars turn with the steering, the front wheel rides up its fork and
// the rear swing arm swings with its spring, and both wheels roll.
void Vehicles::BikeFrameMatrices(const VehicleInstance& c, const VehicleModel& vm, float nodes[VehicleModel::kMoveNodes][16]) const {
    const CarPhys& p = c.phys;
    const HandlingData* h = Handling(vm.info.handling);
    const float lower = h ? h->suspLower : -0.1f, upper = h ? h->suspUpper : 0.1f;
    auto rise = [&](int wheel) {   // how far the wheel is pushed up from where the model has it
        if (!p.active || !vm.suspReady) return 0.0f;
        const float z = std::max(p.wheelZ[wheel * 2], p.wheelZ[wheel * 2 + 1]);
        return std::clamp(z - vm.wheelPos[wheel * 2][2], lower, upper);
    };
    for (int k = 0; k < 8; k++) {
        const VehicleModel::BikeNode& bn = vm.bikeNodes[k];
        float m[16];
        std::memcpy(m, bn.local, sizeof(m));
        if (!bn.found) { std::memset(nodes[k], 0, sizeof(nodes[k])); nodes[k][0] = nodes[k][5] = nodes[k][10] = nodes[k][15] = 1.0f; continue; }
        float r[16];
        if (k == 0 || k == 2) { RotZ(p.barSteer, r); Mul4(r, m, m); }
        else if (k == 1 && vm.swingArmLength > 0.05f) { RotX(-std::asin(std::clamp(rise(1) / vm.swingArmLength, -1.0f, 1.0f)), r); Mul4(r, m, m); }
        else if (k == 3 || k == 4) {
            if (k == 3) m[14] += rise(0);
            RotX(-p.wheelPitch[k - 3], r);
            Mul4(r, m, m);
        }
        else if (k == 5) { RotX(p.crankAngle, r); Mul4(r, m, m); }   // CBmx::PreRender: the chainset turns with the crank
        else if (k == 6 || k == 7) { RotX(-p.crankAngle, r); Mul4(r, m, m); }   // and the pedals stay level
        Mul4(m, bn.parent >= 0 ? nodes[bn.parent] : bn.base, nodes[k]);
    }
}
