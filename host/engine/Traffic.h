#pragma once
// Road traffic: cars spawned on the lanes of the path network around the player (CCarCtrl::GenerateOneRandomCar),
// each following its lane from node to node (CCarAI / CCarCtrl::SteerAICarWithPhysics). The lane is a navi link's
// start point and direction; the car aims a little way ahead on the line to the next link's start, picks that next
// link among the ones that leave the node going its way, slows for the turn and for what is in front of it.

#include <functional>
#include <vector>

#include "PathNodes.h"
#include "PopCycle.h"
#include "Peds.h"
#include "Vehicle.h"

class Traffic {
public:
    struct Env {
        float dt = 0.0f;
        float px = 0, py = 0, pz = 0;       // the player
        float camFx = 0, camFy = 1;         // the camera's forward, flat
        int playerCar = -1;
        int wanted = 0;                     // the player's wanted level (stars)
        Peds* peds = nullptr;               // the drivers
        const PopCycle* pop = nullptr;      // which cars, and how many, for the zone and the hour
        float hour = 12.0f;
        bool weekend = false;
        uint32_t tlMs = 0;                  // the signals' clock (CTrafficLights runs on wall time)
        // road height under (x, y) near z, below -1e5 when the world is not there yet
        std::function<float(float x, float y, float z)> ground;
    };

    int maxCars = 12;                       // CCarCtrl::MaxNumberOfCarsInUse by density, a fixed number for now
    void Update(Vehicles& v, D3DRenderer& r, const PathNodes& paths, const Env& e);
    size_t Count() const { return m_drives.size(); }
    int FirstCar() const { return m_drives.empty() ? -1 : m_drives[0].car; }   // dev: a car to point a test camera at
    void Clear() { m_drives.clear(); }
    template <class F> void EachCar(F f) const { for (const Drive& d : m_drives) f(d.car); }

private:
    struct Drive {
        int car = -1;
        int driverUid = 0;                  // the Peds actor at the wheel (0 none)
        PathNodes::Address from, to;        // the link it is on: lane start node and target node
        float sx = 0, sy = 0, dx = 0, dy = 1, len = 1;   // the lane's start, unit direction and length to the next lane start
        PathNodes::Address nextTo;          // where the next link goes
        float nx = 0, ny = 0, ndx = 0, ndy = 1, nlen = 1;   // the next link: its start, direction, length
        bool haveNext = false;
        int tl = 0, ntl = 0;                // the link's signal axis (0 none, 1 / 2 the axis, 3 both) and
        bool tlDir = false, ntlDir = false; //   whether the link runs the way the light faces: it arrives at the signal
        float cruise = 12.0f;               // m/s
        float dbgWant = 0.0f, dbgErr = 0.0f, dbgHit = -1.0f, dbgLat = 0.0f;
        float shock = 0.0f;                 // seconds left standing after a knock
        int thiefUid = 0;                   // a passer-by on his way to steal the parked car
        float thiefScan = 0.0f;
        float yieldT = 0.0f;                // seconds spent giving way at a junction
        float blockedT = 0.0f, passT = 0.0f;   // held up behind a slow car this long; overtaking for this much longer
        bool cop = false;                   // a police car after the player
        bool parked = false;                // abandoned: the driver left it where it stopped
        bool bail = false;                  // the driver gets out when the shock is over
        float stuck = 0.0f, idle = 0.0f, age = 0.0f;   // stuck: pushing but not moving; idle: not moving at all
    };
    std::vector<Drive> m_drives;
    float m_spawnTimer = 0.0f, m_copTimer = 0.0f;
    // The SWAT helicopter (CHeli::SendDownSwat): at four stars a police Maverick hovers over the player and four officers go down
    // its rope, then it leaves.
    int m_swatHeli = -1, m_swatDropped = 0;
    float m_swatTimer = 0.0f;
    bool m_swatLeaving = false;
    void UpdateSwat(Vehicles& v, D3DRenderer& r, const Env& e);
    unsigned m_rng = 0x7a11c5u;

    float Rand() { m_rng = m_rng * 1664525u + 1013904223u; return (float)(m_rng >> 8) / 16777216.0f; }
    bool PickLink(const PathNodes& paths, PathNodes::Address node, PathNodes::Address avoid, PathNodes::Address* to, float* sx, float* sy,
                  float* dx, float* dy, int* tl = nullptr, bool* tlDir = nullptr);
    bool Spawn(Vehicles& v, D3DRenderer& r, const PathNodes& paths, const Env& e, int forceModel = -1);
    void Pursue(Vehicles& v, Drive& d, const Env& e);
    void Drive_(Vehicles& v, const PathNodes& paths, Drive& d, const Env& e);
};
