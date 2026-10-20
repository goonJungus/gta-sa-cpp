#include "Cutscene.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "ImgLoader.h"
#include "TxdLoader.h"

namespace {

constexpr float kPi = 3.14159265f;

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
std::string Upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
std::string Trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

void QuatToRows(const float q[4], RwMat& m) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    m.m[0][0] = 1 - 2 * (y * y + z * z); m.m[0][1] = 2 * (x * y + w * z); m.m[0][2] = 2 * (x * z - w * y);
    m.m[1][0] = 2 * (x * y - w * z); m.m[1][1] = 1 - 2 * (x * x + z * z); m.m[1][2] = 2 * (y * z + w * x);
    m.m[2][0] = 2 * (x * z + w * y); m.m[2][1] = 2 * (y * z - w * x); m.m[2][2] = 1 - 2 * (x * x + y * y);
}

RwMat Invert(const RwMat& a) {
    const float (*m)[3] = a.m;
    const float c00 = m[1][1] * m[2][2] - m[1][2] * m[2][1], c01 = m[1][2] * m[2][0] - m[1][0] * m[2][2],
                c02 = m[1][0] * m[2][1] - m[1][1] * m[2][0];
    const float det = m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02;
    const float id = std::fabs(det) > 1e-12f ? 1.0f / det : 0.0f;
    RwMat r;
    r.m[0][0] = c00 * id;
    r.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * id;
    r.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * id;
    r.m[1][0] = c01 * id;
    r.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * id;
    r.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * id;
    r.m[2][0] = c02 * id;
    r.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * id;
    r.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * id;
    for (int j = 0; j < 3; j++)
        r.m[3][j] = -(m[3][0] * r.m[0][j] + m[3][1] * r.m[1][j] + m[3][2] * r.m[2][j]);
    return r;
}

// Every number of a spline file in order: '#' lines skipped, separators and the 'f' suffix dropped.
std::vector<float> SplineNumbers(const std::vector<uint8_t>& bytes, std::vector<int>& sectionEnds) {
    std::vector<float> out;
    std::string text((const char*)bytes.data(), bytes.size());
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        std::string tok;
        auto flush = [&] {
            if (tok.empty()) return;
            out.push_back((float)std::atof(tok.c_str()));
            tok.clear();
        };
        for (char ch : line) {
            if (ch == ';') { flush(); sectionEnds.push_back((int)out.size()); }
            else if (ch == ',' || ch == ' ' || ch == '\t' || ch == 'f') flush();
            else tok += ch;
        }
        flush();
    }
    return out;
}

}  // namespace

float Cutscene::Eval1(const std::vector<Key1>& k, float t) {
    if (k.empty()) return 0.0f;
    if (t <= k.front().t) return k.front().v[0];
    size_t i = 1;
    while (i < k.size() && k[i].t <= t) i++;
    if (i >= k.size()) return k.back().v[0];
    const Key1& a = k[i - 1];
    const Key1& b = k[i];
    const float u = std::clamp((t - a.t) / std::max(1e-6f, b.t - a.t), 0.0f, 1.0f);
    const float p0 = a.v[0], p1 = a.v[2], p2 = b.v[1], p3 = b.v[0];
    if (p0 == p1) return p0 + (p3 - p0) * u;
    const float v = 1 - u;
    return v * v * v * p0 + 3 * u * v * v * p1 + 3 * u * u * v * p2 + u * u * u * p3;
}

void Cutscene::Eval3(const std::vector<Key3>& k, float t, float out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (k.empty()) return;
    if (t <= k.front().t) { for (int i = 0; i < 3; i++) out[i] = k.front().v[0][i]; return; }
    size_t i = 1;
    while (i < k.size() && k[i].t <= t) i++;
    if (i >= k.size()) { for (int j = 0; j < 3; j++) out[j] = k.back().v[0][j]; return; }
    const Key3& a = k[i - 1];
    const Key3& b = k[i];
    const float u = std::clamp((t - a.t) / std::max(1e-6f, b.t - a.t), 0.0f, 1.0f);
    const float v = 1 - u;
    bool linear = true;
    for (int j = 0; j < 3; j++) if (a.v[0][j] != a.v[2][j]) linear = false;
    for (int j = 0; j < 3; j++) {
        const float p0 = a.v[0][j], p1 = a.v[2][j], p2 = b.v[1][j], p3 = b.v[0][j];
        out[j] = linear ? p0 + (p3 - p0) * u : v * v * v * p0 + 3 * u * v * v * p1 + 3 * u * u * v * p2 + u * u * u * p3;
    }
}

