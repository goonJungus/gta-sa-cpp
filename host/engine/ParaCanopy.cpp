#include "ParaCanopy.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "ImgLoader.h"
#include "TxdLoader.h"

namespace {

void QuatToRows(const float q[4], RwMat& m) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    m.m[0][0] = 1 - 2 * (y * y + z * z); m.m[0][1] = 2 * (x * y + w * z); m.m[0][2] = 2 * (x * z - w * y);
    m.m[1][0] = 2 * (x * y - w * z); m.m[1][1] = 1 - 2 * (x * x + z * z); m.m[1][2] = 2 * (y * z + w * x);
    m.m[2][0] = 2 * (x * z + w * y); m.m[2][1] = 2 * (y * z - w * x); m.m[2][2] = 1 - 2 * (x * x + y * y);
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

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

}  // namespace

bool ParaCanopy::Load(D3DRenderer& r, ImgLoader& gta3) {
    const std::vector<uint8_t> dff = gta3.Extract("parachute.dff"), txd = gta3.Extract("paracx.txd");
    if (dff.empty()) return false;
    for (SkinnedClump& c : SkinnedDff::LoadClumps(dff.data(), dff.size()))
        if (c.valid) { m_clump = std::move(c); break; }
    SkinnedClump& c = m_clump;
    if (!c.valid || c.inverseBind.size() < c.nodeIds.size() || c.vertices.empty()) return false;
    std::vector<int> frameOfBone(512, -1), nodeOfBone(512, -1);
    for (size_t f = 0; f < c.frames.size(); f++)
        if (c.frames[f].boneId >= 0 && c.frames[f].boneId < 512) frameOfBone[(size_t)c.frames[f].boneId] = (int)f;
    for (size_t n = 0; n < c.nodeIds.size(); n++)
        if (c.nodeIds[n] >= 0 && c.nodeIds[n] < 512) nodeOfBone[(size_t)c.nodeIds[n]] = (int)n;
    m_local.resize(c.frames.size());
    for (size_t f = 0; f < c.frames.size(); f++) m_local[f] = c.frames[f].local;
    // a bone's rest offset from its parent comes from the skin matrices (SkinGetBonePositionsToTable)
    for (size_t n = 1; n < c.nodeIds.size(); n++) {
        const int f = c.nodeIds[n] >= 0 && c.nodeIds[n] < 512 ? frameOfBone[(size_t)c.nodeIds[n]] : -1;
        if (f < 0) continue;
        int pf = c.frames[(size_t)f].parent, pn = -1;
        while (pf >= 0 && (pn = c.frames[(size_t)pf].boneId >= 0 && c.frames[(size_t)pf].boneId < 512 ? nodeOfBone[(size_t)c.frames[(size_t)pf].boneId] : -1) < 0)
            pf = c.frames[(size_t)pf].parent;
        const float zero[3] = {0, 0, 0};
        float bonePos[3], local[3];
        Invert(c.inverseBind[n]).Apply(zero, bonePos);
        c.inverseBind[(size_t)(pn >= 0 ? pn : 0)].Apply(bonePos, local);
        for (int k = 0; k < 3; k++) m_local[(size_t)f].m[3][k] = local[k];
    }
    m_boneFrame.assign(c.nodeIds.size(), 0);
    for (size_t n = 0; n < c.nodeIds.size(); n++) {
        const int f = c.nodeIds[n] >= 0 && c.nodeIds[n] < 512 ? frameOfBone[(size_t)c.nodeIds[n]] : -1;
        m_boneFrame[n] = f >= 0 ? f : 0;
    }
    m_skinned.resize(c.vertices.size());
    for (size_t v = 0; v < c.vertices.size(); v++) {
        const SkinVertex& sv = c.vertices[v];
        m_skinned[v] = {sv.pos[0], sv.pos[1], sv.pos[2], sv.normal[0], sv.normal[1], sv.normal[2], 0xFFFFFFFFu, sv.uv[0], sv.uv[1]};
    }
    std::vector<TxdTexture> tex;
    if (!txd.empty()) tex = TxdLoader::LoadFromMemory(txd.data(), txd.size());
    const size_t numMat = std::max<size_t>(1, c.materialTexture.size());
    for (size_t mi = 0; mi < numMat; mi++) {
        std::vector<uint16_t> idx;
        for (size_t t = 0; t < c.indices.size() / 3; t++) {
            const size_t tm = t < c.triMaterial.size() ? c.triMaterial[t] : 0;
            if (tm == mi) idx.insert(idx.end(), c.indices.begin() + (long)(t * 3), c.indices.begin() + (long)(t * 3 + 3));
        }
        if (idx.empty()) continue;
        D3DRenderMesh* me = r.CreateMesh(m_skinned.data(), (uint32_t)m_skinned.size(), idx.data(), (uint32_t)idx.size());
        if (!me) continue;
        const std::string want = mi < c.materialTexture.size() ? Lower(c.materialTexture[mi]) : std::string();
        for (const TxdTexture& t : tex) {
            if (want.empty() || Lower(t.name) != want) continue;
            IDirect3DTexture9* tx = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(), &t.extraMips);
            me->texture = tx;
            m_textures.push_back(tx);
            break;
        }
        m_meshes.push_back(me);
    }
    m_ready = !m_meshes.empty();
    return m_ready;
}

