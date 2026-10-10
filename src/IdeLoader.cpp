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
    bool inTobj = false;
    while (fgets(lineBuf, sizeof(lineBuf), f)) {
        std::string line = Trim(lineBuf);
        if (line.empty() || line[0] == '#')
            continue;

        std::string low = ToLower(line);
        if (low == "objs") {
            inObjs = true;
            inTobj = false;
            continue;
        }
        // tobj (timed objects) share the objs first-5-field layout:
        // id, modelName, txdName, drawDistance, flags (+ timeOn, timeOff).
        // Parse them so binary-stream instances of timed models resolve.
        if (low == "tobj") {
            inTobj = true;
            inObjs = false;
            continue;
        }
        if (low == "end") {
            inObjs = false;
            inTobj = false;
            continue;
        }
        // Skip other section headers (hier, anim, cars, peds, ...).
        // A section header is a single word with no comma.
        if (line.find(',') == std::string::npos) {
            inObjs = false;  // any new section ends objs/tobj
            inTobj = false;
            continue;
        }
        if (!inObjs && !inTobj)
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

        // Standard form: id, name, txd, drawDistance, flags.
        // Extended objType form (gta-reversed CFileLoader::LoadObject):
        //   id, name, txd, objType(1-3), dd1[, dd2[, dd3]], flags.
        // Retail uses the extended form when the 4th field parses as a
        // draw distance < 4.0 (it's actually objType, not a distance).
        // Example: 320, airtrain_vlo, generic, 1, 2000, 0
        //   -> drawDistance=2000, flags=0 (NOT dd=1, flags=2000).
        float drawDist = 0.0f;
        int flags = 0;
        bool ok = false;
        try {
            drawDist = std::stof(fields[3]);
            flags = std::stoi(fields[4]);
            ok = true;
        } catch (...) {
            ok = false;
        }
        if (ok && drawDist < 4.0f) {
            try {
                int objType = std::stoi(fields[3]);
                if (objType >= 1 && objType <= 3) {
                    // dd1 is fields[4]; dd2/dd3 (objType 2/3) are unused
                    // by retail (fDrawDist2_unused); flags is last field.
                    drawDist = std::stof(fields[4]);
                    flags = (fields.size() >= 6) ? std::stoi(fields[fields.size() - 1]) : 0;
                    ok = true;
                } else {
                    ok = false;
                }
            } catch (...) {
                ok = false;
            }
        }
        if (!ok)
            continue;
        obj.drawDistance = drawDist;
        obj.flags = flags;

        outById[obj.id] = outObjects.size();
        outObjects.push_back(std::move(obj));
    }

    fclose(f);
    return true;
}
