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

// One handling.cfg line (the first fields; the rest are read when the dynamics need them).
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
    float steeringLock = 30.0f;       // degrees
    int monetaryValue = 0;            // handling.cfg nMonetaryValue (GET_CAR_MODEL_VALUE)
    bool low = false;                 // modelFlags IS_LOW (0x4): the low sit pose
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
    float seatPos[3] = {-0.45f, 0.1f, 0.35f};   // ped_frontseat dummy in car space (driver side)
    bool seatFound = false;
    // The misc_a, misc_b and misc_c frames (eCarNodes CAR_MISC_A..C): tank turret, barrel and muzzle, fire truck nozzle.
    struct Node { bool found = false; int parent = -1; float local[16] = {}; float base[16] = {}; };
    Node nodes[3];
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
    // The driver's seat (ped_frontseat) of car i in the world, and whether the car sits low.
    bool SeatWorld(size_t i, float out[3], bool* low = nullptr) const;
    // A turret frame (0 misc_a, 1 misc_b, 2 misc_c) of car i in the world with the turret rotation applied
    // (RwFrameGetLTM). False when the model has no such frame.
    bool NodeMatrix(size_t i, int node, float out[16]) const;
    bool NodeWorldPos(size_t i, int node, float out[3]) const;

private:
    VehicleModel* Model(D3DRenderer& r, int id);

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
