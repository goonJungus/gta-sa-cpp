// CJ's firearms: weapon.dat rows, models in the right hand, and the aim /
// fire / reload animations (CTaskSimpleUseGun, animation part).
// See PlayerPed.h.

#define NOMINMAX
#include "PlayerPed.h"

#include "DffLoader.h"
#include "ImgLoader.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

// The clip names of each weapon's anim group (AnimAssocAnimations): fire,
// crouch fire, reload, crouch reload. Missing ones are null.
struct GunSpec {
    const char* name;       // weapon.dat name
    const char* label;
    const char* fire;
    const char* crouchFire;
    const char* reload;
    const char* crouchReload;
    bool continuous;        // keeps firing while the button is held
    const char* firePoor = nullptr;   // the fire clip of the poor skill group (weapon.dat 'bad' groups)
    bool melee = false;     // a melee weapon: weapon.dat MELEE row, swung through data\melee.dat
    bool thrown = false;    // a PROJECTILE row thrown by CTaskSimpleThrowProjectile (or the USE detonator)
    int type = 0;           // eWeaponType, where the game logic needs it
};

namespace {

constexpr float kFrame = 1.0f / 30.0f;   // weapon.dat anim frames are 1/30 s
constexpr int kBoneRHand = 24;           // BONE_R_HAND

const GunSpec kSpecs[] = {
    {"PISTOL", "Pistol", "colt45_fire", "colt45_crouchfire", "colt45_reload", "colt45_crouchreload", false},
    {"PISTOL_SILENCED", "Silenced pistol", "silence_fire", "silencecrouchfire", "silence_reload", "crouchreload", false},
    {"DESERT_EAGLE", "Desert Eagle", "python_fire", "python_crouchfire", "python_reload", "python_crouchreload", false, "python_fire_poor"},
    {"SHOTGUN", "Shotgun", "shotgun_fire", "shotgun_crouchfire", nullptr, nullptr, false, "shotgun_fire_poor"},
    {"SAWNOFF", "Sawn-off shotgun", "colt45_fire", "colt45_crouchfire", "sawnoff_reload", "colt45_crouchreload", false},
    {"SPAS12", "Combat shotgun", "buddy_fire", "buddy_crouchfire", "buddy_reload", "buddy_crouchreload", false, "buddy_fire_poor"},
    {"MICRO_UZI", "Micro SMG", "colt45_fire", "colt45_crouchfire", "colt45_reload", "colt45_crouchreload", true},
    {"TEC9", "Tec-9", "colt45_fire", "colt45_crouchfire", "colt45_reload", "colt45_crouchreload", true},
    {"MP5", "MP5", "uzi_fire", "uzi_crouchfire", "uzi_reload", "uzi_crouchreload", true, "uzi_fire_poor"},
    {"AK47", "AK-47", "rifle_fire", "rifle_crouchfire", "rifle_load", "rifle_crouchload", true, "rifle_fire_poor"},
    {"M4", "M4", "rifle_fire", "rifle_crouchfire", "rifle_load", "rifle_crouchload", true, "rifle_fire_poor"},
    {"COUNTRYRIFLE", "Country rifle", "weapon_sniper", "weapon_sniper", "rifle_load", "rifle_crouchload", false, nullptr, false, false, 33},
    {"SNIPERRIFLE", "Sniper rifle", "weapon_sniper", "weapon_sniper", "rifle_load", "rifle_crouchload", false, nullptr, false, false, 34},
    // heavy weapons: AnimAssocAnimations aRocketAnimations / aFlameAnimations / aSprayCanAnimations (one fire clip, no reload clip)
    {"RLAUNCHER", "Rocket launcher", "rocketfire", "rocketfire", nullptr, nullptr, false, nullptr, false, false, 35},
    {"RLAUNCHER_HS", "Heat-seeking RPG", "rocketfire", "rocketfire", nullptr, nullptr, false, nullptr, false, false, 36},
    // the camera: a rifle-group weapon, one shot every 1.1 s that takes a photograph instead of a bullet
    {"CAMERA", "Camera", "rifle_fire", "rifle_crouchfire", "rifle_load", "rifle_crouchload", false, nullptr, false, false, 43},
    {"FTHROWER", "Flamethrower", "flame_fire", "flame_fire", nullptr, nullptr, true, nullptr, false, false, 37},
    {"MINIGUN", "Minigun", "flame_fire", "flame_fire", nullptr, nullptr, true, nullptr, false, false, 38},
    {"SPRAYCAN", "Spray can", "spraycan_fire", "spraycan_fire", nullptr, nullptr, true, nullptr, false, false, 41},
    {"EXTINGUISHER", "Fire extinguisher", "flame_fire", "flame_fire", nullptr, nullptr, true, nullptr, false, false, 42},
};

// The rocket in flight: a model only, never selectable (last in the list).
const GunSpec kRocketSpec = {"ROCKET", "Rocket", nullptr, nullptr, nullptr, nullptr, false, nullptr, false, false, 19};

// weapon.dat MELEE rows, in slot order: model, base combo and fight data come from the file.
const GunSpec kMeleeSpecs[] = {
    {"BRASSKNUCKLE", "Brass knuckles", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"GOLFCLUB", "Golf club", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"NIGHTSTICK", "Nightstick", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"KNIFE", "Knife", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"BASEBALLBAT", "Baseball bat", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"SHOVEL", "Shovel", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"POOLCUE", "Pool cue", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"KATANA", "Katana", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"CHAINSAW", "Chainsaw", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"DILDO1", "Purple dildo", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"DILDO2", "Dildo", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"VIBE1", "Vibrator", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"VIBE2", "Silver vibrator", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"FLOWERS", "Flowers", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"CANE", "Cane", nullptr, nullptr, nullptr, nullptr, false, nullptr, true},
    {"PARACHUTE", "Parachute", nullptr, nullptr, nullptr, nullptr, false, nullptr, true, false, 46},
};

// Thrown weapons and the detonator (weapon.dat PROJECTILE / USE rows). The detonator needs no clips.
const GunSpec kThrownSpecs[] = {
    {"GRENADE", "Grenade", nullptr, nullptr, nullptr, nullptr, false, nullptr, false, true, 16},
    {"TEARGAS", "Tear gas", nullptr, nullptr, nullptr, nullptr, false, nullptr, false, true, 17},
    {"MOLOTOV", "Molotov cocktail", nullptr, nullptr, nullptr, nullptr, false, nullptr, false, true, 18},
    {"SATCHEL_CHARGE", "Satchel charge", nullptr, nullptr, nullptr, nullptr, false, nullptr, false, true, 39},
    {"DETONATOR", "Detonator", nullptr, nullptr, nullptr, nullptr, false, nullptr, false, true, 40},
    // USE rows of the goggles group: CTaskComplexUseGoggles puts them on the head model
    {"NIGHTVISION", "Night vision goggles", nullptr, nullptr, nullptr, nullptr, false, nullptr, false, true, 44},
    {"INFRARED", "Thermal goggles", nullptr, nullptr, nullptr, nullptr, false, nullptr, false, true, 45},
};

std::string LowerStr(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<std::string> Tokens(const std::string& line) {
    std::vector<std::string> t;
    std::istringstream in(line);
    for (std::string s; in >> s;) t.push_back(s);
    return t;
}


// The frame matrix of every geometry in a weapon clump, from the atomic -> frame links and the
// frame hierarchy. A gun's flash and its second barrel sit on their own frames, so each
// geometry is moved by its frame before it is drawn. xf[g] = 3 x 3 rotation rows then position.
struct GeomXf { float m[12]; bool valid = false; };
std::vector<GeomXf> GeometryFrames(const std::vector<uint8_t>& b) {
    std::vector<GeomXf> out;
    struct Frame { float m[12]; int parent; };
    std::vector<Frame> frames;
    std::vector<std::pair<int, int>> atomics;   // frame, geometry
    auto rd32 = [&](size_t o) { uint32_t v = 0; if (o + 4 <= b.size()) std::memcpy(&v, &b[o], 4); return v; };
    // chunk walk: header = type, size, version
    struct Walker {
        const std::vector<uint8_t>& b;
        std::vector<Frame>& frames;
        std::vector<std::pair<int, int>>& atomics;
        uint32_t Rd(size_t o) const { uint32_t v = 0; if (o + 4 <= b.size()) std::memcpy(&v, &b[o], 4); return v; }
        void Walk(size_t o, size_t end) {
            while (o + 12 <= end) {
                const uint32_t t = Rd(o), sz = Rd(o + 4);
                const size_t p = o + 12;
                if (p + sz > end) return;
                if (t == 0x10) Walk(p, p + sz);                                  // clump
                else if (t == 0x0E) {                                            // frame list
                    size_t q = p;
                    while (q + 12 <= p + sz) {
                        const uint32_t t2 = Rd(q), s2 = Rd(q + 4);
                        if (t2 == 1) {
                            const uint32_t n = Rd(q + 12);
                            for (uint32_t i = 0; i < n && q + 16 + (i + 1) * 56 <= p + sz; i++) {
                                Frame f;
                                std::memcpy(f.m, &b[q + 16 + i * 56], 48);
                                f.parent = (int)Rd(q + 16 + i * 56 + 48);
                                frames.push_back(f);
                            }
                        }
                        q += 12 + s2;
                    }
                } else if (t == 0x14) {                                          // atomic
                    if (Rd(p) == 1 && Rd(p + 4) >= 12) atomics.push_back({(int)Rd(p + 12), (int)Rd(p + 16)});
                }
                o = p + sz;
            }
        }
    } w{b, frames, atomics};
    w.Walk(0, b.size());
    (void)rd32;
    int maxGeom = -1;
    for (auto& a : atomics) maxGeom = std::max(maxGeom, a.second);
    out.resize((size_t)(maxGeom + 1));
    for (auto& a : atomics) {
        if (a.first < 0 || a.first >= (int)frames.size() || a.second < 0) continue;
        // compose up the parents: child * parent (row vectors)
        float m[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        int f = a.first, guard = 0;
        std::memcpy(m, frames[f].m, sizeof(m));
        f = frames[f].parent;
        while (f >= 0 && f < (int)frames.size() && guard++ < 16) {
            const float* r = frames[f].m;
            float n[12];
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++) n[i * 3 + j] = m[i * 3 + 0] * r[0 * 3 + j] + m[i * 3 + 1] * r[1 * 3 + j] + m[i * 3 + 2] * r[2 * 3 + j];
            for (int j = 0; j < 3; j++) n[9 + j] = m[9] * r[j] + m[10] * r[3 + j] + m[11] * r[6 + j] + r[9 + j];
            std::memcpy(m, n, sizeof(m));
            f = frames[f].parent;
        }
        std::memcpy(out[(size_t)a.second].m, m, sizeof(m));
        out[(size_t)a.second].valid = true;
    }
    return out;
}

}  // namespace

// weapon.dat rows (the standard skill row), default.ide models, then the
// models and textures out of gta3.img.
void PlayerPed::LoadGuns(D3DRenderer& r, const std::string& gameDir) {
    std::ifstream wf(gameDir + "\\data\\weapon.dat");
    std::ifstream idef(gameDir + "\\data\\default.ide");
    if (!wf) { std::fprintf(stderr, "[GUN] data\\weapon.dat not found\n"); return; }
    struct Row { std::vector<std::string> t; };
    std::vector<std::vector<std::string>> rows;
    std::vector<std::vector<std::string>> meleeRows;   // name MELEE ranges model -1 slot baseCombo numCombos flags stealth
    for (std::string line; std::getline(wf, line);) {
        if (line.empty()) continue;
        if ((unsigned char)line[0] == 0xA3) {   // the melee row marker (pound sign, ANSI)
            std::vector<std::string> t = Tokens(line.substr(1));
            if (t.size() >= 11 && t[1] == "MELEE") meleeRows.push_back(t);
            continue;
        }
        if (line[0] != '$') continue;
        std::vector<std::string> t = Tokens(line);
        if (t.size() >= 26) rows.push_back(t);
    }
    struct Ide { std::string dff, txd; };
    std::unordered_map<int, Ide> ide;
    for (std::string line; std::getline(idef, line);) {
        for (char& c : line) if (c == ',') c = ' ';
        std::vector<std::string> t = Tokens(line);
        if (t.size() >= 3 && std::isdigit((unsigned char)t[0][0])) ide[std::atoi(t[0].c_str())] = {LowerStr(t[1]), LowerStr(t[2])};
    }
    ImgLoader gta3;
    const bool haveImg = gta3.Open(gameDir + "\\models\\gta3.img");

    auto loadModel = [&](Gun& g, int model) {
        auto it = ide.find(model);
        if (it != ide.end() && haveImg) {
            const std::vector<uint8_t> dffData = gta3.Extract(it->second.dff + ".dff");
            const std::vector<uint8_t> txdData = gta3.Extract(it->second.txd + ".txd");
            std::vector<TxdTexture> txd;
            if (!txdData.empty()) txd = TxdLoader::LoadFromMemory(txdData.data(), txdData.size());
            for (const TxdTexture& t : txd) {   // CHud::DrawWeaponIcon: <model>icon
                if (TxdLoader::KeyOf(t.name) != it->second.dff + "icon") continue;
                g.icon = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(), &t.extraMips);
                break;
            }
            DffModel m = dffData.empty() ? DffModel() : DffLoader::LoadFromMemory(dffData.data(), dffData.size());
            const std::vector<GeomXf> gx = dffData.empty() ? std::vector<GeomXf>() : GeometryFrames(dffData);
            float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
            double fsum[3] = {0, 0, 0}; size_t fcount = 0;   // muzzle flash vertices: they sit at the muzzle
            for (DffMesh& dm : m.meshes) {
                const size_t gi = (size_t)dm.geometry;
                if (dm.vertices.empty() || dm.indices.empty()) continue;
                if (gi < gx.size() && gx[gi].valid) {   // move the geometry onto its frame
                    const float* f = gx[gi].m;
                    for (DffVertex& v : dm.vertices) {
                        const float x = v.x, y = v.y, z = v.z, nx = v.nx, ny = v.ny, nz = v.nz;
                        v.x = x * f[0] + y * f[3] + z * f[6] + f[9];
                        v.y = x * f[1] + y * f[4] + z * f[7] + f[10];
                        v.z = x * f[2] + y * f[5] + z * f[8] + f[11];
                        v.nx = nx * f[0] + ny * f[3] + nz * f[6];
                        v.ny = nx * f[1] + ny * f[4] + nz * f[7];
                        v.nz = nx * f[2] + ny * f[5] + nz * f[8];
                    }
                }
                if (TxdLoader::KeyOf(dm.textureName) != TxdLoader::KeyOf("muzzle_texture4"))
                    for (const DffVertex& v : dm.vertices) {
                        const float c[3] = {v.x, v.y, v.z};
                        for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], c[k]); hi[k] = std::max(hi[k], c[k]); }
                    }
                D3DRenderMesh* mesh = r.CreateMesh(reinterpret_cast<const MeshVertex*>(dm.vertices.data()),
                                                   (uint32_t)dm.vertices.size(), dm.indices.data(),
                                                   (uint32_t)dm.indices.size());
                if (!mesh) continue;
                for (const TxdTexture& t : txd) {
                    if (TxdLoader::KeyOf(t.name) != TxdLoader::KeyOf(dm.textureName)) continue;
                    IDirect3DTexture9* tex = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(),
                                                             (uint32_t)t.mip0.size(), &t.extraMips);
                    mesh->texture = tex;
                    g.textures.push_back(tex);
                    break;
                }
                const bool flash = TxdLoader::KeyOf(dm.textureName) == TxdLoader::KeyOf("muzzle_texture4");
                if (flash) for (const DffVertex& v : dm.vertices) { fsum[0] += v.x; fsum[1] += v.y; fsum[2] += v.z; fcount++; }
                (flash ? g.muzzle : g.meshes).push_back(mesh);
            }
            if (hi[0] >= lo[0]) {   // barrel: the model's long axis, pointing away from the grip
                int ax = 0;
                for (int k = 1; k < 3; k++) if (hi[k] - lo[k] > hi[ax] - lo[ax]) ax = k;
                g.barrel[0] = g.barrel[1] = g.barrel[2] = 0.0f;
                g.barrel[ax] = (hi[ax] + lo[ax]) < 0.0f ? -1.0f : 1.0f;
                if (fcount) {   // the flash sits in front of the muzzle: body centre -> flash picks the barrel axis and side
                    float d[3];
                    for (int k = 0; k < 3; k++) d[k] = (float)(fsum[k] / (double)fcount) - 0.5f * (hi[k] + lo[k]);
                    int a2 = 0;
                    for (int k = 1; k < 3; k++) if (std::fabs(d[k]) > std::fabs(d[a2])) a2 = k;
                    g.barrel[0] = g.barrel[1] = g.barrel[2] = 0.0f;
                    g.barrel[a2] = d[a2] < 0.0f ? -1.0f : 1.0f;
                }
                std::fprintf(stderr, "[GUN] %s barrel %.0f %.0f %.0f (flash %s)\n", g.name.c_str(), g.barrel[0], g.barrel[1], g.barrel[2], fcount ? "yes" : "no");
            }
        }
    };

    for (const GunSpec& sp : kSpecs) {
        const std::vector<std::string>* row = nullptr;
        for (const auto& t : rows) {
            if (t[1] != sp.name) continue;
            if (!row || t[14] == std::to_string(s_weaponSkill)) row = &t;   // the player's skill row (0 poor, 1 std), else the first
        }
        if (!row) continue;
        Gun g;
        g.spec = &sp;
        g.thrown = sp.thrown; g.type = sp.type; g.detonator = std::strcmp(sp.name, "DETONATOR") == 0;
        g.fireClip = s_weaponSkill == 0 && sp.firePoor ? sp.firePoor : sp.fire;
        g.name = sp.label;
        g.clipSize = std::atoi((*row)[9].c_str());
        g.loopStart = (float)std::atof((*row)[18].c_str()) * kFrame;
        g.loopEnd = (float)std::atof((*row)[19].c_str()) * kFrame;
        g.fireTime = (float)std::atof((*row)[20].c_str()) * kFrame;
        g.loop2Start = (float)std::atof((*row)[21].c_str()) * kFrame;
        g.loop2End = (float)std::atof((*row)[22].c_str()) * kFrame;
        g.fire2Time = (float)std::atof((*row)[23].c_str()) * kFrame;
        g.moveSpeed = (float)std::atof((*row)[17].c_str());
        g.damage = (float)std::atof((*row)[10].c_str());
        g.range = (float)std::atof((*row)[4].c_str());
        g.accuracy = (float)std::atof((*row)[16].c_str());
        g.targetRange = (float)std::atof((*row)[3].c_str());
        // flags are hex, the first digit (CANAIM 1, AIMWITHARM 2, 1STPERSON 4, ONLYFREEAIM 8) is the last character
        g.flags = (unsigned)std::strtoul((*row)[25].c_str(), nullptr, 16);
        g.aimWithArm = (std::strtoul((*row)[25].c_str(), nullptr, 16) & 0x2) != 0;
        g.twin = (std::strtoul((*row)[25].c_str(), nullptr, 16) & 0x800) != 0;
        g.reloadTime = 1.0f;
        g.fireOffset[0] = (float)std::atof((*row)[11].c_str());
        g.fireOffset[1] = (float)std::atof((*row)[12].c_str());
        g.fireOffset[2] = (float)std::atof((*row)[13].c_str());
        loadModel(g, std::atoi((*row)[5].c_str()));
        g.ammo = g.clipSize;
        g.total = g.clipSize * 6;
        m_guns.push_back(std::move(g));
    }
    // melee weapons follow the firearms (the default selection stays the first gun)
    for (const GunSpec& sp : kMeleeSpecs) {
        const std::vector<std::string>* row = nullptr;
        for (const auto& t : meleeRows) if (t[0] == sp.name) row = &t;
        if (!row) continue;
        Gun g;
        g.spec = &sp;
        g.thrown = sp.thrown; g.type = sp.type; g.detonator = std::strcmp(sp.name, "DETONATOR") == 0;
        g.name = sp.label;
        g.comboName = (*row)[7];
        g.range = (float)std::atof((*row)[3].c_str());
        g.flags = 0;
        g.clipSize = g.ammo = g.total = 0;
        if (std::atoi((*row)[8].c_str()) > 1) g.comboName += "#";   // numbered by the fight style
        loadModel(g, std::atoi((*row)[4].c_str()));
        m_guns.push_back(std::move(g));
    }
    // thrown weapons: the clip is one grenade, the stock is the count
    for (const GunSpec& sp : kThrownSpecs) {
        const std::vector<std::string>* row = nullptr;
        for (const auto& t : rows) if (t[1] == sp.name) row = &t;
        if (!row) continue;
        Gun g;
        g.spec = &sp;
        g.thrown = sp.thrown; g.type = sp.type; g.detonator = std::strcmp(sp.name, "DETONATOR") == 0;
        g.name = sp.label;
        g.clipSize = 1;
        g.loopStart = (float)std::atof((*row)[18].c_str()) * kFrame;
        g.loopEnd = (float)std::atof((*row)[19].c_str()) * kFrame;
        g.fireTime = (float)std::atof((*row)[20].c_str()) * kFrame;       // WEAPON_throw
        g.loop2Start = (float)std::atof((*row)[21].c_str()) * kFrame;
        g.loop2End = (float)std::atof((*row)[22].c_str()) * kFrame;
        g.fire2Time = (float)std::atof((*row)[23].c_str()) * kFrame;      // WEAPON_throwu
        g.damage = (float)std::atof((*row)[10].c_str());
        g.range = (float)std::atof((*row)[4].c_str());
        g.targetRange = (float)std::atof((*row)[3].c_str());
        g.flags = (unsigned)std::strtoul((*row)[25].c_str(), nullptr, 16);
        g.fireOffset[0] = (float)std::atof((*row)[11].c_str());
        g.fireOffset[1] = (float)std::atof((*row)[12].c_str());
        g.fireOffset[2] = (float)std::atof((*row)[13].c_str());
        loadModel(g, std::atoi((*row)[5].c_str()));
        g.ammo = 0;
        g.total = std::strcmp(sp.name, "DETONATOR") == 0 ? 0 : sp.type >= 44 ? 1 : 5;
        m_guns.push_back(std::move(g));
    }
    {   // the rocket model (weapon.dat ROCKET row), kept last and hidden from the weapon cycle
        for (const auto& t : rows) {
            if (t[1] != kRocketSpec.name) continue;
            Gun g;
            g.spec = &kRocketSpec;
            g.type = kRocketSpec.type;
            g.name = kRocketSpec.label;
            g.flags = 0;
            g.clipSize = g.ammo = g.total = 0;
            loadModel(g, std::atoi(t[5].c_str()));
            m_guns.push_back(std::move(g));
            break;
        }
    }
    m_meleeOk = Melee::Load(gameDir, m_meleeTable);
    for (Gun& g : m_guns)
        if (g.spec->melee) {
            std::string n = g.comboName;
            if (!n.empty() && n.back() == '#') n = n.substr(0, n.size() - 1) + "_1";
            g.combo = m_meleeTable.Find(n);
        }
    std::fprintf(stderr, "[GUN] %zu weapons, %zu combos\n", m_guns.size(), m_meleeTable.combos.size());
}

