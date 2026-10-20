#pragma once
// Cutscene: the cutscene player (CCutsceneMgr). A cutscene NAME is three files in anim\cuts.img: NAME.cut
// (the object list, subtitle times, offset), NAME.dat (the camera splines, CCamera::LoadPathSplines) and
// NAME.ifp (an old ANPK package with one clip per object). The objects are the models of models\cutscene.img;
// "csplay" is CJ himself and is drawn by the caller through the hook.

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "D3DRenderer.h"
#include "IfpLoader.h"
#include "SkinnedDff.h"

struct CutsceneCamera {
    float pos[3], look[3], up[3];
    float fov;      // degrees
};

class Cutscene {
public:
    // Draws CJ at `origin` from the clip pose at `time` seconds.
    using DrawCj = std::function<void(D3DRenderer&, const AnimClip*, float time, const float origin[3])>;

    bool Load(D3DRenderer& r, const std::string& gameDir, const std::string& name);
    void Free(D3DRenderer& r);
    bool Loaded() const { return m_loaded; }
    void SetOffset(float x, float y, float z) { m_offset[0] = x; m_offset[1] = y; m_offset[2] = z; }
    const float* Offset() const { return m_offset; }
    void Start(float ms = 0.0f) { m_ms = ms; m_running = true; }   // ms: dev seek
    void Update(float dt);                       // dt seconds
    void Skip() { m_ms = m_duration * 1000.0f; m_running = false; m_skipped = true; }
    bool Running() const { return m_running; }
    bool Skipped() const { return m_skipped; }
    float TimeMs() const { return m_ms; }
    float Duration() const { return m_duration; }   // seconds
    // The camera at the current time; false when the cutscene has no camera data.
    bool Camera(CutsceneCamera& out) const;
    // The GXT key of the subtitle showing now, or an empty string.
    std::string Subtitle() const;
    void Draw(D3DRenderer& r, const DrawCj& cj);
    size_t ObjectCount() const { return m_objs.size(); }
    // Dev: the pose of object i (bone id -> world position) for the tests.
    bool BoneWorld(size_t obj, int boneId, float out[3]) const;
    const std::string& ObjectName(size_t i) const { return m_objs[i].model; }

private:
    struct Key3 { float t, v[3][3]; };   // time; point, in-control, out-control
    struct Key1 { float t, v[3]; };
    struct Obj {
        std::string model;
        const AnimClip* clip = nullptr;
        bool isCj = false;
        SkinnedClump clump;
        std::vector<int> boneFrame, frameOfBone;
        std::vector<RwMat> skeletonLocal, globals;
        std::vector<D3DRenderMesh*> meshes;
        std::vector<IDirect3DTexture9*> textures;
        std::vector<MeshVertex> skinned;
    };
    struct Text { float start, dur; std::string key; };

    bool BuildObject(D3DRenderer& r, Obj& o, const std::vector<uint8_t>& dff, const std::vector<uint8_t>& txd);
    void Pose(Obj& o, float seconds);
    static float Eval1(const std::vector<Key1>& k, float t);
    static void Eval3(const std::vector<Key3>& k, float t, float out[3]);

    bool m_loaded = false, m_running = false, m_skipped = false;
    float m_ms = 0.0f, m_duration = 0.0f;
    float m_offset[3] = {};
    std::vector<Obj> m_objs;
    std::vector<Text> m_text;
    std::unordered_map<std::string, AnimClip> m_clips;
    std::vector<Key1> m_fov, m_roll;
    std::vector<Key3> m_src, m_dst;
};
