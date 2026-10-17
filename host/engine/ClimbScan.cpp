#include "ClimbScan.h"
#include "World.h"

#include <cmath>

namespace {

struct Probe { float r, x, y, z; };

// ms_ClimbColModel: four head-height spheres (pieces 0-3), then three columns
// of six (y = 0.5, 1.0, 1.5 ahead; z = 2.9 down to 0.4).
Probe g_climb[22];
// ms_StandUpColModel: the body standing on the ledge.
const Probe g_standUp[] = {{0.35f, 0, 0.6f, 1.75f}, {0.35f, 0, 0.6f, 1.3f}, {0.35f, 0, 0.6f, 0.85f}, {0.35f, 0, 0.3f, 0.65f},
                           {0.35f, 0, 0, 0.5f},     {0.35f, 0, 0, 0.1f},     {0.35f, 0, 0, -0.3f}};
// ms_VaultColModel: the path over the ledge.
const Probe g_vault[] = {{0.3f, 0, 1.1f, -0.2f}, {0.3f, 0, 1.1f, 0.2f}, {0.3f, 0, 1.1f, 0.6f},
                         {0.3f, 0, 0.7f, 0.6f},  {0.3f, 0, 0.35f, 0.6f}, {0.3f, 0, 0, 0.6f}};
// ms_FindEdgeColModel: a small grid that finds the lip of the surface.
Probe g_edge[16];

bool g_init = false;
void Init() {
    if (g_init) return;
    g_init = true;
    const float zs[4] = {1.2f, 1.7f, 2.2f, 2.5f};
    for (int i = 0; i < 4; i++) g_climb[i] = {0.4f, 0, -0.06f, zs[i]};
    const float ys[3] = {0.5f, 1.0f, 1.5f}, hz[6] = {2.9f, 2.4f, 1.9f, 1.4f, 0.9f, 0.4f};
    for (int c = 0; c < 3; c++)
        for (int k = 0; k < 6; k++) g_climb[4 + c * 6 + k] = {0.4f, 0, ys[c], hz[k]};
    const float ey[4] = {-0.5f, -0.3f, -0.1f, 0.1f}, ez[4] = {0.5f, 0.3f, 0.1f, -0.1f};
    for (int a = 0; a < 4; a++)
        for (int b = 0; b < 4; b++) g_edge[a * 4 + b] = {0.3f, 0, ey[a], ez[b]};
}

struct Frame {
    float o[3], fx, fy, rx, ry;
    Frame(const float p[3], float rot) : o{p[0], p[1], p[2]}, fx(-std::sin(rot)), fy(std::cos(rot)), rx(std::cos(rot)), ry(std::sin(rot)) {}
    void World(const Probe& s, float c[3]) const {
        c[0] = o[0] + rx * s.x + fx * s.y;
        c[1] = o[1] + ry * s.x + fy * s.y;
        c[2] = o[2] + s.z;
    }
};

// First probe (in piece order) that touches the world.
int FirstHit(const GameWorld& w, const Frame& f, const Probe* set, int n, GameWorld::ProbeHit& h) {
    for (int i = 0; i < n; i++) {
        float c[3];
        f.World(set[i], c);
        if (w.SphereProbe(c[0], c[1], c[2], set[i].r, h)) return i;
    }
    return -1;
}

// The handhold a contact gives: a wall face turns the ped to face it; a top
// surface keeps his heading. Returns false when the face does not point at him.
bool Accept(const Frame& f, const GameWorld::ProbeHit& h, float curRot, float outPos[3], float* outRot) {
    float n[3] = {h.n[0], h.n[1], h.n[2]};
    if (std::fabs(n[0]) > 0.05f || std::fabs(n[1]) > 0.05f) {
        if ((h.p[0] - f.o[0]) * n[0] + (h.p[1] - f.o[1]) * n[1] < 0.0f) { n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2]; }
        if (n[0] * f.fx + n[1] * f.fy + n[2] * 0.0f <= 0.3f) return false;
        *outRot = std::atan2(-n[0], n[1]);
    } else {
        *outRot = curRot;
    }
    outPos[0] = h.p[0]; outPos[1] = h.p[1]; outPos[2] = h.p[2];
    return true;
}

}  // namespace

bool ClimbScan(const GameWorld& w, int mode, const float ped[3], float rot, float outPos[3], float* outRot) {
    Init();
    const Frame f(ped, rot);
    GameWorld::ProbeHit h;
    if (mode == 2) return FirstHit(w, f, g_standUp, 7, h) >= 0;
    if (mode == 3) return FirstHit(w, f, g_vault, 6, h) >= 0;

    const bool launch = mode == 1;
    const int piece = FirstHit(w, f, g_climb, 22, h);
    if (piece < 0) return false;
    // Pieces 0-3, 4 and 10 are walls in the way (no grab); 16 and the
    // 2.4 m row (5, 11, 17) only count on a launch.
    if (piece <= 4 || piece == 10 || piece == 16) return false;
    if (!launch && (piece == 5 || piece == 11 || piece == 17)) return false;
    float pos[3], r = rot;
    if (!Accept(f, h, rot, pos, &r)) return false;

    // CTaskSimpleClimb::ms_FindEdgeColModel: slide the handhold to the lip.
    Frame e(pos, r);
    GameWorld::ProbeHit eh;
    if (FirstHit(w, e, g_edge, 16, eh) >= 0) {
        float p2[3], r2 = r;
        if (Accept(e, eh, r, p2, &r2)) {
            for (int i = 0; i < 3; i++) pos[i] = p2[i];
            r = r2;
        } else if (std::fabs(eh.n[0]) <= 0.05f && std::fabs(eh.n[1]) <= 0.05f) {
            for (int i = 0; i < 3; i++) pos[i] = eh.p[i];
        }
    }
    for (int i = 0; i < 3; i++) outPos[i] = pos[i];
    *outRot = r;
    return true;
}