void PlayerPed::ShutdownGuns(D3DRenderer& r) {
    for (Gun& g : m_guns) {
        for (D3DRenderMesh* m : g.meshes) r.DestroyMesh(m);
        for (D3DRenderMesh* m : g.muzzle) r.DestroyMesh(m);
        for (IDirect3DTexture9* t : g.textures) if (t) r.DestroyTexture(t);
        if (g.icon) r.DestroyTexture(g.icon);
    }
    m_guns.clear();
}

// Select a weapon (-1 = fists); resets the aim state.
void PlayerPed::SelectGun(int index) {
    m_gun = m_melee = -1;
    if (index >= 0 && index < (int)m_guns.size()) ((m_guns[index].spec->melee || m_guns[index].spec->thrown) ? m_melee : m_gun) = index;
    FightEnd(true);
    m_throw = Throw();
    m_gogglesClip = nullptr;
    m_reloading = m_firing = false;
    m_attackCounter = 0.0f;   // CPlayerPed::MakeChangesForNewWeapon
    for (Assoc& a : m_assocs) if (a.partial) { a.dead = true; a.blend = 0.0f; }
}

void PlayerPed::NextGun() {
    const int cur = WeaponIndex();
    SelectGun(cur + 1 >= GunCount() ? -1 : cur + 1);
}

