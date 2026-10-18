#include "Vehicle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "DffLoader.h"
#include "ImgLoader.h"
#include "TxdLoader.h"

namespace {

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// A DFF clump's frames (matrix, parent, name) and its atomics (frame, geometry).
struct DffFrame { float m[12]; int parent = -1; std::string name; };
struct DffAtomic { int frame, geom; };

struct Walker {
    const std::vector<uint8_t>& b;
    std::vector<DffFrame> frames;
    std::vector<DffAtomic> atomics;
    uint32_t Rd(size_t o) const { uint32_t v = 0; if (o + 4 <= b.size()) std::memcpy(&v, &b[o], 4); return v; }
    void FrameList(size_t p, size_t end) {
        size_t q = p;
        uint32_t n = 0;
        if (q + 12 <= end && Rd(q) == 1) {   // struct: count, then 56 bytes per frame
            n = Rd(q + 12);
            for (uint32_t i = 0; i < n && q + 16 + (i + 1) * 56 <= end; i++) {
                DffFrame f;
                std::memcpy(f.m, &b[q + 16 + i * 56], 48);
                f.parent = (int)Rd(q + 16 + i * 56 + 48);
                frames.push_back(f);
            }
            q += 12 + Rd(q + 4);
        }
        for (uint32_t i = 0; i < n && q + 12 <= end; i++) {   // one extension per frame
            const uint32_t t = Rd(q), sz = Rd(q + 4);
            if (q + 12 + sz > end) break;
            if (t == 3 && i < frames.size()) {
                size_t e = q + 12;
                while (e + 12 <= q + 12 + sz) {
                    const uint32_t pt = Rd(e), ps = Rd(e + 4);
                    if (pt == 0x253F2FE && e + 12 + ps <= b.size())
                        frames[i].name = Lower(std::string((const char*)&b[e + 12], strnlen((const char*)&b[e + 12], ps)));
                    e += 12 + ps;
                }
            }
            q += 12 + sz;
        }
    }
    void Walk(size_t o, size_t end) {
        while (o + 12 <= end) {
            const uint32_t t = Rd(o), sz = Rd(o + 4);
            const size_t p = o + 12;
            if (p + sz > end) return;
            if (t == 0x10) Walk(p, p + sz);
            else if (t == 0x0E) FrameList(p, p + sz);
            else if (t == 0x14 && Rd(p) == 1 && Rd(p + 4) >= 12) atomics.push_back({(int)Rd(p + 12), (int)Rd(p + 16)});
            o = p + sz;
        }
    }
};

void Mul(const float* a, const float* b, float* o) {   // 4x4 row vectors: o = a * b
    float t[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float v = 0.0f;
            for (int k = 0; k < 4; k++) v += a[i * 4 + k] * b[k * 4 + j];
            t[i * 4 + j] = v;
        }
    std::memcpy(o, t, sizeof(t));
}

void FrameMatrix(const float* m12, float* o) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 3; j++) o[i * 4 + j] = m12[i * 3 + j];
        o[i * 4 + 3] = i == 3 ? 1.0f : 0.0f;
    }
}

// Parts that are not shown on an undamaged car: damage and low detail stand-ins, extras.
bool HiddenPart(const std::string& n) {
    return n.find("_dam") != std::string::npos || n.find("_vlo") != std::string::npos ||
           n.find("_lo") != std::string::npos || n.compare(0, 5, "extra") == 0 || n.empty();
}

}  // namespace

