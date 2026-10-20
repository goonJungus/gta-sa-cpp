// CJ's clothes: CClothes::RebuildPlayer / CClothesBuilder::CreateSkinnedClump. The outfit (PedClothesDesc) picks
// one model per slot and a texture per body area from models\player.img; the clothes.dat rules adjust it first
// (Clothes::Preprocess) and the body textures are blended by the fat / muscle ratios and carry the tattoos.

#define NOMINMAX
#include "PlayerPed.h"

#include "ImgLoader.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

const TxdTexture* FindTex(const std::vector<TxdTexture>& txd, const char* name) {
    for (const TxdTexture& t : txd)
        if (Lower(t.name) == name) return &t;
    return nullptr;
}
const TxdTexture* FirstTex(const std::vector<TxdTexture>& txd) {
    for (const TxdTexture& t : txd)
        if (t.d3dFormat == D3DFMT_A8R8G8B8) return &t;
    return nullptr;
}

// 32-bit BGRA pixels, resampled to the destination size (nearest).
struct Raster {
    uint32_t w = 0, h = 0;
    std::vector<uint8_t> px;
    bool ok() const { return w && h && px.size() == (size_t)w * h * 4; }
    static Raster From(const TxdTexture& t) {
        Raster r;
        if (t.d3dFormat != D3DFMT_A8R8G8B8) return r;
        r.w = t.width; r.h = t.height; r.px = t.mip0;
        return r;
    }
    const uint8_t* At(uint32_t x, uint32_t y, uint32_t dw, uint32_t dh) const {
        return &px[((size_t)(y * h / dh) * w + (size_t)(x * w / dw)) * 4];
    }
};

// CClothesBuilder::BlendTextures: colours weighted by the three ratios, alpha untouched.
Raster BlendThree(const Raster& a, const Raster* b, const Raster* c, float ra, float rb, float rc) {
    Raster out = a;
    for (uint32_t y = 0; y < a.h; y++)
        for (uint32_t x = 0; x < a.w; x++) {
            uint8_t* d = &out.px[((size_t)y * a.w + x) * 4];
            const uint8_t* sa = a.At(x, y, a.w, a.h);
            const uint8_t* sb = b ? b->At(x, y, a.w, a.h) : sa;
            const uint8_t* sc = c ? c->At(x, y, a.w, a.h) : sa;
            for (int k = 0; k < 3; k++) d[k] = (uint8_t)std::clamp(sa[k] * ra + sb[k] * rb + sc[k] * rc + 0.5f, 0.0f, 255.0f);
        }
    return out;
}

// CClothesBuilder::PlaceTextureOnTopOfTexture: a texel with any alpha replaces the one below.
void PlaceOnTop(Raster& dst, const Raster& src) {
    for (uint32_t y = 0; y < dst.h; y++)
        for (uint32_t x = 0; x < dst.w; x++) {
            const uint8_t* s = src.At(x, y, dst.w, dst.h);
            if (s[3]) std::memcpy(&dst.px[((size_t)y * dst.w + x) * 4], s, 4);
        }
}
}  // namespace

// The body CJ starts the game in: white vest, denim jeans, black sneakers.
PedClothesDesc PlayerPed::DefaultOutfit() {
    PedClothesDesc d;
    d.SetTextureAndModel("vest", "vest", CLOTHES_TEXTURE_TORSO);
    d.SetTextureAndModel("jeansdenim", "jeans", CLOTHES_TEXTURE_LEGS);
    d.SetTextureAndModel("sneakerbincblk", "sneaker", CLOTHES_TEXTURE_SHOES);
    return d;
}