bool Cutscene::BuildObject(D3DRenderer& r, Obj& o, const std::vector<uint8_t>& dff, const std::vector<uint8_t>& txd) {
    for (SkinnedClump& c : SkinnedDff::LoadClumps(dff.data(), dff.size()))
        if (c.valid) { o.clump = std::move(c); break; }
    SkinnedClump& c = o.clump;
    if (!c.valid || c.inverseBind.size() < c.nodeIds.size() || c.vertices.empty()) return false;
    o.frameOfBone.assign(8192, -1);
    for (size_t f = 0; f < c.frames.size(); f++)
        if (c.frames[f].boneId >= 0 && c.frames[f].boneId < 8192) o.frameOfBone[(size_t)c.frames[f].boneId] = (int)f;
    o.skeletonLocal.resize(c.frames.size());
    for (size_t f = 0; f < c.frames.size(); f++) o.skeletonLocal[f] = c.frames[f].local;
    std::vector<int> nodeOfBone(8192, -1);
    for (size_t n = 0; n < c.nodeIds.size(); n++)
        if (c.nodeIds[n] >= 0 && c.nodeIds[n] < 8192) nodeOfBone[(size_t)c.nodeIds[n]] = (int)n;
    // A bone's rest offset from its parent comes from the skin matrices (SkinGetBonePositionsToTable).
    for (size_t n = 1; n < c.nodeIds.size(); n++) {
        if (c.nodeIds[n] < 0 || c.nodeIds[n] >= 8192) continue;
        const int f = o.frameOfBone[(size_t)c.nodeIds[n]];
        if (f < 0) continue;
        int pf = c.frames[(size_t)f].parent, pn = -1;
        while (pf >= 0 && (pn = c.frames[(size_t)pf].boneId >= 0 ? nodeOfBone[(size_t)c.frames[(size_t)pf].boneId] : -1) < 0)
            pf = c.frames[(size_t)pf].parent;
        const float zero[3] = {0, 0, 0};
        float bonePos[3], local[3];
        Invert(c.inverseBind[n]).Apply(zero, bonePos);
        c.inverseBind[(size_t)(pn >= 0 ? pn : 0)].Apply(bonePos, local);
        for (int k = 0; k < 3; k++) o.skeletonLocal[(size_t)f].m[3][k] = local[k];
    }
    o.boneFrame.resize(c.nodeIds.size(), 0);
    for (size_t n = 0; n < c.nodeIds.size(); n++) {
        const int f = c.nodeIds[n] >= 0 && c.nodeIds[n] < 8192 ? o.frameOfBone[(size_t)c.nodeIds[n]] : -1;
        o.boneFrame[n] = f >= 0 ? f : 0;
    }
    o.skinned.resize(c.vertices.size());
    for (size_t v = 0; v < c.vertices.size(); v++) {
        const SkinVertex& sv = c.vertices[v];
        o.skinned[v] = {sv.pos[0], sv.pos[1], sv.pos[2], sv.normal[0], sv.normal[1], sv.normal[2], 0xFFFFFFFFu, sv.uv[0], sv.uv[1]};
    }
    std::vector<TxdTexture> tex = m_tex;
    const size_t numMat = std::max<size_t>(1, c.materialTexture.size());
    for (size_t mi = 0; mi < numMat; mi++) {
        std::vector<uint16_t> idx;
        for (size_t t = 0; t < c.indices.size() / 3; t++) {
            const size_t tm = t < c.triMaterial.size() ? c.triMaterial[t] : 0;
            if (tm != mi) continue;
            idx.insert(idx.end(), c.indices.begin() + (long)(t * 3), c.indices.begin() + (long)(t * 3 + 3));
        }
        if (idx.empty()) continue;
        if (mi < c.materialColor.size())   // the material colour modulates the texture
            for (uint16_t vi : idx) o.skinned[vi].color = 0xFF000000u | c.materialColor[mi];
        D3DRenderMesh* me = r.CreateMesh(o.skinned.data(), (uint32_t)o.skinned.size(), idx.data(), (uint32_t)idx.size());
        if (!me) continue;
        const std::string want = mi < c.materialTexture.size() ? Lower(c.materialTexture[mi]) : std::string();
        const TxdTexture* pick = nullptr;
        for (const TxdTexture& t : tex) if (!want.empty() && Lower(t.name) == want) { pick = &t; break; }
        if (!pick && !want.empty() && !tex.empty()) pick = &tex[0];
        if (pick) {
            IDirect3DTexture9* tx = r.CreateTexture(pick->width, pick->height, pick->d3dFormat, pick->mip0.data(), (uint32_t)pick->mip0.size(), &pick->extraMips);
            me->texture = tx;
            o.textures.push_back(tx);
        }
        o.meshes.push_back(me);
    }
    return !o.meshes.empty();
}

