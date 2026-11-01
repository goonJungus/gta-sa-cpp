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
    // The heli_dust fx system (effects.fxp) held at const time t (0..1) at p for dt seconds: prim 0 "normal" dust,
    // 1 "sand", 2 "water". acc carries the part-particle emission between frames.
    void HeliDust(const float p[3], float t, int prim, float dt, float& acc);
    // The "nitro" fx system at an exhaust for dt seconds at const time = power: a bright flame jet aft once power is
    // over 0.5 (10 to 100 a second) and a cold blue haze (30 a second) while it recharges below 0.4. vel: the car's
    // velocity in m/s, which the local particles share; acc carries the emission between frames.
    void Nitro(const float p[3], const float back[3], const float vel[3], float power, float dt, float* acc);
    // prt_boatsplash (FxSystem_c::AddParticle with an FxPrtMult): life and size are the multipliers, alpha 0..1;
    // falls at 9.8 m/s^2.
    void BoatSplash(const float p[3], const float v[3], float lifeMult, float sizeMult, float alpha);
    // The boat_prop fx system at a propeller for dt seconds at const time t (0..1): its "spray" thrown back and up
    // and its "propellor" bubbles. vel: the boat's velocity (m/s), which the local particles share.
    void BoatProp(const float p[3], const float fwd[3], const float up[3], const float vel[3], float t, float dt);
    void Fire(const float p[3]);
    // Fx_c::AddSparks: count bright specks thrown along dir (unit) at about speed m/s, falling.
    void Sparks(const float p[3], const float dir[3], float speed, int count);
    void Explosion(const float p[3]);
    // CExplosion fx systems: 0 explosion_tiny, 1 _small, 2 _medium, 3 _large, 4 _molotov.
    void Explosion(const float p[3], int kind);
    // CShadows::AddPermanentShadow of a blast: a scorch mark flat on the ground (size 8, 30 s).
    void Scorch(const float p[3], float size = 8.0f);
    // CBulletTraces::AddTrace(muzzle, hit, weapon, entity): a short streak somewhere along the shot's path
    // (2..5 m long, 300 ms, alpha 70). suppress = the player's first-person / scope modes while standing still.
    void Tracer(const float a[3], const float b[3], bool suppress = false);
    // CBulletTraces::AddTrace(from, to, radius, disappearTime, alpha): sixteen slots, the later ones live shorter.
    int AddTrace(const float from[3], const float to[3], float radius, float lifeMs, int alpha);
    int ActiveTraces() const;
    // CGlass::GeneratePanesForWindow (car type): the rectangle at corner with edges fwd and right breaks into up to
    // 3 x 3 sections of five shards each that fall from vel (per 1/50 s), pushed out from centre, until groundZ.
    void GlassPanes(const float corner[3], const float fwd[3], const float right[3], const float vel[3], const float centre[3],
                    float centreDrag, float groundZ);
    bool TraceInfo(int slot, float* life, float* radius, int* alpha) const;

private:
    // ring: a heli_dust particle (FX friction 0.985, unseen for its first half, then growing to size1 and fading)
    // fall: an fx particle with a FORCE line (grav down, no drag), alpha fading out over its life, size from size0
    // to size1 (through sizeMid at half life when sizeMid >= 0)
    struct Puff { float p[3], v[3]; float age, life, size0, size1; uint32_t rgb; float alpha; bool spark; bool ring;
                  bool fall; float grav, sizeMid; };
    struct Trace { bool exists = false; float a[3], b[3]; float createMs, lifeMs, radius; int alpha; };
    std::vector<Puff> m_puffs;
    Trace m_traces[16];
    float m_clockMs = 0.0f;
    // CFallingGlassPane: matrix right / forward / up / position, speed, spin, which of the five shards.
    struct Pane { bool exist = false; int piece = 0; float r[3], f[3], u[3], p[3], v[3], spin[3]; float bornMs = 0, groundZ = 0; };
    Pane m_panes[44];
    struct Scorch_ { float p[3], size, age, life; };
    std::vector<Scorch_> m_scorch;
    IDirect3DTexture9* m_tex = nullptr;
};
