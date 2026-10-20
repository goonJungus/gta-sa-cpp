// CJ's jetpack, ported from CTaskSimpleJetPack (gta-reversed / the Ghidra decomp). The task keeps its own
// velocity in game-frame units (50 per second): gravity, the nominal up thrust, forward thrust from the
// stick, strafe, the look-behind stop mode and a speed-squared damping. The host moves CJ by that velocity.
// The pack (jetpack.dff) rides on the upper torso like the parachute; its two jetballs tilt with the thrust.

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
constexpr float kFrameRate = 50.0f;
constexpr float kMagic = 0.008f;              // gravity per frame, and the unit of every thrust
constexpr float kThrustNominal = 0.8f, kThrustFull = 0.6f, kThrustStrafe = 0.3f, kThrustStop = 0.5f;
constexpr float kMaxAngle = 1.309f;           // THRUST_MAX_ANGLE
constexpr float kMoveDamping = 0.02f;         // THRUST_MOVE_DAMPING
constexpr float kAngleRate = 0.9f;            // JETPACK_ANGLE_RATE
constexpr float kCruiseHeight = 100.0f;       // thrust fades out above the ground + 100 m
constexpr float kPackOffset[3] = {0.1f, 0.08f, 0.0f};
constexpr float kBallPivot[2][3] = {{0.388f, -0.229f, 0.175f}, {-0.387f, -0.229f, 0.175f}};
constexpr float kBallNozzle[3] = {0.0f, 0.0f, -0.15f};   // JETBALL_OFFSET

float WrapPi(float a) {
    while (a > kPi) a -= 2.0f * kPi;
    while (a < -kPi) a += 2.0f * kPi;
    return a;
}

RwMat RotX(float a) {
    RwMat r;
    const float c = std::cos(a), s = std::sin(a);
    r.m[1][1] = c; r.m[1][2] = s; r.m[2][1] = -s; r.m[2][2] = c;
    return r;
}
RwMat RotY(float a) {
    RwMat r;
    const float c = std::cos(a), s = std::sin(a);
    r.m[0][0] = c; r.m[0][2] = -s; r.m[2][0] = s; r.m[2][2] = c;
    return r;
}
RwMat Trans(const float p[3]) {
    RwMat r;
    r.m[3][0] = p[0]; r.m[3][1] = p[1]; r.m[3][2] = p[2];
    return r;
}
}  // namespace

void PlayerPed::StartJetpack() {
    if (m_jet.on) return;
    m_jet = Jet();
    m_jet.on = true;
    m_jet.prevHeading = m_heading;
    m_aimHeading = m_heading;
    m_ducking = m_aiming = false;
    m_velX = m_velY = 0.0f;
}

void PlayerPed::StopJetpack(bool drop) {
    if (!m_jet.on) return;
    const float time = m_jet.time;
    m_jet = Jet();
    m_jet.time = time;
    m_jet.dropPending = drop;
    if (const AnimClip* c = Clip("jetpack_idle")) Remove(c);
}

bool PlayerPed::JetVelZ(float& v) const {
    if (!m_jet.on || !m_jet.flying) return false;
    v = m_jet.v[2] * kFrameRate;
    return true;
}

