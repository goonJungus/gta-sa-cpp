#pragma once
// CWaterCannons / CWaterCannon: the fire truck and swat van water jet. Each cannon keeps a ring of 32 sections
// (position and velocity, in metres per 20 ms step); a section is fed every frame by the truck, then falls and
// drifts on its own. Fires near a section lose strength, peds near one get knocked down, and the jet is drawn as
// a string of translucent crossed quads.

#include <cstdint>
#include <functional>

#include "D3DRenderer.h"

class WaterCannons {
public:
    static constexpr int kMax = 3;          // MAX_WATER_CANNONS
    static constexpr int kSections = 32;    // SECTIONS_COUNT

    struct Hooks {
        std::function<void(const float* pos, float radius, float strength)> extinguish;   // CFireManager::ExtinguishPointWithWater
        int pedCount = 0;
        std::function<bool(int ped, float* pos)> pedPos;                                  // false for a ped that cannot be pushed
        std::function<void(int ped, const float* secPos, const float* secSpeed, float timeStep)> pedHit;
    };
    struct RenderHooks {
        // Segment a -> b against the world: the fraction of the way to the first hit (1 if clear), surface normal z.
        std::function<float(const float* a, const float* b, float* nz)> ray;
        std::function<void(const float* pos, const float* vel)> splash;
    };

    bool Init(D3DRenderer& r);
    void Shutdown(D3DRenderer& r);

    // CWaterCannons::UpdateOne(vehicle, start, end): feed the cannon of this owner a section (end is its velocity per step).
    void UpdateOne(uint32_t owner, const float start[3], const float end[3], float nowMs);
    // CWaterCannons::Update, once per frame; timeStep is dt in 20 ms units.
    void Update(float timeStep, uint32_t frameCounter, float nowMs, const Hooks& h);
    void Render(D3DRenderer& r, const float camFwd[3], const RenderHooks& h);

    // Inspection (dev logs and tests).
    int ActiveCannons() const;
    int ActiveSections(uint32_t owner) const;
    bool Section(uint32_t owner, int slot, float pos[3], float vel[3]) const;

private:
    struct Cannon {
        uint32_t id = 0;
        int count = 0;                       // m_nSectionsCount: the ring slot being written
        float creationMs = 0.0f;
        bool used[kSections] = {};
        float point[kSections][3] = {};
        float speed[kSections][3] = {};
    };
    void InitCannon(Cannon& c, float nowMs);
    void OncePerFrame(Cannon& c, int index, float ts, uint32_t frame, float nowMs, const Hooks& h);
    void PushPeds(Cannon& c, float ts, const Hooks& h);
    Cannon m_cannons[kMax];
    IDirect3DTexture9* m_white = nullptr;
};