void PlayerPed::PrevGun() {
    const int cur = WeaponIndex();
    SelectGun(cur < 0 ? GunCount() - 1 : cur - 1);
}

void PlayerPed::RefillAmmo() {
    for (Gun& g : m_guns) {
        if (g.spec->thrown) { g.total = g.detonator ? g.total : g.type >= 44 ? 1 : 5; continue; }
        g.ammo = g.clipSize; g.total = g.clipSize * 60;
    }
}

int PlayerPed::GunCount() const {
    int n = (int)m_guns.size();
    while (n > 0 && m_guns[n - 1].type == 19) n--;   // the rocket model is not a weapon
    return n;
}
const char* PlayerPed::GunNameAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].name.c_str() : "Fists"; }
IDirect3DTexture9* PlayerPed::GunIconAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].icon : nullptr; }
int PlayerPed::GunClipAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].ammo : 0; }
int PlayerPed::GunTotalAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].total : 0; }
int PlayerPed::GunClipSizeAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].clipSize : 0; }
const char* PlayerPed::GunName() const { return WeaponIndex() >= 0 ? m_guns[WeaponIndex()].name.c_str() : "Fists"; }
// CPlayerPed::GetWeaponRadiusOnScreen @ 00609cd0: the crosshair circle size.
float PlayerPed::WeaponRadiusOnScreen() const {
    if (m_gun < 0) return 0.0f;
    const Gun& g = m_guns[m_gun];
    const float acc = 0.5f / std::max(0.01f, g.accuracy);
    const std::string id = GunId();
    if (id == "SHOTGUN" || id == "SPAS12" || id == "SAWNOFF") return std::max(0.2f, acc);
    const float radius = (m_attackCounter * 0.5f + 1.0f) * std::min(1.0f, 15.0f / std::max(0.01f, g.range)) * acc;
    return std::max(0.2f, m_ducking ? radius * 0.5f : radius);
}

