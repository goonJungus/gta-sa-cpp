#pragma once
// Parked cars from the IPL car generators (CTheCarGenerators / CCarGenerator).
//
// A generator makes its car once the player is within 160 m, normally at the
// edge of that range (140 m or more) while heading towards it, so the car never
// pops in close by; right after loading everything in range generates at once.
// A spot with no model picks a random car of up to 8 m. At most 5 parked cars
// stand by day and 10 at night unless the spot ignores the limit. Once the
// player takes a car the spot rests a minute; a car nobody touched goes away
// again past 180 m and its spot may make another.

#include <cstdint>
#include <functional>
#include <vector>

#include "BinaryIpl.h"

class D3DRenderer;
class Vehicles;

class CarGenerators {
public:
    void Init(const std::vector<BinIplCarGen>& gens);
    // groundZ(x, y, fromZ) gives the ground under a point (below -1e5 if none). playerVel in m/s.
    void Process(Vehicles& v, D3DRenderer& r, const float player[3], const float playerVel[2], int hour, uint32_t nowMs,
                 const std::function<float(float, float, float)>& groundZ);
    size_t Count() const { return m_gens.size(); }
    int ParkedCars() const { return m_parked; }

private:
    struct Gen {
        BinIplCarGen file;
        int car = -1;                 // m_nVehicleHandle
        int randomModel = 0;          // the model a random spot settled on
        uint32_t nextGenMs = 0;       // m_nNextGenTime
        bool waitUntilFar = false;    // bWaitUntilFarFromPlayer
    };
    std::vector<Gen> m_gens;
    int m_generateClose = 120;        // GenerateEvenIfPlayerIsCloseCounter: passes that may generate close by
    int m_parked = 0;                 // CCarCtrl::NumParkedCars
};
