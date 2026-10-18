// CJ, the player ped. See PlayerPed.h.

#define NOMINMAX
#include "PlayerPed.h"

#include "ImgLoader.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr float kPi = 3.14159265f;

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// RtQuatConvertToMatrix: rows are the rotated right/up/at axes.
void QuatToRows(const float q[4], RwMat& m) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    m.m[0][0] = 1 - 2 * (y * y + z * z); m.m[0][1] = 2 * (x * y + w * z); m.m[0][2] = 2 * (x * z - w * y);
    m.m[1][0] = 2 * (x * y - w * z); m.m[1][1] = 1 - 2 * (x * x + z * z); m.m[1][2] = 2 * (y * z + w * x);
    m.m[2][0] = 2 * (x * z + w * y); m.m[2][1] = 2 * (y * z - w * x); m.m[2][2] = 1 - 2 * (x * x + y * y);
}

// Inverse of QuatToRows (for the bind pose of bones a clip does not animate).
void RowsToQuat(const RwMat& m, float q[4]) {
    const float tr = m.m[0][0] + m.m[1][1] + m.m[2][2];
    if (tr > 0) {
        float s = std::sqrt(tr + 1.0f) * 2;
        q[3] = 0.25f * s;
        q[0] = (m.m[1][2] - m.m[2][1]) / s;
        q[1] = (m.m[2][0] - m.m[0][2]) / s;
        q[2] = (m.m[0][1] - m.m[1][0]) / s;
    } else if (m.m[0][0] > m.m[1][1] && m.m[0][0] > m.m[2][2]) {
        float s = std::sqrt(1.0f + m.m[0][0] - m.m[1][1] - m.m[2][2]) * 2;
        q[3] = (m.m[1][2] - m.m[2][1]) / s;
        q[0] = 0.25f * s;
        q[1] = (m.m[1][0] + m.m[0][1]) / s;
        q[2] = (m.m[2][0] + m.m[0][2]) / s;
    } else if (m.m[1][1] > m.m[2][2]) {
        float s = std::sqrt(1.0f + m.m[1][1] - m.m[0][0] - m.m[2][2]) * 2;
        q[3] = (m.m[2][0] - m.m[0][2]) / s;
        q[0] = (m.m[1][0] + m.m[0][1]) / s;
        q[1] = 0.25f * s;
        q[2] = (m.m[2][1] + m.m[1][2]) / s;
    } else {
        float s = std::sqrt(1.0f + m.m[2][2] - m.m[0][0] - m.m[1][1]) * 2;
        q[3] = (m.m[0][1] - m.m[1][0]) / s;
        q[0] = (m.m[2][0] + m.m[0][2]) / s;
        q[1] = (m.m[2][1] + m.m[1][2]) / s;
        q[2] = 0.25f * s;
    }
}

void Slerp(const float a[4], const float b0[4], float t, float out[4]) {
    float b[4] = {b0[0], b0[1], b0[2], b0[3]};
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (d < 0) { d = -d; for (float& v : b) v = -v; }
    float wa = 1 - t, wb = t;
    if (d < 0.9995f) {
        const float th = std::acos(d), s = std::sin(th);
        wa = std::sin((1 - t) * th) / s;
        wb = std::sin(t * th) / s;
    }
    float len = 0;
    for (int i = 0; i < 4; i++) { out[i] = wa * a[i] + wb * b[i]; len += out[i] * out[i]; }
    len = std::sqrt(len);
    if (len > 1e-6f) for (int i = 0; i < 4; i++) out[i] /= len;
}

// RwMatrixInvert for an affine RW matrix.
RwMat Invert(const RwMat& a) {
    const float (*m)[3] = a.m;
    const float c00 = m[1][1] * m[2][2] - m[1][2] * m[2][1], c01 = m[1][2] * m[2][0] - m[1][0] * m[2][2],
                c02 = m[1][0] * m[2][1] - m[1][1] * m[2][0];
    const float det = m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02;
    const float id = std::fabs(det) > 1e-12f ? 1.0f / det : 0.0f;
    RwMat r;
    r.m[0][0] = c00 * id; r.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * id;
    r.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * id;
    r.m[1][0] = c01 * id; r.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * id;
    r.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * id;
    r.m[2][0] = c02 * id; r.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * id;
    r.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * id;
    float t[3];
    r.ApplyDir(m[3], t);
    for (int k = 0; k < 3; k++) r.m[3][k] = -t[k];
    return r;
}

// CMatrix::ConvertToEulerAngles / ConvertFromEulerAngles @ 0059A840 / 0059AA40.
// The 3x3 rotation is read row-wise (right, up, at); kEulerNext/kEulerAxis are
// the exe's tables at 0x866D94 / 0x866D9C.
constexpr uint8_t kEulerNext[8] = {1, 2, 0, 1, 0, 0, 0, 0};
constexpr uint8_t kEulerAxis[4] = {0, 1, 2, 0};

void ToEuler(const RwMat& mat, float& x, float& y, float& z, unsigned flags) {
    float m[9];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) m[r * 3 + c] = mat.m[r][c];
    const unsigned i = kEulerAxis[(flags >> 3) & 3], neg = (flags >> 2) & 1;
    const unsigned k = kEulerNext[i - neg + 1], j = kEulerNext[neg + i];
    if ((flags >> 1) & 1) {
        const float s = std::sqrt(m[i * 3 + k] * m[i * 3 + k] + m[i * 3 + j] * m[i * 3 + j]);
        if (s > 1.9073486e-06f) {
            x = std::atan2(m[i * 3 + j], m[i * 3 + k]);
            y = std::atan2(s, m[i * 4]);
            z = std::atan2(m[i + j * 3], -m[i + k * 3]);
        } else {
            x = std::atan2(-m[k + j * 3], m[j * 4]);
            y = std::atan2(s, m[i * 4]);
            z = 0.0f;
        }
    } else {
        const float c = std::sqrt(m[i * 4] * m[i * 4] + m[j * 3 + i] * m[j * 3 + i]);
        if (c > 1.9073486e-06f) {
            x = std::atan2(m[j + k * 3], m[k * 4]);
            y = std::atan2(-m[k * 3 + i], c);
            z = std::atan2(m[j * 3 + i], m[i * 4]);
        } else {
            x = std::atan2(-m[j * 3 + k], m[j * 4]);
            y = std::atan2(-m[i + k * 3], c);
            z = 0.0f;
        }
    }
    if (neg) { x = -x; y = -y; z = -z; }
    if (flags & 1) std::swap(x, z);
}

void FromEuler(RwMat& mat, float x, float y, float z, unsigned flags) {   // rotation only
    const unsigned i = kEulerAxis[(flags >> 3) & 3], neg = (flags >> 2) & 1;
    const unsigned j = kEulerNext[neg + i], k = kEulerNext[i - neg + 1];
    if (flags & 1) std::swap(x, z);
    if (neg) { x = -x; y = -y; z = -z; }
    const float cx = std::cos(x), cy = std::cos(y), cz = std::cos(z);
    const float sx = std::sin(x), sy = std::sin(y), sz = std::sin(z);
    const float cc = cz * cx, cs = cx * sz, sc = sx * cz, ss = sx * sz;
    float m[9];
    if ((flags >> 1) & 1) {
        m[i * 4] = cy;
        m[i * 3 + j] = sy * sx;
        m[i * 3 + k] = sy * cx;
        m[j * 3 + i] = sz * sy;
        m[j * 4] = cc - ss * cy;
        m[j * 3 + k] = -(cs * cy) - sc;
        m[i + k * 3] = -(sy * cz);
        m[k * 3 + j] = sc * cy + cs;
        m[k * 4] = cc * cy - ss;
    } else {
        m[i * 4] = cz * cy;
        m[i * 3 + j] = sc * sy - cs;
        m[i * 3 + k] = cc * sy + ss;
        m[j * 3 + i] = sz * cy;
        m[j * 4] = ss * sy + cc;
        m[j * 3 + k] = cs * sy - sc;
        m[i + k * 3] = -sy;
        m[k * 3 + j] = sx * cy;
        m[k * 4] = cy * cx;
    }
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) mat.m[r][c] = m[r * 3 + c];
}

const TxdTexture* FindTex(const std::vector<TxdTexture>& txd, const char* name) {
    for (const TxdTexture& t : txd)
        if (!name || Lower(t.name) == name) return &t;
    return nullptr;
}

// CClothesBuilder::PlaceTextureOnTopOfTexture: clothing over skin by the
// clothing texel's alpha. 32-bit BGRA in D3D byte order; base alpha forced
// opaque (X8R8G8B8 rasters carry no alpha).
std::vector<uint8_t> Composite(const TxdTexture& base, const TxdTexture* over) {
    std::vector<uint8_t> px = base.mip0;
    for (size_t i = 3; i < px.size(); i += 4) px[i] = 0xFF;
    if (!over || over->d3dFormat != D3DFMT_A8R8G8B8) return px;
    for (uint32_t y = 0; y < base.height; y++)
        for (uint32_t x = 0; x < base.width; x++) {
            const uint32_t ox = x * over->width / base.width, oy = y * over->height / base.height;
            const uint8_t* s = &over->mip0[(oy * over->width + ox) * 4];
            uint8_t* dpx = &px[(y * base.width + x) * 4];
            const unsigned a = over->hasAlpha ? s[3] : 255;
            for (int c = 0; c < 3; c++) dpx[c] = (uint8_t)((s[c] * a + dpx[c] * (255 - a)) / 255);
        }
    return px;
}

}  // namespace

const AnimClip* PlayerPed::Clip(const char* name) const {
    auto it = m_clips.find(name);
    return it == m_clips.end() ? nullptr : &it->second;
}

const char* PlayerPed::ClipName() const {   // the dominant association
    const Assoc* best = nullptr;
    for (const Assoc& a : m_assocs)
        if (!best || a.blend > best->blend) best = &a;
    return best ? best->clip->name.c_str() : "-";
}

