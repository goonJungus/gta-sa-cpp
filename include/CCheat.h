#pragma once
// CCheat - minimal stub.
// TODO(port): real cheat system (gta-reversed CCheat.h); verify TOTAL_CHEATS count (added 2026-10-09 for CPed).
class CCheat {
public:
    // TODO(port): real count is TOTAL_CHEATS; 92 covers every index seen in the decomp so far
    inline static bool m_aCheatsActive[92]{};
};
