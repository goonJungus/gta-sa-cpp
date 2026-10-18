#include "Radio.h"

#include <cstdio>
#include <cstring>
#include <fstream>

#include "Audio.h"

namespace {

// CAEStreamTransformer table: the scramble key repeats every 16 bytes of the file.
const uint8_t kKey[16] = {0xEA, 0x3A, 0xC4, 0xA1, 0x9A, 0xA8, 0x14, 0xF3, 0x48, 0xB0, 0xD7, 0x23, 0x9D, 0xE8, 0xFF, 0xF1};
constexpr uint32_t kTrackInfoSize = 0x1F84;   // beat table in front of every track's Ogg data

struct StationDef { const char* pack; const char* name; };
const StationDef kStations[] = {
    {"CH", "Playback FM"}, {"CO", "K-Rose"}, {"CR", "K-DST"}, {"DS", "Bounce FM"}, {"HC", "SF-UR"},
    {"MH", "Radio Los Santos"}, {"MR", "Radio X"}, {"NJ", "CSR 103.9"}, {"RE", "K-JAH West"},
    {"RG", "Master Sounds 98.3"}, {"TK", "WCTR"},
};

}  // namespace

bool Radio::Init(const std::string& gameDir, Audio* audio) {
    m_dir = gameDir;
    m_audio = audio;
    std::ifstream sp(gameDir + "/audio/CONFIG/StrmPaks.dat", std::ios::binary);
    std::ifstream tl(gameDir + "/audio/CONFIG/TrakLkup.dat", std::ios::binary);
    if (!sp || !tl) return false;
    for (char name[16]; sp.read(name, 16);) {
        name[15] = 0;
        m_packNames.push_back(name);
    }
    for (uint8_t e[12]; tl.read(reinterpret_cast<char*>(e), 12);) {
        Track t;
        t.pack = e[0];
        std::memcpy(&t.offset, e + 4, 4);
        std::memcpy(&t.size, e + 8, 4);
        m_lookup.push_back(t);
    }
    for (const StationDef& d : kStations) {
        StationInfo s;
        s.pack = d.pack; s.name = d.name;
        for (size_t p = 0; p < m_packNames.size(); p++) {
            if (m_packNames[p] != d.pack) continue;
            for (size_t t = 0; t < m_lookup.size(); t++)
                if (m_lookup[t].pack == p && m_lookup[t].size > kTrackInfoSize) s.tracks.push_back((uint32_t)t);
        }
        if (!s.tracks.empty()) m_stations.push_back(std::move(s));
    }
    m_ready = !m_stations.empty();
    std::fprintf(stderr, "[RADIO] %zu stations, %zu tracks\n", m_stations.size(), m_lookup.size());
    return m_ready;
}

const char* Radio::StationName(int s) const {
    return s >= 0 && s < (int)m_stations.size() ? m_stations[(size_t)s].name.c_str() : "Radio Off";
}

void Radio::SetVolume(float v) {
    m_vol = v;
    if (m_audio) m_audio->SetMusicVolume(m_station >= 0 ? v : 0.0f);
}

size_t Radio::Pull(int16_t* out, size_t frames) {
    std::lock_guard<std::mutex> g(m_mx);
    if (!m_dec) return 0;
    const int ch = m_dec->Channels();
    if (ch == 2) return m_dec->Read(out, frames);
    // Mono tracks are doubled to stereo.
    std::vector<int16_t> mono(frames);
    const size_t n = m_dec->Read(mono.data(), frames);
    for (size_t i = 0; i < n; i++) out[i * 2] = out[i * 2 + 1] = mono[i];
    return n;
}

bool Radio::StartTrack() {
    if (m_station < 0 || m_station >= (int)m_stations.size()) return false;
    const StationInfo& st = m_stations[(size_t)m_station];
    // A random track that is not the one that just played.
    uint32_t pick = st.tracks[0];
    for (int tries = 0; tries < 4; tries++) {
        m_rng = m_rng * 1664525u + 1013904223u;
        pick = st.tracks[(m_rng >> 8) % st.tracks.size()];
        if ((int)pick != m_lastTrack || st.tracks.size() == 1) break;
    }
    m_lastTrack = (int)pick;
    const Track& t = m_lookup[pick];
    std::ifstream f(m_dir + "/audio/streams/" + m_packNames[t.pack], std::ios::binary);
    if (!f) return false;
    const uint32_t start = t.offset + kTrackInfoSize;
    std::vector<uint8_t> data(t.size - kTrackInfoSize);
    f.seekg(start);
    if (!f.read(reinterpret_cast<char*>(data.data()), (std::streamsize)data.size())) return false;
    for (size_t i = 0; i < data.size(); i++) data[i] ^= kKey[(start + i) & 15];
    auto dec = std::make_unique<VorbisDecoder>();
    {
        std::lock_guard<std::mutex> g(m_mx);
        m_ogg = std::move(data);
        if (!dec->Open(m_ogg.data(), m_ogg.size())) { m_dec.reset(); return false; }
        m_dec = std::move(dec);
    }
    const int rate = m_dec->SampleRate();
    m_audio->SetMusic([this](int16_t* out, size_t n) { return Pull(out, n); }, rate, m_vol);
    std::fprintf(stderr, "[RADIO] %s track %u, %d Hz\n", st.name.c_str(), pick, rate);
    return true;
}

void Radio::Tune(int station) {
    if (!m_ready || !m_audio) return;
    if (station >= (int)m_stations.size()) station = -1;
    m_station = station;
    if (station < 0) {
        m_audio->StopMusic();
        std::lock_guard<std::mutex> g(m_mx);
        m_dec.reset();
        return;
    }
    StartTrack();
}

void Radio::Step(int dir) {
    int s = m_station + dir;
    const int n = (int)m_stations.size();
    if (s < -1) s = n - 1;
    if (s >= n) s = -1;
    Tune(s);
}

void Radio::Update() {
    if (m_ready && m_station >= 0 && m_audio && m_audio->MusicEnded()) StartTrack();
}