bool PlayerPed::Init(D3DRenderer& r, const std::string& gameDir) {
    ImgLoader img;
    if (!img.Open(gameDir + "\\models\\player.img")) {
        std::fprintf(stderr, "[CJ] player.img not found\n");
        return false;
    }
    if (!IfpLoader::Load(gameDir + "\\anim\\ped.ifp", m_clips)) {
        std::fprintf(stderr, "[CJ] anim\\ped.ifp failed to load\n");
        return false;
    }
    {   // the skydive steering clips (parachute.ifp, in anim\anim.img)
        ImgLoader anims;
        if (anims.Open(gameDir + "\\anim\\anim.img")) {
            IfpLoader::LoadFromMemory(anims.Extract("parachute.ifp"), m_clips, true);
            IfpLoader::LoadFromMemory(anims.Extract("swim.ifp"), m_clips, true);   // CTaskSimpleSwim
            for (const char* g : {"colt45", "silenced", "python", "shotgun", "buddy", "uzi", "rifle", "sniper"})   // weapon groups
                IfpLoader::LoadFromMemory(anims.Extract(std::string(g) + ".ifp"), m_clips, true);
        }
    }
    auto txd = [&](const char* name) {
        std::vector<uint8_t> b = img.Extract(std::string(name) + ".txd");
        return b.empty() ? std::vector<TxdTexture>() : TxdLoader::LoadFromMemory(b.data(), b.size());
    };

    // Default outfit (white vest, jeans, black sneakers) on the default body.
    struct TexDef { const char* baseTxd; const char* baseTex; const char* overTxd; const char* overTex; };
    const TexDef texDefs[4] = {
        {"player_torso", "torso", "vest", "vest"},      // "torso" material
        {"player_face", "face", nullptr, nullptr},      // "head" (no head texture key -> player_face)
        {"player_legs", "legs", "jeansdenim", "legs"},  // "legs"
        {"sneakerbincblk", nullptr, nullptr, nullptr},  // "feet"
    };
    for (const TexDef& t : texDefs) {
        std::vector<TxdTexture> base = txd(t.baseTxd), over;
        const TxdTexture* b = FindTex(base, t.baseTex);
        const TxdTexture* o = nullptr;
        if (t.overTxd) { over = txd(t.overTxd); o = FindTex(over, t.overTex); }
        IDirect3DTexture9* tex = nullptr;
        if (b && b->d3dFormat == D3DFMT_A8R8G8B8) {
            std::vector<uint8_t> px = Composite(*b, o);
            tex = r.CreateTexture(b->width, b->height, D3DFMT_A8R8G8B8, px.data(), (uint32_t)px.size());
        }
        if (!tex) std::fprintf(stderr, "[CJ] texture %s failed\n", t.baseTxd);
        m_textures.push_back(tex);
    }

    // The base clump (models\gta3.img player.dff) owns the bone hierarchy and
    // the skin-to-bone matrices; CreateSkinnedClump only takes geometry and
    // weights from the clothing parts and re-indexes their bones onto it.
    SkinnedClump base;
    {
        ImgLoader gta3;
        std::vector<uint8_t> b;
        if (gta3.Open(gameDir + "\\models\\gta3.img")) b = gta3.Extract("player.dff");
        for (SkinnedClump& k : SkinnedDff::LoadClumps(b.data(), b.size()))
            if (k.valid) { base = std::move(k); break; }
    }
    if (!base.valid || base.inverseBind.size() < base.nodeIds.size()) {
        std::fprintf(stderr, "[CJ] gta3.img player.dff failed\n");
        return false;
    }
    m_skeleton = base.frames;
    m_frameOfBone.assign(512, -1);
    for (size_t f = 0; f < m_skeleton.size(); f++)
        if (m_skeleton[f].boneId >= 0 && m_skeleton[f].boneId < 512)
            m_frameOfBone[m_skeleton[f].boneId] = (int)f;
    std::vector<int> nodeOfBone(512, -1);
    for (size_t n = 0; n < base.nodeIds.size(); n++)
        if (base.nodeIds[n] >= 0 && base.nodeIds[n] < 512) nodeOfBone[base.nodeIds[n]] = (int)n;

    // SkinGetBonePositionsToTable: a bone's rest offset from its parent comes
    // from the skin matrices, not the frame list; clips without translation
    // keys (most bones) use it.
    for (size_t n = 1; n < base.nodeIds.size(); n++) {
        const int f = m_frameOfBone[base.nodeIds[n]];
        if (f < 0) continue;
        int pf = m_skeleton[f].parent, pn = -1;
        while (pf >= 0 && (pn = m_skeleton[pf].boneId >= 0 ? nodeOfBone[m_skeleton[pf].boneId] : -1) < 0)
            pf = m_skeleton[pf].parent;
        const float zero[3] = {0, 0, 0};
        float bonePos[3], local[3];
        Invert(base.inverseBind[n]).Apply(zero, bonePos);
        base.inverseBind[pn >= 0 ? pn : 0].Apply(bonePos, local);
        for (int k = 0; k < 3; k++) m_skeleton[f].local.m[3][k] = local[k];
    }

    // CreateSkinnedClump part order and the material texture each part uses.
    struct PartDef { const char* dff; int tex; };
    const PartDef partDefs[] = {{"vest", 0}, {"head", 1}, {"hands", 0}, {"jeans", 2}, {"sneaker", 3}};
    for (const PartDef& pd : partDefs) {
        std::vector<uint8_t> b = img.Extract(std::string(pd.dff) + ".dff");
        std::vector<SkinnedClump> clumps = SkinnedDff::LoadClumps(b.data(), b.size());
        const SkinnedClump* c = nullptr;
        for (const SkinnedClump& k : clumps)
            if (k.valid && (!c || Lower(k.rootName) == "normal")) c = &k;
        if (!c) { std::fprintf(stderr, "[CJ] part %s failed\n", pd.dff); continue; }
        if (Lower(c->rootName) != "normal")
            std::fprintf(stderr, "[CJ] part %s: no Normal clump, using '%s'\n", pd.dff, c->rootName.c_str());

        // BuildBoneIndexConversionTable: part node -> bone id -> base node
        // (bones the base lacks, e.g. the "Nub" end bones, map to node 0).
        Part part;
        part.verts = c->vertices;
        // The Fat and Ripped (muscle) clumps share the Normal one's topology;
        // CClothesBuilder::BlendGeometry mixes their vertices by ratio.
        part.shape[0] = c->vertices;
        for (const SkinnedClump& k : clumps) {
            const std::string n = Lower(k.rootName);
            const int s = n == "fat" ? 1 : n == "ripped" ? 2 : 0;
            if (s && k.valid && k.vertices.size() == c->vertices.size()) part.shape[s] = k.vertices;
        }
        part.boneFrame.resize(c->nodeIds.size(), 0);
        part.inverseBind.resize(c->nodeIds.size());
        for (size_t n = 0; n < c->nodeIds.size(); n++) {
            const int id = c->nodeIds[n];
            int bn = (id >= 0 && id < 512) ? nodeOfBone[id] : -1;
            if (bn < 0) bn = 0;
            const int f = m_frameOfBone[base.nodeIds[bn]];
            part.boneFrame[n] = f >= 0 ? f : 0;
            part.inverseBind[n] = base.inverseBind[bn];
        }
        part.skinned.resize(part.verts.size());
        for (size_t v = 0; v < part.verts.size(); v++) {
            const SkinVertex& sv = part.verts[v];
            part.skinned[v] = {sv.pos[0], sv.pos[1], sv.pos[2], sv.normal[0], sv.normal[1], sv.normal[2],
                               0xFFFFFFFFu, sv.uv[0], sv.uv[1]};
            m_bindMinZ = std::min(m_bindMinZ, sv.pos[2]);
        }
        part.mesh = r.CreateMesh(part.skinned.data(), (uint32_t)part.skinned.size(),
                                 c->indices.data(), (uint32_t)c->indices.size());
        if (part.mesh) part.mesh->texture = m_textures[pd.tex];
        part.isHead = pd.tex == 1;
        m_parts.push_back(std::move(part));
    }
    std::fprintf(stderr, "[CJ] %zu parts, %zu frames, %zu clips, body '%s', feet z %.3f\n",
                 m_parts.size(), m_skeleton.size(), m_clips.size(),
                 m_skeleton.size() > 1 ? m_skeleton[1].name.c_str() : "-", m_bindMinZ);
    m_ready = m_parts.size() == 5 && Clip("idle_stance");
    if (m_ready) Add(Clip("idle_stance"), true, false);
    if (m_ready) LoadGuns(r, gameDir);
    return m_ready;
}

void PlayerPed::Shutdown(D3DRenderer& r) {
    ShutdownGuns(r);
    for (Part& p : m_parts) if (p.mesh) r.DestroyMesh(p.mesh);
    for (IDirect3DTexture9* t : m_textures) if (t) r.DestroyTexture(t);
    m_parts.clear();
    m_textures.clear();
    m_ready = false;
}

// ---- Blend associations (CAnimBlendAssociation / CAnimManager) ----

PlayerPed::Assoc* PlayerPed::Get(const AnimClip* c) {
    for (Assoc& a : m_assocs)
        if (a.clip == c && !a.dead) return &a;
    return nullptr;
}

// CAnimManager::AddAnimation: the clip at full blend, the others untouched.
PlayerPed::Assoc* PlayerPed::Add(const AnimClip* c, bool loop, bool movement) {
    if (!c) return nullptr;
    Assoc* a = Get(c);
    if (!a) {
        m_assocs.emplace_back();
        a = &m_assocs.back();
        a->clip = c;
    }
    a->time = a->prevTime = 0.0f;
    a->blend = 1.0f;
    a->delta = 0.0f;
    a->speed = 1.0f;
    a->loop = loop;
    a->movement = movement;
    a->keepRoot = false;
    a->playing = true;
    a->finished = false;
    return a;
}

// CAnimManager::BlendAnimation: fade the clip in at `delta` per second and
// every other association out at the same rate.
PlayerPed::Assoc* PlayerPed::Blend(const AnimClip* c, bool loop, bool movement, float delta) {
    if (!c) return nullptr;
    Assoc* a = Get(c);
    for (Assoc& o : m_assocs)
        if (&o != a && !o.dead) o.delta = -delta;
    if (!a) {
        a = Add(c, loop, movement);
        a->blend = 0.0f;
    }
    a->delta = delta;
    a->playing = true;
    return a;
}

void PlayerPed::Remove(const AnimClip* c) {
    if (Assoc* a = Get(c)) { a->dead = true; a->blend = 0.0f; }
}

// Root (bone 0) translation of a clip at a time: the part retail extracts
// into the ped's velocity.
float PlayerPed::RootY(const AnimClip* clip, float time, float* x) const {
    const AnimSequence* s = clip ? clip->FindBone(0) : nullptr;
    if (x) *x = 0.0f;
    if (!s || !s->hasTranslation || s->keys.empty()) return 0.0f;
    const std::vector<AnimKey>& keys = s->keys;
    size_t k = 0;
    while (k + 1 < keys.size() && keys[k + 1].time <= time) k++;
    const AnimKey& a = keys[k];
    const AnimKey& b = keys[std::min(k + 1, keys.size() - 1)];
    const float span = b.time - a.time;
    const float t = span > 1e-5f ? std::clamp((time - a.time) / span, 0.0f, 1.0f) : 0.0f;
    if (x) *x = a.t[0] + (b.t[0] - a.t[0]) * t;
    return a.t[1] + (b.t[1] - a.t[1]) * t;
}

