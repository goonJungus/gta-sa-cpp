#include "DffLoader.h"
#include <fstream>
#include <cstring>

// RenderWare section types
enum RwSectionType : uint32_t {
    RW_STRUCT       = 0x01,
    RW_STRING       = 0x02,
    RW_EXTENSION    = 0x03,
    RW_TEXTURE      = 0x06,
    RW_MATERIAL     = 0x07,
    RW_MATERIALLIST = 0x08,
    RW_FRAMELIST    = 0x0E,
    RW_GEOMETRY     = 0x0F,
    RW_CLUMP        = 0x10,
    RW_ATOMIC       = 0x14,
    RW_GEOMETRYLIST = 0x1A,
};

// Geometry flags
enum RwGeometryFlag : uint16_t {
    GEO_TRILIST    = 0x01,
    GEO_POSITIONS  = 0x02,
    GEO_TEXTURED   = 0x04,
    GEO_PRELIT     = 0x08,
    GEO_NORMALS    = 0x10,
    GEO_LIGHT      = 0x20,
    GEO_MODULATE   = 0x40,
    GEO_TEXTURED2  = 0x80,
};

bool DffLoader::Reader::Read(void* out, size_t n) {
    if (pos + n > size)
        return false;
    memcpy(out, data + pos, n);
    pos += n;
    return true;
}

bool DffLoader::Reader::Skip(size_t n) {
    if (pos + n > size)
        return false;
    pos += n;
    return true;
}

DffModel DffLoader::Load(const std::string& path) {
    DffModel model;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
        return model;
    size_t fileSize = (size_t)f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf(fileSize);
    if (!f.read((char*)buf.data(), fileSize))
        return model;

    Reader r{ buf.data(), buf.size(), 0 };
    SectionHeader h;
    if (!r.ReadHeader(h) || h.type != RW_CLUMP)
        return model;  // not a DFF clump

    size_t clumpEnd = r.pos + h.size;

    // Clump struct: numAtomics [, numLights, numCameras]
    if (!r.ReadHeader(h) || h.type != RW_STRUCT)
        return model;
    size_t structEnd = r.pos + h.size;
    int32_t numAtomics = 0;
    r.ReadT(numAtomics);
    r.pos = structEnd;

    // Walk top-level sections
    while (r.pos < clumpEnd && r.Remaining() >= 12) {
        if (!r.ReadHeader(h))
            break;
        size_t sectionEnd = r.pos + h.size;
        if (sectionEnd > r.size)
            break;

        if (h.type == RW_GEOMETRYLIST) {
            // GeometryList struct: numGeometries
            SectionHeader sh;
            if (r.ReadHeader(sh) && sh.type == RW_STRUCT) {
                int32_t numGeos = 0;
                r.ReadT(numGeos);
                // Geometries follow
                for (int32_t i = 0; i < numGeos && r.pos < sectionEnd; i++) {
                    SectionHeader gh;
                    if (!r.ReadHeader(gh))
                        break;
                    size_t geoEnd = r.pos + gh.size;
                    if (gh.type == RW_GEOMETRY) {
                        DffMesh mesh;
                        if (ParseGeometry(r, gh.size, mesh))
                            model.meshes.push_back(std::move(mesh));
                    }
                    r.pos = geoEnd;
                    if (r.pos > r.size)
                        break;
                }
            }
        }
        r.pos = sectionEnd;
    }

    model.valid = !model.meshes.empty();
    return model;
}

