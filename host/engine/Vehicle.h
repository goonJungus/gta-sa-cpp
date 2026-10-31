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
    // tBikeHandlingData: the handling.cfg '!' line, in game units (ConvertBikeDataToGameUnits: the lean, wheelie
    // and stoppie limits as sines, the full animation lean in radians).
    struct Bike {
        bool found = false;
        float leanFwdCOM = 0, leanFwdForce = 0, leanBakCOM = 0, leanBakForce = 0, maxLean = 0, fullAnimLean = 0, desLean = 0;
        float speedSteer = 0, slipSteer = 0, noPlayerCOMz = 0, wheelieAng = 0, stoppieAng = 0, wheelieSteer = 0;
        float wheelieStabMult = 0, stoppieStabMult = 0;
    } bike;
    // tBoatHandlingData: the handling.cfg '%' line (the propeller's place and push, planing, and the water's
    // resistance per axis to moving and turning)
    struct Boat {
        bool found = false;
        float thrustY = 0, thrustZ = 0, thrustAppZ = 0, aqPlaneForce = 0, aqPlaneLimit = 0, aqPlaneOffset = 0, waveAudioMult = 0;
        float moveRes[3] = {1, 1, 1}, turnRes[3] = {1, 1, 1}, lookLRBehindCamHeight = 0;
    } boat;
    // tFlyingHandlingData: the handling.cfg '$' line, raw (retail converts none of it). A field sscanf could
    // not read stays 0, as in the zeroed retail table (RCRAIDER's "0.1s" stops it short).
    struct Fly {
        bool found = false;
        float thrust = 0, thrustFallOff = 0, yaw = 0, yawStab = 0, sideSlip = 0, roll = 0, rollStab = 0, pitch = 0, pitchStab = 0;
        float formLift = 0, attackLift = 0, gearUpR = 0, gearDownL = 0, windMult = 0, moveRes = 0;
        float turnRes[3] = {0, 0, 0}, speedRes[3] = {0, 0, 0};
    } fly;
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
    int comp = -1;                    // a damageable component: 0..5 eDoors (bonnet, boot, doors), 6..12 6 + ePanels
    bool damagedMesh = false;         // the component's _dam stand-in (shown once it is damaged)
    float doorLocal[16] = {};         // part frame -> door hinge frame
    int bumper = -1;                  // 0 / 1 when the part hangs under bump_front_dummy / bump_rear_dummy
    float bumperLocal[16] = {};       // part frame -> bumper frame
    int bikeNode = -1;                // a bike's moving frame the part hangs under (VehicleModel::BikeNode order)
    float bikeLocal[16] = {};         // part frame -> that frame
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
    // headlights, taillights, headlights2, taillights2 dummies (CVehicleModelInfo m_avDummyPos 0..3), right side
    float lightPos[4][3] = {};
    bool lightFound[4] = {false, false, false, false};
    float enginePos[3] = {0, 0, 0};   // the engine dummy: where the fire and overheat smoke come from
    bool engineFound = false;
    // The misc_a, misc_b and misc_c frames (eCarNodes CAR_MISC_A..C): tank turret, barrel and muzzle, fire truck nozzle.
    struct Node { bool found = false; int parent = -1; float local[16] = {}; float base[16] = {}; };
    Node nodes[3];
    // The hinged parts (CAutomobile m_doors, CDoor::Init): slots 0..3 the side doors lf rf lr rr, 4 the bonnet,
    // 5 the boot. CDoor::UpdateFrameMatrix turns the frame about its own axis (X or Z) by the door angle, replacing
    // its rotation and keeping its hinge. dirn is the eDoorAxis the door leaf points along (0 +x .. 5 -z).
    struct Door {
        bool found = false; int frame = -1; float base[16] = {}; float hinge[3] = {}; float openAngle = 0.0f;
        int axis = 2, dirn = 4; bool based = true, lowGravity = false;
        float pivot[3] = {};          // the hinge in car space
    };
    Door doors[6];
    // bump_front_dummy, bump_rear_dummy (CAR_BUMP_FRONT / CAR_BUMP_REAR): the frames a loose bumper bounces on.
    struct Bumper { bool found = false; int frame = -1; float local[16] = {}; float base[16] = {}; float pos[3] = {}; };
    Bumper bumpers[2];
    // eBikeNodes that move: 0 forks_front, 1 forks_rear, 2 handlebars, 3 wheel_front, 4 wheel_rear. parent is the
    // node above (wheel_front hangs on the front forks), else base is the fixed frame's matrix in bike space.
    struct BikeNode { bool found = false; int frame = -1, parent = -1; float local[16] = {}; float base[16] = {}; };
    // bike frames 0..7, then a boat's rudder, rear flaps and propellers (8..14; a plane's propellers are 11..14 too),
    // a helicopter's rotors: 15 static_rotor, 16 moving_rotor, 17 static_rotor2, 18 moving_rotor2 (eHeliNodes 12..15),
    // and a plane's control surfaces: 19 rudder, 20 elevator_l, 21 elevator_r, 22 aileron_l, 23 aileron_r,
    // 24 gear_l, 25 gear_r (ePlaneNodes 16..22)
    static constexpr int kMoveNodes = 26;
    BikeNode bikeNodes[kMoveNodes];
    bool isBike = false, isBoat = false, isHeli = false, isPlane = false, isTrailer = false;
    float hookup[3] = {0, 0, 0};      // the hookup dummy (DUMMY_TRAILER_ATTACH): where a trailer hangs on its tractor
    bool hookupFound = false;
    float swingArmLength = 0.0f;      // CBike::m_fSwingArmLength: forks_rear to the rear wheel
    // The DFF's own collision model (CColModel): body spheres and boxes, and its bounding box.
    // piece: the car part the surface belongs to (eVehicleCollisionComponent: 1 bonnet, 2 boot, 3 front bumper,
    // 4 rear bumper, 5..8 doors lf rf lr rr, 9..12 wings, 19 windscreen, 0 none)
    struct ColSphere { float c[3]; float r; uint8_t piece = 0; };
    struct ColBox { float lo[3], hi[3]; uint8_t piece = 0; };
    std::vector<ColSphere> colSpheres;
    std::vector<ColBox> colBoxes;
    std::vector<ColSphere> probes;    // what touches the world: the spheres, plus each box filled with spheres
    float probeRadius = 0.0f;         // the probes' reach from the car origin
    bool colFound = false;
    // The first two consecutive glass faces of the COL mesh (surface 45..47): the windscreen rectangle.
    float glassTri[6][3] = {};
    bool glassFound = false;
    float colMin[3] = {0, 0, 0}, colMax[3] = {0, 0, 0};
    // CAutomobile::SetupSuspensionLines, eCarWheel order (front left, rear left, front right, rear right):
    // each wheel's line runs from the upper limit down to the lower limit plus half the tyre.
    float suspStart[4][3] = {}, suspEnd[4][3] = {};
    float suspSpring[4] = {}, suspLine[4] = {};   // m_aSuspensionSpringLength / m_aSuspensionLineLength
    float wheelSize[4] = {0.7f, 0.7f, 0.7f, 0.7f};   // GetSizeOfWheel: tyre diameter
    float frontHeight = 0.0f;         // m_fFrontHeightAboveRoad: the body origin over the road at rest
    // CTrailer::SetupSuspensionLines: two support legs at the nose (lines from the box's top front corners down),
    // and misc_a's height (m_fHeight), the legs' frame drawn no higher than it was modelled
    bool trailerLegs = false;
    float supportStart[2][3] = {}, supportEnd[2][3] = {};
    float legHeight = 0.0f;
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
    int piece = 0;                    // the part of the car that took it (m_nPieceType, eVehicleCollisionComponent)
    float impactPos[3] = {0, 0, 0}, impactNormal[3] = {0, 0, 0};   // m_vecLastCollisionPosn, m_vecLastCollisionImpactVelocity
    float lastDI = 0.0f;              // the last frame's strongest knock, for the doors
    float oldMv[3] = {0, 0, 0}, oldTv[3] = {0, 0, 0};   // the speeds a frame ago (the doors swing on the change)
    // CBike: CRideAnimData lean (LeanAngle / DesiredLeanAngle, radians, + right), the handlebars, the rider's
    // forward lean (-1 back .. 1 forward), the wheels' spin, burnout (m_nBrakesOn) and the side stand.
    float leanAngle = 0.0f, desiredLean = 0.0f, barSteer = 0.0f, leanFwd = 0.0f, inLeanFwd = 0.0f;
    float wheelAngVel[2] = {0, 0}, wheelPitch[2] = {0, 0};
    float brakingSlide = 0.0f, lastThrust = 0.0f;
    bool brakesOn = false, onSideStand = true;
    float animLeanLeft = 0.0f, animLeanFwd = 0.0f;   // CRideAnimData AnimLeanLeft / AnimLeanFwd (the rider's pose)
    float riderStill = 0.0f;          // the rider's BIKE_STILL blend: a foot down tips the bike 10 degrees
    bool knockOff = false;            // CEventKnockOffBike raised for the rider this frame
    float knockOffVel[3] = {0, 0, 0}; // the rider's speed as he leaves (m/s)
    int knockOffDir = 0;              // the fall: 0 front, 1 left, 2 back, 3 right (CPed::GetLocalDirection)
    // CBmx: what the rider's clips tell the bicycle (Vehicles::SetBmxRider), and its crank and sprint sway
    bool bmxPedalOn = false;          // sprint energy left: the pedals keep turning with no key held (gas 1)
    float bmxHopBlend = 0.0f;         // BIKE_BUNNYHOP's blend: over a half, no gas
    float bmxHopLaunch = 0.0f;        // the hop's crouch (m_fControlJump, 0..25) to launch on next step, 0 none
    float bmxPedalPhase = 0.0f, bmxPedalBlend = 0.0f, bmxSprintPhase = 0.0f, bmxSprintBlend = 0.0f;
    float crankAngle = 0.0f;          // m_fCrankAngle (the pedals stay level: m_fPedalAngleL/R = -crank)
    // CBoat
    float engineSpeed = 0.0f, propAngle = 0.0f;   // m_EngineSpeed (the propeller), m_PropellerAngle
    float prevVolume = 0.0f, boatForce = 0.0f;    // m_PrevVolume (the last immersion), |the buoyancy force|
    bool boatInWater = false, engineInWater = false;
    float sprintLean = 0.0f;          // m_fSprintLeanAngle: the frame rocks under a sprinting rider
    // CHeli: the pad (ProcessControlInputs: m_fAccelerationBreakStatus, m_fSteeringUpDown, m_fSteeringLeftRight,
    // m_fLeftRightSkid), the rotor (m_fHeliRotorSpeed) and the drawn rotor angles (m_fRotorZ, m_fSecondRotorZ)
    float inSkid = 0.0f;              // look right (+1) / left (-1): the tail rotor's yaw
    float flyAccel = 0.0f, flyUpDown = 0.0f, flyLeftRight = 0.0f, flySkid = 0.0f;
    float rotorSpeed = 0.0f, rotorZ = 0.0f, rotor2Z = 0.0f;
    // CPlane: m_fPropSpeed, m_fLandingGearStatus (0 down, 1 up, between: moving up, below 0: moving down) and the
    // landing gear key this frame (the pad's R3)
    float planePropSpeed = 0.0f, gearStatus = 0.0f;
    bool inGear = false;
    float inGunUpDown = 0.0f;         // the pad's special control up (+1) / down (-1): a tow truck's hoist
    // CTrailer: the legs' extension (m_fTrailerTowedRatio: 1 down, 0 wound up), the least of it that holds
    // (m_fTrailerTowedRatio2) and each leg line's hit (m_supportRatios, m_supportCPs)
    float supportExt = 1.0f, supportMin = 1.0f;
    float supportRatio[2] = {1, 1};
    float supportPt[2][3] = {}, supportN[2][3] = {};
};

