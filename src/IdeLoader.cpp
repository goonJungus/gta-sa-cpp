// IDE parser for gtasa_cpp.exe (Milestone 4).
// See IdeLoader.h for format details.

#include "IdeLoader.h"
#include <cstdio>
#include <cctype>

std::string IdeLoader::Trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && isspace((unsigned char)s[a]))
        a++;
    size_t b = s.size();
    while (b > a && isspace((unsigned char)s[b - 1]))
        b--;
    return s.substr(a, b - a);
}

std::string IdeLoader::ToLower(const std::string& s) {
    std::string out = s;
    for (char& c : out)
        c = (char)tolower((unsigned char)c);
    return out;
}

std::vector<std::string> IdeLoader::SplitComma(const std::string& line) {
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

bool IdeLoader::Load(const std::string& path,
                     std::vector<IdeObject>& outObjects,
                     std::unordered_map<int, size_t>& outById) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f)
        return false;

    char lineBuf[1024];
    bool inObjs = false;
    while (fgets(lineBuf, sizeof(lineBuf), f)) {
        std::string line = Trim(lineBuf);
        if (line.empty() || line[0] == '#')
            continue;

        std::string low = ToLower(line);
        if (low == "objs") {
            inObjs = true;
            continue;
        }
        if (low == "end") {
            inObjs = false;
            continue;
        }
        // Skip other section headers (tobj, hier, anim, cars, peds, ...).
        // A section header is a single word with no comma.
        if (line.find(',') == std::string::npos) {
            inObjs = false;  // any new section ends objs
            continue;
        }
        if (!inObjs)
            continue;

        std::vector<std::string> fields = SplitComma(line);
        if (fields.size() < 5)
            continue;
        IdeObject obj;
        try {
            obj.id = std::stoi(fields[0]);
        } catch (...) {
            continue;
        }
        obj.modelName = ToLower(fields[1]);
        obj.txdName = ToLower(fields[2]);
        try {
            obj.drawDistance = std::stof(fields[3]);
            obj.flags = std::stoi(fields[4]);
        } catch (...) {
            continue;
        }
        outById[obj.id] = outObjects.size();
        outObjects.push_back(std::move(obj));
    }

    fclose(f);
    return true;
}
