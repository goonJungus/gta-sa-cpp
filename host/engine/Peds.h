#pragma once
// Peds: pedestrians (CPed / CPopulation). A ped model is a skinned clump of its own (peds.ide gives
// the model, texture and animation group, animgrp.dat the walk and idle clips). The first slice spawns
// civilians that stand or wander on the ground; no AI beyond walking, no reactions yet.

#include <cstdint>
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <cmath>
#include <vector>

#include "D3DRenderer.h"
#include "IfpLoader.h"
#include "SkinnedDff.h"

struct PedInfo {
    int id = 0;
    std::string dff, txd, type, group;
    int voice = -1;                // peds.ide voice (VOICE_GEN_... index), -1 none
    std::string stat;              // peds.ide PedStat name (pedstats.dat row)
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
    // CPedDamageResponseCalculator for a bullet: damage * the ped stat's defend weakness; a gun forces
    // death on a head hit or 1 time in 8 (snipers: head hits only). Returns true when it killed.
    bool Shoot(int actor, float damage, bool head, bool sniper);
    // CTaskSimpleFight strikes (PlayerPed::Strike): the first live ped whose body the sphere reaches takes
    // damage * defend weakness; force death 1 in 8, a katana third attack 1 in 2. The ped turns to the
    // attacker and plays the group's hit reaction (hitClips = the group's three reaction clip names).
    // Returns the actor index hit or -1; *killed tells whether it died. forceBase is the weapon's CEventDamage
    // death force (fists 0.5, blunt 1.5, knife 0), mod4 the attacker's FatMuscleMod(4): a kill is thrown back by
    // forceBase * mod4 * 5 and a hit that leaves under 15 health knocks the ped down with (mod4 + 1) / 2 * 5.
    int MeleeStrike(const float centre[3], float radius, float damage, const float attackerPos[2], bool sword3,
                    const char* const hitClips[3], int move, bool* killed, float forceBase = 0.0f, float mod4 = 1.0f);
    // CTaskSimpleFight::FindTargetOnGround: a fallen (dead) ped within `range` in front of (x, y) facing `heading`.
    void SetSeed(unsigned s) { m_rng = s; }   // dev
    void SetHealth(size_t i, float hp) { m_actors[i].health = hp; }   // dev
    float VelX(size_t i) const { return std::hypot(m_actors[i].vx, m_actors[i].vy); }   // horizontal speed
    float VelZ(size_t i) const { return m_actors[i].vz; }
    const char* ClipName(size_t i) const { const Actor& a = m_actors[i]; const AnimClip* c = a.dead ? a.deathClip : a.hitClip; return c ? c->name.c_str() : "-"; }
    bool Flying(size_t i) const { return m_actors[i].flying; }
    bool KnockedDown(size_t i) const { return m_actors[i].getUp; }
    bool FallenNear(float x, float y, float heading, float range) const;
    // CTaskSimpleUseGun::RequirePistolWhip: a live ped within a metre in front, inside a 60 degree cone.
    bool WhipTarget(float x, float y, float heading) const;
    // CTaskSimpleStealthKill: the live, unaware ped within 1.4 m in front of (x, y) with its back to us, or -1.
    int StealthTarget(float x, float y, float heading) const;
    // The takedown: the victim plays KILL_Knife_Ped_Die and stays down. Returns true when it died.
    bool StealthKill(int actor);
    // A stomp (AGROUND): the nearest fallen ped the sphere reaches, or -1. It bleeds and takes nothing more.
    int StompStrike(const float centre[3], float radius) const;
    // CExplosion: peds inside `radius` take min(1, 2 (R - d) / R) * 250 (d from the centre to the ped
    // origin, one metre above the feet); survivors flee. Returns the number killed.
    int Explosion(const float centre[3], float radius);
    // CWorld::SetPedsChoking: peds inside `radius` stagger away from the gas.
    void Choke(const float centre[3], float radius);
    // CFire on a ped (CTaskComplexOnFire): it runs, screams (CTX_GLOBAL_PAIN_ON_FIRE) and burns for
    // step * 0.5 per 20 ms times the defend weakness until the fire goes out. Returns true if it died.
    void SetBurning(size_t i, bool on) { if (i < m_actors.size()) m_actors[i].burning = on; }
    bool Burning(size_t i) const { return i < m_actors.size() && m_actors[i].burning; }
    bool Choking(size_t i) const { return i < m_actors.size() && m_actors[i].chokeTimer > 0.0f; }
    // CTaskSimpleChoking: gas_cwr while the gas lasts, then idle_tired for 8-12 s.
    bool Fireproof(size_t) const { return false; }
    // Apply fire damage for dt seconds to a burning ped (CTaskComplexOnFire::ComputeFireDamage); true if it died.
    bool BurnDamage(size_t i, float dt);
    float DefendWeakness(int actor) const { return actor >= 0 && (size_t)actor < m_actors.size() ? m_actors[(size_t)actor].weakness : 1.0f; }
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
        float weakness = 1.0f;            // pedstats.dat defend weakness
        const AnimClip* hitClip = nullptr;   // melee hit reaction in progress
        float hitTime = 0.0f;
        bool burning = false;             // a CFire is attached
        float screamTimer = 0.0f;
        float chokeTimer = 0.0f;          // gas_cwr phase left (CTaskSimpleChoking), re-armed by gas
        float tiredTimer = 0.0f;          // idle_tired phase left
        float chokeAnim = 0.0f, coughTimer = 0.0f;
        float vx = 0, vy = 0, vz = 0;     // m/s, from CPhysical::ApplyMoveForce (a melee knock-back)
        bool flying = false;              // thrown and not yet landed
        bool getUp = false;               // a knock-down in progress: stands up when the hit clip ends
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
    std::unordered_map<std::string, float> m_defend;   // pedstats.dat name -> defend weakness
    unsigned m_rng = 99;
    int Rand(int n) { m_rng = m_rng * 1664525u + 1013904223u; return (int)((m_rng >> 8) % (unsigned)n); }
};
