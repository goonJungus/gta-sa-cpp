#pragma once
// Vehicles: the retail car data and a first spawnable car (CVehicleModelInfo / CHandlingDataMgr).
//
// data/vehicles.ide gives each model its DFF, TXD, handling name and wheel size; data/handling.cfg
// gives the dynamics numbers. A model is built from the DFF frame tree: body parts are drawn at their
// frame, the single wheel atomic is drawn at the four wheel dummies. The car is static for now.

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "D3DRenderer.h"

// cTransmission: the gearbox built from a handling line (InitGearRatios), in game units (per 1/50 s).
struct Transmission {
    struct Gear { float maxVelocity = 0, changeUp = 0, changeDown = 0; };
    Gear gears[6];                    // 0 = reverse
    int numGears = 5;
    char driveType = 'R';
    uint32_t handlingFlags = 0;
    float engineAccel = 0.0f, engineInertia = 10.0f;
    float maxVelocity = 0.0f, maxFlatVelocity = 0.0f, maxReverseVelocity = 0.0f;
    void InitGearRatios();
    // CalculateDriveAcceleration: the drive force for this frame; shifts gear, smooths the inertia terms.
    float DriveAcceleration(float gas, uint8_t& gear, float velocity, float* inertia1, float* inertia2, bool allWheelsOnGround, float timeStep) const;
};

// One handling.cfg line (tHandlingData::InitFromData). The raw numbers keep their file units; the
// game-unit copies (per 1/50 s, ConvertDataToGameUnits) drive the physics.
struct HandlingData {
    std::string name;
    float mass = 1500.0f, turnMass = 3000.0f, dragMult = 2.0f;
    float com[3] = {0, 0, 0};
    int percentSubmerged = 75;
    float tractionMult = 0.8f, tractionLoss = 0.8f, tractionBias = 0.5f;
    int numGears = 5;
    float maxVelocity = 160.0f;       // km/h
    float engineAccel = 20.0f;        // m/s^2
    float engineInertia = 10.0f;
    char driveType = 'R', engineType = 'P';
    float brakeDecel = 8.0f, brakeBias = 0.5f;
    bool abs = false;
    float steeringLock = 30.0f;       // degrees
    float suspForce = 1.0f, suspDamping = 0.1f, suspHighSpdComDamp = 0.0f;
    float suspUpper = 0.25f, suspLower = -0.15f, suspBias = 0.5f, antiDive = 0.0f;
    float seatOffset = 0.2f, collisionDamageMult = 0.5f;
    int monetaryValue = 0;            // handling.cfg nMonetaryValue (GET_CAR_MODEL_VALUE)
    uint32_t modelFlags = 0, handlingFlags = 0;
    bool low = false;                 // modelFlags IS_LOW (0x4): the low sit pose
    // Game units (ConvertDataToGameUnits)
    float brakeDecelGame = 0.0f;      // per frame^2
    float collisionDamageGame = 0.0f; // m_fCollisionDamageMultiplier * 2000 / mass
    Transmission trans;
};

struct VehicleInfo {
    int id = 0;
    std::string dff, txd, type, handling, gameName;
    int vclass = 0;                   // vehicles.ide class: 0 normal, 1 poorfamily .. 11 bicycle (eVehicleClass)
    float wheelScaleFront = 0.7f, wheelScaleRear = 0.7f;
};

struct VehiclePart {
    D3DRenderMesh* mesh = nullptr;
    float m[16] = {};                 // part frame -> car, D3D row vectors
    bool wheel = false;
    int node = -1;                    // misc_a / misc_b / misc_c (0..2) when the part hangs on a turret frame
    int door = -1;                    // 0..3 when the part hangs under a side door hinge (door_lf_dummy .. door_rr_dummy)
    float doorLocal[16] = {};         // part frame -> door hinge frame
};

