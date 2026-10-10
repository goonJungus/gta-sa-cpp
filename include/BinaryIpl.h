#pragma once
// Binary IPL parser for gtasa_cpp.exe.
// GTA SA streams HD instances + props via binary IPLs stored in gta3.img
// (e.g. lae2_stream0.ipl). The text IPLs only contain LOD placeholders.
// Format (from decompile src/CIplStore/LoadIpl_00406080.c, verified by hexdump):
//   magic "bnry" @ 0
//   int32 instanceCount @ 4
//   int32 instanceArrayFileOffset @ 0x1c
//   Each instance record is 40 bytes:
//     pos (3x f32) @ 0, quat (4x f32) @ 12,
//     modelId (i32) @ 28, interior (i32) @ 32, lodIndex (i32) @ 36
// NOTE: lodIndex is an index into the IPL entity array, NOT a model ID.
// NOTE: some exterior props (Grove Street bushes, telephone poles) use
//   interior 256/512 in Rockstar's data but are positioned in the exterior
//   world. The interior field is logged, not used as a visibility filter.

#include <cstdint>
#include <vector>

struct BinIplInstance {
    float x, y, z;          // world position
    float qx, qy, qz, qw;    // rotation quaternion
    int32_t modelId;        // IDE model ID -> resolve via IDE table
    int32_t interior;       // interior ID (0 = exterior; see note above)
    int32_t lodIndex;       // index into IPL entity array (NOT a model ID)
};

class BinaryIplLoader {
public:
    // Parse a binary IPL from memory. Returns false on bad magic/size.
    // Parsed instances are appended to out.
    static bool LoadFromMemory(const uint8_t* data, size_t size,
                               std::vector<BinIplInstance>& out);
};
