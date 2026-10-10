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

    return LoadFromMemory(buf.data(), buf.size());
}

DffModel DffLoader::LoadFromMemory(const uint8_t* data, size_t size) {
    DffModel model;
    Reader r{ data, size, 0 };
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
            // GeometryList struct: numGeometries [+ material index array]
            SectionHeader sh;
            if (r.ReadHeader(sh) && sh.type == RW_STRUCT) {
                size_t listStructEnd = r.pos + sh.size;
                int32_t numGeos = 0;
                r.ReadT(numGeos);
                r.pos = listStructEnd;  // skip any trailing index array
                // Geometries follow
                for (int32_t i = 0; i < numGeos && r.pos < sectionEnd; i++) {
                    SectionHeader gh;
                    if (!r.ReadHeader(gh))
                        break;
                    size_t geoEnd = r.pos + gh.size;
                    if (gh.type == RW_GEOMETRY) {
                        if (!ParseGeometry(r, gh.size, model.meshes))
                            r.pos = geoEnd;  // keep going on a bad geometry
                    } else {
                        r.pos = geoEnd;
                    }
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

bool DffLoader::ParseMaterialList(Reader& r, size_t listEnd,
                                  std::vector<DffMaterial>& outMats) {
    SectionHeader h;
    if (!r.ReadHeader(h) || h.type != RW_STRUCT)
        return false;
    size_t structEnd = r.pos + h.size;
    int32_t numMaterials = 0;
    if (!r.ReadT(numMaterials))
        return false;
    r.pos = structEnd;  // skip material index array / padding

    for (int32_t i = 0; i < numMaterials && r.pos < listEnd; i++) {
        if (!r.ReadHeader(h))
            break;
        size_t matEnd = r.pos + h.size;
        if (matEnd > r.size || h.type != RW_MATERIAL) {
            r.pos = matEnd;
            continue;
        }
        DffMaterial mat;
        // Material struct (flags, color, surface props) - skip by size
        if (r.ReadHeader(h) && h.type == RW_STRUCT) {
            r.pos += h.size;
        }
        // Inner sections: RW_TEXTURE*, RW_EXTENSION
        while (r.pos + 12 <= matEnd && r.pos < r.size) {
            size_t innerPos = r.pos;
            if (!r.ReadHeader(h))
                break;
            size_t innerEnd = r.pos + h.size;
            if (innerEnd > matEnd)
                break;
            if (h.type == RW_TEXTURE) {
                // struct (filter flags) + RW_STRING name + RW_STRING mask
                if (r.ReadHeader(h) && h.type == RW_STRUCT)
                    r.pos += h.size;  // skip filter flags
                int strIdx = 0;
                while (r.pos + 12 <= innerEnd && r.pos < r.size) {
                    if (!r.ReadHeader(h))
                        break;
                    size_t strEnd = r.pos + h.size;
                    if (strEnd > innerEnd)
                        break;
                    if (h.type == RW_STRING && strIdx == 0 && mat.textureName.empty()) {
                        const char* s = (const char*)(r.data + r.pos);
                        size_t n = h.size;
                        while (n > 0 && s[n - 1] == '\0')
                            n--;
                        mat.textureName.assign(s, n);
                    }
                    strIdx++;
                    r.pos = strEnd;
                }
            } else if (h.type == RW_STRING && mat.materialName.empty()) {
                // A bare RW_STRING directly under the material (rare; some
                // exporters stash a material name here).
                const char* s = (const char*)(r.data + r.pos);
                size_t n = h.size;
                while (n > 0 && s[n - 1] == '\0')
                    n--;
                mat.materialName.assign(s, n);
                r.pos = innerEnd;
            } else {
                r.pos = innerEnd;  // extension / unknown: skip
            }
            (void)innerPos;
        }
        outMats.push_back(std::move(mat));
        r.pos = matEnd;
    }
    r.pos = listEnd;
    return true;
}

bool DffLoader::ParseGeometry(Reader& r, uint32_t geomSize,
                              std::vector<DffMesh>& outMeshes) {
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
    (void)flagNormals;

    // RW < 3.4 has ambient/diffuse/specular floats; SA is 3.4+, skip check via version
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
        } else if (morphHasNormals) {
            if (!r.Skip((size_t)numVerts * 3 * sizeof(float)))
                return false;
        }
    }

    if (positions.empty())
        return false;

    // Material list + extension follow the morph data (M3)
    std::vector<DffMaterial> materials;
    while (r.pos + 12 <= geoEnd && r.pos < r.size) {
        if (!r.ReadHeader(h))
            break;
        size_t secEnd = r.pos + h.size;
        if (secEnd > geoEnd || secEnd > r.size)
            break;
        if (h.type == RW_MATERIALLIST) {
            ParseMaterialList(r, secEnd, materials);
        } else {
            r.pos = secEnd;  // RW_EXTENSION / unknown: skip
        }
    }
    r.pos = geoEnd;
    (void)structEnd;

    // Build shared vertex array
    std::vector<DffVertex> verts(numVerts);
    for (uint32_t i = 0; i < numVerts; i++) {
        DffVertex& v = verts[i];
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

    // Group triangle indices per material; one DffMesh per material so each
    // mesh carries a single textureName.
    size_t numBuckets = materials.empty() ? 1 : materials.size();
    std::vector<std::vector<uint16_t>> bucketTris(numBuckets);
    for (uint32_t i = 0; i < numTris; i++) {
        size_t b = 0;
        if (!materials.empty()) {
            b = tris[i].matId < materials.size() ? tris[i].matId : 0;
        }
        // RW stores (v2, v1, v3); convert to CCW (v1, v2, v3) for D3D
        bucketTris[b].push_back(tris[i].v1);
        bucketTris[b].push_back(tris[i].v2);
        bucketTris[b].push_back(tris[i].v3);
    }

    for (size_t b = 0; b < numBuckets; b++) {
        if (bucketTris[b].empty())
            continue;  // material with no triangles: no mesh
        DffMesh mesh;
        mesh.vertices = verts;  // shared copy; simple and correct
        mesh.indices = std::move(bucketTris[b]);
        mesh.hasNormals = !normals.empty();
        mesh.hasUVs = !uvs.empty();
        if (b < materials.size()) {
            mesh.textureName = materials[b].textureName;
            mesh.materialName = materials[b].materialName;
        }
        outMeshes.push_back(std::move(mesh));
    }
    return !outMeshes.empty();
}