struct VehicleModel {
    VehicleInfo info;
    std::vector<VehiclePart> parts;   // body parts
    D3DRenderMesh* wheelMesh = nullptr;
    std::vector<IDirect3DTexture9*> textures;
    struct PaintPart { D3DRenderMesh* mesh; std::vector<MeshVertex> verts; int slot; };
    std::vector<PaintPart> paintParts;   // meshes whose colour follows the car's paint
    float wheelPos[4][3] = {};        // lf, rf, lb, rb in car space
    bool wheelFound[4] = {false, false, false, false};
    float wheelRadius = 0.35f;        // from the wheel mesh
    float rideHeight = 0.7f;          // body origin above the ground when standing on the wheels
    float wheelBase = 2.8f;           // front to rear axle
    float bbMinY = 0, bbMaxY = 0, bbMinZ = 0;   // body mesh extents in car space (stands in for the collision box)
    float bbHalfX = 0;                          // body half width
    float seatPos[3] = {-0.45f, 0.1f, 0.35f};   // ped_frontseat dummy in car space (driver side)
    bool seatFound = false;
    // The misc_a, misc_b and misc_c frames (eCarNodes CAR_MISC_A..C): tank turret, barrel and muzzle, fire truck nozzle.
    struct Node { bool found = false; int parent = -1; float local[16] = {}; float base[16] = {}; };
    Node nodes[3];
    // The side doors (CAutomobile m_doors DOOR_LEFT_FRONT..DOOR_RIGHT_REAR): CDoor::UpdateFrameMatrix swings the
    // frame about its own Z through ratio * openAngle (left -pi/2, right +pi/2), replacing its rotation, keeping its hinge.
    struct Door { bool found = false; int frame = -1; float base[16] = {}; float hinge[3] = {}; float openAngle = 0.0f; };
    Door doors[4];
    // The DFF's own collision model (CColModel): body spheres and boxes, and its bounding box.
    struct ColSphere { float c[3]; float r; };
    struct ColBox { float lo[3], hi[3]; };
    std::vector<ColSphere> colSpheres;
    std::vector<ColBox> colBoxes;
    std::vector<ColSphere> probes;    // what touches the world: the spheres, plus each box filled with spheres
    float probeRadius = 0.0f;         // the probes' reach from the car origin
    bool colFound = false;
    float colMin[3] = {0, 0, 0}, colMax[3] = {0, 0, 0};
    // CAutomobile::SetupSuspensionLines, eCarWheel order (front left, rear left, front right, rear right):
    // each wheel's line runs from the upper limit down to the lower limit plus half the tyre.
    float suspStart[4][3] = {}, suspEnd[4][3] = {};
    float suspSpring[4] = {}, suspLine[4] = {};   // m_aSuspensionSpringLength / m_aSuspensionLineLength
    float wheelSize[4] = {0.7f, 0.7f, 0.7f, 0.7f};   // GetSizeOfWheel: tyre diameter
    bool suspReady = false;
};

// CAutomobile's physics state (CPhysical move/turn speeds and the wheel contacts), per 1/50 s like retail.
struct CarPhys {
    bool active = false;              // the simulation owns this car: its matrix is in VehicleInstance::rot
    float mv[3] = {0, 0, 0}, tv[3] = {0, 0, 0};       // m_vecMoveSpeed, m_vecTurnSpeed
    float fmv[3] = {0, 0, 0}, ftv[3] = {0, 0, 0};     // m_vecFrictionMoveSpeed, m_vecFrictionTurnSpeed
    float comp[4] = {1, 1, 1, 1};     // m_fWheelsSuspensionCompression (eCarWheel order)
    float compPrev[4] = {1, 1, 1, 1};
    float colPt[4][3] = {}, colN[4][3] = {};          // m_wheelColPoint point and normal
    float wheelCounts[4] = {0, 0, 0, 0};
    int wheelState[4] = {0, 0, 0, 0};  // tWheelState: 0 normal, 1 spinning, 2 skidding, 3 fixed
    float wheelZ[4] = {0, 0, 0, 0};    // drawn wheel centre height in car space
    uint8_t gear = 1;
    float inertia1 = 0.0f, inertia2 = 0.0f;
    float rawSteer = 0.0f, steerAngle = 0.0f;         // m_fRawSteerAngle, m_fSteerAngle (radians, positive = left)
    float gas = 0.0f, brake = 0.0f;
    bool handbrake = false, burnout = false, hitWall = false;
    float tireTemp = 1.0f;
    int numContactWheels = 0, driveWheelsOnGround = 0;
    int fakePhysics = 0;              // abandoned and at rest this many frames: speeds are zeroed (m_nFakePhysics)
    float avgForce[3] = {0, 0, 0}, avgTorque[3] = {0, 0, 0};   // m_vecForce / m_vecTorque: smoothed speeds for the rest test
    float inAccel = 0.0f, inBrake = 0.0f, inSteer = 0.0f;      // the pad this frame (accelerate, brake/reverse, -1 left .. 1 right)
    bool player = false;              // STATUS_PLAYER (else STATUS_ABANDONED)
    float damageIntensity = 0.0f;     // strongest impact this frame (m_fDamageIntensity)
    int damager = -1;                 // the car behind that impact, -1 the world (m_pDamageEntity)
    float impactPos[3] = {0, 0, 0}, impactNormal[3] = {0, 0, 0};   // m_vecLastCollisionPosn, m_vecLastCollisionImpactVelocity
};

