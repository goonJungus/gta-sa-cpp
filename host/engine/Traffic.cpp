#include "Traffic.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

// The ordinary cars of the road (cargrp.dat's civilian groups, without the zone weighting for now).
const int kModels[] = {401, 404, 405, 410, 412, 418, 419, 420, 421, 426, 436, 439, 445, 458, 466, 467, 474, 475, 479, 491, 492, 496, 500,
                       507, 516, 517, 518, 526, 527, 529, 533, 540, 542, 546, 547, 549, 550, 551, 566, 576, 585, 587, 589, 600, 602, 603};

float Wrap(float a) {
    while (a > 3.14159265f) a -= 6.2831853f;
    while (a < -3.14159265f) a += 6.2831853f;
    return a;
}

}  // namespace

// The links that leave `node` the way traffic goes (a lane in its own direction), not back to `avoid` unless it is
// the only way out: one chosen at random, its lane start and direction.
bool Traffic::PickLink(const PathNodes& paths, PathNodes::Address node, PathNodes::Address avoid, PathNodes::Address* to, float* sx, float* sy,
                       float* dx, float* dy, int* tl, bool* tlDir) {
    const PathNodes::Node* n = paths.Get(node);
    if (!n) return false;
    struct Cand { PathNodes::Address to; const PathNodes::CarLink* lane; };
    std::vector<Cand> ok, back;
    for (int i = 0; i < n->numLinks; i++) {
        PathNodes::Address t;
        float len = 0.0f;
        const PathNodes::CarLink* lane = nullptr;
        if (!paths.Neighbour(node, i, &t, &len, &lane) || !lane || lane->sameLanes <= 0) continue;
        const PathNodes::Node* tn = paths.Get(t);
        if (!tn || tn->switchedOff || tn->water || !tn->vehicle) continue;
        (t == avoid ? back : ok).push_back({t, lane});
    }
    std::vector<Cand>& use = ok.empty() ? back : ok;
    if (use.empty()) return false;
    const Cand& c = use[(size_t)(Rand() * (float)use.size()) % use.size()];
    *to = c.to; *sx = c.lane->x; *sy = c.lane->y; *dx = c.lane->dx; *dy = c.lane->dy;
    if (tl) *tl = c.lane->trafficLightState;
    if (tlDir) *tlDir = c.lane->trafficLightDirection;
    return true;
}

