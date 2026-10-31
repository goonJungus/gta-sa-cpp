#pragma once
// CTrain: rails from data/paths/tracks*.dat and the mission trains that run on them.
//
// A track is a closed loop of nodes (x, y, z in 1/8 m, plus the distance from the start in 1/3 m). A carriage
// holds a distance along its track and a speed; the engine accelerates toward its cruise speed (stopping at the
// stations) or as the player drives it, the other carriages follow it at fixed spacing (CTrain::ProcessControl,
// CreateMissionTrain). Random trains come and go around the player (DoTrainGenerationAndRemoval); one driven
// too fast into a bend leaves the rails and becomes an ordinary physical body.

#include <cstdint>
#include <string>
#include <vector>

#include "Vehicle.h"

struct TrainNode {
    int16_t x = 0, y = 0, z = 0;       // 1/8 m
    uint16_t dist = 0;                 // distance from the start of the track, 1/3 m
};

struct TrainCarriage {
    int car = 0;                       // Vehicles index
    int track = 0, node = 0;
    float rail = 0.0f;                 // metres along the track
    float length = 0.0f;               // spacing from the previous carriage (negative when clockwise)
    float speed = 0.0f;                // metres per time step (1/50 s), signed
    float gas = 0.0f, brake = 0.0f;
    float cruise = 0.0f;               // m_autoPilot.m_nCruiseSpeed
    bool clockwise = true, engine = false, last = false, mission = false, removed = false;
    // trainFlags: bStopsAtStations, bStoppedAtStation, bPassengersCanEnterAndLeave, bIsDerailed
    bool stopsAtStations = false, stopped = false, doorsOpen = false, derailed = false;
    uint32_t stoppedMs = 0;            // m_nTimeWhenStoppedAtStation
    int prev = -1, next = -1;          // indices into Trains::m_cars
};

class Trains {
public:
    // Read the four track files; false when tracks.dat is missing.
    bool Load(const std::string& gameDir);
    bool Loaded() const { return !m_track[0].empty(); }
    size_t NodeCount(int track) const { return m_track[track].size(); }
    float TotalLength(int track) const { return m_total[track]; }
    const TrainNode& Node(int track, size_t i) const { return m_track[track][i]; }

    // CTrain::FindClosestTrackNode: the nearest node over all four tracks.
    int FindClosestTrackNode(const float p[3], int* outTrack) const;
    // CTrain::FindCoorsFromPositionOnTrack.
    bool CoorsFromRail(float rail, int track, float out[3]) const;

    // CTrain::CreateMissionTrain: the carriages of a train type spawned on the nearest track node. Returns the
    // engine's index into the carriage list, or -1.
    int CreateMissionTrain(Vehicles& v, D3DRenderer& r, const float pos[3], bool clockwise, int type);
    // The same at a given track node (a generated random train: not a mission train, stops at stations).
    int CreateTrainAtNode(Vehicles& v, D3DRenderer& r, int track, int node, bool clockwise, int type, bool mission);
    void SetTrainSpeed(int t, float speed);              // CTrain::SetTrainSpeed (script units, 0.02 per step)
    void SetTrainCruiseSpeed(int t, float speed);
    void RemoveMissionTrains(Vehicles& v);               // CTrain::RemoveMissionTrains
    void ReleaseMissionTrains();                         // CTrain::ReleaseMissionTrains
    void RemoveTrain(Vehicles& v, int t);                // CTrain::RemoveOneMissionTrain: this carriage and the ones behind it
    void ReleaseTrain(int t);
    int FindCaboose(int t) const;
    int FindCarriage(int t, int n) const;
    int CarriageOfCar(int car) const;                    // carriage index for a Vehicles index, or -1
    int FindNumCarriagesPulled(int t) const;
    const std::vector<TrainCarriage>& Cars() const { return m_cars; }
    TrainCarriage& At(int i) { return m_cars[(size_t)i]; }
    // What a frame needs from the game: the clock, the camera and player, the train the player drives (a car
    // index, -1 none) with the pad's accelerate / brake (0..1), and whether random trains may appear.
    struct Frame {
        uint32_t nowMs = 0, prevMs = 0;
        float cam[3] = {0, 0, 0}, player[3] = {0, 0, 0};
        int playerCar = -1;
        float accel = 0.0f, brake = 0.0f;
        bool randomTrains = true;
    };
    enum { kShake = 1, kDerailed = 2 };
    // One frame (dt seconds): random trains come and go, every engine is driven, every carriage is placed on its
    // rail. Returns kShake while the player's train runs near its top speed, kDerailed the frame it leaves the rails.
    int Update(Vehicles& v, D3DRenderer& r, float dt, const Frame& in);
    bool IsDerailed(int car) const { const int t = CarriageOfCar(car); return t >= 0 && m_cars[(size_t)t].derailed; }

private:
    std::vector<TrainNode> m_track[4];
    float m_total[4] = {};
    std::vector<TrainCarriage> m_cars;
    static bool ReadTrack(const std::string& path, std::vector<TrainNode>& nodes, float* total);
    void Place(Vehicles& v, TrainCarriage& c, float dt);
    void DriveEngine(int i, float ts, const Frame& in);
    bool MaxSpeedToStopAtStations(const TrainCarriage& c, float* speed) const;
    void GenerateAndRemove(Vehicles& v, D3DRenderer& r, const Frame& in);
    float m_stationDist[6] = {};       // StationDist: each station's distance along track 0
    int m_genStatus = 0, m_genTrack = 0, m_genNode = 0, m_genConfig = 0, m_lastConfig = 0;   // GenTrain_*
    bool m_genClockwise = false, m_nearStation = false;
    void FindPositionOnTrack(Vehicles& v, TrainCarriage& c, const float pos[3]);
    float ModelLength(Vehicles& v, int model) const;
};
