// Binary IPL parser implementation. See BinaryIpl.h for format details.

#include "BinaryIpl.h"
#include <cstring>

bool BinaryIplLoader::LoadFromMemory(const uint8_t* data, size_t size,
                                     std::vector<BinIplInstance>& out) {
    if (!data || size < 0x20)
        return false;
    if (std::memcmp(data, "bnry", 4) != 0)
        return false;

    // Retail (CIplStore::LoadIpl @00406080) reads the instance count as a
    // SHORT @0x04 (verified in decompile re-pass, 2026-10-10).
    uint16_t count = 0;
    std::memcpy(&count, data + 4, sizeof(count));
    if (count > 100000)
        return false;

    int32_t arrOff = 0;
    std::memcpy(&arrOff, data + 0x1c, sizeof(arrOff));
    if (arrOff < 0 || (size_t)arrOff + (size_t)count * 40 > size)
        return false;

    for (int i = 0; i < count; i++) {
        const uint8_t* r = data + arrOff + (size_t)i * 40;
        BinIplInstance inst;
        std::memcpy(&inst.x, r + 0, 4);
        std::memcpy(&inst.y, r + 4, 4);
        std::memcpy(&inst.z, r + 8, 4);
        std::memcpy(&inst.qx, r + 12, 4);
        std::memcpy(&inst.qy, r + 16, 4);
        std::memcpy(&inst.qz, r + 20, 4);
        std::memcpy(&inst.qw, r + 24, 4);
        std::memcpy(&inst.modelId, r + 28, 4);
        std::memcpy(&inst.areaAndFlags, r + 32, 4);
        std::memcpy(&inst.lodIndex, r + 36, 4);
        // SA IPL placement quaternions are the CONJUGATE of the rotation the
        // engine applies (RenderWare/SA convention: the stored quat is the
        // inverse). Feeding it straight to a standard quat->matrix negates the
        // yaw of every non-axis-aligned instance (0/180 are fixed points, so
        // only angled objects visibly skew). Conjugate here, once, so every
        // consumer -- World render (QuatToMatrix), World collision (QuatXform),
        // main.cpp props (QuatToD3DMatrix) and their COL -- is correct.
        // Verified: the +40 deg ganghous05_lax aligns to its lot only after this.
        inst.qx = -inst.qx;
        inst.qy = -inst.qy;
        inst.qz = -inst.qz;
        out.push_back(inst);
    }
    return true;
}
