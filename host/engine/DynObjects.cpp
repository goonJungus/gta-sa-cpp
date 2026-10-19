#include <array>
// Dynamic props: the objects that object.dat gives a mass (boxes, trash bags, bins, barrels, ...).
// Retail makes each placement a CObject: static until something hits it, then a rigid body that falls,
// bounces and slides, and goes back to sleep. Collision damage effects 20/21 smash it, 200/202 break it,
// and a "causes explosion" row blows it up.
//
// Each prop is one oriented box (the union of its COL boxes / spheres / triangles) with a mass, a scalar
// turn mass and the object.dat elasticity. The world sees that box (Inst::boxes) at its current pose, so
// the player and the camera collide with a prop wherever it has been shoved to.

#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

constexpr float kNoGroundV = ColLoader::kNoGround;
constexpr float kSleepTime = 0.6f;

inline float Dot(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline void Cross(const float* a, const float* b, float* o) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}
inline float Len(const float* a) { return std::sqrt(Dot(a, a)); }

// Row-vector rotation of an IPL-convention quaternion (QuatToMatrix): row i is the world image of local axis i.
void QuatRows(float qx, float qy, float qz, float qw, float R[3][3]) {
    const float xx = qx * qx, yy = qy * qy, zz = qz * qz, xy = qx * qy, xz = qx * qz, yz = qy * qz;
    const float wx = qw * qx, wy = qw * qy, wz = qw * qz;
    R[0][0] = 1.f - 2.f * (yy + zz); R[0][1] = 2.f * (xy + wz);       R[0][2] = 2.f * (xz - wy);
    R[1][0] = 2.f * (xy - wz);       R[1][1] = 1.f - 2.f * (xx + zz); R[1][2] = 2.f * (yz + wx);
    R[2][0] = 2.f * (xz + wy);       R[2][1] = 2.f * (yz - wx);       R[2][2] = 1.f - 2.f * (xx + yy);
}

// The quaternion whose QuatToMatrix is these rows (the standard column rotation is the transpose).
void RowsQuat(const float m[3][3], float& qx, float& qy, float& qz, float& qw) {
    float c[3][3];
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) c[i][j] = m[j][i];
    const float tr = c[0][0] + c[1][1] + c[2][2];
    if (tr > 0.0f) {
        const float s = std::sqrt(tr + 1.0f) * 2.0f;
        qw = 0.25f * s; qx = (c[2][1] - c[1][2]) / s; qy = (c[0][2] - c[2][0]) / s; qz = (c[1][0] - c[0][1]) / s;
    } else if (c[0][0] > c[1][1] && c[0][0] > c[2][2]) {
        const float s = std::sqrt(1.0f + c[0][0] - c[1][1] - c[2][2]) * 2.0f;
        qw = (c[2][1] - c[1][2]) / s; qx = 0.25f * s; qy = (c[0][1] + c[1][0]) / s; qz = (c[0][2] + c[2][0]) / s;
    } else if (c[1][1] > c[2][2]) {
        const float s = std::sqrt(1.0f + c[1][1] - c[0][0] - c[2][2]) * 2.0f;
        qw = (c[0][2] - c[2][0]) / s; qx = (c[0][1] + c[1][0]) / s; qy = 0.25f * s; qz = (c[1][2] + c[2][1]) / s;
    } else {
        const float s = std::sqrt(1.0f + c[2][2] - c[0][0] - c[1][1]) * 2.0f;
        qw = (c[1][0] - c[0][1]) / s; qx = (c[0][2] + c[2][0]) / s; qy = (c[1][2] + c[2][1]) / s; qz = 0.25f * s;
    }
    const float l = std::sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
    if (l > 1e-9f) { qx /= l; qy /= l; qz /= l; qw /= l; }
}

void Orthonormalize(float R[3][3]) {
    float* a = R[0]; float* b = R[1]; float* c = R[2];
    float l = Len(a); for (int i = 0; i < 3; i++) a[i] /= l;
    const float d = Dot(a, b); for (int i = 0; i < 3; i++) b[i] -= a[i] * d;
    l = Len(b); for (int i = 0; i < 3; i++) b[i] /= l;
    Cross(a, b, c);
}