bool DffLoader::ParseGeometry(Reader& r, uint32_t geomSize, DffMesh& mesh) {
    size_t geoEnd = r.pos + geomSize;

    SectionHeader h;
    if (!r.ReadHeader(h) || h.type != RW_STRUCT)
        return false;
    size_t structEnd = r.pos + h.size;

    uint16_t flags = 0, reserved = 0;
    uint32_t numTris = 0, numVerts = 0, numMorphs = 0;
    if (!r.ReadT(flags) || !r.ReadT(reserved) ||
        !r.ReadT(numTris) || !r.ReadT(numVerts) || !r.ReadT(numMorphs))
        return false;

    if (numVerts == 0 || numVerts > 1000000 || numTris > 1000000)
        return false;  // sanity

    bool prelit = (flags & GEO_PRELIT) != 0;
    bool textured = (flags & GEO_TEXTURED) != 0;
    bool flagNormals = (flags & GEO_NORMALS) != 0;

    // RW < 3.4 has ambient/diffuse/specular floats; SA is 3.4+, skip check via version
    uint32_t ver = (h.version >> 16) & 0xFFFF;
    // version field layout: libID(16) | major(8)... actually use raw compare
    if (h.version < 0x34000) {
        float dummy[3];
        r.Read(dummy, sizeof(dummy));
    }

    // Vertex colors (prelit)
    if (prelit) {
        if (!r.Skip((size_t)numVerts * 4))
            return false;
    }

    // UV sets
    std::vector<float> uvs;
    if (textured) {
        uvs.resize((size_t)numVerts * 2);
        if (!r.Read(uvs.data(), uvs.size() * sizeof(float)))
            return false;
        mesh.hasUVs = true;
    }

    // Triangles: (v2, v1, materialId, v3) uint16 each
    struct RwTriangle { uint16_t v2, v1, matId, v3; };
    std::vector<RwTriangle> tris(numTris);
    if (!r.Read(tris.data(), (size_t)numTris * sizeof(RwTriangle)))
        return false;

    // Morph targets: bounding sphere + flags + verts + normals
    float bsphere[4];
    uint32_t hasVerts = 0, morphHasNormals = 0;
    std::vector<float> positions;
    std::vector<float> normals;
    for (uint32_t m = 0; m < numMorphs; m++) {
        if (!r.Read(bsphere, sizeof(bsphere)))
            return false;
        if (!r.ReadT(hasVerts) || !r.ReadT(morphHasNormals))
            return false;
        if (hasVerts && m == 0) {
            positions.resize((size_t)numVerts * 3);
            if (!r.Read(positions.data(), positions.size() * sizeof(float)))
                return false;
        } else if (hasVerts) {
            if (!r.Skip((size_t)numVerts * 3 * sizeof(float)))
                return false;
        }
        if (morphHasNormals && hasVerts && m == 0) {
            normals.resize((size_t)numVerts * 3);
            if (!r.Read(normals.data(), normals.size() * sizeof(float)))
                return false;
            mesh.hasNormals = true;
        } else if (morphHasNormals) {
            if (!r.Skip((size_t)numVerts * 3 * sizeof(float)))
                return false;
        }
    }

    if (positions.empty())
        return false;

    // Build mesh
    mesh.vertices.resize(numVerts);
    for (uint32_t i = 0; i < numVerts; i++) {
        DffVertex& v = mesh.vertices[i];
        v.x = positions[i * 3 + 0];
        v.y = positions[i * 3 + 1];
        v.z = positions[i * 3 + 2];
        if (!normals.empty()) {
            v.nx = normals[i * 3 + 0];
            v.ny = normals[i * 3 + 1];
            v.nz = normals[i * 3 + 2];
        } else {
            v.nx = 0; v.ny = 1; v.nz = 0;
        }
        if (!uvs.empty()) {
            v.u = uvs[i * 2 + 0];
            v.v = uvs[i * 2 + 1];
        } else {
            v.u = 0; v.v = 0;
        }
    }
    mesh.indices.reserve((size_t)numTris * 3);
    for (uint32_t i = 0; i < numTris; i++) {
        // RW stores (v2, v1, v3); convert to CCW (v1, v2, v3) for D3D
        mesh.indices.push_back(tris[i].v1);
        mesh.indices.push_back(tris[i].v2);
        mesh.indices.push_back(tris[i].v3);
    }

    r.pos = geoEnd;
    (void)structEnd;
    return true;
}
