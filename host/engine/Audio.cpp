#include "Audio.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {
constexpr int kRate = 44100;
constexpr int kChunk = 1024;          // frames per waveOut buffer
constexpr int kBuffers = 4;
constexpr int kHeaderBytes = 0x12C4;  // i16 count, pad, 400 x 12-byte entries
constexpr int kMaxSounds = 400;
constexpr int kMaxVoices = 48;

std::string Join(const std::string& a, const std::string& b) { return a + "/" + b; }

// Finds a file ignoring case, the retail names are upper case and the host
// may be a case-sensitive file system.
std::string FindFile(const std::string& dir, const std::string& name) {
    std::string path = Join(dir, name);
    if (FILE* f = fopen(path.c_str(), "rb")) { fclose(f); return path; }
    std::string lower = name, upper = name;
    for (auto& c : lower) c = (char)tolower((unsigned char)c);
    for (auto& c : upper) c = (char)toupper((unsigned char)c);
    for (const auto& n : { lower, upper }) {
        path = Join(dir, n);
        if (FILE* f = fopen(path.c_str(), "rb")) { fclose(f); return path; }
    }
    return std::string();
}

std::string FindPath(const std::string& root, std::initializer_list<const char*> parts) {
    std::string cur = root;
    for (const char* p : parts) {
        std::string next = FindFile(cur, p);
        if (next.empty()) {
            // directories do not open with fopen; try the plain joins
            std::string lower = p, upper = p;
            for (auto& c : lower) c = (char)tolower((unsigned char)c);
            for (auto& c : upper) c = (char)toupper((unsigned char)c);
            next = Join(cur, p);
            DWORD a = GetFileAttributesA(next.c_str());
            if (a == INVALID_FILE_ATTRIBUTES) {
                next = Join(cur, lower);
                a = GetFileAttributesA(next.c_str());
                if (a == INVALID_FILE_ATTRIBUTES) {
                    next = Join(cur, upper);
                    a = GetFileAttributesA(next.c_str());
                    if (a == INVALID_FILE_ATTRIBUTES) return std::string();
                }
            }
        }
        cur = next;
    }
    return cur;
}
} // namespace

struct Audio::Impl {
    HWAVEOUT dev = nullptr;
    WAVEHDR hdr[kBuffers] = {};
    std::vector<int16_t> buf[kBuffers];
    std::thread thread;
    std::atomic<bool> run{ false };
    bool open = false;
};