// RpAnimBlendClumpUpdateAnimations: movement clips advance phase-locked at
// the blend-weighted mean length; blends move by their delta.
void PlayerPed::Advance(float dt) {
    float totalLen = 0.0f, totalMoveBlend = 0.0f, totalBlend = 0.0f;
    for (const Assoc& a : m_assocs) {
        if (a.dead || a.partial) continue;
        totalBlend += a.blend;
        if (a.movement && a.blend > 0.0f) {
            totalLen += a.clip->duration * a.blend;
            totalMoveBlend += a.blend;
        }
    }
    const float meanLen = totalMoveBlend > 0.0f ? totalLen / totalMoveBlend : 0.0f;
    m_rootDX = m_rootDY = 0.0f;
    for (Assoc& a : m_assocs) {
        if (a.dead) continue;
        a.prevTime = a.time;
        const float dur = a.clip->duration;
        if (a.playing && dur > 0.0f) {
            float step = dt * a.speed;
            if (a.movement && meanLen > 0.0f) step *= dur / meanLen;
            float t = a.time + step;
            bool wrapped = false;
            if (a.loop) {
                if (t >= dur) { t = std::fmod(t, dur); wrapped = true; }
            } else if (t >= dur) {
                t = dur;
                a.finished = true;
                if (a.autoRemove) a.dead = true;
            }
            a.time = t;
            if (totalBlend > 0.0f && a.blend > 0.0f && !a.keepRoot && !a.partial) {
                float x0, x1, xe, xs;
                const float y0 = RootY(a.clip, a.prevTime, &x0), y1 = RootY(a.clip, t, &x1);
                float dy = y1 - y0, dx = x1 - x0;
                if (wrapped) {
                    const float ye = RootY(a.clip, dur, &xe), ys = RootY(a.clip, 0.0f, &xs);
                    dy = (ye - y0) + (y1 - ys);
                    dx = (xe - x0) + (x1 - xs);
                }
                m_rootDX += dx * a.blend / totalBlend;
                m_rootDY += dy * a.blend / totalBlend;
            }
        }
        a.blend = std::clamp(a.blend + a.delta * dt, 0.0f, 1.0f);
        if (a.blend <= 0.0f && a.delta < 0.0f) a.dead = true;
    }
    for (auto it = m_assocs.begin(); it != m_assocs.end();)
        it = it->dead ? m_assocs.erase(it) : it + 1;
}

// The blended pose: per bone, the weighted quaternion sum (signs aligned,
// normalised) and weighted translation, normalised by the total weight.
void PlayerPed::BuildPose() {
    const size_t n = m_skeleton.size();
    m_pose.resize(n);
    for (size_t f = 0; f < n; f++) {
        Pose& out = m_pose[f];
        float q[4] = {0, 0, 0, 0}, t[3] = {0, 0, 0}, w = 0.0f;
        // Partial overlays (weapon aim / reload) take their share of this bone;
        // the full-body clips share what is left.
        float wPart = 0.0f, wFull = 0.0f;
        if (m_skeleton[f].boneId >= 0)
            for (const Assoc& a : m_assocs) {
                if (a.dead || a.blend <= 0.0f) continue;
                const AnimSequence* s = a.clip->FindBone(m_skeleton[f].boneId);
                if (!s || s->keys.empty()) continue;
                (a.partial ? wPart : wFull) += a.blend;
            }
        const float partScale = wPart > 1.0f ? 1.0f / wPart : 1.0f;
        const float fullScale = wFull > 0.0f ? std::max(0.0f, 1.0f - std::min(1.0f, wPart)) / wFull : 0.0f;
        for (const Assoc& a : m_assocs) {
            if (a.dead || a.blend <= 0.0f || m_skeleton[f].boneId < 0) continue;
            const AnimSequence* s = a.clip->FindBone(m_skeleton[f].boneId);
            if (!s || s->keys.empty()) continue;
            const float bw = a.blend * (a.partial ? partScale : (wPart > 0.0f ? fullScale : 1.0f));
            if (bw <= 0.0f) continue;
            const std::vector<AnimKey>& keys = s->keys;
            size_t k = 0;
            while (k + 1 < keys.size() && keys[k + 1].time <= a.time) k++;
            const AnimKey& ka = keys[k];
            const AnimKey& kb = keys[std::min(k + 1, keys.size() - 1)];
            const float span = kb.time - ka.time;
            const float u = span > 1e-5f ? std::clamp((a.time - ka.time) / span, 0.0f, 1.0f) : 0.0f;
            float sq[4];
            Slerp(ka.q, kb.q, u, sq);
            float st[3];
            for (int i = 0; i < 3; i++)
                st[i] = s->hasTranslation ? ka.t[i] + (kb.t[i] - ka.t[i]) * u : m_skeleton[f].local.m[3][i];
            if (m_skeleton[f].boneId == 0 && !a.keepRoot) { st[0] = 0.0f; st[1] = 0.0f; }   // extracted root motion
            const float dot = q[0] * sq[0] + q[1] * sq[1] + q[2] * sq[2] + q[3] * sq[3];
            const float sign = (w > 0.0f && dot < 0.0f) ? -1.0f : 1.0f;
            for (int i = 0; i < 4; i++) q[i] += sq[i] * bw * sign;
            for (int i = 0; i < 3; i++) t[i] += st[i] * bw;
            w += bw;
        }
        if (w <= 0.0f) {   // no clip drives this bone: bind pose
            RowsToQuat(m_skeleton[f].local, out.q);
            for (int i = 0; i < 3; i++) out.t[i] = m_skeleton[f].local.m[3][i];
            continue;
        }
        const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        for (int i = 0; i < 4; i++) out.q[i] = len > 1e-6f ? q[i] / len : (i == 3 ? 1.0f : 0.0f);
        for (int i = 0; i < 3; i++) out.t[i] = t[i] / w;
    }
}

void PlayerPed::BuildGlobals(const std::vector<Pose>& pose) {
    m_globals.assign(m_skeleton.size(), RwMat());
    for (size_t f = 1; f < m_skeleton.size(); f++) {   // frame 0 = clump root = skin space
        RwMat local;
        QuatToRows(pose[f].q, local);
        for (int k = 0; k < 3; k++) local.m[3][k] = pose[f].t[k];
        const int p = m_skeleton[f].parent;
        m_globals[f] = (p > 0 && p < (int)f) ? local * m_globals[p] : local;
    }
}

// CPed::ShoulderBoneRotation @ 005DF560, run on every skinned ped after its
// animation: the breast bones (301/302) are really shoulder helpers. Each is
// set to its upper arm's matrix with the first Euler angle (order 0x15) of the
// arm's rotation relative to the clavicle halved, so the shoulder skin
// follows the arm half way.
void PlayerPed::ShoulderBoneRotation() {
    struct Side { int breast, upperArm, clavicle; };
    for (const Side& sd : {Side{302, 32, 31}, Side{301, 22, 21}}) {
        const int fb = m_frameOfBone[sd.breast], fu = m_frameOfBone[sd.upperArm], fc = m_frameOfBone[sd.clavicle];
        if (fb < 0 || fu < 0 || fc < 0) continue;
        RwMat local = m_globals[fu] * Invert(m_globals[fc]);   // the arm relative to the clavicle
        float x, y, z;
        ToEuler(local, x, y, z, 0x15);
        FromEuler(local, x * 0.5f, y, z, 0x15);
        m_globals[fb] = local * m_globals[fc];
    }
}

// ---- On foot ----

// CTaskSimplePlayerOnFoot::PlayerControlZelda @ 006883d0 (movement part) and
// the heading step of CPed::CalculateNewVelocity @ 005e4c50.
void PlayerPed::ControlOnFoot(const Input& in) {
    float sx = in.stickX, sy = in.stickY;
    if (in.rewrite) m_move.FilterStick(in.dt, sx, sy, in.walk, in.sprint, in.blocked);
    m_stickX = sx;
    m_stickY = sy;
    const float mag = std::min(1.0f, std::sqrt(sx * sx + sy * sy));
    // The pad reports +-128; PlayerControlZelda divides by 60, so a full
    // stick asks for a move blend ratio of 2.13 (run). PedWalk caps it at 1.
    float pad = mag * 128.0f / 60.0f;
    if (in.walk && pad > 1.0f) pad = 1.0f;
    if (in.firstPerson) m_aimHeading = in.camYaw;   // the body faces where you look
    if (pad > 0.0f) {
        m_moveAngle = std::atan2(sy, sx);
        if (!in.firstPerson) m_aimHeading = m_moveAngle;
        const float step = 0.07f * 50.0f * in.dt;   // 0.07 per 1/50 s timestep
        const float d = pad - m_ratio;
        m_ratio = d > step ? m_ratio + step : d < -step ? m_ratio - step : pad;
    } else {
        m_ratio = 0.0f;
    }
    SetRealMoveAnim(in.sprint && pad > 0.0f);

    if (in.rewrite) {
        const bool moving = mag >= m_move.p.minimumMovementInput || Active(Clip("walk_player")) ||
                            Active(Clip("run_player")) || Active(Clip("sprint_civi"));
        m_heading = m_move.Heading(in.dt, m_heading, m_aimHeading, moving);
    } else {
        // STAT_PLAYER heading change rate: 9 degrees per 1/50 s timestep.
        float d = m_aimHeading - m_heading;
        while (d > kPi) d -= 2 * kPi;
        while (d < -kPi) d += 2 * kPi;
        const float step = 9.0f * (kPi / 180.0f) * 50.0f * m_headingRate * in.dt;
        m_heading += std::clamp(d, -step, step);
    }
    while (m_heading > kPi) m_heading -= 2 * kPi;
    while (m_heading < -kPi) m_heading += 2 * kPi;
}

// CPlayerPed::SetRealMoveAnim @ 0060a9c0: walk / run weights from the move
// blend ratio, the walk start, sprint in and out, and the run stop.
void PlayerPed::SetRealMoveAnim(bool sprintState) {
    const AnimClip *idle = Clip("idle_stance"), *walk = Clip("walk_player"), *run = Clip("run_player"),
                   *sprint = Clip("sprint_civi"), *walkStart = Clip("walk_start"),
                   *runStop = Clip("run_stop"), *runStopR = Clip("run_stopr");
    Assoc* aIdle = Get(idle);
    Assoc* aWalk = Get(walk);
    Assoc* aRun = Get(run);
    Assoc* aSprint = Get(sprint);
    Assoc* aStart = Get(walkStart);
    Assoc* aStop = Get(runStop);
    if (!aStop) aStop = Get(runStopR);

    if (aStop && !aStop->finished) return;   // skidding to a halt
    if (aStop && aStop->delta >= 0.0f) {     // the stop is done: back to idle
        m_headingRate = 1.0f;                // CPed::RestoreHeadingRate
        Assoc* i = aIdle ? aIdle : Blend(idle, true, false, 8.0f);
        i->blend = 0.0f;
        i->delta = 8.0f;
        aStop->delta = -8.0f;
        return;
    }

    if (m_ratio != 0.0f || aSprint) {
        if (aIdle) {   // setting off from a standstill: WALK_start first
            aStart = Add(walkStart, false, false);
            if (aWalk) aWalk->time = 0.0f;
            if (aRun) aRun->time = 0.0f;
            Remove(idle);
            if (aSprint) { Remove(sprint); aSprint = nullptr; }
        }
        if (!aWalk) { aWalk = Add(walk, true, true); aWalk->blend = 0.0f; }
        if (!aRun) { aRun = Add(run, true, true); aRun->blend = 0.0f; }
        if (aStart && aStart->finished) {
            Remove(walkStart);
            aStart = nullptr;
            aWalk->playing = aRun->playing = true;
        }
        if (sprintState && aStart) sprintState = false;

        if (!aSprint || (sprintState && m_ratio >= 0.4f)) {
            if (aStart) {   // walk/run wait for the walk start to finish
                aWalk->playing = aRun->playing = false;
                aWalk->blend = aRun->blend = 0.0f;
                return;
            }
            aWalk->playing = aRun->playing = true;
            if (sprintState) {
                if (!aSprint) {
                    if (aRun->blend < 1.0f) {   // reach a full run before sprinting
                        if (aWalk->blend == 0.0f && aRun->blend == 0.0f) aWalk->blend = 1.0f;
                        if (aRun->delta <= 0.0f) aRun = Blend(run, true, true, 4.0f);
                        m_ratio = aRun->delta + 1.0f;
                        return;
                    }
                    Blend(sprint, true, true, 2.0f);
                } else if (aSprint->delta < 0.0f) {
                    aSprint->delta = 2.0f;
                    aRun->delta = -2.0f;
                }
                return;
            }
            if (m_ratio >= 1.0f) {
                aWalk->blend = m_ratio >= 2.0f ? 0.0f : 2.0f - m_ratio;
                aRun->blend = m_ratio >= 2.0f ? 1.0f : m_ratio - 1.0f;
            } else {
                aWalk->blend = 1.0f;
                aRun->blend = 0.0f;
            }
            aWalk->delta = aRun->delta = 0.0f;
            return;
        }
        // Sprint released (or the stick let go) while the sprint is in.
        if (aSprint->blend == 0.0f) {
            Remove(sprint);
        } else if (aSprint->delta >= 0.0f || aSprint->blend >= 0.8f) {
            if (m_ratio >= 0.4f) {
                if (aSprint->delta >= 0.0f) { aSprint->delta = -1.0f; aRun->delta = 1.0f; }
            } else {   // RUN_STOP / RUN_STOPR by which foot is down
                const bool second = aSprint->time / aSprint->clip->duration >= 0.5f;
                Add(second ? runStopR : runStop, false, false);
                m_headingRate = 0.0f;
                Remove(sprint);
                aWalk->playing = aRun->playing = false;
                aWalk->blend = aRun->blend = 0.0f;
                aWalk->delta = aRun->delta = 0.0f;
            }
        } else if (m_ratio < 1.0f) {
            aSprint->delta = -8.0f;
            aRun->delta = 8.0f;
        }
        return;
    }
    if (!aIdle) Blend(idle, true, false, 4.0f);   // standing still
}

