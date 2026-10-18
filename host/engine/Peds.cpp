#include "Peds.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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
std::string Trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
std::string ReadText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

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

}  // namespace

bool Peds::Init(D3DRenderer&, const std::string& gameDir) {
    m_dir = gameDir;
    {   // data/peds.ide
        std::istringstream in(ReadText(gameDir + "/data/peds.ide"));
        bool peds = false;
        for (std::string line; std::getline(in, line);) {
            line = Trim(line);
            if (line.empty() || line[0] == '#') continue;
            if (line == "peds") { peds = true; continue; }
            if (line == "end") { peds = false; continue; }
            if (!peds) continue;
            std::vector<std::string> f;
            std::stringstream ss(line);
            for (std::string s; std::getline(ss, s, ',');) f.push_back(Trim(s));
            if (f.size() < 6) continue;
            PedInfo p;
            p.id = std::atoi(f[0].c_str());
            p.dff = Lower(f[1]); p.txd = Lower(f[2]); p.type = f[3]; p.group = Lower(f[5]);
            m_info[p.id] = p;
            if (p.id > 0 && (p.type == "CIVMALE" || p.type == "CIVFEMALE")) m_pool.push_back(p.id);
        }
    }
    {   // data/animgrp.dat: "name, ped, walkcycle, 6" then the clip names
        std::istringstream in(ReadText(gameDir + "/data/animgrp.dat"));
        std::string cur;
        for (std::string line; std::getline(in, line);) {
            line = Trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            if (line == "end") { cur.clear(); continue; }
            if (cur.empty()) {
                const size_t c = line.find(',');
                if (c == std::string::npos) continue;
                cur = Lower(Trim(line.substr(0, c)));
                if (line.find("ped") == std::string::npos) cur.clear();
                else m_groups[cur] = {};
            } else {
                m_groups[cur].push_back(Lower(line));
            }
        }
    }
    IfpLoader::Load(gameDir + "/anim/ped.ifp", m_clips);
    m_ready = !m_pool.empty() && !m_clips.empty();
    std::fprintf(stderr, "[PED] %zu ped models (%zu civilians), %zu animation groups, %zu clips\n", m_info.size(), m_pool.size(),
                 m_groups.size(), m_clips.size());
    return m_ready;
}

void Peds::Shutdown(D3DRenderer& r) {
    for (auto& kv : m_models) {
        for (D3DRenderMesh* me : kv.second.meshes) r.DestroyMesh(me);
        for (IDirect3DTexture9* t : kv.second.textures) if (t) r.DestroyTexture(t);
    }
    m_models.clear();
    m_actors.clear();
}