// The world as the car physics sees it: CWorld::ProcessLineOfSight for the suspension lines and
// CCollision::ProcessColModels for the body spheres.
struct VehicleWorld {
    // Segment a -> b: the fraction to the first hit (1 when clear) and the surface normal there.
    std::function<float(const float* a, const float* b, float* normal)> line;
    // A sphere: the nearest contact point on the world and its normal (out of the surface); false when clear.
    std::function<bool(const float* centre, float radius, float* point, float* normal)> sphere;
    // CWaterLevel::GetWaterLevel: the surface height at (x, y); false where there is no water.
    std::function<bool(float x, float y, float* z)> water;
    // CWeather::WindDir (no wind is simulated yet: still air)
    float wind[3] = {0, 0, 0};
};

struct VehicleInstance {
    int model = 0;
    float x = 0, y = 0, z = 0, heading = 0;
    float wheelSpin = 0.0f;
    int paint[4] = {0, 0, 0, 0};      // carcols.dat colour indices: primary, secondary, tertiary, quaternary
    float speed = 0.0f;               // m/s along the heading, negative in reverse
    float steer = 0.0f;               // current front wheel angle, radians
    float health = 1000.0f;           // below 250 it smokes, at 0 it burns and blows up
    float burnTime = 0.0f;            // m_fBurnTimer: seconds the engine has been on fire (below 250 health)
    float smokeTime = 0.0f;
    bool wrecked = false;
    bool removed = false;             // deleted by the script: not drawn, shot or entered
    float doomV = 0.0f, doomH = 0.05f;   // m_fDoomVerticalRotation / m_fDoomHorizontalRotation: turret yaw and pitch
    float vx = 0.0f, vy = 0.0f;       // world velocity, m/s (speed is its part along the nose)
    bool hasRot = false;              // a recorded playback owns the orientation: rot is right, forward, up (CMatrix)
    bool derailed = false;            // a train carriage off its rails: an ordinary physical body (bIsDerailed)
    float rot[9] = {};
    float doorRatio[4] = {0, 0, 0, 0};   // CDoor::Open angle ratio per side door: 0 shut, 1 fully open
    // CDamageManager: door / bonnet / boot states (eDoors) and panel states (ePanels: wings lf rf lr rr,
    // windscreen, front bumper, rear bumper), ePanelDamageState 0 ok .. 4 missing; smashed lights.
    uint8_t doorStatus[6] = {0, 0, 0, 0, 0, 0};
    uint8_t panelStatus[7] = {0, 0, 0, 0, 0, 0, 0};
    uint8_t lightStatus[4] = {0, 0, 0, 0};
    uint8_t wheelStatus[4] = {0, 0, 0, 0};   // eCarWheel order (fl, rl, fr, rr): 0 ok, 1 burst, 2 missing
    uint16_t randomSeed = 0;          // CEntity::m_nRandomSeed (picks the minute its lights come on)
    bool engineOn = false;            // bEngineOn: started by a driver, keeps running when he leaves
    bool hasDriver = false;           // someone at the wheel (brake lights need a foot on the pedal)
    bool siren = false;               // bSirenOrAlarm: the siren and its light bar
    int radioStation = -1;            // eRadioID it is tuned to, -1 until someone drives it
    bool recycle = false;             // a car generator's parked car: its slot is reused once it is removed
    int alarmMs = 0;                  // m_nAlarmState: 0 none, -1 armed, else ms left ringing
    bool locked = false;              // m_nDoorLock CARLOCK_LOCKED
    // CDoor per eDoors (bonnet, boot, lf, rf, lr, rr) while it swings loose: angle, angular speed, eDoorState.
    float doorAngle[6] = {0, 0, 0, 0, 0, 0}, doorAngVel[6] = {0, 0, 0, 0, 0, 0};
    uint8_t doorHit[6] = {0, 0, 0, 0, 0, 0};
    // CAutomobile m_panels (CBouncingPanel): a loose bumper wobbles on its frame. bumper -1 unused, else 0 front
    // 1 rear; axis picks which two matrix entries the rotation drives; rot and vel are m_vecRotation / m_vecPos.
    struct BouncingPanel { int bumper = -1; int axis = 0; float limit = 0.0f; float rot[3] = {0, 0, 0}, vel[3] = {0, 0, 0}; };
    BouncingPanel panels[3];
    // m_pTowingVehicle / m_pVehicleBeingTowed as car indices (-1 none), and a tow truck's or tractor's hoist
    // (m_wMiscComponentAngle: 0 raised .. 20000 lowered)
    int towedBy = -1, towing = -1;
    int hoist = 0;
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
    // A car model, loading it on first use (nullptr if it cannot be).
    const VehicleModel* LoadModel(D3DRenderer& r, int id) { return Model(r, id); }

