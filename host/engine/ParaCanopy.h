#pragma once
// The parachute canopy: the PARACHUTE object (parachute.dff, paracx.txd) that the parachute script
// attaches to the player and plays the PARA_*_O clips on, in step with the player's PARA_* clips.
// It is a skinned clump (one atomic, an RpHAnim hierarchy), posed on the CPU like a ped.

#include "D3DRenderer.h"
#include "IfpLoader.h"
#include "SkinnedDff.h"

#include <string>
#include <vector>

class ImgLoader;

class ParaCanopy {
public:
    bool Load(D3DRenderer& r, ImgLoader& gta3);
    void Shutdown(D3DRenderer& r);
    bool Ready() const { return m_ready; }
    // Pose with one clip at a time (seconds); the clip's bone ids are the clump's RpHAnim ids.
    void Pose(const AnimClip* clip, float time);
    // Draws with the world transform already set on the device.
    void Draw(D3DRenderer& r);

private:
    SkinnedClump m_clump;
    std::vector<RwMat> m_local;   // bind-pose local per frame
    std::vector<int> m_boneFrame; // node index -> frame
    std::vector<MeshVertex> m_skinned;
    std::vector<D3DRenderMesh*> m_meshes;
    std::vector<IDirect3DTexture9*> m_textures;
    bool m_ready = false;
};