bool Vehicles::Init(D3DRenderer&, const std::string& gameDir) {
    m_dir = gameDir;
    {   // data/vehicles.ide: cars section
        std::istringstream in(ReadText(gameDir + "/data/vehicles.ide"));
        bool cars = false;
        for (std::string line; std::getline(in, line);) {
            line = Trim(line);
            if (line.empty() || line[0] == '#') continue;
            if (line == "cars") { cars = true; continue; }
            if (line == "end") { cars = false; continue; }
            if (!cars) continue;
            std::vector<std::string> f;
            std::stringstream ss(line);
            for (std::string s; std::getline(ss, s, ',');) f.push_back(Trim(s));
            if (f.size() < 15) continue;
            VehicleInfo v;
            v.id = std::atoi(f[0].c_str());
            v.dff = Lower(f[1]); v.txd = Lower(f[2]); v.type = f[3]; v.handling = Lower(f[4]); v.gameName = f[5];
            v.wheelScaleFront = (float)std::atof(f[12].c_str());
            v.wheelScaleRear = (float)std::atof(f[13].c_str());
            m_info[v.id] = v;
            if (v.type == "car") m_ids.push_back(v.id);
        }
    }
    {   // data/handling.cfg: the standard vehicle lines (one name, then numbers and letters)
        std::istringstream in(ReadText(gameDir + "/data/handling.cfg"));
        for (std::string line; std::getline(in, line);) {
            line = Trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '!' || line[0] == '$' || line[0] == '%' || line[0] == '^') continue;
            std::istringstream ls(line);
            std::vector<std::string> t;
            for (std::string s; ls >> s;) t.push_back(s);
            if (t.size() < 30) continue;
            HandlingData h;
            h.name = Lower(t[0]);
            h.mass = (float)std::atof(t[1].c_str());
            h.turnMass = (float)std::atof(t[2].c_str());
            h.dragMult = (float)std::atof(t[3].c_str());
            for (int i = 0; i < 3; i++) h.com[i] = (float)std::atof(t[4 + i].c_str());
            h.percentSubmerged = std::atoi(t[7].c_str());
            h.tractionMult = (float)std::atof(t[8].c_str());
            h.tractionLoss = (float)std::atof(t[9].c_str());
            h.tractionBias = (float)std::atof(t[10].c_str());
            h.numGears = std::atoi(t[11].c_str());
            h.maxVelocity = (float)std::atof(t[12].c_str());
            h.engineAccel = (float)std::atof(t[13].c_str());
            h.engineInertia = (float)std::atof(t[14].c_str());
            h.driveType = t[15][0];
            h.engineType = t[16][0];
            h.brakeDecel = (float)std::atof(t[17].c_str());
            h.brakeBias = (float)std::atof(t[18].c_str());
            h.steeringLock = (float)std::atof(t[20].c_str());
            m_handling[h.name] = h;
        }
    }
    {   // data/carcols.dat: the colour table (col) and the allowed colour pairs per model (car)
        std::istringstream in(ReadText(gameDir + "/data/carcols.dat"));
        std::string sect;
        for (std::string line; std::getline(in, line);) {
            const size_t hash = line.find('#');
            if (hash != std::string::npos) line = line.substr(0, hash);
            line = Trim(line);
            if (line.empty()) continue;
            if (line == "col" || line == "car" || line == "car4" || line == "car3") { sect = line; continue; }
            if (line == "end") { sect.clear(); continue; }
            std::vector<std::string> f;
            std::stringstream ss(line);
            for (std::string t; std::getline(ss, t, ',');) f.push_back(Trim(t));
            if (sect == "col" && f.size() >= 3)
                m_colours.push_back({(uint8_t)std::atoi(f[0].c_str()), (uint8_t)std::atoi(f[1].c_str()), (uint8_t)std::atoi(f[2].c_str())});
            else if ((sect == "car" || sect == "car4") && f.size() >= 3) {
                const int per = sect == "car" ? 2 : 4;
                std::vector<std::array<int, 4>>& sets = m_carColours[Lower(f[0])];
                for (size_t i = 1; i + (size_t)per <= f.size(); i += (size_t)per) {
                    std::array<int, 4> c = {std::atoi(f[i].c_str()), std::atoi(f[i + 1].c_str()), 0, 0};
                    if (per == 4) { c[2] = std::atoi(f[i + 2].c_str()); c[3] = std::atoi(f[i + 3].c_str()); }
                    sets.push_back(c);
                }
            }
        }
    }
    {
        ImgLoader img;
        if (img.Open(gameDir + "/models/gta3.img")) m_sharedTxd = img.Extract("vehicle.txd");
        if (m_sharedTxd.empty()) {
            std::ifstream f(gameDir + "/models/generic/vehicle.txd", std::ios::binary);
            m_sharedTxd.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        }
    }
    m_ready = !m_info.empty() && !m_handling.empty();
    std::fprintf(stderr, "[VEH] %zu models, %zu handling lines, shared txd %zu bytes\n", m_info.size(), m_handling.size(),
                 m_sharedTxd.size());
    return m_ready;
}