    // Spawn a car of this model id (loads the model on first use). Returns an instance index or -1.
    int Spawn(D3DRenderer& r, int id, float x, float y, float z, float heading);
    void Clear() { m_cars.clear(); }
    size_t Count() const { return m_cars.size(); }
    VehicleInstance& Car(size_t i) { return m_cars[i]; }
    void Draw(D3DRenderer& r);
    // The clock and weather the lights read (CClock, CWeather::Foggyness / WetRoads).
    struct LightEnv { int hour = 12, minute = 0; float foggy = 0.0f, wet = 0.0f; };
    // CVehicle::GetVehicleLightsStatus.
    static bool LightsOn(const VehicleInstance& c, const LightEnv& env);
    // CVehicle::ProcessCarAlarm: a ringing alarm counts down. Returns the ringing car nearest to (x, y), or -1.
    int ProcessCarAlarms(float dt, float x, float y);
    // CVehicle::UsesSiren: emergency vehicles and the law (IsLawEnforcementVehicle), not the rhino.
    static bool UsesSiren(int model);
    // CVehicle::DoVehicleLights: head and tail light coronas of the cars near the camera.
    void RenderLights(D3DRenderer& r, const float cam[3], const LightEnv& env) const;

    // Arcade driving from the handling line: throttle and brake 0..1, steer -1 (left) .. 1 (right).
    // groundAt(x, y, z) gives the road height or a value below -1e5 when unknown; blocked(newX, newY, oldX, oldY, z)
    // says the car body would run into a solid moving from the old spot to the new one.
    void Step(size_t i, float dt, float throttle, float brake, float steer, bool handbrake,
              const std::function<float(float, float, float)>& groundAt,
              const std::function<bool(float, float, float, float, float)>& blocked);
    // CAutomobile::ProcessControl + CPhysical::ProcessCollision for car i over dt seconds. accel and brake are the
    // pedal buttons 0..1, steer -1 (left) .. 1 (right). An empty world falls back to Step. Cars, quads and
    // monster trucks only; returns false (and does nothing) for other kinds.
    // The drawn matrix (D3D rows: right, forward, up, position), a bike's lean included, and a seat in car space.
    void BodyMatrix(size_t i, float out[16], bool drawn = false) const;   // drawn: a BMX frame's sprint sway too
    bool SeatLocal(size_t i, float out[3], bool passenger = false) const;
    // CBike::ProcessRiderAnims for the player: the smoothed lean left / right and forward / back (-1 .. 1) the
    // rider's lean clips are posed at, the share left for them, and which base pose: 0 ride, 1 still, 2 pushing back.
    struct BikeRide {
        float leanLeft = 0, leanFwd = 0; float leftWeight = 0, fwdWeight = 0; int base = 0;
        // for CBmx::ProcessDrivingAnims: the bike's lean and the rider's weight, forward speed (per 1/50 s), gas,
        // and the pedal clip's rate (sprinting is for rates under sprintBelow)
        float leanAngle = 0, riderLeanFwd = 0, fwdSpeed = 0, gas = 0, pedalRate = 0, sprintBelow = 2.5f;
    };
    bool BikeRiderAnims(size_t i, float dt, float stillBlend, BikeRide& out);
    // The rider knocked off this bike (CBike::DamageKnockOffRider, or ProcessRiderAnims on a spin / fast reverse):
    // true once, with the speed he is thrown at and the side he falls to.
    bool TakeKnockOff(size_t i, float vel[3], int* dir);
    // The BMX rider's state for this frame (CBmx::ProcessAI / ProcessBunnyHop / ProcessControl): phases are the
    // clip time over its length; hopLaunch is the hop's crouch when BIKE_BUNNYHOP has just finished, else 0.
    struct BmxRider { bool pedalOn = false; float hopBlend = 0, hopLaunch = 0, pedalPhase = 0, pedalBlend = 0, sprintPhase = 0, sprintBlend = 0, crank = 0; };
    void SetBmxRider(size_t i, const BmxRider& r);
    // leanFwd: a bike rider's weight, -1 back .. 1 forward (the pad's steer up / down).
    // skid: look right (+1) / left (-1), a helicopter's tail rotor.
    // gear: the landing gear key went down this frame.
    // gunUpDown: the special control up (+1) / down (-1), a tow truck's hoist.
    bool ProcessCar(size_t i, float dt, float accel, float brake, float steer, bool handbrake, const VehicleWorld& world, float leanFwd = 0.0f,
                    float skid = 0.0f, bool gear = false, float gunUpDown = 0.0f);
    bool IsTrailer(int model) const;          // CTrailer (vehicles.ide type trailer)
    // CTrailer::SetTowLink(cab, true): the trailer is put on the cab's tow bar (ATTACH_TRAILER_TO_CAB).
    bool AttachTrailer(size_t trailer, size_t cab);
    void DetachTrailer(size_t trailer) { BreakTowLink(trailer); }
    // Cars the player left keep rolling until they come to rest (STATUS_ABANDONED with m_nFakePhysics).
    void ProcessAbandoned(float dt, int skip, const VehicleWorld& world);
    bool UsesCarPhysics(int model) const;
    bool UsesBikePhysics(int model) const;   // CBike and CBmx
    bool UsesBoatPhysics(int model) const;   // CBoat
    bool UsesHeliPhysics(int model) const;   // CHeli (a CAutomobile that flies)
    bool UsesPlanePhysics(int model) const;  // CPlane (not yet the Skimmer, Vortex or RC Baron)
    // CPhysical::ProcessCollisionSectorList between cars: body spheres that overlap trade impulses
    // (two-body ApplyCollision + ApplyFriction) and are pushed apart. A parked car that is hit wakes up.
    void CarCarCollisions(float dt);
    // CAutomobile::ProcessSwingingDoor for every simulated car: a hard knock pops doors open (CDoor::ProcessImpact),
    // loose doors, bonnet and boot swing with the car's jolts (CDoor::Process) and slam shut, a bonnet blown fully
    // open at speed tears off.
    void ProcessDoors(float dt);
    // Parts that came off (CAutomobile::SpawnFlyingComponent objects): fall, tumble, bounce and vanish.
    void UpdateFlying(float dt, const VehicleWorld& world);
    size_t FlyingCount() const { return m_flying.size(); }
    // CAutomobile::VehicleDamage for the frame's strongest impact on each simulated car: health falls with the
    // knock above mass / 1500 * 25, scaled by the handling's collision damage (more for the player's victims).
    void CollisionDamage(float dt, int playerCar);
    // A ped's upright cylinder (feet at z) against the car bodies: moves (x, y) out of any car it sinks into, so
    // a parked car blocks and a moving one shoves. Returns the car it touched, or -1.
    // With pedDI, also the ped's damage intensity from the hardest car that ran into it (CPhysical::ApplyCollision,
    // car x10 mass against a 70 kg ped) and that car in *hitCar.
    int PushPedOut(float* x, float* y, float z, float radius, float height, float* pedDI = nullptr, int* hitCar = nullptr) const;
    // KillPedWithCar's force back on car i from a ped struck at pedCentre.
    void PedHitReaction(size_t i, const float pedCentre[3], bool overBonnet);
    // The car's COL bounding box in car space (the body extents when the model has no COL).
    bool ColBounds(size_t i, float mn[3], float mx[3]) const;
    // The car's matrix: right, forward and up.
    void Frame(size_t i, float r[3], float f[3], float u[3]) const;
    // The nearest car within `range` metres of a point, or -1.
    // Ray against the car bodies; returns the car index or -1, *t is the distance along dir.
    // With tyre, the tyres count too (CWorld::bIncludeCarTyres): *tyre is the eCarWheel struck, else -1.
    int Trace(const float* eye, const float* dir, float reach, float* t, int* tyre = nullptr) const;
    // CAutomobile::BurstTyre: an intact tyre goes flat (and the car is jolted). False if it already was.
    bool BurstTyre(size_t i, int wheel);
    // Damage a car; returns true when this hit set the engine on fire (health under 250). Burning cars blow up 5 s later.
    bool Damage(int car, float damage);
    // Advance fire and smoke; calls onSmoke(pos, dark), onFire(pos) and onExplode(pos).
    void UpdateDamage(float dt, const std::function<void(const float*, float)>& onSmoke, const std::function<void(const float*)>& onFire, const std::function<void(const float*)>& onExplode);
    // CGlass::CarWindscreenShatters: called with the glass rectangle (bottom corner, up and across edges, world
    // space), the car's move speed (per 1/50 s) and the rectangle's centre when a windscreen falls out.
    std::function<void(const float* corner, const float* fwd, const float* right, const float* vel, const float* centre)> onWindscreenShatter;
    void WindscreenShatters(size_t ci);
    // enterable: only cars a ped can sit in (not trailers, which have no seat).
    int Nearest(float x, float y, float range, bool enterable = false) const;
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
    // collide false: ProcessControl only (the caller moves the car with CarCollision afterwards).
    void CarStep(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world, bool collide = true);
    void CarCollision(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, const VehicleWorld& world, bool lines = true);
    void BoatInputs(VehicleInstance& c, const HandlingData& h, float ts, bool player);
    void BoatStep(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world);
    void BoatFrameMatrices(const VehicleInstance& c, const VehicleModel& vm, float nodes[VehicleModel::kMoveNodes][16]) const;
    // CHeli::ProcessControlInputs (once a frame), CHeli::ProcessFlyingCarStuff + CVehicle::FlyingControl (each step,
    // at the end of CAutomobile::ProcessControl) and CHeli::PreRender's rotors (FlightPhysics.cpp).
    void HeliInputs(VehicleInstance& c, bool player);
    void HeliFlyingCarStuff(VehicleInstance& c, const HandlingData& h, float ts, bool player, const VehicleWorld& world);
    void HeliRotors(VehicleInstance& c, float ts);
    void HeliFrameMatrices(const VehicleInstance& c, const VehicleModel& vm, float nodes[VehicleModel::kMoveNodes][16]) const;
    // CPlane::ProcessControlInputs (once a frame; the wheel brake and nose-wheel steering it sets), CPlane::
    // ProcessFlyingCarStuff + FlyingControl FLIGHT_MODEL_PLANE (each step) and CPlane::PreRender's moving parts.
    void PlaneInputs(VehicleInstance& c, const HandlingData& h, float ts, bool player);
    void PlaneWheelInputs(VehicleInstance& c, const HandlingData& h, bool player);
    void PlaneFlyingCarStuff(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world);
    void PlaneFrameMatrices(const VehicleInstance& c, const VehicleModel& vm, float nodes[VehicleModel::kMoveNodes][16]) const;
    void BoatDamage(VehicleInstance& c, const HandlingData& h, float di, bool player);
    // CBike::SetupSuspensionLines and one CBike::ProcessControl (BikePhysics.cpp).
    void SetupBikeSuspension(VehicleModel& vm, const HandlingData& h);
    void BikeStep(VehicleInstance& c, const VehicleModel& vm, const HandlingData& h, float ts, bool player, const VehicleWorld& world);
    void BikeFrameMatrices(const VehicleInstance& c, const VehicleModel& vm, float nodes[VehicleModel::kMoveNodes][16]) const;
    void BikeLeanBody(const VehicleInstance& c, const VehicleModel& vm, float body[16], float sway = 0.0f) const;
    float RiderSkill(int model) const;
    void BmxSway(VehicleInstance& c);
    void SyncFromPhys(VehicleInstance& c, const VehicleModel& vm);
    void Activate(VehicleInstance& c);
    // The model and handling of a car model, its suspension set up; false when either is missing.
    bool Lookup(int model, VehicleModel*& vm, const HandlingData*& h);
    // One time step of car i, then of the car it tows (so the link sees both moved alike), and the end of the
    // frame for the towed chain.
    void StepVehicle(size_t i, float ts, bool player, const VehicleWorld& world);
    void SyncTowed(size_t i, float dt);
    bool TowedChainStepped(size_t i, int skip) const;
    // Tow links (TowPhysics.cpp): CVehicle / CAutomobile / CTrailer GetTowHitchPos, GetTowBarPos (world points;
    // other is the car at the far end, -1 none), SetTowLink, BreakTowLink, UpdateTractorLink, UpdateTrailerLink,
    // the CAutomobile::ProcessControl link block, CTrailer::ScanForTowLink and CAutomobile::TowTruckControl.
    bool TowHitchPos(size_t i, bool check, int other, float out[3]);
    bool TowBarPos(size_t i, bool check, int other, float out[3]);
    bool SetTowLink(size_t towed, size_t tractor, bool place, float ts);
    void BreakTowLink(size_t towed);
    void TractorLink(size_t tractor, bool settle, bool fix, float ts);
    void TrailerLink(size_t towed, bool settle, bool fix, float ts);
    void TowLinks(size_t towed, float ts);
    void ScanForTowLink(size_t trailer, float ts);
    void ScanTrailers(float ts);
    void TowTruckControl(size_t i, float ts);
    // A component (0..5 eDoors, 6..12 6 + ePanels, 13 a wheel) of car ci comes off as a flying object (collision type
    // 0 bumper, 1 wheel, 2 door, 3 bonnet, 4 boot, 5 panel). Call before its state hides it.
    void SpawnFlying(size_t ci, int comp, int type, int wheel = 0);
    // CAutomobile::BlowUpCar: the car jumps, its doors fly off, a wheel comes away.
    void BlowUp(size_t ci);
    bool PartVisible(const VehicleInstance& c, const VehiclePart& p) const;
    void PartToCar(const VehicleInstance& c, const VehicleModel& vm, const VehiclePart& p, float out[16]) const;
    float DoorDrawAngle(const VehicleInstance& c, int slot) const;
    // Parts in flight: the meshes (with their frame relative to the object), the body and its speeds.
    struct FlyingPart {
        int model = 0;
        int paint[4] = {0, 0, 0, 0};
        struct Piece { D3DRenderMesh* mesh; float m[16]; };
        std::vector<Piece> pieces;
        float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}, pos[3] = {0, 0, 0}, mv[3] = {0, 0, 0}, tv[3] = {0, 0, 0}, com[3] = {0, 0, 0};
        float mass = 10.0f, turnMass = 25.0f, air = 0.97f, life = 20.0f;
    };
    std::vector<FlyingPart> m_flying;
    int Random(int n);
    void ApplyComponentDamage(VehicleInstance& c, int comp, float intensity);
    void DamageComponents(VehicleInstance& c, const HandlingData& h, float di);
    void BikeDamage(VehicleInstance& c, const HandlingData& h, float di, bool player);

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