// ---- In the air ----

// CTaskSimpleInAir::ProcessPed @ 00680600 (anim choice) and CTaskSimpleJump.
// The skydive pose is the parachute script's (main.scm), approximated.
void PlayerPed::ControlInAir(const Input& in) {
    const AnimClip *launch = Clip("jump_launch"), *jumpGlide = Clip("jump_glide"),
                   *fallGlide = Clip("fall_glide"), *fall = Clip("fall_fall"), *sky = Clip("fall_skydive");
    if (!m_inAir) {   // take-off: keep the ground speed (no air control)
        m_inAir = true;
        m_jumping = in.jumped;
        m_minVelZ = 0.0f;
        m_airTime = 0.0f;
        m_airVelX = m_velX;
        m_airVelY = m_velY;
        m_busy = false;
        m_busyNext = nullptr;
        if (in.jumped) Blend(launch, false, false, 8.0f);
    }
    m_airTime += in.dt;
    m_minVelZ = std::min(m_minVelZ, in.velZ);
    const bool falling = Active(fall) || Active(sky);
    if (m_jumping) {
        const Assoc* l = Get(launch);
        if ((!l || l->finished) && !Active(jumpGlide) && !falling) Blend(jumpGlide, true, false, 4.0f);
    } else if (in.heightAboveGround > 1.3f && !Active(fallGlide) && !falling) {
        Blend(fallGlide, true, false, 4.0f);   // walked off an edge
    }
    // No ground within 4 m and dropping faster than 0.1 per timestep (5 m/s).
    if (in.heightAboveGround > 4.0f && in.velZ < -5.0f && !falling) Blend(fall, true, false, 4.0f);
    if (Active(fall) && in.heightAboveGround > 35.0f && in.velZ < -15.0f) Blend(sky, true, false, 1.0f);
    m_velX = m_airVelX;
    m_velY = m_airVelY;
}

// CTaskComplexInAirAndLand::CreateNextSubTask @ 0067ccb0: how CJ lands.
void PlayerPed::Land(const Input& in) {
    const AnimClip *jumpGlide = Clip("jump_glide"), *fallGlide = Clip("fall_glide"), *fall = Clip("fall_fall"),
                   *sky = Clip("fall_skydive"), *launch = Clip("jump_launch");
    const Assoc* aSky = Get(sky);
    const Assoc* aFall = Get(fall);
    const bool airAnim = (aSky && aSky->blend > 0.0f) || (aFall && aFall->blend > 0.0f) ||
                         Get(jumpGlide) || Get(fallGlide) || Get(launch);
    m_inAir = false;
    if (!airAnim) return;   // a short drop: no in-air animation ever started
    auto busy = [&](const char* clip, const char* next, bool soft) {
        if (!Clip(clip)) return;
        Blend(Clip(clip), false, false, 8.0f);
        m_busy = true;
        m_busySoft = soft;
        m_busyClip = Clip(clip);
        m_busyNext = next ? Clip(next) : nullptr;
        m_ratio = 0.0f;
    };
    if (aSky && aSky->blend > 0.5f) {
        // FALL_skyDive_DIE starts from the skydive pose and ends face down
        // (retail kills CJ here; without health he gets back up).
        busy(Clip("fall_skydive_die") ? "fall_skydive_die" : "ko_skid_front", "getup_front", false);
    } else if (aFall && aFall->blend > 0.5f) {
        // m_fMinZSpeed >= -0.4 per timestep (-20 m/s): FALL_COLLAPSE, else a KO skid.
        if (m_minVelZ >= -20.0f) busy("fall_collapse", nullptr, false);
        else busy("ko_skid_back", "getup_front", false);   // ends in getup_front's first pose
    } else {
        const float mag = std::sqrt(in.stickX * in.stickX + in.stickY * in.stickY);
        if (m_ratio > 1.5f && mag > 0.0f) {   // running: land on the move, keep the stride
            for (const AnimClip* c : {launch, jumpGlide, fallGlide, fall}) {
                if (Assoc* a = Get(c)) a->delta = -8.0f;
            }
            // Bring the move anims back at once: the air clips replaced them and the
            // run would otherwise start again from a standstill.
            const AnimClip *walk = Clip("walk_player"), *run = Clip("run_player"), *sprint = Clip("sprint_civi");
            Assoc* aWalk = Get(walk);
            Assoc* aRun = Get(run);
            if (!aWalk) { aWalk = Add(walk, true, true); }
            if (!aRun) { aRun = Add(run, true, true); }
            if (aWalk && aRun) {
                aWalk->blend = 0.0f; aWalk->delta = 0.0f; aWalk->playing = true;
                aRun->blend = 1.0f; aRun->delta = 0.0f; aRun->playing = true;
            }
            if (Assoc* aSp = Get(sprint)) aSp->delta = -8.0f;
            if (in.sprint && Clip("sprint_civi")) {
                Assoc* sp = Add(sprint, true, true);
                sp->blend = 1.0f;
                if (aRun) aRun->blend = 0.0f;
            }
            m_landClip = Clip("jump_land");
            if (m_landClip) {   // layered over the run, not replacing it
                Assoc* l = Add(m_landClip, false, false);
                l->blend = 0.5f;
            }
        } else {
            busy("fall_land", nullptr, true);
        }
    }
}

// ---- CTaskSimpleClimb ----
// eClimbHeights: the clip wanted (m_HeightForAnim) and where CJ is (m_HeightForPos).
namespace { enum { CNR = 0, CGRAB, CPULL, CSTAND, CFIN, CVAULT, CFINV }; }

// Handhold offsets (008d2f1c..008d2f30, c18f78): horizontal (along the facing,
// negative = back from the ledge) and vertical (ped origin above the handhold).
static void ClimbOffsets(int h, float& horz, float& vert) {
    switch (h) {
    case CGRAB: case CPULL: horz = -0.4f; vert = -1.1f; break;
    case CSTAND: case CVAULT: horz = -0.4f; vert = 0.0f; break;
    case CFIN: horz = 0.15f; vert = 1.0f; break;
    default: horz = 0.5f; vert = 0.0f; break;   // CFINV
    }
}

bool PlayerPed::TryStartClimb(float x, float y, float feetZ, bool launch, const ClimbScanFn& scan) {
    if (!m_ready || m_climbing || !scan) return false;
    const float rot = m_heading - kPi * 0.5f;
    const float ped[3] = {x, y, feetZ + 1.0f};
    float hold[3], hrot = rot;
    if (!scan(launch ? 1 : 0, ped, rot, hold, &hrot)) return false;
    // TestForClimb: there must be room to stand on the ledge.
    const float p2[3] = {hold[0] + std::sin(hrot) * 0.4f, hold[1] - std::cos(hrot) * 0.4f, hold[2]};
    float dummy[3], dr = hrot;
    if (scan(2, p2, hrot, dummy, &dr)) return false;
    for (int i = 0; i < 3; i++) { m_cHold[i] = hold[i]; m_cPed[i] = ped[i]; }
    m_cRot = hrot;
    m_cScan = scan;
    m_climbing = true;
    m_cFinished = m_cChangeAnim = m_cChangePos = false;
    // CTaskComplexJump / CTaskComplexInAirAndLand: a ledge close enough to reach skips the hang.
    m_cHAnim = hold[2] - ped[2] < (launch ? 1.4f : 1.1f) ? CPULL : CGRAB;
    m_cHPos = CNR;
    m_cClip = nullptr;
    m_cCounter = 0.0f;
    m_cSnapValid = m_cRelValid = false;
    m_cVelZ = 0.0f;
    m_busy = true; m_busySoft = false; m_busyClip = nullptr; m_busyNext = nullptr;
    m_landClip = nullptr; m_inAir = false; m_jumping = false; m_ratio = 0.0f;
    m_move.Reset();
    m_heading = m_aimHeading = m_cRot + kPi * 0.5f;
    ClimbStartAnim();
    return true;
}

bool PlayerPed::TakeClimbSnap(float& x, float& y, float& feetZ) {
    if (!m_cSnapValid) return false;
    m_cSnapValid = false;
    x = m_cSnap[0]; y = m_cSnap[1]; feetZ = m_cSnap[2] - 1.0f;
    return true;
}

bool PlayerPed::TakeClimbRelease(float v[3]) {
    if (!m_cRelValid) return false;
    m_cRelValid = false;
    for (int i = 0; i < 3; i++) v[i] = m_cRel[i];
    return true;
}

// CTaskSimpleClimb::StartAnim @ 0067dbe0
void PlayerPed::ClimbStartAnim() {
    auto go = [&](const char* name, bool loop, float rate) { m_cClip = Clip(name); if (m_cClip) { Assoc* b = Blend(m_cClip, loop, false, rate); if (b) {
            b->keepRoot = true;
            if (rate >= 1000.0f) {   // instant switch: the hold position snaps on this same frame, not a frame after the clip starts
                b->blend = 1.0f;
                for (Assoc& o : m_assocs) if (&o != b && !o.dead) o.blend = 0.0f;
            }
        } } };
    switch (m_cHAnim) {
    case CGRAB:
        if (!m_cClip) go("climb_jump", false, 8.0f);
        else go("climb_idle", true, 4.0f);   // hang
        m_cHPos = CGRAB; m_cChangePos = false;
        break;
    case CPULL:
        if (m_cHPos == CNR) {   // reached straight to the pull-up: hold the stand clip until the vault test decides
            m_cHAnim = m_cHPos = CSTAND; go("climb_stand", false, 4.0f); m_cChangePos = false;
            if (Assoc* c = Get(m_cClip)) c->playing = false;
        }
        else { go("climb_pull", false, 1000.0f); m_cHPos = CPULL; m_cChangePos = false; }
        break;
    case CSTAND:
        go("climb_stand", false, 1000.0f); m_cChangePos = true;
        break;
    case CFIN: case CFINV:
        m_ratio = 0.0f;
        go(m_cHAnim == CFINV ? "climb_jump2fall" : "climb_stand_finish", false, 1000.0f);
        m_cChangePos = true;
        break;
    case CVAULT: {
        const bool fromStand = m_cClip == Clip("climb_stand");
        go("climb_jump_b", false, fromStand ? 16.0f : 1000.0f);
        m_cChangePos = true;
        break;
    }
    default: break;
    }
    m_cChangeAnim = false;
}

