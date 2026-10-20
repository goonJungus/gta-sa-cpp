#define NOMINMAX
#include "Pickups.h"

#include "D3DRenderer.h"
#include "DffLoader.h"
#include "ImgLoader.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
// AmmoForWeapon_OnStreet, indexed by eWeaponType 0..46.
const int kOnStreetAmmo[47] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 8, 8, 8, 8, 4, 4, 30, 10, 10, 15, 10, 10,
                               60, 60, 80, 80, 60, 20, 10, 4, 3, 100, 500, 5, 1, 500, 500, 36, 0, 0, 1};
constexpr float kPickRadius2 = 1.8f;      // sqrt(1.8) in 2D
constexpr float kPickHeight = 2.0f;
struct KindModel { const char* dff; const char* txd; };
const KindModel kKindModel[Pickups::kKinds] = {
    {nullptr, nullptr},                // Weapon: the gun's own model
    {"money", "dyn_cash"},             // MI_PICKUP_MONEY 1212
    {"health", "icons5"},              // 1240
    {"bodyarmour", "icons7"},          // 1242
    {"adrenaline", "icons8"},          // 1241
    {"info", "icons6"},                // 1239
    {"bribe", "icons9"},               // 1247
    {"bonus", "icons6"},               // 1248
    {nullptr, nullptr},                // Jetpack: drawn by the player's pack model
};
}  // namespace

int Pickups::OnStreetAmmo(int weapon) { return weapon >= 0 && weapon < 47 ? kOnStreetAmmo[weapon] : 0; }

void Pickups::Clear() { for (Slot& s : m_s) s = Slot(); }

int Pickups::Used() const {
    int n = 0;
    for (const Slot& s : m_s) if (s.used) n++;
    return n;
}

int Pickups::Add(const float pos[3], int kind, int weapon, int type, int ammo, bool empty, uint32_t nowMs) {
    int at = -1;
    for (int i = 0; i < kSlots && at < 0; i++) if (!m_s[i].used) at = i;
    if (at < 0) {   // full: the oldest cash pickup goes, else the oldest one that times out
        uint32_t best = 0xFFFFFFFFu;
        for (int i = 0; i < kSlots; i++)
            if (m_s[i].type == Money && m_s[i].regenMs < best) { best = m_s[i].regenMs; at = i; }
        if (at < 0)
            for (int i = 0; i < kSlots; i++)
                if ((m_s[i].type == OnceTimeout || m_s[i].type == OnceTimeoutSlow) && m_s[i].regenMs < best) { best = m_s[i].regenMs; at = i; }
        if (at < 0) return -1;
    }
    Slot& s = m_s[at];
    s = Slot();
    s.used = true;
    s.type = type; s.kind = kind; s.weapon = weapon; s.ammo = ammo; s.empty = empty;
    for (int k = 0; k < 3; k++) s.pos[k] = pos[k];
    if (type == OnceTimeout) s.regenMs = nowMs + 20000;
    else if (type == OnceTimeoutSlow) s.regenMs = nowMs + 120000;
    else if (type == Money) s.regenMs = nowMs + 30000;
    return at;
}

void Pickups::CreateSomeMoney(const float pos[3], int amount, uint32_t nowMs) {
    const int wads = std::min(amount / 20 + 1, 7);
    const int per = amount / wads;
    for (int i = 0; i < wads; i++) {
        const float a = (float)(Rand() % 628) * 0.01f;
        const float p[3] = {pos[0] + sinf(a) * 1.5f, pos[1] + cosf(a) * 1.5f, pos[2]};
        Add(p, Cash, 0, Money, (Rand() & 3) + per, false, nowMs);
    }
}