const char* PlayerPed::GunId() const { return WeaponIndex() >= 0 && m_guns[WeaponIndex()].spec ? m_guns[WeaponIndex()].spec->name : ""; }
int PlayerPed::GunAmmo() const { return m_gun >= 0 ? m_guns[m_gun].ammo : 0; }
int PlayerPed::GunClipSize() const { return m_gun >= 0 ? m_guns[m_gun].clipSize : HoldingThrown() ? 1 : 0; }
int PlayerPed::GunTotalAmmo() const { return m_gun >= 0 ? m_guns[m_gun].total : HoldingThrown() ? m_guns[m_melee].total : 0; }
int PlayerPed::RocketModelIndex() const {
    for (size_t i = 0; i < m_guns.size(); i++) if (m_guns[i].type == 19) return (int)i;
    return -1;
}
int PlayerPed::GunType(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].type : 0; }
IDirect3DTexture9* PlayerPed::GunIcon() const { return WeaponIndex() >= 0 ? m_guns[WeaponIndex()].icon : nullptr; }

// Armor soaks the hit first, like CPed::InflictDamage.
void PlayerPed::Hurt(float amount) {
    if (amount <= 0.0f || m_health <= 0.0f) return;
    const float soak = std::min(m_armor, amount);
    m_armor -= soak;
    m_health = std::max(0.0f, m_health - (amount - soak));
    m_damageFlash = 1.0f;
}

