// GameWorld implementation - data-driven exterior world (see World.h).
//
// Streaming model (mirrors retail CStreaming):
//   * Per frame: rebuild the "needed" set = instances within view radius,
//     sorted by distance.
//   * Load with a per-frame budget, HD model when the player is inside
//     drawDistance+margin, LOD model for distant instances; models are shared
//     through m_cache (one mesh set per IDE model).
//   * Unload after refs==0 for kUnloadGrace frames (frees meshes + txd).
//   * Terrain ground-height triangles come from real .col entries and are
//     bucketed into an 8m spatial hash; ground height = highest surface.
//   * Solid COL boxes are active for loaded non-ground models only, with
//     retail tolerances (step-over low boxes, duck under tall ones, block
//     only when entering).

#define NOMINMAX // windows.h min/max macros break std::min/std::max

#include "World.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

#include "BinaryIpl.h"
#include "ColLoader.h"
#include "DffLoader.h"
#include "IdeLoader.h"
#include "IplLoader.h"
#include "TxdLoader.h"

namespace {

constexpr float kNoGround = ColLoader::kNoGround;

// --- small byte readers (little-endian, same layout as main.cpp) ---
uint16_t RD_U16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t RD_U32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
float RD_F32(const uint8_t* p) {
    uint32_t u = RD_U32(p);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

// Case-insensitive compare of a null-terminated model name against a
// max-22-byte COL name field.
// Whole-name, case-insensitive match (retail CFileLoader::LoadCollisionFile
// looks the entry name up as a full model name). The field ending first must
// also end the model name: a bare `break` here used to make the entry
// "crlsafhus_LAS" match the model "crlsafhus_las2", so every *_las2 / *2
// model whose prefix exists in an earlier archive got that model's COL --
// foreign boxes (invisible walls) and no triangles (walk-through walls).
bool ColNameEq(const uint8_t* field, const char* model) {
    for (int i = 0; i < 22; i++) {
        char c = (char)field[i];
        if (!c) return model[i] == '\0';
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
        char m = model[i];
        if (!m) {
            // model name shorter than field: must hit field end / padding
            for (int j = i; j < 22; j++)
                if (field[j]) return false;
            return true;
        }
        if (m >= 'A' && m <= 'Z') m += ('a' - 'A');
        if (c != m) return false;
    }
    return true;
}

// Standard full quaternion rotation + translation of a point.
void QuatXform(float qx, float qy, float qz, float qw, float x, float y, float z,
               float tx, float ty, float tz, float& ox, float& oy, float& oz) {
    // Same row-vector convention as QuatToMatrix so collision boxes land
    // exactly on the rendered meshes. (Was the transpose, which offset the
    // pink wireframe from the visuals.)
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;
    ox = x * (1.f - 2.f * (yy + zz)) + y * 2.f * (xy - wz) + z * 2.f * (xz + wy) + tx;
    oy = x * 2.f * (xy + wz) + y * (1.f - 2.f * (xx + zz)) + z * 2.f * (yz - wx) + ty;
    oz = x * 2.f * (xz - wy) + y * 2.f * (yz + wx) + z * (1.f - 2.f * (xx + yy)) + tz;
}

D3DMATRIX QuatToMatrix(float qx, float qy, float qz, float qw,
                       float tx, float ty, float tz) {
    // NB: callers pass IPL quaternions already conjugated at load time
    // (BinaryIpl.cpp / IplLoader.cpp). This builds the standard row-vector
    // rotation matrix for the quaternion as given.
    D3DMATRIX m;
    memset(&m, 0, sizeof(m));
    float xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float wx = qw * qx, wy = qw * qy, wz = qw * qz;
    m._11 = 1.f - 2.f * (yy + zz);
    m._12 = 2.f * (xy + wz);
    m._13 = 2.f * (xz - wy);
    m._21 = 2.f * (xy - wz);
    m._22 = 1.f - 2.f * (xx + zz);
    m._23 = 2.f * (yz + wx);
    m._31 = 2.f * (xz + wy);
    m._32 = 2.f * (yz - wx);
    m._33 = 1.f - 2.f * (xx + yy);
    m._41 = tx;
    m._42 = ty;
    m._43 = tz;
    m._44 = 1.f;
    return m;
}

// Camera frustum for culling. Planes extracted from M = view*proj (row-vector
// D3D convention, Gribb-Hartmann). Works with the mirrored projection (_11<0):
// the six planes still bound the actual projected volume. A sphere is kept iff
// it is inside (or straddling) all six planes.
struct Frustum { float p[6][4]; };

Frustum MakeFrustum(const D3DMATRIX& v, const D3DMATRIX& pr) {
    float m[4][4];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += v.m[i][k] * pr.m[k][j];
            m[i][j] = s;
        }
    Frustum f;
    auto set = [&](int idx, float a, float b, float c, float d) {
        float len = std::sqrt(a * a + b * b + c * c);
        if (len < 1e-9f) len = 1.0f;
        f.p[idx][0] = a / len; f.p[idx][1] = b / len;
        f.p[idx][2] = c / len; f.p[idx][3] = d / len;
    };
    set(0, m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]); // left
    set(1, m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0]); // right
    set(2, m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1]); // bottom
    set(3, m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1]); // top
    set(4, m[0][2], m[1][2], m[2][2], m[3][2]);                                         // near (D3D z>=0)
    set(5, m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2]); // far
    return f;
}

inline bool SphereInFrustum(const Frustum& f, float x, float y, float z, float rad) {
    for (int i = 0; i < 6; i++) {
        float d = f.p[i][0] * x + f.p[i][1] * y + f.p[i][2] * z + f.p[i][3];
        if (d < -rad) return false; // fully outside this plane
    }
    return true;
}

bool IsLampModel(const std::string& n) {
    return n.find("lamppost") != std::string::npos
        || n.find("streetlamp") != std::string::npos
        || n.find("streetlght") != std::string::npos
        || n.find("doublestreet") != std::string::npos;
    // NOTE: traffic lights are deliberately NOT lamps. They were emitting a big
    // round amber street-lamp corona on the signal head, which reads as a lamp,
    // not a traffic light. Retail traffic signals are small red/amber/green
    // bulbs (we don't model signal state), so no lamp glow is better than a
    // wrong one. (If a combined traffic-light-with-streetlamp model needs its
    // lamp lit later, match that specific model, not every "trafficlight".)
}

// Exactly CTrafficLights::IsMITrafficLight: the MI_TRAFFICLIGHTS* models that
// MatchAllModelStrings binds by name. MTraffic3 is not a signal in retail.
bool IsTrafficLightModel(const std::string& n) {
    static const char* kModels[] = {
        "trafficlight1",      // MI_TRAFFICLIGHTS
        "mtraffic4",          // MI_TRAFFICLIGHTS_VERTICAL
        "mtraffic1",          // MI_TRAFFICLIGHTS_MIAMI (overhead gantry)
        "vgsstriptlights1",   // MI_TRAFFICLIGHTS_VEGAS
        "mtraffic2",          // MI_TRAFFICLIGHTS_TWOVERTICAL
        "cj_traffic_light3", "cj_traffic_light4", "cj_traffic_light5",
        "gay_traffic_light",  // MI_TRAFFICLIGHTS_GAY
    };
    for (const char* m : kModels)
        if (n == m) return true;
    return false;
}

// --- CTrafficLights state machine, verbatim from the decomp (riots/cheats
// omitted). uVar2 = (realTimeMs >> 1) & 0x3fff cycles 0..16383 over ~32.77s.
// Returns 0=green, 1=amber, 2=red. Axis 1 and 2 are offset so cross streets
// alternate (one green while the other is red), with an all-red pause.
int TL_LightForCars1(uint32_t u) {       // CTrafficLights::LightForCars1_Visual
    if (u > 4999) return (u > 5999) ? 2 : 1;
    return 0;
}
int TL_LightForCars2(uint32_t u) {       // CTrafficLights::LightForCars2_Visual
    if (u < 6000) return 2;
    if (u > 10999) return (u > 11999) ? 2 : 1;
    return 0;
}
// FindTrafficLightTypeFromOrientation: which axis (1 or 2) a light serves.
int TL_TypeFromOrientation(float deg) {
    if (((deg <= 60.0f) || (150.0f <= deg)) && ((deg <= 240.0f) || (330.0f <= deg)))
        return 2;
    return 1;
}
// aTrafficLightColours[0..2] = green, amber, red (SA data globals).
const uint8_t kTLColR[3] = {0, 255, 255};
const uint8_t kTLColG[3] = {255, 128, 0};
const uint8_t kTLColB[3] = {0, 0, 0};

bool CircleBoxHit(float cx, float cy, float r, const WorldColBox& b) {
    float nx = std::clamp(cx, b.minX, b.maxX);
    float ny = std::clamp(cy, b.minY, b.maxY);
    float dx = cx - nx, dy = cy - ny;
    return dx * dx + dy * dy <= r * r;
}

// 2D circle vs the oriented box footprint (corners 0..7 = i*4+j*2+k over the
// model box after the instance rotation). The XY footprint is a rotated rect:
// corner 0 is (min,min,·), 4 is (max,min,·) along edge U, 2 is (min,max,·)
// along edge V. Project the circle centre into that frame and clamp. Tighter
// than the AABB, so a rotated building no longer blocks the empty corner.
bool CircleObbHit2D(float px, float py, float r, const WorldColObb& o) {
    float ux = o.x[4] - o.x[0], uy = o.y[4] - o.y[0];   // edge U
    float vx = o.x[2] - o.x[0], vy = o.y[2] - o.y[0];   // edge V
    float cx = (o.x[0] + o.x[6]) * 0.5f;                // footprint centre
    float cy = (o.y[0] + o.y[6]) * 0.5f;
    float lu = std::sqrt(ux*ux + uy*uy), lv = std::sqrt(vx*vx + vy*vy);
    if (lu < 1e-6f || lv < 1e-6f)                       // degenerate: fall back to point
        { float dx=px-cx, dy=py-cy; return dx*dx+dy*dy <= r*r; }
    ux/=lu; uy/=lu; vx/=lv; vy/=lv;
    float dx = px - cx, dy = py - cy;
    float tu = std::clamp(dx*ux + dy*uy, -lu*0.5f, lu*0.5f);
    float tv = std::clamp(dx*vx + dy*vy, -lv*0.5f, lv*0.5f);
    float nx = cx + ux*tu + vx*tv, ny = cy + uy*tu + vy*tv;
    float ex = px - nx, ey = py - ny;
    return ex*ex + ey*ey <= r*r;
}

// 2D circle vs a triangle's XY footprint: inside, or within r of any edge.
// Used for solid WALL collision against the COL mesh (models whose COL3 has no
// box section -- the forum, bridge girders, etc. -- so the player can't walk
// through them).
float SegDist2(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    float len2 = dx*dx + dy*dy;
    float t = len2 > 1e-12f ? ((px-ax)*dx + (py-ay)*dy) / len2 : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    float cx = ax + t*dx, cy = ay + t*dy;
    float ex = px - cx, ey = py - cy;
    return ex*ex + ey*ey;
}
// Signed XY clearance from a point to a box footprint: distance outside,
// minus the penetration depth inside.
float ObbClearance2D(float px, float py, const WorldColObb& o) {
    float ux = o.x[4] - o.x[0], uy = o.y[4] - o.y[0];
    float vx = o.x[2] - o.x[0], vy = o.y[2] - o.y[0];
    float cx = (o.x[0] + o.x[6]) * 0.5f, cy = (o.y[0] + o.y[6]) * 0.5f;
    float lu = std::sqrt(ux*ux + uy*uy), lv = std::sqrt(vx*vx + vy*vy);
    float dx = px - cx, dy = py - cy;
    if (lu < 1e-6f || lv < 1e-6f) return std::sqrt(dx*dx + dy*dy);
    ux /= lu; uy /= lu; vx /= lv; vy /= lv;
    float du = std::fabs(dx*ux + dy*uy) - lu * 0.5f;
    float dv = std::fabs(dx*vx + dy*vy) - lv * 0.5f;
    if (du <= 0.0f && dv <= 0.0f) return std::max(du, dv);
    float ou = std::max(du, 0.0f), ov = std::max(dv, 0.0f);
    return std::sqrt(ou*ou + ov*ov);
}

float SegDist2(float px, float py, float ax, float ay, float bx, float by);

// Signed XY clearance from a point to a triangle's footprint (vertical walls
// project to a segment, so this is the distance to that line).
float TriClearance2D(float px, float py, const ColTriangle& t) {
    float e = std::sqrt(std::min({SegDist2(px,py, t.ax,t.ay, t.bx,t.by),
                                  SegDist2(px,py, t.bx,t.by, t.cx,t.cy),
                                  SegDist2(px,py, t.cx,t.cy, t.ax,t.ay)}));
    float d = (t.by-t.cy)*(t.ax-t.cx) + (t.cx-t.bx)*(t.ay-t.cy);
    if (std::fabs(d) > 1e-9f) {
        float l1 = ((t.by-t.cy)*(px-t.cx) + (t.cx-t.bx)*(py-t.cy)) / d;
        float l2 = ((t.cy-t.ay)*(px-t.cx) + (t.ax-t.cx)*(py-t.cy)) / d;
        if (l1 >= 0 && l2 >= 0 && 1.0f - l1 - l2 >= 0) return -e;
    }
    return e;
}

// A move is blocked when it ends within the player radius of a solid AND
// closer than where it started. The old rule ("block only if the origin was
// clear") let a player who was already touching a thin wall keep stepping
// straight through it. This one still allows sliding along walls and
// stepping back out of anything the player spawned or fell into.
bool MoveBlocked(float oldClear, float newClear, float r) {
    return newClear < r && newClear < oldClear - 1e-4f;
}

bool CircleTriHit2D(float px, float py, float r, const ColTriangle& t) {
    // point-in-triangle (XY, barycentric sign)
    float d = (t.by-t.cy)*(t.ax-t.cx) + (t.cx-t.bx)*(t.ay-t.cy);
    if (fabsf(d) > 1e-9f) {
        float l1 = ((t.by-t.cy)*(px-t.cx) + (t.cx-t.bx)*(py-t.cy)) / d;
        float l2 = ((t.cy-t.ay)*(px-t.cx) + (t.ax-t.cx)*(py-t.cy)) / d;
        float l3 = 1.0f - l1 - l2;
        if (l1 >= 0 && l2 >= 0 && l3 >= 0) return true;
    }
    float r2 = r*r;
    return SegDist2(px,py, t.ax,t.ay, t.bx,t.by) < r2
        || SegDist2(px,py, t.bx,t.by, t.cx,t.cy) < r2
        || SegDist2(px,py, t.cx,t.cy, t.ax,t.ay) < r2;
}

} // namespace

// ---------------------------------------------------------------- lifecycle

GameWorld::~GameWorld() { Shutdown(); }