// local vector -> world (row convention)
inline void ToWorld(const float R[3][3], const float* l, float* o) {
    for (int j = 0; j < 3; j++) o[j] = l[0] * R[0][j] + l[1] * R[1][j] + l[2] * R[2][j];
}
// world vector -> local
inline void ToLocal(const float R[3][3], const float* w, float* o) {
    for (int i = 0; i < 3; i++) o[i] = Dot(w, R[i]);
}

std::vector<std::string> Tokens(const std::string& line) {
    std::vector<std::string> t;
    std::string cur;
    for (char ch : line) {
        if (ch == ',' || ch == ' ' || ch == '\t' || ch == '\r') {
            if (!cur.empty()) { t.push_back(cur); cur.clear(); }
        } else {
            cur += ch;
        }
    }
    if (!cur.empty()) t.push_back(cur);
    return t;
}

}  // namespace

// ------------------------------------------------------------------ loading

void GameWorld::LoadObjectDat() {
    std::ifstream f(m_gameDir + "\\data\\object.dat");
    if (!f) { std::fprintf(stderr, "[Dyn] object.dat missing\n"); return; }
    std::string line;
    while (std::getline(f, line)) {
        const size_t sc = line.find(';');
        if (sc != std::string::npos) line.resize(sc);
        std::vector<std::string> t = Tokens(line);
        if (t.size() < 12) continue;
        char* end = nullptr;
        ObjDef d;
        d.mass = std::strtof(t[1].c_str(), &end);
        if (end == t[1].c_str()) continue;
        d.turn = std::max(0.01f, (float)std::atof(t[2].c_str()));
        d.airRes = (float)std::atof(t[3].c_str());
        d.elast = (float)std::atof(t[4].c_str());
        d.uproot = (float)std::atof(t[6].c_str());
        d.cdMult = (float)std::atof(t[7].c_str());
        d.cdEff = std::atoi(t[8].c_str());
        d.spcdr = std::atoi(t[9].c_str());
        d.expl = std::atoi(t[11].c_str());
        if (d.cdEff >= 200 && t.size() >= 23) d.gunBreak = std::atoi(t[22].c_str());
        m_objDef.emplace(ToLower(t[0]), d);
    }
}

void GameWorld::MarkDynInsts() {
    m_dyn.clear();
    for (size_t i = 0; i < m_insts.size(); i++) {
        Inst& in = m_insts[i];
        if (in.lodChildren) continue;
        auto it = m_objDef.find(in.hdModel);
        if (it == m_objDef.end()) continue;
        const ObjDef& d = it->second;
        if (d.mass >= 99999.0f || d.mass > 5000.0f || d.uproot > 0.0f) continue;
        if (!(d.spcdr == 0 || d.spcdr == 2 || d.spcdr == 3 || d.spcdr == 5 || d.spcdr == 9)) continue;
        DynBody b;
        b.inst = (int)i;
        b.def = d;
        in.dyn = (int32_t)m_dyn.size();
        in.groundTrisIn = true;   // a prop is a box, not terrain
        m_dyn.push_back(b);
    }
    if (getenv("GTA_DYNLIST"))
        for (const DynBody& b : m_dyn) {
            const Inst& in = m_insts[b.inst];
            if (in.x > 1800 && in.x < 2900 && in.y > -2100 && in.y < -1200)   // Los Santos
                std::fprintf(stderr, "[DynList] %s %.1f %.1f %.1f mass %.0f\n", in.hdModel.c_str(), in.x, in.y, in.z, b.def.mass);
        }
    std::fprintf(stderr, "[Dyn] %zu object.dat rows, %zu physics props placed\n", m_objDef.size(), m_dyn.size());
}