// The partial fire and reload clips while aiming (the animation half of
// CTaskSimpleUseGun): the fire clip holds at its loop start, plays through
// to the loop end on a shot; a reload crossfades from the held aim pose into
// the reload clip and back. blendSeconds = 0: the retail snap.
void PlayerPed::UpdateGun(const Input& in) {
    if (m_gun < 0) return;
    Gun& g = m_guns[m_gun];
    const GunSpec& sp = *g.spec;
    m_flash = std::max(0.0f, m_flash - in.dt);
    m_flashL = std::max(0.0f, m_flashL - in.dt);
    m_attackCounter *= std::pow(0.96f, in.dt * 50.0f);   // CTaskSimpleUseGun::ProcessPed
    const bool crouch = m_ducking;
    const float loopStart = crouch ? g.loop2Start : g.loopStart, loopEnd = crouch ? g.loop2End : g.loopEnd,
                fireTime = crouch ? g.fire2Time : g.fireTime;
    // the pro (twin) groups crouch-fire with 2guns_crouchfire (aColt45proAnimations / aSawnoffproAnimations)
    const char* crouchFire = g.twin && Clip("2guns_crouchfire") ? "2guns_crouchfire" : sp.crouchFire;
    const AnimClip* fireClip = Clip(crouch && crouchFire && Clip(crouchFire) ? crouchFire : g.fireClip);
    const AnimClip* relClip = Clip(crouch && sp.crouchReload && Clip(sp.crouchReload) ? sp.crouchReload
                                   : sp.reload ? sp.reload : "");
    if (!fireClip) return;
    // a crouch change swaps the clip pair
    if (fireClip != m_gunFireClip) {
        if (m_gunFireClip) if (Assoc* o = Get(m_gunFireClip)) { o->dead = true; o->blend = 0.0f; }
        if (m_gunRelClip) if (Assoc* o = Get(m_gunRelClip)) { o->dead = true; o->blend = 0.0f; }
        m_gunFireClip = fireClip;
        m_gunRelClip = relClip;
        m_firing = false;
    }
    Assoc* fa = Get(fireClip);
    if (!fa) {
        fa = Add(fireClip, false, false);
        fa->partial = true;
        fa->blend = 0.0f;
        fa->time = loopStart;
        fa->playing = false;
    }
    Assoc* ra = relClip ? Get(relClip) : nullptr;

    // blend step per frame
    const float step = in.rewrite ? std::min(1.0f, in.dt / 0.16f) : 1.0f;   // AimReloadBlendSeconds

    // start a reload: R, or the empty gun asked to fire
    if (!m_reloading && g.ammo < g.clipSize && g.total > 0 && (in.reloadJust || (g.ammo == 0 && (in.fireJust || in.autoReload)))) {
        m_reloadEvents++;
        m_reloading = true;
        m_firing = false;
        m_reloadTime = 0.0f;
        if (relClip) {
            if (!ra) { ra = Add(relClip, false, false); ra->partial = true; ra->blend = 0.0f; }
            ra->time = 0.0f;
            ra->finished = false;
            ra->playing = true;
        }
        std::fprintf(stderr, "[GUN] reload %s\n", g.name.c_str());
    }

    if (m_reloading) {
        m_reloadTime += in.dt;
        const bool done = relClip && ra ? ra->finished : m_reloadTime >= g.reloadTime;
        const float wantReload = done ? 0.0f : 1.0f;
        fa->blend += std::clamp(1.0f - wantReload - fa->blend, -step, step);
        if (ra) ra->blend += std::clamp(wantReload - ra->blend, -step, step);
        if (done && (!ra || ra->blend <= 0.0f) && fa->blend >= 1.0f - 1e-4f) {
            m_reloading = false;
            const int take = std::min(g.clipSize - g.ammo, g.total);
            g.ammo += take;
            g.total -= take;
            if (ra) { ra->dead = true; ra->blend = 0.0f; }
        }
        if (done && ra) ra->playing = false;
        fa->delta = 0.0f;
        if (ra) ra->delta = 0.0f;
        fa->time = loopStart;
        fa->playing = false;
        return;
    }

    // aim in
    fa->delta = 0.0f;
    fa->blend += std::clamp(1.0f - fa->blend, -step, step);

    const bool want = in.fireJust || (sp.continuous && in.fireHeld);
    if (!m_firing) {
        fa->time = loopStart;
        fa->playing = false;
        if (want && g.ammo == 0 && in.fireJust && g.total == 0) m_dryEvents++;
        if (want && g.ammo > 0 && fa->blend > 0.9f) {
            m_firing = true;
            m_shotDone = false;
            m_shotLeftDone = false;
            m_fireQueued = false;
            fa->playing = true;
        }
    }
    if (in.fireJust && m_firing) m_fireQueued = true;   // a press during the shot is the next command (CTaskSimpleUseGun m_NextCmd)
    if (m_firing) {
        if (!m_shotDone && fa->time >= fireTime) {
            m_shotDone = true;
            g.ammo--;
            m_shotEvents++;
            m_attackCounter += g.damage * 0.04f;   // CWeapon::FireInstantHit
            m_lastShotLeft = false;
            if (std::strcmp(sp.name, "PISTOL_SILENCED") != 0 && g.type < 35) m_flash = 0.06f;   // the silenced pistol and the heavy weapons have no flash
            std::fprintf(stderr, "[GUN] shot %s ammo %d/%d\n", g.name.c_str(), g.ammo, g.clipSize);
        }
        // CTaskSimpleUseGun::ProcessPed: a twin pistol's left gun fires half a loop after the right
        // ((loop end - loop start) / 2 + fire time), each shot one round of the shared clip.
        if (g.twin && m_shotDone && !m_shotLeftDone && g.ammo > 0 && fa->time >= fireTime + (loopEnd - loopStart) * 0.5f) {
            m_shotLeftDone = true;
            g.ammo--;
            m_shotEvents++;
            m_attackCounter += g.damage * 0.04f;   // CWeapon::FireInstantHit
            m_lastShotLeft = true;
            m_flashL = 0.06f;
            std::fprintf(stderr, "[GUN] shot %s (left) ammo %d/%d\n", g.name.c_str(), g.ammo, g.clipSize);
        }
        if (fa->time >= loopEnd || fa->finished) {
            // retail: the clip wraps from the loop end to the loop start while the next command is fire,
            // the overshoot carries over so the rate is exactly (end - start) per shot
            const bool again = (m_fireQueued || (sp.continuous && in.fireHeld)) && g.ammo > 0;
            m_fireQueued = false;
            fa->finished = false;
            if (again) {
                fa->time = loopStart + std::max(0.0f, fa->time - loopEnd);
                m_shotDone = false;
                m_shotLeftDone = false;
                if (fa->time >= fireTime) {
                    m_shotDone = true;
                    g.ammo--;
                    m_shotEvents++;
                    m_attackCounter += g.damage * 0.04f;
                    m_lastShotLeft = false;
                    if (std::strcmp(sp.name, "PISTOL_SILENCED") != 0 && g.type < 35) m_flash = 0.06f;
                    std::fprintf(stderr, "[GUN] shot %s ammo %d/%d\n", g.name.c_str(), g.ammo, g.clipSize);
                }
            } else {
                m_firing = false;
                fa->time = loopStart;
                fa->playing = false;
            }
        }
    }
}