bool Traffic::Spawn(Vehicles& v, D3DRenderer& r, const PathNodes& paths, const Env& e, int forceModel) {
    const std::vector<PathNodes::Address> closeNodes = paths.NodesNear(e.px, e.py, 170.0f, 1024);
    if (closeNodes.empty()) return false;
    const size_t start = (size_t)(Rand() * (float)closeNodes.size()) % closeNodes.size();
    for (size_t k = 0; k < closeNodes.size(); k++) {
        const PathNodes::Address a = closeNodes[(start + k) % closeNodes.size()];
        const PathNodes::Node* n = paths.Get(a);
        if (!n || n->switchedOff || n->water || n->roadBlock || n->deadEnd || n->spawnProbability == 0) continue;
        const float ddx = n->x - e.px, ddy = n->y - e.py, dist = std::sqrt(ddx * ddx + ddy * ddy);
        const bool nearTest = std::getenv("GTA_TRAFFICNEAR") != nullptr;   // dev: spawn within sight
        if (forceModel >= 0) { if (dist < 55.0f || dist > 120.0f) continue; }   // a police car comes from further down the road
        else if (dist < (nearTest ? 14.0f : 85.0f) || (nearTest && dist > 40.0f)) continue;
        if (forceModel < 0 && !nearTest && dist < 140.0f && (ddx * e.camFx + ddy * e.camFy) / std::max(dist, 1.0f) > 0.3f) continue;   // not where it is seen appearing
        if (forceModel < 0 && Rand() * 15.0f > (float)n->spawnProbability) continue;
        PathNodes::Address to;
        float sx, sy, dx, dy;
        int tl = 0;
        bool tlDir = false;
        if (!PickLink(paths, a, PathNodes::Address{}, &to, &sx, &sy, &dx, &dy, &tl, &tlDir)) continue;
        const PathNodes::Node* tn = paths.Get(to);
        // the lane runs on to the start of the next link
        PathNodes::Address nextTo;
        float nx, ny, ndx, ndy;
        int ntl = 0;
        bool ntlDir = false;
        if (!PickLink(paths, to, a, &nextTo, &nx, &ny, &ndx, &ndy, &ntl, &ntlDir) || nextTo == a) continue;   // not a dead end
        const float vx = nx - sx, vy = ny - sy, segLen = std::sqrt(vx * vx + vy * vy);
        if (segLen < 6.0f) continue;
        const float ux = vx / segLen, uy = vy / segLen;
        const float frac = 0.1f + 0.5f * Rand();
        const float px = sx + ux * segLen * frac, py = sy + uy * segLen * frac;
        bool clear = true;
        for (size_t i = 0; i < v.Count() && clear; i++) {
            const VehicleInstance& o = v.Car(i);
            if (!o.removed && std::fabs(o.x - px) < 7.0f && std::fabs(o.y - py) < 7.0f) clear = false;
        }
        if (!clear) continue;
        int model = forceModel;
        if (model < 0 && e.pop && e.pop->Ready())   // CPopCycle: the zone's people and what they drive
            model = e.pop->PickCar(e.pop->TypeAt(px, py, n->z), e.hour, e.weekend, Rand(), Rand(),
                                   [&](int id) { return v.Info(id) && v.UsesCarPhysics(id) && !v.IsTrailer(id); });
        if (model < 0) model = kModels[(size_t)(Rand() * (float)(sizeof(kModels) / sizeof(kModels[0]))) % (sizeof(kModels) / sizeof(kModels[0]))];
        if (!v.Info(model)) continue;
        const float gz = e.ground ? e.ground(px, py, n->z + (tn ? (tn->z - n->z) * frac : 0.0f)) : -1e9f;
        const float z = gz > -1e5f ? gz : n->z + (tn ? (tn->z - n->z) * frac : 0.0f);
        const int ci = v.Spawn(r, model, px, py, z, std::atan2(-ux, uy));
        if (ci < 0) continue;
        VehicleInstance& c = v.Car((size_t)ci);
        c.recycle = true; c.engineOn = true; c.hasDriver = true;
        Drive d;
        d.car = ci; d.from = a; d.to = to; d.sx = sx; d.sy = sy; d.dx = dx; d.dy = dy; d.len = segLen;
        d.nextTo = nextTo; d.nx = nx; d.ny = ny; d.ndx = ndx; d.ndy = ndy; d.haveNext = true;
        d.tl = tl; d.tlDir = tlDir; d.ntl = ntl; d.ntlDir = ntlDir;
        d.cruise = (n->highway ? 20.0f : 10.0f) + 5.0f * Rand();
        if (forceModel >= 0) { d.cop = true; c.siren = true; }
        c.vx = ux * d.cruise * 0.8f; c.vy = uy * d.cruise * 0.8f;
        if (e.peds && e.peds->Ready()) {   // a driver, any civilian of the pool
            const int pi = e.peds->Spawn(r, -1, px, py, z, 0.0f);
            if (pi >= 0) d.driverUid = e.peds->Uid((size_t)pi);
        }
        m_drives.push_back(d);
        return true;
    }
    return false;
}