// TestForVault (how = 3) / TestForStandUp (how = 2): true when the volume is free.
bool PlayerPed::ClimbTest(int mode, int) {
    const float p[3] = {m_cHold[0] + std::sin(m_cRot) * 0.4f, m_cHold[1] - std::cos(m_cRot) * 0.4f, m_cHold[2]};
    float o[3], r = m_cRot;
    return !m_cScan(mode, p, m_cRot, o, &r);
}

void PlayerPed::ClimbDebug(char* out, size_t n) {
    Assoc* a = Get(m_cClip);
    std::snprintf(out, n, "anim=%d pos=%d chg=%d/%d t=%.2f/%.2f bl=%.2f play=%d fin=%d", m_cHAnim, m_cHPos, (int)m_cChangeAnim, (int)m_cChangePos,
                  a ? a->time : -1.0f, a ? a->clip->duration : -1.0f, a ? a->blend : -1.0f, a ? (int)a->playing : -1, a ? (int)a->finished : -1);
}

// CTaskSimpleClimb::ProcessPed @ 00680dc0
void PlayerPed::ProcessClimb(const Input& in) {
    const float dt = in.dt;
    auto abort = [&]() {   // MakeAbortable: let go and fall
        m_climbing = false; m_busy = false; m_cClip = nullptr;
        for (Assoc& a : m_assocs) a.delta = -8.0f;
        m_cRelValid = true; m_cRel[0] = m_cRel[1] = m_cRel[2] = 0.0f;
        m_inAir = true;
    };
    m_cPed[0] = in.worldX; m_cPed[1] = in.worldY; m_cPed[2] = in.feetZ + 1.0f;
    if (!m_cClip || m_cChangeAnim) ClimbStartAnim();

    bool snapped = false;
    const Assoc* a = Get(m_cClip);
    if (m_cChangePos && a && a->blend >= 0.999f) { snapped = true; m_cChangePos = false; m_cHPos = m_cHAnim; }

    if (m_cHPos <= CNR || !snapped) {
        if (m_cHPos > CNR) {
            float horz, vert; ClimbOffsets(m_cHPos, horz, vert);
            const float t[3] = {m_cHold[0] - std::sin(m_cRot) * horz, m_cHold[1] + std::cos(m_cRot) * horz, m_cHold[2] + vert};
            const float d[3] = {t[0] - m_cPed[0], t[1] - m_cPed[1], t[2] - m_cPed[2]};
            const float d2 = d[0] * d[0] + d[1] * d[1] + d[2] * d[2];
            if (d2 >= 0.1f) {   // ease towards the handhold, at most 0.2 per frame (50 Hz)
                float v[3] = {d[0] * 0.25f / 0.02f, d[1] * 0.25f / 0.02f, d[2] * 0.25f / 0.02f};
                const float m = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if (m > 0.2f / 0.02f) for (float& c : v) c *= (0.2f / 0.02f) / m;
                m_velX = v[0]; m_velY = v[1]; m_cVelZ = v[2];
                if (!a || !a->loop || m_cClip == Clip("climb_idle")) {   // can't get there: give up
                    m_cCounter += dt * 1000.0f;
                    if (m_cCounter > (m_cClip == Clip("climb_idle") ? 500.0f : 1000.0f)) { abort(); return; }
                }
            } else {
                m_velX = d[0] / std::max(dt, 1e-4f); m_velY = d[1] / std::max(dt, 1e-4f); m_cVelZ = d[2] / std::max(dt, 1e-4f);
                Assoc* p = Get(m_cClip);   // at the ledge with the stand clip held: vault if the way over is clear
                if (p && !p->playing && m_cHAnim == CSTAND) {
                    if (ClimbTest(3, 3)) { m_cHAnim = CVAULT; m_cChangeAnim = true; }
                    else p->playing = true;
                }
            }
        }
    } else {   // arrived with the clip fully blended in: snap to the pose
        float horz, vert; ClimbOffsets(m_cHPos, horz, vert);
        m_cSnap[0] = m_cHold[0] - std::sin(m_cRot) * horz;
        m_cSnap[1] = m_cHold[1] + std::cos(m_cRot) * horz;
        m_cSnap[2] = m_cHold[2] + vert;
        m_cSnapValid = true;
        m_velX = m_velY = m_cVelZ = 0.0f;
        if (m_cHPos == CFIN || m_cHPos == CFINV) {   // done: back to normal control
            const float fx = -std::sin(m_cRot), fy = std::cos(m_cRot);
            m_cRelValid = true;
            if (m_cHPos == CFINV) { m_cRel[0] = fx * 0.02f * 50.0f; m_cRel[1] = fy * 0.02f * 50.0f; m_cRel[2] = -0.05f * 50.0f; }
            else { m_cRel[0] = fx * 0.05f * 50.0f; m_cRel[1] = fy * 0.05f * 50.0f; m_cRel[2] = -0.01f * 50.0f; }
            m_airVelX = m_cRel[0]; m_airVelY = m_cRel[1];
            m_climbing = false; m_busy = false;
            // Everything fades out, not only the last climb clip: with the move keys held the
            // walk/run clips come in at once and a clip left at full weight would freeze the body in its
            // final (flat) pose.
            for (Assoc& a : m_assocs) a.delta = -8.0f;
            m_cClip = nullptr;
            m_inAir = m_cHPos == CFINV;   // a vault lands through the fall
            return;
        }
    }

    // Which clip is playing decides what comes next.
    a = Get(m_cClip);
    if (a && a->blend >= 0.999f) {
        const bool done = a->finished || a->time >= a->clip->duration;
        if (m_cClip == Clip("climb_jump")) {
            if (done) {
                if (in.jumpHeld) { m_cHAnim += 1; m_cChangeAnim = true; ClimbStartAnim(); }
                else ClimbStartAnim();   // let go of the button: hang (CLIMB_IDLE)
            }
        } else if (m_cClip == Clip("climb_idle")) {
            if (in.jumpHeld) { m_cHAnim += 1; m_cChangeAnim = true; }   // pull up
        } else if (done) {   // pull, stand, vault clips: on to the next stage
            m_cHAnim += 1; m_cChangeAnim = true;
        }
    }
    if (in.exitPressed) { abort(); return; }

    a = Get(m_cClip);
    if ((m_cHPos != CSTAND && m_cHPos != CVAULT) || !a || !a->playing) {
        if (m_cHAnim == CSTAND && m_cHPos < CSTAND && ClimbTest(3, 3)) m_cHAnim = CVAULT;   // clear volume over the ledge: vault
    } else if (!ClimbTest(2, 2)) { abort(); return; }       // no room to stand: drop
    m_heading = m_aimHeading = m_cRot + kPi * 0.5f;
}

// ---- Rewrite pose layers ----

namespace {
// Rotation about a model-space axis through a pivot, as an RW (row vector)
// matrix: v' = (v - p) R + p.
RwMat AxisRotation(const float axis[3], float radians, const float pivot[3]) {
    const float c = std::cos(radians), s = std::sin(radians), t = 1.0f - c;
    const float x = axis[0], y = axis[1], z = axis[2];
    // column-vector Rodrigues matrix, transposed into RW rows
    const float m[3][3] = {{t * x * x + c, t * x * y - s * z, t * x * z + s * y},
                           {t * x * y + s * z, t * y * y + c, t * y * z - s * x},
                           {t * x * z - s * y, t * y * z + s * x, t * z * z + c}};
    RwMat r;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) r.m[i][j] = m[j][i];
    float rp[3];
    r.ApplyDir(pivot, rp);
    for (int k = 0; k < 3; k++) r.m[3][k] = pivot[k] - rp[k];
    return r;
}
}  // namespace

// Rewrite body pose: the ped matrix rolled by the lean and tipped by
// the pitch about the ped centre, the lower legs turned back so
// the shins stay upright and the head turned a little further
//. Model space: +Y forward, +X right, +Z up, origin = ped centre.
void PlayerPed::ApplyRewritePose() {
    const MovementParams& p = m_move.p;
    const float kDeg = kPi / 180.0f;
    const float lean = m_move.Lean(), pitch = m_move.Pitch() + m_aimPitch;
    if (std::fabs(lean) < p.minimumVisiblePoseDegrees && std::fabs(pitch) < p.minimumVisiblePoseDegrees) return;
    const float fwd[3] = {0, 1, 0}, right[3] = {1, 0, 0}, origin[3] = {0, 0, 0};
    const RwMat body = AxisRotation(fwd, lean * kDeg, origin) * AxisRotation(right, pitch * kDeg, origin);
    for (size_t f = 1; f < m_globals.size(); f++) m_globals[f] = m_globals[f] * body;

    // Lower legs (calf 42/52 and everything below it) turned back about the knee.
    for (int calfId : {42, 52}) {
        const int knee = m_frameOfBone[calfId];
        if (knee < 0) continue;
        const float pv[3] = {m_globals[knee].m[3][0], m_globals[knee].m[3][1], m_globals[knee].m[3][2]};
        const RwMat back = AxisRotation(right, -pitch * p.pitchCompensation * kDeg, pv) *
                           AxisRotation(fwd, -lean * p.bankCompensation * kDeg, pv);
        for (size_t f = 0; f < m_skeleton.size(); f++)
            for (int a = (int)f; a >= 0; a = m_skeleton[a].parent)
                if (a == knee) { m_globals[f] = m_globals[f] * back; break; }
    }

    // Head (bones 5..8) a little further into the lean and pitch.
    const int head = m_frameOfBone[5];
    if (head >= 0) {
        const float pv[3] = {m_globals[head].m[3][0], m_globals[head].m[3][1], m_globals[head].m[3][2]};
        const RwMat extra = AxisRotation(fwd, lean * p.headLeanInfluence * kDeg, pv) *
                            AxisRotation(right, pitch * p.headPitchInfluence * kDeg, pv);
        for (int id = 5; id <= 8; id++)
            if (m_frameOfBone[id] >= 0) m_globals[m_frameOfBone[id]] = m_globals[m_frameOfBone[id]] * extra;
    }
}

