// Skinned RenderWare clump loader. See SkinnedDff.h.

#include "SkinnedDff.h"

#include <cstring>

namespace {

constexpr uint32_t kStruct = 0x01, kExtension = 0x03, kClump = 0x10, kFrameList = 0x0E,
                   kGeometryList = 0x1A, kGeometry = 0x0F, kHAnim = 0x11E, kSkin = 0x116,
                   kFrameName = 0x253F2FE;

struct Cursor {
    const uint8_t* d;
    size_t size;
    template <typename T> bool Get(size_t at, T& out) const {
        if (at + sizeof(T) > size) return false;
        std::memcpy(&out, d + at, sizeof(T));
        return true;
    }
    bool Header(size_t at, uint32_t& type, uint32_t& len) const {
        return Get(at, type) && Get(at + 4, len) && at + 12 + len <= size;
    }
};

RwMat ReadRwMatrix(const Cursor& c, size_t at) {   // RwMatrix: 4 x (vec3 + pad)
    RwMat m;
    for (int r = 0; r < 4; r++)
        for (int k = 0; k < 3; k++) c.Get(at + r * 16 + k * 4, m.m[r][k]);
    return m;
}

void ParseFrameList(const Cursor& c, size_t at, size_t end, SkinnedClump& out) {
    uint32_t type, len;
    if (!c.Header(at, type, len) || type != kStruct) return;
    uint32_t count = 0;
    c.Get(at + 12, count);
    if (count > 512) return;
    out.frames.resize(count);
    for (uint32_t i = 0; i < count; i++) {
        size_t f = at + 16 + i * 56;
        SkinFrame& fr = out.frames[i];
        for (int r = 0; r < 3; r++)
            for (int k = 0; k < 3; k++) c.Get(f + (r * 3 + k) * 4, fr.local.m[r][k]);
        for (int k = 0; k < 3; k++) c.Get(f + 36 + k * 4, fr.local.m[3][k]);
        int32_t parent = -1;
        c.Get(f + 48, parent);
        fr.parent = parent;
    }
    // One extension chunk per frame: frame name + RpHAnim node.
    int rootFrame = -1;
    size_t p = at + 12 + len;
    for (uint32_t i = 0; i < count && p + 12 <= end; i++) {
        if (!c.Header(p, type, len) || type != kExtension) break;
        size_t e = p + 12, eEnd = e + len;
        while (e + 12 <= eEnd) {
            uint32_t pt, pl;
            if (!c.Header(e, pt, pl)) break;
            if (pt == kFrameName) {
                out.frames[i].name.assign((const char*)c.d + e + 12, pl);
                while (!out.frames[i].name.empty() && out.frames[i].name[0] == ' ')
                    out.frames[i].name.erase(0, 1);
            } else if (pt == kHAnim) {
                int32_t id = -1;
                uint32_t numNodes = 0;
                c.Get(e + 16, id);
                c.Get(e + 20, numNodes);
                out.frames[i].boneId = id;
                if (numNodes > 0 && numNodes < 256) {   // the hierarchy's node list
                    rootFrame = (int)i;
                    out.nodeIds.resize(numNodes);
                    for (uint32_t n = 0; n < numNodes; n++) {
                        int32_t nid = 0;
                        c.Get(e + 32 + n * 12, nid);
                        out.nodeIds[n] = nid;
                    }
                }
            }
            e += 12 + pl;
        }
        p += 12 + len;
    }
    // The frame-name plugin follows the HAnim one, so name the root afterwards.
    if (rootFrame >= 0) out.rootName = out.frames[rootFrame].name;
}

bool ParseGeometry(const Cursor& c, size_t at, size_t end, SkinnedClump& out) {
    uint32_t type, len;
    if (!c.Header(at, type, len) || type != kStruct) return false;
    uint32_t flags = 0;
    int32_t numTris = 0, numVerts = 0, numMorph = 0;
    c.Get(at + 12, flags);
    c.Get(at + 16, numTris);
    c.Get(at + 20, numVerts);
    c.Get(at + 24, numMorph);
    if (numVerts <= 0 || numVerts > 65535 || numTris <= 0) return false;
    size_t p = at + 28;
    if (flags & 0x08) p += (size_t)numVerts * 4;                 // prelit colours
    uint32_t numTex = (flags >> 16) & 0xFF;
    if (numTex == 0) numTex = (flags & 0x80) ? 2 : (flags & 0x04) ? 1 : 0;
    out.vertices.assign(numVerts, SkinVertex{});
    for (int32_t v = 0; v < numVerts && numTex; v++) {
        c.Get(p + v * 8, out.vertices[v].uv[0]);
        c.Get(p + v * 8 + 4, out.vertices[v].uv[1]);
    }
    p += (size_t)numTex * numVerts * 8;
    out.indices.reserve((size_t)numTris * 3);
    for (int32_t t = 0; t < numTris; t++) {
        uint16_t v2 = 0, v1 = 0, v3 = 0;   // stored (v2, v1, materialId, v3)
        c.Get(p + t * 8, v2);
        c.Get(p + t * 8 + 2, v1);
        c.Get(p + t * 8 + 6, v3);
        out.indices.push_back(v1);
        out.indices.push_back(v2);
        out.indices.push_back(v3);
    }
    p += (size_t)numTris * 8;
    // Morph target 0: bounding sphere, hasVertices, hasNormals, data.
    uint32_t hasVerts = 0, hasNormals = 0;
    c.Get(p + 16, hasVerts);
    c.Get(p + 20, hasNormals);
    p += 24;
    if (hasVerts) {
        for (int32_t v = 0; v < numVerts; v++)
            for (int k = 0; k < 3; k++) c.Get(p + v * 12 + k * 4, out.vertices[v].pos[k]);
        p += (size_t)numVerts * 12;
    }
    if (hasNormals) {
        for (int32_t v = 0; v < numVerts; v++)
            for (int k = 0; k < 3; k++) c.Get(p + v * 12 + k * 4, out.vertices[v].normal[k]);
    }

    // Skip the material list; find the geometry extension's RpSkin data.
    size_t q = at + 12 + len;
    while (q + 12 <= end) {
        if (!c.Header(q, type, len)) break;
        if (type == kExtension) {
            size_t e = q + 12, eEnd = e + len;
            while (e + 12 <= eEnd) {
                uint32_t pt, pl;
                if (!c.Header(e, pt, pl)) break;
                if (pt == kSkin) {
                    size_t s = e + 12;
                    uint8_t numBones = 0, numUsed = 0;
                    c.Get(s, numBones);
                    c.Get(s + 1, numUsed);
                    s += 4 + numUsed;
                    for (int32_t v = 0; v < numVerts; v++)
                        for (int k = 0; k < 4; k++) c.Get(s + v * 4 + k, out.vertices[v].bone[k]);
                    s += (size_t)numVerts * 4;
                    for (int32_t v = 0; v < numVerts; v++)
                        for (int k = 0; k < 4; k++) c.Get(s + v * 16 + k * 4, out.vertices[v].weight[k]);
                    s += (size_t)numVerts * 16;
                    out.inverseBind.resize(numBones);
                    for (uint32_t b = 0; b < numBones; b++) {
                        if (numUsed == 0) s += 4;   // 0xDEADDEAD marker
                        out.inverseBind[b] = ReadRwMatrix(c, s);
                        s += 64;
                    }
                }
                e += 12 + pl;
            }
        }
        q += 12 + len;
    }
    return !out.inverseBind.empty();
}

}  // namespace