// Cars and props of a cutscene are plain DFFs; the clip's first sequence moves the whole model.
bool Cutscene::BuildRigid(D3DRenderer& r, Obj& o, const std::vector<uint8_t>& dff, const std::vector<uint8_t>& txd) {
    const DffModel m = DffLoader::LoadFromMemory(dff.data(), dff.size());
    if (!m.valid) return false;
    std::vector<TxdTexture> tex = m_tex;
    for (const DffMesh& dm0 : m.meshes) {
        if (dm0.vertices.empty() || dm0.indices.empty()) continue;
        DffMesh dm = dm0;
        for (DffVertex& v : dm.vertices) v.color = 0xFF000000u | (dm.materialColor & 0x00FFFFFFu);   // the material colour modulates the texture
        D3DRenderMesh* me = r.CreateMesh(reinterpret_cast<const MeshVertex*>(dm.vertices.data()), (uint32_t)dm.vertices.size(), dm.indices.data(), (uint32_t)dm.indices.size());
        if (!me) continue;
        const std::string want = Lower(dm.textureName);
        const TxdTexture* pick = nullptr;
        for (const TxdTexture& t : tex) if (!want.empty() && Lower(t.name) == want) { pick = &t; break; }
        if (!pick && !want.empty() && !tex.empty()) pick = &tex[0];
        if (pick) {
            IDirect3DTexture9* tx = r.CreateTexture(pick->width, pick->height, pick->d3dFormat, pick->mip0.data(), (uint32_t)pick->mip0.size(), &pick->extraMips);
            me->texture = tx;
            o.textures.push_back(tx);
        }
        o.meshes.push_back(me);
    }
    o.rigid = !o.meshes.empty();
    return o.rigid;
}

