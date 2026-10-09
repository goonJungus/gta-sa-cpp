#pragma once
// CPathFind / CPathNode / ThePaths - minimal stubs.
// TODO(port): full pathfind port (added 2026-10-09 for CPed).
#include <cstdint>

class CVector;

struct CPathNode {
    // TODO(port): minimal
    CVector* m_pCoords{};
    // TODO(port): stub - decomp static-style
    static CVector* GetPosition(CPathNode* node, CVector* out) { (void)node; (void)out; return nullptr; }
};

// (m_pPathNodes is a CPathFind member, see below)

class CPathFind {
public:
    // TODO(port): minimal - decomp path node array
    CPathNode** m_pPathNodes{}; // TODO(port): decomp uses array of pointers

    // TODO(port): stubs - signature matches decomp static-style call convention
    static void FindNodeClosestToCoors(int32_t* outNodeId, float x, float y, float z, int32_t nodeType, uint32_t arg5, int32_t arg6, int32_t arg7, int32_t arg8, int32_t arg9, int32_t arg10) {
        (void)outNodeId; (void)x; (void)y; (void)z; (void)nodeType; (void)arg5;
        (void)arg6; (void)arg7; (void)arg8; (void)arg9; (void)arg10;
    }
};

// TODO(port): global pathfind instance
inline CPathFind ThePaths{};
