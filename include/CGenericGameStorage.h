#pragma once
// CGenericGameStorage - minimal stub for save/load.
// TODO(port): implement when save system lands.
#include <cstdint>

class CGenericGameStorage {
public:
    // TODO(port): stubs
    static bool SaveDataToWorkBuffer(void* data, uint32_t size);
    static bool LoadDataFromWorkBuffer(void* data, uint32_t size);
};
