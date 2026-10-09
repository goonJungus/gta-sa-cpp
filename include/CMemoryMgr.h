// CMemoryMgr - minimal stand-in for the game's memory manager
// (gta-reversed/source/game_sa/Core/MemoryMgr.h).
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Only the allocation entry points used by converted subsystems are defined.
// The original ran a custom heap with per-pool arenas; this maps to the CRT
// heap until the memory subsystem is converted - see TODO below.

#pragma once

#include <cstddef>
#include <cstdlib>
#include <cstdint>

class CMemoryMgr {
public:
    // Original signature: Malloc(size_t size, uint32_t hint).
    static void* Malloc(size_t size, uint32_t hint = 0) {
        (void)hint;
        // TODO: the original used the game's custom heap pools; verify
        // allocation behavior (alignment, OOM handling) from decomp when the
        // memory subsystem is converted.
        return std::malloc(size);
    }

    static void Free(void* ptr) {
        std::free(ptr);
    }
};
