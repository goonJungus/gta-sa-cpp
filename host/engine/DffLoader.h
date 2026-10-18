#pragma once
// RenderWare DFF (model) loader for gtasa_cpp.exe
// Parses SA-era RW 3.x binary DFFs: Clump -> FrameList/GeometryList/Atomic.
// Extracts geometry (vertices, triangles, normals, UVs) for D3D9 rendering.
// Milestone 3: parses the material list; geometries with multiple materials
// are split into one DffMesh per material so each mesh has a single texture.
// Milestone 6: parses the baked day prelit vertex colours (geometry flag 0x08)
// and the SA night/extra prelit set (geometry extension plugin 0x253F2F9).

#include <cstdint>
#include <string>
#include <vector>

// Layout must stay byte-identical to D3DRenderer::MeshVertex (callers
// reinterpret_cast one to the other).
struct DffVertex {
    float x, y, z;      // position
    float nx, ny, nz;   // normal
    uint32_t color;     // prelit day colour, D3DCOLOR (A8R8G8B8); 0xFFFFFFFF if none
    float u, v;         // texture coords
};

struct DffMesh {
    std::vector<DffVertex> vertices;
    std::vector<uint16_t>  indices;
    bool hasNormals = false;
    bool hasUVs = false;
    bool hasPrelit = false;          // geometry carried baked day prelit colours
    // SA night prelit set (RW plugin 0x253F2F9 inside the geometry extension),
    // one D3DCOLOR per vertex, parallel to `vertices`. Empty when absent.
    std::vector<uint32_t> nightColors;
    int geometry = 0;          // index of the RW geometry this mesh came from (one geometry may give several meshes, one per material)
    std::string textureName;   // from RW_TEXTURE (empty if material is untextured)
    std::string materialName;  // DFF materials carry no name string; kept for API symmetry
};

// One 2dfx EFFECT_LIGHT marker (RW plugin 0x253F2F8) in model-local space:
// used for traffic-light bulbs, building/vehicle lights, etc. Position is the
// bulb centre; rgba is the authored light colour (traffic lights encode which
// signal: green ~ low red, amber ~ high red+green, red ~ high red low green).
struct Dff2dfxLight {
    float x, y, z;
    uint8_t r, g, b, a;
};

// One 2dfx EFFECT_ROADSIGN (type 7): text drawn at runtime on a sign panel
// (CCustomRoadsignMgr). The position is ABSOLUTE world space (retail never
// parents the sign frame to the entity); the text quad is built in the local
// XY plane and rotated z, then x, then y (CEntity::CreateEffects).
struct Dff2dfxRoadsign {
    float x, y, z;
    float width, height;          // whole sign, metres
    float rotX, rotY, rotZ;       // degrees
    uint16_t flags;               // lines (bits 0-1), letters (2-3), palette (4-5)
    char text[4][16];             // not NUL-terminated; '_' = space
};

struct DffModel {
    std::vector<DffMesh> meshes;
    bool valid = false;
    bool hasPrelit = false;      // at least one mesh had prelit day colours
    bool hasNightColors = false; // at least one mesh had the 0x253F2F9 set
    std::vector<Dff2dfxLight> lights;  // 2dfx EFFECT_LIGHT markers (model space)
    std::vector<Dff2dfxRoadsign> roadsigns;  // 2dfx EFFECT_ROADSIGN (model space)
};

class DffLoader {
public:
    // Load a DFF file from disk. Returns model with valid=false on failure.
    static DffModel Load(const std::string& path);

    static DffModel LoadFromMemory(const uint8_t* data, size_t size);

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

    // Parses one RW_GEOMETRY section; appends one DffMesh per material to outMeshes
    // and any 2dfx EFFECT_LIGHT markers to outLights.
    static bool ParseGeometry(Reader& r, uint32_t geomSize, std::vector<DffMesh>& outMeshes,
                              std::vector<Dff2dfxLight>& outLights,
                              std::vector<Dff2dfxRoadsign>& outSigns);
    static bool ParseMaterialList(Reader& r, size_t listEnd, std::vector<DffMaterial>& outMats);
    // Scans a geometry RW_EXTENSION body for plugin 0x253F2F9 (night prelit
    // vertex colours) and plugin 0x253F2F8 (2dfx effects). Plugin entries are
    // chunk-like: 12-byte {pluginId, size, version} headers with no preceding count.
    static void ParseGeometryExtension(Reader& r, size_t extEnd, uint32_t numVerts,
                                       std::vector<uint32_t>& outNight,
                                       std::vector<Dff2dfxLight>& outLights,
                                       std::vector<Dff2dfxRoadsign>& outSigns);
    // RW stores prelit colours as R,G,B,A bytes; D3DCOLOR wants 0xAARRGGBB.
    static uint32_t SwapRgbaToD3d(uint32_t v) {
        return (v & 0xFF00FF00u) | ((v & 0x00FF0000u) >> 16) | ((v & 0x000000FFu) << 16);
    }
};