bool GameWorld::Init(ImgLoader& img, D3DRenderer& renderer, const std::string& gameDir) {
    m_img = &img;
    m_renderer = &renderer;
    m_gameDir = gameDir;
    m_frame = 0;

    if (!LoadIdes(gameDir)) return false;
    LoadRoadsignFont();
    LoadTextIpls(gameDir);
    BuildTextInsts();
    LoadStreams();
    LinkLodChildren();
    LoadCols();
    BuildLampEmitters();

    // GTA_COLAUDIT=1: resolve the COL entry of every placed model once and
    // report the ones without collision (excluding LOD models, which have
    // none by design) plus any whose triangles fail to load.
    if (getenv("GTA_COLAUDIT")) {
        std::set<std::string> models;
        for (const Inst& in : m_insts) if (!in.hdModel.empty()) models.insert(in.hdModel);
        size_t none = 0, triFail = 0, lods = 0;
        for (const std::string& m : models) {
            if (m.compare(0, 3, "lod") == 0) { lods++; continue; }
            const ColEntryLoc* loc = FindColEntry(m);
            if (!loc || loc->arc == size_t(-1)) {
                none++;
                std::fprintf(stderr, "[COLAUDIT] no COL: %s\n", m.c_str());
                continue;
            }
            const std::vector<uint8_t>& d = m_cols[loc->arc].data;
            uint16_t nf = RD_U16(d.data() + loc->off + 0x4C);
            std::vector<ColTriangle> tris;
            if (nf && !ColLoader::LoadModelTriangles(d.data(), d.size(), m.c_str(), 0, 0, 0, 1, 0, 0, 0, tris)) {
                triFail++;
                std::fprintf(stderr, "[COLAUDIT] tri load failed: %s (%s)\n", m.c_str(), m_cols[loc->arc].name.c_str());
            }
        }
        std::fprintf(stderr, "[COLAUDIT] %zu placed models (%zu LOD skipped): %zu without COL, %zu tri failures\n",
                     models.size(), lods, none, triFail);
    }

    // Free the raw text arrays (positions only needed at Init; instances
    // already captured what they need).
    m_text.clear();
    m_textInst.clear();

    std::fprintf(stderr, "[World] Init done: %zu instances, %zu ide models, %zu col archives (%zu stream files parsed)\n",
                 m_insts.size(), m_byId.size(), m_cols.size(), m_streamFiles);
    return true;
}

void GameWorld::Shutdown() {
    for (auto& [name, e] : m_cache) FreeModel(e);
    m_cache.clear();
    for (auto& kv : m_d3dTex)
        if (kv.second) m_renderer->DestroyTexture(kv.second);
    m_d3dTex.clear();
    m_texInfo.clear();
    m_txdPool.clear();
    m_txdRefs.clear();
    m_txdTexKeys.clear();
    if (m_fallbackTex) {
        m_renderer->DestroyTexture(m_fallbackTex);
        m_fallbackTex = nullptr;
    }
    m_cells.clear();
    m_insts.clear();
    m_worldBoxes.clear();
    m_cols.clear();
    m_colEntry.clear();
    m_badModels.clear();
    m_streamFiles = 0;
    m_byId.clear();
    m_byName.clear();
    m_img = nullptr;
}

// ------------------------------------------------------------------ loading

bool GameWorld::LoadIdes(const std::string& gameDir) {
    static const char* generic[] = {"vegepart", "barriers", "dynamic",
                                    "dynamic2", "multiobj", "procobj"};
    static const char* la[] = {"LAn",  "LAn2", "LAs",  "LAs2", "LAe", "LAe2",
                               "LAw",  "LAwn", "LAw2", "LAhills", "LAxref"};
    static const char* sf[] = {"SFn", "SFs", "SFSe", "SFe", "SFw", "SFxref"};
    static const char* vegas[] = {"vegasN", "VegasS", "vegasE", "VegasW", "vegaxref"};
    static const char* country[] = {"countryN", "countn2", "countryS", "countrye",
                                    "countryW", "counxref"};
    static const char* level[] = {"levelmap", "levelxre", "seabed"};
    static const char* interior[] = {"int_LA", "int_SF", "int_veg", "int_cont", "gen_int1",
                                     "gen_int2", "gen_intb", "gen_int3", "gen_int4",
                                     "gen_int5", "savehous", "stadint", "props", "props2",
                                     "propext"};

    int loaded = 0;
    auto collectParents = [&](std::vector<TxdParent>& parents) {
        for (auto& tp : parents) {
            if (tp.child.empty() || tp.parent.empty()) continue;
            // First definition wins (matches m_byId/m_byName policy).
            m_txdParent.emplace(tp.child, tp.parent);
        }
    };
    auto loadOne = [&](const char* dir, const char* name) {
        std::string p = gameDir + "\\data\\maps\\" + dir + "\\" + name + ".ide";
        std::vector<IdeObject> objs;
        std::unordered_map<int, size_t> idx;
        std::vector<TxdParent> parents;
        if (!IdeLoader::Load(p, objs, idx, &parents)) {
            std::fprintf(stderr, "[World] IDE load FAILED: %s\n", p.c_str());
            return;
        }
        for (auto& o : objs) {
            if (m_byId.find(o.id) == m_byId.end()) m_byId[o.id] = o;
            std::string n = ToLower(o.modelName);
            if (m_byName.find(n) == m_byName.end()) m_byName[n] = o;
        }
        collectParents(parents);
        loaded++;
    };

    for (const char* n : generic) loadOne("generic", n);
    for (const char* n : la) loadOne("LA", n);
    for (const char* n : sf) loadOne("SF", n);
    for (const char* n : vegas) loadOne("vegas", n);
    for (const char* n : country) loadOne("country", n);
    for (const char* n : level) loadOne("leveldes", n);
    for (const char* n : interior) loadOne("interior", n);
    loadOne("veh_mods", "veh_mods");

    // Dedicated txd-parent files (txdp-only). data\maps\txd.ide holds the
    // gb_* global/LOD chain (gb_global root); data\txdcut.ide holds cutscene
    // vehicle txd parents. These are the parents for shared/LOD textures that
    // live in no model's own txd.
    auto loadParentsFile = [&](const std::string& rel) {
        std::string p = gameDir + "\\" + rel;
        std::vector<IdeObject> objs;
        std::unordered_map<int, size_t> idx;
        std::vector<TxdParent> parents;
        if (!IdeLoader::Load(p, objs, idx, &parents)) return;
        collectParents(parents);
    };
    loadParentsFile("data\\maps\\txd.ide");
    loadParentsFile("data\\txdcut.ide");

    std::fprintf(stderr, "[World] IDEs: %d files, %zu models, %zu txd parents\n",
                 loaded, m_byId.size(), m_txdParent.size());
    return loaded > 0;
}

bool GameWorld::LoadTextIpls(const std::string& gameDir) {
    // region key (lower) -> text IPL file (relative to data\maps)
    static const std::pair<const char*, const char*> regions[] = {
        {"lae2", "LA\\LAe2.ipl"},     {"lae", "LA\\LAe.ipl"},
        {"lahills", "LA\\LAhills.ipl"}, {"lan", "LA\\LAn.ipl"},
        {"lan2", "LA\\LAn2.ipl"},     {"las", "LA\\LAs.ipl"},
        {"las2", "LA\\LAs2.ipl"},     {"law", "LA\\LAw.ipl"},
        {"law2", "LA\\LAw2.ipl"},     {"lawn", "LA\\LaWn.ipl"},
        {"sfn", "SF\\SFn.ipl"},       {"sfs", "SF\\SFs.ipl"},
        {"sfse", "SF\\SFSe.ipl"},     {"sfe", "SF\\SFe.ipl"},
        {"sfw", "SF\\SFw.ipl"},
        {"vegasn", "vegas\\vegasN.ipl"}, {"vegass", "vegas\\vegasS.ipl"},
        {"vegase", "vegas\\vegasE.ipl"}, {"vegasw", "vegas\\vegasW.ipl"},
        {"countryn", "country\\countryN.ipl"}, {"countn2", "country\\countN2.ipl"},
        {"countrys", "country\\countrys.ipl"}, {"countrye", "country\\countryE.ipl"},
        {"countryw", "country\\countryW.ipl"},
        {"levelmap", "leveldes\\levelmap.ipl"},
        {"seabed", "leveldes\\seabed.ipl"},
    };
    for (auto& r : regions) {
        std::string p = m_gameDir + "\\data\\maps\\" + r.second;
        std::vector<IplInstance> insts;
        if (IplLoader::Load(p, insts))
            m_text[r.first] = std::move(insts);
        else
            std::fprintf(stderr, "[World] text IPL FAILED: %s\n", p.c_str());
    }
    size_t total = 0;
    for (auto& kv : m_text) total += kv.second.size();
    std::fprintf(stderr, "[World] text IPLs: %zu files, %zu insts\n", m_text.size(), total);
    return true;
}

void GameWorld::LoadStreams() {
    static const char* regionKeys[] = {"lae2", "lae", "lahills", "lan",  "lan2",
                                       "las",  "las2", "law",    "law2", "lawn",
                                       "sfn",  "sfs",  "sfse",   "sfe",  "sfw",
                                       "vegasn", "vegass", "vegase", "vegasw",
                                       "countryn", "countn2", "countrys", "countrye",
                                       "countryw", "levelmap", "seabed"};
    size_t parsed = 0, kept = 0, skippedArea = 0, skippedModel = 0, linked = 0;

    for (size_t i = 0; i < m_img->EntryCount(); i++) {
        const std::string& en = m_img->GetEntryName(i);
        // "lae2_stream3.ipl" -> region "lae2"
        auto underscore = en.find('_');
        if (underscore == std::string::npos ||
            en.size() < underscore + 12 ||
            en.compare(underscore, 7, "_stream") != 0 ||
            en.compare(en.size() - 4, 4, ".ipl") != 0)
            continue;
        std::string region = en.substr(0, underscore);
        bool known = false;
        for (const char* k : regionKeys)
            if (region == k) { known = true; break; }
        if (!known) continue;

        std::vector<uint8_t> data = m_img->Extract(en);
        if (data.empty()) continue;
        std::vector<BinIplInstance> bins;
        if (!BinaryIplLoader::LoadFromMemory(data.data(), data.size(), bins)) {
            std::fprintf(stderr, "[World] stream parse FAILED: %s\n", en.c_str());
            continue;
        }
        parsed++;

        // The region's text IPL placements: a binary instance's LOD index
        // points into that file's inst list (CIplStore::SetupRelatedIpls).
        auto tit = m_textInst.find(region);
        const std::vector<int32_t>* text = (tit != m_textInst.end()) ? &tit->second : nullptr;

        for (auto& b : bins) {
            // Area 0 = outdoors, 13 = visible in every area (CEntity::m_nAreaCode).
            if (b.AreaCode() != 0 && b.AreaCode() != 13) { skippedArea++; continue; }
            auto it = m_byId.find(b.modelId);
            if (it == m_byId.end()) {
                skippedModel++;
                if (getenv("GTA_COLAUDIT")) std::fprintf(stderr, "[COLAUDIT] unknown model id %d at %.0f,%.0f in %s\n", b.modelId, b.x, b.y, en.c_str());
                continue;
            }

            Inst in;
            in.x = b.x; in.y = b.y; in.z = b.z;
            in.qx = b.qx; in.qy = b.qy; in.qz = b.qz; in.qw = b.qw;
            in.hdModel = ToLower(it->second.modelName);
            in.hdTxd = ToLower(it->second.txdName);
            in.hdDraw = it->second.drawDistance;
            in.isGround = IsGroundModel(in.hdModel, it->second.flags);

            if (text && b.lodIndex >= 0 && (size_t)b.lodIndex < text->size() &&
                (*text)[b.lodIndex] >= 0) {
                in.lodParent = (*text)[b.lodIndex];
                linked++;
            } else if (b.lodIndex >= 0 && getenv("GTA_LODAUDIT")) {
                std::fprintf(stderr, "[LODAUDIT] unresolved lod index %d (text %zu) for %s in %s\n",
                             b.lodIndex, text ? text->size() : 0, in.hdModel.c_str(), en.c_str());
            }
            m_insts.push_back(std::move(in));
            kept++;
        }
    }
    m_streamFiles = parsed;
    std::fprintf(stderr,
                 "[World] streams: %zu parsed, %zu insts kept, %zu area-skipped, "
                 "%zu model-skipped, %zu LODs linked\n",
                 parsed, kept, skippedArea, skippedModel, linked);
}

// Every text IPL line becomes a placement at its own transform, HD or LOD
// alike. The old scheme drew a binary instance's LOD at the HD instance's
// transform and treated every other text line as a "standalone" that always
// draws -- so a text-placed HD road (laroads_26_las2) and its text-placed LOD
// (lod_laroads_26), linked by the text line's own LOD field, both drew at
// every distance: the blurry LOD road on top of the real one.
void GameWorld::BuildTextInsts() {
    size_t added = 0, textLinks = 0;
    for (auto& [region, arr] : m_text) {
        std::vector<int32_t>& map = m_textInst[region];
        map.assign(arr.size(), -1);
        for (size_t i = 0; i < arr.size(); i++) {
            const IplInstance& li = arr[i];
            const int area = li.interior & 0xFF;   // same rule as binary
            if (area != 0 && area != 13) continue;
            auto nit = m_byName.find(li.modelName);
            if (nit == m_byName.end()) continue; // model unknown
            Inst in;
            in.x = li.x; in.y = li.y; in.z = li.z;
            in.qx = li.qx; in.qy = li.qy; in.qz = li.qz; in.qw = li.qw;
            in.hdModel = li.modelName;
            in.hdTxd = ToLower(nit->second.txdName);
            in.hdDraw = nit->second.drawDistance;
            in.isGround = IsGroundModel(in.hdModel, nit->second.flags);
            map[i] = (int32_t)m_insts.size();
            m_insts.push_back(std::move(in));
            added++;
        }
        // Text-to-text LOD links: the last field of a text inst line.
        for (size_t i = 0; i < arr.size(); i++) {
            const int li = arr[i].lodIndex;
            if (map[i] < 0 || li < 0 || (size_t)li >= arr.size() || (size_t)li == i) continue;
            if (map[li] < 0) continue;
            m_insts[map[i]].lodParent = map[li];
            textLinks++;
        }
    }
    std::fprintf(stderr, "[World] text placements: %zu (%zu text LOD links)\n", added, textLinks);
}

