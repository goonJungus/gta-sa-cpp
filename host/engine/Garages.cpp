#define NOMINMAX
#include "Garages.h"

#include "Stats.h"
#include "Vehicle.h"
#include "World.h"

#include <algorithm>
#include <cmath>

namespace {
// CVehicle::IsLawEnforcementVehicle
bool LawEnforcement(int m) {
    switch (m) {
    case 427: case 430: case 432: case 433: case 490: case 523: case 528: case 596: case 597: case 598: case 599: case 601: return true;
    default: return false;
    }
}
bool Bmx(int m) { return m == 481 || m == 509 || m == 510; }
// CGarages::IsCarSprayable: not the law, not a BMX, not a bus, coach, articulated trailer, fire truck or ambulance
bool Sprayable(int m) {
    if (LawEnforcement(m) || Bmx(m)) return false;
    return m != 431 && m != 437 && m != 435 && m != 407 && m != 416;
}
// The vehicle types FindPlayerVehicle's checks accept: CAutomobile (with its heli, plane, monster truck and quad
// kinds) and CBike (with the BMX).
bool CarOrBike(const Vehicles& v, int model) {
    const VehicleInfo* info = v.Info(model);
    if (!info) return false;
    std::string t = info->type;
    for (char& ch : t) ch = (char)std::tolower((unsigned char)ch);
    return t != "boat" && t != "train";
}
}  // namespace

void Garages::Init(GameWorld& world) {
    m_garages.clear();
    for (const IplGarage& d : world.GarageDefs()) {   // CGarages::AddOne
        Garage g;
        g.name = d.name;
        g.type = d.type;
        g.flags = d.flags;
        g.pos[0] = d.x1; g.pos[1] = d.y1; g.pos[2] = d.z1;
        float ax = d.x2 - d.x1, ay = d.y2 - d.y1, bx = d.x3 - d.x1, by = d.y3 - d.y1;
        g.width = std::sqrt(ax * ax + ay * ay);
        g.height = std::sqrt(bx * bx + by * by);
        if (g.width < 1e-3f || g.height < 1e-3f) continue;
        g.dirA[0] = ax / g.width; g.dirA[1] = ay / g.width;
        g.dirB[0] = bx / g.height; g.dirB[1] = by / g.height;
        g.topZ = d.topZ;
        // CGarage::InitDoorsAtStart: the bomb shops and the Pay 'n' Spray stand open, the rest shut
        const bool open = g.type >= 2 && g.type <= 5;
        g.state = open ? kOpen : kClosed;
        g.door = open ? 1.0f : 0.0f;
        m_garages.push_back(std::move(g));
    }
    // CGarages::FindGarageForObject: a door belongs to the garage it is within 7 m of whose middle is nearest
    for (size_t inst : world.GarageDoorInsts()) {
        float p[3];
        world.InstPosition(inst, p);
        int best = -1;
        float bestD = 99999.9f;
        for (size_t k = 0; k < m_garages.size(); k++) {
            const Garage& g = m_garages[k];
            if (!PointInside(g, p, 7.0f)) continue;
            const float hw = g.width * 0.5f, hh = g.height * 0.5f;
            const float dx = p[0] - (g.dirB[0] * hh + g.dirA[0] * hw + g.pos[0]);
            const float dy = p[1] - (g.dirB[1] * hh + g.dirA[1] * hw + g.pos[1]);
            const float dz = p[2] - g.pos[2];
            const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < bestD) { bestD = dist; best = (int)k; }
        }
        if (best < 0) continue;
        // CObject::ProcessGarageDoorBehaviour: the door's travel is its collision height less 0.1 m
        m_garages[(size_t)best].doors.push_back({inst, p[2], std::max(0.0f, world.InstColHeight(inst) - 0.1f)});
    }
    size_t doors = 0, sprays = 0;
    for (const Garage& g : m_garages) { doors += g.doors.size(); sprays += g.type == kPayNSpray; PlaceDoors(world, g); }
    std::fprintf(stderr, "[Garages] %zu garages (%zu Pay 'n' Spray), %zu doors\n", m_garages.size(), sprays, doors);
}

// CGarage::IsPointInsideGarage(point, radius)
bool Garages::PointInside(const Garage& g, const float p[3], float r) {
    if (p[2] < g.pos[2] - r || p[2] > g.topZ + r) return false;
    const float dx = p[0] - g.pos[0], dy = p[1] - g.pos[1];
    const float a = dx * g.dirA[0] + dy * g.dirA[1], b = dx * g.dirB[0] + dy * g.dirB[1];
    return a >= -r && a <= g.width + r && b >= -r && b <= g.height + r;
}

// IsStaticPlayerCarEntirelyInside: a car or bike, its driver not getting out, still (under 0.01 per step on every
// axis), its body inside the garage (the footprint corners at its height stand in for the col spheres).
bool Garages::CarEntirelyInside(const Garage& g, const Env& e) const {
    if (e.playerCar < 0 || e.leavingCar || !e.cars) return false;
    Vehicles& v = *e.cars;
    const VehicleInstance& c = v.Car((size_t)e.playerCar);
    if (!CarOrBike(v, c.model)) return false;
    float mv[3] = {c.vx / 50.0f, c.vy / 50.0f, 0.0f};
    if (c.phys.active) for (int k = 0; k < 3; k++) mv[k] = c.phys.mv[k];
    for (float m : mv) if (std::fabs(m) > 0.01f) return false;
    if (mv[0] * mv[0] + mv[1] * mv[1] + mv[2] * mv[2] > 0.0001f) return false;
    float hx, y0, y1, m[16];
    if (!v.Footprint((size_t)e.playerCar, &hx, &y0, &y1)) return false;
    v.BodyMatrix((size_t)e.playerCar, m);
    for (int i = 0; i < 4; i++) {
        const float lx = i & 1 ? hx : -hx, ly = i & 2 ? y1 : y0;
        const float p[3] = {m[12] + m[0] * lx + m[4] * ly, m[13] + m[1] * lx + m[5] * ly, m[14] + m[2] * lx + m[6] * ly};
        if (!PointInside(g, p, 0.0f)) return false;
    }
    return true;
}