// Aim IK (stands in for CPedIK::PointGunInDirection): the gun arm is swung
// about the shoulder until shoulder-to-hand points along the aim direction
// (forward, tipped by the camera pitch).
void PlayerPed::ApplyAimIK(const Input& in) {
    if (!m_aiming || m_gun < 0) return;
    // CTaskSimpleUseGun::AimGun: a weapon without the arm flag (rifles,
    // shotguns, SMGs) or any gun while ducking is aimed with the body
    // (CPedIK::PointGunAtPosition tips the torso), the rest with the arm.
    if (!m_guns[m_gun].aimWithArm || m_ducking) {
        const int spine = m_frameOfBone[3];   // BONE_SPINE1: the torso, arms and head hang off it
        if (spine < 0) return;
        const float a = std::clamp(in.camPitch * 0.75f, -0.6f, 0.6f);
        if (std::fabs(a) < 1e-3f) return;
        const float right[3] = {1, 0, 0};
        const float pv[3] = {m_globals[spine].m[3][0], m_globals[spine].m[3][1], m_globals[spine].m[3][2]};
        const RwMat rot = AxisRotation(right, a, pv);
        for (size_t f = 0; f < m_skeleton.size(); f++)
            for (int p = (int)f; p >= 0; p = m_skeleton[p].parent)
                if (p == spine) { m_globals[f] = m_globals[f] * rot; break; }
        return;
    }
    // The arm gun aim (CIKChain PointArm): a two-bone solve that reaches the arm out along the
    // aim direction, the clip only supplies the bend side and the hand pose.
    const int fs = m_frameOfBone[22], fe = m_frameOfBone[23], fh = m_frameOfBone[24];
    if (fs < 0 || fe < 0 || fh < 0) return;
    auto sub = [](const float* a, const float* b, float* o) { for (int k = 0; k < 3; k++) o[k] = a[k] - b[k]; };
    auto len = [](const float* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); };
    auto dotf = [](const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    auto crossf = [](const float* a, const float* b, float* o) {
        o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0];
    };
    auto rotateSubtree = [&](int root, const float* from, const float* to, const float* pivot) {
        float ax[3];
        crossf(from, to, ax);
        const float al = len(ax);
        if (al < 1e-6f) return;
        for (float& v : ax) v /= al;
        const float ang = std::acos(std::clamp(dotf(from, to), -1.0f, 1.0f));
        const RwMat rot = AxisRotation(ax, ang, pivot);
        for (size_t f = 0; f < m_skeleton.size(); f++)
            for (int p = (int)f; p >= 0; p = m_skeleton[p].parent)
                if (p == root) { m_globals[f] = m_globals[f] * rot; break; }
    };
    float S[3] = {m_globals[fs].m[3][0], m_globals[fs].m[3][1], m_globals[fs].m[3][2]};
    float E[3] = {m_globals[fe].m[3][0], m_globals[fe].m[3][1], m_globals[fe].m[3][2]};
    float H[3] = {m_globals[fh].m[3][0], m_globals[fh].m[3][1], m_globals[fh].m[3][2]};
    float u1[3], u2[3];
    sub(E, S, u1); sub(H, E, u2);
    const float L1 = len(u1), L2 = len(u2);
    if (L1 < 1e-4f || L2 < 1e-4f) return;
    const float a = std::clamp(in.camPitch, -1.2f, 1.2f);
    const float dir[3] = {0.0f, std::cos(a), std::sin(a)};
    const float reach = (L1 + L2) * 0.985f;
    // bend side: the clip's elbow offset away from the shoulder -> target line
    float sh[3];
    sub(H, S, sh);
    const float shl = len(sh);
    if (shl < 1e-4f) return;
    float pole[3] = {u1[0], u1[1], u1[2]};
    {
        const float sd[3] = {sh[0] / shl, sh[1] / shl, sh[2] / shl};
        const float pd = dotf(pole, sd);
        for (int k = 0; k < 3; k++) pole[k] -= sd[k] * pd;
        const float pl = len(pole);
        if (pl < 1e-4f) { pole[0] = 0; pole[1] = 0; pole[2] = -1; } else for (float& v : pole) v /= pl;
    }
    // keep the pole perpendicular to the new aim direction
    {
        const float pd = dotf(pole, dir);
        for (int k = 0; k < 3; k++) pole[k] -= dir[k] * pd;
        const float pl = len(pole);
        if (pl < 1e-4f) return;
        for (float& v : pole) v /= pl;
    }
    const float ca = (L1 * L1 - L2 * L2 + reach * reach) / (2.0f * reach);
    const float hh = std::sqrt(std::max(0.0f, L1 * L1 - ca * ca));
    float E2[3], T[3];
    for (int k = 0; k < 3; k++) { E2[k] = S[k] + dir[k] * ca + pole[k] * hh; T[k] = S[k] + dir[k] * reach; }
    // upper arm: current elbow direction -> new elbow direction, about the shoulder
    float d0[3], d1[3];
    sub(E, S, d0); sub(E2, S, d1);
    for (float& v : d0) v /= L1;
    for (float& v : d1) v /= L1;
    rotateSubtree(fs, d0, d1, S);
    // forearm: the moved hand -> the target, about the new elbow
    const float H1[3] = {m_globals[fh].m[3][0], m_globals[fh].m[3][1], m_globals[fh].m[3][2]};
    const float E1[3] = {m_globals[fe].m[3][0], m_globals[fe].m[3][1], m_globals[fe].m[3][2]};
    float f0[3], f1[3];
    sub(H1, E1, f0); sub(T, E1, f1);
    const float l0 = len(f0), l1 = len(f1);
    if (l0 < 1e-4f || l1 < 1e-4f) return;
    for (float& v : f0) v /= l0;
    for (float& v : f1) v /= l1;
    rotateSubtree(fe, f0, f1, E1);
}

// ---- Frame update ----

void PlayerPed::Update(const Input& in) {
    if (!m_ready) return;
    const float dt = in.dt;
    m_damageFlash = std::max(0.0f, m_damageFlash - dt * 1.5f);
    if (Underwater()) {   // air in the lungs; empty lungs drown CJ
        m_breath = std::max(0.0f, m_breath - dt);
        if (m_breath <= 0.0f) Hurt(dt * 12.0f);
    } else {
        m_breath = std::min(kBreathMax, m_breath + dt * 8.0f);
    }
    m_stickX = in.stickX;
    m_stickY = in.stickY;

    if (m_climbing && !in.noclip) {
        m_velX = m_velY = 0.0f;
        ProcessClimb(in);
        if (m_climbing || m_cSnapValid) {
            Advance(dt);
            m_rootDX = m_rootDY = 0.0f;
            BuildPose(); BuildGlobals(m_pose); ShoulderBoneRotation();
            if (in.rewrite) ApplyRewritePose();
            Skin();
            return;
        }
    }
    if (m_swimming && !in.noclip) {
        ControlSwim(in);
        Advance(dt);
        if (dt > 0.0f) {   // the extracted root shift, model space (+Y forward) to world
            const float a = m_heading - kPi * 0.5f;
            const float c = std::cos(a), s = std::sin(a);
            const float k = m_swimState == SwimState::Under ? std::cos(m_swimPitch) : 1.0f;
            m_velX = (m_rootDX * c - m_rootDY * s) / dt * k;
            m_velY = (m_rootDX * s + m_rootDY * c) / dt * k;
            m_swimVelZ = m_swimState == SwimState::Under ? -std::sin(m_swimPitch) * m_rootDY / dt : 0.0f;
        }
        BuildPose(); BuildGlobals(m_pose); ShoulderBoneRotation();
        Skin();
        return;
    }
    if (in.noclip) {
        // F4 fly: CJ floats in the skydive pose, accelerating when moving.
        m_inAir = false;
        m_busy = false;
        m_ratio = 0.0f;
        const float mag = std::sqrt(in.stickX * in.stickX + in.stickY * in.stickY);
        const AnimClip* accel = Clip("fall_skydive_accel");
        const AnimClip* want = mag > 0.1f && accel ? accel : Clip("fall_skydive");
        if (!Active(want)) Blend(want, true, false, 2.0f);
        if (mag > 0.1f) {
            m_aimHeading = std::atan2(in.stickY, in.stickX);
            float d = m_aimHeading - m_heading;
            while (d > kPi) d -= 2 * kPi;
            while (d < -kPi) d += 2 * kPi;
            m_heading += std::clamp(d, -7.85f * dt, 7.85f * dt);
        }
        m_move.Reset();
    } else if (!in.onGround) {
        if (m_aiming || m_ducking) LeaveStance(in.rewrite, false, false);
        ControlInAir(in);
    } else {
        if (m_inAir) Land(in);
        if (m_landClip) {   // JUMP_land over the run: fade it out when done
            Assoc* l = Get(m_landClip);
            if (!l) m_landClip = nullptr;
            else if (l->finished) { l->delta = -8.0f; m_landClip = nullptr; }
        }
        if (m_busy) {
            Assoc* b = Get(m_busyClip);
            const float mag = std::sqrt(in.stickX * in.stickX + in.stickY * in.stickY);
            const bool cut = m_busySoft && b && mag > 0.0f && b->time >= 0.5f * b->clip->duration;
            if (!b || b->finished || cut) {
                if (m_busyNext) {
                    m_busyClip = m_busyNext;
                    m_busyNext = nullptr;
                    Blend(m_busyClip, false, false, 4.0f);
                } else {
                    m_busy = false;
                    if (b) b->delta = -4.0f;
                }
            }
        }
        const bool wantStance = !m_busy && (m_ducking || in.aim || in.duckJust);
        if (wantStance) ControlStance(in);
        else if (!m_busy) { if (m_aiming || m_ducking) LeaveStance(in.rewrite, false, false); ControlOnFoot(in); }
        else { if (m_aiming || m_ducking) LeaveStance(in.rewrite, false, false); m_ratio = 0.0f; }
    }

    // Dev: GTA_CJPOSE="clip:seconds" freezes CJ in one frame of one clip.
    if (const char* pose = std::getenv("GTA_CJPOSE")) {
        const char* colon = std::strchr(pose, ':');
        const std::string name = Lower(colon ? std::string(pose, colon) : std::string(pose));
        if (const AnimClip* c = Clip(name.c_str())) {
            m_assocs.clear();
            Assoc* a = Add(c, false, false);
            a->time = colon ? (float)std::atof(colon + 1) : 0.0f;
            a->playing = false;
        }
    }

    Advance(dt);

    // Velocity: the extracted root shift, model space (+Y forward) to world.
    if (in.noclip) {
        m_velX = m_velY = 0.0f;
    } else if (in.onGround && dt > 0.0f) {
        // First person moves along the stick while the body faces the view.
        const float a = (in.firstPerson && !m_busy && !m_aiming && !m_ducking ? m_moveAngle : m_heading) - kPi * 0.5f;
        const float c = std::cos(a), s = std::sin(a);
        m_velX = (m_rootDX * c - m_rootDY * s) / dt;
        m_velY = (m_rootDX * s + m_rootDY * c) / dt;
    }

    if (in.rewrite && in.onGround && !in.noclip) {
        const float speed = std::sqrt(m_velX * m_velX + m_velY * m_velY);
        const float mag = std::min(1.0f, std::sqrt(m_stickX * m_stickX + m_stickY * m_stickY));
        // the stick's left/right part relative to the camera, as CPad reports it
        const float right = m_stickX * std::sin(in.camYaw) - m_stickY * std::cos(in.camYaw);
        float turn = m_aimHeading - m_heading;
        while (turn > kPi) turn -= 2 * kPi;
        while (turn < -kPi) turn += 2 * kPi;
        // CTimer::ms_fTimeStep * 0.02, capped at 0.1 s
        m_move.UpdateBody(std::min(dt, 0.1f), right, mag, m_ratio, turn, speed >= 0.12f || m_aiming,
                          m_aiming && speed < 0.12f ? 0.70f : 0.0f);   // AimStationaryLeanFactor
    } else if (!in.rewrite) {
        m_move.Reset();
    }

    BuildPose();
    BuildGlobals(m_pose);
    ShoulderBoneRotation();
    if (in.rewrite) ApplyRewritePose();
    ApplyAimIK(in);
    if (in.rewrite) ApplyFootIK(in);
    Skin();
}