// The world as the car physics sees it: CWorld::ProcessLineOfSight for the suspension lines and
// CCollision::ProcessColModels for the body spheres.
struct VehicleWorld {
    // Segment a -> b: the fraction to the first hit (1 when clear) and the surface normal there.
    std::function<float(const float* a, const float* b, float* normal)> line;
    // A sphere: the nearest contact point on the world and its normal (out of the surface); false when clear.
    std::function<bool(const float* centre, float radius, float* point, float* normal)> sphere;
};

struct VehicleInstance {
    int model = 0;
    float x = 0, y = 0, z = 0, heading = 0;
    float wheelSpin = 0.0f;
    int paint[4] = {0, 0, 0, 0};      // carcols.dat colour indices: primary, secondary, tertiary, quaternary
    float speed = 0.0f;               // m/s along the heading, negative in reverse
    float steer = 0.0f;               // current front wheel angle, radians
    float health = 1000.0f;           // below 250 it smokes, at 0 it burns and blows up
    float burnTime = 0.0f;
    bool wrecked = false;
    bool removed = false;             // deleted by the script: not drawn, shot or entered
    float doomV = 0.0f, doomH = 0.05f;   // m_fDoomVerticalRotation / m_fDoomHorizontalRotation: turret yaw and pitch
    float vx = 0.0f, vy = 0.0f;       // world velocity, m/s (speed is its part along the nose)
    bool hasRot = false;              // a recorded playback owns the orientation: rot is right, forward, up (CMatrix)
    float rot[9] = {};
    float doorRatio[4] = {0, 0, 0, 0};   // CDoor::Open angle ratio per side door: 0 shut, 1 fully open
    CarPhys phys;
};

class Vehicles {
public:
    bool Init(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);
    bool Ready() const { return m_ready; }

    const HandlingData* Handling(const std::string& name) const;
    const VehicleInfo* Info(int id) const;
    int ModelCount() const { return (int)m_ids.size(); }
    int IdAt(int i) const { return m_ids[(size_t)i]; }

    // Spawn a car of this model id (loads the model on first use). Returns an instance index or -1.
    int Spawn(D3DRenderer& r, int id, float x, float y, float z, float heading);
    void Clear() { m_cars.clear(); }
    size_t Count() const { return m_cars.size(); }
    VehicleInstance& Car(size_t i) { return m_cars[i]; }
    void Draw(D3DRenderer& r);

