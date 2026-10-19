#pragma once
// Melee combat data: data\melee.dat combos (CTaskSimpleFight) and the
// hard-coded anim group tables of the exe.
#include <string>
#include <vector>

namespace Melee {

constexpr float kFrame = 1.0f / 30.0f;   // melee.dat times are frames at 30 fps

// One move row: ATTACK1..3, AGROUND, AMOVING. Times in seconds.
struct Move {
    float hit = 99.0f * kFrame, chain = 99.0f * kFrame;
    float radius = 0.4f;
    char level[3] = {'H', 0, 0};   // H L G B, optionally + L (HL, LL, GL)
    float damage = 0.0f;
    int hitAudio = 0, altHitAudio = 0;
    float groundLoop = 0.0f;
};

enum MoveIdx { kAttack1, kAttack2, kAttack3, kGround, kMoving, kMoveCount };

struct Combo {
    std::string name, group;
    float range = 1.6f;
    Move moves[kMoveCount];
    float blockHit = 0.0f, blockChain = 0.0f;
    unsigned flags = 0;   // low nibble: 1 attack2, 2 attack3, 4 ground, 8 running
    bool Has(unsigned f) const { return (flags & f) != 0; }
};

struct Table {
    float levelOffset[7][3];   // H L G B HL LL GL
    std::vector<Combo> combos;
    const Combo* Find(const std::string& name) const;
};

// Parses melee.dat; false when the file is missing.
bool Load(const std::string& gameDir, Table& out);

// Strike sphere centre: ped position + heading-rotated level offset.
void StrikeCentre(const Table& t, const char* level, const float pos[3], float heading, float out[3]);

// Anim group member names, retail order: attack 1-3, ground, moving, hit
// reactions 1-3, block, idle. Empty for an unknown group.
const std::vector<std::string>& GroupClips(const std::string& group);
// The IFP block that holds the group's clips.
std::string GroupBlock(const std::string& group);

}  // namespace Melee