// Count each LOD's children and its level (leaf 0, LOD 1, LOD-of-LOD 2...),
// so Render can resolve visibility bottom-up.
void GameWorld::LinkLodChildren() {
    auto recount = [this]() {
        size_t lods = 0;
        for (Inst& in : m_insts) { in.lodChildren = 0; in.lodLevel = 0; }
        for (Inst& in : m_insts)
            if (in.lodParent >= 0 && m_insts[in.lodParent].lodChildren++ == 0) lods++;
        for (size_t i = 0; i < m_insts.size(); i++) {
            uint8_t level = 0;
            for (int32_t p = m_insts[i].lodParent; p >= 0 && level < 8; p = m_insts[p].lodParent) {
                level++;
                if (m_insts[p].lodLevel < level) m_insts[p].lodLevel = level;
            }
        }
        return lods;
    };
    recount();

    // Repair LODs the map data never links. 58 placements of lod* models in
    // Los Santos sit on top of an HD placement whose IPL LOD index is -1
    // (billboards lodlbd3/billbd3, the Santa Maria ferris-wheel seats, dock
    // ramps), or duplicate a LOD that is already linked; retail draws both, so
    // the blurry LOD shows through the HD model. Link a childless lod* placement
    // to the unlinked leaves within 2 m whose model name is related (a common
    // run of 4+ characters once "lod" and the "_region" suffix are dropped), and
    // drop exact duplicates of an already-linked LOD.
    auto stem = [](const std::string& n) {
        std::string s = n.compare(0, 3, "lod") == 0 ? n.substr(3) : n;
        size_t us = s.rfind('_');
        if (us != std::string::npos && us >= 4) s.resize(us);
        return s;
    };
    auto related = [&](const std::string& a, const std::string& b) {
        const std::string x = stem(a), y = stem(b);
        for (size_t i = 0; i + 4 <= x.size(); i++)
            if (y.find(x.substr(i, 4)) != std::string::npos) return true;
        return false;
    };
    auto key = [](float x, float y) {
        return ((int64_t)std::floor(x / 4) << 32) ^ (uint32_t)(int32_t)std::floor(y / 4);
    };
    auto isLodName = [](const std::string& n) { return n.compare(0, 3, "lod") == 0; };
    std::unordered_map<int64_t, std::vector<size_t>> cells;   // 4 m grid of leaves
    for (size_t i = 0; i < m_insts.size(); i++)
        if (m_insts[i].lodChildren == 0 && !isLodName(m_insts[i].hdModel))
            cells[key(m_insts[i].x, m_insts[i].y)].push_back(i);
    auto near2m = [](const Inst& a, const Inst& b) {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz <= 4.0f;
    };
    size_t repaired = 0, relinked = 0, twins = 0;
    for (size_t l = 0; l < m_insts.size(); l++) {
        Inst& lod = m_insts[l];
        if (lod.lodChildren != 0 || !isLodName(lod.hdModel)) continue;
        int32_t twinOf = -1;
        bool linkedAny = false;
        for (int cx = -1; cx <= 1; cx++)
            for (int cy = -1; cy <= 1; cy++) {
                auto it = cells.find(key(lod.x + cx * 4.0f, lod.y + cy * 4.0f));
                if (it == cells.end()) continue;
                for (size_t j : it->second) {
                    Inst& h = m_insts[j];
                    if (!near2m(h, lod) || !related(lod.hdModel, h.hdModel)) continue;
                    if (h.lodParent < 0) {                        // never linked
                        h.lodParent = (int32_t)l;
                        linkedAny = true;
                        repaired++;
                        continue;
                    }
                    const Inst& p = m_insts[h.lodParent];
                    if (p.hdModel == lod.hdModel && !near2m(p, h)) {
                        // Off-by-one index: linked to a copy of this LOD model
                        // elsewhere (projects01_lax) while this copy sits on it.
                        h.lodParent = (int32_t)l;
                        linkedAny = true;
                        relinked++;
                    } else if (near2m(p, lod)) {
                        twinOf = h.lodParent;   // a second LOD on the same spot
                    }
                }
            }
        // An extra LOD on top of an already-linked one (lodroadbnj under
        // lodlaeroad48) draws exactly when that LOD does.
        if (!linkedAny && twinOf >= 0) { lod.lodTwin = twinOf; twins++; }
    }
    const size_t lods = recount();
    std::fprintf(stderr, "[World] LOD placements: %zu (repaired: %zu unlinked, %zu mislinked, %zu twins)\n",
                 lods, repaired, relinked, twins);

    // GTA_LODAUDIT=1: report what is left -- childless lod* placements within
    // 2 m of a leaf, the signature of a LOD drawing over its HD model.
    if (getenv("GTA_LODAUDIT")) {
        size_t orphans = 0, overlaps = 0;
        for (const Inst& in : m_insts) {
            if (in.lodChildren != 0 || in.lodTwin >= 0 || !isLodName(in.hdModel)) continue;
            orphans++;
            for (int cx = -1; cx <= 1; cx++)
                for (int cy = -1; cy <= 1; cy++) {
                    auto it = cells.find(key(in.x + cx * 4.0f, in.y + cy * 4.0f));
                    if (it == cells.end()) continue;
                    for (size_t j : it->second) {
                        const Inst& h = m_insts[j];
                        if (!near2m(h, in)) continue;
                        overlaps++;
                        std::fprintf(stderr, "[LODAUDIT] childless LOD %s (%.1f,%.1f,%.1f) over %s (its lod: %s)\n",
                                     in.hdModel.c_str(), in.x, in.y, in.z, h.hdModel.c_str(),
                                     h.lodParent >= 0 ? m_insts[h.lodParent].hdModel.c_str() : "-");
                    }
                }
        }
        std::fprintf(stderr, "[LODAUDIT] %zu childless LOD placements, %zu overlapping a leaf\n",
                     orphans, overlaps);
    }
}

void GameWorld::LoadCols() {
    // Every COL archive in the image (CColStore keeps them all).
    auto want = [&](const std::string& en) -> bool {
        return en.size() >= 5 && en.compare(en.size() - 4, 4, ".col") == 0;
    };

    for (size_t i = 0; i < m_img->EntryCount(); i++) {
        const std::string& en = m_img->GetEntryName(i);
        if (!want(en)) continue;
        std::vector<uint8_t> d = m_img->Extract(en);
        if (d.empty()) continue;
        m_cols.push_back({en, std::move(d)});
    }
    std::fprintf(stderr, "[World] col archives: %zu\n", m_cols.size());
}

// --------------------------------------------------------------- collision

const GameWorld::ColEntryLoc* GameWorld::FindColEntry(const std::string& model) {
    auto it = m_colEntry.find(model);
    if (it != m_colEntry.end()) return &it->second;
    ColEntryLoc loc{size_t(-1), 0};
    for (size_t a = 0; a < m_cols.size(); a++) {
        const std::vector<uint8_t>& d = m_cols[a].data;
        size_t pos = 0;
        while (pos + 8 <= d.size()) {
            // COL2/COL3/COL4 share the layout up to the offset table (COL2
            // just lacks the shadow mesh); COLL (v1) is a different format.
            const uint8_t* e = d.data() + pos;
            if (memcmp(e, "COL", 3) != 0 || (e[3] != '2' && e[3] != '3' && e[3] != '4')) {
                if (memcmp(e, "COLL", 4) == 0) { pos += 8 + RD_U32(e + 4); continue; }
                pos++;
                continue;
            }
            uint32_t sz = RD_U32(e + 4);
            if (sz < 88 || sz > 1000000) {
                pos += 8 + sz;
                continue;
            }
            if (ColNameEq(e + 8, model.c_str())) {
                loc = {a, pos};
                m_colEntry[model] = loc;
                return &m_colEntry[model];
            }
            pos += 8 + sz;
        }
    }
    m_colEntry[model] = loc; // sentinel: arc == size_t(-1)
    return &m_colEntry[model];
}

bool GameWorld::ParseBoxes(size_t arc, size_t off, std::vector<WorldColBox>& out) {
    const std::vector<uint8_t>& d = m_cols[arc].data;
    if (off + 0x6C > d.size() || memcmp(d.data() + off, "COL", 3) != 0) return false;
    const uint8_t* e = d.data() + off;
    uint16_t nb = RD_U16(e + 0x4A);
    if (nb == 0 || nb > 256) return false;
    // Boxes via the offset table (+0x58, relative to entry+4): valid for COL2
    // (0x6C-byte header) and COL3 (0x78) alike.
    size_t base = off + 4 + RD_U32(e + 0x58);
    size_t count = 0;
    for (uint16_t i = 0; i < nb; i++) {
        size_t b = base + (size_t)i * 28;
        if (b + 28 > d.size()) break;
        const uint8_t* p = d.data() + b;
        float mnx = RD_F32(p + 0), mny = RD_F32(p + 4), mnz = RD_F32(p + 8);
        float mxx = RD_F32(p + 12), mxy = RD_F32(p + 16), mxz = RD_F32(p + 20);
        if (mnx > mxx || mny > mxy || mnz > mxz) continue;
        if (std::fabs(mnx) > 500 || std::fabs(mny) > 500 || std::fabs(mnz) > 500 ||
            std::fabs(mxx) > 500 || std::fabs(mxy) > 500 || std::fabs(mxz) > 500)
            continue;
        out.push_back({mnx, mny, mnz, mxx, mxy, mxz});
        count++;
    }
    return count > 0;
}

void GameWorld::BuildInstBoxes(Inst& inst) {
    inst.boxesIn = true; // never retry (memo below keeps it fast)
    const ColEntryLoc* loc = FindColEntry(inst.hdModel);
    if (getenv("GTA_COLDBG"))
        std::fprintf(stderr, "[COLDBG] boxes %s lodChildren=%d loc=%s\n", inst.hdModel.c_str(),
                     (int)inst.lodChildren,
                     (loc && loc->arc != size_t(-1)) ? m_cols[loc->arc].name.c_str() : "NONE");
    if (!loc || loc->arc == size_t(-1)) return;
    // Spheres (CVector centre, float radius, 4-byte surface = 20 bytes; table
    // at the offset in +0x54, relative to entry+4). Model-space centre -> world.
    {
        const std::vector<uint8_t>& d = m_cols[loc->arc].data;
        const uint8_t* e = d.data() + loc->off;
        uint16_t ns = RD_U16(e + 0x48);
        size_t sphBase = loc->off + 4 + RD_U32(e + 0x54);
        for (uint16_t i = 0; i < ns && ns <= 256; i++) {
            size_t s = sphBase + (size_t)i * 20;
            if (s + 20 > d.size()) break;
            const uint8_t* p = d.data() + s;
            float cx = RD_F32(p), cy = RD_F32(p + 4), cz = RD_F32(p + 8), r = RD_F32(p + 12);
            if (!(r > 0.0f && r < 100.0f)) continue;
            WorldColSphere ws;
            QuatXform(inst.qx, inst.qy, inst.qz, inst.qw, cx, cy, cz,
                      inst.x, inst.y, inst.z, ws.x, ws.y, ws.z);
            ws.r = r;
            inst.spheres.push_back(ws);
        }
    }
    std::vector<WorldColBox> modelBoxes;
    if (!ParseBoxes(loc->arc, loc->off, modelBoxes) || modelBoxes.empty()) {
        // No box section: this model's solid collision (if any) comes from its
        // COL triangle mesh instead, handled by BuildGroundTris + the wall-tri
        // test in HitsSolidCircle.
        return;
    }
    for (auto& b : modelBoxes) {
        // transform 8 corners of the model-space box into world space
        float xs[2] = {b.minX, b.maxX}, ys[2] = {b.minY, b.maxY},
              zs[2] = {b.minZ, b.maxZ};
        float mnx = 1e30f, mny = 1e30f, mnz = 1e30f;
        float mxx = -1e30f, mxy = -1e30f, mxz = -1e30f;
        WorldColObb ob;
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) {
                    float wx, wy, wz;
                    QuatXform(inst.qx, inst.qy, inst.qz, inst.qw, xs[i], ys[j],
                              zs[k], inst.x, inst.y, inst.z, wx, wy, wz);
                    mnx = std::min(mnx, wx); mny = std::min(mny, wy);
                    mnz = std::min(mnz, wz); mxx = std::max(mxx, wx);
                    mxy = std::max(mxy, wy); mxz = std::max(mxz, wz);
                    int c = i * 4 + j * 2 + k;
                    ob.x[c] = wx; ob.y[c] = wy; ob.z[c] = wz;
                }
        inst.boxes.push_back({mnx, mny, mnz, mxx, mxy, mxz});
        inst.obb.push_back(ob);
    }
}

// ----------------------------------------------------------------- terrain

void GameWorld::BuildGroundTris(Inst& inst) {
    inst.groundTrisIn = true;
    const ColEntryLoc* loc = FindColEntry(inst.hdModel);
    if (!loc || loc->arc == size_t(-1)) return;
    const std::vector<uint8_t>& d = m_cols[loc->arc].data;
    std::vector<ColTriangle> tris;
    bool ok = ColLoader::LoadModelTriangles(d.data(), d.size(), inst.hdModel.c_str(),
                                            inst.qx, inst.qy, inst.qz, inst.qw,
                                            inst.x, inst.y, inst.z, tris);
    if (getenv("GTA_COLDBG"))
        std::fprintf(stderr, "[COLDBG] tris %s arc=%s ok=%d n=%zu\n", inst.hdModel.c_str(),
                     m_cols[loc->arc].name.c_str(), (int)ok, tris.size());
    if (!ok) return;
    for (auto& t : tris) {
        // Bin into every cell the triangle's XY bbox overlaps, not just its
        // centroid cell. Large structural panels (stadium walls, bridge
        // girders) span several cells; a centroid-only bin left the player's
        // ±1-cell collision lookup unable to find a wall it was standing right
        // next to. Capped so a degenerate huge triangle can't flood the grid.
        float txmn = std::min({t.ax, t.bx, t.cx}), txmx = std::max({t.ax, t.bx, t.cx});
        float tymn = std::min({t.ay, t.by, t.cy}), tymx = std::max({t.ay, t.by, t.cy});
        int gx0 = (int)std::floor(txmn / kCell), gx1 = (int)std::floor(txmx / kCell);
        int gy0 = (int)std::floor(tymn / kCell), gy1 = (int)std::floor(tymx / kCell);
        if (gx1 - gx0 > 64) gx1 = gx0 + 64;   // guard against bad geometry
        if (gy1 - gy0 > 64) gy1 = gy0 + 64;
        for (int gx = gx0; gx <= gx1; gx++)
            for (int gy = gy0; gy <= gy1; gy++)
                m_cells[CellKey(gx, gy)].tris.push_back(t);
    }
}

int64_t GameWorld::CellKey(int gx, int gy) {
    return ((int64_t)(uint32_t)gx << 32) | (uint32_t)gy;
}

// ------------------------------------------------------------------ stream

// ------------------------------------------------------------- roadsigns
// Port of CCustomRoadsignMgr: highway/street sign text is not baked into the
// sign textures; each sign model carries 2dfx EFFECT_ROADSIGN entries whose
// lines are rendered at runtime from particle.txd's "roadsignfont".

