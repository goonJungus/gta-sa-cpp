// IFP (ANP3) animation loader. See IfpLoader.h.

#include "IfpLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>

const AnimSequence* AnimClip::FindBone(int boneId) const {
    for (const AnimSequence& s : sequences)
        if (s.boneId == boneId) return &s;
    return nullptr;
}

void AnimSequence::Sample(float time, float q[4], float t[3]) const {
    q[0] = q[1] = q[2] = 0.0f; q[3] = 1.0f;
    t[0] = t[1] = t[2] = 0.0f;
    if (keys.empty()) return;
    size_t k = 0;
    while (k + 1 < keys.size() && keys[k + 1].time <= time) k++;
    const AnimKey& ka = keys[k];
    const AnimKey& kb = keys[std::min(k + 1, keys.size() - 1)];
    const float span = kb.time - ka.time;
    const float u = span > 1e-5f ? std::min(1.0f, std::max(0.0f, (time - ka.time) / span)) : 0.0f;
    float b[4] = {kb.q[0], kb.q[1], kb.q[2], kb.q[3]};
    float d = ka.q[0] * b[0] + ka.q[1] * b[1] + ka.q[2] * b[2] + ka.q[3] * b[3];
    if (d < 0) { d = -d; for (float& v : b) v = -v; }
    float wa = 1 - u, wb = u;
    if (d < 0.9995f) {
        const float th = std::acos(d), sn = std::sin(th);
        wa = std::sin((1 - u) * th) / sn;
        wb = std::sin(u * th) / sn;
    }
    float len = 0;
    for (int i = 0; i < 4; i++) { q[i] = wa * ka.q[i] + wb * b[i]; len += q[i] * q[i]; }
    len = std::sqrt(len);
    if (len > 1e-6f) for (int i = 0; i < 4; i++) q[i] /= len;
    if (hasTranslation) for (int i = 0; i < 3; i++) t[i] = ka.t[i] + (kb.t[i] - ka.t[i]) * u;
}

bool IfpLoader::LoadAnpk(const std::vector<uint8_t>& d, std::unordered_map<std::string, AnimClip>& out) {
    if (d.size() < 12 || std::memcmp(d.data(), "ANPK", 4) != 0) return false;
    auto get32 = [&](size_t at) -> uint32_t {
        uint32_t v = 0;
        if (at + 4 <= d.size()) std::memcpy(&v, d.data() + at, 4);
        return v;
    };
    auto pad4 = [](uint32_t n) { return (n + 3u) & ~3u; };
    const bool negate = std::getenv("GTA_CUTQPOS") == nullptr;   // the old format stores the conjugate rotation; dev: GTA_CUTQPOS keeps the file sign
    AnimClip clip;
    bool have = false, inClip = false;
    auto flush = [&] {
        if (!have) return;
        std::string key = clip.name;
        for (char& ch : key) ch = (char)std::tolower((unsigned char)ch);
        out[key] = std::move(clip);
        clip = AnimClip();
        have = false;
    };
    size_t p = 8;
    int boneId = -1;
    std::string seqName;
    while (p + 8 <= d.size()) {
        const char* tag = (const char*)d.data() + p;
        const uint32_t size = get32(p + 4);
        const size_t body = p + 8;
        if (!std::memcmp(tag, "DGAN", 4) || !std::memcmp(tag, "CPAN", 4)) {   // containers: step inside
            if (!std::memcmp(tag, "DGAN", 4)) inClip = true;
            p = body;
            continue;
        }
        if (body + size > d.size()) break;
        if (!std::memcmp(tag, "NAME", 4)) {
            flush();
            clip.name = std::string((const char*)d.data() + body, strnlen((const char*)d.data() + body, size));
            have = true;
            inClip = false;
        } else if (!std::memcmp(tag, "ANIM", 4) && size >= 0x2C) {
            seqName = std::string((const char*)d.data() + body, strnlen((const char*)d.data() + body, 28));
            boneId = (int32_t)get32(body + 28 + 4 + 8);
        } else if (!std::memcmp(tag, "KR00", 4) || !std::memcmp(tag, "KRT0", 4)) {
            const bool trans = tag[2] == 'T';
            const size_t stride = trans ? 32 : 20;
            AnimSequence seq;
            seq.name = seqName;
            seq.boneId = boneId;
            seq.hasTranslation = trans;
            const uint32_t frames = size / (uint32_t)stride;
            seq.keys.resize(frames);
            for (uint32_t k = 0; k < frames; k++) {
                float v[8] = {};
                std::memcpy(v, d.data() + body + k * stride, stride);
                AnimKey& key = seq.keys[k];
                for (int i = 0; i < 4; i++) key.q[i] = v[i];
                if (negate) for (int i = 0; i < 3; i++) key.q[i] = -key.q[i];
                if (trans) { for (int i = 0; i < 3; i++) key.t[i] = v[4 + i]; key.time = v[7]; }
                else { key.t[0] = key.t[1] = key.t[2] = 0.0f; key.time = v[4]; }
            }
            if (!seq.keys.empty() && seq.keys.back().time > clip.duration) clip.duration = seq.keys.back().time;
            clip.sequences.push_back(std::move(seq));
        }
        (void)inClip;
        p = body + pad4(size);
    }
    flush();
    return !out.empty();
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
    return LoadFromMemory(d, out);
}

bool IfpLoader::LoadFromMemory(const std::vector<uint8_t>& d, std::unordered_map<std::string, AnimClip>& out,
                               bool keepExisting) {
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
        if (keepExisting && out.count(key)) continue;
        out[key] = std::move(clip);
    }
    return !out.empty();
}
