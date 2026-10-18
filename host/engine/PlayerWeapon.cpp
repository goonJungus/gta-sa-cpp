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
    {"COUNTRYRIFLE", "Country rifle", "weapon_sniper", "weapon_sniper", "rifle_load", "rifle_crouchload", false},
    {"SNIPERRIFLE", "Sniper rifle", "weapon_sniper", "weapon_sniper", "rifle_load", "rifle_crouchload", false},
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
    for (std::string line; std::getline(wf, line);) {
        if (line.empty() || line[0] != '$') continue;
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

    for (const GunSpec& sp : kSpecs) {
        const std::vector<std::string>* row = nullptr;
        for (const auto& t : rows) {
            if (t[1] != sp.name) continue;
            if (!row || t[14] == std::to_string(s_weaponSkill)) row = &t;   // the player's skill row (0 poor, 1 std), else the first
        }
        if (!row) continue;
        Gun g;
        g.spec = &sp;
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
        // flags are hex, the first digit (CANAIM 1, AIMWITHARM 2, 1STPERSON 4, ONLYFREEAIM 8) is the last character
        g.aimWithArm = (std::strtoul((*row)[25].c_str(), nullptr, 16) & 0x2) != 0;
        g.reloadTime = 1.0f;
        g.fireOffset[0] = (float)std::atof((*row)[11].c_str());
        g.fireOffset[1] = (float)std::atof((*row)[12].c_str());
        g.fireOffset[2] = (float)std::atof((*row)[13].c_str());
        const int model = std::atoi((*row)[5].c_str());
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
        g.ammo = g.clipSize;
        g.total = g.clipSize * 6;
        m_guns.push_back(std::move(g));
    }
    std::fprintf(stderr, "[GUN] %zu weapons\n", m_guns.size());
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
    m_gun = index >= 0 && index < (int)m_guns.size() ? index : -1;
    m_reloading = m_firing = false;
    for (Assoc& a : m_assocs) if (a.partial) { a.dead = true; a.blend = 0.0f; }
}

void PlayerPed::NextGun() {
    SelectGun(m_gun + 1 >= (int)m_guns.size() ? -1 : m_gun + 1);
}

void PlayerPed::RefillAmmo() {
    for (Gun& g : m_guns) { g.ammo = g.clipSize; g.total = g.clipSize * 60; }
}

int PlayerPed::GunCount() const { return (int)m_guns.size(); }
const char* PlayerPed::GunNameAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].name.c_str() : "Fists"; }
IDirect3DTexture9* PlayerPed::GunIconAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].icon : nullptr; }
int PlayerPed::GunClipAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].ammo : 0; }
int PlayerPed::GunTotalAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].total : 0; }
int PlayerPed::GunClipSizeAt(int i) const { return i >= 0 && i < (int)m_guns.size() ? m_guns[i].clipSize : 0; }
const char* PlayerPed::GunName() const { return m_gun >= 0 ? m_guns[m_gun].name.c_str() : "Fists"; }
const char* PlayerPed::GunId() const { return m_gun >= 0 && m_guns[m_gun].spec ? m_guns[m_gun].spec->name : ""; }
int PlayerPed::GunAmmo() const { return m_gun >= 0 ? m_guns[m_gun].ammo : 0; }
int PlayerPed::GunClipSize() const { return m_gun >= 0 ? m_guns[m_gun].clipSize : 0; }
int PlayerPed::GunTotalAmmo() const { return m_gun >= 0 ? m_guns[m_gun].total : 0; }
IDirect3DTexture9* PlayerPed::GunIcon() const { return m_gun >= 0 ? m_guns[m_gun].icon : nullptr; }

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
    const bool crouch = m_ducking;
    const float loopStart = crouch ? g.loop2Start : g.loopStart, loopEnd = crouch ? g.loop2End : g.loopEnd,
                fireTime = crouch ? g.fire2Time : g.fireTime;
    const AnimClip* fireClip = Clip(crouch && sp.crouchFire && Clip(sp.crouchFire) ? sp.crouchFire : g.fireClip);
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
            if (std::strcmp(sp.name, "PISTOL_SILENCED") != 0) m_flash = 0.06f;   // the silenced pistol has no flash
            std::fprintf(stderr, "[GUN] shot %s ammo %d/%d\n", g.name.c_str(), g.ammo, g.clipSize);
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
                if (fa->time >= fireTime) {
                    m_shotDone = true;
                    g.ammo--;
                    m_shotEvents++;
                    if (std::strcmp(sp.name, "PISTOL_SILENCED") != 0) m_flash = 0.06f;
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
    const RwMat& h = m_globals[f];
    float p[3];
    for (int j = 0; j < 3; j++) p[j] = o[0] * h.m[0][j] + o[1] * h.m[1][j] + o[2] * h.m[2][j] + h.m[3][j];
    const float a = m_heading - 1.57079633f, c = std::cos(a), s = std::sin(a);
    out[0] = x + p[0] * c - p[1] * s;
    out[1] = y + p[0] * s + p[1] * c;
    out[2] = feetZ - m_bindMinZ + p[2];
    return true;
}

void PlayerPed::DrawGun(D3DRenderer& r, float x, float y, float feetZ) {
    if (m_gun < 0) return;
    const int f = m_frameOfBone[kBoneRHand];
    if (f < 0 || f >= (int)m_globals.size()) return;
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    const float a = m_heading - 1.57079633f, c = std::cos(a), s = std::sin(a);
    const float w[4][4] = {{c, s, 0, 0}, {-s, c, 0, 0}, {0, 0, 1, 0}, {x, y, feetZ - m_bindMinZ, 1}};
    const RwMat& h = m_globals[f];
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
    Gun& g = m_guns[m_gun];
    for (D3DRenderMesh* m : g.meshes) r.DrawMesh(m);
    if (m_flash > 0.0f && !g.muzzle.empty()) {
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
