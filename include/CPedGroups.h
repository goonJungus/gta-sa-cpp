// CPedGroups / CPedGroupMembership - minimal clean-room header
// Adapted from gta-reversed/source/game_sa/PedGroups.h and PedGroupMembership.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
//
// MINIMAL PORT: only what CPed::ProcessControl needs -
//   CPedGroups::ms_groups[i].m_groupMembership.IsMember(ped).
// TODO: port the full group system (CPedGroup, followers, leaders, tasks)
// when the AI/group batch lands.

#pragma once

#include <cstdint>

class CPed;

class CPedGroupMembership {
public:
    bool IsMember(const CPed* ped) const;

    // TODO(port): stub added 2026-10-09 for CPed
    static class CPed* GetLeader(CPedGroupMembership* membership) { (void)membership; return nullptr; }

    // TODO(port): stub added 2026-10-09 for CPed
    static bool IsFollower(CPedGroupMembership* membership, const class CPed* ped) { (void)membership; (void)ped; return false; }
};

class CPedGroup {
public:
    CPedGroupMembership m_groupMembership;
    // TODO: full CPedGroup members (leader, followers, tasks, ...).
};

class CPedGroups {
public:
    // Was StaticRef<std::array<CPedGroup, 8>>(0xC09920) in the original.
    // inline: owned here so no CPedGroups.cpp is needed yet.
    static inline CPedGroup ms_groups[8]{};

    // TODO(port): stubs added 2026-10-09 for CPed
    static bool IsInPlayersGroup(class CPed* ped) { (void)ped; return false; }
    static int32_t GetPedsGroup(class CPed* ped) { (void)ped; return -1; }
};
