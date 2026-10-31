// CAutomobile physics: springs, tyres, gearbox and the body's contacts with the world.
//
// Ported from CAutomobile::ProcessControl / ProcessControlInputs / ProcessSuspension / ProcessCarWheelPair,
// CVehicle::ProcessWheel, cTransmission and CPhysical (ApplyForce, ApplySpringCollisionAlt, ApplySpringDampening,
// ApplyCollisionAlt, ApplyFriction, ApplySpeed). Like retail it works in game units: speeds per 1/50 s and a
// time step counted in 1/50 s. Surfaces are taken as road (the world COL carries no materials yet).
#define NOMINMAX
#include "Vehicle.h"
#include "PhysicsCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

using namespace physics;

// cBuoyancy::ProcessBuoyancy (PreCalcSetup, SimpleCalcBuoyancy with FindWaterLevel and SimpleSumBuoyancyData,
// CalcBuoyancyForce) for a car: a 3x3 grid over the collision box, each point's depth under the surface (up to the
// box's height) summed into the immersion and a running average turn point; the lift is immersion x the buoyancy
// constant, cut back while the car is already rising fast. False when there is no water here or nothing is under.
bool CarBuoyancy(int model, bool heli, const VehicleModel& vm, const Body& b, float buoyancy, float ts, const VehicleWorld& world,
                 V3& turnPoint, V3& force) {
    float level = 0.0f;
    if (!world.water || !world.water(b.pos.x, b.pos.y, &level)) return false;
    V3 mn(vm.colMin), mx(vm.colMax);
    if (model == 417) { mn.y *= 0.4f; mx.y *= 1.15f; }   // the Leviathan
    else if (heli) { mn.y = -mx.y; mx.z = mn.z * -1.1f; mn.z *= 0.85f; }
    const V3 half = (mx - mn) * 0.5f;
    const float scale = (half.z > half.x && half.z > half.y) ? 1.0f / half.z : (half.y > half.x && half.y > half.z) ? 1.0f / half.y : 1.0f / half.x;
    const V3 norm = half * scale;
    float immersion = 0.0f, checked = 1.0f;
    V3 turn;
    bool inWater = false;
    for (int ix = 0; ix < 3; ix++)
        for (int iy = 0; iy < 3; iy++) {
            V3 pt(mn.x + half.x * (float)ix, mn.y + half.y * (float)iy, 0.0f);
            const V3 tr = b.Rot(pt);
            float lv = level;
            if (!world.water(b.pos.x + tr.x, b.pos.y + tr.y, &lv)) lv = level;
            pt.z = lv - (tr.z + b.pos.z);
            if (pt.z < mn.z) continue;   // COMPLETELY_ABOVE_WATER
            pt.z = std::min(pt.z, mx.z);
            const float d = std::fabs(pt.z - mn.z);
            immersion += d;
            const V3 cur(pt.x * norm.x, pt.y * norm.y, (pt.z + mn.z) * norm.z * 0.5f);
            const float nc = 1.0f / checked;
            turn = cur * (nc * d) + turn * (1.0f - nc);
            checked += 1.0f;
            inWater = true;
        }
    if (!inWater) return false;
    immersion /= 9.0f * (mx.z - mn.z);
    turnPoint = b.Rot(turn);
    const float lift = immersion * buoyancy * ts;
    force = V3(0.0f, 0.0f, lift);
    const float rising = b.mass * b.mv.z;
    if (rising > lift * 4.0f) force.z = std::max(0.0f, lift - rising);
    return true;
}

constexpr float kRoadAdhesion = 4.5f;   // surface.dat Rubber (wheelbase) on Road
constexpr float kBodyAdhesion = 3.0f;   // surface.dat Hard (car body) on Road
constexpr float kDampingLimitInFrame = 0.25f;        // CPhysical::DAMPING_LIMIT_IN_FRAME (0x8CD7A0)
constexpr float kDampingLimitOfSpringForce = 0.999f; // CPhysical::DAMPING_LIMIT_OF_SPRING_FORCE (0x8CD7A4)
constexpr float kRollOntoWheelsForce = 0.0025f;      // CAutomobile::ROLL_ONTO_WHEELS_FORCE
constexpr float kTracFracLimit = 0.3f;               // WS_TRAC_FRAC_LIMIT (0x8D34C0)
constexpr float kAlreadySpinningLoss = 0.2f;         // WS_ALREADY_SPINNING_LOSS (0x8D34C4)
constexpr float kWheelFriction = 0.9f;               // cHandlingDataMgr::fWheelFriction
constexpr float kElasticity = 0.05f;                 // CAutomobile m_fElasticity
enum { WHEEL_NORMAL = 0, WHEEL_SPINNING = 1, WHEEL_SKIDDING = 2, WHEEL_FIXED = 3 };

// CVehicle::ProcessWheel's file static: set by the first wheel that ever slips and never cleared, so from
// then on a tyre over its grip loses no further traction (tractionLoss 1).
bool g_alreadySkidding = false;

constexpr float kBurstTyreMod = 0.13f;                // fBurstTyreMod (0x8D34B4)
constexpr float kBurstSpeedMax = 0.3f;                // fBurstSpeedMax
constexpr float kWheelDamageEffect = 0.5f;            // CDamageManager::ResetDamageStatusAndWheelDamage
constexpr float kTrailerSuspensionForce = 1.5f;       // CTrailer::m_fTrailerSuspensionForce (0x8D3464)
constexpr float kTrailerDampingForce = 0.1f;          // CTrailer::m_fTrailerDampingForce (0x8D3468)
constexpr float kTrailerTowedMinRatio = 0.9f;         // TRAILER_TOWED_MINRATIO (0x8D346C)
constexpr float kSupportRetract = 0.002f, kSupportExtend = 0.002f, kSupportWaitExtend = 0.0005f;   // 0x871C14..1C

// The suspension lines' heights in car space: the model's, or a hydraulic car's own copy.
struct Lines { float top[4], bot[4], spring[4], len[4]; };
Lines LinesOf(const VehicleInstance& c, const VehicleModel& vm) {
    Lines L;
    for (int i = 0; i < 4; i++) {
        const bool own = c.hyd.lines;
        L.top[i] = own ? c.hyd.top[i] : vm.suspStart[i][2];
        L.bot[i] = own ? c.hyd.bot[i] : vm.suspEnd[i][2];
        L.spring[i] = own ? c.hyd.spring[i] : vm.suspSpring[i];
        L.len[i] = L.top[i] - L.bot[i];
    }
    return L;
}

bool FrontDrive(char d) { return d == 'F' || d == '4'; }
bool RearDrive(char d) { return d == 'R' || d == '4'; }

// CVehicle::ProcessWheel: the tyre's grip pushes back on the speed of its contact patch.
void ProcessWheel(Body& b, const HandlingData& h, CarPhys& p, const V3& wheelFwd, const V3& wheelRight, const V3& contactSpeed,
                  const V3& contactPoint, int wheelsOnGround, float thrust, float brake, float adhesion, int wheelId, int* wheelState,
                  float ts, bool burst) {
    float right = 0.0f, fwd = 0.0f;
    const float speedFwd = Dot(wheelFwd, contactSpeed), speedRight = Dot(wheelRight, contactSpeed);
    const bool braking = brake != 0.0f;
    const bool driving = !braking && thrust != 0.0f;
    adhesion *= ts;
    if (*wheelState != WHEEL_NORMAL) {
        g_alreadySkidding = true;
        adhesion *= h.tractionLoss;
        if (*wheelState == WHEEL_SPINNING && p.player) adhesion *= 1.0f - std::fabs(p.gas) * kAlreadySpinningLoss;
    }
    *wheelState = WHEEL_NORMAL;
    if (speedRight != 0.0f) {
        right = -(speedRight / (float)wheelsOnGround);
        if (burst) right += std::min(speedFwd, kBurstSpeedMax) * RandRange(-kBurstTyreMod, kBurstTyreMod);   // a flat wobbles
    }
    if (driving) {
        fwd = thrust;
        right = std::clamp(right, -adhesion, adhesion);
    } else if (speedFwd != 0.0f) {
        fwd = -speedFwd / (float)wheelsOnGround;
        if (!braking && std::fabs(p.gas) < 0.01f) {   // rolling resistance
            brake = kWheelFriction / h.mass;
            if (brake > 500.0f) brake *= 0.1f;
            else if (h.name == "rcbandit") brake *= 0.2f;
        }
        if (brake > adhesion) {
            if (std::fabs(speedFwd) > 0.005f) *wheelState = WHEEL_FIXED;
        } else fwd = std::clamp(fwd, -brake, brake);
    }
    const float speedSq = right * right + fwd * fwd;
    if (speedSq > adhesion * adhesion) {
        if (*wheelState != WHEEL_FIXED) {
            float limit = kTracFracLimit;
            if (speedFwd > 0.15f && (wheelId == 0 || wheelId == 2)) limit += limit;
            *wheelState = driving && limit * adhesion < std::fabs(fwd) ? WHEEL_SPINNING : WHEEL_SKIDDING;
        }
        float tractionLoss = h.tractionLoss;
        if (g_alreadySkidding) tractionLoss = 1.0f;
        else if (*wheelState == WHEEL_SPINNING && p.player) tractionLoss *= 1.0f - std::fabs(p.gas) * kAlreadySpinningLoss;
        const float l = std::sqrt(speedSq);
        fwd *= adhesion * tractionLoss / l;
        right *= adhesion * tractionLoss / l;
    }
    if (fwd != 0.0f || right != 0.0f) {
        const V3 total = wheelFwd * fwd + wheelRight * right;
        V3 turnDir = total;
        bool separate = false;
        if (h.antiDive > 0.0f) {
            if (braking) { separate = true; turnDir -= wheelFwd * (h.antiDive * fwd); }
            else if (driving) { separate = true; turnDir -= wheelFwd * (0.5f * h.antiDive * fwd); }
        }
        const float speed = Len(total);
        const float turnSpeed = separate ? Len(turnDir) : speed;
        const V3 dir = Norm(total);
        turnDir = separate ? Norm(turnDir) : dir;
        b.ApplyMoveForce(dir * (speed * b.mass));
        b.ApplyTurnForce(turnDir * (turnSpeed * b.MassAt(contactPoint, turnDir)), contactPoint);
    }
}

}  // namespace

// cTransmission::InitGearRatios
void Transmission::InitGearRatios() {
    for (Gear& g : gears) g = Gear{};
    const float halfGear = 0.5f * maxVelocity / (float)numGears;
    const float topGear = maxVelocity - halfGear;
    for (int i = 1; i <= numGears; i++) {
        Gear& g = gears[i];
        const Gear& prev = gears[i - 1];
        g.maxVelocity = (float)i * topGear / (float)numGears + halfGear;
        const float diff = g.maxVelocity - prev.maxVelocity;
        if (i >= numGears) g.changeUp = maxVelocity;
        else {
            gears[i + 1].changeDown = 0.42f * diff + prev.maxVelocity;
            g.changeUp = 0.6667f * diff + prev.maxVelocity;
        }
    }
    gears[0].maxVelocity = maxReverseVelocity;
    gears[0].changeUp = -0.01f;
    gears[0].changeDown = maxReverseVelocity;
    gears[1].changeDown = -0.01f;
}

// cTransmission::CalculateDriveAcceleration (no cheats)
float Transmission::DriveAcceleration(float gas, uint8_t& gear, float velocity, float* inertia1, float* inertia2, bool allWheelsOnGround,
                                      float timeStep) const {
    if (velocity < maxReverseVelocity) return 0.0f;
    while (velocity <= maxVelocity) {
        const Gear& g = gears[gear];
        bool accelerate = false, shiftDown = false;
        if (velocity > g.changeUp) {
            if (gear == 0 && gas <= 0.0f) accelerate = true;
            else gear++;
        } else {
            if (velocity >= g.changeDown || gear == 0 || (gear == 1 && gas >= 0.0f)) accelerate = true;
            shiftDown = true;
        }
        if (accelerate) {
            float mult;
            if (numGears == 1) mult = 1.0f;
            else if (gear >= 1) {
                float n = 1.0f - ((float)gear - 1.0f) / ((float)numGears - 1.0f);
                n *= n;
                mult = (handlingFlags & 0x1u) ? n * 5.0f : (handlingFlags & 0x2u) ? n * 4.0f : n * 3.0f;
                mult += 1.0f;
            } else mult = 4.5f;   // reverse
            float drive = mult * engineAccel * 0.4f * gas * timeStep;
            if (inertia1 && inertia2) {
                if (allWheelsOnGround) {
                    float cur, span;
                    const float maxChange = maxVelocity / (float)numGears * (1.0f / 3.0f);
                    if (gear) {
                        if (gear == 1) { cur = maxChange + velocity; span = maxChange + gears[1].changeUp; }
                        else { cur = velocity - g.changeDown; span = g.changeUp - g.changeDown; }
                    } else { cur = maxChange - velocity; span = maxChange - gears[0].changeDown; }
                    const float ratio = cur / span;
                    const float accel = std::clamp(1.0f - (ratio - *inertia1) * engineInertia, 0.1f, 1.0f);
                    *inertia1 = ratio;
                    *inertia2 = accel * (1.0f - 0.85f) + 0.85f * *inertia2;
                    drive *= *inertia2;
                } else {
                    *inertia1 = std::min(*inertia1 + std::fabs(gas) / engineInertia * timeStep * 0.1f, 1.0f);
                    *inertia2 = 0.1f;
                }
            }
            const float gearMax = g.maxVelocity;
            float over;
            if (gearMax >= 0.0f || velocity >= gearMax) {
                if (gearMax <= 0.0f || velocity <= gearMax) return drive;
                over = velocity - gearMax;
            } else over = gearMax - velocity;
            return drive * (1.0f - std::min(over / 0.05f, 1.0f));
        }
        if (shiftDown) gear--;
        inertia1 = inertia2 = nullptr;
        allWheelsOnGround = false;
        if (velocity < maxReverseVelocity) return 0.0f;
    }
    return 0.0f;
}

