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
    // A tracer streak from a to b.
    void Tracer(const float a[3], const float b[3]);

private:
    struct Puff { float p[3], v[3]; float age, life, size0, size1; uint32_t rgb; float alpha; bool spark; };
    struct Trace { float a[3], b[3]; float age, life; };
    std::vector<Puff> m_puffs;
    std::vector<Trace> m_traces;
    IDirect3DTexture9* m_tex = nullptr;
};