// Returns true when the pack owns this frame: CJ is thrusting or in the air. Standing on the ground with
// no thrust, the normal walking code runs (the task only reports bIsStanding then).
bool PlayerPed::UpdateJetpack(const Input& in) {
    Jet& j = m_jet;
    if (!j.on || in.noclip) return false;
    const float dt = in.dt;
    const float ts = dt * kFrameRate;
    j.time += dt;
    j.clock += dt;

    // the pad as the task reads it: along and across the camera
    const float cfx = std::cos(in.camYaw), cfy = std::sin(in.camYaw);
    const float fwdC = in.stickX * cfx + in.stickY * cfy;
    const float sideC = in.stickX * cfy - in.stickY * cfx;

    // mouse mode: the heading follows the camera
    j.prevHeading = m_heading;
    m_heading += WrapPi(in.camYaw - m_heading) * std::min(1.0f, 0.25f * ts);
    m_aimHeading = m_heading;
    j.headingDelta = WrapPi(m_heading - j.prevHeading);

    j.stop = in.jetStop;
    if (j.stop) {
        j.fwd = 0.0f;
        j.strafe = 0.0f;
    } else {
        j.fwd = in.jetBrake ? -1.0f : (std::fabs(fwdC) > 0.1f || in.sprint ? 1.0f : 0.0f);
        if (std::fabs(sideC) > 0.1f) j.strafe = std::clamp(j.strafe + (sideC > 0.0f ? 0.1f : -0.1f) * ts, -1.0f, 1.0f);
        else if (j.strafe > 0.0f) j.strafe = std::max(0.0f, j.strafe - 0.05f * ts);
        else j.strafe = std::min(0.0f, j.strafe + 0.05f * ts);
    }
    // the thrust angle follows the stick's forward component (push forward = tip forward)
    const float target = std::clamp(-fwdC, -1.0f, 1.0f) * kMaxAngle;
    j.angle += (target - j.angle) * (1.0f - std::pow(kAngleRate, ts));

    const bool standing = !(j.fwd > 0.0f || std::fabs(j.strafe) > 0.1f) && !j.stop;
    j.flying = !(in.onGround && standing);
    if (!j.flying) {   // walking with the pack on
        j.v[0] = j.v[1] = j.v[2] = 0.0f;
        j.fxKey = std::max(0.0f, j.fxKey - 0.1f * ts);
        if (in.exitPressed) StopJetpack(true);
        return false;
    }
    if (in.exitPressed && standing) {   // let go of the pack in mid-air hover
        StopJetpack(true);
        return false;
    }

    // vertical speed comes back from the host (ground and ceiling hits), horizontal is the task's own
    j.v[2] = in.velZ / kFrameRate;
    if (in.blocked) { j.v[0] *= 0.5f; j.v[1] *= 0.5f; }

    const float fwdX = std::cos(m_heading), fwdY = std::sin(m_heading);
    const float rightX = std::sin(m_heading), rightY = -std::cos(m_heading);
    j.v[2] -= kMagic * ts;   // gravity
    if (j.stop) {
        j.v[2] += kThrustStop * 2.0f * kMagic * ts;   // 1.0: hold altitude
        const float speed = std::sqrt(j.v[0] * j.v[0] + j.v[1] * j.v[1] + j.v[2] * j.v[2]);
        float stopForce = ts * kThrustStop * kMagic;
        if (stopForce * stopForce > speed * speed) stopForce *= 0.75f;
        if (stopForce >= speed) j.v[0] = j.v[1] = j.v[2] = 0.0f;
        else if (speed > 0.0f) {
            const float k = 1.0f - stopForce / speed;
            j.v[0] *= k; j.v[1] *= k; j.v[2] *= k;
        }
    } else {
        float up = kThrustNominal * kMagic * ts;
        if (j.fwd < 0.0f) up *= 0.5f;
        j.v[2] += up;
        if (j.fwd > 0.0f) {
            const float mult = std::clamp(1.0f - (in.heightAboveGround - kCruiseHeight) / kCruiseHeight, 0.0f, 1.0f);
            const float horiz = -std::sin(j.angle), vert = std::max(std::cos(j.angle), 0.75f);
            const float k = kThrustFull * mult * kMagic * ts;
            j.v[0] += fwdX * horiz * k;
            j.v[1] += fwdY * horiz * k;
            j.v[2] += vert * k;
        }
        const float sk = j.strafe * kThrustStrafe * kMagic * ts;
        j.v[0] += rightX * sk;
        j.v[1] += rightY * sk;
    }
    {   // speed-squared damping
        const float speed = std::sqrt(j.v[0] * j.v[0] + j.v[1] * j.v[1] + j.v[2] * j.v[2]);
        const float k = std::pow(std::max(0.0f, 1.0f - speed * kMoveDamping), ts);
        j.v[0] *= k; j.v[1] *= k; j.v[2] *= k;
    }
    m_velX = j.v[0] * kFrameRate;
    m_velY = j.v[1] * kFrameRate;

    // the flame key: up while thrusting, down otherwise, 0.1 per frame
    const bool burning = j.fwd > 0.0f || j.stop || !in.onGround;
    j.fxKey = std::clamp(j.fxKey + (burning ? 0.1f : -0.1f) * ts, 0.0f, 1.0f);

    // the airborne pose
    if (const AnimClip* idle = Clip("jetpack_idle")) {
        if (!Active(idle)) Blend(idle, true, false, 4.0f);
    } else if (const AnimClip* g = Clip("fall_glide")) {
        if (!Active(g)) Blend(g, true, false, 4.0f);
    }
    m_inAir = true;
    m_jumping = false;
    m_busy = false;
    m_move.Reset();
    return true;
}

// pack -> model space: Ry(90 degrees), the offset, then the upper torso bone (the parachute's attach).
bool PlayerPed::JetPackMatrix(RwMat& out) const {
    const int f = m_frameOfBone[3];
    if (f < 0 || f >= (int)m_globals.size()) return false;
    out = RotY(kPi * 0.5f) * Trans(kPackOffset) * m_globals[f];
    return true;
}

