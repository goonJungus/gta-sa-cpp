#pragma once
// Skinned RenderWare clump loader (ped / player-clothes DFFs).
//
// A ped DFF is a clump whose frame list is a skeleton: every bone frame carries
// an RpHAnim plugin (0x11E) with its bone id, the root bone frame also carries
// the hierarchy's node list, and the single atomic's geometry carries an RpSkin
// plugin (0x116): per-vertex bone indices + weights and one inverse bind matrix
// per node. Player clothes DFFs (player.img) hold three clumps -- "Ripped",
// "Fat", "Normal" -- that CClothesBuilder blends by CJ's muscle/fat stats.
//
// Matrices use RenderWare's row-vector convention (as RwMatrix): rows are the
// right/up/at axes and the position; p' = p.x*r0 + p.y*r1 + p.z*r2 + r3.

#include <cstdint>
#include <string>
#include <vector>

struct RwMat {
    float m[4][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};

    // this * b: apply this, then b (row-vector order, like RwMatrixMultiply).
    RwMat operator*(const RwMat& b) const;
    void Apply(const float in[3], float out[3]) const;        // point
    void ApplyDir(const float in[3], float out[3]) const;     // direction
};

struct SkinFrame {
    std::string name;
    int parent = -1;   // frame index, -1 = none
    int boneId = -1;   // RpHAnim node id, -1 if the frame is not a bone
    RwMat local;       // bind-pose local transform (modelling matrix)
};

struct SkinVertex {
    float pos[3];
    float normal[3];
    float uv[2];
    uint8_t bone[4];   // RpHAnim node *indices* (into nodeIds)
    float weight[4];
};

struct SkinnedClump {
    std::string rootName;                 // e.g. "Normal", "Fat", "Ripped"
    std::vector<SkinFrame> frames;
    std::vector<int> nodeIds;             // HAnim node index -> bone id
    std::vector<RwMat> inverseBind;       // per node index (skin space -> bone space)
    std::vector<SkinVertex> vertices;
    std::vector<uint16_t> indices;        // triangle list, D3D winding
    bool valid = false;
};

class SkinnedDff {
public:
    // All clumps in the file (player clothes have 3, a ped DFF has 1).
    static std::vector<SkinnedClump> LoadClumps(const uint8_t* data, size_t size);
};
