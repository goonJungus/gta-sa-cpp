// Binary IPL parser implementation. See BinaryIpl.h for format details.

#include "BinaryIpl.h"
#include <cstring>

bool BinaryIplLoader::LoadFromMemory(const uint8_t* data, size_t size,
                                     std::vector<BinIplInstance>& out) {
    if (!data || size < 0x20)
        return false;
    if (std::memcmp(data, "bnry", 4) != 0)
        return false;

    int32_t count = 0;
    std::memcpy(&count, data + 4, sizeof(count));
    if (count < 0 || count > 100000)
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
        out.push_back(inst);
    }
    return true;
}