void Pickups::Update(uint32_t nowMs, Env& e) {
    auto log = [&](const char* what, int i, int a, int b) { if (e.log) e.log(what, i, a, b); };
    for (int i = 0; i < kSlots; i++) {
        Slot& s = m_s[i];
        if (!s.used) continue;
        if (s.disabled) {   // CPickup::Update: back after the regeneration time once the player is 10 m away
            if (nowMs > s.regenMs) {
                const float dx = s.pos[0] - e.player[0], dy = s.pos[1] - e.player[1], dz = s.pos[2] - e.player[2];
                if (sqrtf(dx * dx + dy * dy + dz * dz) > 10.0f) { s.disabled = false; log("REGEN", i, s.kind, s.weapon); }
            }
            continue;
        }
        const float dx = e.player[0] - s.pos[0], dy = e.player[1] - s.pos[1];
        const float d2 = sqrtf(dx * dx + dy * dy), dz = fabsf(e.player[2] - s.pos[2]);
        bool picked = false;
        if (s.kind == Bribe) picked = (e.onFoot ? (dz < kPickHeight && d2 * d2 < kPickRadius2) : (dz < kPickHeight && d2 < 2.0f)) && e.alive;
        else picked = e.onFoot && e.alive && dz < kPickHeight && d2 * d2 < kPickRadius2;
        const bool isWeapon = s.kind == Weapon;
        const bool cashType = s.type == Money || s.type == MoneyDoesntDisappear;
        if (picked && !cashType && isWeapon && s.weapon > 0) {
            const int w = s.weapon;
            const int state = e.slotState ? e.slotState(w) : 0;
            if (state == 1) {
                // another weapon in the slot: the ammo is taken out of the pickup in the exchangeable slots,
                // then the player has to ask for the swap
                if (e.exchangeable && e.exchangeable(w) &&
                    (s.type == OnceTimeout || s.type == Once || s.type == OnStreet || s.type == OnceForMission)) {
                    if (s.ammo || !s.empty) {
                        if (e.grantAmmo) e.grantAmmo(w, s.ammo ? s.ammo : OnStreetAmmo(w));
                        log("EXTRACT", i, w, s.ammo ? s.ammo : OnStreetAmmo(w));
                    }
                    s.empty = true; s.ammo = 0;
                    if (e.reloadSlot) e.reloadSlot();
                }
                if (e.canPickNow && !e.canPickNow(w)) picked = false;
                else {
                    if (picked && !s.helpShown) { if (e.help) e.help("PU_CF1"); s.helpShown = true; log("HELP", i, w, 0); }
                    if (!e.collectKey || e.msSinceHsLos < 1500) picked = false;
                }
            } else if (e.msSinceHsLos < 1500 || (e.canPickNow && !e.canPickNow(w))) {
                picked = false;
            }
        }
        if (picked) {
            bool ok = true;
            if (s.kind == Armour && e.armour > e.maxArmour - 0.2f) ok = false;
            if (s.kind == Health && e.health > e.maxHealth - 0.2f) ok = false;
            if (s.kind == Bribe && e.wanted == 0) ok = false;
            if (s.kind == Jetpack && e.onJetpack) ok = false;
            if (!ok) { if (!s.skipLogged) { log("SKIP", i, s.kind, 0); s.skipLogged = true; } picked = false; }
        }
        if (picked) {
            bool removed = false;
            if (cashType) {
                if (e.giveMoney) e.giveMoney(s.ammo);
                log("MONEY", i, s.ammo, 0);
                removed = true;
            } else {
                bool goodie = s.kind != Weapon && s.kind != Cash;
                bool took = true;
                if (goodie) { if (e.goodie) e.goodie(s.kind); log("GOODIE", i, s.kind, 0); }
                else if (!e.wantsWeapon || !e.wantsWeapon(s.weapon)) {
                    took = false;
                    if (s.type != OnStreet && s.type != OnStreetSlow) {
                        if (e.exchangeable && e.exchangeable(s.weapon) && e.grantAmmo) {   // ExtractAmmoFromPickup
                            const int a = s.ammo ? s.ammo : (s.empty ? 0 : OnStreetAmmo(s.weapon));
                            if (a) { e.grantAmmo(s.weapon, a); s.ammo = 0; s.empty = true; }
                        }
                    }
                    log("REFUSE", i, s.weapon, 0);
                } else {
                    const int a = s.ammo ? s.ammo : (s.empty ? 0 : OnStreetAmmo(s.weapon));
                    if (e.grantWeapon) e.grantWeapon(s.weapon, a);
                    log("TAKE", i, s.weapon, a);
                }
                if (took) {
                    if (s.type == OnStreet || s.type == OnStreetSlow) {
                        s.regenMs = nowMs + (s.type == OnStreet ? 30000 : (s.kind == Bribe ? 30000 : 36000));
                        s.disabled = true;
                        log("DISABLE", i, (int)(s.regenMs - nowMs), 0);
                    } else removed = true;
                }
            }
            if (removed) { s = Slot(); continue; }
        }
        if (!s.disabled && (s.type == OnceTimeout || s.type == OnceTimeoutSlow || s.type == Money) && nowMs > s.regenMs) {
            log("EXPIRE", i, s.type, 0);
            s = Slot();
        }
    }
}

