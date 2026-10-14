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

const char* PlayerPed::ClipName() const {
    return m_cur.clip ? m_cur.clip->name.c_str() : "-";
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
        m_parts.push_back(std::move(part));
    }
    std::fprintf(stderr, "[CJ] %zu parts, %zu frames, %zu clips, body '%s', feet z %.3f\n",
                 m_parts.size(), m_skeleton.size(), m_clips.size(),
                 m_skeleton.size() > 1 ? m_skeleton[1].name.c_str() : "-", m_bindMinZ);
    m_ready = m_parts.size() == 5 && Clip("idle_stance");
    if (m_ready) Play(Clip("idle_stance"), true, 0.0f, false);
    return m_ready;
}

void PlayerPed::Shutdown(D3DRenderer& r) {
    for (Part& p : m_parts) if (p.mesh) r.DestroyMesh(p.mesh);
    for (IDirect3DTexture9* t : m_textures) if (t) r.DestroyTexture(t);
    m_parts.clear();
    m_textures.clear();
    m_ready = false;
}

void PlayerPed::Play(const AnimClip* clip, bool loop, float fade, bool keepPhase) {
    if (!clip || clip == m_cur.clip) return;
    float phase = 0.0f;
    if (keepPhase && m_cur.clip && m_cur.clip->duration > 0)
        phase = m_cur.time / m_cur.clip->duration;
    m_prev = m_cur;
    m_cur = {clip, phase * clip->duration, loop};
    if (fade <= 0.0f || !m_prev.clip) { m_blend = 1.0f; m_blendRate = 0.0f; }
    else { m_blend = 0.0f; m_blendRate = 1.0f / fade; }
}

