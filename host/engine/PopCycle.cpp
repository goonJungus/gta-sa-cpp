#include "PopCycle.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace {
std::string Upper(std::string s) {
    for (char& c : s) c = (char)std::toupper((unsigned char)c);
    return s;
}
std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
}  // namespace

bool PopCycle::Load(const std::string& gameDir, const std::function<int(const std::string&)>& modelOfDff) {
    m_ready = false;
    m_zones.clear();
    // data/info.zon: "name, type, x1, y1, z1, x2, y2, z2, island, text"
    {
        std::ifstream f(gameDir + "/data/info.zon");
        std::string line;
        while (std::getline(f, line)) {
            line = Trim(line);
            if (line.empty() || line == "zone" || line == "end") continue;
            std::replace(line.begin(), line.end(), ',', ' ');
            std::istringstream ls(line);
            Zone z;
            int type, island;
            if (ls >> z.label >> type >> z.x1 >> z.y1 >> z.z1 >> z.x2 >> z.y2 >> z.z2 >> island) {
                z.label = Upper(z.label);
                if (z.x1 > z.x2) std::swap(z.x1, z.x2);
                if (z.y1 > z.y2) std::swap(z.y1, z.y2);
                if (z.z1 > z.z2) std::swap(z.z1, z.z2);
                m_zones.push_back(z);
            }
        }
    }
    // data/popcycle.dat: per zone type (20), weekday then weekend, 12 rows: #peds #cars dealers gang cops other, then
    // the group percentages (the 18 ped groups first)
    {
        std::ifstream f(gameDir + "/data/popcycle.dat");
        std::string line;
        int row = 0;
        while (std::getline(f, line) && row < kTypes * 2 * 12) {
            line = Trim(line);
            if (line.empty() || !std::isdigit((unsigned char)line[0])) continue;
            std::istringstream ls(line);
            std::vector<float> v;
            float x;
            while (ls >> x) v.push_back(x);
            if (v.size() < (size_t)(6 + kGroups)) continue;
            Row& r = m_rows[row / 24][(row / 12) % 2][row % 12];
            r.cars = (int)v[1];
            for (int g = 0; g < kGroups; g++) r.group[g] = v[(size_t)(6 + g)];
            row++;
        }
        if (row < kTypes * 2 * 12) return false;
    }
    // data/cargrp.dat: the first 18 lines are the cars of the ped groups, in popcycle.dat's column order
    {
        std::ifstream f(gameDir + "/data/cargrp.dat");
        std::string line;
        int grp = 0;
        while (std::getline(f, line) && grp < kGroups) {
            const size_t hash = line.find('#');
            if (hash != std::string::npos) line.resize(hash);
            line = Trim(line);
            if (line.empty()) continue;
            std::replace(line.begin(), line.end(), ',', ' ');
            std::istringstream ls(line);
            std::string name;
            while (ls >> name) {
                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                const int id = modelOfDff(name);
                if (id >= 0) m_groupCars[grp].push_back(id);
            }
            grp++;
        }
    }
    m_ready = !m_zones.empty();
    std::fprintf(stderr, "[POP] %zu info zones, popcycle loaded\n", m_zones.size());
    return m_ready;
}

int PopCycle::TypeAt(float x, float y, float z) const {
    const Zone* best = nullptr;
    float bestArea = 1e30f;
    for (const Zone& zn : m_zones) {
        if (x < zn.x1 || x > zn.x2 || y < zn.y1 || y > zn.y2 || z < zn.z1 || z > zn.z2) continue;
        const float area = (zn.x2 - zn.x1) * (zn.y2 - zn.y1);
        if (area < bestArea) { bestArea = area; best = &zn; }
    }
    if (best) {
        auto it = m_typeByLabel.find(best->label);
        if (it != m_typeByLabel.end()) return it->second;
    }
    return kResidentialAverage;
}

void PopCycle::SetZonePopulationType(const std::string& label, int type) {
    if (type >= 0 && type < kTypes) m_typeByLabel[Upper(label)] = type;
}

int PopCycle::MaxCars(int type, float hour, bool weekend) const {
    if (type < 0 || type >= kTypes) type = kResidentialAverage;
    const int slot = std::clamp((int)(hour / 2.0f), 0, 11);
    return m_rows[type][weekend ? 1 : 0][slot].cars;
}

int PopCycle::PickCar(int type, float hour, bool weekend, float r1, float r2, const std::function<bool(int)>& ok) const {
    if (type < 0 || type >= kTypes) type = kResidentialAverage;
    const Row& row = m_rows[type][weekend ? 1 : 0][std::clamp((int)(hour / 2.0f), 0, 11)];
    float total = 0.0f;
    for (int g = 0; g < kGroups; g++) if (!m_groupCars[g].empty()) total += row.group[g];
    if (total <= 0.0f) return -1;
    float pick = r1 * total;
    for (int g = 0; g < kGroups; g++) {
        if (m_groupCars[g].empty()) continue;
        pick -= row.group[g];
        if (pick > 0.0f) continue;
        const std::vector<int>& cars = m_groupCars[g];
        for (size_t k = 0; k < cars.size(); k++) {   // from a random start, the first model that fits
            const int id = cars[((size_t)(r2 * (float)cars.size()) + k) % cars.size()];
            if (ok(id)) return id;
        }
        return -1;
    }
    return -1;
}
