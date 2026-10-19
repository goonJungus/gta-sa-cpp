#pragma once
// Peds: pedestrians (CPed / CPopulation). A ped model is a skinned clump of its own (peds.ide gives
// the model, texture and animation group, animgrp.dat the walk and idle clips). The first slice spawns
// civilians that stand or wander on the ground; no AI beyond walking, no reactions yet.

#include <cstdint>
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "D3DRenderer.h"
#include "IfpLoader.h"
#include "SkinnedDff.h"

struct PedInfo {
    int id = 0;
    std::string dff, txd, type, group;
    int voice = -1;                // peds.ide voice (VOICE_GEN_... index), -1 none
};

class Peds {
public:
    bool Init(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);
    bool Ready() const { return m_ready; }
    int ModelCount() const { return (int)m_pool.size(); }

    // Spawn a civilian (model id from the pool, -1 picks one at random). Returns the actor index or -1.
    int Spawn(D3DRenderer& r, int id, float x, float y, float z, float heading);
    float X(size_t i) const { return m_actors[i].x; }
    float Y(size_t i) const { return m_actors[i].y; }
    float Z(size_t i) const { return m_actors[i].z; }
    size_t Count() const { return m_actors.size(); }
    bool Alive(size_t i) const { return i < m_actors.size() && !m_actors[i].dead; }
    float HealthFrac(size_t i) const { return i < m_actors.size() ? std::max(0.0f, m_actors[i].health) / 100.0f : 0.0f; }
    // Lock-on aim point (CPlayerPed::ProcessControl: m_nTargetBone): the head (bone 5, +0.05) within the
    // weapon's head range, else the spine (bone 3, +0.2). Bind-pose heights above the feet.
    void TargetPoint(size_t i, bool head, float out[3]) const {
        const Actor& a = m_actors[i];
        out[0] = a.x; out[1] = a.y; out[2] = a.z + (head ? 1.62f : 1.15f);
    }
    void Clear() { m_actors.clear(); }
    // Advance animation and wandering. groundAt gives the ground height or a value below -1e5.
    // `player` is where CJ stands; `threat` is true while CJ aims or fires a weapon, which makes
    // nearby pedestrians run away. `blocked` covers walls and cars.
    struct Player { float x = 0, y = 0; bool threat = false; };
    void Update(float dt, const std::function<float(float, float, float)>& groundAt,
                const std::function<bool(float, float, float, float, float)>& blocked, const Player& player);
    void Draw(D3DRenderer& r);
    // Shots: ray against the standing pedestrians. Returns the actor index (or -1), distance in *t and
    // whether the head was hit. Damage returns true when the shot killed.
    int Trace(const float* eye, const float* dir, float reach, float* t, bool* head) const;
    bool Damage(int actor, float damage, bool head);
    // Speech requests since the last call: actor index and eGlobalSpeechContext.
    struct SpeechEvent { int actor; int ctx; };
    std::vector<SpeechEvent> TakeSpeech() { std::vector<SpeechEvent> e; e.swap(m_speech); return e; }
    int Voice(size_t i) const { return i < m_actors.size() ? m_actors[i].voice : -1; }
    // Keep about `target` civilians around (px, py): spawn on the ground in a ring in front of and around
    // the player, remove the ones that fell far behind. target 0 clears the crowd.
    void Populate(D3DRenderer& r, float px, float py, float pz, int target,
                  const std::function<float(float, float, float)>& groundAt);

private:
    struct Model {
        PedInfo info;
        SkinnedClump clump;
        std::vector<int> boneFrame;           // node -> skeleton frame
        std::vector<RwMat> skeletonLocal;     // per frame, bind pose (rest offsets fixed from the skin matrices)
        std::vector<int> frameOfBone;
        std::vector<D3DRenderMesh*> meshes;   // one per material, all sharing the skinned vertices
        std::vector<IDirect3DTexture9*> textures;
        std::vector<MeshVertex> skinned;
        float feetZ = 0.0f;                   // lowest bind pose vertex
        const AnimClip* walk = nullptr;
        const AnimClip* run = nullptr;
        const AnimClip* idle = nullptr;
        bool valid = false;
    };
    struct Actor {
        int model = 0;
        float x = 0, y = 0, z = 0, heading = 0;   // heading 0 faces +x
        float animTime = 0.0f, turnTimer = 0.0f, walkTimer = 0.0f;
        bool walking = false;
        float fleeTimer = 0.0f;           // running away from the player while above zero
        float health = 100.0f;
        bool dead = false;
        const AnimClip* deathClip = nullptr;
        float deadTime = 0.0f;            // seconds since death, the body is removed after a while
        bool crowd = false;               // spawned by Populate, removed again when far away
        int voice = -1;                   // speech voice
    };

    Model* GetModel(D3DRenderer& r, int id);
    void Pose(Model& m, const AnimClip* clip, float time);

    bool m_ready = false;
    std::string m_dir;
    std::unordered_map<int, PedInfo> m_info;
    std::vector<int> m_pool;
    std::unordered_map<int, Model> m_models;
    std::unordered_map<std::string, AnimClip> m_clips;
    std::unordered_map<std::string, std::vector<std::string>> m_groups;   // animation group -> clip names
    std::vector<Actor> m_actors;
    std::vector<SpeechEvent> m_speech;
    unsigned m_rng = 99;
    int Rand(int n) { m_rng = m_rng * 1664525u + 1013904223u; return (int)((m_rng >> 8) % (unsigned)n); }
};