// The weapon model sits on the right-hand bone matrix (RenderWeaponPedsForPC).
bool PlayerPed::MuzzleWorld(float x, float y, float feetZ, float out[3]) const {
    if (m_gun < 0) return false;
    const int f = m_frameOfBone[kBoneRHand];
    if (f < 0 || f >= (int)m_globals.size()) return false;
    const float* o = m_guns[m_gun].fireOffset;
    const RwMat h = m_guns[m_gun].twin && m_lastShotLeft ? LeftGunMatrix() : m_globals[f];   // the gun that fired last
    float p[3];
    for (int j = 0; j < 3; j++) p[j] = o[0] * h.m[0][j] + o[1] * h.m[1][j] + o[2] * h.m[2][j] + h.m[3][j];
    const float a = m_heading - 1.57079633f, c = std::cos(a), s = std::sin(a);
    out[0] = x + p[0] * c - p[1] * s;
    out[1] = y + p[0] * s + p[1] * c;
    out[2] = feetZ - m_bindMinZ + p[2];
    return true;
}

void PlayerPed::DrawGun(D3DRenderer& r, float x, float y, float feetZ) {
    if (m_goggles) {   // CPed::PutOnGoggles: the goggles model rides the head bone
        const int hf = m_frameOfBone[5];
        for (size_t i = 0; i < m_guns.size() && hf >= 0 && hf < (int)m_globals.size(); i++)
            if (m_guns[i].type == m_goggles) {
                const float a = m_heading - 1.57079633f, c = std::cos(a), s = std::sin(a);
                const float w[4][4] = {{c, s, 0, 0}, {-s, c, 0, 0}, {0, 0, 1, 0}, {x, y, feetZ - m_bindMinZ, 1}};
                m_drawIdx = (int)i;
                DrawGunAt(r, m_globals[hf], w, false);
                m_drawIdx = -1;
            }
    }
    if (m_gun < 0 && m_melee < 0) return;
    if (m_gun < 0 && m_guns[m_melee].type == 46) {   // the parachute rides CJ's back, never his hand
        const int sf = m_frameOfBone[3];
        if (sf >= 0 && sf < (int)m_globals.size()) {
            const float a = m_heading - 1.57079633f, c = std::cos(a), s = std::sin(a);
            const float w[4][4] = {{c, s, 0, 0}, {-s, c, 0, 0}, {0, 0, 1, 0}, {x, y, feetZ - m_bindMinZ, 1}};
            m_drawIdx = m_melee;
            RwMat pm = m_globals[sf];
            {   // pack seated on the back: turned 90 degrees about Y, lifted 0.1 along the spine
                RwMat loc;
                loc.m[0][0] = 0.0f; loc.m[0][2] = -1.0f;
                loc.m[2][0] = 1.0f; loc.m[2][2] = 0.0f;
                loc.m[3][0] = 0.1f;
                pm = loc * pm;
            }
            DrawGunAt(r, pm, w, false);
            m_drawIdx = -1;
        }
        return;
    }
    if (m_gun < 0 && m_guns[m_melee].type >= 44) return;   // goggles are never held
    const int f = m_frameOfBone[kBoneRHand];
    if (f < 0 || f >= (int)m_globals.size()) return;
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    const float a = m_heading - 1.57079633f, c = std::cos(a), s = std::sin(a);
    const float w[4][4] = {{c, s, 0, 0}, {-s, c, 0, 0}, {0, 0, 1, 0}, {x, y, feetZ - m_bindMinZ, 1}};
    // CVisibilityPlugins::RenderWeaponPedsForPC: the right hand's matrix; a twin gun is drawn again
    // from the left hand turned 180 degrees about X then moved (0.04, -0.05, 0), both pre-concatenated.
    DrawGunAt(r, m_globals[f], w, m_flash > 0.0f);
    const int fl = m_frameOfBone[34];
    if (m_gun >= 0 && m_guns[m_gun].twin && fl >= 0 && fl < (int)m_globals.size()) {
        const RwMat lm = LeftGunMatrix();
        if (std::getenv("GTA_IKDBG")) {
            static int n = 0;
            if (++n % 30 == 0) {
                const float* b = m_guns[m_gun].barrel;
                float cr[3], cl[3];
                const RwMat& rm = m_globals[f];
                for (int k = 0; k < 3; k++) { cr[k] = b[0] * rm.m[0][k] + b[1] * rm.m[1][k] + b[2] * rm.m[2][k]; cl[k] = b[0] * lm.m[0][k] + b[1] * lm.m[1][k] + b[2] * lm.m[2][k]; }
                std::fprintf(stderr, "[TWIN] barrel %.0f %.0f %.0f R (%.2f %.2f %.2f) L (%.2f %.2f %.2f) Lpos (%.2f %.2f %.2f) Rpos (%.2f %.2f %.2f)\n", b[0], b[1], b[2], cr[0], cr[1], cr[2], cl[0], cl[1], cl[2], lm.m[3][0], lm.m[3][1], lm.m[3][2], rm.m[3][0], rm.m[3][1], rm.m[3][2]);
            }
        }
        DrawGunAt(r, lm, w, m_flashL > 0.0f);
    }
}

