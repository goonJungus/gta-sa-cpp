#include "WeaponInfo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {
std::vector<WeaponInfo> g_rows;
constexpr float kFrame = 1.0f / 30.0f;   // weapon.dat anim frames are 1/30 s

std::vector<std::string> Tokens(const std::string& line) {
    std::vector<std::string> t;
    std::istringstream in(line);
    for (std::string s; in >> s;) t.push_back(s);
    return t;
}

WeaponInfo::Fire FireType(const std::string& s) {
    if (s == "MELEE") return WeaponInfo::Melee;
    if (s == "PROJECTILE") return WeaponInfo::Projectile;
    if (s == "AREA_EFFECT") return WeaponInfo::AreaEffect;
    if (s == "CAMERA") return WeaponInfo::Camera;
    if (s == "USE") return WeaponInfo::Use;
    return WeaponInfo::InstantHit;
}
}  // namespace

float WeaponTable::LoopEnd(float start, float end) {
    return start + std::floor(0.1f + (end - start) * 50.0f) / 50.0f - 0.006f;
}

int WeaponTable::Load(const std::string& gameDir) {
    g_rows.clear();
    std::ifstream f(gameDir + "\\data\\weapon.dat");
    for (std::string line; std::getline(f, line);) {
        if (line.find("ENDWEAPONDATA") != std::string::npos) break;
        if (line.empty()) continue;
        const bool melee = (unsigned char)line[0] == 0xA3;   // the pound sign marks a melee row
        if (line[0] != '$' && !melee) continue;
        const std::vector<std::string> t = Tokens(line.substr(1));
        WeaponInfo w;
        if (melee) {
            if (t.size() < 11) continue;
            w.name = t[0];
            w.fire = FireType(t[1]);
            w.targetRange = (float)std::atof(t[2].c_str());
            w.weaponRange = (float)std::atof(t[3].c_str());
            w.model1 = std::atoi(t[4].c_str());
            w.model2 = std::atoi(t[5].c_str());
            w.slot = std::atoi(t[6].c_str());
            w.baseCombo = t[7];
            w.numCombos = std::atoi(t[8].c_str());
            w.meleeFlags = (unsigned)std::strtoul(t[9].c_str(), nullptr, 16);
            w.stealthAnim = t[10];
            w.valid = true;
            g_rows.push_back(w);
            continue;
        }
        if (t.size() < 25) continue;
        w.name = t[0];
        w.fire = FireType(t[1]);
        w.targetRange = (float)std::atof(t[2].c_str());
        w.weaponRange = (float)std::atof(t[3].c_str());
        w.model1 = std::atoi(t[4].c_str());
        w.model2 = std::atoi(t[5].c_str());
        w.slot = std::atoi(t[6].c_str());
        w.animGroup = t[7];
        w.ammoClip = std::atoi(t[8].c_str());
        w.damage = std::atoi(t[9].c_str());
        for (int k = 0; k < 3; k++) w.fireOffset[k] = (float)std::atof(t[10 + k].c_str());
        w.skill = std::atoi(t[13].c_str());
        w.reqStat = std::atoi(t[14].c_str());
        w.accuracy = (float)std::atof(t[15].c_str());
        w.moveSpeed = (float)std::atof(t[16].c_str());
        auto loop = [&](int at, float& s, float& e, float& fi) {
            s = (float)std::atoi(t[at].c_str()) * kFrame;
            e = (float)std::atoi(t[at + 1].c_str()) * kFrame;
            fi = (float)std::atoi(t[at + 2].c_str()) * kFrame;
            e = WeaponTable::LoopEnd(s, e);
        };
        loop(17, w.loopStart, w.loopEnd, w.loopFire);
        loop(20, w.loop2Start, w.loop2End, w.loop2Fire);
        w.breakout = (float)std::atoi(t[23].c_str()) * kFrame;
        w.flags = (unsigned)std::strtoul(t[24].c_str(), nullptr, 16);
        if (t.size() > 25) w.speed = (float)std::atof(t[25].c_str());
        if (t.size() > 26) w.radius = (float)std::atof(t[26].c_str());
        if (t.size() > 27) w.lifespan = (float)std::atof(t[27].c_str());
        if (t.size() > 28) w.spread = (float)std::atof(t[28].c_str());
        w.valid = true;
        g_rows.push_back(w);
    }
    std::fprintf(stderr, "[GUN] weapon.dat: %zu rows\n", g_rows.size());
    return (int)g_rows.size();
}

const WeaponInfo* WeaponTable::Get(const std::string& name, int skill) {
    const WeaponInfo* any = nullptr;
    for (const WeaponInfo& w : g_rows) {
        if (w.name != name) continue;
        if (w.skill == skill || !w.animGroup.size()) return &w;
        any = &w;
    }
    // a weapon without every skill row answers the standard one, else whatever it has
    for (const WeaponInfo& w : g_rows)
        if (w.name == name && w.skill == 1) return &w;
    return any;
}

const std::vector<WeaponInfo>& WeaponTable::All() { return g_rows; }