// CClothesBuilder::ConstructTextures. Body ratios are CClothesBuilder::CreateSkinnedClump's.
bool PlayerPed::BuildTextures(const PedClothesDesc& d, float rN, float rF, float rM) {
    ImgLoader img;
    if (!img.Open(m_gameDir + "\\models\\player.img")) return false;
    auto txd = [&](uint32_t key) {
        const std::string& n = Clothes::Name(key);
        std::vector<uint8_t> b = n.empty() ? std::vector<uint8_t>() : img.Extract(n + ".txd");
        return b.empty() ? std::vector<TxdTexture>() : TxdLoader::LoadFromMemory(b.data(), b.size());
    };
    auto stock = [&](const char* name) { return txd(Clothes::Key(name)); };
    auto upload = [&](const Raster& r) -> IDirect3DTexture9* {
        if (!r.ok()) return nullptr;
        std::vector<uint8_t> px = r.px;
        return m_r->CreateTexture(r.w, r.h, D3DFMT_A8R8G8B8, px.data(), (uint32_t)px.size());
    };
    auto opaque = [](Raster r) { for (size_t i = 3; i < r.px.size(); i += 4) r.px[i] = 0xFF; return r; };

    for (IDirect3DTexture9* t : m_textures) if (t) m_r->DestroyTexture(t);
    m_textures.assign(9, nullptr);

    // a body texture with its fat and ripped variants
    auto body = [&](const char* stockTxd, const std::vector<TxdTexture>& own, const char* nm, const char* fat, const char* ripped) {
        std::vector<TxdTexture> dict = own.empty() ? stock(stockTxd) : own;
        const TxdTexture *n = FindTex(dict, nm), *f = fat ? FindTex(dict, fat) : nullptr, *m = ripped ? FindTex(dict, ripped) : nullptr;
        Raster out;
        if (!n) { if (const TxdTexture* t = FirstTex(dict)) out = Raster::From(*t); return out; }
        Raster rn = Raster::From(*n), rf = f ? Raster::From(*f) : Raster(), rm = m ? Raster::From(*m) : Raster();
        if (!rn.ok()) return out;
        if (rf.ok() || rm.ok()) out = BlendThree(rn, rf.ok() ? &rf : nullptr, rm.ok() ? &rm : nullptr, rN, rF, rM);
        else out = rn;
        return out;
    };

    // torso: skin, tattoos (alpha lerp), then the shirt on top
    Raster torso = body("player_torso", {}, "torso", "torso_fat", "torso_ripped");
    if (torso.ok()) {
        Raster tat;
        for (int i = CLOTHES_TEXTURE_LOWER_LEFT_ARM; i <= CLOTHES_TEXTURE_UPPER_BACK; i++) {
            if (!d.textureKeys[i]) continue;
            std::vector<TxdTexture> t = txd(d.textureKeys[i]);
            const TxdTexture* ft = FirstTex(t);
            if (!ft) continue;
            Raster layer = Raster::From(*ft);
            if (!tat.ok()) { tat.w = layer.w; tat.h = layer.h; tat.px.assign(layer.px.size(), 0); }
            PlaceOnTop(tat, layer);
        }
        if (tat.ok())
            for (uint32_t y = 0; y < torso.h; y++)
                for (uint32_t x = 0; x < torso.w; x++) {
                    const uint8_t* s = tat.At(x, y, torso.w, torso.h);
                    uint8_t* o = &torso.px[((size_t)y * torso.w + x) * 4];
                    const float a = s[3] / 255.0f;
                    for (int k = 0; k < 3; k++) o[k] = (uint8_t)(o[k] + (s[k] - o[k]) * a);
                }
        if (d.textureKeys[CLOTHES_TEXTURE_TORSO]) {
            std::vector<TxdTexture> t = txd(d.textureKeys[CLOTHES_TEXTURE_TORSO]);
            if (const TxdTexture* ft = FirstTex(t)) PlaceOnTop(torso, Raster::From(*ft));
        }
    }
    m_textures[0] = upload(opaque(torso));

    // face: its own dictionary (hair, beard) or the stock face
    {
        std::vector<TxdTexture> dict = d.textureKeys[CLOTHES_TEXTURE_HEAD] ? txd(d.textureKeys[CLOTHES_TEXTURE_HEAD]) : stock("player_face");
        if (dict.empty()) dict = stock("player_face");
        const TxdTexture *n = FindTex(dict, "face"), *f = FindTex(dict, "face_fat");
        Raster out;
        if (n) {
            out = Raster::From(*n);
            if (f && out.ok()) {
                Raster rf = Raster::From(*f);
                const float s = rN + rF + rM;
                if (rf.ok() && s > 0.0f) out = BlendThree(out, &rf, nullptr, (rN + rM) / s, rF / s, 0.0f);
            }
        } else if (const TxdTexture* t = FirstTex(dict)) out = Raster::From(*t);
        m_textures[1] = upload(out.ok() ? opaque(out) : out);
    }

    // legs
    {
        Raster legs = body("player_legs", {}, "legs", "legs_fat", "legs_ripped");
        if (legs.ok() && d.textureKeys[CLOTHES_TEXTURE_LEGS]) {
            std::vector<TxdTexture> t = txd(d.textureKeys[CLOTHES_TEXTURE_LEGS]);
            if (const TxdTexture* ft = FirstTex(t)) PlaceOnTop(legs, Raster::From(*ft));
        }
        m_textures[2] = upload(opaque(legs));
    }

    // feet, then the accessories: the first texture of each dictionary
    {
        std::vector<TxdTexture> t = d.textureKeys[CLOTHES_TEXTURE_SHOES] ? txd(d.textureKeys[CLOTHES_TEXTURE_SHOES]) : stock("player_feet");
        const TxdTexture* ft = FirstTex(t);
        m_textures[3] = ft ? upload(opaque(Raster::From(*ft))) : nullptr;
    }
    static const int kAcc[5] = {CLOTHES_TEXTURE_NECKLACE, CLOTHES_TEXTURE_BRACELET, CLOTHES_TEXTURE_GLASSES, CLOTHES_TEXTURE_HATS, CLOTHES_TEXTURE_SPECIAL};
    for (int i = 0; i < 5; i++) {
        if (!d.textureKeys[kAcc[i]]) continue;
        std::vector<TxdTexture> t = txd(d.textureKeys[kAcc[i]]);
        if (const TxdTexture* ft = FirstTex(t)) m_textures[4 + i] = upload(Raster::From(*ft));
    }
    for (int i = 0; i < 4; i++)
        if (!m_textures[i]) { std::fprintf(stderr, "[CJ] body texture %d failed\n", i); return false; }
    return true;
}