void Vehicles::Shutdown(D3DRenderer& r) {
    for (auto& kv : m_models) {
        for (VehiclePart& p : kv.second.parts) if (p.mesh) r.DestroyMesh(p.mesh);
        if (kv.second.wheelMesh) r.DestroyMesh(kv.second.wheelMesh);
        for (IDirect3DTexture9* t : kv.second.textures) if (t) r.DestroyTexture(t);
    }
    m_models.clear();
    m_cars.clear();
}

const HandlingData* Vehicles::Handling(const std::string& name) const {
    auto it = m_handling.find(Lower(name));
    return it == m_handling.end() ? nullptr : &it->second;
}

const VehicleInfo* Vehicles::Info(int id) const {
    auto it = m_info.find(id);
    return it == m_info.end() ? nullptr : &it->second;
}

VehicleModel* Vehicles::Model(D3DRenderer& r, int id) {
    auto found = m_models.find(id);
    if (found != m_models.end()) return found->second.parts.empty() && !found->second.wheelMesh ? nullptr : &found->second;
    const VehicleInfo* info = Info(id);
    if (!info) return nullptr;
    VehicleModel& vm = m_models[id];
    vm.info = *info;
    ImgLoader img;
    if (!img.Open(m_dir + "/models/gta3.img")) return nullptr;
    const std::vector<uint8_t> dffData = img.Extract(info->dff + ".dff");
    const std::vector<uint8_t> txdData = img.Extract(info->txd + ".txd");
    if (dffData.empty()) return nullptr;
    std::vector<TxdTexture> own, shared;
    if (!txdData.empty()) own = TxdLoader::LoadFromMemory(txdData.data(), txdData.size());
    if (!m_sharedTxd.empty()) shared = TxdLoader::LoadFromMemory(m_sharedTxd.data(), m_sharedTxd.size());
    std::unordered_map<std::string, IDirect3DTexture9*> tex;
    auto texture = [&](const std::string& name) -> IDirect3DTexture9* {
        const std::string key = TxdLoader::KeyOf(name);
        auto it = tex.find(key);
        if (it != tex.end()) return it->second;
        IDirect3DTexture9* t = nullptr;
        for (const std::vector<TxdTexture>* set : {&own, &shared})
            for (const TxdTexture& x : *set) {
                if (t || TxdLoader::KeyOf(x.name) != key) continue;
                t = r.CreateTexture(x.width, x.height, x.d3dFormat, x.mip0.data(), (uint32_t)x.mip0.size(), &x.extraMips);
                if (t) vm.textures.push_back(t);
            }
        tex[key] = t;
        return t;
    };

    DffModel dff = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
    Walker w{dffData, {}, {}};
    w.Walk(0, dffData.size());
    // Global matrix of every frame (parents come first in the list).
    std::vector<std::array<float, 16>> glob(w.frames.size());
    for (size_t i = 0; i < w.frames.size(); i++) {
        float local[16];
        FrameMatrix(w.frames[i].m, local);
        const int p = w.frames[i].parent;
        if (p >= 0 && (size_t)p < i) Mul(local, glob[(size_t)p].data(), glob[i].data());
        else std::memcpy(glob[i].data(), local, sizeof(local));
    }
    static const char* kWheelDummy[4] = {"wheel_lf_dummy", "wheel_rf_dummy", "wheel_lb_dummy", "wheel_rb_dummy"};
    for (size_t i = 0; i < w.frames.size(); i++)
        for (int k = 0; k < 4; k++)
            if (w.frames[i].name == kWheelDummy[k]) {
                vm.wheelFound[k] = true;
                for (int c = 0; c < 3; c++) vm.wheelPos[k][c] = glob[i][12 + (size_t)c];
            }
    std::fprintf(stderr, "[VEH] %s frames:", info->dff.c_str());
    for (const DffFrame& f : w.frames) std::fprintf(stderr, " %s", f.name.c_str());
    std::fprintf(stderr, "\n");

    for (DffMesh& dm : dff.meshes) {
        if (dm.vertices.empty() || dm.indices.empty()) continue;
        const DffAtomic* at = nullptr;
        for (const DffAtomic& a : w.atomics) if (a.geom == dm.geometry) { at = &a; break; }
        const int fr = at ? at->frame : -1;
        const std::string name = fr >= 0 && (size_t)fr < w.frames.size() ? w.frames[(size_t)fr].name : std::string();
        const bool wheel = name == "wheel";
        if (!wheel && HiddenPart(name)) continue;
        // Paint areas are marked with fixed material colours (green, pink, cyan, magenta); the rest of the car
        // keeps its material colour, white under a texture.
        const uint32_t mc = dm.materialColor & 0x00FFFFFFu;
        int slot = -1;
        if (mc == 0x003CFF00u) slot = 0; else if (mc == 0x00FF00AFu) slot = 1; else if (mc == 0x0000FFFFu) slot = 2; else if (mc == 0x00FF00FFu) slot = 3;
        const uint32_t tint = !dm.textureName.empty() ? 0xFFFFFFFFu : (slot >= 0 ? 0xFFFFFFFFu : (0xFF000000u | mc));
        for (DffVertex& v : dm.vertices) v.color = tint;
        D3DRenderMesh* mesh = r.CreateMesh(reinterpret_cast<const MeshVertex*>(dm.vertices.data()), (uint32_t)dm.vertices.size(),
                                           dm.indices.data(), (uint32_t)dm.indices.size());
        if (!mesh) continue;
        mesh->texture = texture(dm.textureName);
        mesh->doubleSided = true;
        if (slot >= 0) {
            VehicleModel::PaintPart pp;
            pp.mesh = mesh; pp.slot = slot;
            pp.verts.assign(reinterpret_cast<const MeshVertex*>(dm.vertices.data()), reinterpret_cast<const MeshVertex*>(dm.vertices.data()) + dm.vertices.size());
            vm.paintParts.push_back(std::move(pp));
        }
        if (wheel) {
            float lo = 1e9f, hi = -1e9f;
            for (const DffVertex& v : dm.vertices) { lo = std::min(lo, v.z); hi = std::max(hi, v.z); }
            if (hi > lo) vm.wheelRadius = 0.5f * (hi - lo);
            vm.wheelMesh = mesh;
            continue;
        }
        VehiclePart p;
        p.mesh = mesh;
        if (fr >= 0 && (size_t)fr < glob.size()) std::memcpy(p.m, glob[(size_t)fr].data(), sizeof(p.m));
        else { std::memset(p.m, 0, sizeof(p.m)); p.m[0] = p.m[5] = p.m[10] = p.m[15] = 1.0f; }
        vm.parts.push_back(p);
    }
    {
        float sumZ = 0.0f; int cnt = 0;
        for (int k = 0; k < 4; k++) if (vm.wheelFound[k]) { sumZ += vm.wheelPos[k][2]; cnt++; }
        if (cnt) vm.rideHeight = vm.wheelRadius - sumZ / (float)cnt - 0.04f;
        if (vm.wheelFound[0] && vm.wheelFound[2]) vm.wheelBase = std::max(1.5f, std::fabs(vm.wheelPos[0][1] - vm.wheelPos[2][1]));
    }
    return vm.parts.empty() && !vm.wheelMesh ? nullptr : &vm;
}

