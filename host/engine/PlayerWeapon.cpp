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
};

namespace {

constexpr float kFrame = 1.0f / 30.0f;   // weapon.dat anim frames are 1/30 s
constexpr int kBoneRHand = 24;           // BONE_R_HAND

const GunSpec kSpecs[] = {
    {"PISTOL", "Pistol", "colt45_fire", "colt45_crouchfire", "colt45_reload", "colt45_crouchreload", false},
    {"PISTOL_SILENCED", "Silenced pistol", "silence_fire", "silencecrouchfire", "silence_reload", "crouchreload", false},
    {"DESERT_EAGLE", "Desert Eagle", "python_fire", "python_crouchfire", "python_reload", "python_crouchreload", false},
    {"SHOTGUN", "Shotgun", "shotgun_fire", "shotgun_crouchfire", nullptr, nullptr, false},
    {"SAWNOFF", "Sawn-off shotgun", "colt45_fire", "colt45_crouchfire", "sawnoff_reload", "colt45_crouchreload", false},
    {"SPAS12", "Combat shotgun", "buddy_fire", "buddy_crouchfire", "buddy_reload", "buddy_crouchreload", false},
    {"MICRO_UZI", "Micro SMG", "colt45_fire", "colt45_crouchfire", "colt45_reload", "colt45_crouchreload", true},
    {"TEC9", "Tec-9", "colt45_fire", "colt45_crouchfire", "colt45_reload", "colt45_crouchreload", true},
    {"MP5", "MP5", "uzi_fire", "uzi_crouchfire", "uzi_reload", "uzi_crouchreload", true},
    {"AK47", "AK-47", "rifle_fire", "rifle_crouchfire", "rifle_load", "rifle_crouchload", true},
    {"M4", "M4", "rifle_fire", "rifle_crouchfire", "rifle_load", "rifle_crouchload", true},
    {"COUNTRYRIFLE", "Country rifle", "rifle_fire", "rifle_crouchfire", "rifle_load", "rifle_crouchload", false},
    {"SNIPERRIFLE", "Sniper rifle", "rifle_fire", "rifle_crouchfire", "rifle_load", "rifle_crouchload", false},
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
            if (!row || t[14] == "1") row = &t;   // skill 1 = STD, else the first
        }
        if (!row) continue;
        Gun g;
        g.spec = &sp;
        g.name = sp.label;
        g.clipSize = std::atoi((*row)[9].c_str());
        g.loopStart = (float)std::atof((*row)[18].c_str()) * kFrame;
        g.loopEnd = (float)std::atof((*row)[19].c_str()) * kFrame;
        g.fireTime = (float)std::atof((*row)[20].c_str()) * kFrame;
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
            for (const DffMesh& dm : m.meshes) {
                if (dm.vertices.empty() || dm.indices.empty()) continue;
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
                g.meshes.push_back(mesh);
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

const char* PlayerPed::GunName() const { return m_gun >= 0 ? m_guns[m_gun].name.c_str() : "Fists"; }
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
    const bool crouch = m_ducking;
    const AnimClip* fireClip = Clip(crouch && sp.crouchFire && Clip(sp.crouchFire) ? sp.crouchFire : sp.fire);
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
        fa->time = g.loopStart;
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
        fa->time = g.loopStart;
        fa->playing = false;
        return;
    }

    // aim in
    fa->delta = 0.0f;
    fa->blend += std::clamp(1.0f - fa->blend, -step, step);

    const bool want = in.fireJust || (sp.continuous && in.fireHeld);
    if (!m_firing) {
        fa->time = g.loopStart;
        fa->playing = false;
        if (want && g.ammo == 0 && in.fireJust && g.total == 0) m_dryEvents++;
        if (want && g.ammo > 0 && fa->blend > 0.9f) {
            m_firing = true;
            m_shotDone = false;
            fa->playing = true;
        }
    }
    if (m_firing) {
        if (!m_shotDone && fa->time >= g.fireTime) {
            m_shotDone = true;
            g.ammo--;
            m_shotEvents++;
            std::fprintf(stderr, "[GUN] shot %s ammo %d/%d\n", g.name.c_str(), g.ammo, g.clipSize);
        }
        if (fa->time >= g.loopEnd || fa->finished) {
            m_firing = false;
            fa->finished = false;
            fa->time = g.loopStart;
            fa->playing = false;
        }
    }
}

// The weapon model sits on the right-hand bone matrix (RenderWeaponPedsForPC).
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
    for (D3DRenderMesh* m : m_guns[m_gun].meshes) r.DrawMesh(m);
}