// IsPlayerOutsideGarage: the player's car (or the player) entirely clear of the garage
bool Garages::PlayerOutside(const Garage& g, const Env& e) const {
    if (e.playerCar < 0 || !e.cars) return !PointInside(g, e.player, 0.5f);
    Vehicles& v = *e.cars;
    float hx, y0, y1, m[16];
    if (!v.Footprint((size_t)e.playerCar, &hx, &y0, &y1)) return true;
    v.BodyMatrix((size_t)e.playerCar, m);
    for (int i = 0; i < 4; i++) {
        const float lx = i & 1 ? hx : -hx, ly = i & 2 ? y1 : y0;
        const float p[3] = {m[12] + m[0] * lx + m[4] * ly, m[13] + m[1] * lx + m[5] * ly, m[14] + m[2] * lx + m[6] * ly};
        if (PointInside(g, p, 0.0f)) return false;
    }
    return true;
}

// SlideDoorOpen / SlideDoorClosed: 0.011 (0.0011 for the hangars, types 44 and 45) up per step, 0.013 down.
bool Garages::SlideOpen(Garage& g, float ts) {
    g.door += ts * (g.type == 44 || g.type == 45 ? 0.0011f : 0.011f);
    if (g.door < 1.0f) return false;
    g.door = 1.0f;
    return true;
}
bool Garages::SlideClosed(Garage& g, float ts) {
    g.door -= ts * (g.type == 44 || g.type == 45 ? 0.0013f : 0.013f);
    if (g.door > 0.0f) return false;
    g.door = 0.0f;
    return true;
}

// CObject::ProcessGarageDoorBehaviour, sliding doors: up by 0.9090909 of the travel when open (the Area 69 hangar,
// type 44, sinks instead). The rotating doors (flag 8) are not in the map data.
void Garages::PlaceDoors(GameWorld& world, const Garage& g) {
    for (const Door& d : g.doors)
        world.SetInstZ(d.inst, g.type == 44 ? d.baseZ - d.height * g.door : d.baseZ + d.height * g.door * 0.9090909f);
}

bool Garages::Update(GameWorld& world, Env& e) {
    for (Garage& g : m_garages) {
        const float before = g.door;
        if (g.type == kPayNSpray) UpdatePayNSpray(g, e);
        if (g.door != before) PlaceDoors(world, g);
    }
    return m_controlsOff;
}

void Garages::UpdatePayNSpray(Garage& g, Env& e) {
    if (e.player[2] >= 950.0f) return;   // in an interior
    Vehicles* v = e.cars;
    switch (g.state) {
    case kClosed: {
        if (noResprays || e.nowMs <= g.timeToOpen) return;
        g.state = kOpening;
        bool charged = false;
        const bool wasWanted = e.wanted && *e.wanted > 0;
        if (wasWanted) { charged = true; *e.wanted = 0; }   // CWanted::ClearWantedLevelAndGoOnParole
        bool colours = wasWanted;
        if (v && e.playerCar >= 0 && CarOrBike(*v, v->Car((size_t)e.playerCar).model)) {
            const size_t ci = (size_t)e.playerCar;
            VehicleInstance& c = v->Car(ci);
            if (c.health < 970.0f) charged = true;
            c.health = std::max(1000.0f, c.health);
            v->Fix(ci);
            Stats::Increment(Stats::STAT_VEHICLE_RESPRAYS, 1.0f);
            if (c.phys.active && c.rot[8] < 0.0f)   // upside down: turned over about its nose
                for (int k : {0, 1, 2, 6, 7, 8}) c.rot[k] = -c.rot[k];
            colours = v->NextColours(ci);
        }
        if (g.flags & 0x80) {
            if (e.message) e.message("GA_22");
        } else if (charged && !respraysFree) {
            if (e.money && *e.money > 0) *e.money = std::max(0, *e.money - 100);
            Stats::Increment(Stats::STAT_AUTO_REPAIR_AND_PAINTING_BUDGET, 100.0f);
            if (e.message) e.message(wasWanted ? "GA_2" : "GA_XX");
        } else if (colours) {
            if (e.message) e.message(std::rand() & 1 ? "GA_15" : "GA_16");
        }
        return;
    }
    case kOpen:
        if (noResprays) return;
        if (CarEntirelyInside(g, e)) {
            const int model = v->Car((size_t)e.playerCar).model;
            if (!Sprayable(model)) {
                if (e.message) e.message(Bmx(model) ? "GA_1B" : "GA_1");
                g.state = kWaitingPlayerToExit;
            } else if (e.money && *e.money < 100 && !respraysFree) {
                if (e.message) e.message("GA_3");
                g.state = kWaitingPlayerToExit;
            } else {
                g.state = kClosing;
                m_controlsOff = true;
            }
        }
        return;
    case kClosing:
        if (v && e.playerCar >= 0) v->Car((size_t)e.playerCar).burnTime = 0.0f;
        if (SlideClosed(g, e.ts)) {
            g.state = kClosed;
            g.timeToOpen = e.nowMs + 2000;
        }
        return;
    case kOpening:
        if (SlideOpen(g, e.ts)) g.state = kWaitingPlayerToExit;
        if (g.door > 0.5f) m_controlsOff = false;
        return;
    case kWaitingPlayerToExit:
        if (PlayerOutside(g, e)) g.state = kOpen;
        return;
    }
}