Peds::Model* Peds::GetModel(D3DRenderer& r, int id) {
    auto it = m_models.find(id);
    if (it != m_models.end()) return it->second.valid ? &it->second : nullptr;
    Model& m = m_models[id];
    auto pi = m_info.find(id);
    if (pi == m_info.end()) return nullptr;
    m.info = pi->second;
    ImgLoader img;
    if (!img.Open(m_dir + "/models/gta3.img")) return nullptr;
    const std::vector<uint8_t> dff = img.Extract(m.info.dff + ".dff"), txd = img.Extract(m.info.txd + ".txd");
    if (dff.empty()) return nullptr;
    for (SkinnedClump& c : SkinnedDff::LoadClumps(dff.data(), dff.size()))
        if (c.valid) { m.clump = std::move(c); break; }
    SkinnedClump& c = m.clump;
    if (!c.valid || c.inverseBind.size() < c.nodeIds.size() || c.vertices.empty()) return nullptr;
    m.frameOfBone.assign(512, -1);
    for (size_t f = 0; f < c.frames.size(); f++)
        if (c.frames[f].boneId >= 0 && c.frames[f].boneId < 512) m.frameOfBone[(size_t)c.frames[f].boneId] = (int)f;
    m.skeletonLocal.resize(c.frames.size());
    for (size_t f = 0; f < c.frames.size(); f++) m.skeletonLocal[f] = c.frames[f].local;
    std::vector<int> nodeOfBone(512, -1);
    for (size_t n = 0; n < c.nodeIds.size(); n++)
        if (c.nodeIds[n] >= 0 && c.nodeIds[n] < 512) nodeOfBone[(size_t)c.nodeIds[n]] = (int)n;
    // A bone's rest offset from its parent comes from the skin matrices (SkinGetBonePositionsToTable).
    for (size_t n = 1; n < c.nodeIds.size(); n++) {
        const int f = m.frameOfBone[(size_t)c.nodeIds[n]];
        if (f < 0) continue;
        int pf = c.frames[(size_t)f].parent, pn = -1;
        while (pf >= 0 && (pn = c.frames[(size_t)pf].boneId >= 0 ? nodeOfBone[(size_t)c.frames[(size_t)pf].boneId] : -1) < 0)
            pf = c.frames[(size_t)pf].parent;
        const float zero[3] = {0, 0, 0};
        float bonePos[3], local[3];
        Invert(c.inverseBind[n]).Apply(zero, bonePos);
        c.inverseBind[(size_t)(pn >= 0 ? pn : 0)].Apply(bonePos, local);
        for (int k = 0; k < 3; k++) m.skeletonLocal[(size_t)f].m[3][k] = local[k];
    }
    m.boneFrame.resize(c.nodeIds.size(), 0);
    for (size_t n = 0; n < c.nodeIds.size(); n++) {
        const int f = c.nodeIds[n] >= 0 && c.nodeIds[n] < 512 ? m.frameOfBone[(size_t)c.nodeIds[n]] : -1;
        m.boneFrame[n] = f >= 0 ? f : 0;
    }
    m.skinned.resize(c.vertices.size());
    m.feetZ = 0.0f;
    for (size_t v = 0; v < c.vertices.size(); v++) {
        const SkinVertex& sv = c.vertices[v];
        m.skinned[v] = {sv.pos[0], sv.pos[1], sv.pos[2], sv.normal[0], sv.normal[1], sv.normal[2], 0xFFFFFFFFu, sv.uv[0], sv.uv[1]};
        m.feetZ = std::min(m.feetZ, sv.pos[2]);
    }
    // One mesh per material, each with its own texture from the ped's dictionary.
    std::vector<TxdTexture> tex;
    if (!txd.empty()) tex = TxdLoader::LoadFromMemory(txd.data(), txd.size());
    auto lowerOf = [](std::string s) { for (char& ch : s) ch = (char)std::tolower((unsigned char)ch); return s; };
    const size_t numMat = std::max<size_t>(1, c.materialTexture.size());
    for (size_t mi = 0; mi < numMat; mi++) {
        std::vector<uint16_t> idx;
        for (size_t t = 0; t < c.indices.size() / 3; t++) {
            const size_t tm = t < c.triMaterial.size() ? c.triMaterial[t] : 0;
            if (tm != mi) continue;
            idx.insert(idx.end(), c.indices.begin() + (long)(t * 3), c.indices.begin() + (long)(t * 3 + 3));
        }
        if (idx.empty()) continue;
        D3DRenderMesh* me = r.CreateMesh(m.skinned.data(), (uint32_t)m.skinned.size(), idx.data(), (uint32_t)idx.size());
        if (!me) continue;
        const std::string want = mi < c.materialTexture.size() ? lowerOf(c.materialTexture[mi]) : std::string();
        const TxdTexture* pick = nullptr;
        for (const TxdTexture& t : tex) if (!want.empty() && lowerOf(t.name) == want) { pick = &t; break; }
        if (!pick && !want.empty() && !tex.empty()) pick = &tex[0];   // name missing from the dictionary
        if (pick) {
            IDirect3DTexture9* tx = r.CreateTexture(pick->width, pick->height, pick->d3dFormat, pick->mip0.data(), (uint32_t)pick->mip0.size(), &pick->extraMips);
            me->texture = tx;
            m.textures.push_back(tx);
        }
        m.meshes.push_back(me);
    }
    if (m.meshes.empty()) return nullptr;
    auto grp = m_groups.find(m.info.group);
    if (grp == m_groups.end()) grp = m_groups.find("man");
    if (grp != m_groups.end() && grp->second.size() >= 4) {
        auto find = [&](const std::string& n) -> const AnimClip* { auto k = m_clips.find(n); return k == m_clips.end() ? nullptr : &k->second; };
        m.walk = find(grp->second[0]);
        m.run = find(grp->second[1]);
        m.idle = find(grp->second[3]);
    }
    m.valid = true;
    return &m;
}

int Peds::Spawn(D3DRenderer& r, int id, float x, float y, float z, float heading) {
    if (!m_ready) return -1;
    if (id < 0) id = m_pool[(size_t)Rand((int)m_pool.size())];
    if (!GetModel(r, id)) return -1;
    Actor a;
    a.model = id; a.x = x; a.y = y; a.z = z; a.heading = heading;
    a.turnTimer = 2.0f + (float)Rand(40) * 0.1f;
    a.walking = Rand(3) != 0;
    a.animTime = (float)Rand(100) * 0.01f;
    m_actors.push_back(a);
    return (int)m_actors.size() - 1;
}