bool Audio::Init(const std::string& gameDir) {
    m_dir = gameDir;
    std::string cfg = FindPath(gameDir, { "audio", "CONFIG" });
    if (cfg.empty()) return false;

    std::string lk = FindFile(cfg, "BankLkup.dat");
    std::string pk = FindFile(cfg, "PakFiles.dat");
    if (lk.empty() || pk.empty()) return false;

    if (FILE* f = fopen(lk.c_str(), "rb")) {
        uint8_t rec[12];
        while (fread(rec, 1, 12, f) == 12) {
            BankLoc l;
            l.pak = rec[0];
            memcpy(&l.sector, rec + 4, 4);
            memcpy(&l.bytes, rec + 8, 4);
            m_locs.push_back(l);
        }
        fclose(f);
    }
    if (FILE* f = fopen(pk.c_str(), "rb")) {
        char rec[0x34];
        while (fread(rec, 1, 0x34, f) == 0x34) {
            char name[13] = {};
            memcpy(name, rec, 12);
            m_paks.emplace_back(name);
        }
        fclose(f);
    }
    if (m_locs.empty() || m_paks.empty()) return false;

    m_impl = new Impl();
    WAVEFORMATEX fmt = {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = kRate;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 4;
    fmt.nAvgBytesPerSec = kRate * 4;
    if (waveOutOpen(&m_impl->dev, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
        m_impl->open = true;
        for (int i = 0; i < kBuffers; i++) {
            m_impl->buf[i].assign(kChunk * 2, 0);
            WAVEHDR& h = m_impl->hdr[i];
            h.lpData = (LPSTR)m_impl->buf[i].data();
            h.dwBufferLength = kChunk * 4;
            waveOutPrepareHeader(m_impl->dev, &h, sizeof(h));
        }
        m_impl->run = true;
        m_impl->thread = std::thread([this] { MixerLoop(); });
    }
    m_ready = true;
    return true;
}

void Audio::Shutdown() {
    if (!m_impl) return;
    m_impl->run = false;
    if (m_impl->thread.joinable()) m_impl->thread.join();
    if (m_impl->open) {
        waveOutReset(m_impl->dev);
        for (int i = 0; i < kBuffers; i++) waveOutUnprepareHeader(m_impl->dev, &m_impl->hdr[i], sizeof(WAVEHDR));
        waveOutClose(m_impl->dev);
    }
    delete m_impl;
    m_impl = nullptr;
    m_ready = false;
}

Audio::Bank& Audio::GetBank(int id) {
    Bank& b = m_banks[id];
    if (b.tried) return b;
    b.tried = true;
    if (id < 0 || id >= (int)m_locs.size()) return b;
    const BankLoc& l = m_locs[id];
    if (l.pak >= m_paks.size() || l.bytes <= kHeaderBytes) return b;
    std::string pak = FindPath(m_dir, { "audio", "sfx", m_paks[l.pak].c_str() });
    if (pak.empty()) return b;
    FILE* f = fopen(pak.c_str(), "rb");
    if (!f) return b;
    std::vector<uint8_t> raw(l.bytes);
    fseek(f, (long)l.sector, SEEK_SET);   // byte offset into the pak
    size_t got = fread(raw.data(), 1, l.bytes, f);
    fclose(f);
    if (got < kHeaderBytes) return b;
    int16_t count = 0;
    memcpy(&count, raw.data(), 2);
    if (count <= 0 || count > kMaxSounds) return b;
    struct Entry { uint32_t off, loop; uint16_t freq; int16_t head; };
    std::vector<Entry> e(count);
    for (int i = 0; i < count; i++) memcpy(&e[i], raw.data() + 4 + i * 12, 12);
    for (int i = 0; i < count; i++) {
        uint32_t end = (i + 1 < count) ? e[i + 1].off : (uint32_t)got;
        Sound s;
        if (e[i].off < got && end > e[i].off && end <= got) {
            size_t n = (end - e[i].off) / 2;
            s.pcm.resize(n);
            memcpy(s.pcm.data(), raw.data() + e[i].off, n * 2);
        }
        s.freq = e[i].freq ? e[i].freq : 22050;
        s.loopStart = e[i].loop > 0 && e[i].loop != 0xFFFFFFFFu ? (int)(e[i].loop / 2) : 0;
        // headroom is in hundredths of a dB
        s.gain = std::pow(10.0f, e[i].head / 2000.0f);
        if (s.gain > 1.0f) s.gain = 1.0f;
        b.sounds.push_back(std::move(s));
    }
    return b;
}

int Audio::SoundCount(int bank) {
    std::lock_guard<std::mutex> g(m_lock);
    return (int)GetBank(bank).sounds.size();
}

int Audio::Play(int bank, int sound, float vol, float pan, float pitch, bool loop) {
    if (!m_ready) return 0;
    std::lock_guard<std::mutex> g(m_lock);
    Bank& b = GetBank(bank);
    if (sound < 0 || sound >= (int)b.sounds.size() || b.sounds[sound].pcm.empty()) return 0;
    m_played++;
    char tag[48];
    snprintf(tag, sizeof(tag), "bank %d sound %d", bank, sound);
    m_last = tag;
    if ((int)m_voices.size() >= kMaxVoices) {
        // Drop the oldest one-shot to make room.
        for (size_t i = 0; i < m_voices.size(); i++)
            if (!m_voices[i].loop) { m_voices.erase(m_voices.begin() + i); break; }
        if ((int)m_voices.size() >= kMaxVoices) return 0;
    }
    Voice v;
    v.id = m_nextVoice++;
    v.snd = &b.sounds[sound];
    v.vol = vol; v.pan = pan; v.pitch = pitch; v.loop = loop;
    m_voices.push_back(v);
    return v.id;
}

void Audio::Stop(int voice) {
    if (!voice) return;
    std::lock_guard<std::mutex> g(m_lock);
    for (size_t i = 0; i < m_voices.size(); i++)
        if (m_voices[i].id == voice) { m_voices.erase(m_voices.begin() + i); return; }
}

void Audio::SetVoice(int voice, float vol, float pan, float pitch) {
    if (!voice) return;
    std::lock_guard<std::mutex> g(m_lock);
    for (auto& v : m_voices)
        if (v.id == voice) { v.vol = vol; v.pan = pan; v.pitch = pitch; return; }
}

void Audio::Mix(int16_t* out, int frames) {
    std::vector<float> acc(frames * 2, 0.0f);
    float master = m_master * m_sfx;
    {
        std::lock_guard<std::mutex> g(m_lock);
        for (auto& v : m_voices) {
            const Sound& s = *v.snd;
            double step = (double)s.freq / kRate * v.pitch;
            float pan = std::clamp(v.pan, -1.0f, 1.0f);
            // Equal power pan.
            float ang = (pan + 1.0f) * 0.78539816f;
            float gl = std::cos(ang) * 1.41421356f * 0.5f * 1.2f;
            float gr = std::sin(ang) * 1.41421356f * 0.5f * 1.2f;
            float g0 = v.vol * s.gain * master / 32768.0f;
            int n = (int)s.pcm.size();
            for (int i = 0; i < frames; i++) {
                int p = (int)v.pos;
                if (p >= n) {
                    if (!v.loop) break;
                    v.pos = s.loopStart + (v.pos - n);
                    p = (int)v.pos;
                    if (p >= n) p = 0;
                }
                float f = (float)(v.pos - p);
                float a = s.pcm[p];
                float c = p + 1 < n ? s.pcm[p + 1] : (v.loop ? s.pcm[s.loopStart] : 0.0f);
                float smp = (a + (c - a) * f) * g0;
                acc[i * 2] += smp * gl;
                acc[i * 2 + 1] += smp * gr;
                v.pos += step;
            }
        }
        m_voices.erase(std::remove_if(m_voices.begin(), m_voices.end(), [](const Voice& v) {
            return !v.loop && (int)v.pos >= (int)v.snd->pcm.size();
        }), m_voices.end());
    }
    for (int i = 0; i < frames * 2; i++) {
        // Soft clip so stacked gunfire does not crackle.
        float x = acc[i];
        x = x / (1.0f + std::fabs(x) * 0.5f) * 1.5f;
        x = std::clamp(x, -1.0f, 1.0f);
        out[i] = (int16_t)(x * 32767.0f);
    }
}

void Audio::MixerLoop() {
    Impl& im = *m_impl;
    for (int i = 0; i < kBuffers; i++) {
        Mix(im.buf[i].data(), kChunk);
        waveOutWrite(im.dev, &im.hdr[i], sizeof(WAVEHDR));
    }
    int next = 0;
    while (im.run) {
        WAVEHDR& h = im.hdr[next];
        if (h.dwFlags & WHDR_DONE) {
            Mix(im.buf[next].data(), kChunk);
            h.dwFlags &= ~WHDR_DONE;
            waveOutWrite(im.dev, &h, sizeof(WAVEHDR));
            next = (next + 1) % kBuffers;
        } else {
            Sleep(2);
        }
    }
}

// ---- Cues ----------------------------------------------------------------
// Sound ids are the retail GENRL_WEAPONS entries (see AEWeaponAudioEntity).
namespace {
enum {
    W_9MM_L = 0, W_9MM_R = 1, W_9MM_TAIL = 2, W_AK_L = 3, W_AK_R = 4, W_AK_TAIL = 5,
    W_EAGLE_L = 6, W_EAGLE_R = 7, W_EAGLE_TAIL = 8, W_MP5_L = 17, W_MP5_R = 18,
    W_SG_L = 21, W_SG_R = 22, W_SG_TAIL = 23, W_SIL_L = 24, W_SIL_R = 25,
    W_SNIPER_L = 26, W_SNIPER_R = 27, W_9MM_DRY = 29, W_9MM_LOWS = 30, W_AK_A = 31, W_AK_B = 32,
    W_AK_DRY = 33, W_EAGLE_B = 51, W_EAGLE_DRY = 52, W_EAGLE_LOWS = 53, W_HANDGUN_A = 55,
    W_PISTOL_B = 66, W_SAWNOFF_A = 69, W_SAWNOFF_B = 70, W_SG_A = 71, W_SG_B = 72, W_SG_DRY = 73,
    W_SG_LOWS = 74, W_SIL_DRY = 76, W_SIL_LOWS = 77, W_UZI_A = 84, W_UZI_B = 85,
};
struct GunSfx { const char* name; int l, r, tail, lows, dry, relA, relB; float pitch, vol; };
const GunSfx kGunSfx[] = {
    {"PISTOL",          W_EAGLE_L,  W_EAGLE_R,  W_EAGLE_TAIL, W_EAGLE_LOWS, W_EAGLE_DRY, W_HANDGUN_A, W_PISTOL_B, 1.41421f, 1.0f},
    {"PISTOL_SILENCED", W_SIL_L,    W_SIL_L,    -1,           W_SIL_LOWS,   W_SIL_DRY,   W_HANDGUN_A, W_PISTOL_B, 1.0f, 0.5f},
    {"DESERT_EAGLE",    W_EAGLE_L,  W_EAGLE_R,  W_EAGLE_TAIL, W_EAGLE_LOWS, W_EAGLE_DRY, W_EAGLE_B,   W_PISTOL_B, 0.94387f, 1.0f},
    {"SHOTGUN",         W_SG_L,     W_SG_R,     W_SG_TAIL,    W_SG_LOWS,    W_SG_DRY,    W_SG_A,      W_SG_B,     1.0f, 1.0f},
    {"SAWNOFF",         W_SG_L,     W_SG_R,     W_SG_TAIL,    W_SG_LOWS,    W_SG_DRY,    W_SAWNOFF_A, W_SAWNOFF_B, 0.7937f, 1.0f},
    {"SPAS12",          W_SG_L,     W_SG_R,     W_SG_TAIL,    W_SG_LOWS,    W_SG_DRY,    W_SG_A,      W_SG_B,     1.0f, 1.0f},
    {"MICRO_UZI",       W_9MM_L,    W_9MM_R,    W_9MM_TAIL,   W_9MM_LOWS,   W_9MM_DRY,   W_UZI_A,     W_UZI_B,    1.0f, 1.0f},
    {"TEC9",            W_9MM_L,    W_9MM_R,    W_9MM_TAIL,   W_9MM_LOWS,   W_9MM_DRY,   W_UZI_A,     W_UZI_B,    1.25992f, 1.0f},
    {"MP5",             W_MP5_L,    W_MP5_R,    W_9MM_TAIL,   W_9MM_LOWS,   W_9MM_DRY,   W_UZI_A,     W_UZI_B,    1.0f, 1.0f},
    {"AK47",            W_AK_L,     W_AK_R,     W_AK_TAIL,    W_EAGLE_LOWS, W_AK_DRY,    W_AK_A,      W_AK_B,     1.0f, 1.0f},
    {"M4",              W_AK_L,     W_AK_R,     W_AK_TAIL,    W_EAGLE_LOWS, W_AK_DRY,    W_AK_A,      W_AK_B,     1.0f, 1.0f},
    {"COUNTRYRIFLE",    W_SNIPER_L, W_SNIPER_R, W_SG_TAIL,    W_EAGLE_LOWS, W_EAGLE_DRY, W_AK_A,      W_AK_B,     0.89f, 1.0f},
    {"SNIPERRIFLE",     W_SNIPER_L, W_SNIPER_R, W_SG_TAIL,    W_EAGLE_LOWS, W_EAGLE_DRY, W_AK_A,      W_AK_B,     1.0f, 1.0f},
};
} // namespace

void Audio::Gun(const char* weapon, int kind, float vol) {
    if (!m_ready || !weapon) return;
    const GunSfx* g = nullptr;
    for (const auto& s : kGunSfx) if (_stricmp(s.name, weapon) == 0) g = &s;
    if (!g) return;
    switch (kind) {
    case 0:
        Play(BANK_WEAPONS, g->l, vol * g->vol, -0.2f, g->pitch);
        Play(BANK_WEAPONS, g->r, vol * g->vol, 0.2f, g->pitch);
        if (g->lows >= 0) Play(BANK_WEAPONS, g->lows, vol * 0.7f * g->vol);
        if (g->tail >= 0) Play(BANK_WEAPONS, g->tail, vol * 0.6f * g->vol, 0.0f, g->pitch > 1.2f ? 1.0f : 0.95f);
        break;
    case 1: Play(BANK_WEAPONS, g->dry, vol); break;
    case 2: Play(BANK_WEAPONS, g->relA, vol * 0.9f); break;
    case 3: Play(BANK_WEAPONS, g->relB, vol * 0.9f); break;
    }
}

void Audio::Step(int bank, float vol, float pan) {
    if (!m_ready) return;
    int n = SoundCount(bank);
    if (n <= 0) { bank = BANK_FEET_GENERIC; n = SoundCount(bank); }
    if (n <= 0) return;
    Play(bank, Rand(n), vol, pan, 0.92f + Rand(17) * 0.01f);
}

void Audio::Ui(int cue) {
    // FRONTEND_MENU: back 0/1, error 2/3, highlight 4/5, select 6/7 (L / R).
    switch (cue) {
    case 1: Play(BANK_FRONTEND_MENU, 4, 0.7f, -0.3f); Play(BANK_FRONTEND_MENU, 5, 0.7f, 0.3f); break;
    case 2: Play(BANK_FRONTEND_MENU, 6, 0.9f, -0.3f); Play(BANK_FRONTEND_MENU, 7, 0.9f, 0.3f); break;
    case 3: Play(BANK_FRONTEND_MENU, 0, 0.9f, -0.3f); Play(BANK_FRONTEND_MENU, 1, 0.9f, 0.3f); break;
    }
}

void Audio::Pain(float vol) {
    if (!m_ready) return;
    int n = SoundCount(BANK_PAIN_CARL);
    if (n > 0) Play(BANK_PAIN_CARL, Rand(n), vol);
}

void Audio::Impact(float dist) {
    if (!m_ready) return;
    const int n = SoundCount(BANK_BULLET_HITS);
    if (n <= 0) return;
    const float vol = std::max(0.15f, 1.0f - dist / 90.0f);
    Play(BANK_BULLET_HITS, Rand(n), vol, 0.0f, 0.95f + 0.1f * (float)Rand(10) / 10.0f);
}

void Audio::WeaponSwitch() {
    // FRONTEND_GAME weapon (L 0x1B, R 0x1C)
    Play(BANK_FRONTEND_GAME, 0x1B, 0.8f, -0.3f);
    Play(BANK_FRONTEND_GAME, 0x1C, 0.8f, 0.3f);
}

void Audio::Ambient(float city, float rain, float wind) {
    if (!m_ready) return;
    // The loops are started once and then only their levels move.
    auto bed = [&](int& voice, int bank, int sound, float vol, float pan) {
        if (vol <= 0.002f) { if (voice) { Stop(voice); voice = 0; } return; }
        if (!voice) voice = Play(bank, sound, vol, pan, 1.0f, true);
        else SetVoice(voice, vol, pan, 1.0f);
    };
    bed(m_ambCity, BANK_FRONTEND_GAME, 0, city * 0.35f, 0.0f);       // CITY_NOISE_L
    bed(m_ambRain, 105, 0, rain * 0.6f, 0.0f);                       // GENRL_RAIN: RAINLONG
    bed(m_ambWindL, BANK_FRONTEND_GAME, 0x1D, wind * 0.7f, -0.4f);   // WIND_L
    bed(m_ambWindR, BANK_FRONTEND_GAME, 0x1E, wind * 0.7f, 0.4f);    // WIND_R
}
