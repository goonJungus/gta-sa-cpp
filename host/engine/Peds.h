#pragma once
// Peds: pedestrians (CPed / CPopulation). A ped model is a skinned clump of its own (peds.ide gives
// the model, texture and animation group, animgrp.dat the walk and idle clips). The first slice spawns
// civilians that stand or wander on the ground; no AI beyond walking, no reactions yet.

#include <cstdint>
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
};

class Peds {
public:
    bool Init(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);
    bool Ready() const { return m_ready; }
    int ModelCount() const { return (int)m_pool.size(); }

    // Spawn a civilian (model id from the pool, -1 picks one at random). Returns the actor index or -1.
    int Spawn(D3DRenderer& r, int id, float x, float y, float z, float heading);
    size_t Count() const { return m_actors.size(); }
    void Clear() { m_actors.clear(); }
    // Advance animation and wandering. groundAt gives the ground height or a value below -1e5.
    void Update(float dt, const std::function<float(float, float, float)>& groundAt,
                const std::function<bool(float, float, float, float, float)>& blocked);
    void Draw(D3DRenderer& r);

private:
    struct Model {
        PedInfo info;
        SkinnedClump clump;
        std::vector<int> boneFrame;           // node -> skeleton frame
        std::vector<RwMat> skeletonLocal;     // per frame, bind pose (rest offsets fixed from the skin matrices)
        std::vector<int> frameOfBone;
        D3DRenderMesh* mesh = nullptr;
        IDirect3DTexture9* texture = nullptr;
        std::vector<MeshVertex> skinned;
        float feetZ = 0.0f;                   // lowest bind pose vertex
        const AnimClip* walk = nullptr;
        const AnimClip* idle = nullptr;
        bool valid = false;
    };
    struct Actor {
        int model = 0;
        float x = 0, y = 0, z = 0, heading = 0;   // heading 0 faces +x
        float animTime = 0.0f, turnTimer = 0.0f, walkTimer = 0.0f;
        bool walking = false;
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
    unsigned m_rng = 99;
    int Rand(int n) { m_rng = m_rng * 1664525u + 1013904223u; return (int)((m_rng >> 8) % (unsigned)n); }
};
