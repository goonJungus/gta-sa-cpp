#pragma once
// Speech: the retail ped speech lookup (CAEPedSpeechAudioEntity). A global context (pull a gun,
// run from a gun, bump, ...) maps per audio ped type to a specific context, and that with the
// ped's voice to a range of sounds in the voice's speech bank. A context cannot repeat within its
// repeat time; one random line from the range plays, not the one heard last.

#include <cstdint>
#include <string>
#include <unordered_map>

class Audio;

enum SpeechContext : int {   // eGlobalSpeechContext values used here
    CTX_BUMP = 28, CTX_GUN_RUN = 115, CTX_PULL_GUN = 176, CTX_SHOCKED = 181,
};

class Speech {
public:
    void Init(Audio* audio) { m_audio = audio; }
    // A generic ped voice by its peds.ide name (VOICE_GEN_...), -1 if unknown.
    static int GenVoice(const std::string& name);
    // CJ (player voice VOICE_PLY_CR unless told otherwise). Returns true if a line started.
    bool SayPlayer(int gctx, float vol = 1.0f, int voice = 10);
    // A generic ped; `speaker` keys the repeat timer (the actor index).
    bool SayGen(int speaker, int voice, int gctx, float vol, float pan);
    void Tick(float dt) { m_time += dt; }

private:
    bool Play(int key, int gctx, int bank, int first, int last, float vol, float pan);
    Audio* m_audio = nullptr;
    double m_time = 0.0;
    std::unordered_map<int64_t, double> m_next;   // (speaker, context) -> time it may speak again
    std::unordered_map<int, int> m_last;          // bank -> last sound
    unsigned m_rng = 777;
};