bool Pickups::LoadModel(D3DRenderer& r, void* img, const char* dff, const char* txd, Model& out) {
    ImgLoader& gta3 = *static_cast<ImgLoader*>(img);
    const std::vector<uint8_t> dffData = gta3.Extract(std::string(dff) + ".dff");
    const std::vector<uint8_t> txdData = gta3.Extract(std::string(txd) + ".txd");
    if (dffData.empty()) return false;
    std::vector<TxdTexture> txdTex;
    if (!txdData.empty()) txdTex = TxdLoader::LoadFromMemory(txdData.data(), txdData.size());
    DffModel m = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    for (DffMesh& dm : m.meshes) {
        if (dm.vertices.empty() || dm.indices.empty()) continue;
        for (const DffVertex& v : dm.vertices) {
            const float c[3] = {v.x, v.y, v.z};
            for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], c[k]); hi[k] = std::max(hi[k], c[k]); }
        }
        D3DRenderMesh* mesh = r.CreateMesh(reinterpret_cast<const MeshVertex*>(dm.vertices.data()), (uint32_t)dm.vertices.size(),
                                           dm.indices.data(), (uint32_t)dm.indices.size());
        if (!mesh) continue;
        for (const TxdTexture& t : txdTex) {
            if (TxdLoader::KeyOf(t.name) != TxdLoader::KeyOf(dm.textureName)) continue;
            IDirect3DTexture9* tex = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(), &t.extraMips);
            mesh->texture = tex;
            out.textures.push_back(tex);
            break;
        }
        out.meshes.push_back(mesh);
    }
    if (hi[0] >= lo[0]) out.maxDim = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2], 0.01f});
    return !out.meshes.empty();
}

bool Pickups::Load(D3DRenderer& r, const std::string& gameDir) {
    ImgLoader gta3;
    if (!gta3.Open(gameDir + "\\models\\gta3.img")) return false;
    int n = 0;
    for (int k = 0; k < kKinds; k++) {
        if (!kKindModel[k].dff) continue;
        if (LoadModel(r, &gta3, kKindModel[k].dff, kKindModel[k].txd, m_model[k])) n++;
        else std::fprintf(stderr, "[PICKUP] model %s not loaded\n", kKindModel[k].dff);
    }
    std::fprintf(stderr, "[PICKUP] %d models loaded\n", n);
    return n > 0;
}

void Pickups::Shutdown(D3DRenderer& r) {
    for (Model& m : m_model) {
        for (D3DRenderMesh* me : m.meshes) r.DestroyMesh(me);
        for (IDirect3DTexture9* t : m.textures) if (t) r.DestroyTexture(t);
        m = Model();
    }
}

void Pickups::Draw(D3DRenderer& r, uint32_t nowMs, const float cam[3],
                   const std::function<void(int, const float[4][4])>& drawWeapon,
                   const std::function<void(const float[4][4])>& drawJetpack) {
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    for (const Slot& s : m_s) {
        if (!s.used || s.disabled) continue;
        const float dx = s.pos[0] - cam[0], dy = s.pos[1] - cam[1];
        if (dx * dx + dy * dy > 100.0f * 100.0f) continue;   // CPickup::IsVisible: 100 m
        // CPickups::Update spins the models: (t & 0x7ff) * 0.0030566407 radians
        const float a = (float)(nowMs & 0x7ff) * 0.0030566407f;
        const Model& m = m_model[s.kind];
        float sc = 1.0f;
        if (s.kind != Weapon && s.kind != Jetpack) sc = (std::max(1.2f / m.maxDim, 1.0f) - 1.0f) * 0.6f + 1.0f;
        const float c = cosf(a) * sc, sn = sinf(a) * sc;
        float w[4][4] = {{c, sn, 0, 0}, {-sn, c, 0, 0}, {0, 0, sc, 0}, {s.pos[0], s.pos[1], s.pos[2], 1}};
        if (s.kind == Weapon) { if (drawWeapon) drawWeapon(s.weapon, w); continue; }
        if (s.kind == Jetpack) { if (drawJetpack) drawJetpack(w); continue; }
        D3DMATRIX out;
        float* o = &out._11;
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) o[i * 4 + j] = w[i][j];
        dev->SetTransform(D3DTS_WORLD, &out);
        for (D3DRenderMesh* me : m.meshes) r.DrawMesh(me);
    }
}