void Traffic::Drive_(Vehicles& v, const PathNodes& paths, Drive& d, const Env& e) {
    const float dt = e.dt;
    VehicleInstance& c = v.Car((size_t)d.car);
    const float fx = c.phys.active ? c.rot[3] : -std::sin(c.heading), fy = c.phys.active ? c.rot[4] : std::cos(c.heading);
    const float now = c.phys.active ? std::sqrt(c.phys.mv[0] * c.phys.mv[0] + c.phys.mv[1] * c.phys.mv[1]) * 50.0f : 0.0f;
    // the segment of the lane: from this link's start to the next link's start
    float ex = d.nx, ey = d.ny;
    if (!d.haveNext) { if (const PathNodes::Node* tn = paths.Get(d.to)) { ex = tn->x; ey = tn->y; } }
    float vx = ex - d.sx, vy = ey - d.sy;
    float segLen = std::sqrt(vx * vx + vy * vy);
    if (segLen < 0.5f) { vx = d.dx; vy = d.dy; segLen = 0.5f; }
    const float ux = vx / segLen, uy = vy / segLen;
    const float t = (c.x - d.sx) * ux + (c.y - d.sy) * uy;
    if (t >= segLen - 1.5f) {   // on to the next link
        if (!d.haveNext) { d.stuck = 99.0f; return; }
        d.from = d.to; d.to = d.nextTo;
        d.sx = d.nx; d.sy = d.ny; d.dx = d.ndx; d.dy = d.ndy;
        d.tl = d.ntl; d.tlDir = d.ntlDir;
        d.haveNext = paths.Get(d.to) && PickLink(paths, d.to, d.from, &d.nextTo, &d.nx, &d.ny, &d.ndx, &d.ndy, &d.ntl, &d.ntlDir) && d.nextTo != d.from;   // a dead end ends the trip
        if (!d.haveNext) d.stuck = 99.0f;
        return;
    }
    // aim a little way ahead on the line, round the corner into the next link's direction
    const float look = std::clamp(4.0f + now * 0.5f, 5.0f, 14.0f);
    float ax, ay;
    if (t + look <= segLen) { ax = d.sx + ux * (t + look); ay = d.sy + uy * (t + look); }
    else if (d.haveNext) { const float over = t + look - segLen; ax = d.nx + d.ndx * over; ay = d.ny + d.ndy * over; }
    else { ax = ex + ux * (t + look - segLen); ay = ey + uy * (t + look - segLen); }
    if (d.passT > 0.0f) {   // overtaking: the aim point swings into the lane on the right-hand side's left
        const float off = 3.4f * std::min({1.0f, (6.0f - d.passT) * 0.8f, d.passT * 0.8f});
        ax -= uy * off; ay += ux * off;
    }
    const float err = Wrap(std::atan2(ay - c.y, ax - c.x) - std::atan2(fy, fx));
    const float maxSteer = now > 14.0f ? 0.12f : now > 8.0f ? 0.25f : 0.5f;
    const float steer = std::clamp(err, -maxSteer, maxSteer);
    // the speed: the cruise, less for the turn coming up and for what is in the way
    float want = d.cruise;
    if (d.haveNext && segLen - t < 30.0f) {
        const float ang = std::acos(std::clamp(ux * d.ndx + uy * d.ndy, -1.0f, 1.0f));
        want = std::min(want, std::max(3.5f, d.cruise * (1.0f - 0.85f * std::min(1.0f, ang / 1.2f))));
    }
    // what is in the way: another car, or the player on foot, in the strip ahead of the nose
    d.dbgHit = -1.0f;
    {
        const float rx = fy, ry = -fx, reach = 8.0f + now * 1.6f;
        float blockSpeed = 1e9f;
        auto consider = [&](float ox, float oy, float halfWidth, float halfLength, float ospeed = 1e9f) {
            const float relx = ox - c.x, rely = oy - c.y;
            const float along = relx * fx + rely * fy, side = relx * rx + rely * ry;
            if (along <= 0.0f || along > reach + halfLength + 2.4f || std::fabs(side) > halfWidth + 1.1f) return;
            const float gap = along - halfLength - 2.4f;
            want = std::min(want, std::max(0.0f, gap - 1.0f) * 0.9f);
            d.dbgHit = gap; blockSpeed = ospeed;
        };
        for (size_t i = 0; i < v.Count(); i++) {
            const VehicleInstance& o = v.Car(i);
            if ((int)i == d.car || o.removed) continue;
            consider(o.x, o.y, 1.0f, 2.4f, std::fabs(o.speed));
        }
        if (e.playerCar < 0) consider(e.px, e.py, 0.4f, 0.4f);
        // CCarCtrl overtaking, reduced: stuck behind a car that is nearly stopped, with the lane to the left empty, swing round it
        if (d.passT > 0.0f) d.passT -= dt;
        if (d.dbgHit >= 0.0f && d.dbgHit < 12.0f && blockSpeed < 0.35f * d.cruise && now < 3.0f) d.blockedT += dt;
        else d.blockedT = std::max(0.0f, d.blockedT - dt);
        if (std::getenv("GTA_TRAFFICDBG") && d.blockedT > 1.0f) { std::fprintf(stderr, "[PASS] car %d blockedT %.1f gap %.1f blockSpeed %.1f passT %.1f", d.car, d.blockedT, d.dbgHit, blockSpeed, d.passT); std::fputc(10, stderr); }
        if (d.blockedT > 4.0f && d.passT <= 0.0f) {
            bool clear = true;
            for (size_t i = 0; i < v.Count() && clear; i++) {
                const VehicleInstance& o = v.Car(i);
                if ((int)i == d.car || o.removed) continue;
                const float rx2 = o.x - c.x, ry2 = o.y - c.y;
                const float along = rx2 * ux + ry2 * uy, lat = -rx2 * uy + ry2 * ux;
                if (along > -10.0f && along < 38.0f && lat > 1.2f && lat < 6.0f) clear = false;
            }
            if (clear) { d.passT = 6.0f; d.blockedT = 0.0f; }
        }
    }
    // CTrafficLights::ShouldCarStopForLight: a link that arrives at a signal holds the car short of it while its axis
    // shows amber or red (an amber too close to stop for is run)
    if (d.tl && d.tlDir) {
        const uint32_t u = (e.tlMs >> 1) & 0x3fffu;   // the car phase, as the signals themselves read it
        const int s1 = u > 4999 ? (u > 5999 ? 2 : 1) : 0;                                  // LightForCars1_Visual
        const int s2 = u < 6000 ? 2 : u > 10999 ? (u > 11999 ? 2 : 1) : 0;                  // LightForCars2_Visual
        const int st = d.tl == 1 ? s1 : d.tl == 2 ? s2 : std::max(s1, s2);
        if (const PathNodes::Node* tn = paths.Get(d.to)) {
            const float dist = std::sqrt((tn->x - c.x) * (tn->x - c.x) + (tn->y - c.y) * (tn->y - c.y));
            if (st >= 1 && dist < 45.0f && !(st == 1 && dist < 12.0f)) want = std::min(want, std::max(0.0f, dist - 8.0f) * 0.5f);
        }
    }
    if (std::fabs(err) > 1.0f) want = std::min(want, 3.0f);
    d.dbgWant = want; d.dbgErr = err; d.dbgLat = (c.x - d.sx) * -uy + (c.y - d.sy) * ux;
    float gas = 0.0f, brake = 0.0f;
    const float diff = want - now;
    if (diff <= 0.0f) brake = std::min(-diff * 0.12f + (want < 0.5f ? 0.3f : 0.0f), 1.0f);
    else gas = std::min(diff * (now >= 2.0f ? 0.125f : 0.3f), 1.0f);
    v.SetAIControls((size_t)d.car, steer, gas, brake, false);
    if (now < 0.4f && want > 1.5f) d.stuck += dt; else d.stuck = std::max(0.0f, d.stuck - dt);
    if (now < 0.4f) d.idle += dt; else d.idle = 0.0f;
}

