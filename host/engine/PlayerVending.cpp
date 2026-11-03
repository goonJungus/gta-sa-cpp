// CJ at a vending machine: the task sequence vending_machine.scm gives him (script.img), played by the ped.
//   TASK_CHAR_SLIDE_TO_COORD_AND_PLAY_ANIM: VEND_USE while he slides to the spot in front of the machine
//     (CTaskSimpleSlideToCoord: 0.4 of the way each 1/50 s, turning to the machine's heading),
//   drink machines: TASK_PICK_UP_OBJECT, the juice can in the left hand (ped node 5) with VEND_USE_PT2,
//     then TASK_PLAY_ANIM_SECONDARY VEND_DRINK2_P,
//   snack machines: TASK_PLAY_ANIM_SECONDARY VEND_EAT_P.
// The secondary clips are upper-body overlays on the idle. The machine side lives in Vending.cpp.

#define NOMINMAX
#include "PlayerPed.h"

#include "DffLoader.h"
#include "ImgLoader.h"
#include "TxdLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr float kPi = 3.14159265f;
constexpr int kBoneLHand = 34;   // BONE_L_HAND (ped node 5, PED_NODE_LEFT_HAND)

float WrapPi(float a) {
    while (a > kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}
}

void PlayerPed::LoadVendingCan(D3DRenderer& r, ImgLoader& gtaInt) {
    const std::vector<uint8_t> dffData = gtaInt.Extract("cj_juice_can.dff");
    const std::vector<uint8_t> txdData = gtaInt.Extract("juice.txd");
    if (dffData.empty()) { std::fprintf(stderr, "[VEND] cj_juice_can.dff not found\n"); return; }
    std::vector<TxdTexture> txd;
    if (!txdData.empty()) txd = TxdLoader::LoadFromMemory(txdData.data(), txdData.size());
    DffModel m = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
    float maxY = -1e9f;
    for (DffMesh& dm : m.meshes) {
        if (dm.vertices.empty() || dm.indices.empty()) continue;
        for (const DffVertex& v : dm.vertices) maxY = std::max(maxY, v.y);
        D3DRenderMesh* mesh = r.CreateMesh(reinterpret_cast<const MeshVertex*>(dm.vertices.data()), (uint32_t)dm.vertices.size(),
                                           dm.indices.data(), (uint32_t)dm.indices.size());
        if (!mesh) continue;
        for (const TxdTexture& t : txd) {
            if (TxdLoader::KeyOf(t.name) != TxdLoader::KeyOf(dm.textureName)) continue;
            IDirect3DTexture9* tex = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(), &t.extraMips);
            mesh->texture = tex;
            m_canTextures.push_back(tex);
            break;
        }
        m_canMeshes.push_back(mesh);
    }
    // the script holds it at (0.062, height / 2 + 0.027, 0) from the hand, the height from GET_MODEL_DIMENSIONS
    if (maxY > -1e8f) m_canOffsetY = maxY * 0.5f + 0.027f;
}

bool PlayerPed::StartVending(float x, float y, float heading, bool snack) {
    if (m_vend.stage != 0 || Dead() || m_seated || m_swimming || m_jet.on) return false;
    const AnimClip* use = Clip("vend_use");
    if (!use) return false;
    m_vend = Vend();
    m_vend.stage = 1;
    m_vend.x = x; m_vend.y = y; m_vend.heading = heading; m_vend.snack = snack;
    m_aiming = m_ducking = false;
    m_fight.active = false;
    m_busy = false;
    m_ratio = 0.0f;
    Blend(use, false, false, 4.0f);   // blend delta 4.0 from the script
    return true;
}