bool GameWorld::InitDynBody(DynBody& b) {
    Inst& in = m_insts[b.inst];
    float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
    auto grow = [&](float x, float y, float z) {
        mn[0] = std::min(mn[0], x); mn[1] = std::min(mn[1], y); mn[2] = std::min(mn[2], z);
        mx[0] = std::max(mx[0], x); mx[1] = std::max(mx[1], y); mx[2] = std::max(mx[2], z);
    };
    const ColEntryLoc* loc = FindColEntry(in.hdModel);
    struct Raw { bool sphere; float c[3], h[3], r; };
    std::vector<Raw> raw;
    if (loc && loc->arc != size_t(-1)) {
        std::vector<WorldColBox> boxes;
        if (ParseBoxes(loc->arc, loc->off, boxes))
            for (const WorldColBox& x : boxes) {
                grow(x.minX, x.minY, x.minZ); grow(x.maxX, x.maxY, x.maxZ);
                raw.push_back({false, {0.5f * (x.minX + x.maxX), 0.5f * (x.minY + x.maxY), 0.5f * (x.minZ + x.maxZ)},
                               {0.5f * (x.maxX - x.minX), 0.5f * (x.maxY - x.minY), 0.5f * (x.maxZ - x.minZ)}, 0.0f});
            }
        const std::vector<uint8_t>& d = m_cols[loc->arc].data;
        const uint8_t* e = d.data() + loc->off;
        if (loc->off + 0x6C <= d.size() && std::memcmp(e, "COL", 3) == 0) {
            auto u16 = [](const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); };
            auto u32 = [](const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); };
            auto f32 = [&](const uint8_t* p) { uint32_t u = u32(p); float f; std::memcpy(&f, &u, 4); return f; };
            const uint16_t ns = u16(e + 0x48);
            const size_t sb = loc->off + 4 + u32(e + 0x54);
            for (uint16_t i = 0; i < ns && ns <= 256; i++) {
                const size_t s = sb + (size_t)i * 20;
                if (s + 20 > d.size()) break;
                const float cx = f32(d.data() + s), cy = f32(d.data() + s + 4), cz = f32(d.data() + s + 8), r = f32(d.data() + s + 12);
                if (!(r > 0.0f && r < 100.0f)) continue;
                grow(cx - r, cy - r, cz - r); grow(cx + r, cy + r, cz + r);
                raw.push_back({true, {cx, cy, cz}, {r, r, r}, r});
            }
        }
        if (mn[0] > mx[0]) {
            std::vector<ColTriangle> tris;
            if (ColLoader::LoadModelTriangles(d.data(), d.size(), in.hdModel.c_str(), 0, 0, 0, 1, 0, 0, 0, tris))
                for (const ColTriangle& t : tris) { grow(t.ax, t.ay, t.az); grow(t.bx, t.by, t.bz); grow(t.cx, t.cy, t.cz); }
        }
    }
    if (mn[0] > mx[0] || mx[0] - mn[0] > 12.0f || mx[1] - mn[1] > 12.0f || mx[2] - mn[2] > 12.0f) {
        // No usable shape: leave it as plain static collision.
        b.dead = true;
        in.dyn = -1;
        in.groundTrisIn = false;
        return false;
    }
    for (int i = 0; i < 3; i++) {
        b.c[i] = 0.5f * (mn[i] + mx[i]);
        b.hx[i] = std::max(0.04f, 0.5f * (mx[i] - mn[i]));
    }
    b.parts.clear();
    for (const Raw& r : raw) {
        DynBody::Part p;
        p.sphere = r.sphere; p.r = r.r;
        for (int i = 0; i < 3; i++) { p.c[i] = r.c[i] - b.c[i]; p.h[i] = r.h[i]; }
        b.parts.push_back(p);
    }
    if (b.parts.empty()) {   // a triangle-mesh model: its hull box stands in
        DynBody::Part p;
        for (int i = 0; i < 3; i++) p.h[i] = b.hx[i];
        b.parts.push_back(p);
    }
    if (getenv("GTA_DYNSHAPES")) {
        int nb = 0, ns = 0;
        for (const DynBody::Part& p : b.parts) (p.sphere ? ns : nb)++;
        std::fprintf(stderr, "[DynShape] %s boxes %d spheres %d%s\n", in.hdModel.c_str(), nb, ns, raw.empty() ? " (hull)" : "");
    }
    QuatRows(in.qx, in.qy, in.qz, in.qw, b.R);
    float off[3];
    ToWorld(b.R, b.c, off);
    b.p[0] = in.x + off[0]; b.p[1] = in.y + off[1]; b.p[2] = in.z + off[2];
    b.ready = true;
    b.awake = false;
    DynSync(b);
    return true;
}

// ------------------------------------------------------------------ pose

