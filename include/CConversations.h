#pragma once
// CConversations / CPedToPlayerConversations - minimal stubs.
// TODO(port): full conversation system port (added 2026-10-09 for CPed).
class CPed;

class CConversations {
public:
    // TODO(port): stub
    static void RemoveConversationForPed(CPed* ped) { (void)ped; }
};

class CPedToPlayerConversations {
public:
    // TODO(port): stub
    inline static CPed* m_pPed{};
};