void Peds::Pose(Model& m, const AnimClip* clip, float time) {
    const SkinnedClump& c = m.clump;
    std::vector<RwMat> globals(c.frames.size());
    for (size_t f = 1; f < c.frames.size(); f++) {
        RwMat local = m.skeletonLocal[f];
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
            if (c.frames[f].boneId == 0) { local.m[3][0] = 0.0f; local.m[3][1] = 0.0f; }   // root motion is applied by the walk
        }
        const int p = c.frames[f].parent;
        globals[f] = (p > 0 && p < (int)f) ? local * globals[(size_t)p] : local;
    }
    std::vector<RwMat> skin(c.inverseBind.size());
    for (size_t n = 0; n < skin.size(); n++) skin[n] = c.inverseBind[n] * globals[(size_t)m.boneFrame[n]];
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
        MeshVertex& o = m.skinned[v];
        o.x = pos[0]; o.y = pos[1]; o.z = pos[2]; o.nx = nrm[0]; o.ny = nrm[1]; o.nz = nrm[2];
    }
}

void Peds::Update(float dt, const std::function<float(float, float, float)>& groundAt,
                  const std::function<bool(float, float, float, float, float)>& blocked, const Player& player) {
    for (size_t ai = 0; ai < m_actors.size(); ai++) {
        Actor& a = m_actors[ai];
        auto it = m_models.find(a.model);
        if (it == m_models.end()) continue;
        const Model& m = it->second;
        const float pdx = player.x - a.x, pdy = player.y - a.y, pd = std::sqrt(pdx * pdx + pdy * pdy);
        // Gunfire or an aimed weapon close by: run away from CJ for a while.
        if (player.threat && pd < 25.0f) a.fleeTimer = 6.0f;
        const bool fleeing = a.fleeTimer > 0.0f && m.run;
        if (a.fleeTimer > 0.0f) a.fleeTimer -= dt;
        a.turnTimer -= dt;
        // Face away from the threat, or look at CJ when he stands close and stop to watch him pass.
        bool watching = false;
        if (fleeing) {
            a.heading = std::atan2(-pdy, -pdx);
            a.walking = true;
        } else if (pd < 3.0f) {
            const float want = std::atan2(pdy, pdx);
            float d = want - a.heading;
            while (d > kPi) d -= 2.0f * kPi;
            while (d < -kPi) d += 2.0f * kPi;
            a.heading += d * std::min(1.0f, 4.0f * dt);
            a.walking = false;
            watching = true;
            a.turnTimer = std::max(a.turnTimer, 1.0f);
        } else if (a.turnTimer <= 0.0f) {   // pick a new heading and whether to stand or walk
            a.turnTimer = 3.0f + (float)Rand(50) * 0.1f;
            a.heading += ((float)Rand(200) - 100.0f) * 0.01f * kPi * 0.5f;
            a.walking = Rand(4) != 0;
        }
        const AnimClip* clip = fleeing ? m.run : (a.walking && m.walk ? m.walk : m.idle);
        const float dur = clip ? std::max(clip->duration, 0.1f) : 1.0f;
        a.animTime = std::fmod(a.animTime + dt, dur);
        if ((a.walking && m.walk) || fleeing) {
            const float speed = fleeing ? 4.2f : 1.4f;
            float h = a.heading;
            // Look ahead: when something stands a body length in front, veer to the clearer side.
            const float look = fleeing ? 1.6f : 1.0f;
            if (blocked && blocked(a.x + std::cos(h) * look, a.y + std::sin(h) * look, a.x, a.y, a.z)) {
                const float lx = std::cos(h + 1.0f), ly = std::sin(h + 1.0f), rx = std::cos(h - 1.0f), ry = std::sin(h - 1.0f);
                const bool leftBlocked = blocked(a.x + lx * look, a.y + ly * look, a.x, a.y, a.z);
                const bool rightBlocked = blocked(a.x + rx * look, a.y + ry * look, a.x, a.y, a.z);
                if (!leftBlocked) h += 1.0f; else if (!rightBlocked) h -= 1.0f; else h += kPi;
                a.heading = h;
                if (!fleeing) a.turnTimer = 2.0f;
            }
            // Walk around other pedestrians instead of through them.
            float sx = 0.0f, sy = 0.0f;
            for (size_t bi = 0; bi < m_actors.size(); bi++) {
                if (bi == ai) continue;
                const float dx = a.x - m_actors[bi].x, dy = a.y - m_actors[bi].y, d2 = dx * dx + dy * dy;
                if (d2 < 0.9f * 0.9f && d2 > 1e-4f) { const float d = std::sqrt(d2); sx += dx / d * (0.9f - d); sy += dy / d * (0.9f - d); }
            }
            const float nx = a.x + std::cos(a.heading) * speed * dt + sx * 2.0f * dt, ny = a.y + std::sin(a.heading) * speed * dt + sy * 2.0f * dt;
            if (blocked && blocked(nx, ny, a.x, a.y, a.z)) { a.turnTimer = 0.0f; a.heading += kPi * 0.5f; }
            else { a.x = nx; a.y = ny; }
        } else if (!watching) {
            // Standing pedestrians still give way to others who bump them.
            for (size_t bi = 0; bi < m_actors.size(); bi++) {
                if (bi == ai) continue;
                const float dx = a.x - m_actors[bi].x, dy = a.y - m_actors[bi].y, d2 = dx * dx + dy * dy;
                if (d2 < 0.6f * 0.6f && d2 > 1e-4f) {
                    const float d = std::sqrt(d2);
                    const float nx = a.x + dx / d * (0.6f - d) * 3.0f * dt, ny = a.y + dy / d * (0.6f - d) * 3.0f * dt;
                    if (!(blocked && blocked(nx, ny, a.x, a.y, a.z))) { a.x = nx; a.y = ny; }
                }
            }
        }
        if (groundAt) {
            const float g = groundAt(a.x, a.y, a.z + 1.0f);
            if (g > -1e5f) a.z += (g - a.z) * std::min(1.0f, 10.0f * dt);
        }
    }
}

