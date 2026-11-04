#pragma once
// CInterestingEvents: the eight things near the player worth a look. The world reports them (a cop at work,
// an explosion, a fight, a car crash...) and the idle camera picks one to watch. The rules follow
// CInterestingEvents::Add, GetInterestingEvent, InvalidateEvent, InvalidateNonVisibleEvents and
// ScanForNearbyEntities, with the priority and delay tables of its constructor.

#include <cstdint>
#include <functional>
#include <vector>

class InterestingEvents {
public:
    static constexpr int kTypes = 29;
    // EType: what the event is about. The ones the world here reports; the rest keep their table rows.
    enum EType {
        None = 0, Sunbather = 2, Attractor = 3, Prostitute = 4, Cop = 5, Criminal = 6, Gang = 7, SexyVehicle = 8,
        Medic = 11, Flee = 13, Vehicle = 14, PedDamaged = 15, KnockOffBike = 16, CarCrash = 17, RoadRage = 18,
        Melee = 19, PedEnteredVehicle = 20, Helicopter = 21, Gunfire = 22, KillCriminal = 25, Explosion = 27, PedDead = 28
    };
    // The entity an event is about. A ped by its uid, a car by its index, the player, or a point in the world.
    struct Ref {
        enum Kind { NoEntity = 0, Ped, Car, Player, Point };
        int kind = NoEntity, id = -1;
        float pos[3] = {0, 0, 0};   // Point only
        bool Empty() const { return kind == NoEntity; }
        bool operator==(const Ref& o) const { return kind == o.kind && id == o.id; }
    };
    struct Event {
        int type = 0;
        uint32_t time = 0;
        Ref ref;
    };
    using LosFn = std::function<bool(const float* a, const float* b)>;   // clear from a to b
    // Where an entity is now: false when it is gone. ped: it is a ped; lookUp: how far above its origin to look.
    using ResolveFn = std::function<bool(const Ref&, float pos[3], bool* ped, float* lookUp)>;
    // What a call needs of the camera and the world.
    struct Frame {
        uint32_t now = 0;                 // CTimer::m_snTimeInMilliseconds
        float cam[3] = {0, 0, 0};         // where the camera is
        float player[3] = {0, 0, 0};
        float playerFwd[3] = {0, 1, 0};
        LosFn los;
        ResolveFn resolve;
    };
    // One thing the scan looks at.
    struct Candidate {
        Ref ref;
        float pos[3] = {0, 0, 0};
        int pedType = -1;                 // ePedType, -1 for a vehicle
        bool aircraft = false;            // a helicopter
    };

    InterestingEvents();                                     // the constructor's tables
    void Add(int type, const Ref& ref, const Frame& f);      // CInterestingEvents::Add
    const Event* Get(uint32_t now);                          // GetInterestingEvent
    void Invalidate(const Event* e);                         // InvalidateEvent
    void InvalidateNonVisible(const Frame& f);               // InvalidateNonVisibleEvents
    bool ScanDue(uint32_t now) const { return now - m_lastScan > 499; }
    void Scan(const Frame& f, const std::vector<Candidate>& around);   // ScanForNearbyEntities
    const Event& Slot(int i) const { return m_events[(size_t)i]; }
    int Used() const { int n = 0; for (const Event& e : m_events) n += e.ref.Empty() ? 0 : 1; return n; }
    int Priority(int type) const { return m_priority[(size_t)type]; }
    int Delay(int type) const { return m_delay[(size_t)type]; }

private:
    Event m_events[8];
    int m_current = -1;               // m_nInterestingEvent
    uint32_t m_lastScan = 0;
    float m_radius = 30.0f;
    uint8_t m_priority[kTypes] = {};
    uint16_t m_delay[kTypes] = {};
    uint32_t m_endOfTime[kTypes] = {};
    float m_dir[3] = {0, 1, 0}, m_centre[3] = {0, 0, 0};
    unsigned m_rng = 12345u;
};
