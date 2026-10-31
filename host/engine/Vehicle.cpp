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

// The clump's collision plugin (0x253F2FA, CCollisionPlugin): a COL2/COL3 model with the body spheres and boxes
// (CFileLoader::LoadCollisionModelVer2/3). Offsets inside it count from just after the magic.
void ParseVehicleCol(const std::vector<uint8_t>& b, size_t o, size_t end, VehicleModel& vm) {
    auto u32 = [&](size_t p) { uint32_t v = 0; if (p + 4 <= b.size()) std::memcpy(&v, &b[p], 4); return v; };
    auto u16 = [&](size_t p) { uint16_t v = 0; if (p + 2 <= b.size()) std::memcpy(&v, &b[p], 2); return v; };
    auto f32 = [&](size_t p) { float v = 0; if (p + 4 <= b.size()) std::memcpy(&v, &b[p], 4); return v; };
    while (o + 12 <= end && !vm.colFound) {
        const uint32_t t = u32(o), sz = u32(o + 4);
        const size_t p = o + 12;
        if (p + sz > end) return;
        if (t == 0x253F2FA && sz >= 0x6C && (std::memcmp(&b[p], "COL3", 4) == 0 || std::memcmp(&b[p], "COL2", 4) == 0)) {
            for (int k = 0; k < 3; k++) { vm.colMin[k] = f32(p + 0x20 + 4 * (size_t)k); vm.colMax[k] = f32(p + 0x2C + 4 * (size_t)k); }
            const uint16_t ns = u16(p + 0x48), nb = u16(p + 0x4A);
            const uint32_t offS = u32(p + 0x54), offB = u32(p + 0x58);
            for (uint16_t k = 0; k < ns; k++) {
                const size_t q = p + 4 + offS + (size_t)k * 20;
                if (q + 20 > p + sz) break;
                vm.colSpheres.push_back({{f32(q), f32(q + 4), f32(q + 8)}, f32(q + 12), b[q + 17]});   // CColSurface: material, piece
            }
            for (uint16_t k = 0; k < nb; k++) {
                const size_t q = p + 4 + offB + (size_t)k * 28;
                if (q + 28 > p + sz) break;
                vm.colBoxes.push_back({{f32(q), f32(q + 4), f32(q + 8)}, {f32(q + 12), f32(q + 16), f32(q + 20)}, b[q + 25]});
            }
            vm.colFound = true;
            return;
        }
        if (t == 0x10 || t == 0x3) ParseVehicleCol(b, p, p + sz, vm);
        o = p + sz;
    }
}

// Parts that are not shown on an undamaged car: damage and low detail stand-ins, extras.
bool HiddenPart(const std::string& n) {
    return n.find("_dam") != std::string::npos || n.find("_vlo") != std::string::npos ||
           n.find("_lo") != std::string::npos || n.compare(0, 5, "extra") == 0 || n.empty();
}

// The damageable component a part belongs to (CDamageManager::GetCarNodeIndexFromDoor / FromPanel): its
// `<node>_ok` mesh shows while intact, `<node>_dam` once damaged. -1 for other parts.
int DamageComp(const std::string& n, bool* damaged) {
    static const char* kComp[13] = {"bonnet", "boot", "door_lf", "door_rf", "door_lr", "door_rr",
                                    "wing_lf", "wing_rf", "wing_lr", "wing_rr", "windscreen", "bump_front", "bump_rear"};
    for (int k = 0; k < 13; k++) {
        const std::string c = kComp[k];
        if (n == c + "_ok") { *damaged = false; return k; }
        if (n == c + "_dam") { *damaged = true; return k; }
    }
    return -1;
}

// CAutomobile::PreRender: the rhino and swat van turret (misc_a yaws about Z by the vertical doom angle, misc_b pitches
// about X by the horizontal one) and the fire truck nozzle (misc_a pitches about X, then yaws about Z). The rotation
// replaces the frame's own and keeps its position.
bool TurretRot(int model, int node, float doomV, float doomH, float r[9]) {
    const float cv = std::cos(doomV), sv = std::sin(doomV), ch = std::cos(doomH), sh = std::sin(doomH);
    const float rz[9] = {cv, sv, 0, -sv, cv, 0, 0, 0, 1};
    const float rx[9] = {1, 0, 0, 0, ch, sh, 0, -sh, ch};
    if ((model == 432 || model == 601) && node == 0) { std::memcpy(r, rz, sizeof(rz)); return true; }
    if ((model == 432 || model == 601) && node == 1) { std::memcpy(r, rx, sizeof(rx)); return true; }
    if (model == 407 && node == 0) {
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) r[i * 3 + j] = rx[i * 3] * rz[j] + rx[i * 3 + 1] * rz[3 + j] + rx[i * 3 + 2] * rz[6 + j];
        return true;
    }
    return false;
}

