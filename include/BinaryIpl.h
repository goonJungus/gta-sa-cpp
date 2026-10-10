#pragma once
// Binary IPL parser for gtasa_cpp.exe.
// GTA SA streams HD instances + props via binary IPLs stored in gta3.img
// (e.g. lae2_stream0.ipl). The text IPLs only contain LOD placeholders.
// Format (cross-checked against gtamaptk's MIT-licensed iplcomp +
// gta-reversed's CFileObjectInstance, and empirical hexdump):
//   magic "bnry" @ 0 (0x79726E62)
//   int32 instanceCount @ 4
//   int32 carGenCount @ 0x14
//   int32 instanceArrayFileOffset @ 0x1c
//   int32 carGenArrayFileOffset @ 0x3c (48-byte car-generator records; skipped)
//   Each instance record is 40 bytes (0x28):
//     pos (3x f32) @ 0, quat (4x f32) @ 12,
//     modelId (i32) @ 28, areaAndFlags (u32 bitfield) @ 32, lodIndex (i32) @ 36
// NOTE: lodIndex is an index into the IPL entity array, NOT a model ID.
// NOTE: the 4 bytes @32 are a bitfield union: low byte = area ID,
//   upper bits = stream flags (0x100 redundant-stream, 0x200 dont-stream,
//   0x400 underwater, 0x800/0x1000 tunnel). Use AreaCode() (= value & 0xFF)
//   for the real area/interior. Some exterior props use e.g. 256/512 but live
//   in the exterior world; the field is logged, not used as a visibility filter.

#include <cstdint>
#include <vector>

struct BinIplInstance {
    float x, y, z;          // world position
    float qx, qy, qz, qw;    // rotation quaternion
    int32_t modelId;        // IDE model ID -> resolve via IDE table
    int32_t areaAndFlags;   // bitfield: low byte = area/interior ID, upper bits = flags
    int32_t lodIndex;       // index into IPL entity array (NOT a model ID)

    int32_t AreaCode() const { return areaAndFlags & 0xFF; }
};

class BinaryIplLoader {
public:
    // Parse a binary IPL from memory. Returns false on bad magic/size.
    // Parsed instances are appended to out.
    static bool LoadFromMemory(const uint8_t* data, size_t size,
                               std::vector<BinIplInstance>& out);
};
