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
                       float* dx, float* dy) {
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
    return true;
}

bool Traffic::Spawn(Vehicles& v, D3DRenderer& r, const PathNodes& paths, const Env& e) {
    const std::vector<PathNodes::Address> closeNodes = paths.NodesNear(e.px, e.py, 170.0f, 1024);
    if (closeNodes.empty()) return false;
    const size_t start = (size_t)(Rand() * (float)closeNodes.size()) % closeNodes.size();
    for (size_t k = 0; k < closeNodes.size(); k++) {
        const PathNodes::Address a = closeNodes[(start + k) % closeNodes.size()];
        const PathNodes::Node* n = paths.Get(a);
        if (!n || n->switchedOff || n->water || n->roadBlock || n->deadEnd || n->spawnProbability == 0) continue;
        const float ddx = n->x - e.px, ddy = n->y - e.py, dist = std::sqrt(ddx * ddx + ddy * ddy);
        if (dist < 85.0f) continue;
        if (dist < 140.0f && (ddx * e.camFx + ddy * e.camFy) / std::max(dist, 1.0f) > 0.3f) continue;   // not where it is seen appearing
        if (Rand() * 15.0f > (float)n->spawnProbability) continue;
        PathNodes::Address to;
        float sx, sy, dx, dy;
        if (!PickLink(paths, a, PathNodes::Address{}, &to, &sx, &sy, &dx, &dy)) continue;
        const PathNodes::Node* tn = paths.Get(to);
        // the lane runs on to the start of the next link
        PathNodes::Address nextTo;
        float nx, ny, ndx, ndy;
        if (!PickLink(paths, to, a, &nextTo, &nx, &ny, &ndx, &ndy) || nextTo == a) continue;   // not a dead end
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
        const int model = kModels[(size_t)(Rand() * (float)(sizeof(kModels) / sizeof(kModels[0]))) % (sizeof(kModels) / sizeof(kModels[0]))];
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
        d.cruise = (n->highway ? 20.0f : 10.0f) + 5.0f * Rand();
        c.vx = ux * d.cruise * 0.8f; c.vy = uy * d.cruise * 0.8f;
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
        d.haveNext = paths.Get(d.to) && PickLink(paths, d.to, d.from, &d.nextTo, &d.nx, &d.ny, &d.ndx, &d.ndy) && d.nextTo != d.from;   // a dead end ends the trip
        if (!d.haveNext) d.stuck = 99.0f;
        return;
    }
    // aim a little way ahead on the line, round the corner into the next link's direction
    const float look = std::clamp(4.0f + now * 0.5f, 5.0f, 14.0f);
    float ax, ay;
    if (t + look <= segLen) { ax = d.sx + ux * (t + look); ay = d.sy + uy * (t + look); }
    else if (d.haveNext) { const float over = t + look - segLen; ax = d.nx + d.ndx * over; ay = d.ny + d.ndy * over; }
    else { ax = ex + ux * (t + look - segLen); ay = ey + uy * (t + look - segLen); }
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
        auto consider = [&](float ox, float oy, float halfWidth, float halfLength) {
            const float relx = ox - c.x, rely = oy - c.y;
            const float along = relx * fx + rely * fy, side = relx * rx + rely * ry;
            if (along <= 0.0f || along > reach + halfLength + 2.4f || std::fabs(side) > halfWidth + 1.1f) return;
            const float gap = along - halfLength - 2.4f;
            want = std::min(want, std::max(0.0f, gap - 1.0f) * 0.9f);
            d.dbgHit = gap;
        };
        for (size_t i = 0; i < v.Count(); i++) {
            const VehicleInstance& o = v.Car(i);
            if ((int)i == d.car || o.removed) continue;
            consider(o.x, o.y, 1.0f, 2.4f);
        }
        if (e.playerCar < 0) consider(e.px, e.py, 0.4f, 0.4f);
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

void Traffic::Update(Vehicles& v, D3DRenderer& r, const PathNodes& paths, const Env& e) {
    if (!paths.Ready()) return;
    for (size_t i = 0; i < m_drives.size();) {
        Drive& d = m_drives[i];
        VehicleInstance& c = v.Car((size_t)d.car);
        const float dx = c.x - e.px, dy = c.y - e.py;
        const bool tooFar = dx * dx + dy * dy > 250.0f * 250.0f;
        bool drop = false;
        if (c.removed || c.wrecked) drop = true;
        else if (d.car == e.playerCar) { v.ClearAIControls((size_t)d.car); drop = true; }   // the player took it
        else if (tooFar || d.stuck > 10.0f || d.idle > 20.0f || c.health <= 0.0f || (c.phys.active && c.rot[8] < 0.2f)) { c.removed = true; drop = true; }   // gone, stuck or flipped
        if (drop) { m_drives.erase(m_drives.begin() + (long)i); continue; }
        d.age += e.dt;
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
    if ((int)m_drives.size() < maxCars && m_spawnTimer <= 0.0f) {
        Spawn(v, r, paths, e);
        m_spawnTimer = 0.5f;
    }
}