    // Arcade driving from the handling line: throttle and brake 0..1, steer -1 (left) .. 1 (right).
    // groundAt(x, y, z) gives the road height or a value below -1e5 when unknown; blocked(newX, newY, oldX, oldY, z)
    // says the car body would run into a solid moving from the old spot to the new one.
    void Step(size_t i, float dt, float throttle, float brake, float steer, bool handbrake,
              const std::function<float(float, float, float)>& groundAt,
              const std::function<bool(float, float, float, float, float)>& blocked);
    // CAutomobile::ProcessControl + CPhysical::ProcessCollision for car i over dt seconds. accel and brake are the
    // pedal buttons 0..1, steer -1 (left) .. 1 (right). An empty world falls back to Step. Cars, quads and
    // monster trucks only; returns false (and does nothing) for other kinds.
    bool ProcessCar(size_t i, float dt, float accel, float brake, float steer, bool handbrake, const VehicleWorld& world);
    // Cars the player left keep rolling until they come to rest (STATUS_ABANDONED with m_nFakePhysics).
    void ProcessAbandoned(float dt, int skip, const VehicleWorld& world);
    bool UsesCarPhysics(int model) const;
    // CPhysical::ProcessCollisionSectorList between cars: body spheres that overlap trade impulses
    // (two-body ApplyCollision + ApplyFriction) and are pushed apart. A parked car that is hit wakes up.
    void CarCarCollisions(float dt);
    // CAutomobile::VehicleDamage for the frame's strongest impact on each simulated car: health falls with the
    // knock above mass / 1500 * 25, scaled by the handling's collision damage (more for the player's victims).
    void CollisionDamage(float dt, int playerCar);
    // A ped's upright cylinder (feet at z) against the car bodies: moves (x, y) out of any car it sinks into, so
    // a parked car blocks and a moving one shoves. Returns the car it touched, or -1.
    int PushPedOut(float* x, float* y, float z, float radius, float height) const;
    // The nearest car within `range` metres of a point, or -1.
    // Ray against the car bodies; returns the car index or -1, *t is the distance along dir.
    int Trace(const float* eye, const float* dir, float reach, float* t) const;
    // Damage a car; returns true when this shot started the burn. Burning cars blow up after a few seconds (true in *exploded).
    bool Damage(int car, float damage);
    // Advance fire and smoke; calls onSmoke(pos, dark), onFire(pos) and onExplode(pos).
    void UpdateDamage(float dt, const std::function<void(const float*, float)>& onSmoke, const std::function<void(const float*)>& onFire, const std::function<void(const float*)>& onExplode);
    int Nearest(float x, float y, float range) const;
    // Body extents of a loaded model: y range and the lowest z. False when the model is not loaded.
    bool ModelBounds(int id, float* minY, float* maxY, float* minZ) const;
    // The driver's seat (ped_frontseat, or its passenger-side twin) of car i in the world, and whether the car sits low.
    bool SeatWorld(size_t i, float out[3], bool* low = nullptr, bool passenger = false) const;
    // Car i's model has this side door hinge (0 lf, 1 rf, 2 lr, 3 rr).
    bool HasDoor(size_t i, int door) const;
    // Car i's body footprint in car space: half width and the y range. False when the model is not loaded.
    bool Footprint(size_t i, float* halfX, float* minY, float* maxY) const;
    // A turret frame (0 misc_a, 1 misc_b, 2 misc_c) of car i in the world with the turret rotation applied
    // (RwFrameGetLTM). False when the model has no such frame.
    bool NodeMatrix(size_t i, int node, float out[16]) const;
    bool NodeWorldPos(size_t i, int node, float out[3]) const;

private:
    VehicleModel* Model(D3DRenderer& r, int id);
    void SetupSuspension(VehicleModel& vm, const HandlingData& h);
    void CarStep(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world);
    void CarCollision(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, const VehicleWorld& world);
    void SyncFromPhys(VehicleInstance& c, const VehicleModel& vm);
    void Activate(VehicleInstance& c);

    bool m_ready = false;
    std::string m_dir;
    std::unordered_map<int, VehicleInfo> m_info;
    std::vector<int> m_ids;
    std::unordered_map<std::string, HandlingData> m_handling;
    std::unordered_map<int, VehicleModel> m_models;
    std::vector<VehicleInstance> m_cars;
    std::vector<uint8_t> m_sharedTxd;
    std::vector<std::array<uint8_t, 3>> m_colours;                  // carcols.dat colour table
    std::unordered_map<std::string, std::vector<std::array<int, 4>>> m_carColours;   // model name -> allowed paint sets
    unsigned m_rng = 7;
    void ApplyPaint(D3DRenderer& r, VehicleModel& vm, const int paint[4]);
    int m_paintedFor = -1;
};
