#pragma once
// IDE (Item Definition) parser for gtasa_cpp.exe (Milestone 4).
// Parses the "objs" and "tobj" sections: id, modelName, txdName,
// drawDistance, flags (tobj timeOn/timeOff are ignored).
// Other sections (hier, anim, cars, peds, weap, 2dfx) are skipped.

#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>

struct IdeObject {
    int id = -1;
    std::string modelName;  // DFF name without extension (lowercase)
    std::string txdName;    // TXD name without extension (lowercase)
    float drawDistance = 0;
    int flags = 0;
};

// One txd-parent relationship from a "txdp" section (child TXD -> parent TXD,
// both lowercase, no extension). Retail: CFileLoader::LoadTXDParent, section
// 0xb in CFileLoader::LoadObjectTypes. RW resolves a texture missing from a
// child txd by walking up to its parent dictionary.
struct TxdParent {
    std::string child;
    std::string parent;
};

class IdeLoader {
public:
    // Parse an IDE file. Returns false on failure.
    // Objects are appended to outObjects; outById maps id -> index.
    // If outTxdParents is non-null, "txdp" entries are appended to it.
    static bool Load(const std::string& path,
                     std::vector<IdeObject>& outObjects,
                     std::unordered_map<int, size_t>& outById,
                     std::vector<TxdParent>* outTxdParents = nullptr);

private:
    static std::string Trim(const std::string& s);
    static std::string ToLower(const std::string& s);
    // Split a comma-separated line, trimming each field.
    static std::vector<std::string> SplitComma(const std::string& line);
};