bool Vehicles::UsesCarPhysics(int model) const {
    const VehicleInfo* info = Info(model);
    if (!info) return false;
    std::string t = info->type;
    for (char& ch : t) ch = (char)std::tolower((unsigned char)ch);
    return t == "car" || t == "mtruck" || t == "quad" || t == "trailer";
}

// CAutomobile::SetupSuspensionLines, and the probe spheres the body touches the world with.
void Vehicles::SetupSuspension(VehicleModel& vm, const HandlingData& h) {
    static const int kDummy[4] = {0, 2, 1, 3};   // eCarWheel -> lf, rf, lb, rb dummy
    for (int i = 0; i < 4; i++) {
        const int k = kDummy[i];
        float wp[3];
        if (vm.wheelFound[k]) std::memcpy(wp, vm.wheelPos[k], sizeof(wp));
        else {   // a missing dummy: mirror the one across, else guess from the body
            const int other = k ^ 1;
            if (vm.wheelFound[other]) { std::memcpy(wp, vm.wheelPos[other], sizeof(wp)); wp[0] = -wp[0]; }
            else { wp[0] = (k & 1 ? 1.0f : -1.0f) * vm.bbHalfX * 0.8f; wp[1] = (k < 2 ? 0.5f : -0.5f) * vm.wheelBase; wp[2] = 0.0f; }
        }
        vm.suspStart[i][0] = vm.suspEnd[i][0] = wp[0];
        vm.suspStart[i][1] = vm.suspEnd[i][1] = wp[1];
        vm.suspStart[i][2] = wp[2] + h.suspUpper;
        vm.suspEnd[i][2] = wp[2] + h.suspLower - vm.wheelSize[i] / 2.0f;
        vm.suspSpring[i] = h.suspUpper - h.suspLower;
        vm.suspLine[i] = vm.suspStart[i][2] - vm.suspEnd[i][2];
    }
    vm.frontHeight = (1.0f - 1.0f / (h.suspForce * 4.0f)) * vm.suspSpring[0] + vm.wheelSize[0] / 2.0f - vm.suspStart[0][2];
    if (vm.isTrailer && vm.info.id != 606 && vm.info.id != 607 && vm.colFound) {   // CTrailer::SetupSuspensionLines (not the baggage carts)
        float mn[3], mx[3];
        std::memcpy(mn, vm.colMin, sizeof(mn)); std::memcpy(mx, vm.colMax, sizeof(mx));
        mn[2] = std::min(mn[2], vm.suspEnd[0][2]);
        for (int k = 0; k < 2; k++) {
            vm.supportStart[k][0] = vm.supportEnd[k][0] = k == 0 ? mn[0] : -mn[0];
            vm.supportStart[k][1] = vm.supportEnd[k][1] = mx[1];
            vm.supportStart[k][2] = mx[2];
        }
        // the legs share the weight with the wheels by their distance from the middle; the nose rests at the
        // height the springs settle the wheels at under that share
        const float legY = std::fabs(vm.supportStart[0][1]);
        const float wheelY = (std::fabs(vm.suspStart[0][1]) + std::fabs(vm.suspStart[1][1])) * 0.5f;
        const float weight = legY / (wheelY + legY);
        vm.frontHeight = vm.suspSpring[0] * (1.0f - weight / (h.suspForce * 4.0f)) + vm.wheelSize[0] / 2.0f - vm.suspStart[0][2];
        const float endZ = vm.supportStart[0][2] - (vm.supportStart[0][2] + vm.frontHeight) / (1.0f - (1.0f - weight) / (kTrailerSuspensionForce * 2.0f));
        vm.supportEnd[0][2] = vm.supportEnd[1][2] = endZ;
        vm.legHeight = vm.nodes[0].found ? vm.nodes[0].local[14] : 0.0f;
        vm.trailerLegs = true;
    }
    vm.probes = vm.colSpheres;
    for (const VehicleModel::ColBox& bx : vm.colBoxes) {   // fill each box with a row or grid of spheres
        float half[3], mid[3];
        for (int a = 0; a < 3; a++) { half[a] = 0.5f * std::fabs(bx.hi[a] - bx.lo[a]); mid[a] = 0.5f * (bx.hi[a] + bx.lo[a]); }
        const float r = std::max(0.05f, std::min({half[0], half[1], half[2]}));
        const int nx = std::max(1, (int)std::ceil(half[0] / r)), ny = std::max(1, (int)std::ceil(half[1] / r)), nz = std::max(1, (int)std::ceil(half[2] / r));
        for (int ix = 0; ix < nx; ix++)
            for (int iy = 0; iy < ny; iy++)
                for (int iz = 0; iz < nz; iz++) {
                    auto at = [&](int a, int n, int idx) { return n == 1 ? mid[a] : bx.lo[a] + r + (2.0f * half[a] - 2.0f * r) * (float)idx / (float)(n - 1); };
                    vm.probes.push_back({{at(0, nx, ix), at(1, ny, iy), at(2, nz, iz)}, r, bx.piece});
                }
    }
    if (vm.probes.empty()) {   // no COL: a row of spheres along the body
        const float r = std::max(0.3f, vm.bbHalfX * 0.6f);
        for (int k = 0; k < 4; k++) {
            const float y = vm.bbMinY + r + (vm.bbMaxY - vm.bbMinY - 2.0f * r) * (float)k / 3.0f;
            vm.probes.push_back({{-vm.bbHalfX + r, y, 0.2f}, r});
            vm.probes.push_back({{vm.bbHalfX - r, y, 0.2f}, r});
        }
    }
    vm.probeRadius = 0.0f;
    for (const VehicleModel::ColSphere& s : vm.probes) vm.probeRadius = std::max(vm.probeRadius, Len(V3(s.c)) + s.r);
    vm.suspReady = true;
}