// Frame (car space) of a turret node: local frame with the turret rotation, on top of its parent.
void NodeCarSpace(const VehicleModel& vm, int model, int node, float doomV, float doomH, float out[16]) {
    const VehicleModel::Node& nd = vm.nodes[node];
    float loc[16];
    std::memcpy(loc, nd.local, sizeof(loc));
    float r[9];
    if (TurretRot(model, node, doomV, doomH, r))
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) loc[i * 4 + j] = r[i * 3 + j];
    if (nd.parent >= 0 && vm.nodes[nd.parent].found) {
        float pm[16];
        NodeCarSpace(vm, model, nd.parent, doomV, doomH, pm);
        Mul(loc, pm, out);
    } else Mul(loc, nd.base, out);
}

}  // namespace

bool Vehicles::NodeMatrix(size_t i, int node, float out[16]) const {
    if (i >= m_cars.size() || node < 0 || node > 2) return false;
    const VehicleInstance& c = m_cars[i];
    auto it = m_models.find(c.model);
    if (it == m_models.end() || !it->second.nodes[node].found) return false;
    float cs[16];
    NodeCarSpace(it->second, c.model, node, c.doomV, c.doomH, cs);
    const float co = std::cos(c.heading), sn = std::sin(c.heading);
    const float body[16] = {co, sn, 0, 0, -sn, co, 0, 0, 0, 0, 1, 0, c.x, c.y, c.z, 1};
    Mul(cs, body, out);
    return true;
}

