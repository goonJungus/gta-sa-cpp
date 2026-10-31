// CAutomobile physics: springs, tyres, gearbox and the body's contacts with the world.
//
// Ported from CAutomobile::ProcessControl / ProcessControlInputs / ProcessSuspension / ProcessCarWheelPair,
// CVehicle::ProcessWheel, cTransmission and CPhysical (ApplyForce, ApplySpringCollisionAlt, ApplySpringDampening,
// ApplyCollisionAlt, ApplyFriction, ApplySpeed). Like retail it works in game units: speeds per 1/50 s and a
// time step counted in 1/50 s. Surfaces are taken as road (the world COL carries no materials yet).
#define NOMINMAX
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

struct V3 {
    float x = 0, y = 0, z = 0;
    V3() = default;
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    explicit V3(const float* p) : x(p[0]), y(p[1]), z(p[2]) {}
    V3 operator+(const V3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(const V3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator-() const { return {-x, -y, -z}; }
    V3 operator*(float s) const { return {x * s, y * s, z * s}; }
    V3 operator/(float s) const { return {x / s, y / s, z / s}; }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(const V3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    void To(float* p) const { p[0] = x; p[1] = y; p[2] = z; }
};
V3 operator*(float s, const V3& v) { return v * s; }
float Dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 Cross(const V3& a, const V3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float Len2(const V3& v) { return Dot(v, v); }
float Len(const V3& v) { return std::sqrt(Dot(v, v)); }
V3 Norm(const V3& v) { const float l = Len(v); return l > 1e-9f ? v / l : V3(1, 0, 0); }

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

// The car as CPhysical sees it for one step: its matrix, the speeds, mass and centre of mass.
struct Body {
    V3 right, fwd, up, pos;
    V3 mv, tv, fmv, ftv;
    float mass = 1500.0f, turnMass = 3000.0f;
    V3 com;   // car space
    V3 Rot(const V3& l) const { return right * l.x + fwd * l.y + up * l.z; }   // CMatrix::TransformVector
    V3 ComW() const { return Rot(com); }
    // Points are relative to the car's position, as retail passes them.
    void ApplyForce(const V3& f, const V3& p) { mv += f / mass; tv += Cross(p - ComW(), f) / turnMass; }
    void ApplyMoveForce(const V3& f) { mv += f / mass; }
    void ApplyTurnForce(const V3& f, const V3& p) { tv += Cross(p - ComW(), f) / turnMass; }
    void ApplyFrictionForce(const V3& f, const V3& p) { fmv += f / mass; ftv += Cross(p - ComW(), f) / turnMass; }
    V3 Speed(const V3& p) const { return Cross(tv + ftv, p - ComW()) + mv + fmv; }   // CPhysical::GetSpeed
    float MassAt(const V3& p, const V3& dir) const { return 1.0f / (Len2(Cross(p, dir)) / turnMass + 1.0f / mass); }   // GetMass(pos, dir)
};

Body Load(const VehicleInstance& c, const HandlingData& h) {
    Body b;
    b.right = V3(&c.rot[0]); b.fwd = V3(&c.rot[3]); b.up = V3(&c.rot[6]);
    b.pos = V3(c.x, c.y, c.z);
    b.mv = V3(c.phys.mv); b.tv = V3(c.phys.tv); b.fmv = V3(c.phys.fmv); b.ftv = V3(c.phys.ftv);
    b.mass = h.mass; b.turnMass = h.turnMass;
    b.com = V3(h.com);
    return b;
}

void Store(const Body& b, VehicleInstance& c) {
    b.right.To(&c.rot[0]); b.fwd.To(&c.rot[3]); b.up.To(&c.rot[6]);
    c.x = b.pos.x; c.y = b.pos.y; c.z = b.pos.z;
    b.mv.To(c.phys.mv); b.tv.To(c.phys.tv); b.fmv.To(c.phys.fmv); b.ftv.To(c.phys.ftv);
}

bool FrontDrive(char d) { return d == 'F' || d == '4'; }
bool RearDrive(char d) { return d == 'R' || d == '4'; }

// CVehicle::ProcessWheel: the tyre's grip pushes back on the speed of its contact patch.
void ProcessWheel(Body& b, const HandlingData& h, CarPhys& p, const V3& wheelFwd, const V3& wheelRight, const V3& contactSpeed,
                  const V3& contactPoint, int wheelsOnGround, float thrust, float brake, float adhesion, int wheelId, int* wheelState,
                  float ts) {
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
    if (speedRight != 0.0f) right = -(speedRight / (float)wheelsOnGround);
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

// One CAutomobile::ProcessControl at time step ts, then CPhysical::ProcessCollision.
void Vehicles::CarStep(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world) {
    CarPhys& p = c.phys;
    Body b = Load(c, h);
    const Transmission& tr = h.trans;
    p.burnout = false;
    p.hitWall = false;
    p.player = player;

    if (player) {   // CAutomobile::ProcessControlInputs
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
    } else {   // ProcessAI, STATUS_ABANDONED
        p.brake = Len2(b.mv) < 0.01f ? 0.2f : 0.0f;
        p.handbrake = false;
        p.steerAngle = 0.0f;
        p.gas = 0.0f;
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

    // The lines' hit fractions become spring compressions (0 fully squashed, 1 the wheel hanging free).
    for (int i = 0; i < 4; i++) {
        const float wr = 1.0f - vm.suspSpring[i] / vm.suspLine[i];
        p.comp[i] = (p.comp[i] - wr) / (1.0f - wr);
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
            limit[i] = stress * (std::min(ts, 3.0f) * b.mass) * h.suspForce * bias * 0.016f;
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
            const float dts = std::clamp(std::min(ts, 3.0f) * h.suspDamping, -kDampingLimitInFrame, kDampingLimitInFrame);
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

    // ---- Wheel contact points and speeds ----
    V3 cp[4], cs[4];
    for (int i = 0; i < 4; i++) {
        cp[i] = p.comp[i] >= 1.0f ? b.Rot(V3(vm.suspEnd[i])) : V3(p.colPt[i]) - b.pos;
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
    if (player)   // the engine runs while someone drives
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
                ProcessWheel(b, h, p, wf, wr, cs[w], cp[w], p.numContactWheels, thrust, pairBrake * brakeBias, adhesion * tractionBias, w, &state, ts);
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
    Store(b, c);
    CarCollision(c, vm, h, ts, world);
}

// CPhysical::ProcessCollision for a car against the world: move by the speeds, find where the suspension
// lines meet the ground, and push the body spheres back out of whatever they hit (ApplyCollisionAlt + ApplyFriction).
void Vehicles::CarCollision(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, const VehicleWorld& world) {
    CarPhys& p = c.phys;
    Body b = Load(c, h);
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
    for (int i = 0; i < 4; i++) {
        const V3 a = b.pos + b.Rot(V3(vm.suspStart[i])), e = b.pos + b.Rot(V3(vm.suspEnd[i]));
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
        p.wheelZ[i] = vm.suspStart[i][2] - frac * vm.suspLine[i] + vm.wheelSize[i] / 2.0f;
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

bool Vehicles::ProcessCar(size_t i, float dt, float accel, float brake, float steer, bool handbrake, const VehicleWorld& world) {
    if (i >= m_cars.size() || !world.line || !world.sphere) return false;
    VehicleInstance& c = m_cars[i];
    if (!UsesCarPhysics(c.model)) return false;
    auto mi = m_models.find(c.model);
    const VehicleInfo* info = Info(c.model);
    const HandlingData* h = info ? Handling(info->handling) : nullptr;
    if (mi == m_models.end() || !h) return false;
    if (c.hasRot) return true;   // a recorded playback owns the car
    VehicleModel& vm = mi->second;
    if (!vm.suspReady) SetupSuspension(vm, *h);
    CarPhys& p = c.phys;
    Activate(c);
    p.inAccel = accel; p.inBrake = brake; p.inSteer = steer; p.handbrake = handbrake;
    p.fakePhysics = 0;
    const float total = dt * 50.0f;
    const int steps = std::clamp((int)std::ceil(total), 1, 8);
    for (int k = 0; k < steps; k++) CarStep(c, vm, *h, total / (float)steps, true, world);
    SyncFromPhys(c, vm);
    c.wheelSpin += c.speed * dt / std::max(0.2f, vm.wheelSize[0] / 2.0f);
    return true;
}

void Vehicles::ProcessAbandoned(float dt, int skip, const VehicleWorld& world) {
    if (!world.line || !world.sphere) return;
    for (size_t i = 0; i < m_cars.size(); i++) {
        VehicleInstance& c = m_cars[i];
        CarPhys& p = c.phys;
        if ((int)i == skip || !p.active || c.removed || c.hasRot || p.fakePhysics >= 10) continue;
        auto mi = m_models.find(c.model);
        const VehicleInfo* info = Info(c.model);
        const HandlingData* h = info ? Handling(info->handling) : nullptr;
        if (mi == m_models.end() || !h) continue;
        VehicleModel& vm = mi->second;
        if (!vm.suspReady) SetupSuspension(vm, *h);
        const float total = dt * 50.0f;
        const int steps = std::clamp((int)std::ceil(total), 1, 8);
        for (int k = 0; k < steps; k++) CarStep(c, vm, *h, total / (float)steps, false, world);
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
    }
}

namespace {

// A car's matrix: the simulated one, else the heading's (a parked car the simulation has not touched).
void CarFrame(const VehicleInstance& c, V3& right, V3& fwd, V3& up) {
    if (c.phys.active || c.hasRot) { right = V3(&c.rot[0]); fwd = V3(&c.rot[3]); up = V3(&c.rot[6]); return; }
    const float cs = std::cos(c.heading), sn = std::sin(c.heading);
    right = V3(cs, sn, 0); fwd = V3(-sn, cs, 0); up = V3(0, 0, 1);
}

}   // namespace

void Vehicles::CarCarCollisions(float dt) {
    (void)dt;
    struct Entry { size_t i; VehicleModel* vm; const HandlingData* h; V3 pos; bool awake; };
    std::vector<Entry> cars;
    for (size_t i = 0; i < m_cars.size(); i++) {
        const VehicleInstance& c = m_cars[i];
        if (c.removed || c.hasRot || !UsesCarPhysics(c.model)) continue;
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
            st = 3;
        } else if (st < 3) st++;
    };
    if (group == 2 || group == 3 || group == 4) {   // ProgressDoorDamage
        uint8_t& st = c.doorStatus[rel];
        if (st == 0 || st == 1) { st = 2; if (rel >= 2) c.doorRatio[rel - 2] = 0.0f; }   // slammed shut
        else if (st == 2) st = 3;
        else if (st == 3 && Random(9) == 0) st = 4;
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
        if (!h || c.wrecked) continue;
        // CAutomobile::VehicleDamage(0, ...): the frame's strongest knock
        if (c.rot[8] < 0.0f && (int)i != playerCar) c.health = std::max(0.0f, c.health - dt * 50.0f * 4.0f);   // lying on its roof
        if (di == 0.0f) continue;
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
