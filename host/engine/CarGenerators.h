#pragma once
// Parked cars from the IPL car generators (CTheCarGenerators / CCarGenerator).
//
// A generator makes its car once the player is within 160 m, normally at the
// edge of that range (140 m or more) while heading towards it, so the car never
// pops in close by; right after loading everything in range generates at once.
// A spot with no model picks a random car of up to 8 m. At most 5 parked cars
// stand by day and 10 at night unless the spot ignores the limit. Once the
// player takes a car the spot rests a minute; a car nobody touched goes away
// again past 180 m and its spot may make another. The script adds generators of
// its own (CREATE_CAR_GENERATOR), which stay off until it switches them on.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "BinaryIpl.h"

class D3DRenderer;
class Vehicles;

class CarGenerators {
public:
    // The IPL generators, switched on (CFileLoader::LoadCarGenerator). Script generators made before are kept.
    void Init(const std::vector<BinIplCarGen>& gens);
    // CTheCarGenerators::CreateCarGenerator for the script: angle in degrees, -1 model for a random car, -1 colours
    // for the model's own. Returns the handle, or -1 for a model that is no vehicle. It starts switched off.
    int CreateScript(float x, float y, float z, float angleDeg, int model, int colour1, int colour2, bool forceSpawn,
                     int alarmChance, int doorLockChance, int minDelay, int maxDelay, const std::string& plate = {});
    // SWITCH_CAR_GENERATOR: 0 off, 1..100 that many cars, above that for ever.
    void SwitchScript(int handle, int count, uint32_t nowMs);
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
        int count = -1;               // m_nGenerateCount: 0 off, -1 for ever
        std::string plate;            // CSpecialPlateHandler: the script's plate text for its cars
    };
    std::vector<Gen> m_gens;          // the IPL generators, then the script's
    size_t m_iplCount = 0;
    int m_generateClose = 120;        // GenerateEvenIfPlayerIsCloseCounter: passes that may generate close by
    int m_parked = 0;                 // CCarCtrl::NumParkedCars
};