bool PlayerPed::JetNozzles(float x, float y, float feetZ, float out[2][3]) const {
    RwMat pack;
    if (!JetPackMatrix(pack)) return false;
    const float a = m_heading - kPi * 0.5f, c = std::cos(a), s = std::sin(a);
    for (int side = 0; side < 2; side++) {
        float rx = 0.0f, ry = 0.0f;
        if (m_jet.stop) { rx = std::sin(m_jet.clock * 1000.0f / 79.58f) * 45.0f * kPi / 180.0f; ry = std::cos(m_jet.clock * 1000.0f / 79.58f) * 45.0f * kPi / 180.0f; }
        else {
            rx = std::clamp(m_jet.angle - std::clamp(m_jet.headingDelta, -0.2f, 0.2f) * 10.0f, -kPi * 0.5f, kPi * 0.5f);
            ry = (side == 0 ? std::max(0.0f, m_jet.strafe) : std::min(0.0f, m_jet.strafe)) * 45.0f * kPi / 180.0f;
        }
        const RwMat ball = RotX(rx) * RotY(ry) * Trans(kBallPivot[side]) * pack;
        float p[3];
        ball.Apply(kBallNozzle, p);
        out[side][0] = x + p[0] * c - p[1] * s;
        out[side][1] = y + p[0] * s + p[1] * c;
        out[side][2] = feetZ - m_bindMinZ + p[2];
    }
    return true;
}

void PlayerPed::DrawJetpack(D3DRenderer& r, const D3DMATRIX& world) {
    if (!m_jet.on || m_jetBody.empty()) return;
    RwMat pack;
    if (!JetPackMatrix(pack)) return;
    IDirect3DDevice9* dev = r.GetDevice();
    auto set = [&](const RwMat& m) {
        float hm[4][4];
        for (int i = 0; i < 4; i++) { for (int j = 0; j < 3; j++) hm[i][j] = m.m[i][j]; hm[i][3] = i == 3 ? 1.0f : 0.0f; }
        const float* w = &world._11;
        D3DMATRIX out;
        float* o = &out._11;
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++) {
                float v = 0.0f;
                for (int k = 0; k < 4; k++) v += hm[i][k] * w[k * 4 + j];
                o[i * 4 + j] = v;
            }
        dev->SetTransform(D3DTS_WORLD, &out);
    };
    set(pack);
    for (D3DRenderMesh* m : m_jetBody) r.DrawMesh(m);
    for (int side = 0; side < 2; side++) {
        float rx, ry;
        if (m_jet.stop) {   // stop mode wobbles the balls
            rx = std::sin(m_jet.clock * 1000.0f / 79.58f) * 45.0f * kPi / 180.0f;
            ry = std::cos(m_jet.clock * 1000.0f / 79.58f) * 45.0f * kPi / 180.0f;
        } else {
            rx = std::clamp(m_jet.angle - std::clamp(m_jet.headingDelta, -0.2f, 0.2f) * 10.0f, -kPi * 0.5f, kPi * 0.5f);
            ry = (side == 0 ? std::max(0.0f, m_jet.strafe) : std::min(0.0f, m_jet.strafe)) * 45.0f * kPi / 180.0f;
        }
        set(RotX(rx) * RotY(ry) * Trans(kBallPivot[side]) * pack);
        for (D3DRenderMesh* m : m_jetBall[side]) r.DrawMesh(m);
    }
}

void PlayerPed::LoadJetpack(D3DRenderer& r, ImgLoader& gta3) {
    const std::vector<uint8_t> dffData = gta3.Extract("jetpack.dff");
    const std::vector<uint8_t> txdData = gta3.Extract("jetpack.txd");
    if (dffData.empty()) { std::fprintf(stderr, "[JET] jetpack.dff not found\n"); return; }
    std::vector<TxdTexture> txd;
    if (!txdData.empty()) txd = TxdLoader::LoadFromMemory(txdData.data(), txdData.size());
    DffModel m = DffLoader::LoadFromMemory(dffData.data(), dffData.size());
    for (DffMesh& dm : m.meshes) {
        if (dm.vertices.empty() || dm.indices.empty()) continue;
        D3DRenderMesh* mesh = r.CreateMesh(reinterpret_cast<const MeshVertex*>(dm.vertices.data()), (uint32_t)dm.vertices.size(),
                                           dm.indices.data(), (uint32_t)dm.indices.size());
        if (!mesh) continue;
        for (const TxdTexture& t : txd) {
            if (TxdLoader::KeyOf(t.name) != TxdLoader::KeyOf(dm.textureName)) continue;
            IDirect3DTexture9* tex = r.CreateTexture(t.width, t.height, t.d3dFormat, t.mip0.data(), (uint32_t)t.mip0.size(), &t.extraMips);
            mesh->texture = tex;
            m_jetTextures.push_back(tex);
            break;
        }
        // atomics are (frame, geometry): geometry 0 jetball1, 1 jetball2, 2 the body
        (dm.geometry == 0 ? m_jetBall[0] : dm.geometry == 1 ? m_jetBall[1] : m_jetBody).push_back(mesh);
    }
    std::fprintf(stderr, "[JET] jetpack.dff body %zu balls %zu/%zu\n", m_jetBody.size(), m_jetBall[0].size(), m_jetBall[1].size());
}