// CPU skinning: v' = sum_k w_k * (v * inverseBind_k * bone_k).
void PlayerPed::Skin() {
    std::vector<RwMat> skin;
    const bool bind = std::getenv("GTA_CJBIND") != nullptr;   // dev: the raw bind mesh
    for (Part& p : m_parts) {
        skin.resize(p.inverseBind.size());
        for (size_t n = 0; n < skin.size(); n++) skin[n] = p.inverseBind[n] * m_globals[p.boneFrame[n]];
        if (bind) skin.assign(skin.size(), RwMat());
        for (size_t v = 0; v < p.verts.size(); v++) {
            const SkinVertex& sv = p.verts[v];
            float pos[3] = {0, 0, 0}, nrm[3] = {0, 0, 0};
            for (int k = 0; k < 4; k++) {
                const float w = sv.weight[k];
                if (w <= 0.0f || sv.bone[k] >= skin.size()) continue;
                float a[3], b[3];
                skin[sv.bone[k]].Apply(sv.pos, a);
                skin[sv.bone[k]].ApplyDir(sv.normal, b);
                for (int i = 0; i < 3; i++) { pos[i] += a[i] * w; nrm[i] += b[i] * w; }
            }
            const float nl = std::sqrt(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
            if (nl > 1e-6f) for (float& c : nrm) c /= nl;
            MeshVertex& out = p.skinned[v];
            out.x = pos[0]; out.y = pos[1]; out.z = pos[2];
            out.nx = nrm[0]; out.ny = nrm[1]; out.nz = nrm[2];
        }
    }
}

bool PlayerPed::BoneWorldPos(int boneId, float x, float y, float feetZ, float out[3]) const {
    const int f = boneId >= 0 && boneId < 512 ? m_frameOfBone[boneId] : -1;
    if (f < 0 || f >= (int)m_globals.size()) return false;
    const float a = m_heading - kPi * 0.5f, c = std::cos(a), s = std::sin(a);
    const float* p = m_globals[f].m[3];
    out[0] = x + p[0] * c - p[1] * s;
    out[1] = y + p[0] * s + p[1] * c;
    out[2] = feetZ - m_bindMinZ + p[2];
    return true;
}

void PlayerPed::Draw(D3DRenderer& r, float x, float y, float feetZ, bool hideHead) {
    if (!m_ready) return;
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    // Ped models face +Y; heading 0 faces +X.
    const float a = m_heading - kPi * 0.5f, c = std::cos(a), s = std::sin(a);
    D3DMATRIX w = {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, x, y, feetZ - m_bindMinZ, 1};
    dev->SetTransform(D3DTS_WORLD, &w);
    for (Part& p : m_parts) {
        if (!p.mesh || (hideHead && p.isHead)) continue;
        r.UpdateMeshVertices(p.mesh, p.skinned.data(), (uint32_t)p.skinned.size());
        r.DrawMesh(p.mesh);
    }
    DrawGun(r, x, y, feetZ);
}

// ---- CTaskSimpleSwim @ 00688930 -------------------------------------------

void PlayerPed::StartSwim() {
    if (m_swimming) return;
    m_ducking = m_aiming = false;
    m_swimming = true;
    m_swimState = SwimState::Tread;
    m_swimPitch = 0.0f;
    m_swimVelZ = 0.0f;
    m_swimClip = nullptr;
    m_inAir = false; m_jumping = false; m_busy = false;
    m_landClip = nullptr;
    m_ratio = 0.0f;
    m_move.Reset();
    SwimWant("swim_tread", 8.0f, false);
}

void PlayerPed::StopSwim() {
    if (!m_swimming) return;
    m_swimming = false;
    m_swimClip = nullptr;
    m_swimPitch = 0.0f;
    m_swimVelZ = 0.0f;
    m_ratio = 0.0f;
    // back to the idle stance (CTaskSimpleSwim::~, BlendAnimation IDLE)
    for (Assoc& a : m_assocs) a.delta = -8.0f;
    if (const AnimClip* idle = Clip("idle_stance")) Blend(idle, true, false, 8.0f);
}

void PlayerPed::SwimSurface() {
    if (!m_swimming || m_swimState != SwimState::Under) return;
    m_swimState = SwimState::Tread;
    m_swimPitch = 0.0f;
    m_swimVelZ = 0.0f;
}

// ProcessSwimmingResistance: where the ped origin rides below the surface.
float PlayerPed::SwimSubmerge() const {
    float sum = 0.0f, diff = 1.0f;
    for (const Assoc& a : m_assocs) {
        if (a.dead || !a.clip) continue;
        if (a.clip->name == "swim_breast") { sum += 0.4f * a.blend; diff -= a.blend; }
        else if (a.clip->name == "swim_crawl") { sum += 0.2f * a.blend; diff -= a.blend; }
    }
    return std::max(diff, 0.0f) * 0.55f + sum;
}

// Make `name` the one swim clip playing; every other association fades out.
void PlayerPed::SwimWant(const char* name, float rate, bool movement) {
    const AnimClip* c = Clip(name);
    if (!c || c == m_swimClip) return;
    m_swimClip = c;
    for (Assoc& a : m_assocs)
        if (a.clip != c && !a.dead) a.delta = -std::abs(rate);
    Assoc* a = Get(c);
    if (a && a->delta < 0.0f) { a->delta = std::abs(rate); a->dead = false; }
    else if (!a) { a = Add(c, name != std::string("swim_dive_under") && name != std::string("swim_jumpout"), movement); a->blend = 0.0f; a->delta = std::abs(rate); }
    a->movement = movement;
}

// ProcessControlInput + ProcessSwimAnims, folded: the stick steers and sets
// the stroke, dive / jump change the state.
void PlayerPed::ControlSwim(const Input& in) {
    const float sx = in.stickX, sy = in.stickY;
    const float mag = std::min(1.0f, std::sqrt(sx * sx + sy * sy));
    const float step = 0.07f * 50.0f * in.dt;   // move blend ratio ramp, 0.07 per 1/50 s
    if (mag > 0.0f) {
        m_aimHeading = std::atan2(sy, sx);
        const float d = mag - m_ratio;
        m_ratio = d > step ? m_ratio + step : d < -step ? m_ratio - step : mag;
    } else {
        m_ratio = std::max(0.0f, m_ratio - 0.05f * 50.0f * in.dt);
    }
    // turn toward the stick (retail: ~0.03 rad per 1/50 s at full lock)
    if (mag > 0.1f) {
        float d = m_aimHeading - m_heading;
        while (d > kPi) d -= 2 * kPi;
        while (d < -kPi) d += 2 * kPi;
        const float rate = 2.2f * in.dt;
        m_heading += std::clamp(d, -rate, rate);
        while (m_heading > kPi) m_heading -= 2 * kPi;
        while (m_heading < -kPi) m_heading += 2 * kPi;
    }

    const Assoc* cur = Get(m_swimClip);
    switch (m_swimState) {
    case SwimState::Tread:
    case SwimState::Sprint:
    case SwimState::Sprinting:
        if (in.diveJust) {
            m_swimState = SwimState::Dive;
            m_ratio = 0.0f;
            SwimWant("swim_dive_under", 8.0f, false);
            break;
        }
        if (in.jumpDown) {   // BACK_TO_SURFACE: climb out; the host tries the ledge
            m_swimState = SwimState::BackToSurface;
            SwimWant("swim_jumpout", 8.0f, false);
            break;
        }
        if (m_ratio < 0.1f) { m_swimState = SwimState::Tread; SwimWant("swim_tread", 8.0f, false); }
        else if (in.sprint) { m_swimState = SwimState::Sprinting; SwimWant("swim_crawl", 2.0f, true); }
        else { m_swimState = SwimState::Sprint; SwimWant("swim_breast", 2.0f, true); }
        break;
    case SwimState::Dive:
        if (cur && cur->finished) {
            m_swimState = SwimState::Under;
            m_swimPitch = 0.7854f;   // SWIM_DIVE_UNDER_ANGLE: head first, down
            m_swimClip = nullptr;
        }
        break;
    case SwimState::Under: {
        // descend / ascend held bend the swimmer; otherwise level out
        const float want = in.diveUp ? -0.9f : in.diveDown ? 0.9f : 0.0f;
        const float r = 1.2f * in.dt;
        m_swimPitch += std::clamp(want - m_swimPitch, -r, r);
        SwimWant(m_ratio > 0.1f ? "swim_under" : "swim_glide", 4.0f, m_ratio > 0.1f);
        break;
    }
    case SwimState::BackToSurface:
        if (cur && cur->finished) { m_swimState = SwimState::Tread; m_swimClip = nullptr; }
        break;
    }
}

// CClothesBuilder::BlendGeometry: positions, normals and UVs are the ratio-
// weighted sum of the Normal, Fat and Ripped shapes (ratios 1-f-m, f, m).
void PlayerPed::SetBodyShape(float fat, float muscle) {
    fat = std::clamp(fat, 0.0f, 1.0f);
    muscle = std::clamp(muscle, 0.0f, 1.0f);
    if (fat + muscle > 1.0f) { const float k = 1.0f / (fat + muscle); fat *= k; muscle *= k; }
    const float r[3] = {1.0f - fat - muscle, fat, muscle};
    for (Part& p : m_parts) {
        for (size_t i = 0; i < p.verts.size(); i++) {
            SkinVertex& v = p.verts[i];
            for (int k = 0; k < 3; k++) v.pos[k] = v.normal[k] = 0.0f;
            v.uv[0] = v.uv[1] = 0.0f;
            for (int s = 0; s < 3; s++) {
                if (r[s] == 0.0f) continue;
                const std::vector<SkinVertex>& src = p.shape[p.shape[s].empty() ? 0 : s];
                for (int k = 0; k < 3; k++) { v.pos[k] += r[s] * src[i].pos[k]; v.normal[k] += r[s] * src[i].normal[k]; }
                v.uv[0] += r[s] * src[i].uv[0]; v.uv[1] += r[s] * src[i].uv[1];
            }
            const float n = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] + v.normal[2] * v.normal[2]);
            if (n > 1e-6f) for (int k = 0; k < 3; k++) v.normal[k] /= n;
        }
    }
}

// ---- CTaskSimpleDuck @ 00691fc0 and the aiming stance ----------------------

// SmoothDampAngle: critically damped approach with a speed cap.
static float SmoothDampAngle(float cur, float target, float& vel, float smooth, float maxSpeed, float dt) {
    float d = target - cur;
    while (d > kPi) d -= 2 * kPi;
    while (d < -kPi) d += 2 * kPi;
    target = cur + d;
    smooth = std::max(0.0001f, smooth);
    const float omega = 2.0f / smooth, x = omega * dt;
    const float e = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    const float maxChange = maxSpeed * smooth;
    const float change = std::clamp(cur - target, -maxChange, maxChange);
    const float tgt = cur - change;
    const float temp = (vel + omega * change) * dt;
    vel = (vel - omega * temp) * e;
    float out = tgt + (change + temp) * e;
    if ((target - cur > 0.0f) == (out > target)) { out = target; vel = dt > 0.0f ? (out - target) / dt : 0.0f; }
    return out;
}

