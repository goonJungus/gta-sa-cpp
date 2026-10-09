// CWaterLevel - minimal stub for the clean-room C++ build
// Source: gta-reversed/source/game_sa/WaterLevel.h
//
// Only the water-level query used by converted TUs.
// Full port (water zones, rendering) belongs to a later pass.

#pragma once

#include <cstdint>

class CWaterLevel {
public:
    static bool GetWaterLevel(float x, float y, float z, float* outWaterHeight,
                              bool checkWaves, void* pVecNormal) {
        (void)x; (void)y; (void)z; (void)checkWaves; (void)pVecNormal;
        *outWaterHeight = 0.0f;
        return false; // TODO(water): real water level query
    }
};
