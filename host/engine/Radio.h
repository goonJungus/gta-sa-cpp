#pragma once
// Radio: the retail radio stations. Each station is a stream pack in audio/streams holding many
// Ogg Vorbis tracks, XOR scrambled on disk (CAEStreamTransformer); audio/CONFIG/StrmPaks.dat names the
// packs and TrakLkup.dat gives every track's pack, offset and size (CAETrackLoader).
// Tracks are decoded with VorbisDecoder and fed to the mixer as a music stream.

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "VorbisDecoder.h"

class Audio;

class Radio {
public:
    bool Init(const std::string& gameDir, Audio* audio);
    bool Ready() const { return m_ready; }
    int StationCount() const { return (int)m_stations.size(); }
    // Display name of a station, "Radio Off" for -1.
    const char* StationName(int s) const;
    int Station() const { return m_station; }
    // -1 turns the radio off.
    void Tune(int station);
    void Step(int dir);          // next / previous station, wrapping through "off"
    // Cutscene audio (CAECutsceneTrackManager): plays TrakLkup track `trak` over the radio until stopped.
    bool PlayCutsceneTrack(uint32_t trak);
    void StopCutsceneTrack();
    bool CutsceneTrackActive() const { return m_cutTrack; }
    void Update();               // call every frame: starts the next track when one ends
    void SetVolume(float v);
    void SetClockHour(int h) { m_hour = h; }   // the DJ talks about the time of day
    // Decoded stereo frames of the current track (the mixer thread pulls these; tests may too).
    size_t Pull(int16_t* out, size_t frames);

private:
    struct StationInfo { std::string pack, name; int id = 0; };   // id: eRadioID
    bool StartTrack();
    bool PlayLookup(uint32_t trak);
    // CAERadioTrackManager::ChooseTracksForStation: the next items (ident, advert, DJ banter, intro,
    // music, outro) as TrakLkup indices.
    void ChooseTracks(int id);
    bool Queue(int id, int type);
    int Rnd(int lo, int hi);
    bool Chance(float p);
    int ChooseIdent(int id);
    int ChooseAdvert(int id);
    int ChooseMusic(int id);
    int ChooseBanter(int id);
    int BanterFromList(int id, const int (*list)[2]);
    std::vector<uint32_t> m_queue;
    int m_hour = 12;
    int m_musicHist[13][20], m_identHist[13][8], m_advertHist[13][40], m_banterHist[13][15];

    bool m_ready = false;
    std::string m_dir;
    Audio* m_audio = nullptr;
    std::vector<StationInfo> m_stations;
    struct Track { uint32_t pack, offset, size; };
    std::vector<Track> m_lookup;
    std::vector<std::string> m_packNames;
    int m_station = -1;
    float m_vol = 0.5f;
    unsigned m_rng = 4242;
    std::mutex m_mx;
    std::vector<uint8_t> m_ogg;
    std::unique_ptr<VorbisDecoder> m_dec;
    int m_lastTrack = -1;
    bool m_cutTrack = false;
};
