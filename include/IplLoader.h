#pragma once
// IPL (Item Placement) parser for gtasa_cpp.exe (Milestone 4).
// Parses the "inst" section:
//   id, modelName, interior, posX, posY, posZ, rotX, rotY, rotZ, rotW, lodIndex
// Rotation is a quaternion. Other sections (cull, path, grge, etc.) skipped.

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

class IplLoader {
public:
    // Parse an IPL file, appending instances to outInstances.
    // Returns false on failure.
    static bool Load(const std::string& path,
                     std::vector<IplInstance>& outInstances);

private:
    static std::string Trim(const std::string& s);
    static std::string ToLower(const std::string& s);
    static std::vector<std::string> SplitComma(const std::string& line);
};