void GameWorld::DynSync(DynBody& b) {
    Inst& in = m_insts[b.inst];
    float off[3];
    ToWorld(b.R, b.c, off);
    in.x = b.p[0] - off[0]; in.y = b.p[1] - off[1]; in.z = b.p[2] - off[2];
    RowsQuat(b.R, in.qx, in.qy, in.qz, in.qw);
    in.boxes.clear(); in.obb.clear(); in.spheres.clear();
    for (const DynBody::Part& pt : b.parts) {
        float cw[3];
        ToWorld(b.R, pt.c, cw);
        const float C[3] = {b.p[0] + cw[0], b.p[1] + cw[1], b.p[2] + cw[2]};
        if (pt.sphere) { in.spheres.push_back(WorldColSphere{C[0], C[1], C[2], pt.r}); continue; }
        WorldColObb ob;
        float mn[3] = {1e30f, 1e30f, 1e30f}, mx[3] = {-1e30f, -1e30f, -1e30f};
        for (int i = 0; i < 2; i++)
            for (int j = 0; j < 2; j++)
                for (int k = 0; k < 2; k++) {
                    const float l[3] = {i ? pt.h[0] : -pt.h[0], j ? pt.h[1] : -pt.h[1], k ? pt.h[2] : -pt.h[2]};
                    float w[3];
                    ToWorld(b.R, l, w);
                    const int c = i * 4 + j * 2 + k;
                    ob.x[c] = C[0] + w[0]; ob.y[c] = C[1] + w[1]; ob.z[c] = C[2] + w[2];
                    mn[0] = std::min(mn[0], ob.x[c]); mx[0] = std::max(mx[0], ob.x[c]);
                    mn[1] = std::min(mn[1], ob.y[c]); mx[1] = std::max(mx[1], ob.y[c]);
                    mn[2] = std::min(mn[2], ob.z[c]); mx[2] = std::max(mx[2], ob.z[c]);
                }
        in.boxes.push_back(WorldColBox{mn[0], mn[1], mn[2], mx[0], mx[1], mx[2]});
        in.obb.push_back(ob);
    }
    in.boxesIn = true;
}

void GameWorld::DynWake(DynBody& b) {
    if (b.dead || !b.ready) return;
    b.awake = true;
    b.still = 0.0f;
}

void GameWorld::DynImpulse(DynBody& b, const float* at, const float* j) {
    DynWake(b);
    const float inv = 1.0f / std::max(0.1f, b.def.mass);
    for (int i = 0; i < 3; i++) b.v[i] += j[i] * inv;
    const float r[3] = {at[0] - b.p[0], at[1] - b.p[1], at[2] - b.p[2]};
    float t[3];
    Cross(r, j, t);
    for (int i = 0; i < 3; i++) b.w[i] += t[i] / b.def.turn;
}

void GameWorld::DynBreak(DynBody& b, bool explode) {
    if (b.dead) return;
    b.dead = true;
    b.awake = false;
    Inst& in = m_insts[b.inst];
    in.hidden = true;
    in.boxes.clear(); in.obb.clear(); in.spheres.clear();
    if (explode) { if (onDynExplode) onDynExplode(b.p); }
    else if (onDynSmash) onDynSmash(b.p);
}

size_t GameWorld::DynAwakeCount() const {
    size_t n = 0;
    for (const DynBody& b : m_dyn) if (b.awake && !b.dead) n++;
    return n;
}

// ------------------------------------------------------------------ step

