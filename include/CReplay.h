#pragma once
// CReplay - minimal stub.
// TODO(port): full replay system port (added 2026-10-09 for CPed).
class CPed;

class CReplay {
public:
    // TODO(port): stub
    static void RecordPedDeleted(CPed* ped) { (void)ped; }
    // TODO(port): minimal replay mode (added 2026-10-09 for CPed)
    inline static uint8_t Mode{};
    static constexpr uint8_t MODE_PLAYBACK = 1;
};