namespace {
// RoadsignGetLineAndRow: character -> glyph (column 0-3, row 0-31) in the
// 32x512 roadsignfont. Anything else (incl. '_' and ' ') -> (0,26), blank.
struct RoadsignGlyph { uint8_t ch, col, row; };
const RoadsignGlyph kRoadsignGlyphs[] = {
    {33,0,0}, {35,3,21}, {36,0,22}, {37,2,21}, {38,2,0}, {40,0,1}, {41,1,1}, {42,1,22}, {43,2,1},
    {45,0,2}, {46,1,2}, {48,3,2}, {49,0,3}, {50,1,3}, {51,2,3}, {52,3,3}, {53,0,4}, {54,1,4}, {55,2,4},
    {56,3,4}, {57,0,5}, {58,2,5}, {59,1,5}, {60,2,20}, {62,3,20}, {63,3,5}, {64,2,22}, {65,0,6},
    {66,1,6}, {67,2,6}, {68,3,6}, {69,0,7}, {70,1,7}, {71,2,7}, {72,3,7}, {73,0,8}, {74,1,8}, {75,2,8},
    {76,3,8}, {77,0,9}, {78,1,9}, {79,2,9}, {80,3,9}, {81,0,10}, {82,1,10}, {83,2,10}, {84,3,10},
    {85,0,11}, {86,1,11}, {87,2,11}, {88,3,11}, {89,0,12}, {90,1,12}, {91,2,12}, {93,0,13}, {94,0,21},
    {97,1,13}, {98,2,13}, {99,3,13}, {100,0,14}, {101,1,14}, {102,2,14}, {103,3,14}, {104,0,15},
    {105,1,15}, {106,2,15}, {107,3,15}, {108,0,16}, {109,1,16}, {110,2,16}, {111,3,16}, {112,0,17},
    {113,1,17}, {114,2,17}, {115,3,17}, {116,0,18}, {117,1,18}, {118,2,18}, {119,3,18}, {120,0,19},
    {121,1,19}, {122,2,19}, {123,0,23}, {124,3,22}, {125,2,23}, {126,1,21},
};
void RoadsignGetLineAndRow(char c, int& col, int& row) {
    col = 0; row = 26;
    for (const RoadsignGlyph& g : kRoadsignGlyphs)
        if (g.ch == (uint8_t)c) { col = g.col; row = g.row; return; }
}
}  // namespace

void GameWorld::LoadRoadsignFont() {
    std::vector<TxdTexture> txd = TxdLoader::Load(m_gameDir + "\\models\\particle.txd");
    for (const TxdTexture& t : txd) {
        if (TxdLoader::KeyOf(t.name) != "roadsignfont") continue;
        if (t.width == 32 && t.height == 512 && t.d3dFormat == D3DFMT_A8R8G8B8 &&
            t.mip0.size() >= 32u * 512u * 4u) {
            m_signFont.resize(32 * 512);
            std::memcpy(m_signFont.data(), t.mip0.data(), m_signFont.size() * 4);
        }
    }
    std::fprintf(stderr, "[World] roadsign font: %s\n", m_signFont.empty() ? "MISSING" : "ok");
}

void GameWorld::BuildRoadsigns(const std::vector<Dff2dfxRoadsign>& signs, ModelCacheEntry& ent) {
    if (signs.empty() || m_signFont.empty()) return;
    const float kDeg = 3.14159265f / 180.0f;
    for (const Dff2dfxRoadsign& s : signs) {
        // C2dEffect::Roadsign_GetNumLines/NumLetters/PaletteIDFromFlags.
        static const int kLines[4] = {4, 1, 2, 3};
        static const int kLetters[4] = {16, 2, 4, 8};
        const int numLines = kLines[s.flags & 3];
        const int numLetters = kLetters[(s.flags >> 2) & 3];
        // CreateRoadsignAtomicA material colour: white, black, grey, red.
        static const uint32_t kPalette[4] = {0xFFFFFFFFu, 0xFF000000u, 0xFF808080u, 0xFFFF0000u};
        const uint32_t color = kPalette[(s.flags >> 4) & 3];

        // Frame: RwFrameRotate z (replace), then x, then y (post-concat), then
        // translate -- i.e. p' = Ry * Rx * Rz * p + pos for column vectors.
        float m[3][3];
        {
            float cz = std::cos(s.rotZ * kDeg), sz = std::sin(s.rotZ * kDeg);
            float cx = std::cos(s.rotX * kDeg), sx = std::sin(s.rotX * kDeg);
            float cy = std::cos(s.rotY * kDeg), sy = std::sin(s.rotY * kDeg);
            const float Rz[3][3] = {{cz, -sz, 0}, {sz, cz, 0}, {0, 0, 1}};
            const float Rx[3][3] = {{1, 0, 0}, {0, cx, -sx}, {0, sx, cx}};
            const float Ry[3][3] = {{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}};
            float t[3][3];
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) {
                    t[i][j] = 0;
                    for (int k = 0; k < 3; k++) t[i][j] += Rx[i][k] * Rz[k][j];
                }
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) {
                    m[i][j] = 0;
                    for (int k = 0; k < 3; k++) m[i][j] += Ry[i][k] * t[k][j];
                }
        }
        auto xf = [&](float lx, float ly, MeshVertex& v) {
            v.x = m[0][0] * lx + m[0][1] * ly + s.x;
            v.y = m[1][0] * lx + m[1][1] * ly + s.y;
            v.z = m[2][0] * lx + m[2][1] * ly + s.z;
            v.nx = m[0][2]; v.ny = m[1][2]; v.nz = m[2][2];   // local +Z
            v.color = color;
        };

        const float lineH = s.height / (float)numLines;
        for (int line = 0; line < numLines; line++) {
            // RoadsignGenerateTextRaster: numLetters 8x16 glyphs side by side.
            const int tw = numLetters * 8, th = 16;
            std::vector<uint32_t> px((size_t)tw * th, 0u);
            for (int c = 0; c < numLetters; c++) {
                int col, row;
                RoadsignGetLineAndRow(s.text[line][c], col, row);
                for (int y = 0; y < 16; y++)
                    for (int x = 0; x < 8; x++)
                        px[(size_t)y * tw + c * 8 + x] = m_signFont[(size_t)(row * 16 + y) * 32 + col * 8 + x];
            }
            IDirect3DTexture9* tex = m_renderer->CreateTexture(
                (uint32_t)tw, (uint32_t)th, D3DFMT_A8R8G8B8, px.data(), (uint32_t)(px.size() * 4));
            if (!tex) continue;

            // CreateRoadsignAtomicA: line quads stacked top-down around the
            // sign origin, each 95% of its slot tall, U across, V down.
            const float off = ((float)numLines * 0.5f - 1.0f - (float)line) * lineH;
            const float h = lineH * 0.95f, hw = s.width * 0.5f;
            MeshVertex v[4];
            xf(-hw, off, v[0]);     v[0].u = 0; v[0].v = 1;
            xf(hw, off, v[1]);      v[1].u = 1; v[1].v = 1;
            xf(hw, off + h, v[2]);  v[2].u = 1; v[2].v = 0;
            xf(-hw, off + h, v[3]); v[3].u = 0; v[3].v = 0;
            const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
            D3DRenderMesh* mesh = m_renderer->CreateMesh(v, 4, idx, 6, true);
            if (!mesh) { m_renderer->DestroyTexture(tex); continue; }
            mesh->texture = tex;
            mesh->alphaBlend = true;     // anti-aliased white glyphs on alpha 0
            mesh->doubleSided = false;   // retail draws the atomic culled
            ent.signMeshes.push_back(mesh);
            ent.signTex.push_back(tex);
        }
    }
}

void GameWorld::FreeModel(ModelCacheEntry& e) {
    for (D3DRenderMesh* m : e.meshes)
        if (m) m_renderer->DestroyMesh(m);
    for (D3DRenderMesh* m : e.signMeshes)
        if (m) m_renderer->DestroyMesh(m);
    for (IDirect3DTexture9* t : e.signTex)
        if (t) m_renderer->DestroyTexture(t);
    e.meshes.clear();
    e.signMeshes.clear();
    e.signTex.clear();
}

bool GameWorld::LoadModel(const std::string& modelName, const std::string& txdName) {
    if (m_badModels.count(modelName)) return false;
    auto cit = m_cache.find(modelName);
    if (cit != m_cache.end()) return !cit->second.meshes.empty();

    if (!m_img->HasFile(modelName + ".dff")) {
        m_badModels.insert(modelName);
        return false;
    }
    std::vector<uint8_t> dff = m_img->Extract(modelName + ".dff");
    DffModel dm = DffLoader::LoadFromMemory(dff.data(), dff.size());
    if (!dm.valid || dm.meshes.empty()) {
        m_badModels.insert(modelName);
        return false;
    }

    ModelCacheEntry ent;
    ent.txdName = txdName;
    ent.lastSeen = m_frame;
    if (!txdName.empty()) LoadTxd(txdName);

    // Bounding-sphere radius about the local origin (rotation-invariant), used
    // for frustum culling in Render(). Max vertex distance over all meshes; also
    // track the local Z range so lamp bulbs can be located at the top.
    float maxR2 = 0.0f;
    float minZ = 1e30f, maxZ = -1e30f;
    for (auto& m : dm.meshes) {
        const MeshVertex* mv = reinterpret_cast<const MeshVertex*>(m.vertices.data());
        for (size_t vi = 0; vi < m.vertices.size(); vi++) {
            float d2 = mv[vi].x * mv[vi].x + mv[vi].y * mv[vi].y + mv[vi].z * mv[vi].z;
            if (d2 > maxR2) maxR2 = d2;
            if (mv[vi].z < minZ) minZ = mv[vi].z;
            if (mv[vi].z > maxZ) maxZ = mv[vi].z;
        }
    }
    ent.radius = std::sqrt(maxR2);

    // 2dfx EFFECT_LIGHT markers -> traffic-light bulbs. The authored colour
    // encodes which signal (same rule CTrafficLights::DisplayActualLight uses):
    // green = low red; amber = high red + green; red = high red, low green.
    for (auto& L : dm.lights) {
        int idx = (L.r < 0xc9) ? 0 : (2 - (L.g > 100 ? 1 : 0)); // 0 grn,1 amb,2 red
        ent.tlBulbs.push_back({L.x, L.y, L.z, idx});
    }
    BuildRoadsigns(dm.roadsigns, ent);

    // Lamp bulb(s): the lamp head(s) sit in the top ~12% of the model. A single
    // arm gives one head (offset to one side of the pole); a double freeway lamp
    // has an arm on each side. Detect by splitting the top vertices across the
    // pole axis (the pole is at the model's local x/y origin): clusters on both
    // sides with enough separation => two bulbs, otherwise one.
    if (IsLampModel(modelName) && maxZ > minZ) {
        const float band = maxZ - 0.12f * (maxZ - minZ);
        double cx = 0, cy = 0, cz = 0; size_t nTop = 0;
        float sxMin = 1e30f, sxMax = -1e30f, syMin = 1e30f, syMax = -1e30f;
        for (auto& m : dm.meshes) {
            const MeshVertex* mv = reinterpret_cast<const MeshVertex*>(m.vertices.data());
            for (size_t vi = 0; vi < m.vertices.size(); vi++) {
                if (mv[vi].z < band) continue;
                cx += mv[vi].x; cy += mv[vi].y; cz += mv[vi].z; nTop++;
                sxMin = std::min(sxMin, mv[vi].x); sxMax = std::max(sxMax, mv[vi].x);
                syMin = std::min(syMin, mv[vi].y); syMax = std::max(syMax, mv[vi].y);
            }
        }
        if (nTop > 0) {
            cx /= nTop; cy /= nTop; cz /= nTop;
            const bool axisX = (sxMax - sxMin) >= (syMax - syMin);  // arm direction
            // Split across the pole (local origin 0 on the arm axis).
            double px = 0, py = 0, pz = 0; size_t pn = 0;
            double nx = 0, ny = 0, nz = 0; size_t nn = 0;
            for (auto& m : dm.meshes) {
                const MeshVertex* mv = reinterpret_cast<const MeshVertex*>(m.vertices.data());
                for (size_t vi = 0; vi < m.vertices.size(); vi++) {
                    if (mv[vi].z < band) continue;
                    float coord = axisX ? mv[vi].x : mv[vi].y;
                    if (coord >= 0.0f) { px += mv[vi].x; py += mv[vi].y; pz += mv[vi].z; pn++; }
                    else               { nx += mv[vi].x; ny += mv[vi].y; nz += mv[vi].z; nn++; }
                }
            }
            bool twoArms = false;
            if (pn > nTop / 5 && nn > nTop / 5) {  // both sides substantially populated
                float pcx = (float)(px/pn), pcy = (float)(py/pn), pcz = (float)(pz/pn);
                float ncx = (float)(nx/nn), ncy = (float)(ny/nn), ncz = (float)(nz/nn);
                float sdx = pcx-ncx, sdy = pcy-ncy;
                if (sdx*sdx + sdy*sdy > 1.0f) {   // arms at least ~1m apart
                    ent.bulb[0][0]=pcx; ent.bulb[0][1]=pcy; ent.bulb[0][2]=pcz;
                    ent.bulb[1][0]=ncx; ent.bulb[1][1]=ncy; ent.bulb[1][2]=ncz;
                    ent.bulbCount = 2; twoArms = true;
                }
            }
            if (!twoArms) {
                ent.bulb[0][0]=(float)cx; ent.bulb[0][1]=(float)cy; ent.bulb[0][2]=(float)cz;
                ent.bulbCount = 1;
            }
        }
    }

    // Backface culling comes from the IDE flags, exactly as retail:
    // CBaseModelInfo::SetBaseModelInfoFlags sets the "backface culled" bit
    // unless IDE flag bit 21 (0x200000, "disable backface culling") is set.
    // (Was a "texture has alpha => two-sided" heuristic, which drew the back
    // panel of billbd3 two-sided so it z-fought the skatepark's ad face.)
    bool noCull = false;
    {
        auto bit = m_byName.find(modelName);
        if (bit != m_byName.end()) noCull = (bit->second.flags & 0x200000) != 0;
    }

    for (auto& m : dm.meshes) {
        D3DRenderMesh* mesh = m_renderer->CreateMesh(
            reinterpret_cast<const MeshVertex*>(m.vertices.data()),
            static_cast<uint32_t>(m.vertices.size()), m.indices.data(), static_cast<uint32_t>(m.indices.size()), m.hasPrelit,
            m.nightColors.empty() ? nullptr : m.nightColors.data());
        if (!mesh) continue;
        mesh->texture = GetTexture(txdName, m.textureName);
        mesh->doubleSided = noCull;
        // Retail draws soft-alpha textures (road/sand decals, foliage cards,
        // glass) with source-alpha blending; without it those texels render
        // as opaque black.
        {
            std::string tkey = txdName + "/" + TxdLoader::KeyOf(m.textureName);
            auto ti = m_texInfo.find(tkey);
            if (ti != m_texInfo.end()) {
                // Soft (intermediate) alpha also blends with no depth write;
                // cutout alpha keeps the Init alpha test (opaque, writes depth).
                if (ti->second->softAlpha) mesh->alphaBlend = true;
            }
        }
        ent.meshes.push_back(mesh);
    }
    if (ent.meshes.empty()) {
        m_badModels.insert(modelName);
        return false;
    }

    if (!txdName.empty()) m_txdRefs[txdName]++;
    m_cache[modelName] = std::move(ent);
    return true;
}