// CCarAI police behaviour, reduced: drive straight at the player, hard, and stop alongside.
void Traffic::Pursue(Vehicles& v, Drive& d, const Env& e) {
    VehicleInstance& c = v.Car((size_t)d.car);
    const float fx = c.phys.active ? c.rot[3] : -std::sin(c.heading), fy = c.phys.active ? c.rot[4] : std::cos(c.heading);
    const float now = c.phys.active ? std::sqrt(c.phys.mv[0] * c.phys.mv[0] + c.phys.mv[1] * c.phys.mv[1]) * 50.0f : 0.0f;
    const float tx = e.px - c.x, ty = e.py - c.y, dist = std::sqrt(tx * tx + ty * ty);
    const float err = Wrap(std::atan2(ty, tx) - std::atan2(fy, fx));
    const float steer = std::clamp(err * 1.2f, -0.6f, 0.6f);
    float want = dist < 9.0f ? 0.0f : std::min(24.0f, 6.0f + dist * 0.4f);
    if (std::fabs(err) > 1.2f) want = std::min(want, 5.0f);
    float gas = 0.0f, brake = 0.0f;
    const float diff = want - now;
    if (diff <= 0.0f) brake = std::min(-diff * 0.15f + (want < 0.5f ? 0.4f : 0.0f), 1.0f);
    else gas = std::min(diff * (now >= 2.0f ? 0.15f : 0.4f), 1.0f);
    if (std::getenv("GTA_TRAFFICDBG")) { static float acc = 0.0f; acc += e.dt; if (acc > 3.0f) { acc = 0.0f; std::fprintf(stderr, "[COP] car %d now %.1f dist %.0f err %.2f gas %.2f brake %.2f stuck %.1f active %d", d.car, now, dist, err, gas, brake, d.stuck, (int)c.phys.active); std::fputc(10, stderr); } }
    v.SetAIControls((size_t)d.car, steer, gas, brake, false);
    if (now < 0.4f && dist > 15.0f) d.stuck += e.dt; else d.stuck = std::max(0.0f, d.stuck - e.dt);
}

