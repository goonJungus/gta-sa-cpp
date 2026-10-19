#include "Speech.h"

#include <cstring>

#include "Audio.h"
#include "SpeechTables.inc"

namespace {
const SpeechGlobalCtx* Global(int gctx) {   // the table has a few gaps, so it is searched
    for (const SpeechGlobalCtx& g : kSpeechGlobal)
        if (g.gctx == gctx) return &g;
    return nullptr;
}
}  // namespace

int Speech::GenVoice(const std::string& name) {
    for (int i = 0; i < 209; i++)
        if (name == kSpeechGenVoice[i]) return i;
    return -1;
}

bool Speech::Play(int key, int gctx, int bank, int first, int last, float vol, float pan) {
    if (!m_audio || first < 0 || last < first || bank < 0) return false;
    const SpeechGlobalCtx* g = Global(gctx);
    const int64_t k = ((int64_t)key << 16) | (int64_t)gctx;
    auto it = m_next.find(k);
    if (it != m_next.end() && m_time < it->second) return false;
    if (m_audio->SoundCount(bank) <= last) return false;
    int pick = first;
    for (int tries = 0; tries < 4; tries++) {
        m_rng = m_rng * 1664525u + 1013904223u;
        pick = first + (int)((m_rng >> 8) % (unsigned)(last - first + 1));
        if (pick != m_last[bank] || last == first) break;
    }
    m_last[bank] = pick;
    m_next[k] = m_time + (g ? g->repeatMs : 7000) / 1000.0;
    m_audio->Play(bank, pick, vol, pan);
    return true;
}

bool Speech::SayPlayer(int gctx, float vol, int voice) {
    const SpeechGlobalCtx* g = Global(gctx);
    if (!g || voice < 0 || voice >= 20) return false;
    const int ctx = g->ctx[2];
    if (ctx < 0 || ctx >= 111) return false;
    return Play(-1, gctx, kSpeechPlyBank[voice], kSpeechPly[ctx][voice][0], kSpeechPly[ctx][voice][1], vol, 0.0f);
}

bool Speech::SayGen(int speaker, int voice, int gctx, float vol, float pan) {
    const SpeechGlobalCtx* g = Global(gctx);
    if (!g || voice < 0 || voice >= 209) return false;
    const int ctx = g->ctx[0];
    if (ctx < 0 || ctx >= 103) return false;
    return Play(speaker, gctx, kSpeechGenBank[voice], kSpeechGen[ctx][voice][0], kSpeechGen[ctx][voice][1], vol, pan);
}
