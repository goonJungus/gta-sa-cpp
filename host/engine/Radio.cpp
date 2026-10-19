#include "Radio.h"

#include <cstdio>
#include <algorithm>
#include <cstring>
#include <fstream>

#include "Audio.h"
#include "RadioTables.inc"

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
    for (size_t i = 0; i < sizeof(kStations) / sizeof(kStations[0]); i++) {   // eRadioID 1 .. 11
        StationInfo s;
        s.pack = kStations[i].pack; s.name = kStations[i].name; s.id = (int)i + 1;
        if (kRadioMusic[s.id][0] < (int)m_lookup.size()) m_stations.push_back(std::move(s));
    }
    std::fill(&m_musicHist[0][0], &m_musicHist[0][0] + 13 * 20, -1);
    std::fill(&m_identHist[0][0], &m_identHist[0][0] + 13 * 8, -1);
    std::fill(&m_advertHist[0][0], &m_advertHist[0][0] + 13 * 40, -1);
    std::fill(&m_banterHist[0][0], &m_banterHist[0][0] + 13 * 15, -1);
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

int Radio::Rnd(int lo, int hi) {   // CAEAudioUtility::GetRandomNumberInRange, inclusive
    m_rng = m_rng * 1664525u + 1013904223u;
    return hi <= lo ? lo : lo + (int)((m_rng >> 8) % (unsigned)(hi - lo + 1));
}

bool Radio::Chance(float p) {   // CAEAudioUtility::ResolveProbability
    m_rng = m_rng * 1664525u + 1013904223u;
    return (float)((m_rng >> 8) & 0xFFFF) / 65536.0f < p;
}

namespace {
template <size_t N> bool InHistory(const int (&h)[N], int v, int n) {
    for (int i = 0; i < n && i < (int)N; i++) if (h[i] == v) return true;
    return false;
}
template <size_t N> void Push(int (&h)[N], int v) {
    for (size_t i = N - 1; i > 0; i--) h[i] = h[i - 1];
    h[0] = v;
}
}  // namespace

// CAERadioTrackManager::ChooseIdentIndex @ 004e94c0: a station ident not among the last few.
int Radio::ChooseIdent(int id) {
    const int lo = kRadioIdent[id][0], hi = kRadioIdent[id][1];
    if (lo == 1922) return -1;
    for (int tries = 0; tries < 64; tries++) {
        const int v = Rnd(lo, hi);
        if (!InHistory(m_identHist[id], v, std::min(8, hi - lo - 1))) { Push(m_identHist[id], v); return v; }
    }
    return Rnd(lo, hi);
}

// ChooseAdvertIndex @ 004e9570: any advert this station plays that was not heard lately.
int Radio::ChooseAdvert(int id) {
    for (int tries = 0; tries < 256; tries++) {
        const int v = Rnd(kRadioAdvertMin, kRadioAdvertMax);
        bool bad = false;
        for (int e : kRadioAdvertExclude[id]) if (e == v) bad = true;
        if (bad || InHistory(m_advertHist[id], v, 40)) continue;
        Push(m_advertHist[id], v);
        return v;
    }
    return Rnd(kRadioAdvertMin, kRadioAdvertMax);
}

// ChooseMusicTrackIndex @ 004ea270: a song index not among the last 20 (count - 2 for short lists).
int Radio::ChooseMusic(int id) {
    const int count = kRadioMusicCount[id];
    for (int tries = 0; tries < 256; tries++) {
        const int v = Rnd(0, count - 1);
        if (!InHistory(m_musicHist[id], v, std::min(20, count - 2))) { Push(m_musicHist[id], v); return v; }
    }
    return Rnd(0, count - 1);
}

// ChooseDJBanterIndexFromList @ 004e95e0
int Radio::BanterFromList(int id, const int (*list)[2]) {
    const int lo = list[id][0], hi = list[id][1];
    if (lo == 1922) return -1;
    const int span = hi - lo + 1, start = Rnd(0, hi - lo);
    const int histN = std::min(15, kRadioBanterGeneral[id][1] - kRadioBanterGeneral[id][0] - 1);
    for (int k = 0; k < span; k++) {
        const int v = (k + start) % span + lo;
        if (!InHistory(m_banterHist[id], v, histN)) { Push(m_banterHist[id], v); return v; }
    }
    return -1;
}