void Peds::Draw(D3DRenderer& r) {
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    for (const Actor& a : m_actors) {
        auto it = m_models.find(a.model);
        if (it == m_models.end() || !it->second.valid) continue;
        Model& m = it->second;
        const AnimClip* clip = a.fleeTimer > 0.0f && m.run ? m.run : (a.walking && m.walk ? m.walk : m.idle);
        Pose(m, clip, a.animTime);
        // Stand the posed feet on the ground: the animation sets the pelvis height, the bind pose does not.
        float footZ = 1e9f;
        for (const MeshVertex& v : m.skinned) footZ = std::min(footZ, v.z);
        const float ang = a.heading - kPi * 0.5f, c = std::cos(ang), s = std::sin(ang);
        D3DMATRIX w = {c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, a.x, a.y, a.z - footZ, 1};
        dev->SetTransform(D3DTS_WORLD, &w);
        for (D3DRenderMesh* me : m.meshes) {
            r.UpdateMeshVertices(me, m.skinned.data(), (uint32_t)m.skinned.size());
            r.DrawMesh(me);
        }
    }
}

void Peds::Populate(D3DRenderer& r, float px, float py, float pz, int target,
                    const std::function<float(float, float, float)>& groundAt) {
    if (!m_ready) return;
    // Despawn: far away, or the whole crowd when the target is zero (only crowd members, not dev spawns).
    for (size_t i = m_actors.size(); i-- > 0;) {
        Actor& a = m_actors[i];
        if (!a.crowd) continue;
        const float dx = a.x - px, dy = a.y - py;
        if (target <= 0 || dx * dx + dy * dy > 90.0f * 90.0f) m_actors.erase(m_actors.begin() + (long)i);
    }
    int have = 0;
    for (const Actor& a : m_actors) have += a.crowd ? 1 : 0;
    // At most one spawn per call so a big gap fills over a few frames.
    if (have < target) {
        const float ang = (float)Rand(628) * 0.01f, dist = 12.0f + (float)Rand(250) * 0.1f;
        const float x = px + std::cos(ang) * dist, y = py + std::sin(ang) * dist;
        float g = groundAt ? groundAt(x, y, pz) : -1e9f;
        // Where the player's own spot has no collision either (a scene without streamed ground) use the player's height.
        if (g <= -1e5f && (!groundAt || groundAt(px, py, pz) <= -1e5f)) g = pz;
        if (g > -1e5f && std::fabs(g - pz) < 15.0f) {
            const int n = Spawn(r, -1, x, y, g, (float)Rand(628) * 0.01f);
            if (n >= 0) m_actors[(size_t)n].crowd = true;
        }
    }
}