void ParaCanopy::Shutdown(D3DRenderer& r) {
    for (IDirect3DTexture9* t : m_textures) r.DestroyTexture(t);
    m_textures.clear();
    m_meshes.clear();
    m_ready = false;
}

void ParaCanopy::Pose(const AnimClip* clip, float time) {
    if (!m_ready) return;
    const SkinnedClump& c = m_clump;
    std::vector<RwMat> globals(c.frames.size());
    for (size_t f = 0; f < c.frames.size(); f++) {
        RwMat local = m_local[f];
        const AnimSequence* s = clip && c.frames[f].boneId >= 0 ? clip->FindBone(c.frames[f].boneId) : nullptr;
        if (s && !s->keys.empty()) {
            const std::vector<AnimKey>& keys = s->keys;
            size_t k = 0;
            while (k + 1 < keys.size() && keys[k + 1].time <= time) k++;
            const AnimKey& ka = keys[k];
            const AnimKey& kb = keys[std::min(k + 1, keys.size() - 1)];
            const float span = kb.time - ka.time;
            const float u = span > 1e-5f ? std::clamp((time - ka.time) / span, 0.0f, 1.0f) : 0.0f;
            float q[4];
            Slerp(ka.q, kb.q, u, q);
            QuatToRows(q, local);
            if (s->hasTranslation) for (int i = 0; i < 3; i++) local.m[3][i] = ka.t[i] + (kb.t[i] - ka.t[i]) * u;
        }
        const int p = c.frames[f].parent;
        globals[f] = (p >= 0 && p < (int)f) ? local * globals[(size_t)p] : local;
    }
    std::vector<RwMat> skin(c.inverseBind.size());
    for (size_t n = 0; n < skin.size(); n++) skin[n] = c.inverseBind[n] * globals[(size_t)m_boneFrame[n]];
    for (size_t v = 0; v < c.vertices.size(); v++) {
        const SkinVertex& sv = c.vertices[v];
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
        if (nl > 1e-6f) for (float& n : nrm) n /= nl;
        MeshVertex& o = m_skinned[v];
        o.x = pos[0]; o.y = pos[1]; o.z = pos[2]; o.nx = nrm[0]; o.ny = nrm[1]; o.nz = nrm[2];
    }
}

void ParaCanopy::Draw(D3DRenderer& r) {
    if (!m_ready) return;
    for (D3DRenderMesh* me : m_meshes) {
        r.UpdateMeshVertices(me, m_skinned.data(), (uint32_t)m_skinned.size());
        r.DrawMesh(me);
    }
}
