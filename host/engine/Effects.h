#pragma once
// Effects - short lived world effects from gunfire: bullet tracers, impact
// dust puffs and sparks. Everything is a camera-facing quad drawn through
// D3DRenderer::DrawBlendedTriangles with one soft procedural sprite.

#include <vector>

#include "D3DRenderer.h"

class Effects {
public:
    bool Init(D3DRenderer& r);
    void Shutdown(D3DRenderer& r);
    void Update(float dt);
    void Render(D3DRenderer& r);

    // Blood spray where a bullet hit a ped.
    void Blood(const float p[3]);
    // A bullet hit on a surface at p; nz = surface normal z (0 = wall).
    void Impact(const float p[3], float nz);
    // Foam and droplets thrown up where CJ hits or wades through water (strength 0..1).
    void Splash(const float p[3], float strength);
    // A kick of dust at the feet (hard landings, sprint starts).
    void Dust(const float p[3], float strength);
    // Smoke rising from a damaged engine (dark 0..1) and a car blowing up.
    void Gas(const float p[3]);   // a tear gas cloud puff
    void Smoke(const float p[3], float dark);
    void Spray(const float p[3], uint32_t rgb);   // a paint / foam puff from the spray can or extinguisher
    void Fire(const float p[3]);
    void Explosion(const float p[3]);
    // CExplosion fx systems: 0 explosion_tiny, 1 _small, 2 _medium, 3 _large, 4 _molotov.
    void Explosion(const float p[3], int kind);
    // CShadows::AddPermanentShadow of a blast: a scorch mark flat on the ground (size 8, 30 s).
    void Scorch(const float p[3], float size = 8.0f);
    // A tracer streak from a to b.
    void Tracer(const float a[3], const float b[3]);

private:
    struct Puff { float p[3], v[3]; float age, life, size0, size1; uint32_t rgb; float alpha; bool spark; };
    struct Trace { float a[3], b[3]; float age, life; };
    std::vector<Puff> m_puffs;
    std::vector<Trace> m_traces;
    struct Scorch_ { float p[3], size, age, life; };
    std::vector<Scorch_> m_scorch;
    IDirect3DTexture9* m_tex = nullptr;
};