void GameWorld::DynUpdate(float dt, float gravity) {
    if (dt <= 0.0f) return;
    dt = std::min(dt, 0.1f);
    const int steps = std::clamp((int)std::ceil(dt / 0.0167f), 1, 4);
    const float h = dt / (float)steps;
    for (DynBody& b : m_dyn) {
        if (!b.ready || !b.awake || b.dead) continue;
        m_skipInst = b.inst;
        const float invM = 1.0f / std::max(0.1f, b.def.mass);
        const float turn = b.def.turn;
        // contact points: corners and face centres, relative to the centre in local space
        // (every COL part: a box gives its corners and face centres, a sphere its centre with its radius)
        std::vector<std::array<float, 4>> lp;
        for (const DynBody::Part& pt : b.parts) {
            if (pt.sphere) { lp.push_back({pt.c[0], pt.c[1], pt.c[2], pt.r}); continue; }
            for (int i = 0; i < 2; i++) for (int j = 0; j < 2; j++) for (int k = 0; k < 2; k++)
                lp.push_back({pt.c[0] + (i ? pt.h[0] : -pt.h[0]), pt.c[1] + (j ? pt.h[1] : -pt.h[1]), pt.c[2] + (k ? pt.h[2] : -pt.h[2]), 0.0f});
            for (int a = 0; a < 3; a++) for (int sg = -1; sg <= 1; sg += 2) {
                std::array<float, 4> f = {pt.c[0], pt.c[1], pt.c[2], 0.0f};
                f[a] += sg * pt.h[a];
                lp.push_back(f);
            }
        }
        const int n = (int)lp.size();
        for (int step = 0; step < steps; step++) {
            b.v[2] -= gravity * h;
            const float drag = std::pow(std::clamp(b.def.airRes, 0.5f, 1.0f), h * 30.0f);
            for (int i = 0; i < 3; i++) { b.v[i] *= drag; b.w[i] *= std::pow(0.97f, h * 30.0f); }
            const float sp = Len(b.v);
            if (sp > 60.0f) for (int i = 0; i < 3; i++) b.v[i] *= 60.0f / sp;
            const float wl = Len(b.w);
            if (wl > 30.0f) for (int i = 0; i < 3; i++) b.w[i] *= 30.0f / wl;
            for (int i = 0; i < 3; i++) b.p[i] += b.v[i] * h;
            if (wl > 1e-4f) {
                const float th = wl * h;
                const float ax[3] = {b.w[0] / wl, b.w[1] / wl, b.w[2] / wl};
                const float cs = std::cos(th), sn = std::sin(th), oc = 1.0f - cs;
                // standard column rotation Rc (world vectors), the row form is its transpose
                const float rc[3][3] = {
                    {cs + ax[0] * ax[0] * oc, ax[0] * ax[1] * oc - ax[2] * sn, ax[0] * ax[2] * oc + ax[1] * sn},
                    {ax[1] * ax[0] * oc + ax[2] * sn, cs + ax[1] * ax[1] * oc, ax[1] * ax[2] * oc - ax[0] * sn},
                    {ax[2] * ax[0] * oc - ax[1] * sn, ax[2] * ax[1] * oc + ax[0] * sn, cs + ax[2] * ax[2] * oc}};
                float nr[3][3];
                for (int i = 0; i < 3; i++)
                    for (int j = 0; j < 3; j++)
                        nr[i][j] = b.R[i][0] * rc[j][0] + b.R[i][1] * rc[j][1] + b.R[i][2] * rc[j][2];
                std::memcpy(b.R, nr, sizeof(nr));
                Orthonormalize(b.R);
            }
            bool touched = false;
            for (int iter = 0; iter < 3; iter++) {
                for (int k = 0; k < n; k++) {
                    float r[3];
                    const float lpk[3] = {lp[k][0], lp[k][1], lp[k][2]};
                    const float rad = lp[k][3];
                    ToWorld(b.R, lpk, r);
                    const float pt[3] = {b.p[0] + r[0], b.p[1] + r[1], b.p[2] + r[2]};
                    float nrm[3] = {0, 0, 0}, pen = 0.0f;
                    const float low = pt[2] - rad;   // a sphere touches with its underside
                    const float gh = GetGroundHeight(pt[0], pt[1], low + 0.05f);
                    if (gh > kNoGroundV * 0.5f && gh > low && gh - low < 0.6f) {
                        nrm[2] = 1.0f; pen = gh - low;
                        if (rad > 0.0f) r[2] -= rad;
                    } else {
                        ProbeHit ph;
                        const float pr = rad > 0.0f ? rad : 0.05f;
                        if (SphereProbe(pt[0], pt[1], pt[2], pr, ph) && std::fabs(ph.n[2]) < 0.7f) {
                            const float d[3] = {pt[0] - ph.p[0], pt[1] - ph.p[1], pt[2] - ph.p[2]};
                            pen = pr - Len(d);
                            if (pen > 0.0f) { nrm[0] = ph.n[0]; nrm[1] = ph.n[1]; nrm[2] = ph.n[2]; }
                            if (rad > 0.0f) for (int i = 0; i < 3; i++) r[i] -= nrm[i] * rad;
                        }
                    }
                    if (pen <= 0.0f && Dot(nrm, nrm) < 0.5f) continue;
                    touched = true;
                    if (pen > 0.0f) for (int i = 0; i < 3; i++) b.p[i] += nrm[i] * std::min(pen, 0.15f) * 0.5f;
                    float wr[3], vp[3];
                    Cross(b.w, r, wr);
                    for (int i = 0; i < 3; i++) vp[i] = b.v[i] + wr[i];
                    const float vn = Dot(vp, nrm);
                    if (vn >= 0.0f) continue;
                    float rn[3];
                    Cross(r, nrm, rn);
                    const float e = vn < -1.5f ? b.def.elast : 0.0f;
                    const float jn = -(1.0f + e) * vn / (invM + Dot(rn, rn) / turn);
                    float imp[3] = {nrm[0] * jn, nrm[1] * jn, nrm[2] * jn};
                    float tq[3];
                    Cross(r, imp, tq);
                    for (int i = 0; i < 3; i++) { b.v[i] += imp[i] * invM; b.w[i] += tq[i] / turn; }
                    // friction along the contact
                    Cross(b.w, r, wr);
                    for (int i = 0; i < 3; i++) vp[i] = b.v[i] + wr[i];
                    const float vn2 = Dot(vp, nrm);
                    float vt[3] = {vp[0] - nrm[0] * vn2, vp[1] - nrm[1] * vn2, vp[2] - nrm[2] * vn2};
                    const float vtl = Len(vt);
                    if (vtl > 1e-4f) {
                        const float tdir[3] = {vt[0] / vtl, vt[1] / vtl, vt[2] / vtl};
                        float rt[3];
                        Cross(r, tdir, rt);
                        float jt = vtl / (invM + Dot(rt, rt) / turn);
                        jt = std::min(jt, 0.7f * jn);
                        const float fi[3] = {-tdir[0] * jt, -tdir[1] * jt, -tdir[2] * jt};
                        Cross(r, fi, tq);
                        for (int i = 0; i < 3; i++) { b.v[i] += fi[i] * invM; b.w[i] += tq[i] / turn; }
                    }
                }
            }
            if (touched) for (int i = 0; i < 3; i++) b.w[i] *= std::pow(0.9f, h * 30.0f);
        }
        m_skipInst = -1;
        if (Len(b.v) < 0.12f && Len(b.w) < 0.3f) b.still += dt; else b.still = 0.0f;
        if (b.still > kSleepTime) {
            b.awake = false;
            b.v[0] = b.v[1] = b.v[2] = 0.0f;
            b.w[0] = b.w[1] = b.w[2] = 0.0f;
        }
        if (getenv("GTA_DYNLOG")) {
            static int s_n = 0;
            if (++s_n % 15 == 0)
                std::fprintf(stderr, "[DynLog] %s p %.2f %.2f %.2f v %.2f %.2f %.2f awake\n", m_insts[b.inst].hdModel.c_str(), b.p[0], b.p[1], b.p[2], b.v[0], b.v[1], b.v[2]);
        }
        DynSync(b);
        if (b.p[2] < -50.0f) DynBreak(b, false);
    }
    m_skipInst = -1;
}