int Vehicles::Spawn(D3DRenderer& r, int id, float x, float y, float z, float heading) {
    const VehicleModel* spawnModel = Model(r, id);
    if (!spawnModel) return -1;
    VehicleInstance c;
    c.model = id; c.x = x; c.y = y; c.z = z + spawnModel->rideHeight;   // z is the ground under the car c.heading = heading;
    auto cc = m_carColours.find(Info(id)->dff);
    if (cc != m_carColours.end() && !cc->second.empty()) {
        m_rng = m_rng * 1664525u + 1013904223u;
        const std::array<int, 4>& set = cc->second[(m_rng >> 8) % cc->second.size()];
        for (int k = 0; k < 4; k++) c.paint[k] = set[(size_t)k];
    }
    m_cars.push_back(c);
    return (int)m_cars.size() - 1;
}

void Vehicles::Draw(D3DRenderer& r) {
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    // The paint is the vertex colour: let it drive the ambient term too, or the bright ambient washes it out.
    dev->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
    for (const VehicleInstance& c : m_cars) {
        auto it = m_models.find(c.model);
        if (it == m_models.end()) continue;
        const VehicleModel& vm = it->second;
        // Heading 0 faces +y like the ped; the model's nose is +y too.
        const float cs = std::cos(c.heading), sn = std::sin(c.heading);
        const float body[16] = {cs, sn, 0, 0, -sn, cs, 0, 0, 0, 0, 1, 0, c.x, c.y, c.z, 1};
        D3DMATRIX out;
        for (const VehicleModel::PaintPart& pp : const_cast<VehicleModel&>(vm).paintParts) {   // this car's paint
            const int idx = c.paint[pp.slot];
            const std::array<uint8_t, 3> rgb = idx >= 0 && (size_t)idx < m_colours.size() ? m_colours[(size_t)idx] : std::array<uint8_t, 3>{200, 200, 200};
            const uint32_t col = 0xFF000000u | ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | rgb[2];
            std::vector<MeshVertex>& vv = const_cast<VehicleModel::PaintPart&>(pp).verts;
            for (MeshVertex& v : vv) v.color = col;
            r.UpdateMeshVertices(pp.mesh, vv.data(), (uint32_t)vv.size());
        }
        for (const VehiclePart& p : vm.parts) {
            Mul(p.m, body, &out._11);
            dev->SetTransform(D3DTS_WORLD, &out);
            r.DrawMesh(p.mesh);
        }
        if (!vm.wheelMesh) continue;
        for (int k = 0; k < 4; k++) {
            if (!vm.wheelFound[k]) continue;
            const float side = (k % 2 == 0) ? -1.0f : 1.0f;   // left wheels are the mirrored model
            const float spin = c.wheelSpin, sc = std::cos(spin), ss = std::sin(spin);
            // Wheel model: x across, spin about x.
            float wm[16] = {side, 0, 0, 0, 0, sc, ss, 0, 0, -ss, sc, 0, 0, 0, 0, 1};
            if (k < 2) {   // front wheels turn about the vertical axis after they spin
                const float a = -c.steer, ca = std::cos(a), sa = std::sin(a);
                const float rz[16] = {ca, sa, 0, 0, -sa, ca, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                Mul(wm, rz, wm);
            }
            wm[12] = vm.wheelPos[k][0]; wm[13] = vm.wheelPos[k][1]; wm[14] = vm.wheelPos[k][2];
            float t[16];
            Mul(wm, body, t);
            std::memcpy(&out._11, t, sizeof(t));
            dev->SetTransform(D3DTS_WORLD, &out);
            r.DrawMesh(vm.wheelMesh);
        }
    }
    dev->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
}

int Vehicles::Nearest(float x, float y, float range) const {
    int best = -1;
    float bd = range * range;
    for (size_t i = 0; i < m_cars.size(); i++) {
        const float dx = m_cars[i].x - x, dy = m_cars[i].y - y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = (int)i; }
    }
    return best;
}

