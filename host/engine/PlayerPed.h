#pragma once
// CJ: the player ped, built and animated the way retail does it.
//
// Model: CClothesBuilder::CreateSkinnedClump assembles the player from
// player.img clothing parts (torso, head, hands, legs, feet), whose bones are
// re-indexed onto the base player.dff clump (gta3.img): that clump supplies
// the bone hierarchy and the skin-to-bone matrices. The "Normal" body variant is used
// (retail blends Ripped/Fat/Normal by the muscle and fat stats). Textures are
// composited like CClothesBuilder::ConstructTextures: the clothing texture is
// placed on top of the player's skin texture (vest over player_torso, jeans
// over player_legs).
//
// Animation: clips from anim/ped.ifp (IDLE_stance, WALK_player, run_player,
// sprint_civi, JUMP_*, FALL_*), sampled per bone, cross-faded, and skinned on
// the CPU every frame. Root motion is stripped horizontally: the controller
// moves CJ, and locomotion clips play at the rate that matches his speed.

#include "D3DRenderer.h"
#include "IfpLoader.h"
#include "SkinnedDff.h"

#include <string>
#include <unordered_map>
#include <vector>

class PlayerPed {
public:
    struct Input {
        float dt = 0.0f;
        float moveX = 0.0f, moveY = 0.0f;   // horizontal velocity actually moved (m/s)
        bool onGround = true;
        float velZ = 0.0f;
    };

    bool Init(D3DRenderer& r, const std::string& gameDir);
    void Shutdown(D3DRenderer& r);
    bool Ready() const { return m_ready; }

    void Update(const Input& in);
    // feet = world position of the ground contact point.
    void Draw(D3DRenderer& r, float x, float y, float feetZ);

    float Heading() const { return m_heading; }   // radians, world yaw CJ faces
    void SetHeading(float h) { m_heading = h; }
    const char* ClipName() const;

private:
    struct Part {
        std::vector<SkinVertex> verts;
        std::vector<int> boneFrame;      // part node index -> skeleton frame
        std::vector<RwMat> inverseBind;  // part node index -> inverse bind
        std::vector<MeshVertex> skinned; // per-frame output
        D3DRenderMesh* mesh = nullptr;
    };
    struct Pose { float q[4]; float t[3]; };
    struct Track {
        const AnimClip* clip = nullptr;
        float time = 0.0f;
        bool loop = true;
    };

    const AnimClip* Clip(const char* name) const;
    void Play(const AnimClip* clip, bool loop, float fade, bool keepPhase);
    void Sample(const Track& tr, std::vector<Pose>& out) const;
    void BuildGlobals(const std::vector<Pose>& pose);

    bool m_ready = false;
    std::vector<SkinFrame> m_skeleton;
    std::vector<int> m_frameOfBone;            // bone id (0..511) -> frame index
    std::vector<RwMat> m_globals;              // per frame, relative to frame 0
    std::vector<Part> m_parts;
    std::vector<IDirect3DTexture9*> m_textures;
    std::unordered_map<std::string, AnimClip> m_clips;

    Track m_cur, m_prev;
    float m_blend = 1.0f, m_blendRate = 0.0f;  // m_prev fades out as m_blend -> 1
    std::vector<Pose> m_poseA, m_poseB;
    float m_heading = 1.5708f;
    float m_bindMinZ = 0.0f;                   // feet in model space
    float m_airTime = 0.0f;
    bool m_wasOnGround = true;
    const AnimClip* m_landClip = nullptr;
};