// ------------------------------------------------------------------ interactions

bool GameWorld::DynOverlapCapsule(const DynBody& b, float x, float y, float feetZ, float radius, float height, float* out) const {
    const float reach = Len(b.hx) + radius + 0.2f;
    if (std::fabs(x - b.p[0]) > reach || std::fabs(y - b.p[1]) > reach) return false;
    float best = 1e9f;
    bool hit = false;
    const float zs[3] = {feetZ + 0.35f, feetZ + height * 0.5f, feetZ + height - 0.1f};
    for (const DynBody::Part& pt : b.parts) {
        float cw[3];
        ToWorld(b.R, pt.c, cw);
        const float C[3] = {b.p[0] + cw[0], b.p[1] + cw[1], b.p[2] + cw[2]};
        for (float z : zs) {
            const float P[3] = {x, y, z};
            float Q[3];
            if (pt.sphere) {
                float d[3] = {P[0] - C[0], P[1] - C[1], P[2] - C[2]};
                const float dl = Len(d);
                if (dl < 1e-5f) { Q[0] = C[0]; Q[1] = C[1]; Q[2] = C[2]; }
                else for (int i = 0; i < 3; i++) Q[i] = C[i] + d[i] / dl * pt.r;
                if (dl < pt.r) { Q[0] = C[0] + d[0]; Q[1] = C[1] + d[1]; Q[2] = C[2] + d[2]; }
            } else {
                const float rel[3] = {P[0] - C[0], P[1] - C[1], P[2] - C[2]};
                float l[3];
                ToLocal(b.R, rel, l);
                float q[3];
                for (int i = 0; i < 3; i++) q[i] = std::clamp(l[i], -pt.h[i], pt.h[i]);
                float qw[3];
                ToWorld(b.R, q, qw);
                Q[0] = C[0] + qw[0]; Q[1] = C[1] + qw[1]; Q[2] = C[2] + qw[2];
            }
            float dx = x - Q[0], dy = y - Q[1];
            float dh = std::sqrt(dx * dx + dy * dy);
            if (dh < 1e-4f) {
                dx = x - b.p[0]; dy = y - b.p[1];
                dh = std::sqrt(dx * dx + dy * dy);
                if (dh < 1e-4f) { dx = 1.0f; dy = 0.0f; dh = 1.0f; }
                dh = 0.0f;
                const float inv = 1.0f / std::sqrt(dx * dx + dy * dy);
                dx *= inv; dy *= inv;
            } else {
                dx /= dh; dy /= dh;
            }
            if (dh < radius + 0.1f && dh < best) {
                best = dh;
                out[0] = dx; out[1] = dy; out[2] = radius - dh;
                out[3] = Q[0]; out[4] = Q[1]; out[5] = Q[2];
                hit = true;
            }
        }
    }
    return hit;
}

