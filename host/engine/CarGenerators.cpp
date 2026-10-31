// Parked cars from the IPL car generators. See CarGenerators.h.

#define NOMINMAX
#include "CarGenerators.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Vehicle.h"

namespace {

unsigned g_genRng = 0x2545f491u;
int GenRand(int n) { g_genRng = g_genRng * 1664525u + 1013904223u; return n > 0 ? (int)((g_genRng >> 8) % (unsigned)n) : 0; }

}  // namespace

void CarGenerators::Init(const std::vector<BinIplCarGen>& gens) {
    m_gens.clear();
    for (const BinIplCarGen& g : gens) {
        Gen e;
        e.file = g;
        m_gens.push_back(e);
    }
    m_generateClose = 120;
}

void CarGenerators::Process(Vehicles& v, D3DRenderer& r, const float player[3], const float playerVel[2], int hour, uint32_t nowMs,
                            const std::function<float(float, float, float)>& groundZ) {
    // CCarGenerator::Process: forget a car that is gone, rest a minute after the player takes one.
    m_parked = 0;
    for (Gen& g : m_gens) {
        if (g.car < 0) continue;
        if ((size_t)g.car >= v.Count() || v.Car((size_t)g.car).removed || !v.Car((size_t)g.car).recycle) { g.car = -1; continue; }
        VehicleInstance& c = v.Car((size_t)g.car);
        if (c.hasDriver || c.engineOn) {   // the player has had it: it is his now
            c.recycle = false;
            g.car = -1;
            g.nextGenMs = nowMs + 60000u;
            g.waitUntilFar = true;
            continue;
        }
        const float dx = c.x - player[0], dy = c.y - player[1];
        if (dx * dx + dy * dy > 180.0f * 180.0f) {   // CCarCtrl::PossiblyRemoveVehicle
            c.removed = true;
            g.car = -1;
            continue;
        }
        m_parked++;
    }
    const bool night = hour >= 21 || hour < 7;   // CClock::ClockHoursInRange(21, 7)
    for (Gen& g : m_gens) {
        if (g.car >= 0 || nowMs < g.nextGenMs) continue;
        // CheckIfWithinRangeOfAnyPlayers
        const float rx = player[0] - g.file.x, ry = player[1] - g.file.y;
        if (std::fabs(player[2] - g.file.z) > 50.0f) continue;
        const float d = std::sqrt(rx * rx + ry * ry);
        if (d >= 160.0f) { g.waitUntilFar = false; continue; }
        if (m_generateClose <= 0) {
            if (g.waitUntilFar) continue;
            if (g.file.z >= 950.0f) continue;   // interiors stand high up
            if (d < 140.0f || playerVel[0] * rx + playerVel[1] * ry > 0.0f) continue;
        }
        // DoInternalProcessing
        const bool ignoreLimit = (g.file.flags & 2) != 0;
        if (!ignoreLimit && m_parked >= (night ? 10 : 5)) continue;
        int model = g.file.modelId;
        if (model < 0) {   // PickRandomCar: anything that is not a boat and is up to 8 m long
            if (g.randomModel <= 0) {
                for (int tries = 0; tries < 8 && g.randomModel <= 0; tries++) {
                    const int id = v.IdAt(GenRand(v.ModelCount()));
                    const VehicleModel* vm = v.LoadModel(r, id);
                    if (vm && vm->bbMaxY - vm->bbMinY <= 8.0f) g.randomModel = id;
                }
                if (g.randomModel <= 0) continue;
            }
            model = g.randomModel;
        }
        const VehicleInfo* info = v.Info(model);
        if (!info || info->type == "boat" || info->type == "train") continue;   // boats float, trains run on rails
        // CheckForBlockage: another car on the spot
        bool blocked = false;
        for (size_t i = 0; i < v.Count() && !blocked; i++) {
            const VehicleInstance& o = v.Car(i);
            if (o.removed) continue;
            const float ox = o.x - g.file.x, oy = o.y - g.file.y;
            blocked = ox * ox + oy * oy < 5.0f * 5.0f && std::fabs(o.z - g.file.z) < 4.0f;
        }
        if (blocked) { g.waitUntilFar = true; continue; }
        float gz = groundZ(g.file.x, g.file.y, g.file.z > -100.0f ? g.file.z + 1.0f : 1000.0f);
        if (gz < -1e5f || std::fabs(gz - g.file.z) > 5.0f) gz = g.file.z;   // ground not streamed in yet: the spot's own height
        const int n = v.Spawn(r, model, g.file.x, g.file.y, gz, g.file.angle);
        if (n < 0) continue;
        VehicleInstance& c = v.Car((size_t)n);
        c.recycle = true;
        if (g.file.modelId >= 0) {
            if (g.file.colour1 >= 0) c.paint[0] = g.file.colour1;
            if (g.file.colour2 >= 0) c.paint[1] = g.file.colour2;
        }
        g.car = n;
        m_parked++;
        if (std::getenv("GTA_CARDBG")) std::fprintf(stderr, "[CARGEN] model %d at %.1f %.1f %.1f (spot %.1f, %.0f m away)\n", model, c.x, c.y, c.z, g.file.z, d);
    }
    if (m_generateClose > 0) m_generateClose--;
}