// Drive the stance clips to the given weights (they sum to 1); anything else
// fades out. expResp > 0: exponential pose-weight response (rewrite aiming);
// otherwise each clip's own linear blend rate (units per second).
void PlayerPed::StanceBlend(const StanceW* ws, int n, float dt, float expResp, float fadeRate) {
    for (Assoc& a : m_assocs) {
        if (a.dead || a.partial) continue;
        bool listed = false;
        for (int i = 0; i < n; i++) if (a.clip == Clip(ws[i].name)) listed = true;
        if (!listed && a.delta >= 0.0f) a.delta = -fadeRate;
    }
    for (int i = 0; i < n; i++) {
        const AnimClip* c = Clip(ws[i].name);
        if (!c) continue;
        Assoc* a = Get(c);
        if (!a && ws[i].w <= 0.0f) continue;
        if (!a) { a = Add(c, ws[i].loop, ws[i].move); a->blend = 0.0f; }
        a->delta = 0.0f;
        if (expResp > 0.0f) {
            a->blend += (ws[i].w - a->blend) * (1.0f - std::exp(-expResp * dt));
            if (std::fabs(ws[i].w - a->blend) < 0.002f) a->blend = ws[i].w;
        } else {
            const float step = ws[i].rate * dt;
            a->blend = a->blend < ws[i].w ? std::min(ws[i].w, a->blend + step) : std::max(ws[i].w, a->blend - step);
        }
        a->playing = ws[i].w > 0.0f || ws[i].loop == false;
        if (a->blend <= 0.0f && ws[i].w <= 0.0f) a->dead = true;
    }
}

// exitMoving / exitSprint: PlayerControlDucked starts the walk or run at a
// move blend ratio of 1.0 / 1.5 when the stick is held past 0.5.
void PlayerPed::LeaveStance(bool rewrite, bool exitMoving, bool exitSprint) {
    const bool wasAiming = m_aiming && m_gun >= 0;
    m_ducking = m_aiming = false;
    m_duckX = m_duckY = 0.0f;
    m_aimPitch = m_aimPitchVel = 0.0f;
    m_reloading = m_firing = false;
    m_gunFireClip = m_gunRelClip = nullptr;
    m_ratio = exitMoving ? (exitSprint ? 1.5f : 1.0f) : 0.0f;
    // rewrite: ExitBlendResponse 4.0, and PreserveMovementOnExit keeps the
    // crouch walk playing while it fades into standing movement
    const float rate = rewrite ? 4.0f : 8.0f;
    for (Assoc& a : m_assocs) a.delta = -rate;
    if (!exitMoving)
        if (const AnimClip* idle = Clip("idle_stance")) {
            Assoc* i = Blend(idle, true, false, 4.0f);   // MakeAbortable: BlendAnimation IDLE at 4
            i->delta = 4.0f;
        }
    // CTaskSimpleUseGun::RemoveStanceAnims (END_LEISURE): the gun comes down
    // through Gun_2_IDLE, blended in at 8.
    if (wasAiming)
        if (const AnimClip* g2i = Clip("gun_2_idle")) {
            Assoc* a = Blend(g2i, false, false, 8.0f);
            a->partial = true;
            a->autoRemove = true;
            a->time = 0.0f;
            a->finished = false;
            a->playing = true;
        }
}

void PlayerPed::ControlStance(const Input& in) {
    const float sx = in.stickX, sy = in.stickY;
    const float mag = std::min(1.0f, std::sqrt(sx * sx + sy * sy));
    if (!in.sprint) m_sprintBeforeDuck = false;

    // Duck on / off: DuckJustDown. CanPedDuck: retail refuses while running or
    // sprinting; the rewrite (EnableCrouchLocomotion) allows it from a run.
    if (in.duckJust && !m_ducking && (in.rewrite || (m_ratio <= 1.0f && !in.sprint))) {
        m_ducking = true;
        m_duckX = m_duckY = 0.0f;
        m_sprintBeforeDuck = in.sprint;   // sprint already held is ignored until released
        m_ratio = 0.0f;
    } else if (m_ducking) {
        bool leave = in.duckJust || in.jumpDown;
        if (in.rewrite) {   // KeepCrouchedWithSprint / ExitToRunWithSprint
            if (in.sprint && !m_sprintBeforeDuck && mag > 0.1f) leave = true;
        } else if (in.sprint) {
            leave = true;
        }
        if (leave) {
            m_ducking = false;
            if (!m_aiming && !in.aim) { LeaveStance(in.rewrite, mag > 0.5f && !in.jumpDown, in.sprint); return; }
        }
    }
    m_aiming = in.aim;
    if (!m_ducking && !m_aiming) { LeaveStance(in.rewrite, false, false); return; }
    m_ratio = 0.0f;

    if (m_aiming) m_aimHeading = in.camYaw;       // face where the camera looks
    else if (mag > 0.0f) m_aimHeading = std::atan2(sy, sx);

    // stick in the ped's own frame: ly forward, lx right
    const float ch = std::cos(m_heading), sh = std::sin(m_heading);
    float ly = sx * ch + sy * sh, lx = sx * sh - sy * ch;
    if (!m_aiming) { ly = mag; lx = 0.0f; }   // unaimed: he turns and walks forward

    // ControlDuckMove: the command follows the stick at 0.07 per 1/50 s
    const float step = 0.07f * 50.0f * in.dt;
    if (m_ducking) {
        const float tgtY = -ly;   // y < 0 is forward
        const float dy = tgtY - m_duckY;
        m_duckY = dy > step ? m_duckY + step : dy < -step ? m_duckY - step : tgtY;
        m_duckX = (m_aiming && std::fabs(ly) < 0.1f && std::fabs(lx) >= 0.9f) ? (lx < 0 ? -1.0f : 1.0f) : 0.0f;
    }

    // clip weights
    StanceW ws[6];
    int n = 0;
    float moveSum = 0.0f;
    if (m_ducking) {
        // retail starts the crouch walk once the crouch is fully in; the
        // rewrite's MoveStartBlend overlaps them from 35%
        const Assoc* cr = Get(Clip("weapon_crouch"));
        const bool moveOk = cr && cr->blend >= (in.rewrite ? 0.35f : 0.99f);
        const float f = moveOk ? std::max(0.0f, -m_duckY) : 0.0f, b = moveOk ? std::max(0.0f, m_duckY) : 0.0f;
        const float enter = 4.0f, move = 8.0f;   // EnterBlendResponse / MoveBlendResponse
        if (m_duckX != 0.0f) {
            ws[n++] = {m_duckX > 0 ? "crouch_roll_r" : "crouch_roll_l", 1.0f, false, false, move};
        } else {
            ws[n++] = {"weapon_crouch", 1.0f - std::min(1.0f, f + b), false, false, enter};
            ws[n++] = {"guncrouchfwd", f, true, true, move};
            ws[n++] = {"guncrouchbwd", b, true, true, move};
            moveSum = f + b;
        }
        if (n == 3 && moveSum > 0.0f) {   // retail: speed max(0.6, |cmd|)
            if (Assoc* a = Get(Clip("guncrouchfwd"))) a->speed = std::max(0.6f, f);
            if (Assoc* a = Get(Clip("guncrouchbwd"))) a->speed = std::max(0.6f, b);
        }
        StanceBlend(ws, n, in.dt, 0.0f, in.rewrite ? 4.0f : 8.0f);
    } else {
        const float f = std::max(0.0f, ly), b = std::max(0.0f, -ly), r = std::max(0.0f, lx), l = std::max(0.0f, -lx);
        const float tot = f + b + r + l;
        const float k = tot > 1.0f ? 1.0f / tot : 1.0f;
        if (m_gun < 0 && Clip("fightidle")) {   // no gun in hand: the boxing stance, not a gun hold
            ws[n++] = {"fightidle", 1.0f - std::min(1.0f, tot * k), false, true, 8.0f};
            ws[n++] = {"fightsh_fwd", f * k, true, true, 8.0f};
            ws[n++] = {"fightsh_bwd", b * k, true, true, 8.0f};
            ws[n++] = {"fightsh_right", r * k, true, true, 8.0f};
            ws[n++] = {"fightsh_left", l * k, true, true, 8.0f};
        } else {
            ws[n++] = {"gun_stand", 1.0f - std::min(1.0f, tot * k), false, true, 8.0f};
            ws[n++] = {"gunmove_fwd", f * k, true, true, 8.0f};
            ws[n++] = {"gunmove_bwd", b * k, true, true, 8.0f};
            ws[n++] = {"gunmove_r", r * k, true, true, 8.0f};
            ws[n++] = {"gunmove_l", l * k, true, true, 8.0f};
        }
        // rewrite EnableAimStrafeBlend: AimStrafeBlendResponse 12.5
        StanceBlend(ws, n, in.dt, in.rewrite ? 12.5f : 0.0f, 8.0f);
        // weapon.dat move speed scales the aim strafe clips
        if (m_gun >= 0)
            for (const char* nm : {"gunmove_fwd", "gunmove_bwd", "gunmove_r", "gunmove_l"})
                if (Assoc* as = Get(Clip(nm))) as->speed = m_guns[m_gun].moveSpeed;
    }

    // turn: rewrite aiming is a damped follow of the camera heading
    // (AimHeadingSmoothTimeSeconds, first person 0.03235; 720 deg/s cap)
    if (m_aiming && in.rewrite) {
        m_heading = SmoothDampAngle(m_heading, m_aimHeading, m_aimHeadVel, in.firstPerson ? 0.03235f : 0.055f,
                                    720.0f * (kPi / 180.0f), in.dt);
    } else {
        m_aimHeadVel = 0.0f;
        float d = m_aimHeading - m_heading;
        while (d > kPi) d -= 2 * kPi;
        while (d < -kPi) d += 2 * kPi;
        const float rate = (m_aiming ? 12.0f : 9.0f * (kPi / 180.0f) * 50.0f) * in.dt;
        m_heading += std::clamp(d, -rate, rate);
    }
    while (m_heading > kPi) m_heading -= 2 * kPi;
    while (m_heading < -kPi) m_heading += 2 * kPi;

    // vertical body follow: AimPitchInfluence 0.65 of the camera pitch, at most
    // 16 degrees, damped (0.10 s, 540 deg/s); the crosshair stays instant
    if (m_aiming && in.rewrite) {
        const float deg = 180.0f / kPi;
        const float tgt = std::clamp(in.camPitch * deg * 0.65f, -16.0f, 16.0f);
        float p = m_aimPitch / deg, v = m_aimPitchVel / deg;
        m_aimPitch = SmoothDampAngle(p, tgt / deg, v, 0.10f, 540.0f / deg, in.dt) * deg;
        m_aimPitchVel = v * deg;
    } else {
        m_aimPitch = m_aimPitchVel = 0.0f;
    }
    if (m_aiming) UpdateGun(in);
}