bool GameWorld::LoadTxd(const std::string& txdName) {
    if (m_txdPool.count(txdName)) return true; // already resident
    std::vector<uint8_t> raw = m_img->Extract(txdName + ".txd");
    if (raw.empty()) return false;
    std::vector<TxdTexture> tex = TxdLoader::LoadFromMemory(raw.data(), raw.size());
    if (tex.empty()) return false;
    if (getenv("GTA_TXDDBG")) {
        std::fprintf(stderr, "[TXDDBG] %s: %zu textures:", txdName.c_str(), tex.size());
        for (auto& t : tex) std::fprintf(stderr, " %s", t.name.c_str());
        std::fprintf(stderr, "\n");
    }
    // A newly-resident txd may now satisfy a name that previously missed in
    // every txd, so drop the negative cache (see GetTexture cross-txd search).
    m_texMiss.clear();
    // Texture cache keys are scoped per TXD: GTA models take their textures from
    // their IDE txd, and texture names repeat across txds ("black64" is in many),
    // so a global name-keyed cache let one txd's unload free a D3D texture that
    // still-resident meshes of another txd were pointing at.
    std::vector<std::string> keys;
    for (size_t i = 0; i < tex.size(); i++) {
        std::string key = txdName + "/" + TxdLoader::KeyOf(tex[i].name);
        keys.push_back(key);
        m_texInfo[key] = &tex[i]; // pointer into m_txdPool element (stable)
    }
    m_txdPool[txdName] = std::move(tex);
    m_txdTexKeys[txdName] = std::move(keys);
    return true;
}

IDirect3DTexture9* GameWorld::GetTexture(const std::string& txdName,
                                         const std::string& texName) {
    if (texName.empty()) return nullptr;
    std::string want = TxdLoader::KeyOf(texName);
    std::string key = txdName + "/" + want;
    auto dt = m_d3dTex.find(key);
    if (dt != m_d3dTex.end()) return dt->second;

    const TxdTexture* src = nullptr;
    bool borrowed = false;
    auto ti = m_texInfo.find(key);
    if (ti != m_texInfo.end()) {
        src = ti->second;                 // present in the model's own txd
    } else if (!m_texMiss.count(key)) {
        // SA resolves a texture missing from a model's own txd through the txd
        // parent chain: RW's FindNamedTexture walks the dictionary's parent
        // pointer (set up from the "txdp" sections -> m_txdParent). Walk that
        // chain, loading each parent txd on demand (the gb_* globals hold the
        // shared/LOD textures). Cycle-guarded by a depth cap.
        std::string cur = txdName;
        for (int depth = 0; depth < 16 && !src; depth++) {
            auto pit = m_txdParent.find(cur);
            if (pit == m_txdParent.end()) break;
            const std::string& parent = pit->second;
            if (parent == cur) break;     // self-parent guard
            LoadTxd(parent);              // no-op if already resident / absent
            auto pti = m_texInfo.find(parent + "/" + want);
            if (pti != m_texInfo.end()) {
                src = pti->second;
                borrowed = true;
            }
            cur = parent;
        }
        // Last resort: scan any resident txd that defines this texture name.
        // Catches cross-txd cases with no txdp link (e.g. nitwin01_la lives in
        // nitelites_lae2, referenced by a model whose txd is nitewin_las).
        // Not for LOD txds: whether the HD txd happens to be resident would
        // decide how a distant LOD looks, so they resolve like retail.
        if (!src && txdName.compare(0, 3, "lod") != 0) {
            const std::string suffix = "/" + want;
            for (auto& kv : m_texInfo) {
                if (kv.first.size() > suffix.size() &&
                    kv.first.compare(kv.first.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    src = kv.second;
                    borrowed = true;
                    break;
                }
            }
        }
    }
    if (!src) {
        if (m_texMiss.insert(key).second) {
            static std::set<std::string> s_missLogged;
            if (s_missLogged.insert(key).second && std::getenv("GTA_TEXMISS"))   // retail misses these too; opt-in log
                std::fprintf(stderr, "[World] TEXMISS %s\n", key.c_str());
        }
        // Still nowhere: draw it untextured, like retail. RW's texture find
        // stops after the txd parent chain and leaves the material without a
        // texture, so the prelit vertex colours alone shade it. Many LA LOD
        // models (lod2lae1: comptwall36, adeta, bigblue2, ...) reference HD
        // texture names their LOD txd lacks; these used to show as pink
        // until the HD model swapped in. A 1x1 white texel = no texture.
        if (!m_fallbackTex) {
            static const uint8_t white[4] = {0xFF, 0xFF, 0xFF, 0xFF}; // BGRA
            m_fallbackTex = m_renderer->CreateTexture(1, 1, D3DFMT_A8R8G8B8,
                                                     white, 4);
        }
        return m_fallbackTex;
    }
    const TxdTexture& t = *src;
    IDirect3DTexture9* tex = m_renderer->CreateTexture(t.width, t.height,
                                                      t.d3dFormat, t.mip0.data(),
                                                      (uint32_t)t.mip0.size(),
                                                      &t.extraMips);
    if (!tex) return m_fallbackTex;
    // Cache (and upload) an independent copy under the REQUESTING key, and
    // register it with the requesting txd so it's freed when that txd unloads
    // -- even when the pixels were borrowed from another (still-resident) txd.
    m_d3dTex[key] = tex;
    if (borrowed) m_txdTexKeys[txdName].push_back(key);
    return tex;
}

// Max COL ground-tri builds per Stream frame (task 1: house lots need tris).
static const int kMaxColBuildsPerFrame = 40;

void GameWorld::Stream(float px, float py, float pz) {
    (void)pz;
    m_frame++;
    m_visInsts = 0;

    // Reset refcounts.
    for (auto& kv : m_cache) kv.second.refs = 0;

    // Per-instance view radius: retail keeps a model alive out to its IDE draw
    // distance (HD) or the LOD's draw distance, and nothing lives past the
    // timecyc FarClp + load margin. Ground/terrain instances keep a floor so
    // the player never falls off the collision mesh.
    const float capR = m_farClip + kLoadMargin;

    struct Near { uint32_t distSq; size_t idx; };
    std::vector<Near> nearList;
    nearList.reserve(1024);
    for (size_t i = 0; i < m_insts.size(); i++) {
        Inst& in = m_insts[i];
        float dx = in.x - px, dy = in.y - py;
        uint32_t dsq = (uint32_t)(dx * dx + dy * dy);
        float r = kStreamRadius;
        if (in.isGround) r = std::max(r, kGroundRadius);
        const float drawDist = DrawDist(in);
        if (!in.hdModel.empty() && drawDist > r) r = drawDist;
        r += kLoadMargin;
        if (r > m_farClip + kLoadMargin) {
            r = in.isGround ? std::max(m_farClip + kLoadMargin, kGroundRadius)
                            : m_farClip + kLoadMargin;
        }
        uint32_t lodDsq = dsq;
        if (in.lodParent >= 0) {
            const Inst& p = m_insts[in.lodParent];
            float lx = p.x - px, ly = p.y - py;
            lodDsq = (uint32_t)(lx * lx + ly * ly);
        }
        const uint32_t r2 = (uint32_t)(r * r);
        in.needed = dsq <= r2 || lodDsq <= r2;
        if (!in.needed) continue;
        in.distSq = dsq;
        in.lodDistSq = lodDsq;
        m_visInsts++;
        nearList.push_back({dsq, i});
    }
    std::sort(nearList.begin(), nearList.end(),
              [](const Near& a, const Near& b) { return a.distSq < b.distSq; });

    // Ground collision triangles (lazy, budgeted). Build for every instance
    // with COL, not just ground-flagged ones — house lots, sidewalks, and
    // building floors need ground tris or the player falls through (task 1).
    {
        int colBuilds = 0;
        auto colStart = std::chrono::steady_clock::now();
        for (auto& n : nearList) {
            Inst& in = m_insts[n.idx];
            if (in.groundTrisIn) continue;
            if (colBuilds >= kMaxColBuildsPerFrame) break;
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - colStart).count() > 5) break;
            BuildGroundTris(in);
            colBuilds++;
        }
        m_lastColBuilds = (size_t)colBuilds;
        m_lastColMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - colStart).count();
    }

    // Decide what each needed inst wants, load with budget, count refs.
    auto wantName = [this](const Inst& in, float dist, std::string& name,
                           std::string& txd) -> bool {
        if (dist < DrawDist(in) + kLoadMargin) {
            name = in.hdModel;
            txd = in.hdTxd;
            return true;
        }
        return false; // nothing to draw at this distance (its LOD covers it)
    };

    std::unordered_map<std::string, bool> tried;
    int budget = kMaxLoadsPerFrame;
    int modelsLoaded = 0;
    const auto loadStart = std::chrono::steady_clock::now();
    for (auto& n : nearList) {
        Inst& in = m_insts[n.idx];
        float dist = std::sqrt((float)in.lodDistSq);
        std::string want, txd;
        if (!wantName(in, dist, want, txd)) continue;
        auto cit = m_cache.find(want);
        if (cit != m_cache.end()) {
            cit->second.refs++;
            cit->second.lastSeen = m_frame;
            continue;
        }
        if (!tried.count(want)) {
            if (budget > 0) {
                modelsLoaded++;
                if (LoadModel(want, txd)) {
                    auto it = m_cache.find(want);
                    if (it != m_cache.end()) {
                        it->second.refs = 1;
                        it->second.lastSeen = m_frame;
                    }
                }
                budget--;
                // Bound the per-frame streaming stall: once we've spent the time
                // budget, let the remaining models wait for the next frame
                // (nearList is distance-sorted, so the closest already loaded).
                double ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - loadStart).count();
                if (ms >= kLoadBudgetMs) budget = 0;
            }
            tried[want] = true; // dedup even when out of budget this frame
        }
    }
    m_lastModelsLoaded = (size_t)modelsLoaded;
    m_lastLoadMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - loadStart).count();

    // Solid boxes + spheres for every instance near the player. Retail treats
    // every COL box as solid regardless of what the model "is": the old
    // "isGround => no boxes" rule (name heuristics: *road*, *_ground*) threw
    // away real buildings -- Willowfield's Ammu-Nation is a box inside the
    // roadsbx_las2 chunk. Collision also no longer waits for the render
    // model to stream in.
    m_worldBoxes.clear();
    m_worldObbs.clear();
    m_worldSpheres.clear();
    m_worldBoxOwner.clear();
    const uint32_t colR2 = (uint32_t)(kColRadius * kColRadius);
    int boxBuilds = 0;   // first-time builds per frame (nearest first)
    for (auto& n : nearList) {
        if (n.distSq > colR2) break;   // distance-sorted
        Inst& in = m_insts[n.idx];
        // No "standalone" skip: text-IPL placements include real HD models
        // (roadsbx_las2, the Ammu-Nation block, is one); true LOD models just
        // have no COL entry, so FindColEntry finds nothing for them.
        if (!in.boxesIn) {
            if (boxBuilds >= 96) continue;
            BuildInstBoxes(in);
            boxBuilds++;
        }
        for (auto& b : in.boxes) { m_worldBoxes.push_back(b); m_worldBoxOwner.push_back((uint32_t)n.idx); }
        for (auto& o : in.obb) m_worldObbs.push_back(o);
        for (auto& s : in.spheres) m_worldSpheres.push_back(s);
    }

    // Unload models nobody wants anymore (after a grace period).
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        ModelCacheEntry& e = it->second;
        if (e.refs == 0 && m_frame - e.lastSeen > kUnloadGrace) {
            FreeModel(e);
            if (!e.txdName.empty()) {
                int& r = m_txdRefs[e.txdName];
                if (--r <= 0) {
                    // free all textures that came from this txd
                    auto tk = m_txdTexKeys.find(e.txdName);
                    if (tk != m_txdTexKeys.end()) {
                        for (auto& key : tk->second) {
                            auto dt = m_d3dTex.find(key);
                            if (dt != m_d3dTex.end()) {
                                m_renderer->DestroyTexture(dt->second);
                                m_d3dTex.erase(dt);
                            }
                            m_texInfo.erase(key);
                        }
                        m_txdTexKeys.erase(tk);
                    }
                    m_txdRefs.erase(e.txdName);
                    m_txdPool.erase(e.txdName);
                }
            }
            it = m_cache.erase(it);
        } else {
            ++it;
        }
    }
}

// ------------------------------------------------------------------- render

D3DMATRIX GameWorld::InstTransform(const Inst& in) const {
    return QuatToMatrix(in.qx, in.qy, in.qz, in.qw, in.x, in.y, in.z);
}

