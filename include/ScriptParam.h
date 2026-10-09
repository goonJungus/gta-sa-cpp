// ScriptParam - adapted from gta-reversed for clean-room C++ build
// Original: gta-reversed/source/game_sa/Scripts/ScriptParam.h
// Script VM parameter slot: every script local/global variable is a 4-byte
// slot that can be viewed as int, float, pointer, or string.
// Dependency header for CRunningScript.h / CTheScripts.h.

#pragma once

#include <cstdint>

// *** UNION ***
union tScriptParam {
    uint8_t  u8Param;
    int8_t   i8Param;

    uint16_t u16Param;
    int16_t  i16Param;

    uint32_t uParam{ 0u };
    int32_t  iParam;

    float    fParam;
    void*    pParam;
    char*    szParam;
    bool     bParam;
};

#if INTPTR_MAX == INT32_MAX
static_assert(sizeof(tScriptParam) == 0x4, "tScriptParam layout changed");
#endif