RwMat RwMat::operator*(const RwMat& b) const {
    RwMat r;
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 3; k++) {
            r.m[i][k] = m[i][0] * b.m[0][k] + m[i][1] * b.m[1][k] + m[i][2] * b.m[2][k];
            if (i == 3) r.m[i][k] += b.m[3][k];
        }
    }
    return r;
}

void RwMat::Apply(const float in[3], float out[3]) const {
    for (int k = 0; k < 3; k++)
        out[k] = in[0] * m[0][k] + in[1] * m[1][k] + in[2] * m[2][k] + m[3][k];
}

void RwMat::ApplyDir(const float in[3], float out[3]) const {
    for (int k = 0; k < 3; k++)
        out[k] = in[0] * m[0][k] + in[1] * m[1][k] + in[2] * m[2][k];
}

std::vector<SkinnedClump> SkinnedDff::LoadClumps(const uint8_t* data, size_t size) {
    std::vector<SkinnedClump> clumps;
    Cursor c{data, size};
    size_t at = 0;
    uint32_t type, len;
    while (c.Header(at, type, len) && type == kClump) {
        SkinnedClump clump;
        size_t p = at + 12, end = at + 12 + len;
        bool geom = false;
        while (p + 12 <= end) {
            uint32_t t, l;
            if (!c.Header(p, t, l)) break;
            if (t == kFrameList) {
                ParseFrameList(c, p + 12, p + 12 + l, clump);
            } else if (t == kGeometryList && !geom) {
                size_t g = p + 12, gEnd = p + 12 + l;
                while (g + 12 <= gEnd) {
                    uint32_t gt, gl;
                    if (!c.Header(g, gt, gl)) break;
                    if (gt == kGeometry) { geom = ParseGeometry(c, g + 12, g + 12 + gl, clump); break; }
                    g += 12 + gl;
                }
            }
            p += 12 + l;
        }
        clump.valid = geom && !clump.frames.empty() && !clump.nodeIds.empty();
        clumps.push_back(std::move(clump));
        at = end;
    }
    return clumps;
}