void Vehicles::Step(size_t i, float dt, float throttle, float brake, float steerIn, bool handbrake,
                    const std::function<float(float, float, float)>& groundAt,
                    const std::function<bool(float, float, float, float, float)>& blocked) {
    if (i >= m_cars.size()) return;
    VehicleInstance& c = m_cars[i];
    const VehicleInfo* info = Info(c.model);
    const HandlingData* h = info ? Handling(info->handling) : nullptr;
    HandlingData def;
    if (!h) h = &def;
    auto mi = m_models.find(c.model);
    const float ride = mi != m_models.end() ? mi->second.rideHeight : 0.7f;
    const float wheelbase = mi != m_models.end() ? mi->second.wheelBase : 2.8f;

    // Velocity split along the nose and across the car; tyres kill the sideways part.
    const float fx = -std::sin(c.heading), fy = std::cos(c.heading);   // heading 0 faces +y, positive turns left
    const float rx = fy, ry = -fx;
    float vf = c.vx * fx + c.vy * fy, vl = c.vx * rx + c.vy * ry;

    const float vmax = h->maxVelocity / 3.6f;
    const float accel = h->engineAccel * 0.4f + 2.0f;   // m/s^2 at a standstill
    if (throttle > 0.0f) {
        if (vf > -0.5f) vf += throttle * accel * std::max(0.0f, 1.0f - std::pow(std::max(vf, 0.0f) / vmax, 1.5f)) * dt;
        else vf += throttle * h->brakeDecel * dt;   // throttle while rolling backwards brakes first
    }
    if (brake > 0.0f) {
        if (vf > 0.3f) vf = std::max(0.0f, vf - brake * h->brakeDecel * dt);
        else vf -= brake * accel * 0.5f * std::max(0.0f, 1.0f - std::pow(std::max(-vf, 0.0f) / (vmax * 0.3f), 1.5f)) * dt;
    }
    // Rolling resistance always, air drag grows with the square of the speed.
    const float roll = 0.35f + h->dragMult * 0.0008f * vf * vf;
    if (throttle <= 0.0f && brake <= 0.0f) vf -= std::copysign(std::min(std::fabs(vf), (roll + 0.6f) * dt), vf);
    else vf -= std::copysign(std::min(std::fabs(vf), roll * 0.5f * dt), vf);

    // Grip: how fast the tyres remove sideways speed. The handbrake locks the rears; hard throttle
    // on a rear driven car with the wheel turned also loosens them (tractionLoss).
    float grip = 14.0f * h->tractionMult;
    if (handbrake) { grip *= 0.18f; vf -= std::copysign(std::min(std::fabs(vf), 6.0f * dt), vf); }
    else if (h->driveType != 'F' && throttle > 0.8f && std::fabs(c.steer) > 0.2f && std::fabs(vf) < vmax * 0.6f)
        grip *= 0.35f + 0.65f * h->tractionLoss;
    vl *= std::exp(-grip * dt);

    // Steering eases towards the target and the lock narrows with speed.
    const float lock = h->steeringLock * 0.0174533f / (1.0f + std::fabs(vf) * 0.03f);
    c.steer += (steerIn * lock - c.steer) * std::min(1.0f, 7.0f * dt);
    // Heavier cars turn in slower (turnMass), a loose rear spins up extra.
    const float yawRate = std::tan(c.steer) * vf / wheelbase * (1.0f + (handbrake ? 0.5f : 0.0f));
    c.heading -= yawRate * dt;   // steer > 0 turns right (clockwise)

    // Back to world axes with the new heading.
    const float nfx = -std::sin(c.heading), nfy = std::cos(c.heading);
    c.vx = nfx * vf + nfy * vl; c.vy = nfy * vf - nfx * vl;
    const float nx = c.x + c.vx * dt, ny = c.y + c.vy * dt;
    if (blocked && blocked(nx, ny, c.x, c.y, c.z)) {
        c.vx *= -0.25f; c.vy *= -0.25f;
    } else {
        c.x = nx; c.y = ny;
    }
    c.speed = c.vx * nfx + c.vy * nfy;
    if (groundAt) {
        const float g = groundAt(c.x, c.y, c.z + 1.0f);
        if (g > -1e5f) c.z += (g + ride - c.z) * std::min(1.0f, 12.0f * dt);
    }
    c.wheelSpin += c.speed * dt / std::max(0.2f, mi != m_models.end() ? mi->second.wheelRadius : 0.35f);
}

