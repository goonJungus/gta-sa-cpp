#pragma once
// IFP (ANP3) animation package loader -- CAnimManager::LoadAnimFile.
//
// An ANP3 file is a block name and a list of clips; each clip is a list of
// per-bone sequences matched to a skeleton by bone id (the RpHAnim node id).
// Frame types (the per-sequence "frameType"):
//   1 KR00 uncompressed: quat(4f) time(f)                  20 bytes
//   2 KRT0 uncompressed: quat(4f) time(f) trans(3f)        32 bytes
//   3 KR00 compressed:   quat(4s/4096) time(s/60)          10 bytes
//   4 KRT0 compressed:   quat(4s/4096) time(s/60) t(3s/1024) 16 bytes
// ANP3 quaternions are used as stored (only the old ANPK format negates xyz).

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct AnimKey {
    float q[4];      // x, y, z, w
    float t[3];      // translation (valid when the sequence has translation)
    float time;      // seconds
};

struct AnimSequence {
    std::string name;
    int boneId = -1;
    bool hasTranslation = false;
    std::vector<AnimKey> keys;
};

struct AnimClip {
    std::string name;
    float duration = 0.0f;
    std::vector<AnimSequence> sequences;
    const AnimSequence* FindBone(int boneId) const;
};

class IfpLoader {
public:
    // Load every clip of an ANP3 file. Keys are lowercase clip names.
    static bool Load(const std::string& path, std::unordered_map<std::string, AnimClip>& out);
    // Same, from an ANP3 image (e.g. a file of anim\anim.img). keepExisting:
    // clips already in out win over same-named ones in this file.
    static bool LoadFromMemory(const std::vector<uint8_t>& data,
                               std::unordered_map<std::string, AnimClip>& out, bool keepExisting = false);
};