void GameWorld::Render(D3DRenderer& r, float px, float py, float pz) {
    (void)px; (void)py; (void)pz;
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    m_lastDrawnInsts = 0; m_lastDrawMeshCalls = 0; m_lastSkippedInsts = 0;
    // Frustum for culling off-screen instances (biggest draw-call saving: the
    // stream radius is 360 degrees, so ~half the "needed" set is behind the camera).
    const Frustum fr = MakeFrustum(r.GetViewMatrix(), r.GetProjMatrix());
    // LOD visibility, bottom-up like CRenderer::SetupBigBuildingVisibility: a
    // placement draws when its model is resident and it is inside its draw
    // distance; a LOD additionally hides while any child draws, so the HD
    // model and its LOD never overlap. A leaf past its distance keeps drawing
    // only while its LOD is not resident (no holes while streaming).
    auto loaded = [this](const Inst& in) -> const ModelCacheEntry* {
        auto it = m_cache.find(in.hdModel);
        return (it != m_cache.end() && !it->second.meshes.empty()) ? &it->second : nullptr;
    };
    uint8_t maxLevel = 0;
    for (Inst& in : m_insts) {
        in.draw = false;
        in.childDrawn = false;
        if (in.needed && in.lodLevel > maxLevel) maxLevel = in.lodLevel;
    }
    for (uint8_t level = 0; level <= maxLevel; level++) {
        for (Inst& in : m_insts) {
            if (!in.needed || in.lodLevel != level) continue;
            if (in.childDrawn || !loaded(in)) continue;
            const float dist = std::sqrt((float)in.lodDistSq);
            bool draw = dist < DrawDist(in) + 5.0f;
            if (!draw && in.lodChildren == 0) {   // fallback: HD resident, LOD not
                const Inst* p = in.lodParent >= 0 ? &m_insts[in.lodParent] : nullptr;
                draw = !p || !p->needed || !loaded(*p);
            }
            in.draw = draw;
            if (draw && in.lodParent >= 0) m_insts[in.lodParent].childDrawn = true;
        }
    }
    for (Inst& in : m_insts)
        if (in.lodTwin >= 0 && in.draw) in.draw = m_insts[in.lodTwin].draw;

    for (auto& n : m_insts) {
        const Inst& in = n;
        if (!in.needed) continue;
        if (!in.draw) { ++m_lastSkippedInsts; continue; }
        auto hit = m_cache.find(in.hdModel);
        const std::vector<D3DRenderMesh*>* meshes = &hit->second.meshes;
        const float rad = hit->second.radius;
        // Cull instances whose bounding sphere is entirely outside the frustum.
        if (!SphereInFrustum(fr, in.x, in.y, in.z, rad + 2.0f)) {
            ++m_lastSkippedInsts; continue;
        }
        D3DMATRIX m = InstTransform(in);
        dev->SetTransform(D3DTS_WORLD, &m);
        ++m_lastDrawnInsts;
        // Opaque pass: draw non-blended meshes now; defer blended ones (glass,
        // decals, soft foliage) to RenderAlpha so they draw after the player
        // and back-to-front against each other.
        for (D3DRenderMesh* mesh : *meshes) {
            if (mesh->alphaBlend) {
                m_alphaQueue.push_back({in.distSq, m, mesh});
                continue;
            }
            ++m_lastDrawMeshCalls;
            r.DrawMesh(mesh);
        }
        // Roadsign text: HD model only, out to 500 m like
        // CCustomRoadsignMgr::RenderRoadsignAtomic; blended after the panel.
        // Retail never parents the sign frame to the entity (CreateEffects
        // only rotates + translates it by the 2dfx data), so the 2dfx position
        // is absolute world space: draw with an identity world matrix.
        if (in.distSq <= 250000u && !hit->second.signMeshes.empty()) {
            static const D3DMATRIX kIdentity = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
            for (D3DRenderMesh* sm : hit->second.signMeshes)
                m_alphaQueue.push_back({in.distSq, kIdentity, sm});
        }
    }
}

void GameWorld::RenderAlpha(D3DRenderer& r) {
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev || m_alphaQueue.empty()) return;
    // Back-to-front: farthest transparent surface first, so nearer glass blends
    // over what's already in the framebuffer behind it.
    std::sort(m_alphaQueue.begin(), m_alphaQueue.end(),
              [](const AlphaDraw& a, const AlphaDraw& b) { return a.distSq > b.distSq; });
    for (const AlphaDraw& d : m_alphaQueue) {
        dev->SetTransform(D3DTS_WORLD, &d.world);
        ++m_lastDrawMeshCalls;
        r.DrawMesh(d.mesh);
    }
    m_alphaQueue.clear();
}

void GameWorld::BuildLampEmitters() {
    m_lampEmitters.clear();
    m_tlEmitters.clear();
    for (uint32_t i = 0; i < m_insts.size(); i++) {
        const Inst& in = m_insts[i];
        if (IsLampModel(in.hdModel)) m_lampEmitters.push_back(i);
        if (IsTrafficLightModel(in.hdModel)) m_tlEmitters.push_back(i);
    }
    std::fprintf(stderr, "[World] lamp emitters: %zu, traffic lights: %zu\n",
                 m_lampEmitters.size(), m_tlEmitters.size());
}

void GameWorld::RenderLamps(D3DRenderer& r, float camX, float camY, float camZ,
                            float intensity) {
    if (intensity <= 0.01f || m_lampEmitters.empty()) return;
    if (getenv("GTA_NOLAMP")) return;
    const Frustum fr = MakeFrustum(r.GetViewMatrix(), r.GetProjMatrix());
    // Distant lamps were lighting up the whole skyline. Pull the view radius in
    // and start the fade much earlier, then square it, so a lamp's glow falls
    // off quickly with distance and only nearby lamps read as bright.
    const float viewR = 150.0f, viewR2 = viewR * viewR, fadeStart = viewR * 0.30f;
    std::vector<D3DRenderer::GlowSprite> coronas;
    coronas.reserve(512);
    for (uint32_t idx : m_lampEmitters) {
        const Inst& in = m_insts[idx];
        float dx = in.x - camX, dy = in.y - camY, dz = in.z - camZ;
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > viewR2) continue;
        float d = std::sqrt(d2);
        float fade = d <= fadeStart ? 1.0f
                   : std::max(0.0f, 1.0f - (d - fadeStart) / (viewR - fadeStart));
        fade *= fade;   // quadratic: distant lamps dim much faster than near ones
        float a = intensity * fade;
        // Additive glow: the sun-glow texture has a near-white centre, so a
        // high core alpha blows out to a white starburst. Keep core modest and
        // small so the lamp reads as a contained warm amber bloom, not a flare.
        uint8_t core = (uint8_t)(a * 60.0f);
        uint8_t halo = (uint8_t)(a * 26.0f);

        // Bulb position(s): model-local bulb offset(s) (from whichever lamp mesh
        // is resident) rotated by the instance, so each glow sits on its lamp
        // head. A double freeway lamp has two. Fall back to a point up the pole
        // if the model has not streamed in yet (distant lamps only).
        const ModelCacheEntry* ce = nullptr;
        auto hit = m_cache.find(in.hdModel);
        if (hit != m_cache.end() && hit->second.bulbCount > 0) ce = &hit->second;
        int nb = ce ? ce->bulbCount : 1;
        for (int b = 0; b < nb; b++) {
            float bx = in.x, by = in.y, bz = in.z + 6.0f;
            if (ce)
                QuatXform(in.qx, in.qy, in.qz, in.qw,
                          ce->bulb[b][0], ce->bulb[b][1], ce->bulb[b][2],
                          in.x, in.y, in.z, bx, by, bz);
            if (!SphereInFrustum(fr, bx, by, bz, 6.0f)) continue;
            // Two camera-facing sprites per bulb: a bright core + a dim halo.
            coronas.push_back({ bx, by, bz, 3.4f, (uint32_t)D3DCOLOR_ARGB(halo, 255, 170, 92) });
            coronas.push_back({ bx, by, bz, 1.2f, (uint32_t)D3DCOLOR_ARGB(core, 255, 185, 104) });
        }
    }
    r.DrawCoronas(coronas.data(), (int)coronas.size());
}

void GameWorld::RenderTrafficLights(D3DRenderer& r, float camX, float camY, float camZ,
                                    uint32_t timeMs, float dayNight) {
    if (m_tlEmitters.empty()) return;
    const Frustum fr = MakeFrustum(r.GetViewMatrix(), r.GetProjMatrix());
    const D3DMATRIX& view = r.GetViewMatrix();   // row-vector view: column 3 = look dir
    const float camFx = view._13, camFy = view._23, camFz = view._33;
    const float viewR = 130.0f, viewR2 = viewR * viewR;
    const uint32_t uCars = (timeMs >> 1) & 0x3fff;   // CTrafficLights car phase
    // Pedestrian signals. Retail (CTrafficLights::LightForPeds) runs ONE
    // global ped phase: walk only in the ~7 s all-red window (u 12000-16383),
    // so a crossing whose cars went red at u 6000 kept "don't walk" for 12 s.
    // Realistic per-crossing timing instead: a ped head follows the car axis
    // it CROSSES -- WALK as soon as that axis turns red, flashing red hand for
    // the last kPedClear before it turns green again, solid red hand while it
    // has green/amber. (Future ped AI must use the same rule.)
    const uint32_t kPedClear = 2500;            // ~5 s clearance (u = ms/2)
    auto pedStateFor = [&](int crossedType) -> int {   // 0 walk, 1 flash, 2 solid
        int carState = (crossedType == 1) ? TL_LightForCars1(uCars) : TL_LightForCars2(uCars);
        if (carState != 2) return 2;
        const uint32_t greenStart = (crossedType == 1) ? 0u : 6000u;
        uint32_t toGreen = (greenStart + 0x4000u - uCars) & 0x3fffu;
        return toGreen > kPedClear ? 0 : 1;
    };
    bool pedFlashOn = (timeMs & 0x100) != 0;
    // Signals are lit day and night; a touch brighter at night.
    float bright = 0.6f + 0.4f * dayNight;
    uint8_t aCore = (uint8_t)(std::min(1.0f, bright) * 255.0f);
    uint8_t aHalo = (uint8_t)(std::min(1.0f, bright) * 110.0f);

    std::vector<D3DRenderer::GlowSprite> glow;
    glow.reserve(64);
    auto addGlow = [&](float wx, float wy, float wz, uint8_t rr, uint8_t gg, uint8_t bb) {
        // Nudge the glow slightly toward the camera so it clears the recessed
        // lens/housing face (otherwise the billboard centre z-fights / hides).
        float dx = camX - wx, dy = camY - wy, dz = camZ - wz;
        float l = std::sqrt(dx*dx + dy*dy + dz*dz);
        if (l > 1e-3f) { float k = 0.18f / l; wx += dx*k; wy += dy*k; wz += dz*k; }
        if (!SphereInFrustum(fr, wx, wy, wz, 1.0f)) return;
        glow.push_back({wx, wy, wz, 0.55f, (uint32_t)D3DCOLOR_ARGB(aHalo, rr, gg, bb)});
        glow.push_back({wx, wy, wz, 0.22f, (uint32_t)D3DCOLOR_ARGB(aCore, rr, gg, bb)});
    };

    for (uint32_t idx : m_tlEmitters) {
        const Inst& in = m_insts[idx];
        float dx = in.x - camX, dy = in.y - camY, dz = in.z - camZ;
        if (dx*dx + dy*dy + dz*dz > viewR2) continue;
        const ModelCacheEntry* ce = nullptr;
        auto hit = m_cache.find(in.hdModel);
        if (hit != m_cache.end() && !hit->second.tlBulbs.empty()) ce = &hit->second;
        if (!ce) continue;

        // Knocked-over signals stay dark (retail: matrix up.z >= 0.96).
        float upx, upy, upz;
        QuatXform(in.qx, in.qy, in.qz, in.qw, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, upx, upy, upz);
        if (upz < 0.96f) continue;
        // Orientation -> axis type -> current car-signal colour (decomp-exact).
        float fwx, fwy, fwz;
        QuatXform(in.qx, in.qy, in.qz, in.qw, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, fwx, fwy, fwz);
        // Only the heads facing the camera light up: retail compares the
        // camera's look direction with the light's forward axis and draws the
        // bulbs on the opposite local-Y side (DisplayActualLight).
        const bool camAlong = camFx * fwx + camFy * fwy + camFz * fwz > 0.0f;
        float deg = std::atan2(fwy, fwx) * 57.29578f;
        if (deg < 0.0f) deg += 360.0f;
        int type = TL_TypeFromOrientation(deg);
        int state = (type == 1) ? TL_LightForCars1(uCars) : TL_LightForCars2(uCars);

        for (auto& bulb : ce->tlBulbs) {
            if (bulb.colorIdx != state) continue;
            if ((bulb.y > 0.0f) == camAlong) continue;
            float wx, wy, wz;
            QuatXform(in.qx, in.qy, in.qz, in.qw, bulb.x, bulb.y, bulb.z,
                      in.x, in.y, in.z, wx, wy, wz);
            addGlow(wx, wy, wz, kTLColR[state], kTLColG[state], kTLColB[state]);
        }

        // Pedestrian walk/don't-walk light. Offsets are the two corners of the
        // ped-signal quad from CTrafficLights::DisplayActualLight, per model;
        // the glow sits at their midpoint. Green = walk, flashing red =
        // clearance, solid red = don't walk.
        const float* pc = nullptr;   // {x1,y1,z1, x2,y2,z2}
        static const float kPedTL1[6] = { 2.673f, 1.262f, -0.539f,  2.673f, 0.713f, -0.127f };
        static const float kPedTL4[6] = {-0.141f,-0.157f, 1.607f,  0.189f,-0.157f, 1.848f };
        static const float kPedTL5[6] = {-0.044f,-0.261f, 1.561f,  0.222f,-0.261f, 1.867f };
        if (in.hdModel.find("trafficlight1") != std::string::npos)          pc = kPedTL1;
        else if (in.hdModel.find("cj_traffic_light4") != std::string::npos ||
                 in.hdModel.find("gay_traffic_light") != std::string::npos) pc = kPedTL4;
        else if (in.hdModel.find("cj_traffic_light5") != std::string::npos) pc = kPedTL5;
        if (pc) {
            // Which car axis these peds cross: the car heads face local +-Y.
            // A ped quad spanning local Y (trafficlight1) faces sideways, so
            // its peds cross THIS light's road; a quad spanning local X (4/5)
            // faces along the road, so its peds cross the other axis.
            bool facesSideways = std::fabs(pc[4] - pc[1]) > std::fabs(pc[3] - pc[0]);
            int pedState = pedStateFor(facesSideways ? type : 3 - type);
            bool show = false; uint8_t pr = 0, pg = 0, pb = 0;
            if (pedState == 0)      { show = true;       pr = 0;   pg = 255; pb = 0; }
            else if (pedState == 1) { show = pedFlashOn; pr = 255; pg = 0;   pb = 0; }
            else                    { show = true;       pr = 255; pg = 0;   pb = 0; }
            if (show) {
                float wx, wy, wz;
                QuatXform(in.qx, in.qy, in.qz, in.qw,
                          (pc[0] + pc[3]) * 0.5f, (pc[1] + pc[4]) * 0.5f, (pc[2] + pc[5]) * 0.5f,
                          in.x, in.y, in.z, wx, wy, wz);
                addGlow(wx, wy, wz, pr, pg, pb);
            }
        }
    }
    if (!glow.empty()) r.DrawCoronas(glow.data(), (int)glow.size());
}