// CAutomobile::HydraulicControl: the car keeps its own suspension lines. The horn pumps it up to the extended limits
// (or drops it back), the special control stick lifts the wheels on one corner or side (num 8 / 2 / 4 / 6), the jump
// key lifts all four; a parked car settles a little below its normal ride. Each change moves the lines and keeps the
// tyres where they touched.
void Vehicles::HydraulicControl(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h) {
    if (h.handlingFlags & 0x40000u) return;   // HYDRAULIC_NONE
    VehicleInstance::Hydraulics& y = c.hyd;
    CarPhys& p = c.phys;
    if (!y.lines) {   // CVehicle::GetSpecialColModel: a copy of the model's lines
        for (int i = 0; i < 4; i++) { y.top[i] = vm.suspStart[i][2]; y.bot[i] = vm.suspEnd[i][2]; y.spring[i] = vm.suspSpring[i]; }
        y.lines = true;
    }
    const bool geom = (h.handlingFlags & 0x10000u) != 0;
    const float wheelR = vm.wheelSize[0] / 2.0f;
    float wheelZ[4];   // GetWheelPosn z: the dummy under each line
    for (int i = 0; i < 4; i++) wheelZ[i] = vm.suspStart[i][2] - h.suspUpper;
    float nU = y.normUp, nL = y.normLo, eU = y.extUp, eL = y.extLo;
    float nS = nU - nL, eS = eU - eL;
    if (y.extUp >= 100.0f) {   // the limits, once: a longer travel for the lowriders, raised up to the body's lowest point
        float minZ = 0.0f;
        for (const VehicleModel::ColSphere& s : vm.colSpheres) minZ = std::min(minZ, s.c[2] - s.r);
        const float limitExtend = minZ - wheelZ[0];
        nL = h.suspLower; nU = h.suspUpper;
        nS = nU - nL;
        if (geom) nS *= 1.5f;
        nU = nS + nL;
        y.normUp = nU; y.normLo = nL;
        eU = nU - (geom ? 0.4f : 0.2f);
        eL = nL - (geom ? 0.4f : 0.2f);
        eU = std::max(eU, limitExtend);
        eS = eU - eL;
        y.extUp = eU; y.extLo = eL;
        const float idle = (1.0f - 0.7f) * 0.5f * nS;
        y.idleUp = nU - idle; y.idleLo = nL + idle;
        const float wheelHeight = limitExtend + wheelR - 0.1f;
        if (wheelHeight < y.idleLo) { const float d = y.idleLo - wheelHeight; y.idleUp -= d; y.idleLo -= d; }
    }
    bool setPrev = false;
    if (y.state >= 20 || y.prevSpeed <= 0.02f || p.gas == 0.0f) {
        if (y.state && y.state < 61 && y.prevSpeed < 0.01f) y.state--;   // at rest the pump winds down to idle
    } else if (y.state) y.state++;
    else { y.state = 20; setPrev = true; }   // driving off: the pump starts

    float maxDelta = 0.0f;
    auto setLine = [&](int i, float upper, float spring) {
        const float len = spring + wheelR;
        const float tyre = y.top[i] - (y.top[i] - y.bot[i]) * p.comp[i];   // where the tyre touched on the old line
        const float top = wheelZ[i] + upper, bot = top - len;
        maxDelta = std::max(std::fabs(maxDelta), std::fabs(bot - y.bot[i]));
        y.top[i] = top; y.bot[i] = bot; y.spring[i] = spring;
        if (p.comp[i] < 1.0f) p.comp[i] = std::min((top - tyre) / len, 1.0f);
    };
    bool triggered = false;
    if (p.inHornJust) {   // (one press, one toggle: the frame's later steps see it released)
        p.inHornJust = false;
        y.state = y.state < 500 ? 500 : 60;
        if (y.state >= 500) { for (int i = 0; i < 4; i++) setLine(i, eU, eS); triggered = true; }
        else for (int i = 0; i < 4; i++) setLine(i, nU, nS);
    }
    if (!triggered) {
        float chg[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        if (!p.inHydJump) {   // the stick's direction picks the corner, its push the lift
            const float lr = p.inGunLeftRight * 128.0f, ud = p.inGunUpDown * 128.0f;
            const float jump = std::sqrt(ud * ud + lr * lr) * 1.5f / 128.0f;
            const float ang = std::atan2(lr, ud) - 0.78539816f;
            const float rr = std::cos(ang) * jump, fr = std::sin(ang) * jump;
            chg[0] = std::max(0.0f, -rr); chg[1] = std::max(0.0f, -fr);   // eCarWheel: front left, rear left,
            chg[2] = std::max(0.0f, fr); chg[3] = std::max(0.0f, rr);     // front right, rear right
        }
        maxDelta = 0.0f;
        if (y.state < 500) {
            if (chg[0] == 0.0f && chg[1] == 0.0f && chg[2] == 0.0f && chg[3] == 0.0f) {
                if (y.state == 0) { nU = y.idleUp; nL = y.idleLo; nS = nU - nL; }
            } else {
                if (y.state == 0) std::fill(chg, chg + 4, 0.0f);
                y.state = 60;
            }
            for (int i = 0; i < 4; i++) {
                const float k = std::min(chg[i], 1.0f);
                setLine(i, (eU - nU) * k + nU, (eS - nS) * k + nS);
            }
        } else {
            if (y.state < 504) y.state++;
            for (int i = 0; i < 4; i++) {
                const float k = std::min(chg[i], 1.0f);
                setLine(i, (nU - eU) * k + eU, (nS - eS) * k + eS);
            }
        }
        const float limitDiff = eL - nL;
        if (limitDiff != 0.0f && std::fabs(maxDelta / limitDiff) > 0.01f) {
            const float f = std::clamp((limitDiff + maxDelta) * 0.5f / limitDiff, 0.0f, 1.0f);
            if (f < 0.4f || f > 0.6f) setPrev = true;
        }
    }
    if (setPrev || triggered)
        for (int i = 0; i < 4; i++) {
            const float t = 1.0f - y.spring[i] / (y.top[i] - y.bot[i]);
            p.compPrev[i] = (p.comp[i] - t) / (1.0f - t);
        }
}

// One CAutomobile::ProcessControl at time step ts, then CPhysical::ProcessCollision.
void Vehicles::CarStep(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world, bool collide) {
    CarPhys& p = c.phys;
    const size_t self = (size_t)(&c - m_cars.data());
    if (c.model == 460) for (uint8_t& w : c.wheelStatus) w = 2;   // CPlane::ProcessControl: the Skimmer has floats, no wheels
    Body b = Load(c, h);
    const Transmission& tr = h.trans;
    p.burnout = false;
    const bool hitWallPrev = p.hitWall;
    p.hitWall = false;
    p.player = player;
    const bool heli = UsesHeliPhysics(c.model), plane = UsesPlanePhysics(c.model);

    if (plane) PlaneWheelInputs(c, h, player);
    else if (heli) {   // CHeli::ProcessControlInputs: the wheels stay braked and straight, the rotor does the rest
        p.gas = 0.0f; p.brake = 1.0f; p.steerAngle = 0.0f;
        if (!player) p.handbrake = false;
    } else if (player) {   // CAutomobile::ProcessControlInputs
        p.rawSteer += (-p.inSteer - p.rawSteer) / 5.0f * ts;
        p.rawSteer = std::clamp(p.rawSteer, -1.0f, 1.0f);
        const float fwdVel = Dot(b.mv, b.fwd);
        const float gasInput = p.inAccel - p.inBrake;
        float gas, brake;
        if (std::fabs(fwdVel) < 0.01f) {
            if (p.inAccel > 150.0f / 255.0f && p.inBrake > 150.0f / 255.0f) { p.burnout = true; gas = p.inAccel; brake = p.inBrake; }
            else { gas = gasInput; brake = 0.0f; }
        } else if (fwdVel >= 0.0f) {
            if (gasInput < 0.0f) { gas = 0.0f; brake = std::fabs(gasInput); }
            else { gas = gasInput; brake = 0.0f; }
        } else if (gasInput >= 0.0f && (p.gas <= 0.5f || fwdVel <= -0.15f)) { gas = 0.0f; brake = gasInput; }
        else { gas = gasInput; brake = 0.0f; }
        p.gas = gas; p.brake = brake;
        p.steerAngle = h.steeringLock * 0.0174532925f * std::copysign(p.rawSteer * p.rawSteer, p.rawSteer);
    } else if (p.ai) {   // ProcessAI, STATUS_PHYSICS: the AI driver's controls
        p.gas = p.aiGas; p.brake = p.aiBrake; p.steerAngle = p.aiSteer; p.handbrake = p.aiHandbrake;
    } else if (c.towedBy >= 0) {   // ProcessAI, STATUS_IS_TOWED: rolling free behind the tractor
        p.handbrake = false;
        p.brake = 0.0f; p.steerAngle = 0.0f; p.gas = 0.0f;
    } else {   // ProcessAI, STATUS_ABANDONED
        p.brake = Len2(b.mv) < 0.01f ? 0.2f : 0.0f;
        p.handbrake = false;
        p.steerAngle = 0.0f;
        p.gas = 0.0f;
    }

    const int hoistPrev = c.hoist;
    if (player && (c.model == 525 || c.model == 531)) {   // CAutomobile::ProcessControl: the tow truck's and tractor's hoist
        Store(b, c);
        TowTruckControl(self, ts);
        b = Load(c, h);
    }
    if (player) MiscComponentControl(self, ts);
    if (c.model == 524 && !c.wrecked) {   // CAutomobile::PreRender: the mixer drum turns, faster as the control winds it up
        c.propRot += ((float)c.hoist / 2500.0f + 0.5f) * ts * 0.02f;
        if (c.propRot > 6.2831855f) c.propRot -= 6.2831855f;
    }
    if (c.model == 532) {   // the combine's reel: at least 0.1 a step going forward, still in reverse
        const float s = Dot(b.mv, b.fwd);
        c.propRot -= (s < 0.0f ? 0.0f : std::max(s, 0.1f)) * ts;
        if (c.propRot < -6.2831855f) c.propRot += 6.2831855f;
    }
    if (player && c.hyd.on && !heli && !plane) HydraulicControl(c, vm, h);
    const Lines L = LinesOf(c, vm);
    const bool hydGeom = c.hyd.on && (h.handlingFlags & 0x10000u);   // HYDRAULIC_GEOM with hydraulics in
    if (c.model == 460 && h.boat.found) {   // CAutomobile::ProcessControl: the Skimmer floats as a boat (else ProcessBuoyancy)
        Store(b, c);
        BoatControl(c, vm, h, ts, player, hitWallPrev, world);
        b = Load(c, h);
    } else {   // CAutomobile::ProcessBuoyancy
        const float handlingConst = h.mass * 0.8f / (float)std::max(1, h.percentSubmerged);
        if (p.buoyancyConst <= 0.0f) p.buoyancyConst = handlingConst;
        V3 turnPoint, force;
        if (!CarBuoyancy(c.model, heli, vm, b, p.buoyancyConst, ts, world, turnPoint, force)) {
            p.drowning = false; p.touchingWater = false;
            p.buoyancyConst = handlingConst;
        } else {
            if (plane && b.mv.z < -1.0f && !c.wrecked) { c.health = 0.0f; c.engineStatus = 225; c.burnTime = 5.0f; }   // a plane dived in: BlowUpCar now
            p.touchingWater = true;
            const bool vortex = c.model == 539;
            const float used = b.mass / 125.0f;
            float fz = force.z / (std::max(0.01f, ts) * used);
            if (used > p.buoyancyConst) fz *= 1.05f * used / p.buoyancyConst;
            const float drag = std::pow(std::max(0.5f, 1.0f - fz / 20.0f), ts);
            if (!vortex || b.up.z <= 0.3f || p.drowning) { b.mv *= drag; b.tv *= drag; }
            bool rotorKilled = false, done = false;
            if (heli && p.rotorSpeed > 0.15f) {   // water stops a rotor
                bool kill = false;
                if (c.model != 447 && c.model != 417) {
                    b.ApplyMoveForce(force * -2.0f / std::max(1.0f, fz * 8.0f));
                    b.tv *= drag;
                    if (fz <= 0.9f) done = true;
                    kill = true;
                }
                if (!done && (fz > 3.0f || kill)) { p.rotorSpeed = 0.0f; rotorKilled = true; }
            }
            if (!done) {
                b.ApplyMoveForce(force);
                b.ApplyTurnForce(force, turnPoint);
                const bool amphibious = c.model == 447 || c.model == 417;   // the Sea Sparrow and the Leviathan float
                bool anyFree = false;
                for (float cp : p.compPrev) anyFree = anyFree || cp >= 1.0f;
                if (amphibious && fz < 3.0f && (b.up.z > -0.5f || fz < 0.6f)) p.drowning = false;
                else if (vortex && player && b.up.z > 0.3f && !p.drowning) p.drowning = false;
                else if (rotorKilled || fz >= 1.0f || (fz > 0.6f && anyFree)) {   // under: it sinks, slowly
                    p.drowning = true;
                    b.mv.z = std::max(-0.1f, b.mv.z);
                    if (b.mass * 8.0f / 1250.0f < p.buoyancyConst) p.buoyancyConst -= b.mass * 0.01f / 1250.0f;
                    if (p.buoyancyConst < b.mass / 125.0f) c.engineOn = false;
                } else {
                    p.drowning = false;
                }
            }
        }
    }

    // CPhysical::ProcessControl: ApplyFriction, ApplyGravity, ApplyAirResistance
    b.mv += b.fmv; b.tv += b.ftv;
    b.fmv = V3(); b.ftv = V3();
    b.mv.z -= ts * 0.008f;
    {
        float air = h.dragMult <= 0.01f ? h.dragMult : h.dragMult / 1000.0f / 2.0f;
        if (plane && p.gearStatus == 1.0f) air = h.dragMult / 1000.0f / 2.0f * h.fly.gearUpR;   // CPlane::SetGearUp
        b.mv *= std::pow(std::max(0.0f, 1.0f - Len(b.mv) * air), ts);
        b.tv *= 0.99f;
    }

    // The lines' hit fractions become spring compressions (0 fully squashed, 1 the wheel hanging free).
    for (int i = 0; i < 4; i++) {
        const float wr = 1.0f - L.spring[i] / L.len[i];
        p.comp[i] = (p.comp[i] - wr) / (1.0f - wr);
    }
    {   // CAutomobile::DoBurstAndSoftGroundRatios: a missing wheel never touches, a flat sags now and then
        const float fwdRatio = std::fabs(Dot(b.mv, b.fwd));
        for (int i = 0; i < 4; i++) {
            if (c.wheelStatus[i] == 2) p.comp[i] = 1.0f;
            else if (c.wheelStatus[i] == 1 && RandInt((int)(fwdRatio * 40.0f) + 98 + 1) < 100)
                p.comp[i] = std::min(1.0f, p.comp[i] + (L.len[i] - L.spring[i]) / L.len[i] / 4.0f);
        }
    }

    // ---- CAutomobile::ProcessSuspension ----
    {
        float limit[4] = {0, 0, 0, 0};
        const float fwdSpeed = std::fabs(Dot(b.mv, b.fwd));
        V3 cp[4], dir[4];
        for (int i = 0; i < 4; i++) {
            dir[i] = -b.up;
            cp[i] = p.comp[i] < 1.0f ? V3(p.colPt[i]) - b.pos : V3();
        }
        for (int i = 0; i < 4; i++) {   // ApplySpringCollisionAlt
            if (p.comp[i] >= 1.0f) continue;
            const float bias = (i == 1 || i == 3) ? 1.0f - h.suspBias : h.suspBias;
            const float stress = 1.0f - p.comp[i];
            V3 n(p.colN[i]);
            if (Dot(dir[i], n) > 0.0f) n = -n;
            // a lowrider with hydraulics stiffens its springs at a crawl
            const float force = hydGeom && fwdSpeed < 0.15f ? h.suspForce * 1.5f : h.suspForce;
            limit[i] = stress * (std::min(ts, 3.0f) * b.mass) * force * bias * 0.016f;
            b.ApplyForce(n * limit[i], cp[i]);
        }
        V3 cs[4];
        for (int i = 0; i < 4; i++) {
            cs[i] = b.Speed(cp[i]);
            if (p.comp[i] < 1.0f && p.colN[i][2] > 0.35f) dir[i] = -V3(p.colN[i]);
        }
        for (int i = 0; i < 4; i++) {   // ApplySpringDampening
            if (p.comp[i] >= 1.0f) continue;
            const float speedAlong = Dot(cs[i], dir[i]);
            const float pointAlong = Dot(b.Speed(cp[i]), dir[i]);
            const float damping = c.hyd.on && h.suspDamping > 0.1f && fwdSpeed < 0.15f ? 0.1f : h.suspDamping;   // and bounces softer
            const float dts = std::clamp(std::min(ts, 3.0f) * damping, -kDampingLimitInFrame, kDampingLimitInFrame);
            float ds = -(dts * speedAlong);
            if (ds > 0.0f && ds + pointAlong > 0.0f) ds = pointAlong >= 0.0f ? 0.0f : -pointAlong;
            else if (ds < 0.0f && ds + pointAlong < 0.0f) ds = pointAlong <= 0.0f ? 0.0f : -pointAlong;
            float force = b.MassAt(cp[i] - b.ComW(), dir[i]) * ds;
            force = std::min(force, std::fabs(limit[i]) * kDampingLimitOfSpringForce);
            b.ApplyForce(dir[i] * force, cp[i]);
        }
        // A car lying on its side at a standstill is rolled back onto its wheels.
        const float threshold = player ? 0.02f : 0.04f, roll = player ? kRollOntoWheelsForce : kRollOntoWheelsForce * 2.0f;
        if (fwdSpeed < threshold) {
            float side = 0.0f;
            if (p.comp[0] == 1.0f && p.comp[1] == 1.0f && (p.comp[2] < 1.0f || p.comp[3] < 1.0f)) side = 1.0f;
            else if (p.comp[2] == 1.0f && p.comp[3] == 1.0f && (p.comp[0] < 1.0f || p.comp[1] < 1.0f)) side = -1.0f;
            if (side != 0.0f) {
                const V3 r = Cross(b.fwd, V3(0, 0, 1));
                if (std::fabs(Dot(r, b.right)) < 0.6f) {
                    b.ApplyTurnForce(b.up * (side * roll * b.turnMass), b.right * vm.colMax[0]);
                    b.ApplyMoveForce(-r * (roll * b.mass * side));
                }
            }
        }
    }
    if (vm.trailerLegs) {   // CTrailer::ProcessSuspension: the support legs carry the nose of a trailer left standing
        V3 lp[2], ls[2];
        bool touch[2] = {false, false};
        for (int k = 0; k < 2; k++) {
            if (!(p.supportRatio[k] < p.supportMin && p.supportMin > 0.1f)) continue;
            touch[k] = true;
            const V3 down = -b.up;
            lp[k] = V3(p.supportPt[k]) - b.pos;
            V3 n(p.supportN[k]);
            if (Dot(down, n) > 0.0f) n = -n;
            // ApplySpringCollisionAlt with the leg's spring, the line scaled to the extension
            const float limit = (1.0f - p.supportRatio[k] / p.supportMin) * (std::min(ts, 3.0f) * b.mass) * kTrailerSuspensionForce * 0.016f;
            b.ApplyForce(n * limit, lp[k]);
            ls[k] = b.Speed(lp[k]);
            const V3 dir = p.supportN[k][2] > 0.35f ? -V3(p.supportN[k]) : down;
            // ApplySpringDampening
            const float speedAlong = Dot(ls[k], dir);
            const float pointAlong = Dot(b.Speed(lp[k]), dir);
            const float dts = std::clamp(std::min(ts, 3.0f) * kTrailerDampingForce, -kDampingLimitInFrame, kDampingLimitInFrame);
            float ds = -(dts * speedAlong);
            if (ds > 0.0f && ds + pointAlong > 0.0f) ds = pointAlong >= 0.0f ? 0.0f : -pointAlong;
            else if (ds < 0.0f && ds + pointAlong < 0.0f) ds = pointAlong <= 0.0f ? 0.0f : -pointAlong;
            float force = b.MassAt(lp[k] - b.ComW(), dir) * ds;
            force = std::min(force, std::fabs(limit) * kDampingLimitOfSpringForce);
            b.ApplyForce(dir * force, lp[k]);
        }
        if (c.towedBy < 0) {   // standing, the feet grip like locked wheels (ProcessCarWheelPair front left / rear left, brake 1000)
            const float traction = h.tractionMult * 0.004f * 0.25f;
            for (int k = 0; k < 2; k++) {
                const int w = k;   // the leg borrows that wheel's ground contact
                if (!touch[k] || p.wheelCounts[w] <= 0.0f) continue;
                const V3 n(p.colN[w]);
                const V3 wf = Norm(b.fwd - n * Dot(b.fwd, n));
                const V3 wr = Norm(Cross(wf, n));
                int state = p.wheelState[w];
                ProcessWheel(b, h, p, wf, wr, ls[k], lp[k], std::max(1, p.numContactWheels), 0.0f, 1000.0f * 2.0f * h.brakeBias,
                             kRoadAdhesion * traction * 2.0f * h.tractionBias, w, &state, ts, false);
            }
        }
    }

    // ---- Wheel contact points and speeds ----
    V3 cp[4], cs[4];
    for (int i = 0; i < 4; i++) {
        cp[i] = p.comp[i] >= 1.0f ? b.Rot(V3(vm.suspEnd[i][0], vm.suspEnd[i][1], L.bot[i])) : V3(p.colPt[i]) - b.pos;
        cs[i] = b.Speed(cp[i]);
    }
    const float speedForward = Dot(b.mv, b.fwd);
    const float brake = h.brakeDecelGame * p.brake * ts;
    p.numContactWheels = 0;
    p.driveWheelsOnGround = 0;
    for (int i = 0; i < 4; i++) {
        if (p.comp[i] >= 1.0f) {
            p.wheelCounts[i] = std::max(p.wheelCounts[i] - ts, 0.0f);
            if (p.wheelCounts[i] <= 0.0f) continue;
        } else p.wheelCounts[i] = 4.0f;
        p.numContactWheels++;
        const bool front = i == 0 || i == 2;
        if (tr.driveType == '4' || (tr.driveType == 'F' && front) || (tr.driveType == 'R' && !front)) p.driveWheelsOnGround++;
    }
    float acceleration = 0.0f;
    if ((player || p.ai) && !heli && !plane)   // the engine runs while someone drives (aircraft have no driven wheels)
        acceleration = tr.DriveAcceleration(p.gas, p.gear, speedForward, &p.inertia1, &p.inertia2, p.driveWheelsOnGround != 0, ts);
    const float traction = h.tractionMult / 250.0f * 0.25f;
    {   // the player's steering is cut back to what the tyres can hold at speed
        float steer = 1.0f;
        if (speedForward > 0.01f && !(p.wheelCounts[0] <= 0.0f && p.wheelCounts[1] <= 0.0f) && player) {
            const float speedRight = Dot(b.mv, b.right);
            steer = std::min(kRoadAdhesion * traction * 4.0f * 4.0f / (speedForward * speedForward), 1.0f);
            steer = std::asin(steer) / (h.steeringLock * 0.0174532925f);
            if ((p.steerAngle < 0.0f && speedRight > 0.05f) || (p.steerAngle > 0.0f && speedRight < -0.05f) || p.handbrake || steer > 1.0f)
                steer = 1.0f;
        }
        p.steerAngle *= steer;
    }

    // ---- CAutomobile::ProcessCarWheelPair ----
    auto wheelPair = [&](int left, int rightWheel, float steerAngle, bool front) {
        const bool drive = front ? FrontDrive(tr.driveType) : RearDrive(tr.driveType);
        float suspBias = 2.0f * h.suspBias;
        float pairTraction = traction, pairBrake = brake;
        if (!front) {
            suspBias = 2.0f - suspBias;
            if (!p.handbrake || (h.handlingFlags & 0x40u)) {
                if (drive && p.burnout) {
                    pairBrake = 0.0f;
                    pairTraction = 0.0f;
                    // retail passes the contact point as the force and the sideways shove as the point
                    b.ApplyTurnForce(cp[left], b.right * (std::min(1.0f, 3000.0f / b.turnMass) * -0.002f * b.turnMass * p.steerAngle));
                } else if (!(h.handlingFlags & 0x80000u) && RearDrive(tr.driveType)) pairTraction *= p.tireTemp;
            } else pairBrake = 20000.0f;
        }
        if (p.wheelCounts[left] > 0.0f || p.wheelCounts[rightWheel] > 0.0f) {
            const bool canSteer = steerAngle > -100.0f;
            const float sn = canSteer ? std::sin(steerAngle) : 0.0f, cn = canSteer ? std::cos(steerAngle) : 0.0f;
            const float brakeBias = front ? 2.0f * h.brakeBias : 2.0f - 2.0f * h.brakeBias;
            const float tractionBias = front ? 2.0f * h.tractionBias : 2.0f - 2.0f * h.tractionBias;
            for (int w : {left, rightWheel}) {
                if (p.wheelCounts[w] <= 0.0f) continue;
                const float thrust = drive ? acceleration : 0.0f;
                const V3 n(p.colN[w]);
                V3 wf = Norm(b.fwd - n * Dot(b.fwd, n));
                V3 wr = Norm(Cross(wf, n));
                if (canSteer) {
                    const V3 t = wf * cn - wr * sn;
                    wr = wf * sn + wr * cn;
                    wf = t;
                }
                float adhesion = kRoadAdhesion * pairTraction;
                if (player) adhesion *= std::min(suspBias * h.suspForce * 4.0f * (1.0f - p.comp[w]), 2.0f);
                int state = p.wheelState[w];
                const bool burst = c.wheelStatus[w] == 1;
                ProcessWheel(b, h, p, wf, wr, cs[w], cp[w], p.numContactWheels, thrust, pairBrake * brakeBias,
                             adhesion * (burst ? kWheelDamageEffect : 1.0f) * tractionBias, w, &state, ts, burst);
                p.wheelState[w] = drive && p.gas < 0.0f && state == WHEEL_SPINNING ? WHEEL_NORMAL : state;
            }
        }
        if (!front && !(h.handlingFlags & 0x80000u)) {   // tyres heat up in a burnout and cool after
            if (p.burnout && drive && (p.wheelState[1] == WHEEL_SPINNING || p.wheelState[3] == WHEEL_SPINNING))
                p.tireTemp = std::min(p.tireTemp + ts / 1000.0f, 3.0f);
            else if (p.tireTemp > 1.0f) p.tireTemp = std::pow(0.995f, ts) * (p.tireTemp - 1.0f) + 1.0f;
        }
    };
    const bool steerRear = (h.handlingFlags & 0x20u) != 0, rearFirst = (h.handlingFlags & 0x800000u) != 0;
    if (!rearFirst) wheelPair(0, 2, steerRear ? -999.0f : p.steerAngle, true);
    wheelPair(1, 3, steerRear ? -p.steerAngle : -999.0f, false);
    if (rearFirst) wheelPair(0, 2, steerRear ? -999.0f : p.steerAngle, true);

    for (int i = 0; i < 4; i++) { p.compPrev[i] = p.comp[i]; p.comp[i] = 1.0f; }
    if (player && c.hyd.on && Len(b.mv) < 0.2f && std::fabs(p.inGunLeftRight * 128.0f) > 50.0f && std::fabs(p.inGunUpDown * 128.0f) < 50.0f) {
        // CAutomobile::ProcessControl: rocking a lowrider side to side, its roll is let die away only slowly
        const float roll = Dot(b.tv, b.fwd);
        const float keep = std::pow(std::pow(0.985f, ts) / (roll * roll * 5.0f + 1.0f), ts);
        b.ApplyTurnForce(b.up * (-(keep * roll - roll) * b.turnMass), b.right + b.ComW());
    }
    c.hyd.prevSpeed = Dot(b.mv, b.fwd);
    Store(b, c);
    if (c.towedBy >= 0) TowLinks(self, ts);
    else if (c.towing >= 0 && ((size_t)c.towing >= m_cars.size() || m_cars[(size_t)c.towing].towedBy != (int)self)) c.towing = -1;
    {   // CAutomobile::ProcessControl: with no gas, a crawl under 0.0045 per step on any axis stops dead once a wheel was down
        const bool slow = std::fabs(c.phys.mv[0]) < 0.0045f && std::fabs(c.phys.mv[1]) < 0.0045f && std::fabs(c.phys.mv[2]) < 0.0045f;
        const bool towStill = c.towedBy < 0 || Len2(V3(m_cars[(size_t)c.towedBy].phys.mv)) == 0.0f;
        const bool wheelDown = p.compPrev[0] < 1.0f || p.compPrev[1] < 1.0f || p.compPrev[2] < 1.0f || p.compPrev[3] < 1.0f;
        if ((p.gas == 0.0f || c.wrecked) && slow && !((heli || plane) && p.flyAccel != 0.0f) && c.hoist == hoistPrev && towStill && wheelDown) {
            std::fill(c.phys.mv, c.phys.mv + 3, 0.0f);
            c.phys.tv[2] = 0.0f;
        }
    }
    if (vm.trailerLegs) {   // CTrailer::ProcessControl: the legs wind up once hitched, down again when left
        float& ext = p.supportExt;
        if (c.towedBy >= 0 && ext > 0.0f) ext = std::max(ext - ts * kSupportRetract, 0.0f);
        else if (c.towedBy < 0 && ext < 1.0f) {
            ext = std::min(ext + ts * (ext <= 0.1f ? kSupportWaitExtend : kSupportExtend), 1.0f);
            p.fakePhysics = 0;
        }
        p.supportMin = c.towedBy >= 0 && ext < kTrailerTowedMinRatio ? kTrailerTowedMinRatio : ext;
        p.supportRatio[0] = p.supportRatio[1] = p.supportMin;
    }
    if (heli) { HeliFlyingCarStuff(c, h, ts, player, world); HeliRotors(c, ts); }
    if (plane) PlaneFlyingCarStuff(c, vm, h, ts, player, world);
    if (collide) CarCollision(c, vm, h, ts, world);
}

// CPhysical::ProcessCollision for a car against the world: move by the speeds, find where the suspension
// lines meet the ground, and push the body spheres back out of whatever they hit (ApplyCollisionAlt + ApplyFriction).
void Vehicles::CarCollision(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, const VehicleWorld& world, bool lines) {
    CarPhys& p = c.phys;
    Body b = Load(c, h);
    const Lines L = LinesOf(c, vm);
    // ApplyMoveSpeed, ApplyTurnSpeed, Reorthogonalise
    b.pos += b.mv * ts;
    {
        const V3 d = b.tv * ts;
        b.right += Cross(d, b.right);
        b.fwd += Cross(d, b.fwd);
        b.up += Cross(d, b.up);
        b.pos += Cross(d, b.Rot(-b.com));
        b.right = Norm(Cross(b.fwd, b.up));
        b.fwd = Norm(Cross(b.up, b.right));
        b.up = Cross(b.right, b.fwd);
    }
    // The four suspension lines against the world
    for (int i = 0; i < 4 && lines; i++) {
        const V3 a = b.pos + b.Rot(V3(vm.suspStart[i][0], vm.suspStart[i][1], L.top[i])), e = b.pos + b.Rot(V3(vm.suspEnd[i][0], vm.suspEnd[i][1], L.bot[i]));
        float n[3] = {0, 0, 1};
        const float f = world.line(&a.x, &e.x, n);
        if (f < 1.0f) {
            V3 nn(n);
            if (Dot(nn, e - a) > 0.0f) nn = -nn;   // the surface faces up the line
            p.comp[i] = f;
            (a + (e - a) * f).To(p.colPt[i]);
            nn.To(p.colN[i]);
        }
        const float frac = std::min(p.comp[i], 1.0f);   // the tyre sits on the hit, or hangs at the line's end
        p.wheelZ[i] = L.top[i] - frac * L.len[i] + vm.wheelSize[i] / 2.0f;
    }
    for (int k = 0; k < 2 && lines && vm.trailerLegs; k++) {   // CTrailer::ProcessEntityCollision: the support lines
        const V3 a = b.pos + b.Rot(V3(vm.supportStart[k])), e = b.pos + b.Rot(V3(vm.supportEnd[k]));
        float n[3] = {0, 0, 1};
        const float f = world.line(&a.x, &e.x, n);
        if (f < p.supportRatio[k]) {
            V3 nn(n);
            if (Dot(nn, e - a) > 0.0f) nn = -nn;
            p.supportRatio[k] = f;
            (a + (e - a) * f).To(p.supportPt[k]);
            nn.To(p.supportN[k]);
        }
    }
    // The body spheres against the world
    struct Contact { V3 point, normal; float depth; int piece; };
    Contact contacts[64];
    int count = 0;
    for (const VehicleModel::ColSphere& s : vm.probes) {
        if (count >= 64) break;
        const V3 centre = b.pos + b.Rot(V3(s.c));
        float pt[3], nm[3];
        if (!world.sphere(&centre.x, s.r, pt, nm)) continue;
        const V3 q(pt);
        const float depth = s.r - Len(centre - q);
        if (depth <= 0.0f) continue;
        contacts[count++] = {q, V3(nm), depth, s.piece};
    }
    float maxImpact = 0.0f;
    V3 impactPos, impactNormal;
    int impactPiece = 0;
    if (count) {
        V3 addMove, addTurn;
        int accepted = 0;
        for (int k = 0; k < count; k++) {
            const Contact& ct = contacts[k];
            const V3 d = ct.point - b.pos;
            // ApplyCollisionAlt
            const float along = Dot(ct.normal, b.Speed(d));
            if (along >= 0.0f) continue;
            const V3 comW = b.ComW();
            const float colMass = 1.0f / (Len2(Cross(d - comW, ct.normal)) / b.turnMass + 1.0f / b.mass);
            float impact;
            const float slow = ts * 0.008f * 1.4f;
            if (b.up.z < -0.3f && std::fabs(b.mv.x) < slow && std::fabs(b.mv.y) < slow && slow * 2.0f > std::fabs(b.mv.z))
                impact = -0.95f * colMass * along;   // an upturned wreck settles without bouncing
            else impact = -((kElasticity + 1.0f) * colMass * along);
            V3 move = ct.normal * impact;
            const V3 speed = move / b.mass;
            addMove += p.hitWall ? speed : speed * 1.2f;
            move *= 0.8f;
            addTurn += Cross(d - comW, move) / b.turnMass;
            accepted++;
            if (impact > maxImpact) { maxImpact = impact; impactPos = ct.point; impactNormal = ct.normal; impactPiece = ct.piece; }
            // ApplyFriction with the body's grip on the road, split over the contacts
            float friction = kBodyAdhesion / (float)count;
            if (b.up.z > 0.3f && Len2(b.mv) < 0.02f && Len2(b.tv) < 0.01f) friction = 0.0f;
            else if (!(p.player && Dot(ct.normal, b.up) >= 0.707f)) friction = 150.0f / b.mass * friction * impact;
            const V3 v = b.Speed(d);
            const V3 slide = v - ct.normal * Dot(v, ct.normal);
            const float sl = Len(slide);
            if (sl > 0.0f && friction > 0.0f) {
                const V3 dir = slide / sl;
                float fm = -(1.0f / (Len2(Cross(d - comW, dir)) / b.turnMass + 1.0f / b.mass) * sl);
                fm = std::max(fm, -friction);
                b.ApplyFrictionForce(dir * fm, d);
            }
        }
        if (accepted) {
            b.mv += addMove / (float)accepted;
            b.tv += addTurn / (float)accepted;
            if (p.player && std::fabs(b.mv.x) > 0.2f && std::fabs(b.mv.y) > 0.2f) {
                b.fmv.x -= addMove.x * 0.3f / (float)count;
                b.fmv.y -= addMove.y * 0.3f / (float)count;
                b.ftv -= addTurn * 0.3f / (float)count;
            }
            p.hitWall = true;
        }
        // CPhysical::ProcessShift: lift the body out of what it sank into
        V3 shift;
        for (int k = 0; k < count; k++) {
            const float need = contacts[k].depth - Dot(shift, contacts[k].normal);
            if (need > 0.0f) shift += contacts[k].normal * need;
        }
        b.pos += shift;
    }
    if (maxImpact > p.damageIntensity) {
        p.damageIntensity = maxImpact; p.damager = -1; p.piece = impactPiece;
        impactPos.To(p.impactPos); impactNormal.To(p.impactNormal);
    }
    Store(b, c);
}

void Vehicles::Activate(VehicleInstance& c) {
    CarPhys& p = c.phys;
    if (p.active) return;
    // Hand the car to the simulation: its matrix from the heading, its speed from the arcade model.
    const float cs = std::cos(c.heading), sn = std::sin(c.heading);
    const float r[9] = {cs, sn, 0, -sn, cs, 0, 0, 0, 1};
    std::memcpy(c.rot, r, sizeof(r));
    p = CarPhys{};
    p.mv[0] = c.vx / 50.0f; p.mv[1] = c.vy / 50.0f;
    p.active = true;
}

void Vehicles::SyncFromPhys(VehicleInstance& c, const VehicleModel& vm) {
    const V3 fwd(&c.rot[3]);
    c.heading = std::atan2(-fwd.x, fwd.y);
    c.vx = c.phys.mv[0] * 50.0f;
    c.vy = c.phys.mv[1] * 50.0f;
    c.speed = Dot(V3(c.phys.mv), fwd) * 50.0f;
    c.steer = -c.phys.steerAngle;
    (void)vm;
}

void Vehicles::SetAIControls(size_t i, float steer, float gas, float brake, bool handbrake) {
    if (i >= m_cars.size()) return;
    VehicleInstance& c = m_cars[i];
    Activate(c);
    CarPhys& p = c.phys;
    p.ai = true;
    p.aiSteer = steer; p.aiGas = gas; p.aiBrake = brake; p.aiHandbrake = handbrake;
    p.fakePhysics = 0;   // a driven car never sleeps
}

void Vehicles::EndPlayback(size_t i) {
    if (i >= m_cars.size()) return;
    VehicleInstance& c = m_cars[i];
    if (!c.hasRot) return;
    float rot[9];
    std::memcpy(rot, c.rot, sizeof(rot));
    c.hasRot = false;
    Activate(c);   // (a car new to the simulation takes the heading's matrix; the playback's is kept)
    std::memcpy(c.rot, rot, sizeof(rot));
    c.phys.mv[0] = c.vx / 50.0f; c.phys.mv[1] = c.vy / 50.0f;
    c.phys.fakePhysics = 0;
}

bool Vehicles::Lookup(int model, VehicleModel*& vm, const HandlingData*& h) {
    auto mi = m_models.find(model);
    const VehicleInfo* info = Info(model);
    h = info ? Handling(info->handling) : nullptr;
    if (mi == m_models.end() || !h) return false;
    vm = &mi->second;
    if (!vm->suspReady) { if (UsesBikePhysics(model)) SetupBikeSuspension(*vm, *h); else SetupSuspension(*vm, *h); }
    return true;
}

void Vehicles::StepVehicle(size_t i, float ts, bool player, const VehicleWorld& world) {
    VehicleInstance& c = m_cars[i];
    VehicleModel* vm = nullptr;
    const HandlingData* h = nullptr;
    if (!Lookup(c.model, vm, h)) return;
    if (UsesBikePhysics(c.model)) { BikeStep(c, *vm, *h, ts, player, world); return; }
    if (UsesBoatPhysics(c.model)) { BoatStep(c, *vm, *h, ts, player, world); return; }
    // the car and the chain it tows: every ProcessControl (the links among them) first, then every move, as the
    // world's two passes do
    size_t chain[8];
    VehicleModel* vms[8];
    const HandlingData* hs[8];
    int n = 0;
    chain[0] = i; vms[0] = vm; hs[0] = h; n = 1;
    for (int prev = (int)i, t = c.towing; n < 8 && t >= 0 && (size_t)t < m_cars.size() && m_cars[(size_t)t].towedBy == prev; prev = t, t = m_cars[(size_t)t].towing) {
        if (!Lookup(m_cars[(size_t)t].model, vms[n], hs[n]) || UsesBikePhysics(m_cars[(size_t)t].model) || UsesBoatPhysics(m_cars[(size_t)t].model)) break;
        chain[n++] = (size_t)t;
    }
    for (int k = 0; k < n; k++) CarStep(m_cars[chain[k]], *vms[k], *hs[k], ts, k == 0 && player, world, false);
    for (int k = 0; k < n; k++) CarCollision(m_cars[chain[k]], *vms[k], *hs[k], ts, world);
}

void Vehicles::SyncTowed(size_t i, float dt) {
    for (int k = 0, prev = (int)i, t = m_cars[i].towing; k < 8 && t >= 0 && (size_t)t < m_cars.size() && m_cars[(size_t)t].towedBy == prev; k++) {
        VehicleInstance& tc = m_cars[(size_t)t];
        auto mi = m_models.find(tc.model);
        if (mi == m_models.end()) break;
        SyncFromPhys(tc, mi->second);
        tc.wheelSpin += tc.speed * dt / std::max(0.2f, mi->second.wheelSize[0] / 2.0f);
        prev = t; t = tc.towing;
    }
}

// A towed car is stepped with the car at the head of its chain: true when that one runs this frame (the player's
// car, or an abandoned one still awake).
bool Vehicles::TowedChainStepped(size_t i, int skip) const {
    int head = (int)i;
    for (int k = 0; k < 8 && m_cars[(size_t)head].towedBy >= 0 && (size_t)m_cars[(size_t)head].towedBy < m_cars.size(); k++) head = m_cars[(size_t)head].towedBy;
    if (head == (int)i) return false;
    if (head == skip) return true;
    const VehicleInstance& hc = m_cars[(size_t)head];
    return hc.phys.active && !hc.removed && !hc.hasRot && hc.phys.fakePhysics < 10;
}

bool Vehicles::ProcessCar(size_t i, float dt, float accel, float brake, float steer, bool handbrake, const VehicleWorld& world, float leanFwd,
                          float skid, bool gear, float gunUpDown) {
    if (i >= m_cars.size() || !world.line || !world.sphere) return false;
    VehicleInstance& c = m_cars[i];
    const bool bike = UsesBikePhysics(c.model), boat = UsesBoatPhysics(c.model), heli = UsesHeliPhysics(c.model), plane = UsesPlanePhysics(c.model);
    if (!bike && !boat && !heli && !plane && !c.derailed && !UsesCarPhysics(c.model)) return false;
    if (c.derailed) { accel = brake = steer = leanFwd = skid = 0.0f; handbrake = gear = false; }   // nobody drives it now
    auto mi = m_models.find(c.model);
    const VehicleInfo* info = Info(c.model);
    const HandlingData* h = info ? Handling(info->handling) : nullptr;
    if (mi == m_models.end() || !h) return false;
    if (c.hasRot) return true;   // a recorded playback owns the car
    VehicleModel& vm = mi->second;
    if (!vm.suspReady) { if (bike) SetupBikeSuspension(vm, *h); else SetupSuspension(vm, *h); }
    CarPhys& p = c.phys;
    Activate(c);
    if (c.towedBy >= 0) BreakTowLink(i);   // CAutomobile::ProcessControl: a driver at the wheel (STATUS_PLAYER) ends the tow
    p.inAccel = accel; p.inBrake = brake; p.inSteer = steer; p.handbrake = handbrake; p.inLeanFwd = leanFwd; p.inSkid = skid; p.inGear = gear;
    p.inGunUpDown = gunUpDown;
    p.fakePhysics = 0;
    const float total = dt * 50.0f;
    const int steps = std::clamp((int)std::ceil(total), 1, 8);
    if (boat) BoatInputs(c, *h, total, true);
    if (heli) HeliInputs(c, true);
    if (plane) PlaneInputs(c, *h, total, true);
    for (int k = 0; k < steps; k++) StepVehicle(i, total / (float)steps, true, world);
    if (bike && (vm.info.type == "bmx" || vm.info.type == "BMX")) BmxSway(c);
    SyncFromPhys(c, vm);
    c.wheelSpin += c.speed * dt / std::max(0.2f, vm.wheelSize[0] / 2.0f);
    SyncTowed(i, dt);
    return true;
}

void Vehicles::ProcessAbandoned(float dt, int skip, const VehicleWorld& world) {
    if (!world.line || !world.sphere) return;
    ScanTrailers(dt * 50.0f);
    for (size_t i = 0; i < m_cars.size(); i++) {
        VehicleInstance& c = m_cars[i];
        CarPhys& p = c.phys;
        if ((int)i == skip || !p.active || c.removed || c.hasRot || p.fakePhysics >= 10) continue;
        if (c.towedBy >= 0 && TowedChainStepped(i, skip)) continue;   // stepped behind its tractor
        auto mi = m_models.find(c.model);
        const VehicleInfo* info = Info(c.model);
        const HandlingData* h = info ? Handling(info->handling) : nullptr;
        if (mi == m_models.end() || !h) continue;
        VehicleModel& vm = mi->second;
        const bool bike = UsesBikePhysics(c.model), boat = UsesBoatPhysics(c.model);
        if (!vm.suspReady) { if (bike) SetupBikeSuspension(vm, *h); else SetupSuspension(vm, *h); }
        const float total = dt * 50.0f;
        const int steps = std::clamp((int)std::ceil(total), 1, 8);
        if (boat) BoatInputs(c, *h, total, false);
        if (UsesHeliPhysics(c.model)) HeliInputs(c, false);
        if (UsesPlanePhysics(c.model)) PlaneInputs(c, *h, total, false);
        for (int k = 0; k < steps; k++) StepVehicle(i, total / (float)steps, false, world);
        // CAutomobile::ProcessControl, STATUS_ABANDONED: once the averaged speeds stay tiny the car sleeps
        for (int a = 0; a < 3; a++) { p.avgForce[a] = (p.avgForce[a] + p.mv[a]) / 2.0f; p.avgTorque[a] = (p.avgTorque[a] + p.tv[a]) / 2.0f; }
        const float fl = 0.003f * total, tl = 0.0009f * total;
        const V3 af(p.avgForce), at(p.avgTorque);
        const float moved = Len(V3(p.mv)) * total;
        if (Len2(af) > fl * fl || Len2(at) > tl * tl || moved >= 0.005f) p.fakePhysics = 0;
        else if (++p.fakePhysics >= 10) {
            p.fakePhysics = 10;
            std::memset(p.mv, 0, sizeof(p.mv)); std::memset(p.tv, 0, sizeof(p.tv));
        }
        SyncFromPhys(c, vm);
        c.wheelSpin += c.speed * dt / std::max(0.2f, vm.wheelSize[0] / 2.0f);
        SyncTowed(i, dt);
    }
}

namespace {


}   // namespace

void Vehicles::CarCarCollisions(float dt) {
    (void)dt;
    struct Entry { size_t i; VehicleModel* vm; const HandlingData* h; V3 pos; bool awake; };
    std::vector<Entry> cars;
    for (size_t i = 0; i < m_cars.size(); i++) {
        const VehicleInstance& c = m_cars[i];
        if (c.removed || c.hasRot || (!UsesCarPhysics(c.model) && !UsesHeliPhysics(c.model) && !UsesPlanePhysics(c.model) && !c.derailed)) continue;
        auto mi = m_models.find(c.model);
        const VehicleInfo* info = Info(c.model);
        const HandlingData* h = info ? Handling(info->handling) : nullptr;
        if (mi == m_models.end() || !h) continue;
        if (!mi->second.suspReady) SetupSuspension(mi->second, *h);
        cars.push_back({i, &mi->second, h, V3(c.x, c.y, c.z), c.phys.active && c.phys.fakePhysics < 10});
    }
    struct Contact { V3 point, normal; float depth; int pieceA, pieceB; };   // the normal points from B to A
    std::vector<Contact> contacts;
    for (size_t a = 0; a < cars.size(); a++)
        for (size_t bI = a + 1; bI < cars.size(); bI++) {
            Entry& ea = cars[a];
            Entry& eb = cars[bI];
            if (!ea.awake && !eb.awake) continue;   // two cars at rest stay at rest
            // a tractor and its trailer overlap at the coupling (CTrailer::ProcessEntityCollision drops the pair's
            // triangles; the bodies here are all spheres and boxes, so the pair is skipped)
            if (m_cars[ea.i].towing == (int)eb.i || m_cars[eb.i].towing == (int)ea.i) continue;
            const float reach = ea.vm->probeRadius + eb.vm->probeRadius;
            if (Len2(ea.pos - eb.pos) > reach * reach) continue;
            VehicleInstance& ca = m_cars[ea.i];
            VehicleInstance& cb = m_cars[eb.i];
            V3 ra, fa, ua, rb, fb, ub;
            CarFrame(ca, ra, fa, ua);
            CarFrame(cb, rb, fb, ub);
            contacts.clear();
            for (const VehicleModel::ColSphere& sa : ea.vm->probes) {
                const V3 pa = ea.pos + ra * sa.c[0] + fa * sa.c[1] + ua * sa.c[2];
                for (const VehicleModel::ColSphere& sb : eb.vm->probes) {
                    const V3 pb = eb.pos + rb * sb.c[0] + fb * sb.c[1] + ub * sb.c[2];
                    const V3 d = pa - pb;
                    const float l2 = Len2(d), rr = sa.r + sb.r;
                    if (l2 >= rr * rr || l2 < 1e-8f) continue;
                    const float l = std::sqrt(l2);
                    const V3 n = d / l;
                    const float depth = rr - l;
                    contacts.push_back({pb + n * (sb.r - depth * 0.5f), n, depth, sa.piece, sb.piece});
                }
            }
            if (contacts.empty()) continue;
            const bool bWasAwake = eb.awake;
            Activate(ca); Activate(cb);
            ca.phys.fakePhysics = 0; cb.phys.fakePhysics = 0;
            for (VehicleInstance* av : {&ca, &cb}) if (av->alarmMs == -1) av->alarmMs = 15000;   // CPhysical::ProcessCollision sets armed alarms off
            ea.awake = eb.awake = true;
            Body A = Load(ca, *ea.h), B = Load(cb, *eb.h);
            const float count = (float)contacts.size();
            float maxA = 0.0f, maxB = 0.0f;
            V3 hitA, hitB, nA0, nB0;
            int pieceA = 0, pieceB = 0;
            for (const Contact& ct : contacts) {
                // CPhysical::ApplyCollision(entity, colPoint, thisDI, entityDI), the free-body branch (mass factors 1)
                V3 dA = ct.point - A.pos, dB = ct.point - B.pos;
                const float vA = Dot(A.Speed(dA), ct.normal), vB = Dot(B.Speed(dB), ct.normal);
                const float mA = 1.0f / (Len2(Cross(dA - A.ComW(), ct.normal)) / A.turnMass + 1.0f / A.mass);
                const float mB = 1.0f / (Len2(Cross(dB - B.ComW(), ct.normal)) / B.turnMass + 1.0f / B.mass);
                const float common = (mB * vB + mA * vA) / (mB + mA);
                if (vA - common >= 0.0f) continue;
                const float tA = ca.phys.hitWall ? common : common - kElasticity * (vA - common);
                const float tB = cb.phys.hitWall ? common : common - (vB - common) * kElasticity;
                const float diA = (tA - vA) * mA;
                const float diB = -((tB - vB) * mB);
                V3 fA = ct.normal * diA, fB = ct.normal * -diB;
                if (!ca.phys.hitWall) {
                    if (ct.normal.z < 0.7f) fA.z *= 0.3f;
                    if (ca.phys.player) dA *= 0.8f;
                }
                if (!cb.phys.hitWall) {
                    if (-ct.normal.z < 0.7f) fB.z *= 0.3f;
                    if (cb.phys.player) dB *= 0.8f;
                }
                A.ApplyForce(fA, dA);
                B.ApplyForce(fB, dB);
                if (diA > maxA) { maxA = diA; hitA = ct.point; nA0 = ct.normal; pieceA = ct.pieceA; }
                // SetDamagedPieceRecord hands the struck car the striker's piece (m_nPieceTypeA); a car that is
                // simulating meets the striker again in its own pass, with its own piece.
                if (diB > maxB) { maxB = diB; hitB = ct.point; nB0 = -ct.normal; pieceB = bWasAwake ? ct.pieceB : ct.pieceA; }
                // ApplyFriction(entity, adhesion / points, colPoint): a moving car's grip scales with the blow
                float friction = kBodyAdhesion / count;
                if (Len2(A.mv) > 0.02f || Len2(A.tv) > 0.01f) friction *= diA;
                dA = ct.point - A.pos; dB = ct.point - B.pos;
                const V3 sA = A.Speed(dA), sB = B.Speed(dB);
                const float nA = Dot(sA, ct.normal), nB = Dot(sB, ct.normal);
                if (0.2f * 0.707f > std::fabs(nA)) friction *= 0.05f;
                const V3 slideA = sA - ct.normal * nA, slideB = sB - ct.normal * nB;
                const float lA = Len(slideA), lB = Len(slideB);
                if (lA <= 0.0f) continue;
                const V3 dir = slideA / lA;
                const float fmA = 1.0f / (Len2(Cross(dA - A.ComW(), dir)) / A.turnMass + 1.0f / A.mass);
                const float fmB = 1.0f / (Len2(Cross(dB - B.ComW(), dir)) / B.turnMass + 1.0f / B.mass);
                const float slide = (lB * fmB + fmA * lA) / (fmB + fmA);
                if (lA <= slide) continue;
                const float jA = std::max(fmA * (slide - lA), -friction);
                const float jB = std::min(fmB * (slide - lB), friction);
                A.ApplyFrictionForce(dir * jA, dA);
                B.ApplyFrictionForce(dir * jB, dB);
            }
            // ProcessShift: part the bodies, the lighter one moving further
            V3 shift;
            for (const Contact& ct : contacts) {
                const float need = ct.depth - Dot(shift, ct.normal);
                if (need > 0.0f) shift += ct.normal * need;
            }
            const float share = B.mass / (A.mass + B.mass);
            A.pos += shift * share;
            B.pos -= shift * (1.0f - share);
            ea.pos = A.pos; eb.pos = B.pos;
            Store(A, ca); Store(B, cb);
            if (maxA > ca.phys.damageIntensity) {
                ca.phys.damageIntensity = maxA; ca.phys.damager = (int)eb.i; ca.phys.piece = pieceA;
                hitA.To(ca.phys.impactPos); nA0.To(ca.phys.impactNormal);
            }
            if (maxB > cb.phys.damageIntensity) {
                cb.phys.damageIntensity = maxB; cb.phys.damager = (int)ea.i; cb.phys.piece = pieceB;
                hitB.To(cb.phys.impactPos); nB0.To(cb.phys.impactNormal);
            }
            SyncFromPhys(ca, *ea.vm);
            SyncFromPhys(cb, *eb.vm);
        }
}

void Vehicles::PedHitReaction(size_t i, const float pedCentre[3], bool overBonnet) {
    if (i >= m_cars.size()) return;
    VehicleInstance& c = m_cars[i];
    const VehicleInfo* info = Info(c.model);
    const HandlingData* h = info ? Handling(info->handling) : nullptr;
    if (!h || !c.phys.active) return;
    // CPed::KillPedWithCar's push back on the car: along the flat line to the ped (dipped 0.2 unless the ped went
    // over the bonnet), scaled by the car's speed that way and its mass up to 1600 kg, at a quarter of the offset.
    Body b = Load(c, *h);
    V3 rel = V3(pedCentre) - b.pos;
    rel -= b.up * Dot(rel, b.up);
    V3 dir = Norm(rel);
    const float along = Dot(dir, b.mv);
    if (!overBonnet) dir.z -= 0.2f;
    const float m = h->mass;
    const float f = std::min(1.0f, m / 1600.0f) * std::min(m, 1600.0f) * along * -0.5f;
    b.ApplyForce(dir * f, rel * 0.25f);
    Store(b, c);
    SyncFromPhys(c, m_models[c.model]);
    // CAutomobile::RemoveBonnetInPedCollision: a bonnet hanging open more than 40% goes with the ped
    const VehicleModel& vm = m_models[c.model];
    if (overBonnet && vm.doors[4].found && (c.doorStatus[0] == 1 || c.doorStatus[0] == 3) && !(vm.doors[4].openAngle * 0.4f >= c.doorAngle[0])) {
        SpawnFlying(i, 0, 2);
        c.doorStatus[0] = 4;
    }
}

bool Vehicles::ColBounds(size_t i, float mn[3], float mx[3]) const {
    if (i >= m_cars.size()) return false;
    auto it = m_models.find(m_cars[i].model);
    if (it == m_models.end()) return false;
    const VehicleModel& vm = it->second;
    if (vm.colFound) { std::memcpy(mn, vm.colMin, sizeof(vm.colMin)); std::memcpy(mx, vm.colMax, sizeof(vm.colMax)); return true; }
    mn[0] = -vm.bbHalfX; mn[1] = vm.bbMinY; mn[2] = vm.bbMinZ;
    mx[0] = vm.bbHalfX; mx[1] = vm.bbMaxY; mx[2] = 1.0f;
    return true;
}

void Vehicles::Frame(size_t i, float r[3], float f[3], float u[3]) const {
    V3 a(1, 0, 0), b(0, 1, 0), c(0, 0, 1);
    if (i < m_cars.size()) CarFrame(m_cars[i], a, b, c);
    r[0] = a.x; r[1] = a.y; r[2] = a.z; f[0] = b.x; f[1] = b.y; f[2] = b.z; u[0] = c.x; u[1] = c.y; u[2] = c.z;
}

bool Vehicles::BurstTyre(size_t i, int wheel) {
    if (i >= m_cars.size() || wheel < 0 || wheel > 3) return false;
    VehicleInstance& c = m_cars[i];
    if (c.model == 432 || c.wheelStatus[wheel] != 0) return false;   // the rhino runs on tracks
    c.wheelStatus[wheel] = 1;
    if (!UsesCarPhysics(c.model)) return true;
    const VehicleInfo* info = Info(c.model);
    const HandlingData* h = info ? Handling(info->handling) : nullptr;
    if (!h) return true;
    Activate(c);
    Body b = Load(c, *h);   // the bang shoves the car sideways and twists it a little
    b.ApplyMoveForce(b.right * (RandRange(-0.03f, 0.03f) * b.mass));
    b.ApplyTurnForce(b.right * (RandRange(-0.03f, 0.03f) * b.turnMass), b.fwd);
    Store(b, c);
    c.phys.fakePhysics = 0;
    return true;
}

int Vehicles::Trace(const float* eye, const float* dir, float reach, float* t, int* tyre) const {
    int best = -1, bestTyre = -1;
    float bt = reach;
    if (tyre) *tyre = -1;
    for (size_t i = 0; i < m_cars.size(); i++) {
        const VehicleInstance& c = m_cars[i];
        if (c.removed) continue;
        auto mi = m_models.find(c.model);
        if (mi == m_models.end()) continue;
        const VehicleModel& vm = mi->second;
        V3 r, f, u;
        CarFrame(c, r, f, u);
        const V3 rel = V3(eye) - V3(c.x, c.y, c.z), dw(dir);
        const float o[3] = {Dot(rel, r), Dot(rel, f), Dot(rel, u)};   // into car space
        const float d[3] = {Dot(dw, r), Dot(dw, f), Dot(dw, u)};
        float lo[3], hi[3];
        if (vm.colFound) for (int k = 0; k < 3; k++) { lo[k] = vm.colMin[k]; hi[k] = vm.colMax[k]; }
        else { lo[0] = -vm.bbHalfX; hi[0] = vm.bbHalfX; lo[1] = vm.bbMinY; hi[1] = vm.bbMaxY; lo[2] = -vm.rideHeight; hi[2] = 1.5f - vm.rideHeight; }
        float t0 = 0.0f, t1 = bt;
        bool ok = true;
        for (int k = 0; k < 3 && ok; k++) {
            if (std::fabs(d[k]) < 1e-7f) { if (o[k] < lo[k] || o[k] > hi[k]) ok = false; continue; }
            float a = (lo[k] - o[k]) / d[k], b = (hi[k] - o[k]) / d[k];
            if (a > b) std::swap(a, b);
            t0 = std::max(t0, a); t1 = std::min(t1, b);
            if (t0 > t1) ok = false;
        }
        if (ok && t0 < bt) { bt = t0; best = (int)i; bestTyre = -1; }
        // CWorld::bIncludeCarTyres: CAutomobile::SetUpWheelColModel puts a sphere of the tyre size on each wheel
        if (!tyre || c.model == 531 || c.model == 532 || c.model == 571) continue;   // tractor, combine, kart
        static const int kDummy[4] = {0, 2, 1, 3};   // eCarWheel -> lf, rf, lb, rb dummy
        for (int w = 0; w < 4; w++) {
            const int k = kDummy[w];
            if (!vm.wheelFound[k] || c.wheelStatus[w] == 2) continue;
            const float wc[3] = {vm.wheelPos[k][0], vm.wheelPos[k][1], c.phys.active ? c.phys.wheelZ[w] : vm.wheelPos[k][2]};
            const float rad = vm.wheelSize[w] / 2.0f;
            const float oc[3] = {o[0] - wc[0], o[1] - wc[1], o[2] - wc[2]};
            const float bq = oc[0] * d[0] + oc[1] * d[1] + oc[2] * d[2];
            const float cq = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - rad * rad;
            const float disc = bq * bq - cq;
            if (disc < 0.0f) continue;
            const float tw = -bq - std::sqrt(disc);
            if (tw < 0.0f || tw >= reach) continue;
            bool take = tw < bt;
            if (!take && (int)i == best && bestTyre < 0) {
                // A shot from the far side through the body still finds the tyre when it crosses steeply enough.
                const float hitSide = o[0] + d[0] * tw;
                take = ((d[0] < 0.0f && hitSide > 0.0f) || (d[0] > 0.0f && hitSide < 0.0f)) && std::fabs(d[0]) > 0.5f;
            }
            if (take) { bt = tw; best = (int)i; bestTyre = w; }
        }
    }
    if (best >= 0 && t) *t = bt;
    if (tyre) *tyre = bestTyre;
    return best;
}

void Vehicles::SpawnFlying(size_t ci, int comp, int type, int wheel) {
    if (ci >= m_cars.size() || m_flying.size() >= 150) return;   // CObject::nNoTempObjects
    const VehicleInstance& c = m_cars[ci];
    auto it = m_models.find(c.model);
    if (it == m_models.end()) return;
    const VehicleModel& vm = it->second;
    FlyingPart fp;
    fp.model = c.model;
    std::memcpy(fp.paint, c.paint, sizeof(fp.paint));
    // The component's frame in car space: a hinge, a wheel, or the part's own frame
    float pivot[3] = {0, 0, 0};
    if (comp == 13) {
        static const int kDummy[4] = {0, 2, 1, 3};   // eCarWheel -> lf, rf, lb, rb dummy
        const int k = kDummy[wheel & 3];
        if (!vm.wheelMesh || !vm.wheelFound[k]) return;
        pivot[0] = vm.wheelPos[k][0]; pivot[1] = vm.wheelPos[k][1];
        pivot[2] = c.phys.active && vm.suspReady ? c.phys.wheelZ[wheel & 3] : vm.wheelPos[k][2];
        const float side = k % 2 == 0 ? -1.0f : 1.0f;
        FlyingPart::Piece pc{vm.wheelMesh, {side, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
        fp.pieces.push_back(pc);
    } else {
        const int slot = comp < 6 ? (comp == 0 ? 4 : comp == 1 ? 5 : comp - 2) : -1;
        bool havePivot = false;
        if (slot >= 0 && vm.doors[slot].found) { std::memcpy(pivot, vm.doors[slot].pivot, sizeof(pivot)); havePivot = true; }
        for (const VehiclePart& p : vm.parts) {   // GetCurrentAtomicObjectCB: the mesh shown now
            if (p.comp != comp || !PartVisible(c, p)) continue;
            FlyingPart::Piece pc;
            pc.mesh = p.mesh;
            PartToCar(c, vm, p, pc.m);
            if (!havePivot) { pivot[0] = pc.m[12]; pivot[1] = pc.m[13]; pivot[2] = pc.m[14]; havePivot = true; }
            fp.pieces.push_back(pc);
        }
        if (fp.pieces.empty()) return;
    }
    for (FlyingPart::Piece& pc : fp.pieces) { pc.m[12] -= pivot[0]; pc.m[13] -= pivot[1]; pc.m[14] -= pivot[2]; }
    V3 r, f, u;
    CarFrame(c, r, f, u);
    r.To(&fp.rot[0]); f.To(&fp.rot[3]); u.To(&fp.rot[6]);
    const V3 carPos(c.x, c.y, c.z);
    V3 pos = carPos + r * pivot[0] + f * pivot[1] + u * pivot[2];
    // The object's centre of mass for its collision type
    if (type == 0) fp.com[0] = -pivot[0];
    else if (type == 2) fp.com[1] = -0.5f;
    else if (type == 3) fp.com[1] = 0.4f;
    else if (type == 4) fp.com[1] = -0.3f;
    const size_t n = m_flying.size() + 1;
    fp.life = n <= 10 ? 20.0f : n <= 20 ? 10.0f : 4.0f;   // m_nRemovalTime
    const V3 cmv = c.phys.active ? V3(c.phys.mv) : V3(c.vx / 50.0f, c.vy / 50.0f, 0.0f);
    float mz;
    if (cmv.z > 0.0f) mz = cmv.z * 1.5f;
    else if (u.z < 0.0f && (type == 3 || type == 4)) mz = 0.04f - cmv.z * 1.5f;
    else mz = cmv.z / 4.0f;
    V3 mv(cmv.x * 0.75f, cmv.y * 0.75f, mz);
    const V3 tv = c.phys.active ? V3(c.phys.tv) * 2.0f : V3();
    V3 dir = Norm(pos - carPos);
    if (type == 3 || type == 4) {
        dir += u;
        if (u.z > 0.0f) pos += u * std::sqrt(cmv.x * cmv.x + cmv.y * cmv.y);
    }
    mv += dir / fp.mass;   // ApplyMoveForce(objDir)
    V3 tvv = tv;
    if (type == 1) { fp.turnMass = 5.0f; tvv.x = 0.5f; fp.air = 0.99f; }
    pos.To(fp.pos); mv.To(fp.mv); tvv.To(fp.tv);
    m_flying.push_back(std::move(fp));
}

void Vehicles::BlowUp(size_t ci) {
    if (ci >= m_cars.size()) return;
    VehicleInstance& c = m_cars[ci];
    if (UsesCarPhysics(c.model)) { Activate(c); c.phys.mv[2] += 0.13f; c.phys.fakePhysics = 0; }   // the blast lifts it
    auto it = m_models.find(c.model);
    if (it == m_models.end()) return;
    const VehicleModel& vm = it->second;
    // CDamageManager::FuckCarCompletely: a wheel comes off, every door, the bonnet and the boot go. (Its panel
    // reset to intact is not drawn: SetBumperDamage leaves the bumpers looking as they were.)
    const int wheel = Random(4);
    const bool flies = c.model != 564 && c.model != 441;   // not the RC tiger or RC bandit
    for (int d = 0; d < 6; d++) {
        const int slot = d == 0 ? 4 : d == 1 ? 5 : d - 2;
        if (flies && vm.doors[slot].found && c.doorStatus[d] != 4) SpawnFlying(ci, d, d == 0 ? 3 : d == 1 ? 4 : 2);
        c.doorStatus[d] = 4;
    }
    for (int k = 0; k < 4; k++) c.lightStatus[k] = 0;
    // BlowUpCar throws CAR_WHEEL_LF; the wheel thrown here is the one FuckCarCompletely took off, so it is not drawn twice.
    if (flies && c.wheelStatus[wheel] != 2) SpawnFlying(ci, 13, 1, wheel);
    c.wheelStatus[wheel] = 2;
}

namespace {

// CDoor::Process: the door leaf swings with the jolt its hinge feels this frame, damped, bounced off its stops.
// Returns true when a latching door slams shut.
bool DoorProcess(const VehicleModel::Door& d, float& angle, float& angVel, uint8_t& hit, const Body& b, const V3& oldMv,
                 const V3& oldTv, const V3& offset, float ts) {
    V3 acc = b.Speed(offset) - (Cross(oldTv, offset) + oldMv);
    acc.z += ts * (d.lowGravity ? 0.0016f : 0.008f);
    const float al[3] = {Dot(acc, b.right), Dot(acc, b.fwd), Dot(acc, b.up)};   // into car space
    float dir[3] = {0, 0, 0};
    dir[d.dirn % 3] = d.dirn < 3 ? 1.0f : -1.0f;
    const float s = std::sin(angle), cs = std::cos(angle);
    const int a = (d.axis + 1) % 3, bi = (d.axis + 2) % 3;
    const float na = cs * dir[a] - s * dir[bi], nb = s * dir[a] + cs * dir[bi];   // the leaf turned about the axis
    dir[a] = na; dir[bi] = nb;
    const float cr[3] = {al[1] * dir[2] - al[2] * dir[1], al[2] * dir[0] - al[0] * dir[2], al[0] * dir[1] - al[1] * dir[0]};
    const float ang = cr[d.axis];
    if (std::fabs(ang) > 0.001f || std::fabs(angVel) > 0.001f) angVel += ang;   // DOOR_SPEED_MIN_LIMIT
    angVel *= 0.97f;                                                            // DOOR_DAMP_RATE (and _BONNET)
    angVel = std::clamp(angVel, -0.5f, 0.5f);                                   // DOOR_SPEED_MAX_CAPPED
    angle += angVel;
    hit = 0;
    const bool posOpen = d.openAngle >= 0.0f;
    const bool hitOpen = posOpen ? angle > d.openAngle : angle < d.openAngle;
    const bool hitClosed = posOpen ? angle < 0.0f : angle > 0.0f;
    if (hitOpen) { angVel *= -0.8f; angle = d.openAngle; hit = 1; }
    else if (hitClosed) {
        angle = 0.0f;
        if (d.based && std::fabs(angVel) > 0.1f) { angVel = 0.0f; hit = 4; return true; }   // DOOR_SLAM_SHUT_SPEED
        angVel *= d.lowGravity ? -0.9f : -0.4f;
        hit = 2;
    }
    return false;
}

// CDoor::ProcessImpact: a jolt across the leaf past 0.1 (x 0.75..1.5) pops a shut door open.
bool DoorImpact(const VehicleModel::Door& d, const Body& b, const V3& oldMv, const V3& oldTv, const V3& offset) {
    const V3 acc = b.Speed(offset) - (Cross(oldTv, offset) + oldMv);
    const V3 axes[3] = {b.right, b.fwd, b.up};
    const V3 dir = d.dirn < 3 ? axes[d.dirn] : -axes[d.dirn - 3];
    const V3 cr = Cross(acc, dir);
    const float ang = d.axis == 0 ? cr.x : d.axis == 1 ? cr.y : cr.z;
    const float limit = RandRange(0.75f, 1.5f) * 0.1f;   // POP_OPEN_DOOR_ACCEL_LIMIT
    return d.openAngle >= 0.0f ? ang > limit : ang < -limit;
}

}   // namespace

void Vehicles::ProcessDoors(float dt) {
    const float ts = dt * 50.0f;
    for (size_t i = 0; i < m_cars.size(); i++) {
        VehicleInstance& c = m_cars[i];
        CarPhys& p = c.phys;
        if (c.removed || !p.active) continue;
        auto mi = m_models.find(c.model);
        const VehicleInfo* info = Info(c.model);
        const HandlingData* h = info ? Handling(info->handling) : nullptr;
        if (mi == m_models.end() || !h) continue;
        const VehicleModel& vm = mi->second;
        const Body b = Load(c, *h);
        const V3 oldMv(p.oldMv), oldTv(p.oldTv);
        const bool awake = p.player || p.fakePhysics < 10;
        for (int d = 0; d < 6; d++) {   // CAutomobile::ProcessSwingingDoor
            const int slot = d == 0 ? 4 : d == 1 ? 5 : d - 2;
            const VehicleModel::Door& door = vm.doors[slot];
            uint8_t& st = c.doorStatus[d];
            if (!door.found || st == 4) continue;
            bool open = st == 1 || st == 3;
            if (!open && (p.lastDI <= 100.0f || d == 0 || !awake)) continue;
            const V3 offset = b.Rot(V3(door.pivot));
            if (!open && DoorImpact(door, b, oldMv, oldTv, offset)) {   // SetDoorOpen
                st = st == 0 ? 1 : 3;
                open = true;
                c.doorAngle[d] = slot < 4 ? c.doorRatio[slot] * door.openAngle : 0.0f;
                c.doorAngVel[d] = 0.0f;
            }
            if (!open) continue;
            if (d == 0 && door.dirn == 1)   // a bonnet that opens forwards catches the wind
                c.doorAngVel[d] += std::sin(c.doorAngle[d] + 0.1f) * 0.05f * Dot(b.fwd, b.mv);
            if (DoorProcess(door, c.doorAngle[d], c.doorAngVel[d], c.doorHit[d], b, oldMv, oldTv, offset, ts)) {
                st = st == 1 ? 0 : 2;   // SetDoorClosed
                c.doorAngle[d] = 0.0f;
                if (slot < 4) c.doorRatio[slot] = 0.0f;
            }
            if (d == 0 && c.doorHit[0] == 1 && Dot(b.mv, b.fwd) > 0.4f) {   // flung fully open at speed: it tears off
                SpawnFlying(i, 0, 2);
                if (!m_flying.empty() && m_flying.back().model == c.model) {
                    FlyingPart& fp = m_flying.back();
                    V3 mv = b.mv * 0.4f + b.right * 0.1f;
                    mv += Random(2) ? b.up * 0.5f : b.up * -0.5f;
                    mv.To(fp.mv);
                    const V3 tv = Cross(b.fwd - V3(fp.com), b.up * 10.0f) / fp.turnMass;   // ApplyTurnForce(up * 10, forward)
                    (V3(fp.tv) + tv).To(fp.tv);
                }
                st = 4;
            }
        }
        for (VehicleInstance::BouncingPanel& bp : c.panels) {   // CBouncingPanel::ProcessPanel
            if (bp.bumper < 0 || !vm.bumpers[bp.bumper].found) continue;
            const V3 pos = b.Rot(V3(vm.bumpers[bp.bumper].pos));
            const V3 w = b.Speed(pos) - (oldMv + Cross(oldTv, pos));
            const float av[3] = {Dot(w, b.right), Dot(w, b.fwd), Dot(w, b.up)};
            auto change = [&](float v) { return std::clamp(v, -0.1f, 0.1f) * bp.limit; };   // BOUNCE_VEL_CHANGE_LIMIT
            if (bp.axis == 0) { bp.vel[1] += change(av[2]); bp.vel[2] += change(av[1]); }
            else if (bp.axis == 1) { bp.vel[0] += change(av[2]); bp.vel[2] += change(av[1]); }
            else { bp.vel[0] += change(av[1]); bp.vel[1] += change(av[0]); }
            const bool hanging = bp.axis == 2;
            const float ret = hanging ? 0.02f : 0.1f, damp = std::pow(hanging ? 0.98f : 0.95f, ts);   // RETURN / DAMP_MULT
            for (int k = 0; k < 3; k++) {
                bp.vel[k] -= ret * bp.rot[k] * ts;
                bp.vel[k] *= damp;
                bp.rot[k] += bp.vel[k] * ts;
            }
        }
        std::memcpy(p.oldMv, p.mv, sizeof(p.oldMv));
        std::memcpy(p.oldTv, p.tv, sizeof(p.oldTv));
    }
}

void Vehicles::UpdateFlying(float dt, const VehicleWorld& world) {
    const float total = dt * 50.0f;
    const int steps = std::clamp((int)std::ceil(total), 1, 8);
    const float ts = total / (float)steps;
    for (size_t k = 0; k < m_flying.size();) {
        FlyingPart& fp = m_flying[k];
        fp.life -= dt;
        if (fp.life <= 0.0f) { m_flying.erase(m_flying.begin() + (long)k); continue; }
        V3 r(&fp.rot[0]), f(&fp.rot[3]), u(&fp.rot[6]), pos(fp.pos), mv(fp.mv), tv(fp.tv);
        for (int s = 0; s < steps; s++) {
            mv.z -= ts * 0.008f;                                   // ApplyGravity
            const float air = std::pow(fp.air, ts);                // ApplyAirResistance
            mv *= air; tv *= air;
            pos += mv * ts;
            const V3 dr = tv * ts;
            r += Cross(dr, r); f += Cross(dr, f); u += Cross(dr, u);
            r = Norm(Cross(f, u)); f = Norm(Cross(u, r)); u = Cross(r, f);
            if (!world.sphere) continue;
            const V3 centre = pos + r * fp.com[0] + f * fp.com[1] + u * fp.com[2];
            const float rad = 0.25f;
            float pt[3], nm[3];
            if (!world.sphere(&centre.x, rad, pt, nm)) continue;
            const V3 n(nm);
            const float depth = rad - Len(centre - V3(pt));
            if (depth > 0.0f) pos += n * depth;
            const float vn = Dot(mv, n);
            if (vn < 0.0f) {   // elasticity 0.1, and the ground drags it to a stop
                mv -= n * (vn * 1.1f);
                const V3 slide = mv - n * Dot(mv, n);
                const float sl = Len(slide), drop = std::min(sl, 0.01f * ts);
                if (sl > 0.0f) mv -= slide * (drop / sl);
                tv *= std::pow(0.8f, ts);
            }
        }
        r.To(&fp.rot[0]); f.To(&fp.rot[3]); u.To(&fp.rot[6]); pos.To(fp.pos); mv.To(fp.mv); tv.To(fp.tv);
        k++;
    }
}

void Vehicles::WindscreenShatters(size_t ci) {
    if (ci >= m_cars.size() || !onWindscreenShatter) return;
    const VehicleInstance& c = m_cars[ci];
    auto it = m_models.find(c.model);
    if (it == m_models.end() || !it->second.glassFound) return;
    const VehicleModel& vm = it->second;
    V3 r(1, 0, 0), f(0, 1, 0), u(0, 0, 1);
    if (c.phys.active || c.hasRot) { r = V3(&c.rot[0]); f = V3(&c.rot[3]); u = V3(&c.rot[6]); }
    else { const float cs = std::cos(c.heading), sn = std::sin(c.heading); r = V3(cs, sn, 0); f = V3(-sn, cs, 0); }
    const V3 pos(c.x, c.y, c.z);
    V3 tv[6];
    for (int k = 0; k < 6; k++) { const V3 l(vm.glassTri[k]); tv[k] = pos + r * l.x + f * l.y + u * l.z; }
    // the first face's plane normal, then the glass's own up and across directions
    const V3 l0(vm.glassTri[0]), l1(vm.glassTri[1]), l2(vm.glassTri[2]);
    const V3 nl = Norm(Cross(l2 - l0, l1 - l0));
    const V3 n = r * nl.x + f * nl.y + u * nl.z;
    const V3 gf = Norm(Cross(r, n)), gr = Norm(Cross(u, n));
    float rd[6], fd[6], maxR = -1e30f, maxF = -1e30f, minSum = 1e30f;
    int bl = 0;
    for (int k = 0; k < 6; k++) {
        rd[k] = Dot(tv[k], gr); fd[k] = Dot(tv[k], gf);
        maxR = std::max(maxR, rd[k]); maxF = std::max(maxF, fd[k]);
        if (rd[k] + fd[k] < minSum) { minSum = rd[k] + fd[k]; bl = k; }
    }
    const V3 fwd = gf * (maxF - fd[bl]), right = gr * (maxR - rd[bl]);
    const V3 centre = tv[bl] + fwd * 0.5f + right * 0.5f;
    onWindscreenShatter(&tv[bl].x, &fwd.x, &right.x, c.phys.mv, &centre.x);
}

int Vehicles::Random(int n) {
    m_rng = m_rng * 1664525u + 1013904223u;
    return (int)((m_rng >> 8) % (unsigned)n);
}

// CDamageManager::ApplyDamage(component, intensity): past 150 (after the group's scale) the part gets one step
// worse: doors, bonnet and boot go bashed, then bashed and loose, then (1 in 9) fall off; panels go loose, bashed,
// then (1 in 9, windscreen 1 in 3) come away; a wing hit also smashes that light. comp is tComponent (5..17).
void Vehicles::ApplyComponentDamage(VehicleInstance& c, int comp, float intensity) {
    static const float kGroupMult[6] = {2.5f, 1.25f, 3.2f, 1.4f, 2.5f, 2.8f};   // panel, wheel, door, bonnet, boot, light
    int group, rel;
    if (comp == 5) { group = 3; rel = 0; }
    else if (comp == 6) { group = 4; rel = 1; }
    else if (comp >= 7 && comp <= 10) { group = 2; rel = comp - 5; }
    else if (comp >= 11 && comp <= 14) { group = 5; rel = comp - 11; }
    else if (comp >= 15 && comp <= 17) { group = 0; rel = comp - 11; }
    else return;
    intensity *= kGroupMult[group];
    if (comp == 15) intensity *= 0.6f;
    if (intensity <= 150.0f) return;
    auto progressPanel = [&](int panel) {
        uint8_t& st = c.panelStatus[panel];
        if (st == 2) {
            if (Random(panel == 4 ? 3 : 9) != 0) return;
            // SetBumperDamage / SetPanelDamage: a bumper or wing comes away (the windscreen shatters instead)
            if (panel != 4) SpawnFlying((size_t)(&c - m_cars.data()), 6 + panel, panel >= 5 ? 0 : 5);
            else WindscreenShatters((size_t)(&c - m_cars.data()));
            st = 3;
        } else if (st < 3) {
            st++;
            // SetBumperDamage: a bumper gone loose bounces (CheckIfExistsGetFree), unless the model says not to
            const VehicleInfo* vi = Info(c.model);
            const HandlingData* hd = vi ? Handling(vi->handling) : nullptr;
            if (st == 1 && panel >= 5 && !(hd && (hd->modelFlags & 0x10000000u))) {   // BOUNCE_PANELS
                for (VehicleInstance::BouncingPanel& bp : c.panels) {
                    if (bp.bumper == panel - 5) break;
                    if (bp.bumper >= 0) continue;
                    bp = VehicleInstance::BouncingPanel();
                    bp.bumper = panel - 5;
                    bp.axis = 0;
                    bp.limit = RandRange(-0.5f, -0.2f);
                    break;
                }
            }
        }
    };
    const size_t ci = (size_t)(&c - m_cars.data());
    if (group == 2 || group == 3 || group == 4) {   // ProgressDoorDamage, then SetDoorDamage
        uint8_t& st = c.doorStatus[rel];
        if (st == 0 || st == 1) {   // slammed shut
            st = 2;
            if (rel >= 2) c.doorRatio[rel - 2] = 0.0f;
            c.doorAngle[rel] = c.doorAngVel[rel] = 0.0f;
        } else if (st == 2) st = 3;
        else if (st == 3 && Random(9) == 0) {
            SpawnFlying(ci, rel, rel == 0 ? 3 : rel == 1 ? 4 : 2);
            st = 4;
        }
    } else if (group == 5) {
        c.lightStatus[rel] = 1;
        progressPanel(rel);
    } else progressPanel(rel);
}

// CAutomobile::VehicleDamage's part damage: the struck piece takes four times the knock; a front bumper hit
// also reaches the wings, then the bonnet and windscreen once the bumper is gone soft, a rear one the boot.
void Vehicles::DamageComponents(VehicleInstance& c, const HandlingData& h, float di) {
    const float hit = di * 4.0f;
    const V3 right(&c.rot[0]);
    switch (c.phys.piece) {
    case 1: ApplyComponentDamage(c, 5, hit); break;
    case 2: ApplyComponentDamage(c, 6, hit); break;
    case 3: {
        ApplyComponentDamage(c, 16, hit);
        auto it = m_models.find(c.model);
        const float halfW = it != m_models.end() && it->second.colMax[0] > 0.0f ? it->second.colMax[0] : 1.0f;
        const float localDir = Dot(V3(c.phys.impactPos) - V3(c.x, c.y, c.z), right) / halfW;
        const float dirRight = Dot(V3(c.phys.impactNormal), right);
        if (localDir > 0.7f || (localDir > 0.5f && dirRight < -0.5f && h.mass * 0.35f < di)) ApplyComponentDamage(c, 12, hit);
        else if (localDir > -0.7f || (localDir > -0.5f && dirRight > 0.5f && h.mass * 0.35f < di)) ApplyComponentDamage(c, 11, hit);
        if (c.panelStatus[5] != 0 && h.mass * 0.2f < di) ApplyComponentDamage(c, 5, hit);
        if (c.panelStatus[5] >= 2 && h.mass * 0.2f < di) ApplyComponentDamage(c, 15, hit);
        break;
    }
    case 4:
        ApplyComponentDamage(c, 17, hit);
        if (c.panelStatus[6] < 2) ApplyComponentDamage(c, 6, hit);
        break;
    case 5: case 6: case 7: case 8: ApplyComponentDamage(c, c.phys.piece + 2, hit); break;
    case 9: case 10: case 11: case 12: ApplyComponentDamage(c, c.phys.piece + 2, hit); break;
    case 19: ApplyComponentDamage(c, 15, hit); break;
    default: break;
    }
}

void Vehicles::CollisionDamage(float dt, int playerCar) {
    for (size_t i = 0; i < m_cars.size(); i++) {
        VehicleInstance& c = m_cars[i];
        CarPhys& p = c.phys;
        if (!p.active || c.removed) continue;
        const VehicleInfo* info = Info(c.model);
        const HandlingData* h = info ? Handling(info->handling) : nullptr;
        const float di = p.damageIntensity;
        p.damageIntensity = 0.0f;   // taken once per frame
        p.lastDI = di;
        if (!h || c.wrecked) continue;
        // CAutomobile::VehicleDamage(0, ...): the frame's strongest knock
        if (c.rot[8] < 0.0f && (int)i != playerCar) c.health = std::max(0.0f, c.health - dt * 50.0f * 4.0f);   // lying on its roof
        if (di == 0.0f) continue;
        if (UsesBikePhysics(c.model)) { BikeDamage(c, *h, di, (int)i == playerCar); continue; }
        if (UsesBoatPhysics(c.model)) { BoatDamage(c, *h, di, (int)i == playerCar); continue; }
        const float minDi = h->mass / 1500.0f * 25.0f;
        const V3 up(&c.rot[6]);
        if (p.damager < 0 && Dot(V3(p.impactNormal), up) > 0.6f) continue;   // landing on the ground
        if (di <= minDi) continue;
        if (up.z > 0.0f || Len2(V3(p.mv)) > 0.3f) DamageComponents(c, *h, di);
        const float loss = (di - minDi) * h->collisionDamageGame * 0.6f;
        if (loss <= 0.0f) continue;
        if ((int)i == playerCar) c.health -= loss / 2.0f;
        else if (p.damager >= 0 && p.damager == playerCar) c.health -= loss / 1.5f;
        else c.health -= loss / 4.0f;
        if (c.health < 0.0f) c.health = 0.0f;
    }
}

int Vehicles::PushPedOut(float* x, float* y, float z, float radius, float height, float* pedDI, int* hitCar) const {
    int touched = -1;
    if (pedDI) *pedDI = 0.0f;
    for (size_t i = 0; i < m_cars.size(); i++) {
        const VehicleInstance& c = m_cars[i];
        if (c.removed) continue;
        auto mi = m_models.find(c.model);
        if (mi == m_models.end() || !mi->second.suspReady) continue;
        const VehicleModel& vm = mi->second;
        const float dx0 = *x - c.x, dy0 = *y - c.y, reach = vm.probeRadius + radius;
        if (dx0 * dx0 + dy0 * dy0 > reach * reach) continue;
        V3 r, f, u;
        CarFrame(c, r, f, u);
        const V3 o(c.x, c.y, c.z);
        for (const VehicleModel::ColSphere& s : vm.probes) {
            const V3 p = o + r * s.c[0] + f * s.c[1] + u * s.c[2];
            if (p.z + s.r < z + 0.3f || p.z - s.r > z + height) continue;   // stepped over, or above the head
            // The sphere's slice at the nearest height of the ped's column
            const float hz = std::clamp(p.z, z, z + height) - p.z;
            const float sr2 = s.r * s.r - hz * hz;
            if (sr2 <= 0.0f) continue;
            const float sr = std::sqrt(sr2);
            const float dx = *x - p.x, dy = *y - p.y, d2 = dx * dx + dy * dy, rr = sr + radius;
            if (d2 >= rr * rr) continue;
            const float d = std::sqrt(d2);
            const float nx = d > 1e-4f ? dx / d : 1.0f, ny = d > 1e-4f ? dy / d : 0.0f;
            *x += nx * (rr - d); *y += ny * (rr - d);
            touched = (int)i;
            const VehicleInfo* info = pedDI ? Info(c.model) : nullptr;
            const HandlingData* h = info ? Handling(info->handling) : nullptr;
            if (!h || !c.phys.active) continue;
            // CPhysical::ApplyCollision, car against ped: the car counts ten times its mass (fThisMassFactor 10)
            const Body b = Load(c, *h);
            const V3 n(-nx, -ny, 0.0f);   // out of the ped, into the car
            const V3 dp = V3(p.x + nx * sr, p.y + ny * sr, p.z + hz) - b.pos;
            const float vCar = Dot(b.Speed(dp), n);
            const float mCar = 1.0f / (Len2(Cross(dp - b.ComW(), n)) / (10.0f * b.turnMass) + 1.0f / (10.0f * b.mass));
            const float mPed = 70.0f;
            const float common = mCar * vCar / (mPed + mCar);
            if (vCar - common >= 0.0f) continue;
            const float di = -((common + common * kElasticity) * mPed);   // ped target speed common - (0 - common) e
            if (di > *pedDI) { *pedDI = di; if (hitCar) *hitCar = (int)i; }
        }
    }
    return touched;
}
