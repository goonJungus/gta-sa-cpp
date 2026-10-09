#pragma once
// CPedSaveStructure - minimal stub for save/load.
// TODO(port): implement when save system lands.
#include <cstdint>

struct CPedSaveStructure {
    // TODO(port): stub, real layout from decomp
    uint8_t data[1024];
    void Construct(void* arg); // TODO(port): stub
    void Extract(void* arg); // TODO(port): stub
};
