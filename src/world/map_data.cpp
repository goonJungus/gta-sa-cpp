// map_data.cpp - see map_data.hpp.

#include "world/map_data.hpp"

#include <fstream>
#include <sstream>
#include <cstring>
#include <cctype>
#include <algorithm>

namespace gta {

namespace {

std::string Trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}
std::string ToLower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
std::vector<std::string> SplitComma(const std::string& line) {
    std::vector<std::string> f;
    std::string cur;
    for (char c : line) {
        if (c == ',') { f.push_back(Trim(cur)); cur.clear(); }
        else cur += c;
    }
    f.push_back(Trim(cur));
    return f;
}
// Normalize a gta.dat relative path to the host filesystem (backslashes ->
// native). std::ifstream on Windows accepts '/' too, so just unify to '/'.
std::string NormPath(std::string p) {
    for (char& c : p) if (c == '\\') c = '/';
    return p;
}

}  // namespace

bool ParseGtaDat(const std::string& gameDir,
                 std::vector<std::string>& ideRel,
                 std::vector<std::string>& iplRel) {
    std::ifstream f(gameDir + "/data/gta.dat");
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        std::string t = Trim(line);
        if (t.size() < 5 || t[0] == '#') continue;
        std::string kind = ToLower(t.substr(0, 3));
        std::string rest = Trim(t.substr(3));
        if (kind == "ide") ideRel.push_back(NormPath(rest));
        else if (kind == "ipl") iplRel.push_back(NormPath(rest));
    }
    return true;
}

bool LoadIde(const std::string& path, std::vector<ObjectDef>& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    int section = 0;  // 1 = objs, 3 = tobj
    while (std::getline(f, line)) {
        std::string t = Trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::string low = ToLower(t);
        if (low == "objs") { section = 1; continue; }
        if (low == "tobj") { section = 3; continue; }
        if (low == "end")  { section = 0; continue; }
        // any other single-word line is a new (unhandled) section header
        if (t.find(',') == std::string::npos) { section = 0; continue; }
        if (section != 1 && section != 3) continue;

        std::vector<std::string> fields = SplitComma(t);
        if (fields.size() < 5) continue;
        ObjectDef o;
        try { o.id = std::stoi(fields[0]); } catch (...) { continue; }
        o.modelName = ToLower(fields[1]);
        o.txdName = ToLower(fields[2]);
        o.isTimed = (section == 3);

        // Standard: id,name,txd,drawDist,flags.
        // Extended: id,name,txd,objType(1-3),dd1[,dd2[,dd3]],flags - used when
        // field[3] parses below 4.0 (it's really objType, not a distance).
        float drawDist = 0.0f; int flags = 0; bool ok = false;
        try { drawDist = std::stof(fields[3]); flags = std::stoi(fields[4]); ok = true; }
        catch (...) { ok = false; }
        if (ok && drawDist < 4.0f) {
            try {
                int objType = std::stoi(fields[3]);
                if (objType >= 1 && objType <= 3) {
                    drawDist = std::stof(fields[4]);
                    flags = (fields.size() >= 6) ? std::stoi(fields[fields.size() - 1]) : 0;
                } else ok = false;
            } catch (...) { ok = false; }
        }
        if (!ok) continue;
        o.drawDistance = drawDist;
        o.flags = (uint32_t)flags;
        out.push_back(std::move(o));
    }
    return true;
}

bool LoadIplText(const std::string& path, std::vector<Placement>& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    bool inInst = false;
    while (std::getline(f, line)) {
        std::string t = Trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::string low = ToLower(t);
        if (low == "inst") { inInst = true; continue; }
        if (low == "end")  { inInst = false; continue; }
        if (!inInst) continue;
        if (t.find(',') == std::string::npos) { inInst = false; continue; }

        std::vector<std::string> f2 = SplitComma(t);
        if (f2.size() < 10) continue;  // id,model,interior,x,y,z,qx,qy,qz,qw[,lod]
        Placement p;
        try {
            p.modelId = std::stoi(f2[0]);
            p.interior = std::stoi(f2[2]);
            p.position = {std::stof(f2[3]), std::stof(f2[4]), std::stof(f2[5])};
            // SA IPL quats are the inverse of the applied rotation; conjugate
            // (negate x,y,z) so the stored quat is ready to apply directly.
            p.rotation = {-std::stof(f2[6]), -std::stof(f2[7]), -std::stof(f2[8]),
                          std::stof(f2[9])};
            p.lod = (f2.size() >= 11) ? std::stoi(f2[10]) : -1;
        } catch (...) { continue; }
        out.push_back(p);
    }
    return true;
}

bool LoadIplBinary(const uint8_t* data, std::size_t size,
                   std::vector<Placement>& out) {
    if (!data || size < 0x4c) return false;
    if (std::memcmp(data, "bnry", 4) != 0) return false;
    auto rdI32 = [&](std::size_t o) {
        int32_t v; std::memcpy(&v, data + o, 4); return v;
    };
    auto rdF32 = [&](std::size_t o) {
        float v; std::memcpy(&v, data + o, 4); return v;
    };
    int32_t instCount = rdI32(4);
    int32_t instOff = rdI32(0x1c);
    if (instCount < 0 || instCount > 1000000) return false;
    if (instOff < 0 || (std::size_t)instOff + (std::size_t)instCount * 40 > size) return false;

    for (int32_t i = 0; i < instCount; i++) {
        std::size_t r = (std::size_t)instOff + (std::size_t)i * 40;
        Placement p;
        p.position = {rdF32(r + 0), rdF32(r + 4), rdF32(r + 8)};
        // conjugate (see text path)
        p.rotation = {-rdF32(r + 12), -rdF32(r + 16), -rdF32(r + 20), rdF32(r + 24)};
        p.modelId = rdI32(r + 28);
        int32_t areaAndFlags = rdI32(r + 32);
        p.interior = areaAndFlags & 0xFF;
        p.lod = rdI32(r + 36);
        out.push_back(p);
    }
    return true;
}

}  // namespace gta