void PlayerPed::UpdateVending(const Input& in) {
    const float dt = in.dt;
    m_vend.t += dt;
    if (m_vend.stage == 1) {   // CTaskSimpleSlideToCoord
        const float dx = m_vend.x - in.worldX, dy = m_vend.y - in.worldY;
        if (dx * dx + dy * dy >= 0.0025f && dt > 0.0f) {
            const float k = std::min(1.0f, 0.4f * dt * 50.0f);   // m_Speed of the remaining distance per step
            m_velX = dx * k / dt; m_velY = dy * k / dt;
        } else m_velX = m_velY = 0.0f;
        // CPed::UpdatePosition turns him toward m_fAimingRotation: the player's 9 degrees a step
        const float diff = WrapPi(m_vend.heading - m_heading);
        const float turn = 9.0f * (kPi / 180.0f) * 50.0f * dt;
        m_heading = WrapPi(m_heading + std::clamp(diff, -turn, turn));
        const Assoc* a = Get(Clip("vend_use"));
        const bool animDone = !a || a->finished || a->dead;
        if (animDone && m_vend.animEnd < 0.0f) m_vend.animEnd = m_vend.t;
        const bool there = dx * dx + dy * dy < 0.0025f && std::fabs(diff) < 0.1f;
        // the task ends when the clip has played and he is there, or 0.5 s after the clip at the latest
        if (animDone && (there || m_vend.t - m_vend.animEnd > 0.5f)) {
            m_velX = m_velY = 0.0f;
            if (!m_vend.snack && Clip("vend_use_pt2")) {
                m_vend.stage = 2;
                m_vend.can = true;
                Blend(Clip("vend_use_pt2"), false, false, 16.0f);
            } else {
                m_vend.stage = 3;
                StartVendingSecondary(m_vend.snack ? "vend_eat_p" : "vend_drink2_p");
            }
        }
        return;
    }
    m_velX = m_velY = 0.0f;
    if (m_vend.stage == 2) {   // VEND_USE_PT2: the can comes to the hand
        const Assoc* a = Get(Clip("vend_use_pt2"));
        if (!a || a->finished || a->dead) {
            m_vend.stage = 3;
            StartVendingSecondary("vend_drink2_p");
        }
        return;
    }
    if (m_vend.stage == 3) {   // the secondary clip over the idle
        const Assoc* a = Get(m_vend.secondary);
        if (!a || a->finished || a->dead) {
            if (a) Remove(m_vend.secondary);
            m_vend.stage = 0;
            m_vend.can = false;
            m_vend.done = true;
        }
    }
}

void PlayerPed::StartVendingSecondary(const char* clip) {
    if (const AnimClip* idle = Clip("idle_stance")) Blend(idle, true, false, 4.0f);   // back to the stand under the overlay
    m_vend.secondary = Clip(clip);
    if (!m_vend.secondary) return;
    Assoc* a = Add(m_vend.secondary, false, false);
    a->partial = true;
    a->blend = 0.0f;
    a->delta = 4.0f;
}

void PlayerPed::DrawVendingCan(D3DRenderer& r, const float w[4][4]) {
    if (!m_vend.can || m_canMeshes.empty()) return;
    const int f = m_frameOfBone[kBoneLHand];
    if (f < 0 || f >= (int)m_globals.size()) return;
    IDirect3DDevice9* dev = r.GetDevice();
    if (!dev) return;
    RwMat off;
    off.m[3][0] = 0.062f; off.m[3][1] = m_canOffsetY; off.m[3][2] = 0.0f;
    const RwMat h = off * m_globals[f];
    float hm[4][4];
    for (int i = 0; i < 4; i++) { for (int j = 0; j < 3; j++) hm[i][j] = h.m[i][j]; hm[i][3] = i == 3 ? 1.0f : 0.0f; }
    D3DMATRIX out;
    float* o = &out._11;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float v = 0.0f;
            for (int k = 0; k < 4; k++) v += hm[i][k] * w[k][j];
            o[i * 4 + j] = v;
        }
    dev->SetTransform(D3DTS_WORLD, &out);
    for (D3DRenderMesh* m : m_canMeshes) r.DrawMesh(m);
}