void Traffic::Update(Vehicles& v, D3DRenderer& r, const PathNodes& paths, const Env& e) {
    if (!paths.Ready()) return;
    for (size_t i = 0; i < m_drives.size();) {
        Drive& d = m_drives[i];
        VehicleInstance& c = v.Car((size_t)d.car);
        const float dx = c.x - e.px, dy = c.y - e.py;
        const bool tooFar = dx * dx + dy * dy > 250.0f * 250.0f;
        bool drop = false;
        if (c.removed || c.wrecked) drop = true;
        else if (d.car == e.playerCar) { v.ClearAIControls((size_t)d.car); c.speed = 0.0f; drop = true; }   // the player took it
        else if (d.cop && ((e.wanted == 0 && dx * dx + dy * dy > 60.0f * 60.0f) || d.stuck > 8.0f)) { c.removed = true; drop = true; }
        else if (tooFar || d.stuck > 10.0f || d.idle > 20.0f || c.health <= 0.0f || (c.phys.active && c.rot[8] < 0.2f)) { c.removed = true; drop = true; }   // gone, stuck or flipped
        if (drop) {
            if (e.peds && d.driverUid) {   // the driver goes with a removed car, and gets out of one the player takes
                const int pi = e.peds->FindUid(d.driverUid);
                if (pi >= 0 && c.removed) e.peds->Remove((size_t)pi);
                else if (pi >= 0) {   // jacked: out on the driver's side, running
                    const float side = -2.0f;
                    e.peds->Unseat((size_t)pi, c.x + (c.phys.active ? c.rot[0] : std::cos(c.heading)) * side, c.y + (c.phys.active ? c.rot[1] : std::sin(c.heading)) * side, c.z);
                }
            }
            m_drives.erase(m_drives.begin() + (long)i);
            continue;
        }
        d.age += e.dt;
        if (e.peds && d.driverUid) {   // the driver sits in the seat of the car's drawn matrix
            const int pi = e.peds->FindUid(d.driverUid);
            float body[16], seat[3];
            if (pi >= 0 && v.SeatLocal((size_t)d.car, seat)) { v.BodyMatrix((size_t)d.car, body); e.peds->Seat((size_t)pi, body, seat); }
            else d.driverUid = 0;
        }
        {   // CCarCtrl / CTaskComplexCarReactToVehicleCollision: a hard knock stops the car for a moment; if the player's car
            // did it, or it is nearly wrecked, the driver gets out and runs
            if (std::getenv("GTA_TRAFFICKNOCK") && i == 0 && d.age > 6.0f && d.age < 6.05f) { c.phys.lastDI = 100.0f; c.phys.damager = e.playerCar; }   // dev: a fake knock from the player's car
            const float knock = c.phys.lastDI;
            if (knock > 25.0f && d.shock <= 0.0f) {
                d.shock = 2.5f;
                if (c.phys.damager == e.playerCar || c.health < 300.0f) d.bail = true;
            }
            if (d.parked) {
                v.SetAIControls((size_t)d.car, 0.0f, 0.0f, 1.0f, true);
                if (e.peds) {   // CCarEnterExit / CTaskComplexCarSteal: a passer-by walks to the door, gets in, hotwires it (3 s) and drives off
                    const float rx = c.phys.active ? c.rot[0] : std::cos(c.heading), ry = c.phys.active ? c.rot[1] : std::sin(c.heading);
                    const float doorX = c.x - rx * 1.4f, doorY = c.y - ry * 1.4f;
                    if (d.thiefUid) {
                        const int ti = e.peds->FindUid(d.thiefUid);
                        if (ti < 0 || !e.peds->Alive((size_t)ti)) d.thiefUid = 0;
                        else if (std::hypot(e.peds->X((size_t)ti) - doorX, e.peds->Y((size_t)ti) - doorY) < 1.3f) {
                            e.peds->SetScripted((size_t)ti, false);
                            e.peds->ClearGoal((size_t)ti);
                            d.driverUid = d.thiefUid; d.thiefUid = 0;
                            d.parked = false; d.bail = false; d.shock = 3.0f;   // the hotwire
                            if (std::getenv("GTA_TRAFFICDBG")) std::fprintf(stderr, "[THIEF] in car %d\n", d.car);
                        } else e.peds->SetGoal((size_t)ti, doorX, doorY, 0);
                    } else if ((d.thiefScan -= e.dt) <= 0.0f) {
                        d.thiefScan = 4.0f;
                        int best = -1; float bestD = 25.0f;
                        for (size_t k = 0; k < e.peds->Count(); k++) {
                            if (!e.peds->Alive(k) || e.peds->Seated(k) || e.peds->Hidden(k) || e.peds->HasGoal(k) || e.peds->Fleeing(k)) continue;
                            const float dd = std::hypot(e.peds->X(k) - c.x, e.peds->Y(k) - c.y);
                            if (dd < bestD) { bestD = dd; best = (int)k; }
                        }
                        if (std::getenv("GTA_TRAFFICDBG")) std::fprintf(stderr, "[THIEF] parked car %d: nearest ped %d at %.1f m\n", d.car, best, bestD);
                        if (best >= 0 && Rand() < 0.5f) { d.thiefUid = e.peds->Uid((size_t)best); e.peds->SetScripted((size_t)best, true); }
                    }
                }
                if (!d.parked) { /* just got in: fall through to the shock hold below */ }
                else { i++; continue; }
            }
            if (d.shock > 0.0f) {
                d.shock -= e.dt;
                v.SetAIControls((size_t)d.car, 0.0f, 0.0f, 1.0f, false);
                if (d.shock <= 0.0f && d.bail && e.peds && d.driverUid) {
                    const int pi = e.peds->FindUid(d.driverUid);
                    if (pi >= 0) {
                        const float rx = c.phys.active ? c.rot[0] : std::cos(c.heading), ry = c.phys.active ? c.rot[1] : std::sin(c.heading);
                        e.peds->Unseat((size_t)pi, c.x - rx * 2.0f, c.y - ry * 2.0f, c.z);
                    }
                    d.driverUid = 0;
                    d.parked = true;   // the car stays where it stopped until it is far away
                }
                i++;
                continue;
            }
        }
        if (d.cop) { Pursue(v, d, e); i++; continue; }
        Drive_(v, paths, d, e);
        i++;
    }
    m_spawnTimer -= e.dt;
    if (std::getenv("GTA_TRAFFICDBG")) {
        static float acc = 0.0f;
        acc += e.dt;
        if (acc > 2.0f) {
            acc = 0.0f;
            std::fprintf(stderr, "[TRAFFIC] %zu cars", m_drives.size());
            for (size_t i = 0; i < m_drives.size() && i < 12; i++) { const VehicleInstance& c = v.Car((size_t)m_drives[i].car); std::fprintf(stderr, " | %d @%.0f,%.0f v%.1f want %.1f err %.2f hit %.1f lat %.1f", c.model, c.x, c.y, c.speed, m_drives[i].dbgWant, m_drives[i].dbgErr, m_drives[i].dbgHit, m_drives[i].dbgLat); }
            std::fprintf(stderr, "\n");
        }
    }
    int limit = maxCars;
    if (e.pop && e.pop->Ready()) limit = std::min(maxCars, std::max(2, e.pop->MaxCars(e.pop->TypeAt(e.px, e.py, e.pz), e.hour, e.weekend)));   // #Cars
    int civ = 0, cops = 0;
    for (const Drive& dd : m_drives) { if (dd.cop) cops++; else civ++; }
    if (civ < limit && m_spawnTimer <= 0.0f) {
        Spawn(v, r, paths, e);
        m_spawnTimer = 0.5f;
    }
    if (e.wanted > 0 && cops < std::min(1 + e.wanted, 5) && m_copTimer <= 0.0f && v.Info(596)) {   // CWanted: the police come to the player
        Spawn(v, r, paths, e, 596);
        m_copTimer = 3.0f;
    }
    m_copTimer -= e.dt;
}
