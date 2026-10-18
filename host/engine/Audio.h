#pragma once
// Audio - sound effects from the retail SFX paks (port of the CAEBankLoader
// bank format) played through a small software mixer.
//
// BankLkup.dat locates each bank inside a pak file, a bank is a header of up
// to 400 sound entries followed by 16-bit mono PCM. The mixer runs on its own
// thread and feeds waveOut with 44.1 kHz stereo; each voice has volume, pan,
// pitch and an optional loop. When no audio device exists the mixer stays
// silent and everything else keeps working.

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// Bank ids from the retail bank table.
enum SfxBank {
    BANK_FEET_GENERIC = 0, BANK_FEET_GRASS = 1, BANK_FEET_GRAVEL = 2, BANK_FEET_METAL = 3,
    BANK_FEET_SAND = 4, BANK_FEET_TILE = 5, BANK_FEET_WOOD = 6,
    BANK_BULLET_HITS = 27, BANK_COLLISIONS = 39,
    BANK_SWIMMING = 128,
    BANK_FRONTEND_GAME = 59, BANK_FRONTEND_MENU = 60,
    BANK_WEAPONS = 143,
    BANK_PAIN_CARL = 144, BANK_PAIN_FEMALE = 145, BANK_PAIN_MALE = 146,
};

class Audio {
public:
    ~Audio() { Shutdown(); }
    bool Init(const std::string& gameDir);
    void Shutdown();
    bool Ready() const { return m_ready; }

    void SetVolumes(float master, float sfx) { m_master = master; m_sfx = sfx; }

    // Plays one sound. vol 0..1, pan -1..1, pitch 1 = normal. Returns a voice id
    // (0 = nothing played) that Stop / SetVoice accept for loops.
    int Play(int bank, int sound, float vol = 1.0f, float pan = 0.0f, float pitch = 1.0f, bool loop = false);
    void Stop(int voice);
    void SetVoice(int voice, float vol, float pan, float pitch);
    int SoundCount(int bank);

    // Gun cues by weapon.dat name: 0 shot, 1 dry fire, 2 reload start, 3 reload end.
    void Gun(const char* weapon, int kind, float vol = 1.0f);
    // Footstep on a surface bank (BANK_FEET_*), picks a random variation.
    void Step(int bank, float vol, float pan = 0.0f);
    // Menu cue from FeMenu::TakeUiEvent: 1 highlight, 2 select, 3 back.
    void Ui(int cue);
    void Pain(float vol = 1.0f);
    // CAEPedAudioEntity::HandleSwimSplash / HandleLandingEvent: a splash from the swimming bank,
    // and a landing thud from the generic feet bank (sound 6 after a fall, 0 otherwise).
    void Splash(float vol = 1.0f);
    void Land(bool afterFall, float vol = 1.0f);
    // A bullet landing dist units away (hit sound, quieter with distance).
    void Impact(float dist);
    // Weapon change cue (frontend weapon sound).
    void WeaponSwitch();
    // Background beds, call every frame: city murmur, rain and wind levels 0..1.
    void Ambient(float city, float rain, float wind);

    // A streamed music source (the radio): `pull` fills interleaved stereo frames at `rate` Hz and
    // returns how many it wrote, 0 when the track is over. It runs on the mixer thread.
    void SetMusic(std::function<size_t(int16_t*, size_t)> pull, int rate, float vol);
    void StopMusic();
    void SetMusicVolume(float vol) { m_musVol = vol; }
    bool MusicEnded() const { return m_musEnded; }

    // Counters for tests: sounds started so far.
    int Played() const { return m_played; }
    const std::string& LastPlayed() const { return m_last; }

private:
    struct Sound { std::vector<int16_t> pcm; int freq = 22050; int loopStart = 0; float gain = 1.0f; };
    struct Bank { bool tried = false; std::vector<Sound> sounds; };
    struct Voice {
        int id = 0; const Sound* snd = nullptr;
        double pos = 0; float vol = 1, pan = 0, pitch = 1; bool loop = false;
    };
    struct BankLoc { uint8_t pak; uint32_t sector; uint32_t bytes; };   // sector holds a byte offset

    Bank& GetBank(int id);
    void MixerLoop();
    void Mix(int16_t* out, int frames);

    bool m_ready = false;
    std::string m_dir;
    std::vector<BankLoc> m_locs;
    std::vector<std::string> m_paks;
    std::map<int, Bank> m_banks;
    std::mutex m_lock;
    std::vector<Voice> m_voices;
    int m_nextVoice = 1;
    float m_master = 0.8f, m_sfx = 1.0f;
    int m_played = 0;
    unsigned m_rng = 12345;
    std::function<size_t(int16_t*, size_t)> m_musPull;
    int m_musRate = 32000;
    float m_musVol = 0.0f;
    bool m_musEnded = false;
    std::vector<int16_t> m_musBuf;   // decoded stereo frames not yet played
    double m_musPos = 0.0;
    int m_ambCity = 0, m_ambRain = 0, m_ambWindL = 0, m_ambWindR = 0;
    int Rand(int n) { m_rng = m_rng * 1664525u + 1013904223u; return (int)((m_rng >> 8) % (unsigned)n); }
    std::string m_last;

    struct Impl;
    Impl* m_impl = nullptr;
};