RwMat PlayerPed::LeftGunMatrix() const {
    RwMat local;   // translate, then rotate, in the hand's space
    local.m[1][1] = -1.0f; local.m[2][2] = -1.0f;
    local.m[3][0] = 0.04f; local.m[3][1] = 0.05f; local.m[3][2] = 0.0f;   // (0.04, -0.05, 0) * Rx(180)
    const int fl = m_frameOfBone[34];
    return fl >= 0 && fl < (int)m_globals.size() ? local * m_globals[fl] : local;
}

void PlayerPed::DrawGunAt(D3DRenderer& r, const RwMat& h, const float w[4][4], bool flash) {
    IDirect3DDevice9* dev = r.GetDevice();
    float hm[4][4];
    for (int i = 0; i < 4; i++) { for (int j = 0; j < 3; j++) hm[i][j] = h.m[i][j]; hm[i][3] = i == 3 ? 1.0f : 0.0f; }
    D3DMATRIX out;
    float* o = &out._11;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float v = 0.0f;
            for (int k = 0; k < 4; k++) v += hm[i][k] * w[k][j];
            o[i * 4 + j] = v;
        }
    dev->SetTransform(D3DTS_WORLD, &out);
    Gun& g = m_guns[m_drawIdx >= 0 ? m_drawIdx : m_gun >= 0 ? m_gun : m_melee];
    for (D3DRenderMesh* m : g.meshes) r.DrawMesh(m);
    if (flash && !g.muzzle.empty()) {
        // the flash atomic: additive, no depth write, a little bigger or smaller every shot
        DWORD ab, sb, db, zw;
        dev->GetRenderState(D3DRS_ALPHABLENDENABLE, &ab);
        dev->GetRenderState(D3DRS_SRCBLEND, &sb);
        dev->GetRenderState(D3DRS_DESTBLEND, &db);
        dev->GetRenderState(D3DRS_ZWRITEENABLE, &zw);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        for (D3DRenderMesh* m : g.muzzle) r.DrawMesh(m);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, ab);
        dev->SetRenderState(D3DRS_SRCBLEND, sb);
        dev->SetRenderState(D3DRS_DESTBLEND, db);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, zw);
    }
}
