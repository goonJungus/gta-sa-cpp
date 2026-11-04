#pragma once
// What a ped thinks of others: CAcquaintance (who it respects, likes, ignores, dislikes and hates, as bit
// masks of ped types), CMentalState (the anger a ped builds up when it is bumped and crowded) and
// CPlayerRelationshipRecorder (the sixteen peds the player is in a fight or a chase with).

#include <cstdint>

// ePedType as a bit: CPedType::GetPedFlag
inline uint32_t PedTypeFlag(int pedType) { return pedType >= 0 && pedType < 32 ? 1u << pedType : 0u; }

// CAcquaintance: one mask per relationship, a type in at most one of them.
struct Acquaintance {
    enum Id { Respect = 0, Like, Ignore, Dislike, Hate, kCount };
    uint32_t mask[kCount] = {0, 0, 0, 0, 0};

    uint32_t Get(int id) const { return id >= 0 && id < kCount ? mask[id] : 0; }          // GetAcquaintances
    void Set(int id, uint32_t value) { if (id >= 0 && id < kCount) mask[id] = value; }    // SetAcquaintances
    void Clear(int id) { if (id >= 0 && id < kCount) mask[id] = 0; }                      // ClearAcquaintances
    void ClearAs(int id, uint32_t bits) { if (id >= 0 && id < kCount) mask[id] &= ~bits; }   // ClearAsAcquaintance
    void SetAs(int id, uint32_t bits) {                                                   // SetAsAcquaintance
        if (id < 0 || id >= kCount || (bits & mask[id]) != 0) return;
        mask[id] |= bits;
        for (int k = 0; k < kCount; k++) if (k != id) mask[k] &= ~bits;                   // moves it out of the others
    }
    bool Hates(int pedType) const { return (mask[Hate] & PedTypeFlag(pedType)) != 0; }
    bool Dislikes(int pedType) const { return (mask[Dislike] & PedTypeFlag(pedType)) != 0; }
    bool Likes(int pedType) const { return (mask[Like] & PedTypeFlag(pedType)) != 0; }
};

// CMentalState::IncrementAnger: a bump adds anger at most once in three seconds.
struct MentalState {
    uint8_t anger = 0;
    uint32_t timeStarted = 0, duration = 0;
    bool enabled = true, first = true;
    void IncrementAnger(uint32_t nowMs, int amount) {
        if (!enabled) return;
        if (first) { timeStarted = nowMs; first = false; }
        if (duration + timeStarted <= nowMs) {
            timeStarted = nowMs;
            duration = 3000;
            enabled = true;
            anger = (uint8_t)(anger + amount);
        }
    }
};

// CPlayerRelationshipRecorder: sixteen slots of (ped, how it relates). Only the first slot is ever written
// by Record and Add, as in the retail code.
class PlayerRelationshipRecorder {
public:
    struct Entry { int ped = 0; uint8_t relationship = 0; };
    void Flush() { for (Entry& e : m_e) e = Entry(); }
    void Clear(int ped) { for (Entry& e : m_e) if (e.ped == ped) { e = Entry(); return; } }
    uint8_t Get(int ped) const { for (const Entry& e : m_e) if (e.ped == ped) return e.relationship; return 0; }
    void Add(int ped, uint8_t relationship) { if (m_e[0].ped != 0) { m_e[0].ped = ped; m_e[0].relationship = relationship; } }
    // RecordRelationshipWithPlayer: the ped's old entry goes; a ped chasing or following the player is 3, one
    // out to kill him 7. Like AddRelationship it writes only when the first slot is already taken, which a
    // fresh recorder never has, so a recorder nobody has seeded stays empty (the retail behaviour).
    void Record(int ped, bool chasing, bool killing) {
        Clear(ped);
        if (m_e[0].ped == 0) return;
        if (chasing) { m_e[0].relationship = 3; m_e[0].ped = ped; }
        else if (killing) { m_e[0].relationship = 7; m_e[0].ped = ped; }
    }
    const Entry& At(int i) const { return m_e[i & 15]; }
private:
    Entry m_e[16];
};
