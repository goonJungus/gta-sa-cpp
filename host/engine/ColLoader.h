#pragma once
// COL3 collision loader for gtasa_cpp.exe.
// Parses Rockstar COL3 entries (from .col archives in gta3.img) and builds
// world-space ground triangles for player feet collision.
//
// Format reference (verified empirically against retail lae2_4.col, and
// cross-checked with the project's decompiled CFileLoader::LoadCollisionModelVer3):
//   Entry: "COL3" magic @0, u32 size @4 (excludes 8-byte header).
//   u16 numSpheres @E+0x48, u16 numBoxes @E+0x4A, u16 numTriangles @E+0x4C.
//   u32 offVerts @E+0x60, u32 offTris @E+0x64; filePos(off) = E+4+off.
//   Vertices: 3x int16 LE, decompressed as v/128.0f (CompressedVector).
//   Triangles: u16 a,b,c + u8 material + u8 light (8 bytes).
// Collision is in the same model space as the DFF: apply the identical
// IPL placement matrix (quaternion + translation).

#include <cstdint>
#include <vector>

struct ColTriangle {
    float ax, ay, az;
    float bx, by, bz;
    float cx, cy, cz;
};

class ColLoader {
public:
    // Load the triangle collision for one model from a .col archive already
    // extracted to memory. Transforms vertices by (qx,qy,qz,qw)+(tx,ty,tz).
    // Returns false if the entry is missing or malformed.
    // outTris receives world-space triangles (appended).
    static bool LoadModelTriangles(const uint8_t* colData, size_t colSize,
                                   const char* modelName,
                                   float qx, float qy, float qz, float qw,
                                   float tx, float ty, float tz,
                                   std::vector<ColTriangle>& outTris);

    // Highest triangle surface at (x,y) at or below refZ+maxStepUp.
    // Returns -1e30f when no triangle is under the point (caller falls back
    // to the flat grass plane). refZ = player feet Z; the step-up allowance
    // prevents snapping onto the elevated highway when walking under it.
    static float GetGroundHeight(const std::vector<ColTriangle>& tris,
                                 float x, float y, float refZ,
                                 float maxStepUp = 1.2f);

    static constexpr float kNoGround = -1e30f;
};
