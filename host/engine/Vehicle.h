#pragma once
// Vehicles: the retail car data and a first spawnable car (CVehicleModelInfo / CHandlingDataMgr).
//
// data/vehicles.ide gives each model its DFF, TXD, handling name and wheel size; data/handling.cfg
// gives the dynamics numbers. A model is built from the DFF frame tree: body parts are drawn at their
// frame, the single wheel atomic is drawn at the four wheel dummies. The car is static for now.

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
};

struct VehicleInfo {
    int id = 0;
    std::string dff, txd, type, handling, gameName;
    float wheelScaleFront = 0.7f, wheelScaleRear = 0.7f;
};

struct VehiclePart {
    D3DRenderMesh* mesh = nullptr;
    float m[16] = {};                 // part frame -> car, D3D row vectors
    bool wheel = false;
};

struct VehicleModel {
    VehicleInfo info;
    std::vector<VehiclePart> parts;   // body parts
    D3DRenderMesh* wheelMesh = nullptr;
    std::vector<IDirect3DTexture9*> textures;
    float wheelPos[4][3] = {};        // lf, rf, lb, rb in car space
    bool wheelFound[4] = {false, false, false, false};
};

struct VehicleInstance {
    int model = 0;
    float x = 0, y = 0, z = 0, heading = 0;
    float wheelSpin = 0.0f;
    float speed = 0.0f;               // m/s along the heading, negative in reverse
    float steer = 0.0f;               // current front wheel angle, radians
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
    int Nearest(float x, float y, float range) const;

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
};
