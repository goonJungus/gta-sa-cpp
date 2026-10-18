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
        // The baked colours of a car are not its paint; paint comes from carcols.dat (not read yet), so
        // untextured body panels get a plain light grey and textured ones stay white.
        const uint32_t tint = dm.textureName.empty() ? 0xFFC0C0C0u : 0xFFFFFFFFu;
        for (DffVertex& v : dm.vertices) v.color = tint;
        D3DRenderMesh* mesh = r.CreateMesh(reinterpret_cast<const MeshVertex*>(dm.vertices.data()), (uint32_t)dm.vertices.size(),
                                           dm.indices.data(), (uint32_t)dm.indices.size());
        if (!mesh) continue;
        mesh->texture = texture(dm.textureName);
        mesh->doubleSided = true;
        if (wheel) { vm.wheelMesh = mesh; continue; }
        VehiclePart p;
        p.mesh = mesh;
        if (fr >= 0 && (size_t)fr < glob.size()) std::memcpy(p.m, glob[(size_t)fr].data(), sizeof(p.m));
        else { std::memset(p.m, 0, sizeof(p.m)); p.m[0] = p.m[5] = p.m[10] = p.m[15] = 1.0f; }
        vm.parts.push_back(p);
    }
    return vm.parts.empty() && !vm.wheelMesh ? nullptr : &vm;
}

int Vehicles::Spawn(D3DRenderer& r, int id, float x, float y, float z, float heading) {
    if (!Model(r, id)) return -1;
    VehicleInstance c;
    c.model = id; c.x = x; c.y = y; c.z = z; c.heading = heading;
    m_cars.push_back(c);
    return (int)m_cars.size() - 1;
}

void Vehicles::Draw(D3DRenderer& r) {
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    for (const VehicleInstance& c : m_cars) {
        auto it = m_models.find(c.model);
        if (it == m_models.end()) continue;
        const VehicleModel& vm = it->second;
        // Heading 0 faces +y like the ped; the model's nose is +y too.
        const float cs = std::cos(c.heading), sn = std::sin(c.heading);
        const float body[16] = {cs, sn, 0, 0, -sn, cs, 0, 0, 0, 0, 1, 0, c.x, c.y, c.z, 1};
        D3DMATRIX out;
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
    const float vmax = h->maxVelocity / 3.6f * 0.85f;
    const float accel = h->engineAccel * 0.55f;
    // Throttle fades out towards top speed; reverse is slower.
    if (throttle > 0.0f && c.speed > -0.5f) c.speed += throttle * accel * std::max(0.0f, 1.0f - c.speed / vmax) * dt;
    if (brake > 0.0f) {
        if (c.speed > 0.3f) c.speed -= brake * h->brakeDecel * 1.2f * dt;
        else c.speed -= brake * accel * 0.5f * std::max(0.0f, 1.0f + c.speed / (vmax * 0.3f)) * dt;
    }
    if (handbrake) c.speed -= std::copysign(std::min(std::fabs(c.speed), 12.0f * dt), c.speed);
    // Rolling and air drag.
    const float drag = 0.15f + h->dragMult * 0.004f * std::fabs(c.speed);
    if (throttle <= 0.0f && brake <= 0.0f) c.speed -= std::copysign(std::min(std::fabs(c.speed), drag * 4.0f * dt), c.speed);
    // Steering eases towards the target and the lock narrows with speed.
    const float lock = h->steeringLock * 0.0174533f / (1.0f + std::fabs(c.speed) * 0.045f);
    c.steer += (steerIn * lock - c.steer) * std::min(1.0f, 8.0f * dt);
    const float wheelbase = 2.8f;
    c.heading -= std::tan(c.steer) * c.speed / wheelbase * dt;   // steer > 0 turns right (clockwise)
    const float nx = c.x - std::sin(c.heading) * c.speed * dt, ny = c.y + std::cos(c.heading) * c.speed * dt;
    // Heading 0 faces +y; a positive heading turns left, so the nose is (-sin, cos).
    if (blocked && blocked(nx, ny, c.x, c.y, c.z)) {
        c.speed *= -0.25f;
    } else {
        c.x = nx; c.y = ny;
    }
    if (groundAt) {
        const float g = groundAt(c.x, c.y, c.z + 1.0f);
        if (g > -1e5f) c.z += (g + 0.1f - c.z) * std::min(1.0f, 12.0f * dt);
    }
    c.wheelSpin += c.speed * dt / 0.35f;
}
