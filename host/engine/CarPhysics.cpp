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
                    vm.probes.push_back({{at(0, nx, ix), at(1, ny, iy), at(2, nz, iz)}, r});
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
    struct Contact { V3 point, normal; float depth; };
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
        contacts[count++] = {q, V3(nm), depth};
    }
    float maxImpact = 0.0f;
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
            if (impact > maxImpact) { maxImpact = impact; ct.point.To(p.impactPos); ct.normal.To(p.impactNormal); }
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
    p.damageIntensity = std::max(p.damageIntensity, maxImpact);
    Store(b, c);
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
    if (!p.active) {   // hand the car to the simulation: its matrix from the heading, its speed from the arcade model
        const float cs = std::cos(c.heading), sn = std::sin(c.heading);
        const float r[9] = {cs, sn, 0, -sn, cs, 0, 0, 0, 1};
        std::memcpy(c.rot, r, sizeof(r));
        p = CarPhys{};
        p.mv[0] = c.vx / 50.0f; p.mv[1] = c.vy / 50.0f;
        p.active = true;
    }
    p.inAccel = accel; p.inBrake = brake; p.inSteer = steer; p.handbrake = handbrake;
    p.fakePhysics = 0;
    p.damageIntensity = 0.0f;
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
        p.damageIntensity = 0.0f;
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