bool Cutscene::Load(D3DRenderer& r, const std::string& gameDir, const std::string& name) {
    Free(r);
    const std::string n = Lower(name);
    ImgLoader cuts;
    if (!cuts.Open(gameDir + "/anim/cuts.img")) { std::fprintf(stderr, "[CUT] cuts.img not found\n"); return false; }
    const std::vector<uint8_t> cut = cuts.Extract(n + ".cut"), dat = cuts.Extract(n + ".dat"), ifp = cuts.Extract(n + ".ifp");
    if (cut.empty() || ifp.empty()) { std::fprintf(stderr, "[CUT] %s: files missing (cut %zu ifp %zu)\n", n.c_str(), cut.size(), ifp.size()); return false; }
    if (!IfpLoader::LoadAnpk(ifp, m_clips)) { std::fprintf(stderr, "[CUT] %s: ifp unreadable\n", n.c_str()); return false; }
    for (const auto& kv : m_clips) m_duration = std::max(m_duration, kv.second.duration);

    // NAME.cut: sections from a name to "end", commas as spaces (CCutsceneMgr::LoadCutSceneFile)
    ImgLoader models, gta3;
    const bool haveModels = models.Open(gameDir + "/models/cutscene.img");
    gta3.Open(gameDir + "/models/gta3.img");
    std::unordered_map<std::string, std::string> parents;
    {
        std::ifstream pf(gameDir + "/data/txdcut.ide");
        std::string pl;
        bool inP = false;
        while (std::getline(pf, pl)) {
            pl = Trim(pl);
            for (char& ch : pl) if (ch == ',') ch = ' ';
            if (pl.empty() || pl[0] == '#') continue;
            if (Lower(pl) == "txdp") { inP = true; continue; }
            if (Lower(pl) == "end") { inP = false; continue; }
            std::istringstream ps(pl);
            std::string a, b;
            if (inP && (ps >> a >> b)) parents[Lower(a)] = Lower(b);
        }
    }
    const std::vector<uint8_t> sharedVeh = gta3.Extract("vehicle.txd");
    std::istringstream in(std::string((const char*)cut.data(), cut.size()));
    std::string line, section;
    while (std::getline(in, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        for (char& ch : line) if (ch == ',') ch = ' ';
        if (section.empty()) { section = Lower(line); continue; }
        if (Lower(line) == "end") { section.clear(); continue; }
        std::istringstream ls(line);
        if (section == "info") {
            std::string key;
            ls >> key;
            if (Lower(key) == "offset") ls >> m_offset[0] >> m_offset[1] >> m_offset[2];
        } else if (section == "model") {
            int idx;
            std::string model, anim;
            ls >> idx >> model;
            if (model.empty()) continue;
            Obj o;
            o.model = Lower(model);
            while (ls >> anim) { o.clip = nullptr; break; }
            auto ck = m_clips.find(o.model);
            if (ck == m_clips.end() && !anim.empty()) ck = m_clips.find(Lower(anim));
            if (ck != m_clips.end()) o.clip = &ck->second;
            o.isCj = o.model == "csplay";
            if (!o.isCj) {
                std::vector<uint8_t> dff, txd;
                if (haveModels) { dff = models.Extract(o.model + ".dff"); txd = models.Extract(o.model + ".txd"); }
                // data\txdcut.ide "txdp": a cutscene model's TXD may name a parent whose textures it borrows
                m_tex.clear();
                if (!txd.empty()) m_tex = TxdLoader::LoadFromMemory(txd.data(), txd.size());
                for (std::string p = o.model; parents.count(p);) {
                    p = parents[p];
                    std::vector<uint8_t> pb = gta3.Extract(p + ".txd");
                    if (pb.empty() && haveModels) pb = models.Extract(p + ".txd");
                    if (pb.empty()) break;
                    for (TxdTexture& t : TxdLoader::LoadFromMemory(pb.data(), pb.size())) m_tex.push_back(std::move(t));
                }
                if (!dff.empty() && parents.count(o.model) && !sharedVeh.empty())   // the shared vehicle textures every car uses
                    for (TxdTexture& t : TxdLoader::LoadFromMemory(sharedVeh.data(), sharedVeh.size())) m_tex.push_back(std::move(t));
                if (dff.empty() || !(BuildObject(r, o, dff, txd) || BuildRigid(r, o, dff, txd)))
                    std::fprintf(stderr, "[CUT] %s: model %s did not build\n", n.c_str(), o.model.c_str());
            }
            m_objs.push_back(std::move(o));
        } else if (section == "text") {
            float st, du;
            std::string key;
            ls >> st >> du >> key;
            if (!key.empty()) m_text.push_back({st, du, Upper(key)});
        }
    }
    // unordered_map keeps node addresses, but a later Load must not leave dangling clip pointers
    for (Obj& o : m_objs) {
        auto ck = m_clips.find(o.model);
        if (ck != m_clips.end()) o.clip = &ck->second;
    }

    // NAME.dat: FOV, roll, camera source, camera target
    if (!dat.empty()) {
        std::vector<int> ends;
        const std::vector<float> v = SplineNumbers(dat, ends);
        size_t p = 0;
        for (int sec = 0; sec < 4 && p < v.size(); sec++) {
            const int count = (int)v[p++];
            const size_t width = sec < 2 ? 4 : 10;
            for (int i = 0; i < count && p + width <= v.size(); i++, p += width) {
                if (sec < 2) {
                    Key1 k;
                    k.t = v[p];
                    for (int j = 0; j < 3; j++) k.v[j] = v[p + 1 + (size_t)j];
                    (sec == 0 ? m_fov : m_roll).push_back(k);
                } else {
                    Key3 k;
                    k.t = v[p];
                    for (int a = 0; a < 3; a++)
                        for (int b = 0; b < 3; b++) k.v[a][b] = v[p + 1 + (size_t)(a * 3 + b)];
                    (sec == 2 ? m_src : m_dst).push_back(k);
                }
            }
        }
        if (!m_src.empty()) m_duration = std::max(m_duration, m_src.back().t);
    }
    m_loaded = true;
    std::fprintf(stderr, "[CUT] %s: %zu objects, %zu clips, %zu text, camera keys fov %zu roll %zu src %zu dst %zu, %.2f s, offset %.2f %.2f %.2f\n",
                 n.c_str(), m_objs.size(), m_clips.size(), m_text.size(), m_fov.size(), m_roll.size(), m_src.size(), m_dst.size(),
                 m_duration, m_offset[0], m_offset[1], m_offset[2]);
    return true;
}

void Cutscene::Free(D3DRenderer& r) {
    for (Obj& o : m_objs) {
        for (D3DRenderMesh* me : o.meshes) r.DestroyMesh(me);
        for (IDirect3DTexture9* t : o.textures) if (t) r.DestroyTexture(t);
    }
    m_objs.clear();
    m_text.clear();
    m_clips.clear();
    m_fov.clear(); m_roll.clear(); m_src.clear(); m_dst.clear();
    m_loaded = m_running = m_skipped = false;
    m_ms = m_duration = 0.0f;
    m_offset[0] = m_offset[1] = m_offset[2] = 0.0f;
}

void Cutscene::Update(float dt) {
    if (!m_running) return;
    m_ms += dt * 1000.0f;
    if (m_ms >= m_duration * 1000.0f) { m_ms = m_duration * 1000.0f; m_running = false; }
}

bool Cutscene::Camera(CutsceneCamera& out) const {
    if (m_src.empty() || m_dst.empty()) return false;
    const float t = m_ms / 1000.0f;
    float src[3], dst[3];
    Eval3(m_src, t, src);
    Eval3(m_dst, t, dst);
    const float roll = Eval1(m_roll, t) * kPi / 180.0f;
    float f[3], len = 0;
    for (int i = 0; i < 3; i++) { f[i] = dst[i] - src[i]; len += f[i] * f[i]; }
    len = std::sqrt(len);
    if (len < 1e-6f) return false;
    for (float& v : f) v /= len;
    float up[3] = {std::cos(roll + kPi * 0.5f), 0.0f, std::sin(roll + kPi * 0.5f)};
    float right[3] = {up[1] * f[2] - up[2] * f[1], up[2] * f[0] - up[0] * f[2], up[0] * f[1] - up[1] * f[0]};
    float rl = std::sqrt(right[0] * right[0] + right[1] * right[1] + right[2] * right[2]);
    if (rl < 1e-6f) { up[0] = 0; up[1] = 1; up[2] = 0; right[0] = f[1] * 0 - f[2]; right[1] = 0; right[2] = f[0]; rl = std::sqrt(right[0] * right[0] + right[2] * right[2]); }
    for (float& v : right) v /= std::max(rl, 1e-6f);
    up[0] = f[1] * right[2] - f[2] * right[1];
    up[1] = f[2] * right[0] - f[0] * right[2];
    up[2] = f[0] * right[1] - f[1] * right[0];
    for (int i = 0; i < 3; i++) {
        out.pos[i] = src[i] + m_offset[i];
        out.look[i] = dst[i] + m_offset[i];
        out.up[i] = up[i];
    }
    if (std::getenv("GTA_CUTLOG")) { static int ls = -1; if ((int)t != ls) { ls = (int)t; std::fprintf(stderr, "[CUT] cam t=%d roll %.2f up %.2f %.2f %.2f\n", ls, roll * 180.0f / kPi, up[0], up[1], up[2]); } }
    out.fov = m_fov.empty() ? 70.0f : Eval1(m_fov, t);
    return true;
}

std::string Cutscene::Subtitle() const {
    for (const Text& t : m_text)
        if (m_ms >= t.start && m_ms < t.start + t.dur) return t.key;
    return std::string();
}

void Cutscene::Pose(Obj& o, float seconds) {
    const SkinnedClump& c = o.clump;
    o.globals.assign(c.frames.size(), RwMat());
    for (size_t f = 1; f < c.frames.size(); f++) {
        RwMat local = o.skeletonLocal[f];
        const AnimSequence* s = o.clip && c.frames[f].boneId >= 0 ? o.clip->FindBone(c.frames[f].boneId) : nullptr;
        if (s && !s->keys.empty()) {
            float q[4], t[3];
            s->Sample(seconds, q, t);
            QuatToRows(q, local);
            if (s->hasTranslation) for (int i = 0; i < 3; i++) local.m[3][i] = t[i];
        }
        const int p = c.frames[f].parent;
        o.globals[f] = (p > 0 && p < (int)f) ? local * o.globals[(size_t)p] : local;
    }
    std::vector<RwMat> skin(c.inverseBind.size());
    for (size_t n = 0; n < skin.size(); n++) skin[n] = c.inverseBind[n] * o.globals[(size_t)o.boneFrame[n]];
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
        if (nl > 1e-6f) for (float& v2 : nrm) v2 /= nl;
        MeshVertex& m = o.skinned[v];
        m.x = pos[0]; m.y = pos[1]; m.z = pos[2]; m.nx = nrm[0]; m.ny = nrm[1]; m.nz = nrm[2];
    }
}

