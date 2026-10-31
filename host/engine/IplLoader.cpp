// IPL parser for gtasa_cpp.exe (Milestone 4).
// See IplLoader.h for format details.

#include "IplLoader.h"
#include <cstdio>
#include <cctype>

std::string IplLoader::Trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && isspace((unsigned char)s[a]))
        a++;
    size_t b = s.size();
    while (b > a && isspace((unsigned char)s[b - 1]))
        b--;
    return s.substr(a, b - a);
}

std::string IplLoader::ToLower(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        c = (char)tolower((unsigned char)c);
    return out;
}

std::vector<std::string> IplLoader::SplitComma(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    for (char c : line) {
        if (c == ',') {
            fields.push_back(Trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    fields.push_back(Trim(cur));
    return fields;
}

bool IplLoader::LoadGarages(const std::string& path, std::vector<IplGarage>& out) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char lineBuf[1024];
    bool in = false;
    while (fgets(lineBuf, sizeof(lineBuf), f)) {
        const std::string line = Trim(lineBuf);
        if (line.empty() || line[0] == '#') continue;
        const std::string low = ToLower(line);
        if (line.find(',') == std::string::npos) { in = low == "grge"; continue; }
        if (!in) continue;
        const std::vector<std::string> v = SplitComma(line);
        if (v.size() < 10) continue;
        IplGarage g;
        try {
            g.x1 = std::stof(v[0]); g.y1 = std::stof(v[1]); g.z1 = std::stof(v[2]);
            g.x2 = std::stof(v[3]); g.y2 = std::stof(v[4]);
            g.x3 = std::stof(v[5]); g.y3 = std::stof(v[6]); g.topZ = std::stof(v[7]);
            g.flags = std::stoi(v[8]); g.type = std::stoi(v[9]);
        } catch (...) {
            continue;
        }
        if (v.size() > 10) g.name = v[10];
        out.push_back(std::move(g));
    }
    fclose(f);
    return true;
}

bool IplLoader::Load(const std::string& path,
                     std::vector<IplInstance>& outInstances) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f)
        return false;

    char lineBuf[1024];
    bool inInst = false;
    while (fgets(lineBuf, sizeof(lineBuf), f)) {
        std::string line = Trim(lineBuf);
        if (line.empty() || line[0] == '#')
            continue;

        std::string low = ToLower(line);
        if (low == "inst") {
            inInst = true;
            continue;
        }
        if (low == "end") {
            inInst = false;
            continue;
        }
        if (line.find(',') == std::string::npos) {
            inInst = false;
            continue;
        }
        if (!inInst)
            continue;

        std::vector<std::string> fields = SplitComma(line);
        if (fields.size() < 11)
            continue;
        IplInstance inst;
        try {
            inst.id = std::stoi(fields[0]);
            inst.modelName = ToLower(fields[1]);
            inst.interior = std::stoi(fields[2]);
            inst.x = std::stof(fields[3]);
            inst.y = std::stof(fields[4]);
            inst.z = std::stof(fields[5]);
            // Conjugate: SA IPL quats are the inverse of the applied rotation
            // (same convention as the binary IPL loader -- see BinaryIpl.cpp).
            // These feed LOD instances; keep them consistent with their HD.
            inst.qx = -std::stof(fields[6]);
            inst.qy = -std::stof(fields[7]);
            inst.qz = -std::stof(fields[8]);
            inst.qw = std::stof(fields[9]);
            inst.lodIndex = std::stoi(fields[10]);
        } catch (...) {
            continue;
        }
        outInstances.push_back(std::move(inst));
    }

    fclose(f);
    return true;
}