void GameWorld::DrawCollision(D3DRenderer& r, float px, float py, float pz,
                              float radius) const {
    (void)pz;
    const uint32_t kPink = 0xFFFF3CD2u;
    std::vector<DebugLineVertex> v;
    auto add = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
        v.push_back({x0, y0, z0, kPink});
        v.push_back({x1, y1, z1, kPink});
    };

    const float r2 = radius * radius;
    static const int E[12][2] = {
        {0,4},{1,5},{2,6},{3,7},  // x edges
        {0,2},{1,3},{4,6},{5,7},  // y edges
        {0,1},{2,3},{4,5},{6,7},  // z edges
    };
    // COL spheres: three great circles each.
    for (const WorldColSphere& s : m_worldSpheres) {
        float dx = s.x - px, dy = s.y - py;
        if (dx * dx + dy * dy > r2) continue;
        const int N = 12;
        for (int k = 0; k < N; k++) {
            float a0 = 6.2831853f * k / N, a1 = 6.2831853f * (k + 1) / N;
            float c0 = std::cos(a0) * s.r, s0 = std::sin(a0) * s.r;
            float c1 = std::cos(a1) * s.r, s1 = std::sin(a1) * s.r;
            add(s.x + c0, s.y + s0, s.z, s.x + c1, s.y + s1, s.z);
            add(s.x + c0, s.y, s.z + s0, s.x + c1, s.y, s.z + s1);
            add(s.x, s.y + c0, s.z + s0, s.x, s.y + c1, s.z + s1);
        }
    }
    // PINK = oriented COL boxes (hug the model, what retail collides against).
    for (const WorldColObb& b : m_worldObbs) {
        float cx = 0, cy = 0;
        for (int c = 0; c < 8; c++) { cx += b.x[c]; cy += b.y[c]; }
        cx /= 8.0f; cy /= 8.0f;
        float dx = cx - px, dy = cy - py;
        if (dx * dx + dy * dy > r2) continue;
        for (auto& e : E)
            add(b.x[e[0]], b.y[e[0]], b.z[e[0]],
                b.x[e[1]], b.y[e[1]], b.z[e[1]]);
    }
    // (Collision now tests these same oriented boxes via CircleObbHit2D, so the
    // pink wireframe is exactly what blocks the player — no separate AABB.)

    // Terrain: the same cell grid the ground query uses, clipped to radius.
    int rc = (int)(radius / kCell) + 1;
    if (rc > 16) rc = 16;
    int gx = (int)std::floor(px / kCell), gy = (int)std::floor(py / kCell);
    for (int i = -rc; i <= rc; i++) {
        for (int j = -rc; j <= rc; j++) {
            auto it = m_cells.find(CellKey(gx + i, gy + j));
            if (it == m_cells.end()) continue;
            for (const ColTriangle& t : it->second.tris) {
                float mx = (t.ax + t.bx + t.cx) / 3.0f - px;
                float my = (t.ay + t.by + t.cy) / 3.0f - py;
                if (mx * mx + my * my > r2) continue;
                add(t.ax, t.ay, t.az, t.bx, t.by, t.bz);
                add(t.bx, t.by, t.bz, t.cx, t.cy, t.cz);
                add(t.cx, t.cy, t.cz, t.ax, t.ay, t.az);
            }
        }
    }

    if (!v.empty()) r.DrawDebugLines(v.data(), (uint32_t)v.size());
}

// ------------------------------------------------------------------- query

void GameWorld::DumpNearby(float px, float py, float pz, float radius) const {
    FILE* f = fopen("inst_dump.txt", "w");
    if (!f) return;
    fprintf(f, "# player at %.2f %.2f %.2f, radius %.1f\n", px, py, pz, radius);
    fprintf(f, "# model pos(x,y,z) quat(qx,qy,qz,qw)\n");
    float r2 = radius * radius;
    for (auto& in : m_insts) {
        if (!in.needed) continue;
        float dx = in.x - px, dy = in.y - py, dz = in.z - pz;
        if (dx*dx + dy*dy + dz*dz > r2) continue;
        fprintf(f, "%s %.2f %.2f %.2f  %.4f %.4f %.4f %.4f  lod=%s children=%u draw=%d\n",
                in.hdModel.c_str(), in.x, in.y, in.z,
                in.qx, in.qy, in.qz, in.qw,
                in.lodParent >= 0 ? m_insts[in.lodParent].hdModel.c_str() : "-",
                (unsigned)in.lodChildren, (int)in.draw);
    }
    fclose(f);
}

void GameWorld::DumpCollisionProbe(float px, float py, float feetZ, float radius,
                                   const char* path) const {
    const float stepOver = 0.55f, playerHeight = 1.8132f, R = 0.5f; // match main.cpp
    FILE* f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "# collision probe @ (%.2f, %.2f) feetZ=%.2f  span +-%.0fm\n",
            px, py, feetZ, radius);
    fprintf(f, "# active OBB boxes=%zu, cells with tris nearby listed below\n",
            m_worldObbs.size());

    // --- per-instance OBB footprints within radius ---
    fprintf(f, "\n## nearby instances (model | qz,qw | isGround | #obb | obbFootprint minX,minY..maxX,maxY | z0..z1)\n");
    float r2 = radius * radius;
    for (auto& in : m_insts) {
        if (!in.needed) continue;
        float dx = in.x - px, dy = in.y - py;
        if (dx*dx + dy*dy > r2) continue;
        // footprint extents from this instance's world boxes (if any)
        float fminx=1e30f,fminy=1e30f,fmaxx=-1e30f,fmaxy=-1e30f,z0=1e30f,z1=-1e30f;
        for (auto& b : in.boxes) {
            fminx=std::min(fminx,b.minX); fminy=std::min(fminy,b.minY);
            fmaxx=std::max(fmaxx,b.maxX); fmaxy=std::max(fmaxy,b.maxY);
            z0=std::min(z0,b.minZ); z1=std::max(z1,b.maxZ);
        }
        // DFF world Z-range (render) for comparison with collision Z.
        float dz0 = 1e30f, dz1 = -1e30f;
        if (m_img->HasFile(in.hdModel + ".dff")) {
            std::vector<uint8_t> dff = m_img->Extract(in.hdModel + ".dff");
            DffModel dm = DffLoader::LoadFromMemory(dff.data(), dff.size());
            float lx0=1e30f,ly0=1e30f,lz0=1e30f,lx1=-1e30f,ly1=-1e30f,lz1=-1e30f;
            bool any=false;
            for (auto& mh : dm.meshes) for (auto& vv : mh.vertices) {
                lx0=std::min(lx0,vv.x); ly0=std::min(ly0,vv.y); lz0=std::min(lz0,vv.z);
                lx1=std::max(lx1,vv.x); ly1=std::max(ly1,vv.y); lz1=std::max(lz1,vv.z); any=true;
            }
            if (any) {
                float xs[2]={lx0,lx1}, ys[2]={ly0,ly1}, zs[2]={lz0,lz1};
                for(int a=0;a<2;a++)for(int b=0;b<2;b++)for(int c=0;c<2;c++){
                    float wx,wy,wz; QuatXform(in.qx,in.qy,in.qz,in.qw,xs[a],ys[b],zs[c],
                                              in.x,in.y,in.z,wx,wy,wz);
                    dz0=std::min(dz0,wz); dz1=std::max(dz1,wz);
                }
            }
        }
        fprintf(f, "%-22s o.z=%.1f %.2f,%.2f g=%d #obb=%zu ",
                in.hdModel.c_str(), in.z, in.qz, in.qw, (int)in.isGround, in.boxes.size());
        if (!in.boxes.empty())
            fprintf(f, "COLz=%.1f..%.1f ", z0, z1);
        else
            fprintf(f, "COLz=(tri/none) ");
        fprintf(f, "DFFz=%.1f..%.1f\n", dz0, dz1);
    }

    // --- solidity map: '#'=OBB solid, 'T'=wall-tri solid, 'o'=overlap, '.'=open ---
    const float cell = 0.75f;
    int half = (int)(radius / cell);
    auto obbSolid = [&](float x, float y) -> bool {
        for (size_t i = 0; i < std::min(m_worldObbs.size(), m_worldBoxes.size()); i++) {
            const WorldColBox& b = m_worldBoxes[i];
            if (b.maxZ <= feetZ + stepOver) continue;
            if (b.minZ >= feetZ + playerHeight) continue;
            if (CircleObbHit2D(x, y, R, m_worldObbs[i])) return true;
        }
        return false;
    };
    auto triSolid = [&](float x, float y) -> bool {
        int gx=(int)std::floor(x/kCell), gy=(int)std::floor(y/kCell);
        for (int i=-1;i<=1;i++) for (int j=-1;j<=1;j++) {
            auto it = m_cells.find(CellKey(gx+i,gy+j));
            if (it==m_cells.end()) continue;
            for (const ColTriangle& t : it->second.tris) {
                float tzmin=std::min({t.az,t.bz,t.cz}), tzmax=std::max({t.az,t.bz,t.cz});
                if (tzmax<=feetZ+stepOver) continue;
                if (tzmin>=feetZ+playerHeight) continue;
                float ux=t.bx-t.ax,uy=t.by-t.ay,uz=t.bz-t.az;
                float vx=t.cx-t.ax,vy=t.cy-t.ay,vz=t.cz-t.az;
                float nz=ux*vy-uy*vx;
                float nlen=std::sqrt((uy*vz-uz*vy)*(uy*vz-uz*vy)+(uz*vx-ux*vz)*(uz*vx-ux*vz)+nz*nz);
                if (nlen<1e-6f) continue;
                if (fabsf(nz)/nlen>0.5f) continue;
                if (CircleTriHit2D(x,y,R,t)) return true;
            }
        }
        return false;
    };
    fprintf(f, "\n## solidity map (%.2fm cells, +Y up, +X right; '#'=box 'T'=wall 'o'=both '.'=open 'P'=player)\n", cell);
    for (int gy = half; gy >= -half; gy--) {
        fprintf(f, "%6.1f ", py + gy*cell);
        for (int gx = -half; gx <= half; gx++) {
            float x = px + gx*cell, y = py + gy*cell;
            if (gx==0 && gy==0) { fputc('P', f); continue; }
            bool b = obbSolid(x,y), t = triSolid(x,y);
            fputc(b && t ? 'o' : b ? '#' : t ? 'T' : '.', f);
        }
        fputc('\n', f);
    }

    // --- which instance owns each blocking box in the map area ---
    {
        std::map<uint32_t, std::vector<size_t>> owners;
        for (size_t i = 0; i < std::min(m_worldObbs.size(), m_worldBoxOwner.size()); i++) {
            const WorldColBox& b = m_worldBoxes[i];
            if (b.maxZ <= feetZ + stepOver || b.minZ >= feetZ + playerHeight) continue;
            if (b.maxX < px - radius || b.minX > px + radius ||
                b.maxY < py - radius || b.minY > py + radius) continue;
            owners[m_worldBoxOwner[i]].push_back(i);
        }
        fprintf(f, "\n## blocking boxes in the map area, by owner (model @ origin: box world AABB z)\n");
        for (auto& [idx, boxes] : owners) {
            const Inst& in = m_insts[idx];
            fprintf(f, "%-22s @ (%.1f,%.1f,%.1f) g=%d:\n", in.hdModel.c_str(), in.x, in.y, in.z, (int)in.isGround);
            for (size_t bi : boxes) {
                const WorldColBox& b = m_worldBoxes[bi];
                fprintf(f, "    (%.1f,%.1f)..(%.1f,%.1f) z %.1f..%.1f\n", b.minX, b.minY, b.maxX, b.maxY, b.minZ, b.maxZ);
            }
        }
    }

    // --- triangle stats in the central cell neighbourhood ---
    int gx=(int)std::floor(px/kCell), gy=(int)std::floor(py/kCell);
    size_t totTri=0, vertTri=0;
    for (int i=-2;i<=2;i++) for (int j=-2;j<=2;j++) {
        auto it=m_cells.find(CellKey(gx+i,gy+j));
        if (it==m_cells.end()) continue;
        for (const ColTriangle& t: it->second.tris) {
            totTri++;
            float ux=t.bx-t.ax,uy=t.by-t.ay,uz=t.bz-t.az;
            float vx=t.cx-t.ax,vy=t.cy-t.ay,vz=t.cz-t.az;
            float nz=ux*vy-uy*vx;
            float nlen=std::sqrt((uy*vz-uz*vy)*(uy*vz-uz*vy)+(uz*vx-ux*vz)*(uz*vx-ux*vz)+nz*nz);
            if (nlen>1e-6f && fabsf(nz)/nlen<=0.5f) vertTri++;
        }
    }
    fprintf(f, "\n## tris in central 5x5 cells: %zu total, %zu near-vertical (wall)\n", totTri, vertTri);

    // --- ground-height diagnostics at the player XY ---
    fprintf(f, "\n## ground query at player XY:\n");
    for (float ref : {feetZ + 30.0f, feetZ + 2.0f, feetZ - 10.0f, -1000.0f}) {
        float h = GetGroundHeight(px, py, ref);
        fprintf(f, "  GetGroundHeight(refZ=%.1f) = %.2f%s\n", ref, h,
                h < kNoGround * 0.5f ? "  (NO GROUND)" : "");
    }
    // All near-HORIZONTAL triangle surface Z's under the player cell (ground
    // candidates), sorted, so a phantom floating floor is visible.
    {
        std::vector<float> zs;
        int gcx=(int)std::floor(px/kCell), gcy=(int)std::floor(py/kCell);
        for (int i=-1;i<=1;i++) for (int j=-1;j<=1;j++) {
            auto it=m_cells.find(CellKey(gcx+i,gcy+j));
            if (it==m_cells.end()) continue;
            for (const ColTriangle& t: it->second.tris) {
                float ux=t.bx-t.ax,uy=t.by-t.ay,uz=t.bz-t.az;
                float vx=t.cx-t.ax,vy=t.cy-t.ay,vz=t.cz-t.az;
                float nz=ux*vy-uy*vx;
                float nlen=std::sqrt((uy*vz-uz*vy)*(uy*vz-uz*vy)+(uz*vx-ux*vz)*(uz*vx-ux*vz)+nz*nz);
                if (nlen>1e-6f && fabsf(nz)/nlen>0.5f) { // near-horizontal = ground
                    // does the triangle's XY cover the player point?
                    if (CircleTriHit2D(px,py,0.3f,t))
                        zs.push_back((t.az+t.bz+t.cz)/3.0f);
                }
            }
        }
        std::sort(zs.begin(), zs.end());
        fprintf(f, "## horizontal ground-tri Z's under player (%zu): ", zs.size());
        for (float z : zs) fprintf(f, "%.1f ", z);
        fprintf(f, "\n");
    }
    fclose(f);
    std::fprintf(stderr, "[PROBE] wrote %s (%zu active OBB boxes)\n", path, m_worldObbs.size());
}

void GameWorld::BuildAllGroundTris() {
    for (Inst& in : m_insts)
        if (!in.groundTrisIn) BuildGroundTris(in);
}

float GameWorld::GetGroundHeight(float x, float y, float refZ) const {
    const float kMaxStepUp = 1.5f;
    float best = kNoGround;
    int gx = (int)std::floor(x / kCell);
    int gy = (int)std::floor(y / kCell);
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            auto it = m_cells.find(CellKey(gx + i, gy + j));
            if (it == m_cells.end()) continue;
            float h = ColLoader::GetGroundHeight(it->second.tris, x, y, refZ, kMaxStepUp);
            if (h > best) best = h;
        }
    }
    // COL box TOPS are floors too. Retail boxes are fully solid; many houses
    // model the porch / steps / raised slab as a box with no matching faces
    // (glenphouse03_lax: porch box z 25.69..26.69, top = the drawn porch
    // floor). Using boxes only as walls let the player drop through the porch
    // onto the terrain under the house. The top face is corners 1,3,5,7
    // (k = 1) of the oriented box -- tested as two triangles so a tilted
    // instance gets the true sloped top.
    std::vector<ColTriangle> top(2);
    for (const WorldColObb& o : m_worldObbs) {
        top[0] = {o.x[1], o.y[1], o.z[1], o.x[5], o.y[5], o.z[5], o.x[7], o.y[7], o.z[7]};
        top[1] = {o.x[1], o.y[1], o.z[1], o.x[7], o.y[7], o.z[7], o.x[3], o.y[3], o.z[3]};
        float h = ColLoader::GetGroundHeight(top, x, y, refZ, kMaxStepUp);
        if (h > best) best = h;
    }
    return best;
}

