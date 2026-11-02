#pragma once
// Road traffic: cars spawned on the lanes of the path network around the player (CCarCtrl::GenerateOneRandomCar),
// each following its lane from node to node (CCarAI / CCarCtrl::SteerAICarWithPhysics). The lane is a navi link's
// start point and direction; the car aims a little way ahead on the line to the next link's start, picks that next
// link among the ones that leave the node going its way, slows for the turn and for what is in front of it.

#include <functional>
#include <vector>

#include "PathNodes.h"
#include "Vehicle.h"

class Traffic {
public:
    struct Env {
        float dt = 0.0f;
        float px = 0, py = 0, pz = 0;       // the player
        float camFx = 0, camFy = 1;         // the camera's forward, flat
        int playerCar = -1;
        // road height under (x, y) near z, below -1e5 when the world is not there yet
        std::function<float(float x, float y, float z)> ground;
    };

    int maxCars = 12;                       // CCarCtrl::MaxNumberOfCarsInUse by density, a fixed number for now
    void Update(Vehicles& v, D3DRenderer& r, const PathNodes& paths, const Env& e);
    size_t Count() const { return m_drives.size(); }
    void Clear() { m_drives.clear(); }

private:
    struct Drive {
        int car = -1;
        PathNodes::Address from, to;        // the link it is on: lane start node and target node
        float sx = 0, sy = 0, dx = 0, dy = 1, len = 1;   // the lane's start, unit direction and length to the next lane start
        PathNodes::Address nextTo;          // where the next link goes
        float nx = 0, ny = 0, ndx = 0, ndy = 1, nlen = 1;   // the next link: its start, direction, length
        bool haveNext = false;
        float cruise = 12.0f;               // m/s
        float dbgWant = 0.0f, dbgErr = 0.0f, dbgHit = -1.0f, dbgLat = 0.0f;
        float stuck = 0.0f, idle = 0.0f, age = 0.0f;   // stuck: pushing but not moving; idle: not moving at all
    };
    std::vector<Drive> m_drives;
    float m_spawnTimer = 0.0f;
    unsigned m_rng = 0x7a11c5u;

    float Rand() { m_rng = m_rng * 1664525u + 1013904223u; return (float)(m_rng >> 8) / 16777216.0f; }
    bool PickLink(const PathNodes& paths, PathNodes::Address node, PathNodes::Address avoid, PathNodes::Address* to, float* sx, float* sy,
                  float* dx, float* dy);
    bool Spawn(Vehicles& v, D3DRenderer& r, const PathNodes& paths, const Env& e);
    void Drive_(Vehicles& v, const PathNodes& paths, Drive& d, const Env& e);
};