bool Vehicles::NodeWorldPos(size_t i, int node, float out[3]) const {
    float m[16];
    if (!NodeMatrix(i, node, m)) return false;
    out[0] = m[12]; out[1] = m[13]; out[2] = m[14];
    return true;
}

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
            {
                static const char* const kClasses[] = {"normal", "poorfamily", "richfamily", "executive", "worker", "big", "taxi", "moped",
                                                       "motorbike", "leisureboat", "workerboat", "bicycle"};
                for (int ci = 0; ci < 12; ci++) if (Lower(f[7]) == kClasses[ci]) v.vclass = ci;
            }
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
            h.abs = std::atoi(t[19].c_str()) != 0;
            h.steeringLock = (float)std::atof(t[20].c_str());
            h.suspForce = (float)std::atof(t[21].c_str());
            h.suspDamping = (float)std::atof(t[22].c_str());
            h.suspHighSpdComDamp = (float)std::atof(t[23].c_str());
            h.suspUpper = (float)std::atof(t[24].c_str());
            h.suspLower = (float)std::atof(t[25].c_str());
            h.suspBias = (float)std::atof(t[26].c_str());
            h.antiDive = (float)std::atof(t[27].c_str());
            h.seatOffset = (float)std::atof(t[28].c_str());
            h.collisionDamageMult = (float)std::atof(t[29].c_str());
            h.monetaryValue = std::atoi(t[30].c_str());
            if (t.size() > 31) h.modelFlags = (uint32_t)std::strtoul(t[31].c_str(), nullptr, 16);
            if (t.size() > 32) h.handlingFlags = (uint32_t)std::strtoul(t[32].c_str(), nullptr, 16);
            h.low = (h.modelFlags & 0x4u) != 0;
            // cHandlingDataMgr::ConvertDataToGameUnits (InitFromData scales the engine by 0.4 first)
            constexpr float kAccel = 1.0f / (50.0f * 50.0f), kVelocity = 0.277778f / 50.0f;
            Transmission& tr = h.trans;
            tr.numGears = std::clamp(h.numGears, 1, 5);
            tr.driveType = h.driveType;
            tr.handlingFlags = h.handlingFlags;
            tr.engineAccel = h.engineAccel * 0.4f * kAccel;
            tr.engineInertia = h.engineInertia;
            tr.maxVelocity = h.maxVelocity * kVelocity;
            h.brakeDecelGame = h.brakeDecel * kAccel;
            h.collisionDamageGame = h.collisionDamageMult / h.mass * 2000.0f;
            const float accelLimit = tr.engineAccel / 6.0f;
            float maxV = tr.maxVelocity;
            while (maxV > 0.0f) {   // the top speed where drag eats the whole engine
                maxV -= 0.01f;
                if (h.dragMult >= 0.01f) {
                    if (maxV * maxV * (h.dragMult / 2.0f) / 1000.0f <= accelLimit) break;
                } else if ((1.0f / (maxV * maxV * h.dragMult + 1.0f) - 1.0f) * -maxV <= accelLimit) break;
            }
            if (h.name == "rcbandit") { tr.maxFlatVelocity = maxV; tr.maxReverseVelocity = -maxV; }
            else if (h.handlingFlags & 0x1000000u) {   // USE_MAXSP_LIMIT
                tr.maxFlatVelocity = maxV / 1.2f;
                tr.maxReverseVelocity = std::min(-tr.maxFlatVelocity / 4.0f, -0.2f);
            } else {
                tr.maxVelocity = maxV * 1.2f;
                tr.maxFlatVelocity = maxV;
                tr.maxReverseVelocity = std::min(-maxV * 0.3f, -0.2f);
            }
            tr.engineAccel /= h.driveType == '4' ? 4.0f : 2.0f;
            tr.InitGearRatios();
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
    {
        static const char* kNode[3] = {"misc_a", "misc_b", "misc_c"};
        for (size_t i = 0; i < w.frames.size(); i++)
            for (int k = 0; k < 3; k++) {
                if (w.frames[i].name != kNode[k]) continue;
                VehicleModel::Node& nd = vm.nodes[k];
                nd.found = true;
                FrameMatrix(w.frames[i].m, nd.local);
                const int p = w.frames[i].parent;
                for (int j = 0; j < 3; j++) if (p >= 0 && (size_t)p < w.frames.size() && w.frames[(size_t)p].name == kNode[j]) nd.parent = j;
                if (nd.parent < 0 && p >= 0 && (size_t)p < glob.size()) std::memcpy(nd.base, glob[(size_t)p].data(), sizeof(nd.base));
                else { std::memset(nd.base, 0, sizeof(nd.base)); nd.base[0] = nd.base[5] = nd.base[10] = nd.base[15] = 1.0f; }
            }
    }
    // eCarNodes CAR_DOOR_LF..RR: the hinge dummies; the door_*_ok / door_*_dam meshes hang under them.
    static const char* kDoor[4] = {"door_lf_dummy", "door_rf_dummy", "door_lr_dummy", "door_rr_dummy"};
    for (size_t i = 0; i < w.frames.size(); i++)
        for (int k = 0; k < 4; k++) {
            if (w.frames[i].name != kDoor[k]) continue;
            VehicleModel::Door& d = vm.doors[k];
            d.found = true;
            d.frame = (int)i;
            for (int c = 0; c < 3; c++) d.hinge[c] = w.frames[i].m[9 + c];
            d.openAngle = (k % 2 == 0 ? -0.5f : 0.5f) * 3.14159265f;
            const int p = w.frames[i].parent;
            if (p >= 0 && (size_t)p < i) std::memcpy(d.base, glob[(size_t)p].data(), sizeof(d.base));
            else { std::memset(d.base, 0, sizeof(d.base)); d.base[0] = d.base[5] = d.base[10] = d.base[15] = 1.0f; }
        }
    for (size_t i = 0; i < w.frames.size(); i++)
        if (w.frames[i].name == "ped_frontseat") {   // the dummy sits on the passenger side; the driver mirrors it
            vm.seatFound = true;
            vm.seatPos[0] = -std::fabs(glob[i][12]); vm.seatPos[1] = glob[i][13]; vm.seatPos[2] = glob[i][14];
        }
    ParseVehicleCol(dffData, 0, dffData.size(), vm);
    // GetSizeOfWheel (eCarWheel order: front left, rear left, front right, rear right): the vehicles.ide
    // wheel scale is the tyre diameter.
    for (int k = 0; k < 4; k++) vm.wheelSize[k] = (k == 0 || k == 2) ? info->wheelScaleFront : info->wheelScaleRear;
    std::fprintf(stderr, "[VEH] %s col: %zu spheres %zu boxes\n", info->dff.c_str(), vm.colSpheres.size(), vm.colBoxes.size());
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
        bool damagedMesh = false;
        const int comp = DamageComp(name, &damagedMesh);
        if (!wheel && comp < 0 && HiddenPart(name)) continue;
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
        p.comp = comp; p.damagedMesh = damagedMesh;
        {
            const float oy = fr >= 0 && (size_t)fr < glob.size() ? glob[(size_t)fr][13] : 0.0f, oz = fr >= 0 && (size_t)fr < glob.size() ? glob[(size_t)fr][14] : 0.0f;
            const float ox = fr >= 0 && (size_t)fr < glob.size() ? glob[(size_t)fr][12] : 0.0f;
            for (const DffVertex& v : dm.vertices) {
                vm.bbHalfX = std::max(vm.bbHalfX, std::fabs(v.x + ox));
                vm.bbMinY = std::min(vm.bbMinY, v.y + oy); vm.bbMaxY = std::max(vm.bbMaxY, v.y + oy); vm.bbMinZ = std::min(vm.bbMinZ, v.z + oz);
            }
        }
        if (fr >= 0 && (size_t)fr < glob.size()) std::memcpy(p.m, glob[(size_t)fr].data(), sizeof(p.m));
        else { std::memset(p.m, 0, sizeof(p.m)); p.m[0] = p.m[5] = p.m[10] = p.m[15] = 1.0f; }
        for (int k = 0; k < 3; k++) if (vm.nodes[k].found && name == (k == 0 ? "misc_a" : k == 1 ? "misc_b" : "misc_c")) p.node = k;
        for (int k = 0; k < 4 && fr >= 0; k++) {   // under a door hinge: keep the part's frame relative to it
            if (!vm.doors[k].found) continue;
            float rel[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            int f = fr;
            for (int guard = 0; f >= 0 && f != vm.doors[k].frame && (size_t)f < w.frames.size() && guard < 64; guard++) {
                float local[16];
                FrameMatrix(w.frames[(size_t)f].m, local);
                Mul(rel, local, rel);
                f = w.frames[(size_t)f].parent;
            }
            if (f == vm.doors[k].frame && f != fr) { p.door = k; std::memcpy(p.doorLocal, rel, sizeof(rel)); }
        }
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

bool Vehicles::ModelBounds(int id, float* minY, float* maxY, float* minZ) const {
    auto it = m_models.find(id);
    if (it == m_models.end()) return false;
    *minY = it->second.bbMinY; *maxY = it->second.bbMaxY; *minZ = it->second.bbMinZ;
    return true;
}

int Vehicles::Spawn(D3DRenderer& r, int id, float x, float y, float z, float heading) {
    const VehicleModel* spawnModel = Model(r, id);
    if (!spawnModel) return -1;
    VehicleInstance c;
    c.model = id; c.x = x; c.y = y; c.z = z + spawnModel->rideHeight;   c.heading = heading;   // z is the ground under the car
    auto cc = m_carColours.find(Info(id)->dff);
    if (cc != m_carColours.end() && !cc->second.empty()) {
        m_rng = m_rng * 1664525u + 1013904223u;
        const std::array<int, 4>& set = cc->second[(m_rng >> 8) % cc->second.size()];
        for (int k = 0; k < 4; k++) c.paint[k] = set[(size_t)k];
    }
    m_cars.push_back(c);
    return (int)m_cars.size() - 1;
}

bool Vehicles::HasDoor(size_t i, int door) const {
    if (i >= m_cars.size() || door < 0 || door > 3) return false;
    auto it = m_models.find(m_cars[i].model);
    return it != m_models.end() && it->second.doors[door].found;
}

bool Vehicles::Footprint(size_t i, float* halfX, float* minY, float* maxY) const {
    if (i >= m_cars.size()) return false;
    auto it = m_models.find(m_cars[i].model);
    if (it == m_models.end()) return false;
    *halfX = it->second.bbHalfX; *minY = it->second.bbMinY; *maxY = it->second.bbMaxY;
    return true;
}

bool Vehicles::SeatWorld(size_t i, float out[3], bool* low, bool passenger) const {
    if (i >= m_cars.size()) return false;
    const VehicleInstance& c = m_cars[i];
    auto it = m_models.find(c.model);
    if (it == m_models.end()) return false;
    const VehicleModel& vm = it->second;
    const float cs = std::cos(c.heading), sn = std::sin(c.heading);
    const float sx = passenger ? -vm.seatPos[0] : vm.seatPos[0];   // the front passenger seat mirrors the driver's
    out[0] = c.x + sx * cs - vm.seatPos[1] * sn;
    out[1] = c.y + sx * sn + vm.seatPos[1] * cs;
    out[2] = c.z + vm.seatPos[2];
    if (low) {
        const HandlingData* h = Handling(vm.info.handling);
        *low = h && h->low;
    }
    return true;
}

void Vehicles::Draw(D3DRenderer& r) {
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    // The paint is the vertex colour: let it drive the ambient term too, or the bright ambient washes it out.
    dev->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_COLOR1);
    for (const VehicleInstance& c : m_cars) {
        if (c.removed) continue;
        auto it = m_models.find(c.model);
        if (it == m_models.end()) continue;
        const VehicleModel& vm = it->second;
        // Heading 0 faces +y like the ped; the model's nose is +y too.
        const float cs = std::cos(c.heading), sn = std::sin(c.heading);
        float body[16] = {cs, sn, 0, 0, -sn, cs, 0, 0, 0, 0, 1, 0, c.x, c.y, c.z, 1};
        if (c.hasRot || c.phys.active) {   // CMatrix rows: right, forward, up
            const float* q = c.rot;
            const float m[16] = {q[0], q[1], q[2], 0, q[3], q[4], q[5], 0, q[6], q[7], q[8], 0, c.x, c.y, c.z, 1};
            std::memcpy(body, m, sizeof(m));
        }
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
            if (p.comp >= 0) {   // CAutomobile::SetComponentVisibility: intact, damaged or gone
                const int st = p.comp < 6 ? c.doorStatus[p.comp] : c.panelStatus[p.comp - 6];
                const bool dam = p.comp < 6 ? (st == 2 || st == 3) : (st != 0);
                if (st == 4 || (p.comp >= 6 && st == 3) || dam != p.damagedMesh) continue;
            }
            if (p.node >= 0) {   // a turret part follows its (rotated) frame
                float nm[16];
                if (NodeMatrix((size_t)(&c - m_cars.data()), p.node, nm)) { dev->SetTransform(D3DTS_WORLD, reinterpret_cast<D3DMATRIX*>(nm)); r.DrawMesh(p.mesh); continue; }
            }
            if (p.door >= 0 && c.doorRatio[p.door] > 0.0f) {   // an open door: rotation about Z at its hinge
                const VehicleModel::Door& d = vm.doors[p.door];
                const float a = c.doorRatio[p.door] * d.openAngle, ca = std::cos(a), sa = std::sin(a);
                const float loc[16] = {ca, sa, 0, 0, -sa, ca, 0, 0, 0, 0, 1, 0, d.hinge[0], d.hinge[1], d.hinge[2], 1};
                float g[16];
                Mul(p.doorLocal, loc, g);
                Mul(g, d.base, g);
                Mul(g, body, &out._11);
                dev->SetTransform(D3DTS_WORLD, &out);
                r.DrawMesh(p.mesh);
                continue;
            }
            Mul(p.m, body, &out._11);
            dev->SetTransform(D3DTS_WORLD, &out);
            r.DrawMesh(p.mesh);
        }
        if (!vm.wheelMesh) continue;
        for (int k = 0; k < 4; k++) {
            static const int kSaWheelOf[4] = {0, 2, 1, 3};   // lf, rf, lb, rb -> eCarWheel
            if (!vm.wheelFound[k] || c.wheelStatus[kSaWheelOf[k]] == 2) continue;   // a wheel that came off
            const float side = (k % 2 == 0) ? -1.0f : 1.0f;   // left wheels are the mirrored model
            const float spin = c.wheelSpin, sc = std::cos(spin), ss = std::sin(spin);
            // Wheel model: x across, spin about x.
            float wm[16] = {side, 0, 0, 0, 0, sc, ss, 0, 0, -ss, sc, 0, 0, 0, 0, 1};
            if (k < 2) {   // front wheels turn about the vertical axis after they spin
                const float a = -c.steer, ca = std::cos(a), sa = std::sin(a);
                const float rz[16] = {ca, sa, 0, 0, -sa, ca, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
                Mul(wm, rz, wm);
            }
            static const int kSaWheel[4] = {0, 2, 1, 3};   // lf, rf, lb, rb -> eCarWheel
            wm[12] = vm.wheelPos[k][0]; wm[13] = vm.wheelPos[k][1];
            wm[14] = c.phys.active && vm.suspReady ? c.phys.wheelZ[kSaWheel[k]] : vm.wheelPos[k][2];   // on its spring
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
        if (m_cars[i].removed) continue;
        const float dx = m_cars[i].x - x, dy = m_cars[i].y - y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = (int)i; }
    }
    return best;
}

void Vehicles::Step(size_t i, float dt, float throttle, float brake, float steerIn, bool handbrake,
                    const std::function<float(float, float, float)>& groundAt,
                    const std::function<bool(float, float, float, float, float)>& blocked) {
    if (i >= m_cars.size()) return;
    if (m_cars[i].hasRot) return;   // a recorded playback owns the car
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
        if (c.removed) continue;
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
