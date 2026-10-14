// IFP (ANP3) animation loader. See IfpLoader.h.

#include "IfpLoader.h"

#include <cctype>
#include <cstdio>
#include <cstring>

const AnimSequence* AnimClip::FindBone(int boneId) const {
    for (const AnimSequence& s : sequences)
        if (s.boneId == boneId) return &s;
    return nullptr;
}

bool IfpLoader::Load(const std::string& path, std::unordered_map<std::string, AnimClip>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> d;
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size > 0) {
        d.resize((size_t)size);
        if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
    }
    std::fclose(f);
    if (d.size() < 36 || std::memcmp(d.data(), "ANP3", 4) != 0) return false;

    auto get32 = [&](size_t at) -> uint32_t {
        uint32_t v = 0;
        if (at + 4 <= d.size()) std::memcpy(&v, d.data() + at, 4);
        return v;
    };
    auto name24 = [&](size_t at) {
        std::string s((const char*)d.data() + at, strnlen((const char*)d.data() + at, 24));
        return s;
    };

    const uint32_t numAnims = get32(32);
    size_t p = 36;
    for (uint32_t a = 0; a < numAnims && p + 36 <= d.size(); a++) {
        AnimClip clip;
        clip.name = name24(p);
        const uint32_t numSeq = get32(p + 24);
        p += 36;
        for (uint32_t s = 0; s < numSeq && p + 36 <= d.size(); s++) {
            AnimSequence seq;
            seq.name = name24(p);
            const uint32_t type = get32(p + 24), frames = get32(p + 28);
            seq.boneId = (int32_t)get32(p + 32);
            p += 36;
            const size_t stride = type == 1 ? 20 : type == 2 ? 32 : type == 3 ? 10 : type == 4 ? 16 : 0;
            if (!stride || p + frames * stride > d.size()) return false;
            seq.hasTranslation = (type == 2 || type == 4);
            seq.keys.resize(frames);
            for (uint32_t k = 0; k < frames; k++) {
                const uint8_t* r = d.data() + p + k * stride;
                AnimKey& key = seq.keys[k];
                key.t[0] = key.t[1] = key.t[2] = 0.0f;
                if (type >= 3) {
                    int16_t v[8] = {};
                    std::memcpy(v, r, stride);
                    for (int i = 0; i < 4; i++) key.q[i] = v[i] / 4096.0f;
                    key.time = v[4] / 60.0f;
                    if (type == 4)
                        for (int i = 0; i < 3; i++) key.t[i] = v[5 + i] / 1024.0f;
                } else {
                    float v[8] = {};
                    std::memcpy(v, r, stride);
                    for (int i = 0; i < 4; i++) key.q[i] = v[i];
                    key.time = v[4];
                    if (type == 2)
                        for (int i = 0; i < 3; i++) key.t[i] = v[5 + i];
                }
            }
            p += frames * stride;
            if (!seq.keys.empty() && seq.keys.back().time > clip.duration)
                clip.duration = seq.keys.back().time;
            clip.sequences.push_back(std::move(seq));
        }
        std::string key = clip.name;
        for (char& ch : key) ch = (char)std::tolower((unsigned char)ch);
        out[key] = std::move(clip);
    }
    return !out.empty();
}