bool GameWorld::HitsSolidCircle(float x, float y, float feetZ, float ox, float oy,
                                float radius, float stepOver, float playerHeight) const {
    // m_worldObbs and m_worldBoxes are parallel (same order). Use the AABB's Z
    // span for the step-over / above-head gates (Z is unaffected by yaw), and
    // the ORIENTED footprint for the XY hit so rotated buildings don't block
    // the empty corner (the old AABB test = "invisible walls").
    size_t n = std::min(m_worldObbs.size(), m_worldBoxes.size());
    for (size_t i = 0; i < n; i++) {
        const WorldColBox& b = m_worldBoxes[i];
        const WorldColObb& o = m_worldObbs[i];
        if (b.maxZ <= feetZ + stepOver) continue;      // step over it
        if (b.minZ >= feetZ + playerHeight) continue;  // above the player's head
        if (!CircleObbHit2D(x, y, radius, o)) continue;
        if (MoveBlocked(ObbClearance2D(ox, oy, o), ObbClearance2D(x, y, o), radius)) return true;
    }
    // COL spheres: solid over their vertical extent, same gates as boxes.
    for (const WorldColSphere& s : m_worldSpheres) {
        if (s.z + s.r <= feetZ + stepOver) continue;
        if (s.z - s.r >= feetZ + playerHeight) continue;
        float nc = std::sqrt((x - s.x) * (x - s.x) + (y - s.y) * (y - s.y)) - s.r;
        if (nc >= radius) continue;
        float oc = std::sqrt((ox - s.x) * (ox - s.x) + (oy - s.y) * (oy - s.y)) - s.r;
        if (MoveBlocked(oc, nc, radius)) return true;
    }
    // Wall collision against the COL mesh (for models whose COL3 has no box
    // section -- forum, bridge girders, etc.). Test near-vertical triangles in
    // the player's cell neighbourhood whose Z span overlaps the body: block
    // when the destination hits one but the origin didn't.
    int gx = (int)std::floor(x / kCell), gy = (int)std::floor(y / kCell);
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++) {
            auto it = m_cells.find(CellKey(gx + i, gy + j));
            if (it == m_cells.end()) continue;
            for (const ColTriangle& t : it->second.tris) {
                float tzmin = std::min({t.az, t.bz, t.cz});
                float tzmax = std::max({t.az, t.bz, t.cz});
                if (tzmax <= feetZ + stepOver) continue;     // floor/curb: walk over
                if (tzmin >= feetZ + playerHeight) continue; // above head
                // Steepness: skip near-horizontal faces (floors/ramps you walk on).
                float ux = t.bx-t.ax, uy = t.by-t.ay, uz = t.bz-t.az;
                float vx = t.cx-t.ax, vy = t.cy-t.ay, vz = t.cz-t.az;
                float nz = ux*vy - uy*vx;                    // z of cross(u,v)
                float nlen = std::sqrt((uy*vz-uz*vy)*(uy*vz-uz*vy) +
                                       (uz*vx-ux*vz)*(uz*vx-ux*vz) + nz*nz);
                if (nlen < 0.02f) continue;                  // slivers (area < 1 cm2... paper-thin leftovers) never block
                if (fabsf(nz) / nlen > 0.5f) continue;       // >~60deg from vertical = walkable
                if (!CircleTriHit2D(x, y, radius, t)) continue;
                if (MoveBlocked(TriClearance2D(ox, oy, t), TriClearance2D(x, y, t), radius)) return true;
            }
        }
    return false;
}

namespace {
// Moller-Trumbore, both faces: the hit fraction along d, or -1.
float RayTri(const float o[3], const float d[3], const float a[3], const float b[3], const float c[3]) {
    const float e1[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float e2[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const float p[3] = {d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0]};
    const float det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
    if (std::fabs(det) < 1e-9f) return -1.0f;
    const float inv = 1.0f / det;
    const float t0[3] = {o[0] - a[0], o[1] - a[1], o[2] - a[2]};
    const float u = (t0[0] * p[0] + t0[1] * p[1] + t0[2] * p[2]) * inv;
    if (u < 0.0f || u > 1.0f) return -1.0f;
    const float q[3] = {t0[1] * e1[2] - t0[2] * e1[1], t0[2] * e1[0] - t0[0] * e1[2], t0[0] * e1[1] - t0[1] * e1[0]};
    const float v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) * inv;
    if (v < 0.0f || u + v > 1.0f) return -1.0f;
    return (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) * inv;
}
}  // namespace

float GameWorld::RayCast(float ax, float ay, float az, float bx, float by, float bz, int* kind, float* hitNz) const {
    const float o[3] = {ax, ay, az}, d[3] = {bx - ax, by - ay, bz - az};
    float best = 1.0f;
    int curKind = 0;
    auto hit = [&](const float a[3], const float b[3], const float c[3]) {
        const float t = RayTri(o, d, a, b, c);
        if (t >= 0.0f && t < best) {
            best = t;
            if (kind) *kind = curKind;
            if (hitNz) {
                const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
                const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
                const float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                *hitNz = l > 1e-9f ? std::fabs(n[2]) / l : 1.0f;
            }
        }
    };
    const float lo[3] = {std::min(ax, bx), std::min(ay, by), std::min(az, bz)};
    const float hi[3] = {std::max(ax, bx), std::max(ay, by), std::max(az, bz)};
    // COL meshes, cell by cell over the segment's bounds
    const int x0 = (int)std::floor(lo[0] / kCell), x1 = (int)std::floor(hi[0] / kCell);
    const int y0 = (int)std::floor(lo[1] / kCell), y1 = (int)std::floor(hi[1] / kCell);
    for (int i = x0; i <= x1; i++)
        for (int j = y0; j <= y1; j++) {
            auto it = m_cells.find(CellKey(i, j));
            if (it == m_cells.end()) continue;
            for (const ColTriangle& t : it->second.tris) {
                const float a[3] = {t.ax, t.ay, t.az}, b[3] = {t.bx, t.by, t.bz}, c[3] = {t.cx, t.cy, t.cz};
                hit(a, b, c);
            }
        }
    // Oriented boxes: the 6 faces (corner bit 0 = top, bits 1/2 the other axes).
    curKind = 1;
    const size_t n = std::min(m_worldObbs.size(), m_worldBoxes.size());
    for (size_t k = 0; k < n; k++) {
        const WorldColBox& ab = m_worldBoxes[k];
        if (ab.maxX < lo[0] || ab.minX > hi[0] || ab.maxY < lo[1] || ab.minY > hi[1] ||
            ab.maxZ < lo[2] || ab.minZ > hi[2]) continue;
        const WorldColObb& ob = m_worldObbs[k];
        for (int axis : {1, 2, 4})
            for (int side = 0; side < 2; side++) {
                const int b1 = axis == 1 ? 2 : 1, b2 = axis == 4 ? 2 : 4;
                const int base = side ? axis : 0;
                const int q[4] = {base, base | b1, base | b1 | b2, base | b2};
                float v[4][3];
                for (int m = 0; m < 4; m++) { v[m][0] = ob.x[q[m]]; v[m][1] = ob.y[q[m]]; v[m][2] = ob.z[q[m]]; }
                hit(v[0], v[1], v[2]);
                hit(v[0], v[2], v[3]);
            }
    }
    // Spheres
    const float dd = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
    if (dd > 1e-12f)
        for (const WorldColSphere& s : m_worldSpheres) {
            const float m[3] = {ax - s.x, ay - s.y, az - s.z};
            const float bq = m[0] * d[0] + m[1] * d[1] + m[2] * d[2];
            const float cq = m[0] * m[0] + m[1] * m[1] + m[2] * m[2] - s.r * s.r;
            const float disc = bq * bq - dd * cq;
            if (disc < 0.0f) continue;
            const float t = (-bq - std::sqrt(disc)) / dd;
            if (t >= 0.0f && t < best) { best = t; if (kind) *kind = 2; if (hitNz) *hitNz = 0.0f; }
        }
    return best;
}

namespace {
// Closest point on triangle abc to p (Ericson, Real-Time Collision Detection).
void ClosestOnTri(const float p[3], const float a[3], const float b[3], const float c[3], float o[3]) {
    auto sub = [](const float* u, const float* v, float* r) { for (int i = 0; i < 3; i++) r[i] = u[i] - v[i]; };
    auto dot = [](const float* u, const float* v) { return u[0] * v[0] + u[1] * v[1] + u[2] * v[2]; };
    float ab[3], ac[3], ap[3];
    sub(b, a, ab); sub(c, a, ac); sub(p, a, ap);
    const float d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) { for (int i = 0; i < 3; i++) o[i] = a[i]; return; }
    float bp[3]; sub(p, b, bp);
    const float d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) { for (int i = 0; i < 3; i++) o[i] = b[i]; return; }
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) { const float v = d1 / (d1 - d3); for (int i = 0; i < 3; i++) o[i] = a[i] + v * ab[i]; return; }
    float cp[3]; sub(p, c, cp);
    const float d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) { for (int i = 0; i < 3; i++) o[i] = c[i]; return; }
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) { const float w = d2 / (d2 - d6); for (int i = 0; i < 3; i++) o[i] = a[i] + w * ac[i]; return; }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        for (int i = 0; i < 3; i++) o[i] = b[i] + w * (c[i] - b[i]);
        return;
    }
    const float den = 1.0f / (va + vb + vc), v = vb * den, w = vc * den;
    for (int i = 0; i < 3; i++) o[i] = a[i] + ab[i] * v + ac[i] * w;
}
}  // namespace

bool GameWorld::SphereProbe(float cx, float cy, float cz, float r, ProbeHit& out) const {
    const float c[3] = {cx, cy, cz};
    bool hit = false;
    auto take = [&](const float q[3], const float n[3]) {
        out.p[0] = q[0]; out.p[1] = q[1]; out.p[2] = q[2];
        out.n[0] = n[0]; out.n[1] = n[1]; out.n[2] = n[2];
        hit = true;
    };
    auto outward = [&](const float q[3], float n[3]) {   // from the contact point to the sphere centre
        float l = 0;
        for (int i = 0; i < 3; i++) { n[i] = c[i] - q[i]; l += n[i] * n[i]; }
        l = std::sqrt(l);
        if (l < 1e-5f) { n[0] = n[1] = 0; n[2] = 1; } else for (int i = 0; i < 3; i++) n[i] /= l;
    };
    float best = r * r;
    // Oriented boxes: clamp the centre onto each of the box's three axes.
    const size_t nb = std::min(m_worldObbs.size(), m_worldBoxes.size());
    for (size_t k = 0; k < nb; k++) {
        const WorldColBox& ab = m_worldBoxes[k];
        if (cx + r < ab.minX || cx - r > ab.maxX || cy + r < ab.minY || cy - r > ab.maxY ||
            cz + r < ab.minZ || cz - r > ab.maxZ) continue;
        const WorldColObb& o = m_worldObbs[k];
        const float org[3] = {o.x[0], o.y[0], o.z[0]};
        const int ax[3] = {1, 2, 4};
        float q[3] = {org[0], org[1], org[2]};
        for (int a = 0; a < 3; a++) {
            float e[3] = {o.x[ax[a]] - org[0], o.y[ax[a]] - org[1], o.z[ax[a]] - org[2]};
            const float len2 = e[0] * e[0] + e[1] * e[1] + e[2] * e[2];
            if (len2 < 1e-9f) continue;
            float t = ((cx - org[0]) * e[0] + (cy - org[1]) * e[1] + (cz - org[2]) * e[2]) / len2;
            t = std::clamp(t, 0.0f, 1.0f);
            for (int i = 0; i < 3; i++) q[i] += e[i] * t;
        }
        const float d2 = (q[0] - cx) * (q[0] - cx) + (q[1] - cy) * (q[1] - cy) + (q[2] - cz) * (q[2] - cz);
        if (d2 < best) { float n[3]; outward(q, n); best = d2; take(q, n); }
    }
    for (const WorldColSphere& s : m_worldSpheres) {
        const float dx = cx - s.x, dy = cy - s.y, dz = cz - s.z;
        const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d > s.r + r || d < 1e-5f) continue;
        const float n[3] = {dx / d, dy / d, dz / d};
        const float q[3] = {s.x + n[0] * s.r, s.y + n[1] * s.r, s.z + n[2] * s.r};
        const float d2 = (d - s.r) * (d - s.r);
        if (d2 < best) { best = d2; take(q, n); }
    }
    const int gx = (int)std::floor(cx / kCell), gy = (int)std::floor(cy / kCell);
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++) {
            auto it = m_cells.find(CellKey(gx + i, gy + j));
            if (it == m_cells.end()) continue;
            for (const ColTriangle& t : it->second.tris) {
                if (std::min({t.az, t.bz, t.cz}) > cz + r || std::max({t.az, t.bz, t.cz}) < cz - r) continue;
                const float a[3] = {t.ax, t.ay, t.az}, b[3] = {t.bx, t.by, t.bz}, cc[3] = {t.cx, t.cy, t.cz};
                float q[3];
                ClosestOnTri(c, a, b, cc, q);
                const float d2 = (q[0] - cx) * (q[0] - cx) + (q[1] - cy) * (q[1] - cy) + (q[2] - cz) * (q[2] - cz);
                if (d2 >= best) continue;
                // the triangle's own normal, facing the sphere
                const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {cc[0] - a[0], cc[1] - a[1], cc[2] - a[2]};
                float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
                const float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (l < 1e-9f) continue;
                for (int m = 0; m < 3; m++) n[m] /= l;
                if (n[0] * (cx - q[0]) + n[1] * (cy - q[1]) + n[2] * (cz - q[2]) < 0) for (int m = 0; m < 3; m++) n[m] = -n[m];
                best = d2; take(q, n);
            }
        }
    return hit;
}

// ------------------------------------------------------------------ helpers

std::string GameWorld::ToLower(const std::string& s) {
    std::string out = s;
    for (auto& c : out)
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    return out;
}

bool GameWorld::IsGroundModel(const std::string& name, int flags) {
    if (flags & 1) return true;
    if (name.find("landhub") != std::string::npos) return true;
    if (name == "rydbkyar1_lae2") return true;
    if (name.find("_ground") != std::string::npos) return true;
    if (name.find("road") != std::string::npos &&
        name.find("roadsign") == std::string::npos)
        return true;
    return false;
}