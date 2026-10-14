// COL3 collision loader for gtasa_cpp.exe.
// See ColLoader.h for the format reference.

#include "ColLoader.h"
#include <cstring>
#include <cctype>
#include <cmath>

namespace {

inline uint16_t rdU16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
inline uint32_t rdU32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline int16_t rdS16(const uint8_t* p) {
    return (int16_t)(p[0] | (p[1] << 8));
}

// Case-insensitive C-string compare against the fixed-size name field.
// The field is NUL-terminated but trailing bytes may be garbage (observed
// "Lae2_roads89\0\0LAe2..." in retail lae2_4.col), so stop at the first NUL.
bool NameEquals(const uint8_t* field, size_t fieldLen, const char* name) {
    size_t n = strlen(name);
    for (size_t i = 0; i < fieldLen; i++) {
        char a = (char)field[i];
        if (a == '\0')
            return i == n;   // field ended; match iff name ended here too
        if (i >= n)
            return false;    // field longer than name
        if (tolower((unsigned char)a) != tolower((unsigned char)name[i]))
            return false;
    }
    return n == fieldLen;     // no NUL in field: require exact-length match
}

// Rotate (x,y,z) by unit quaternion, then translate. Must be the D3D
// row-vector product v*M with M = World.cpp QuatToMatrix (the render
// transform), i.e. identical to World.cpp QuatXform.
//
// This used to apply the TRANSPOSE (M*v): the inverse rotation. Yaws of
// 0/180 deg were unaffected and a thin panel at 90 deg mirrors onto itself,
// so it only showed on diagonal placements -- a billboard at 45 deg got
// collision at -45 deg, i.e. perpendicular to the board. Every rotated
// instance's ground/wall triangles were mirrored the same way.
void TransformPoint(float qx, float qy, float qz, float qw,
                    float tx, float ty, float tz,
                    float& x, float& y, float& z) {
    float xx = qx*qx, yy = qy*qy, zz = qz*qz;
    float xy = qx*qy, xz = qx*qz, yz = qy*qz;
    float wx = qw*qx, wy = qw*qy, wz = qw*qz;
    float ox = (1-2*(yy+zz))*x + 2*(xy-wz)*y     + 2*(xz+wy)*z;
    float oy = 2*(xy+wz)*x     + (1-2*(xx+zz))*y + 2*(yz-wx)*z;
    float oz = 2*(xz-wy)*x     + 2*(yz+wx)*y     + (1-2*(xx+yy))*z;
    x = ox + tx; y = oy + ty; z = oz + tz;
}

} // namespace