void GameWorld::DynPush(float x, float y, float feetZ, float radius, float height, float vx, float vy, float playerMass) {
    for (DynBody& b : m_dyn) {
        if (!b.ready || b.dead) continue;
        if (b.p[2] + Len(b.hx) < feetZ + 0.2f) continue;   // under the feet: walked over
        float o[6];
        if (!DynOverlapCapsule(b, x, y, feetZ, radius, height, o)) continue;
        const float vb[2] = {b.v[0], b.v[1]};
        const float closing = -((vx - vb[0]) * o[0] + (vy - vb[1]) * o[1]);   // normal points prop -> player
        if (closing < 0.25f) continue;
        const float m = b.def.mass;
        const float red = playerMass * m / (playerMass + m);
        const float jn = red * closing * 1.15f;
        const float at[3] = {o[3], o[4], o[5]};
        const float j[3] = {-o[0] * jn, -o[1] * jn, 0.0f};
        DynImpulse(b, at, j);
    }
}

void GameWorld::DynVehicle(float x, float y, float z, float vx, float vy, float radius, float carMass) {
    const float speed = std::sqrt(vx * vx + vy * vy);
    if (speed < 0.8f) return;
    for (DynBody& b : m_dyn) {
        if (!b.ready || b.dead || b.def.mass > 1500.0f) continue;
        float o[6];
        if (!DynOverlapCapsule(b, x, y, z - 0.4f, radius, 1.6f, o)) continue;
        const float closing = -((vx - b.v[0]) * o[0] + (vy - b.v[1]) * o[1]);
        if (closing < 0.5f) continue;
        if (b.def.cdEff == 20 || b.def.cdEff == 21) { DynBreak(b, false); continue; }
        if (b.def.expl && speed > 14.0f) { DynBreak(b, true); continue; }
        const float m = b.def.mass;
        const float red = carMass * m / (carMass + m);
        const float jn = red * closing * 1.3f;
        const float at[3] = {o[3], o[4], o[5]};
        const float j[3] = {-o[0] * jn, -o[1] * jn, jn * 0.25f};
        DynImpulse(b, at, j);
    }
}