int Vehicles::Trace(const float* eye, const float* dir, float reach, float* t) const {
    int best = -1;
    float bt = reach;
    for (size_t i = 0; i < m_cars.size(); i++) {
        const VehicleInstance& c = m_cars[i];
        auto mi = m_models.find(c.model);
        const float wb = mi != m_models.end() ? mi->second.wheelBase : 2.8f;
        const float ride = mi != m_models.end() ? mi->second.rideHeight : 0.7f;
        const float hx = 0.95f, hy = wb * 0.5f + 1.1f, z0 = -ride, z1 = -ride + 1.5f;
        // into car space: right = (cos h, sin h), nose = (-sin h, cos h)
        const float cs = std::cos(c.heading), sn = std::sin(c.heading);
        const float ox = eye[0] - c.x, oy = eye[1] - c.y, oz = eye[2] - c.z;
        const float o[3] = {ox * cs + oy * sn, -ox * sn + oy * cs, oz};
        const float d[3] = {dir[0] * cs + dir[1] * sn, -dir[0] * sn + dir[1] * cs, dir[2]};
        const float lo[3] = {-hx, -hy, z0}, hi[3] = {hx, hy, z1};
        float t0 = 0.0f, t1 = bt;
        bool ok = true;
        for (int k = 0; k < 3 && ok; k++) {
            if (std::fabs(d[k]) < 1e-7f) { if (o[k] < lo[k] || o[k] > hi[k]) ok = false; continue; }
            float a = (lo[k] - o[k]) / d[k], b = (hi[k] - o[k]) / d[k];
            if (a > b) std::swap(a, b);
            t0 = std::max(t0, a); t1 = std::min(t1, b);
            if (t0 > t1) ok = false;
        }
        if (ok && t0 < bt) { bt = t0; best = (int)i; }
    }
    if (best >= 0 && t) *t = bt;
    return best;
}