void Cutscene::Draw(D3DRenderer& r, const DrawCj& cj) {
    if (!m_loaded) return;
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    const float seconds = m_ms / 1000.0f;
    static int s_logSec = -1;
    const bool log = std::getenv("GTA_CUTLOG") && (int)seconds != s_logSec;
    if (log) s_logSec = (int)seconds;
    for (Obj& o : m_objs) {
        if (log) {
            float rp[3] = {0, 0, 0}, rq[4] = {0, 0, 0, 1}, rt[3] = {0, 0, 0};
            if (o.clip) if (const AnimSequence* rs = o.clip->FindBone(0)) rs->Sample(seconds, rq, rt);
            std::fprintf(stderr, "[CUT] t=%d %s clip %s root t %.2f %.2f %.2f q %.2f %.2f %.2f %.2f meshes %zu\n", (int)seconds, o.model.c_str(),
                         o.clip ? o.clip->name.c_str() : "-", rt[0], rt[1], rt[2], rq[0], rq[1], rq[2], rq[3], o.meshes.size());
            (void)rp;
        }
        // SetupCutsceneToStart lifts the offset by 1 m for the actors; the camera keeps the plain offset.
        const float origin[3] = {m_offset[0], m_offset[1], m_offset[2] + 1.0f};
        if (o.isCj) { if (cj) cj(r, o.clip, seconds, origin); continue; }
        if (o.meshes.empty()) continue;
        if (o.rigid) {
            RwMat m;
            const AnimSequence* s = o.clip && !o.clip->sequences.empty() ? &o.clip->sequences[0] : nullptr;
            if (s && !s->keys.empty()) {
                float q[4], t[3];
                s->Sample(seconds, q, t);
                QuatToRows(q, m);
                if (s->hasTranslation) for (int i = 0; i < 3; i++) m.m[3][i] = t[i];
            }
            D3DMATRIX w = {m.m[0][0], m.m[0][1], m.m[0][2], 0, m.m[1][0], m.m[1][1], m.m[1][2], 0, m.m[2][0], m.m[2][1], m.m[2][2], 0,
                           m.m[3][0] + origin[0], m.m[3][1] + origin[1], m.m[3][2] + origin[2], 1};
            dev->SetTransform(D3DTS_WORLD, &w);
            for (D3DRenderMesh* me : o.meshes) r.DrawMesh(me);
            continue;
        }
        Pose(o, seconds);
        if (log) {
            float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
            int zeroW = 0;
            for (size_t vi = 0; vi < o.skinned.size(); vi++) {
                const MeshVertex& mv = o.skinned[vi];
                const SkinVertex& sw = o.clump.vertices[vi];
                if (sw.weight[0] + sw.weight[1] + sw.weight[2] + sw.weight[3] < 0.5f) { zeroW++; continue; }
                const float v3[3] = {mv.x, mv.y, mv.z};
                for (int i = 0; i < 3; i++) { lo[i] = std::min(lo[i], v3[i]); hi[i] = std::max(hi[i], v3[i]); }
            }
            std::fprintf(stderr, "[CUT] t=%d %s bounds (%d of %zu unweighted) %.2f %.2f %.2f .. %.2f %.2f %.2f\n", (int)seconds, o.model.c_str(), zeroW, o.skinned.size(), lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
        }
        D3DMATRIX w = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, origin[0], origin[1], origin[2], 1};
        dev->SetTransform(D3DTS_WORLD, &w);
        for (D3DRenderMesh* me : o.meshes) {
            r.UpdateMeshVertices(me, o.skinned.data(), (uint32_t)o.skinned.size());
            r.DrawMesh(me);
        }
    }
}

bool Cutscene::BoneWorld(size_t obj, int boneId, float out[3]) const {
    if (obj >= m_objs.size()) return false;
    const Obj& o = m_objs[obj];
    const int f = boneId >= 0 && boneId < 8192 && !o.frameOfBone.empty() ? o.frameOfBone[(size_t)boneId] : -1;
    if (f < 0 || f >= (int)o.globals.size()) return false;
    for (int i = 0; i < 3; i++) out[i] = o.globals[(size_t)f].m[3][i] + m_offset[i] + (i == 2 ? 1.0f : 0.0f);
    return true;
}