float GameWorld::DynShoot(const float* origin, const float* dir, float range, float impulse, int gunType) {
    (void)gunType;
    DynBody* hit = nullptr;
    float bestT = range;
    for (DynBody& b : m_dyn) {
        if (!b.ready || b.dead) continue;
        for (const DynBody::Part& pt : b.parts) {
            float cw[3];
            ToWorld(b.R, pt.c, cw);
            const float C[3] = {b.p[0] + cw[0], b.p[1] + cw[1], b.p[2] + cw[2]};
            const float rel[3] = {origin[0] - C[0], origin[1] - C[1], origin[2] - C[2]};
            float t0 = 0.0f;
            bool ok = true;
            if (pt.sphere) {
                const float bq = Dot(rel, dir), cq = Dot(rel, rel) - pt.r * pt.r;
                const float disc = bq * bq - cq;
                if (disc < 0.0f) ok = false;
                else { t0 = -bq - std::sqrt(disc); if (t0 < 0.0f) t0 = cq <= 0.0f ? 0.0f : -1.0f; if (t0 < 0.0f) ok = false; }
            } else {
                float o[3], d[3];
                ToLocal(b.R, rel, o);
                ToLocal(b.R, dir, d);
                float t1 = bestT;
                for (int i = 0; i < 3 && ok; i++) {
                    if (std::fabs(d[i]) < 1e-8f) {
                        if (o[i] < -pt.h[i] || o[i] > pt.h[i]) ok = false;
                    } else {
                        float a2 = (-pt.h[i] - o[i]) / d[i], c2 = (pt.h[i] - o[i]) / d[i];
                        if (a2 > c2) std::swap(a2, c2);
                        t0 = std::max(t0, a2); t1 = std::min(t1, c2);
                        if (t0 > t1) ok = false;
                    }
                }
            }
            if (ok && t0 < bestT) { bestT = t0; hit = &b; }
        }
    }
    if (!hit) return -1.0f;
    const float at[3] = {origin[0] + dir[0] * bestT, origin[1] + dir[1] * bestT, origin[2] + dir[2] * bestT};
    if (hit->def.expl) { DynBreak(*hit, true); return bestT; }
    if (hit->def.cdEff == 20 || hit->def.cdEff == 21 || (hit->def.cdEff >= 200 && hit->def.gunBreak > 0)) {
        DynBreak(*hit, false);
        return bestT;
    }
    const float j[3] = {dir[0] * impulse, dir[1] * impulse, dir[2] * impulse};
    DynImpulse(*hit, at, j);
    return bestT;
}

void GameWorld::DynExplosion(const float* pos, float radius, float visible) {
    // CWorld::TriggerExplosionSectorList for objects: a body inside the radius takes a move force of
    // mass / 1400 * rel * visibleDistance (rel = min(1, 2 (R - d) / R), force units are m per 20 ms tick) along the
    // direction away from the centre (z at least 0.2), a turn force at half the bound radius above the centre,
    // and ObjectDamage(rel * 300).
    for (DynBody& b : m_dyn) {
        if (!b.ready || b.dead) continue;
        float d[3] = {b.p[0] - pos[0], b.p[1] - pos[1], b.p[2] - pos[2]};
        const float dist = Len(d);
        if (dist >= radius) continue;
        const float rel = std::min(1.0f, 2.0f * (radius - dist) / radius);
        const float fall = 1.0f - dist / radius;
        if (b.def.cdEff == 20 || b.def.cdEff == 21 || b.def.cdEff >= 200) { if (rel * 300.0f > 40.0f) { DynBreak(b, false); continue; } }
        if (b.def.expl && fall > 0.3f) { DynBreak(b, true); continue; }
        const float inv = 1.0f / std::max(0.01f, dist);
        for (float& v : d) v *= inv;
        if (d[2] < 0.1f) d[2] = 0.2f;
        const float dv = rel * visible / 1400.0f * 50.0f;   // m/s
        const float j[3] = {d[0] * dv * b.def.mass, d[1] * dv * b.def.mass, d[2] * dv * b.def.mass};
        const float at[3] = {b.p[0], b.p[1], b.p[2] + std::max(b.hx[0], std::max(b.hx[1], b.hx[2])) * 0.5f};
        DynImpulse(b, at, j);
    }
}