void PlayerPed::Sample(const Track& tr, std::vector<Pose>& out) const {
    out.resize(m_skeleton.size());
    for (size_t f = 0; f < m_skeleton.size(); f++) {
        RowsToQuat(m_skeleton[f].local, out[f].q);
        for (int k = 0; k < 3; k++) out[f].t[k] = m_skeleton[f].local.m[3][k];
        const AnimSequence* s = tr.clip ? tr.clip->FindBone(m_skeleton[f].boneId) : nullptr;
        if (!s || s->keys.empty() || m_skeleton[f].boneId < 0) continue;
        const std::vector<AnimKey>& keys = s->keys;
        size_t k = 0;
        while (k + 1 < keys.size() && keys[k + 1].time <= tr.time) k++;
        const AnimKey& a = keys[k];
        const AnimKey& b = keys[std::min(k + 1, keys.size() - 1)];
        const float span = b.time - a.time;
        const float t = span > 1e-5f ? std::clamp((tr.time - a.time) / span, 0.0f, 1.0f) : 0.0f;
        Slerp(a.q, b.q, t, out[f].q);
        if (s->hasTranslation)
            for (int i = 0; i < 3; i++) out[f].t[i] = a.t[i] + (b.t[i] - a.t[i]) * t;
        if (m_skeleton[f].boneId == 0) { out[f].t[0] = 0.0f; out[f].t[1] = 0.0f; }   // root motion
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

void PlayerPed::Update(const Input& in) {
    if (!m_ready) return;
    const float speed = std::sqrt(in.moveX * in.moveX + in.moveY * in.moveY);

    // Face the direction of travel, turning at a bounded rate.
    if (speed > 0.3f) {
        float target = std::atan2(in.moveY, in.moveX), d = target - m_heading;
        while (d > kPi) d -= 2 * kPi;
        while (d < -kPi) d += 2 * kPi;
        const float step = 12.0f * in.dt;
        m_heading += std::clamp(d, -step, step);
        while (m_heading > kPi) m_heading -= 2 * kPi;
        while (m_heading < -kPi) m_heading += 2 * kPi;
    }

    const AnimClip* walk = Clip("walk_player");
    const AnimClip* run = Clip("run_player");
    const AnimClip* sprint = Clip("sprint_civi");
    auto natural = [](const AnimClip* c) {   // root travel per second
        const AnimSequence* s = c ? c->FindBone(0) : nullptr;
        if (!s || s->keys.size() < 2 || c->duration <= 0) return 1.0f;
        return std::max(0.1f, (s->keys.back().t[1] - s->keys.front().t[1]) / c->duration);
    };
    auto isLoco = [&](const AnimClip* c) { return c && (c == walk || c == run || c == sprint); };
    float rate = 1.0f;

    if (!in.onGround) {
        m_airTime += in.dt;
        const AnimClip* launch = Clip("jump_launch");
        const AnimClip* glide = Clip("jump_glide");
        if (m_wasOnGround && in.velZ > 0.0f) Play(launch, false, 0.08f, false);
        else if (m_cur.clip == launch && m_cur.time >= launch->duration) Play(glide, true, 0.1f, false);
        else if (m_airTime > 0.8f && in.velZ < 0.0f) Play(Clip("fall_fall"), true, 0.25f, false);
        else if (m_cur.clip != launch && m_cur.clip != glide && m_cur.clip != Clip("fall_fall"))
            Play(glide, true, 0.15f, false);
    } else {
        if (!m_wasOnGround && m_airTime > 0.15f) {
            m_landClip = Clip(m_airTime > 0.8f ? "fall_land" : "jump_land");
            Play(m_landClip, false, 0.05f, false);
        }
        m_airTime = 0.0f;
        const bool landing = m_cur.clip && m_cur.clip == m_landClip && m_cur.time < m_cur.clip->duration;
        if (!landing || speed > 2.0f) {
            const AnimClip* want = speed < 0.3f ? Clip("idle_stance")
                                 : speed < 3.0f ? walk : speed < 7.6f ? run : sprint;
            Play(want, true, isLoco(want) && isLoco(m_cur.clip) ? 0.25f : 0.2f,
                 isLoco(want) && isLoco(m_cur.clip));
            if (isLoco(m_cur.clip)) rate = std::clamp(speed / natural(m_cur.clip), 0.4f, 1.6f);
        }
    }
    m_wasOnGround = in.onGround;

    auto advance = [&](Track& tr, float r) {
        if (!tr.clip) return;
        tr.time += in.dt * r;
        const float dur = tr.clip->duration;
        if (dur <= 0) tr.time = 0;
        else if (tr.loop) tr.time = std::fmod(tr.time, dur);
        else tr.time = std::min(tr.time, dur);
    };
    advance(m_cur, rate);
    advance(m_prev, rate);
    // Dev: GTA_CJPOSE="clip:seconds" freezes CJ in one frame of one clip.
    if (const char* pose = std::getenv("GTA_CJPOSE")) {
        const char* colon = std::strchr(pose, ':');
        const std::string name = colon ? std::string(pose, colon) : std::string(pose);
        if (const AnimClip* c = Clip(name.c_str())) {
            m_cur = {c, colon ? (float)std::atof(colon + 1) : 0.0f, false};
            m_prev = {};
            m_blend = 1.0f;
        }
    }
    m_blend = std::min(1.0f, m_blend + in.dt * m_blendRate);

    Sample(m_cur, m_poseA);
    if (m_blend < 1.0f && m_prev.clip) {
        Sample(m_prev, m_poseB);
        for (size_t f = 0; f < m_poseA.size(); f++) {
            float q[4];
            Slerp(m_poseB[f].q, m_poseA[f].q, m_blend, q);
            std::memcpy(m_poseA[f].q, q, sizeof q);
            for (int i = 0; i < 3; i++)
                m_poseA[f].t[i] = m_poseB[f].t[i] + (m_poseA[f].t[i] - m_poseB[f].t[i]) * m_blend;
        }
    }
    BuildGlobals(m_poseA);
    ShoulderBoneRotation();

    // CPU skinning: v' = sum_k w_k * (v * inverseBind_k * bone_k).
    std::vector<RwMat> skin;
    for (Part& p : m_parts) {
        skin.resize(p.inverseBind.size());
        for (size_t n = 0; n < skin.size(); n++) skin[n] = p.inverseBind[n] * m_globals[p.boneFrame[n]];
        if (std::getenv("GTA_CJBIND")) skin.assign(skin.size(), RwMat());   // dev: the raw bind mesh
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

void PlayerPed::Draw(D3DRenderer& r, float x, float y, float feetZ) {
    if (!m_ready) return;
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    // Ped models face +Y; heading 0 faces +X.
    const float a = m_heading - kPi * 0.5f, c = std::cos(a), s = std::sin(a);
    D3DMATRIX w = {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, x, y, feetZ - m_bindMinZ, 1};
    dev->SetTransform(D3DTS_WORLD, &w);
    for (Part& p : m_parts) {
        if (!p.mesh) continue;
        r.UpdateMeshVertices(p.mesh, p.skinned.data(), (uint32_t)p.skinned.size());
        r.DrawMesh(p.mesh);
    }
}