// CClothesBuilder::CreateSkinnedClump: one part per worn model, skinned onto the base skeleton.
bool PlayerPed::BuildOutfit(const PedClothesDesc& want) {
    if (!m_r) return false;
    PedClothesDesc d = want;
    static const char* kBase[5] = {"torso", "head", "hands", "legs", "feet"};
    for (int i = 0; i < 5; i++)
        if (!d.modelKeys[i]) d.SetModel(kBase[i], (ClothesModelPart)i);
    m_outfit = d;
    Clothes::Preprocess(d, false);
    for (int i = 0; i < 5; i++)   // a rule may hide a base part, the builder still needs it
        if (!d.modelKeys[i] && i != CLOTHES_MODEL_TORSO && i != CLOTHES_MODEL_LEGS) d.SetModel(kBase[i], (ClothesModelPart)i);

    // CreateSkinnedClump's blend ratios
    float rM = std::clamp(m_muscleStat, 0.0f, 1.0f), rF = std::clamp(m_fatStat, 0.0f, 1.0f), rN = 1.0f - rM - rF;
    if (rN <= 0.0f) { const float t = 1.0f / (rF + rM); rM *= t; rF *= t; rN = 0.0f; }
    if (!BuildTextures(d, rN, rF, rM)) return false;
    m_texFat = m_fatStat;
    m_texMuscle = m_muscleStat;

    ImgLoader img;
    if (!img.Open(m_gameDir + "\\models\\player.img")) return false;
    for (Part& p : m_parts) if (p.mesh) m_r->DestroyMesh(p.mesh);
    m_parts.clear();
    m_bindMinZ = 0.0f;
    static const int kTex[CLOTHES_MODEL_TOTAL] = {0, 1, 0, 2, 3, 4, 5, 6, 7, 8};   // BODY_PART_TEX_NAMES
    for (int slot = 0; slot < CLOTHES_MODEL_TOTAL; slot++) {
        if (!d.modelKeys[slot]) continue;
        const std::string& name = Clothes::Name(d.modelKeys[slot]);
        std::vector<uint8_t> b = name.empty() ? std::vector<uint8_t>() : img.Extract(name + ".dff");
        std::vector<SkinnedClump> clumps = SkinnedDff::LoadClumps(b.data(), b.size());
        const SkinnedClump* c = nullptr;
        for (const SkinnedClump& k : clumps)
            if (k.valid && (!c || Lower(k.rootName) == "normal")) c = &k;
        if (!c) { std::fprintf(stderr, "[CJ] part %s failed\n", name.c_str()); continue; }

        // BuildBoneIndexConversionTable: part node -> bone id -> base node (bones the base lacks map to node 0)
        Part part;
        part.verts = c->vertices;
        part.shape[0] = c->vertices;   // CClothesBuilder::BlendGeometry mixes the Fat and Ripped shapes by ratio
        for (const SkinnedClump& k : clumps) {
            const std::string n = Lower(k.rootName);
            const int s = n == "fat" ? 1 : n == "ripped" ? 2 : 0;
            if (s && k.valid && k.vertices.size() == c->vertices.size()) part.shape[s] = k.vertices;
        }
        part.boneFrame.resize(c->nodeIds.size(), 0);
        part.inverseBind.resize(c->nodeIds.size());
        for (size_t n = 0; n < c->nodeIds.size(); n++) {
            const int id = c->nodeIds[n];
            int bn = (id >= 0 && id < 512) ? m_baseNodeOfBone[id] : -1;
            if (bn < 0) bn = 0;
            const int f = m_frameOfBone[m_baseNodeIds[bn]];
            part.boneFrame[n] = f >= 0 ? f : 0;
            part.inverseBind[n] = m_baseInverse[bn];
        }
        part.skinned.resize(part.verts.size());
        for (size_t v = 0; v < part.verts.size(); v++) {
            const SkinVertex& sv = part.verts[v];
            part.skinned[v] = {sv.pos[0], sv.pos[1], sv.pos[2], sv.normal[0], sv.normal[1], sv.normal[2],
                               0xFFFFFFFFu, sv.uv[0], sv.uv[1]};
            m_bindMinZ = std::min(m_bindMinZ, sv.pos[2]);
        }
        part.mesh = m_r->CreateMesh(part.skinned.data(), (uint32_t)part.skinned.size(), c->indices.data(), (uint32_t)c->indices.size());
        if (part.mesh) part.mesh->texture = m_textures[kTex[slot]];
        part.isHead = slot == CLOTHES_MODEL_HEAD;
        m_parts.push_back(std::move(part));
    }
    return m_parts.size() >= 5;
}

// CClothes::RebuildPlayer: the new outfit on the same skeleton, the poses carry on.
bool PlayerPed::SetOutfit(const PedClothesDesc& desc) {
    if (!m_ready) return false;
    const PedClothesDesc old = m_outfit;
    if (!BuildOutfit(desc)) { BuildOutfit(old); return false; }
    SetBodyShape(m_fatStat, m_muscleStat);
    return true;
}
