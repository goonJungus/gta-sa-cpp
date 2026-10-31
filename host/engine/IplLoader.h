#pragma once
// IPL (Item Placement) parser for gtasa_cpp.exe (Milestone 4).
// Parses the "inst" section:
//   id, modelName, interior, posX, posY, posZ, rotX, rotY, rotZ, rotW, lodIndex
// Rotation is a quaternion. LoadGarages reads the "grge" section; the others (cull, path, ...) are skipped.

#include <cstdint>
#include <string>
#include <vector>

struct IplInstance {
    int id = -1;
    std::string modelName;  // lowercase
    int interior = 0;
    float x = 0, y = 0, z = 0;           // position
    float qx = 0, qy = 0, qz = 0, qw = 1; // rotation quaternion
    int lodIndex = -1;
};

// One "grge" line (CFileLoader::LoadGarage -> CGarages::AddOne): a corner, the far end of its first edge (x2, y2),
// the far end of its second edge (x3, y3), the roof height, the flags, the eGarageType and the name.
struct IplGarage {
    float x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0, topZ = 0;
    int flags = 0, type = 0;
    std::string name;
};

class IplLoader {
public:
    // Parse an IPL file, appending instances to outInstances.
    // Returns false on failure.
    static bool Load(const std::string& path,
                     std::vector<IplInstance>& outInstances);
    // Append the file's "grge" entries.
    static bool LoadGarages(const std::string& path, std::vector<IplGarage>& out);

private:
    static std::string Trim(const std::string& s);
    static std::string ToLower(const std::string& s);
    static std::vector<std::string> SplitComma(const std::string& line);
};