bool ColLoader::LoadModelTriangles(const uint8_t* colData, size_t colSize,
                                   const char* modelName,
                                   float qx, float qy, float qz, float qw,
                                   float tx, float ty, float tz,
                                   std::vector<ColTriangle>& outTris) {
    if (!colData || colSize < 0x70 || !modelName) return false;

    // Walk entries: "COL2"/"COL3"/"COL4", u32 size @+4 (excludes the 8-byte
    // header). COL2 is COL3 without the shadow-mesh fields; both locate their
    // sections through the offset table at +0x54, so one reader serves both.
    // (COL2 was skipped entirely before: 599 of the 3017 LA + generic entries,
    // e.g. every road barrier and the multiobj/procobj rooftop props.)
    auto isColV2plus = [&](size_t p) {
        return memcmp(colData + p, "COL", 3) == 0 &&
               (colData[p + 3] == '2' || colData[p + 3] == '3' || colData[p + 3] == '4');
    };
    size_t pos = 0;
    while (pos + 8 <= colSize) {
        if (!isColV2plus(pos)) {
            if (memcmp(colData + pos, "COLL", 4) == 0) {   // v1: skip by size
                pos += 8 + rdU32(colData + pos + 4);
                continue;
            }
            // Not an entry here; scan forward for the next magic.
            size_t next = pos + 1;
            bool found = false;
            while (next + 4 <= colSize) {
                if (isColV2plus(next)) { found = true; break; }
                next++;
            }
            if (!found) return false;
            pos = next;
            continue;
        }
        size_t E = pos;
        uint32_t size = rdU32(colData + E + 4);
        size_t entryEnd = E + 8 + size;
        if (entryEnd > colSize || size < 0x64) return false; // corrupt archive

        if (NameEquals(colData + E + 8, 22, modelName)) {
            uint16_t numTris = rdU16(colData + E + 0x4C);
            uint32_t offVerts = rdU32(colData + E + 0x60);
            uint32_t offTris  = rdU32(colData + E + 0x64);
            // Empirically verified on retail lae2_4.col:
            // filePos(offset) = E + 4 + offset.
            size_t vertPos = E + 4 + offVerts;
            size_t triPos  = E + 4 + offTris;
            if (triPos + (size_t)numTris * 8 > entryEnd) return false;
            if (vertPos + 6 > entryEnd) return false;

            // Read triangle indices first; numVerts = maxIndex+1
            // (the header carries no vertex count).
            uint16_t maxIdx = 0;
            for (uint16_t t = 0; t < numTris; t++) {
                const uint8_t* r = colData + triPos + t * 8;
                uint16_t a = rdU16(r), b = rdU16(r + 2), c = rdU16(r + 4);
                if (a > maxIdx) maxIdx = a;
                if (b > maxIdx) maxIdx = b;
                if (c > maxIdx) maxIdx = c;
            }
            size_t numVerts = (size_t)maxIdx + 1;
            if (vertPos + numVerts * 6 > entryEnd) return false;

            // Decompress vertices (CompressedVector: int16 / 128.0f).
            std::vector<float> vx(numVerts), vy(numVerts), vz(numVerts);
            for (size_t v = 0; v < numVerts; v++) {
                const uint8_t* r = colData + vertPos + v * 6;
                float x = rdS16(r) / 128.0f;
                float y = rdS16(r + 2) / 128.0f;
                float z = rdS16(r + 4) / 128.0f;
                TransformPoint(qx, qy, qz, qw, tx, ty, tz, x, y, z);
                vx[v] = x; vy[v] = y; vz[v] = z;
            }

            outTris.reserve(outTris.size() + numTris);
            for (uint16_t t = 0; t < numTris; t++) {
                const uint8_t* r = colData + triPos + t * 8;
                uint16_t a = rdU16(r), b = rdU16(r + 2), c = rdU16(r + 4);
                ColTriangle tri;
                tri.ax = vx[a]; tri.ay = vy[a]; tri.az = vz[a];
                tri.bx = vx[b]; tri.by = vy[b]; tri.bz = vz[b];
                tri.cx = vx[c]; tri.cy = vy[c]; tri.cz = vz[c];
                outTris.push_back(tri);
            }
            return true;
        }
        pos = entryEnd;
    }
    return false; // model not found in archive
}

float ColLoader::GetGroundHeight(const std::vector<ColTriangle>& tris,
                                 float x, float y, float refZ,
                                 float maxStepUp) {
    float best = kNoGround;
    float zLimit = refZ + maxStepUp;
    for (const auto& t : tris) {
        // 2D barycentric point-in-triangle (XY plane).
        float d = (t.by - t.cy) * (t.ax - t.cx) + (t.cx - t.bx) * (t.ay - t.cy);
        if (fabsf(d) < 1e-12f) continue; // degenerate
        float l1 = ((t.by - t.cy) * (x - t.cx) + (t.cx - t.bx) * (y - t.cy)) / d;
        float l2 = ((t.cy - t.ay) * (x - t.cx) + (t.ax - t.cx) * (y - t.cy)) / d;
        float l3 = 1.0f - l1 - l2;
        const float eps = -1e-5f;
        if (l1 < eps || l2 < eps || l3 < eps) continue;
        float z = l1 * t.az + l2 * t.bz + l3 * t.cz;
        // Highest surface at/below the step-up limit (ignores the elevated
        // highway when the player walks underneath it).
        if (z <= zLimit && z > best) best = z;
    }
    return best;
}
