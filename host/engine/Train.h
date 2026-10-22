#pragma once
// CTrain: rails from data/paths/tracks*.dat and the mission trains that run on them.
//
// A track is a closed loop of nodes (x, y, z in 1/8 m, plus the distance from the start in 1/3 m). A carriage
// holds a distance along its track and a speed; the engine of a mission train accelerates toward its cruise
// speed, the other carriages follow it at fixed spacing (CTrain::ProcessControl, CreateMissionTrain).

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
    // One frame (dt seconds): control of every engine, then every carriage is placed on its rail.
    void Update(Vehicles& v, float dt);

private:
    std::vector<TrainNode> m_track[4];
    float m_total[4] = {};
    std::vector<TrainCarriage> m_cars;
    static bool ReadTrack(const std::string& path, std::vector<TrainNode>& nodes, float* total);
    void Place(Vehicles& v, TrainCarriage& c, float dt);
    void FindPositionOnTrack(Vehicles& v, TrainCarriage& c, const float pos[3]);
    float ModelLength(Vehicles& v, int model) const;
};