bool Vehicles::Damage(int car, float damage) {
    if (car < 0 || (size_t)car >= m_cars.size()) return false;
    VehicleInstance& c = m_cars[(size_t)car];
    if (c.wrecked || c.health <= 0.0f) return false;
    c.health -= damage;
    if (c.health <= 0.0f) { c.health = 0.0f; return true; }
    return false;
}

void Vehicles::UpdateDamage(float dt, const std::function<void(const float*, float)>& onSmoke, const std::function<void(const float*)>& onFire,
                            const std::function<void(const float*)>& onExplode) {
    for (VehicleInstance& c : m_cars) {
        if (c.wrecked) {   // a burnt out shell keeps smoking
            const float p[3] = {c.x, c.y, c.z + 0.6f};
            if (std::fmod(c.burnTime += dt, 0.12f) < dt) onSmoke(p, 1.0f);
            continue;
        }
        const float p[3] = {c.x - std::sin(c.heading) * 1.4f, c.y + std::cos(c.heading) * 1.4f, c.z + 0.6f};   // the bonnet
        if (c.health <= 0.0f) {
            c.burnTime += dt;
            onFire(p);
            onSmoke(p, 0.9f);
            c.vx *= 1.0f - std::min(1.0f, 2.0f * dt); c.vy *= 1.0f - std::min(1.0f, 2.0f * dt);
            if (c.burnTime > 5.0f) {
                c.wrecked = true; c.burnTime = 0.0f;
                for (int k = 0; k < 4; k++) c.paint[k] = 0;   // burnt black
                c.speed = 0.0f; c.vx = c.vy = 0.0f;
                onExplode(p);
            }
        } else if (c.health < 250.0f) {
            c.burnTime += dt;
            if (std::fmod(c.burnTime, 0.15f) < dt) onSmoke(p, 0.3f + 0.5f * (1.0f - c.health / 250.0f));
        }
    }
}
