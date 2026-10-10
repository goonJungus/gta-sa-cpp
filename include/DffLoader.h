#pragma once
// RenderWare DFF (model) loader for gtasa_cpp.exe
// Parses SA-era RW 3.x binary DFFs: Clump -> FrameList/GeometryList/Atomic.
// Extracts geometry (vertices, triangles, normals, UVs) for D3D9 rendering.
// Milestone 3: parses the material list; geometries with multiple materials
// are split into one DffMesh per material so each mesh has a single texture.

#include <cstdint>
#include <string>
#include <vector>

struct DffVertex {
    float x, y, z;      // position
    float nx, ny, nz;   // normal
    float u, v;         // texture coords
};

struct DffMesh {
    std::vector<DffVertex> vertices;
    std::vector<uint16_t>  indices;
    bool hasNormals = false;
    bool hasUVs = false;
    std::string textureName;   // from RW_TEXTURE (empty if material is untextured)
    std::string materialName;  // DFF materials carry no name string; kept for API symmetry
};

struct DffModel {
    std::vector<DffMesh> meshes;
    bool valid = false;
};

class DffLoader {
public:
    // Load a DFF file from disk. Returns model with valid=false on failure.
    static DffModel Load(const std::string& path);

private:
    struct SectionHeader {
        uint32_t type;
        uint32_t size;
        uint32_t version;
    };

    struct Reader {
        const uint8_t* data;
        size_t size;
        size_t pos;

        bool Read(void* out, size_t n);
        bool Skip(size_t n);
        template<typename T> bool ReadT(T& out) { return Read(&out, sizeof(T)); }
        bool ReadHeader(SectionHeader& h) { return ReadT(h); }
        size_t Remaining() const { return pos < size ? size - pos : 0; }
    };

    struct DffMaterial {
        std::string textureName;
        std::string materialName;
    };

    // Parses one RW_GEOMETRY section; appends one DffMesh per material to outMeshes.
    static bool ParseGeometry(Reader& r, uint32_t geomSize, std::vector<DffMesh>& outMeshes);
    static bool ParseMaterialList(Reader& r, size_t listEnd, std::vector<DffMaterial>& outMats);
};
