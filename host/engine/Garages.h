#pragma once
// CGarages / CGarage: the garages of the map IPLs ("grge") and their doors. The Pay 'n' Spray runs as retail's
// CGarage::Update PAYNSPRAY case: drive in and stop, the door comes down, two seconds later it goes up on a repaired,
// resprayed car with the wanted level cleared, for $100. The other garage types keep their doors as
// CGarage::InitDoorsAtStart leaves them.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class GameWorld;
class Vehicles;

class Garages {
public:
    enum Type { kPayNSpray = 5 };
    enum DoorState { kClosed = 0, kOpen = 1, kClosing = 2, kOpening = 3, kWaitingPlayerToExit = 4 };

    // Builds the garages from the world's IPL entries and gives each its door placements.
    void Init(GameWorld& world);

    struct Env {
        Vehicles* cars = nullptr;
        int playerCar = -1;            // the car the player drives, -1 on foot
        float player[3] = {0, 0, 0};   // the player's feet
        bool leavingCar = false;       // TASK_COMPLEX_LEAVE_CAR: not counted as parked inside
        int* money = nullptr;
        int* wanted = nullptr;
        std::function<void(const char*)> message;   // CGarages::TriggerMessage: a GXT key, 4 s
        uint32_t nowMs = 0;
        float ts = 1.0f;               // CTimer::ms_fTimeStep (1 = 20 ms)
    };
    // CGarages::Update. Returns true while the player's controls are taken (the door is coming down on him).
    bool Update(GameWorld& world, Env& e);

    bool respraysFree = false;     // CGarages::RespraysAreFree (SET_GARAGE_RESPRAY_FREE)
    bool noResprays = false;       // CGarages::NoResprays

    size_t Count() const { return m_garages.size(); }
    int State(size_t i) const { return m_garages[i].state; }
    float DoorPosition(size_t i) const { return m_garages[i].door; }
    const std::string& Name(size_t i) const { return m_garages[i].name; }
    int TypeOf(size_t i) const { return m_garages[i].type; }

private:
    struct Door { size_t inst; float baseZ, height; };
    struct Garage {
        std::string name;
        int type = 0, flags = 0;
        float pos[3] = {0, 0, 0};          // m_vPosn
        float dirA[2] = {1, 0}, dirB[2] = {0, 1};
        float width = 0, height = 0, topZ = 0;
        int state = kClosed;
        float door = 0.0f;                 // m_fDoorPosition: 0 shut .. 1 open
        uint32_t timeToOpen = 0;
        std::vector<Door> doors;
    };
    std::vector<Garage> m_garages;
    bool m_controlsOff = false;

    static bool PointInside(const Garage& g, const float p[3], float r);
    bool CarEntirelyInside(const Garage& g, const Env& e) const;
    bool PlayerOutside(const Garage& g, const Env& e) const;
    bool SlideOpen(Garage& g, float ts);
    bool SlideClosed(Garage& g, float ts);
    void UpdatePayNSpray(Garage& g, Env& e);
    static void PlaceDoors(GameWorld& world, const Garage& g);
};