// ChooseDJBanterIndex @ 004ea2d0: the station's first banter once, then time-of-day talk (30 %), else
// the general pool.
int Radio::ChooseBanter(int id) {
    int v = kRadioBanterFirst[id][0];
    if (v != 1922 && !InHistory(m_banterHist[id], v, 15)) { Push(m_banterHist[id], v); return v; }
    const int h = m_hour;
    const int (*list)[2] = nullptr;
    if (h >= 6 && h <= 8) { if (Chance(0.3f)) list = kRadioBanterMorning; }
    else if (h >= 18 && h <= 20) { if (Chance(0.3f)) list = kRadioBanterEvening; }
    else if ((h >= 22 || h <= 2) && Chance(0.3f)) list = kRadioBanterNight;
    if (list && (v = BanterFromList(id, list)) != -1) return v;
    return BanterFromList(id, kRadioBanterGeneral);
}

// QueueUpTracksForStation @ 004ea670. Types: 0 ident, 1 advert, 2 DJ banter, 3 intro + song + outro,
// 4 song + outro, 5 an outro alone.
bool Radio::Queue(int id, int type) {
    switch (type) {
    case 0: { const int v = ChooseIdent(id); if (v < 0) return false; m_queue.push_back((uint32_t)v); return true; }
    case 1: m_queue.push_back((uint32_t)ChooseAdvert(id)); return true;
    case 2: { const int v = ChooseBanter(id); if (v < 0) return false; m_queue.push_back((uint32_t)v); return true; }
    case 3: case 4: case 5: {
        const int m = ChooseMusic(id);
        if (type == 3) m_queue.push_back((uint32_t)Rnd(kRadioIntro[id][m][0], kRadioIntro[id][m][1]));
        if (type != 5) m_queue.push_back((uint32_t)kRadioMusic[id][m]);
        m_queue.push_back((uint32_t)Rnd(kRadioOutro[id][m][0], kRadioOutro[id][m][1]));
        return true;
    }
    }
    return true;
}

// ChooseTracksForStation @ 004eb180
void Radio::ChooseTracks(int id) {
    if (!Chance(0.95f)) {
        if (Chance(0.5f)) Queue(id, 0);
        if (!Queue(id, 2)) Queue(id, 1);
        Queue(id, 3);
        return;
    }
    if (Chance(0.9f)) { Queue(id, 4); return; }
    if (Chance(0.5f)) {
        if (Chance(0.5f)) Queue(id, 0);
        Queue(id, 3);
        return;
    }
    Queue(id, 5);
    if (Chance(0.5f)) {
        if (Chance(0.5f)) Queue(id, 0);
        Queue(id, 3);
        return;
    }
    if (Chance(0.5f)) Queue(id, 0);
    if (!Queue(id, 2)) Queue(id, 1);
}

bool Radio::PlayLookup(uint32_t pick) {
    if (pick >= m_lookup.size()) return false;
    const Track& t = m_lookup[pick];
    if (t.size <= kTrackInfoSize || t.pack >= m_packNames.size()) return false;
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
    std::fprintf(stderr, "[RADIO] %s %u (%s), %d Hz\n", m_stations[(size_t)m_station].name.c_str(), pick,
                 m_packNames[t.pack].c_str(), rate);
    return true;
}

bool Radio::StartTrack() {
    if (m_station < 0 || m_station >= (int)m_stations.size()) return false;
    const int id = m_stations[(size_t)m_station].id;
    for (int guard = 0; guard < 8; guard++) {
        if (m_queue.empty()) ChooseTracks(id);
        if (m_queue.empty()) return false;
        const uint32_t pick = m_queue.front();
        m_queue.erase(m_queue.begin());
        if (PlayLookup(pick)) return true;
    }
    return false;
}

void Radio::Tune(int station) {
    if (!m_ready || !m_audio) return;
    if (station >= (int)m_stations.size()) station = -1;
    if (station != m_station) m_queue.clear();
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
