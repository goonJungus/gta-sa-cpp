#pragma once
// CPedStats - minimal stub (ped stats manager).
// TODO(port): full port; verify count (added 2026-10-09 for CPed).
#include <cstdint>

// Minimal CPedStat (added 2026-10-09 for CPed). TODO(port): full port.
struct CPedStat {
    float m_fHeadingChangeRate{};
    int32_t m_nDefaultDecisionMaker{};
};

class CPedStats {
public:
    // TODO(port): real count is NUM_PEDSTATS; 44 covers the ePedStats values
    inline static CPedStat* ms_apPedStats[44]{};
};
