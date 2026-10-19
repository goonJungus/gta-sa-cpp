#include "Melee.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace Melee {

namespace {

std::vector<std::string> Words(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream is(line);
    for (std::string w; is >> w;) out.push_back(w);
    return out;
}

struct GroupDef {
    const char* group;
    const char* block;
    std::vector<std::string> clips;
};

// Hard-coded in the exe (CAnimManager anim group definitions).
const std::vector<GroupDef>& Groups() {
    static const std::vector<GroupDef> g = {
        {"melee_1", "ped", {"FightA_1", "FightA_2", "FightA_3", "FightA_G", "FightA_M", "HitA_1", "HitA_2", "HitA_3", "FightA_Block", "FIGHTIDLE"}},
        {"melee_2", "fight_b", {"FightB_1", "FightB_2", "FightB_3", "FightB_G", "FightB_M", "HitB_1", "HitB_2", "HitB_3", "FightB_Block", "FightB_Idle"}},
        {"melee_3", "fight_c", {"FightC_1", "FightC_2", "FightC_3", "FightC_G", "FightC_M", "HitC_1", "HitC_2", "HitC_3", "FightC_Block", "FightC_Idle"}},
        {"melee_4", "fight_d", {"FightD_1", "FightD_2", "FightD_3", "FightD_G", "FightD_M", "HitD_1", "HitD_2", "HitD_3", "FightD_Block", "FightD_Idle"}},
        {"bbbat_1", "baseball", {"Bat_1", "Bat_2", "Bat_3", "Bat_4", "BAT_PART", "Bat_Hit_1", "Bat_Hit_2", "Bat_Hit_3", "Bat_Block", "Bat_Idle"}},
        {"gclub_1", "baseball", {"Bat_1", "Bat_2", "Bat_3", "Bat_4", "BAT_PART", "Bat_Hit_1", "Bat_Hit_2", "Bat_Hit_3", "Bat_Block", "Bat_Idle"}},
        {"knife_1", "knife", {"knife_1", "knife_2", "knife_3", "knife_G", "knife_part", "knife_hit_1", "knife_hit_2", "knife_hit_3", "knife_block", "WEAPON_knifeidle"}},
        {"sword_1", "sword", {"sword_1", "sword_2", "sword_3", "sword_4", "sword_part", "sword_Hit_1", "sword_Hit_2", "sword_Hit_3", "sword_block", "sword_idle"}},
        {"dildo_1", "dildo", {"Dildo_1", "Dildo_2", "Dildo_3", "Dildo_G", "Dildo_1", "Dildo_Hit_1", "Dildo_Hit_2", "Dildo_Hit_3", "Dildo_block", "Dildo_idle"}},
        {"flowers_1", "flowers", {"Flower_attack", "Flower_attack", "Flower_attack", "Flower_attack", "Flower_attack", "Flower_hit", "Flower_hit", "Flower_hit"}},
        {"csaw_1", "chainsaw", {"CSaw_1", "CSaw_2", "CSaw_3", "CSaw_G", "CSaw_Part", "CSaw_Hit_1", "CSaw_Hit_2", "CSaw_Hit_3", "CSaw_Part", "Idle_CSaw"}},
        {"kick_std", "fight_e", {"FIGHTkick", "FIGHTkick_B", "FIGHTkick", "FIGHTkick", "FIGHTkick", "HIT_FightKick", "Hit_FightKick_B"}},
        {"pistlwhp", "ped", {"GUN_BUTT", "GUN_BUTT_crouch", "GUN_BUTT", "GUN_BUTT", "GUN_BUTT", "HIT_GUN_BUTT", "HIT_GUN_BUTT"}},
    };
    return g;
}

int LevelIndex(const char* l) {
    const char a = l[0], b = l[0] ? l[1] : 0;
    int base = a == 'H' ? 0 : a == 'L' ? 1 : a == 'G' ? 2 : a == 'B' ? 3 : -1;
    if (base < 0) return 0;
    if (b == 'L') return base == 0 ? 4 : base == 1 ? 5 : base == 2 ? 6 : base;
    return base;
}

}  // namespace

const Combo* Table::Find(const std::string& name) const {
    for (const Combo& c : combos) if (c.name == name) return &c;
    return nullptr;
}

bool Load(const std::string& gameDir, Table& out) {
    std::ifstream f(gameDir + "\\data\\melee.dat");
    if (!f) return false;
    static const char* kLevels[7] = {"LEVEL_H", "LEVEL_L", "LEVEL_G", "LEVEL_B", "LEVEL_HL", "LEVEL_LL", "LEVEL_GL"};
    Combo* cur = nullptr;
    for (std::string line; std::getline(f, line);) {
        if (const size_t h = line.find('#'); h != std::string::npos) line.erase(h);
        std::vector<std::string> t = Words(line);
        if (t.empty()) continue;
        const std::string& k = t[0];
        auto num = [&](size_t i) { return i < t.size() ? (float)std::atof(t[i].c_str()) : 0.0f; };
        for (int i = 0; i < 7; i++)
            if (k == kLevels[i]) for (int j = 0; j < 3; j++) out.levelOffset[i][j] = num(1 + (size_t)j);
        if (k == "START_COMBO" && t.size() > 1) {
            out.combos.emplace_back();
            cur = &out.combos.back();
            cur->name = t[1];
        } else if (k == "END_COMBO") {
            cur = nullptr;
        } else if (!cur) {
            continue;
        } else if (k == "ANIMGROUP" && t.size() > 1) {
            cur->group = t[1];
        } else if (k == "RANGES") {
            cur->range = num(1);
        } else if (k == "FLAGS" && t.size() > 1) {
            cur->flags = (unsigned)std::strtoul(t[1].c_str(), nullptr, 16);
        } else if (k == "ABLOCK") {
            cur->blockHit = num(1) * kFrame;
            cur->blockChain = num(2) * kFrame;
        } else {
            int mi = k == "ATTACK1" ? kAttack1 : k == "ATTACK2" ? kAttack2 : k == "ATTACK3" ? kAttack3
                   : k == "AGROUND" ? kGround : k == "AMOVING" ? kMoving : -1;
            if (mi < 0 || t.size() < 8) continue;
            Move& m = cur->moves[mi];
            m.hit = num(1) * kFrame;
            m.chain = num(2) * kFrame;
            m.radius = num(3);
            m.level[0] = t[4][0];
            m.level[1] = t[4].size() > 1 ? t[4][1] : 0;
            m.damage = num(5);
            m.hitAudio = (int)num(6);
            m.altHitAudio = (int)num(7);
            if (mi == kGround) m.groundLoop = num(8) * kFrame;
        }
    }
    return !out.combos.empty();
}

void StrikeCentre(const Table& t, const char* level, const float pos[3], float heading, float out[3]) {
    const float* o = t.levelOffset[LevelIndex(level)];
    // offset: x right, y forward; the engine heading is the angle of the facing from +x
    const float c = std::cos(heading), s = std::sin(heading);
    out[0] = pos[0] + o[1] * c + o[0] * s;
    out[1] = pos[1] + o[1] * s - o[0] * c;
    out[2] = pos[2] + o[2];
}

const std::vector<std::string>& GroupClips(const std::string& group) {
    static const std::vector<std::string> none;
    for (const GroupDef& g : Groups()) if (group == g.group) return g.clips;
    return none;
}

std::string GroupBlock(const std::string& group) {
    for (const GroupDef& g : Groups()) if (group == g.group) return g.block;
    return "";
}

}  // namespace Melee
